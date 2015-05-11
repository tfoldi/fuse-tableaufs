
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
#include "workgroup.h"
#include "libpq-fe.h"
#include "libpq/libpq-fs.h"

#define BUFSIZE          1024
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


static PGconn *conn;

int TFS_WG_IO_operation(tfs_wg_operations_t op, const uint64_t loid, char * buf, 
    const size_t size, const off_t offset)
{
  PGresult *res;
  int fd, ret = 0;
  int mode;

  if ( op == TFS_WG_READ )
    mode = INV_READ;
  else
    mode = INV_WRITE;

  // LO operations only supported within transactions
  // On our FS one read is one transaction
  res = PQexec(conn, "BEGIN");
  PQclear(res);

  fd = lo_open(conn, (Oid)loid, mode);

  fprintf(stderr, "TFS_WG_open: reading from fd %d (l:%lu:o:%lu)\n",
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
        ret = lo_read(conn, fd, buf, size);
        break;
      case TFS_WG_WRITE:
        ret = lo_write(conn, fd, buf, size);
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
  const char *paramValues[2] = { node->site, node->project };

  if (node->level == TFS_WG_ROOT) 
  {
    res = PQexec(conn, TFS_WG_LIST_SITES);
  } else if (node->level ==  TFS_WG_SITE ) {
    res = PQexecParams(conn, TFS_WG_LIST_PROJECTS, 1, NULL, paramValues,
        NULL, NULL, 0);   
  } else if (node->level == TFS_WG_PROJECT) {
    res = PQexecParams(conn, 
        TFS_WG_LIST_WORKBOOKS " union all " TFS_WG_LIST_DATASOURCES, 
        2, NULL, paramValues, NULL, NULL, 0);   
  }


  if (PQresultStatus(res) != PGRES_TUPLES_OK)
  {
    fprintf(stderr, "SELECT entries failed: %s", PQerrorMessage(conn));
    // TODO: error handling
    ret = -EIO;
  } else if (PQntuples(res) == 0 ) {
    ret = -ENOENT;
  } else {
    for (i = 0; i < PQntuples(res); i++)
      filler(buffer, PQgetvalue(res, i, TFS_WG_QUERY_NAME), NULL, 0);
  }

  PQclear(res);

  return ret;
}

int TFS_WG_stat_file(tfs_wg_node_t * node)
{
  const char *paramValues[3] = { node->site, node->project, node->file };
  PGresult * res;
  int ret;

  // basic stat stuff: file type, nlinks, size of dirs
  if ( node->level < TFS_WG_FILE) {
    node->st.st_mode = S_IFDIR | 0555;   // read only
    node->st.st_nlink = 2;
    node->st.st_size = 0;
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
        TFS_WG_LIST_WORKBOOKS " and $3 IN ( c.name||'.twbx', c.name||'.twb') "
        "union all " 
        TFS_WG_LIST_DATASOURCES " and $3 IN ( c.name||'.tdsx', c.name||'.tds') ", 
        3, NULL, paramValues, NULL, NULL, 0);   
  }

  if (PQresultStatus(res) != PGRES_TUPLES_OK)
  {
    fprintf(stderr, "SELECT entries failed: %s", PQerrorMessage(conn));
    // TODO: error handling
    ret = -EINVAL;
  } else if (PQntuples(res) == 0 ) {
    ret =  -ENOENT;
  } else {
    node->st.st_mtime = atoll( PQgetvalue(res, 0, TFS_WG_QUERY_MTIME) );

    if ( node->level == TFS_WG_FILE ) {
      node->st.st_size = atoll( PQgetvalue(res, 0, TFS_WG_QUERY_SIZE) );
      node->loid = (uint64_t)atoll( PQgetvalue(res, 0, TFS_WG_QUERY_CONTENT) );
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

    node->level = ret;
    
    // get stat from node
    ret = TFS_WG_stat_file(node);

    return ret;
  }
}

int TFS_WG_connect_db(const char * pghost, const char * pgport,
    const char * login, const char * pwd)
{

  /*
   * set up the connection
   */
  conn = PQsetdbLogin(pghost, pgport, NULL, NULL, "workgroup", login, pwd );

  /* check to see that the backend connection was successfully made */
  if (PQstatus(conn) == CONNECTION_BAD)
  {
    fprintf(stderr, "Connection to database '%s' failed.\n", pghost);
    fprintf(stderr, "%s", PQerrorMessage(conn));
    PQfinish(conn);
    return -1;
  } 

  return 0;
}

