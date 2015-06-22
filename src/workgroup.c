
/*
   Copyright (c) 2015, Tamas Foldi, Starschema

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:
   1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
   2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
   THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
   IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
   OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
   IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
   INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
   NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
   THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
   */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <pthread.h>
#include "workgroup.h"
#include "tableaufs.h"
#include "libpq-fe.h"
#include "libpq/libpq-fs.h"

#define BUFSIZE          1024
#define TFS_WG_BLOCKSIZE 8196
#define _NAME_MAX "255"

#define TFS_WG_MTIME \
  ", extract(epoch from coalesce(c.updated_at,'2000-01-01')) ctime "

#define TFS_WG_LIST_SITES  \
  "select c.name" TFS_WG_MTIME "from sites c where 1 = 1 "

#define TFS_WG_LIST_PROJECTS \
  "select c.name" TFS_WG_MTIME "from projects c inner join" \
  " sites p on (p.id = c.site_id) where p.name = $1"

#define TFS_WG_LIST_FILE( entity, ext ) \
  "select c.name || '." #ext "' || case when substring(data from 1 for 2) = " \
  "'PK' then 'x' else '' end filename " TFS_WG_MTIME ", content," \
  "(select sum(length(data)) from pg_largeobject where pg_largeobject.loid = " \
  "repository_data.content) size from " #entity " c inner join repository_data " \
  " on (repository_data.id = coalesce(repository_data_id,repository_extract_data_id))" \
  "inner join projects on (c.project_id = projects.id) inner join sites on " \
  "(sites.id = projects.site_id) inner join pg_largeobject on " \
  "(repository_data.content = pg_largeobject.loid) where pg_largeobject.pageno = 0" \
  " and sites.name = $1 and projects.name = $2 "

#define TFS_WG_LIST_WORKBOOKS TFS_WG_LIST_FILE( workbooks, twb )

#define TFS_WG_LIST_DATASOURCES TFS_WG_LIST_FILE( datasources, tds )

#define TFS_WG_NAMES_WITHOUT_SLASH(ext) \
  "replace(c.name,'/','_')||'." #ext "x', replace(c.name,'/','_')||'." #ext "' "


/** The global connection object */
static PGconn* global_conn;

/** Mutex for transaction LO access */
static pthread_mutex_t tfs_wg_transaction_block_mutex = PTHREAD_MUTEX_INITIALIZER;



/**
 * Connection data for postgres.
 *
 * This gets filled on the call to TFS_WG_connect_db.
 */
static struct tableau_cmdargs pg_connection_data;

/** Helper function to wrap connecting to a database using the connection data */
static PGconn* connect_to_pg( struct tableau_cmdargs conn_data)
{
  PGconn* new_conn = PQsetdbLogin(
      conn_data.pghost,
      conn_data.pgport,
      NULL, NULL, "workgroup",
      conn_data.pguser,
      conn_data.pgpass );

  /* check to see that the backend connection was successfully made */
  if (PQstatus(new_conn) == CONNECTION_BAD)
  {
    fprintf(stderr, "Connection to database '%s' failed.\n", conn_data.pghost);
    fprintf(stderr, "%s", PQerrorMessage(new_conn));
    PQfinish(new_conn);
    return NULL;
  }

  return new_conn;
}

/**
 * Helper function to get the current connection to the database
 *
 * TODO: should this reconnect be wrapped in a mutex?
 */
static PGconn* get_pg_connection()
{
  // List all cases, figure out which need reconnection
  switch( PQstatus(global_conn) )
  {
    case CONNECTION_BAD:
      fprintf(stderr, "CONNECTION_BAD encountered: '%s'. Trying to reconnect.\n", PQerrorMessage(global_conn));
      // overwrite the existing global (EEEEEK) connection
      global_conn = connect_to_pg( pg_connection_data );
      return global_conn;


    case CONNECTION_NEEDED:
      // Postgres 9 Docs is silent about this enum value. What does this state mean?
      // TODO: is it safe to return conn here?
      return global_conn;


    case CONNECTION_SETENV:
    case CONNECTION_AUTH_OK:
    case CONNECTION_SSL_STARTUP:
    case CONNECTION_MADE:
    case CONNECTION_AWAITING_RESPONSE:
    case CONNECTION_STARTED:
    case CONNECTION_OK:
      return global_conn;
  }
}


