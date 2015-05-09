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

#define FUSE_USE_VERSION 30
#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include "workgroup.h"

#define TFS_WG_PARSE_PATH( path, node ) \
{ \
  int __ret = TFS_WG_parse_path(path, node);\
  if (__ret < 0){  \
    return __ret; \
  }; \
} while (0)  

static int tableau_getattr(const char *path, struct stat *stbuf)
{
  int res = 0;
  tfs_wg_node_t node;

  TFS_WG_PARSE_PATH(path, &node);

  memcpy(stbuf, &(node.st), sizeof(struct stat));
  
  return res;
}

static int tableau_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
    off_t offset, struct fuse_file_info *fi)
{
  tfs_wg_node_t node;

  TFS_WG_PARSE_PATH(path, &node);

  if ( node.level == TFS_WG_FILE )
    return -ENOTDIR;

  filler(buf, ".", NULL, 0);
  filler(buf, "..", NULL, 0);

  TFS_WG_readdir(&node, buf, filler);

  return 0;
}

static int tableau_open(const char *path, struct fuse_file_info *fi)
{  
  tfs_wg_node_t node;
  int ret;

  TFS_WG_PARSE_PATH(path, &node);

  ret = TFS_WG_open(&node, fi->flags, &(fi->fh) );
  fi->direct_io = 1; // during read we can return smaller buffer than
                     // requested

  return ret;
}

static int tableau_read(const char *path, char *buf, size_t size, off_t offset,
    struct fuse_file_info *fi)
{
  if(fi->fh < 0)
    return -EBADF;

  return TFS_WG_read(fi->fh, buf, size, offset);
}

static int tableau_release(const char *path, struct fuse_file_info *fi)
{
  return 0;
}

static struct fuse_operations tableau_oper = {
  .getattr        = tableau_getattr,
  .readdir        = tableau_readdir,
  .open           = tableau_open,
  .read           = tableau_read,
  .release        = tableau_release,
};

int main(int argc, char *argv[])
{

  TFS_WG_connect_db( "localhost", "5432", "postgres", "readonly" );

  return fuse_main(argc, argv, &tableau_oper, NULL);
}
