# Copyright (c) 2012-2015, Tamas Foldi, Starschema
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
# NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

cmake_minimum_required(VERSION 2.6)
project(fuse-tableaufs C)


set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
include(DefaultCompilerFlags)
include(FindPkgConfig)

# Try to find Postgres
find_package(Postgres 9.0 REQUIRED)

# Load FUSE
pkg_check_modules( FUSE REQUIRED fuse>=2.9.0)

# Add the include directories
include_directories(
  ${POSTGRES_INCLUDE_DIR}
  ${FUSE_INCLUDE_DIRS} )


# SUBDIRS(...) is deprecated
#
# ADD_SUBDIRECTORY processes the subdir at the time it is called,
# whereas SUBDIRS pushes the dirs onto a list which is processed
# at the end of the current cmakelists file - this is the old
# behaviour and some vars are initialized 'out of order'  -or at
# least in unexpected order
add_subdirectory(src)
