
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
#include <string.h>
#include <errno.h>
#include "workgroup.h"
#include "libpq-fe.h"
#include "libpq/libpq-fs.h"

#define BUFSIZE          1024
#define _NAME_MAX "255"

static PGconn *conn;

int TFS_WG_parse_path(const char * path, tfs_wg_node_t * node)
{
  int ret;

  if ( strlen(path) > PATH_MAX )
    return -EINVAL;

  memset(node, 0, sizeof(tfs_wg_node_t));
  ret = sscanf(path, "/%" _NAME_MAX "[^/]/%" _NAME_MAX "[^/]/%255s",
      node->site, node->project, node->file );

  if (ret < 0 ) {
    return ret;
  } else {

    fprintf(stderr, "TFS_WG_parse_path: site: %s proj: %s file: %s\n",
        node->site, node->project, node->file);

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

