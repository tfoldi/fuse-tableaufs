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
#include <stddef.h>
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
  return TFS_WG_IO_operation(TFS_WG_READ, fi->fh, NULL, buf, size, offset);
}

static int tableau_write(const char *path, const char *buf, size_t size, off_t offset,
    struct fuse_file_info *fi)
{
  return TFS_WG_IO_operation(TFS_WG_WRITE, fi->fh, buf, NULL, size, offset);
}

static int tableau_truncate(const char *path, off_t offset)
{
  tfs_wg_node_t node;
  int ret;

  TFS_WG_PARSE_PATH(path, &node);
  if (node.level != TFS_WG_FILE )
    ret = -EISDIR;
  else
    ret = TFS_WG_IO_operation(TFS_WG_TRUNCATE, node.loid, NULL, NULL, 0, offset);

  return ret;
}

static struct fuse_operations tableau_oper = {
  .getattr        = tableau_getattr,
  .readdir        = tableau_readdir,
  .open           = tableau_open,
  .read           = tableau_read,
  .write          = tableau_write,
  .truncate       = tableau_truncate,
};

struct tableau_cmdargs { 
  char *pghost; 
  char *pgport; 
  char *pguser; 
  char *pgpass; 
};

static struct tableau_cmdargs tableau_cmdargs;
static struct fuse_opt tableaufs_opts[] =
{
    { "pghost=%s", offsetof(struct tableau_cmdargs, pghost), 0 },
    { "pgport=%s", offsetof(struct tableau_cmdargs, pgport), 0 },
    { "pguser=%s", offsetof(struct tableau_cmdargs, pguser), 0 },
    { "pgpass=%s", offsetof(struct tableau_cmdargs, pgpass), 0 }
};

int main(int argc, char *argv[])
{
  struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

  if (fuse_opt_parse(&args, &tableau_cmdargs, tableaufs_opts, 0) == -1)
    return -1;


  if (tableau_cmdargs.pguser == NULL ||
      tableau_cmdargs.pghost == NULL ||
      tableau_cmdargs.pgport == NULL ||
      tableau_cmdargs.pgpass == NULL) {
    fprintf(stderr, "Error: You should specify all of the following mount options:\n");
    fprintf(stderr, "\tpghost pgport pguser pgpass\n");
    return -1;
  }

  printf("Connecting to %s@%s:%s\n", tableau_cmdargs.pguser, 
      tableau_cmdargs.pghost, tableau_cmdargs.pgport );

  TFS_WG_connect_db( tableau_cmdargs.pghost, tableau_cmdargs.pgport,
      tableau_cmdargs.pguser, tableau_cmdargs.pgpass);

  return fuse_main(args.argc, args.argv, &tableau_oper, NULL);
}
