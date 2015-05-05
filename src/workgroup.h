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
#include <linux/limits.h>

typedef enum
{
  TFS_WG_ROOT = 0,
  TFS_WG_SITE = 1,
  TFS_WG_PROJECT = 2,
  TFS_WG_FILE = 3
} tfs_wg_level_t;

typedef struct tfs_wg_node_t {
  tfs_wg_level_t level;
  unsigned char site[NAME_MAX+1];
  unsigned char project[NAME_MAX+1];
  unsigned char file[NAME_MAX+1];
} tfs_wg_node_t;

extern int TFS_WG_connect_db(const char * pghost, const char * pgport,
    const char * login, const char * pwd);

extern int TFS_WG_parse_path(const char * path, tfs_wg_node_t * node);

#endif /* tableaufs_workgroup_h */
