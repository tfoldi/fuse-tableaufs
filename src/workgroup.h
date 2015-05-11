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

#ifndef tableaufs_workgroup_h
#define tableaufs_workgroup_h
#include <stdint.h>
#include <limits.h>
#include <fcntl.h>

typedef enum
{
  TFS_WG_ROOT = 0,
  TFS_WG_SITE = 1,
  TFS_WG_PROJECT = 2,
  TFS_WG_FILE = 3
} tfs_wg_level_t;

typedef struct tfs_wg_node_t {
  tfs_wg_level_t level;  // level inside the mount point
  char site[NAME_MAX+1]; // site name
  char project[NAME_MAX+1]; // project name
  char file[NAME_MAX+1]; // Workbook/Datasource name
  uint64_t loid;  // repo id, if file
  struct stat st; // file stat info
} tfs_wg_node_t;

typedef enum {
  TFS_WG_QUERY_NAME = 0,
  TFS_WG_QUERY_MTIME = 1,
  TFS_WG_QUERY_CONTENT = 2,
  TFS_WG_QUERY_SIZE = 3
} tfs_wg_list_query_cols_t;

typedef int(* tfs_wg_add_dir_t )(void *buf, const char *name, 
    const struct stat *stbuf, off_t off);

extern int TFS_WG_read(const uint64_t fd, char * buf, const size_t size, 
    const off_t offset);

extern int TFS_WG_open(const tfs_wg_node_t * node, int mode, uint64_t * fh);

extern int TFS_WG_readdir(const tfs_wg_node_t * node, void * buffer,
    tfs_wg_add_dir_t filler);

extern int TFS_WG_connect_db(const char * pghost, const char * pgport,
    const char * login, const char * pwd);

extern int TFS_WG_parse_path(const char * path, tfs_wg_node_t * node);

#endif /* tableaufs_workgroup_h */
