
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

#define TFS_WG_MTIME_SIZE \
  TFS_WG_MTIME            \
  ", size  " 

#define TFS_WG_LIST_SITES  \
  "select c.name" TFS_WG_MTIME "from sites c where 1 = 1 "

#define TFS_WG_LIST_PROJECTS \
  "select c.name" TFS_WG_MTIME "from projects c right outer join" \
  " sites p on (p.id = c.site_id) where p.name = $1"

#define TFS_WG_LIST_WORKBOOKS \
  "select c.name ||'.twbx'" TFS_WG_MTIME_SIZE "from sites ps inner join " \
  "projects pp on (pp.site_id = ps.id) left outer join workbooks c on " \
  "(pp.id = c.project_id) where ps.name = $1 and pp.name = $2 "   \

#define TFS_WG_LIST_DATASOURCES \
  "select c.name ||'.tdsx'" TFS_WG_MTIME_SIZE "from sites ps inner join " \
  "projects pp on (pp.site_id = ps.id) left outer join datasources c on " \
  "(pp.id = c.project_id) where ps.name = $1 and pp.name = $2"               

#define TFS_WG_GET_CONTENT_ID \
  "select content from workbooks inner join repository_data on (repository_"\
  "data.id = coalesce(repository_data_id,repository_extract_data_id)) inner "\
  "join projects on (workbooks.project_id = projects.id) inner join sites on "\
  "(sites.id = projects.site_id) where sites.name = $1 and " \
  "projects.name = $2 and workbooks.name||'.twbx' = $3 " \
  "union all " \
  "select content from datasources inner join repository_data on (repository_"\
  "data.id = coalesce(repository_data_id,repository_extract_data_id)) inner "\
  "join projects on (datasources.project_id = projects.id) inner join sites on "\
  "(sites.id = projects.site_id) where sites.name = $1 and " \
  "projects.name = $2 and datasources.name||'.tdsx' = $3"


static PGconn *conn;

int TFS_WG_read(const uint64_t loid, char * buf, const size_t size, const off_t offset)
{
  PGresult *res;
  int fd, ret = 0;

  // LO operations only supported within transactions
  // On our FS one read is one transaction
  res = PQexec(conn, "BEGIN");
  PQclear(res);

  fd = lo_open(conn, (Oid)loid, INV_READ);

  fprintf(stderr, "TFS_WG_open: reading from fd %d (l:%lu:o:%lu)\n",
      fd, size, offset);

  if ( lo_lseek(conn, fd, (int)offset, SEEK_SET) < 0 ) {
    ret = -EINVAL;
  } else {
    ret = lo_read(conn, fd, buf, size);
  }
  
  res = PQexec(conn, "END");
  PQclear(res);

  return ret;
}


int TFS_WG_open(const tfs_wg_node_t * node, int mode, uint64_t * fh)
{
  PGresult *res;
  const char *paramValues[3] = { node->site, node->project, node->file };

  if ((mode & 3) != O_RDONLY)
    return -EACCES;

  // get large object identifier from 
  res = PQexecParams(conn, TFS_WG_GET_CONTENT_ID, 3, NULL, paramValues,
      NULL, NULL, 0); 

  if (PQntuples(res) == 0 ) {
    if (PQresultStatus(res) != PGRES_TUPLES_OK)
    {
      // query failed
      fprintf(stderr, "SELECT failed: %s", PQerrorMessage(conn));
      PQclear(res);
    } else {
      // no rows: no such file or directory
      // we should rarely reach this part
      // as parse path already checked this node
      PQclear(res);
      fprintf(stderr, "TFS_WG_open: ENOENT site: '%s' proj: '%s' file: '%s'\n",
          node->site, node->project, node->file);

      return -ENOENT;
    }
  }

  *fh = (Oid)atoll(PQgetvalue(res, 0, 0));
  fprintf(stderr,"TFS_WG_open: Got loid: %li from workbook %s\n", *fh, node->file);
  PQclear(res);


  return 0;
}

int TFS_WG_readdir(const tfs_wg_node_t * node, void * buffer,
    tfs_wg_add_dir_t filler)
{
  PGresult *res;
  int i;
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
    PQclear(res);
    // TODO: error handling
    return -1;
  }

  // no records = parent not found
  if (PQntuples(res) == 0 ) {
    PQclear(res);
    return -ENOENT;
  }

  for (i = 0; i < PQntuples(res); i++)
    if ( PQgetvalue(res, i, 0)[0] != '\0' )
      filler(buffer, PQgetvalue(res, i, 0), NULL, 0);

  PQclear(res);

  return 0;
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
        TFS_WG_LIST_WORKBOOKS " and c.name||'.twbx' = $3 union all " 
        TFS_WG_LIST_DATASOURCES " and c.name||'.tdsx' = $3", 
        3, NULL, paramValues, NULL, NULL, 0);   
  }

  if (PQresultStatus(res) != PGRES_TUPLES_OK)
  {
    fprintf(stderr, "SELECT entries failed: %s", PQerrorMessage(conn));
    // TODO: error handling
    ret = -EINVAL;
  } else if (PQntuples(res) == 0 ) {
    // no records = parent not found
    ret =  -ENOENT;
  } else if ( PQgetvalue(res, 0, 0)[0] == '\0' ) {
    // null for name, no child
    ret = -ENOENT;
  } else {
    node->st.st_mtime = atoll( PQgetvalue(res, 0, 1) );

    if ( node->level == TFS_WG_FILE )
      node->st.st_size = atoll( PQgetvalue(res, 0, 2) );
   
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

