
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
#include <string.h>
#include <errno.h>
#include "workgroup.h"
#include "libpq-fe.h"
#include "libpq/libpq-fs.h"

#define BUFSIZE          1024
#define _NAME_MAX "255"

#define TFS_WG_ATIME_MTIME \
  ", extract(epoch from coalesce(c.created_at,'2000-01-01')) ctime" \
  ", floor(extract(epoch from c.updated_at)) mtime "

#define TFS_WG_LIST_SITES  \
  "select c.name" TFS_WG_ATIME_MTIME "from sites c"

#define TFS_WG_LIST_PROJECTS \
  "select c.name" TFS_WG_ATIME_MTIME "from projects c right outer join" \
  " sites p on (p.id = c.site_id) where p.name = $1"

#define TFS_WG_LIST_FILES \
  "select c.name ||'.twb'" TFS_WG_ATIME_MTIME "from sites ps inner join " \
  "projects pp on (pp.site_id = ps.id) left outer join workbooks c on " \
  "(pp.id = c.project_id) where ps.name = $1 and pp.name = $2"               

#define TFS_WG_GET_CONTENT_ID \
  "select content from workbooks inner join repository_data on (repository_"\
  "data.id = coalesce(repository_data_id,repository_extract_data_id)) inner "\
  "join projects on (workbooks.project_id = projects.id) inner join sites on "\
  "(sites.id = projects.site_id) where sites.name = $1 and " \
  "projects.name = $2 and workbooks.name||'.twb' = $3"


static PGconn *conn;

int TFS_WG_read(const tfs_wg_node_t * node, const int loid, 
    char * buf, const size_t size, const off_t offset)
{
  PGresult *res;
  int fd, ret = 0;

  // currently only read only mode supported
  res = PQexec(conn, "BEGIN");
  PQclear(res);

  fd = lo_open(conn, loid, INV_READ);

  fprintf(stderr, "reading file %s from fd %d (%lu:%lu\n",
      node->file, fd, size, offset);

  if ( lo_lseek(conn, fd, offset, SEEK_SET) < 0 ) {
    ret = -EINVAL;
  } else {
    ret = lo_read(conn, fd, buf, size);
  }
  
  res = PQexec(conn, "END");
  PQclear(res);

  return ret;
}


int TFS_WG_open(const tfs_wg_node_t * node, int mode)
{
  PGresult *res;
  Oid lobjId;
  const char *paramValues[3] = { node->site, node->project, node->file };

  res = PQexecParams(conn, TFS_WG_GET_CONTENT_ID, 3, NULL, paramValues,
      NULL, NULL, 0);
  
  if (PQntuples(res) == 0 ) {
    if (PQresultStatus(res) != PGRES_TUPLES_OK)
    {
      fprintf(stderr, "SELECT failed: %s", PQerrorMessage(conn));
    }
    PQclear(res);
    fprintf(stderr, "%s\n", TFS_WG_GET_CONTENT_ID );
    fprintf(stderr, "ERR: TFS_WG_open: site: '%s' proj: '%s' file: '%s'\n",
        node->site, node->project, node->file);

    return -ENOENT;
  }

  lobjId = (Oid)atoi(PQgetvalue(res, 0, 0));
  fprintf(stderr,"Got loid: %u from workbook %s\n", lobjId, node->file);
  PQclear(res);


  return lobjId;
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
    res = PQexecParams(conn, TFS_WG_LIST_FILES, 2, NULL, paramValues,
        NULL, NULL, 0);   
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

int TFS_WG_parse_path(const char * path, tfs_wg_node_t * node)
{
  int ret;

  if ( strlen(path) > PATH_MAX )
    return -EINVAL;
  else if ( strlen(path) == 1 && path[0] == '/' ) {
    node->level = TFS_WG_ROOT;
    return 0;
  }

  memset(node, 0, sizeof(tfs_wg_node_t));
  ret = sscanf(path, "/%" _NAME_MAX "[^/]/%" _NAME_MAX "[^/]/%255s",
      node->site, node->project, node->file );

  /* sscanf returned with error */
  if (ret == EOF ) {
    return errno;
  } else if ( strchr(node->file, '/' ) != NULL  ) {
    /* file name has / char in it */
    return -EINVAL;
  } else {

    fprintf(stderr, "TFS_WG_parse_path: site: %s proj: %s file: %s\n",
        node->site, node->project, node->file);

    // TODO: check if file/directory exists

    node->level = ret;
    return 0;
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