int TFS_WG_IO_operation(tfs_wg_operations_t op, const uint64_t loid,
    const char * src, char * dst, const size_t size, const off_t offset)
{
  PGresult *res;
  int fd, ret = 0;
  int mode;

  // get the connection via a reconnect-capable backer
  PGconn* conn = get_pg_connection();


  if ( op == TFS_WG_READ )
    mode = INV_READ;
  else
    mode = INV_WRITE;

  // LO operations only supported within transactions
  // On our FS one read is one transaction
  // While libpq is thread safe, still, we cannot have parallel
  // transactions from multiple threads on the same connection
  pthread_mutex_lock(&tfs_wg_transaction_block_mutex);

  res = PQexec(conn, "BEGIN");
  PQclear(res);

  fd = lo_open(conn, (Oid)loid, mode);

  fprintf(stderr, "TFS_WG_open: reading from fd %d (l:%lu:o:%tu)\n",
      fd, size, offset);

#ifdef HAVE_LO_LSEEK64
  if ( lo_lseek64(conn, fd, offset, SEEK_SET) < 0 ) {
#else
  if ( lo_lseek(conn, fd, (int)offset, SEEK_SET) < 0 ) {
#endif // HAVE_LO_LSEEK64
    ret = -EINVAL;
  } else {
    switch (op) {
      case TFS_WG_READ:
        ret = lo_read(conn, fd, dst, size);
        break;
      case TFS_WG_WRITE:
        ret = lo_write(conn, fd, src, size);
        break;
      case TFS_WG_TRUNCATE:
#ifdef HAVE_LO_TRUNCATE64
        ret = lo_truncate64(conn, fd, offset);
#else
        ret = lo_truncate(conn, fd,(size_t) offset);
#endif
        break;
      default:
        ret = -EINVAL;
        break;
    }
  }

  res = PQexec(conn, "END");
  pthread_mutex_unlock(&tfs_wg_transaction_block_mutex);

  PQclear(res);

  return ret;
}


int TFS_WG_open(const tfs_wg_node_t * node, int mode, uint64_t * fh)
{

  if (node->level != TFS_WG_FILE )
    return -EISDIR;
  else
    *fh = node->loid;

  return 0;
}

int TFS_WG_readdir(const tfs_wg_node_t * node, void * buffer,
    tfs_wg_add_dir_t filler)
{
  PGresult *res;
  int i, ret;
  size_t j, len;
  char * name;
  const char *paramValues[2] = { node->site, node->project };

  // get the connection via a reconnect-capable backer
  PGconn* conn = get_pg_connection();


  switch(node->level)
  {
    case TFS_WG_ROOT:
      res = PQexec(conn, TFS_WG_LIST_SITES);
      break;

    case TFS_WG_SITE:
      res = PQexecParams(conn, TFS_WG_LIST_PROJECTS, 1, NULL, paramValues, NULL, NULL, 0);
      break;


    case TFS_WG_PROJECT:
      res = PQexecParams(conn,
          TFS_WG_LIST_WORKBOOKS " union all " TFS_WG_LIST_DATASOURCES,
          2, NULL, paramValues, NULL, NULL, 0);
      break;

    default:
      // make sure res is initialized, so the code is clean and we dont get a warning
      res = NULL;
      fprintf(stderr, "Unknown node level found: %u\n", node->level);
      break;

  }

  if (PQresultStatus(res) != PGRES_TUPLES_OK)
  {
    fprintf(stderr, "SELECT entries failed: %s", PQerrorMessage(conn));
    // TODO: error handling
    ret = -EIO;
  } else if (PQntuples(res) == 0 ) {
    ret = -ENOENT;
  } else {
    // return a zero as the universal OK sign
    ret = 0;
    for (i = 0; i < PQntuples(res); i++) {
      name = PQgetvalue(res, i, TFS_WG_QUERY_NAME);
      len = strlen( name );

      for ( j = 0 ; j < len ; j++ )
        if ( name[j] == '/' )
          name[j] = '_';

      filler(buffer, name, NULL, 0);
    }
  }

  PQclear(res);

  return ret;
}

int TFS_WG_stat_file(tfs_wg_node_t * node)
{
  const char *paramValues[3] = { node->site, node->project, node->file };
  PGresult * res;
  int ret;

  // get the connection via a reconnect-capable backer
  PGconn* conn = get_pg_connection();

  node->st.st_blksize = TFS_WG_BLOCKSIZE;

  // basic stat stuff: file type, nlinks, size of dirs
  if ( node->level < TFS_WG_FILE) {
    node->st.st_mode = S_IFDIR | 0555;   // read only
    node->st.st_nlink = 2;
    node->st.st_size = TFS_WG_BLOCKSIZE;
    node->st.st_blocks = 1;
  } else if (node->level == TFS_WG_FILE) {
    node->st.st_mode = S_IFREG | 0444;   // read only
    node->st.st_nlink = 1;
  }

  if (node->level == TFS_WG_ROOT) {
    time(&(node->st.st_mtime));
    return 0;
  } else if (node->level == TFS_WG_SITE) {

    res = PQexecParams(conn, TFS_WG_LIST_SITES " and c.name = $1", 1, NULL,
        paramValues, NULL, NULL, 0);

  } else if (node->level ==  TFS_WG_PROJECT) {

    res = PQexecParams(conn, TFS_WG_LIST_PROJECTS " and c.name = $2",
        2, NULL, paramValues, NULL, NULL, 0);

  } else if (node->level == TFS_WG_FILE) {

    res = PQexecParams(conn,
        TFS_WG_LIST_WORKBOOKS " and $3 IN (" TFS_WG_NAMES_WITHOUT_SLASH(twb) ") "
        "union all "
        TFS_WG_LIST_DATASOURCES " and $3 IN (" TFS_WG_NAMES_WITHOUT_SLASH(tds) ") ",
        3, NULL, paramValues, NULL, NULL, 0);
  } else {
    // res defaults to NULL
    res = NULL;
  }

  if (PQresultStatus(res) != PGRES_TUPLES_OK)
  {
    fprintf(stderr, "SELECT entries failed: %s/%s", PQresultErrorMessage(res), PQerrorMessage(conn));
    // TODO: error handling
    ret = -EINVAL;
  } else if (PQntuples(res) == 0 ) {
    ret =  -ENOENT;
  } else {
    node->st.st_mtime = atoll( PQgetvalue(res, 0, TFS_WG_QUERY_MTIME) );

    if ( node->level == TFS_WG_FILE ) {
      node->loid = (uint64_t)atoll( PQgetvalue(res, 0, TFS_WG_QUERY_CONTENT) );
      node->st.st_size = atoll( PQgetvalue(res, 0, TFS_WG_QUERY_SIZE) );
      if ( node->st.st_size > 0 )
        node->st.st_blocks = (int) node->st.st_size / TFS_WG_BLOCKSIZE + 1;
    }

    ret = 0;
  }

  PQclear(res);
  return ret;
}

int TFS_WG_parse_path(const char * path, tfs_wg_node_t * node)
{
  int ret;

  if ( strlen(path) > PATH_MAX )
    return -EINVAL;
  else if ( strlen(path) == 1 && path[0] == '/' ) {
    node->level = TFS_WG_ROOT;
    return TFS_WG_stat_file(node);
  }

  memset(node, 0, sizeof(tfs_wg_node_t));
  ret = sscanf(path, "/%" _NAME_MAX "[^/]/%" _NAME_MAX "[^/]/%255[^/]s",
      node->site, node->project, node->file );

  /* sscanf returned with error */
  if (ret == EOF ) {
    return errno; // TODO: this so thread unsafe
  } else if ( strchr(node->file, '/' ) != NULL  ) {
    /* file name has / char in it */
    return -EINVAL;
  } else {

    fprintf(stderr, "TFS_WG_parse_path: site: %s proj: %s file: %s\n",
        node->site, node->project, node->file);

    // cast so the signed conversion warning goes away
    node->level = (tfs_wg_level_t)ret;

    // get stat from node
    ret = TFS_WG_stat_file(node);

    return ret;
  }
}

int TFS_WG_connect_db(const char * pghost, const char * pgport,
    const char * login, const char * pwd)
{
  // save the connection data to the global (eeeeek) state.
  struct tableau_cmdargs conn_data = {(char*)pghost, (char*)pgport, (char*)login, (char*)pwd};
  pg_connection_data = conn_data;

  // Set up the connection
  global_conn = connect_to_pg( conn_data );

  // return based on whether we have the connection
  if (global_conn == NULL) return -1;
  return 0;
}

