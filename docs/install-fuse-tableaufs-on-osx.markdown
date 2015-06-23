# Installing FUSE-TableauFS on OSX

This guide tries to help you with installing FUSE-TableaFS on your OSX
machine.

For brewitys sake in the following guide, we'll use the acronym TFS instead of writing
FUSE-TableauFS each and every time.

## Pre-requisites

You'll need to install a bunch of libraries and command-line utilities
to compile TFS:

- XCode Developer Tools (with Command Line Support)
- cmake (=> 2.6)
- libpq (>= 9.0)
- fuse (=> 2.9)

We recommend installing these pre-requisites using
[HomeBrew](http://brew.sh/).


#### XCode Developer Tools

XCode Developer Tools gives us the much needed compilers, and tooling
necessary to compile C source code and is necessary for using HomeBrew.

You can download the latest XCode from the App Store or from
[Apple Developer Download Page](https://developer.apple.com/xcode/downloads/).

A detailed [guide to install the XCode Command Line development tools for Yosemite]
(https://railsapps.github.io/xcode-command-line-tools.html). Please note
that this guide uses GCC as the example, but the gcc executable is just
a wrapper for CLang handling the GCC command line switches properly.

After installation check if you're successful:

```
$ clang --version
Apple LLVM version 6.0 (clang-600.0.51) (based on LLVM 3.5svn)
Target: x86_64-apple-darwin13.4.0
Thread model: posix
```


#### CMake

Installing CMake is fairly straightforward

```
brew install cmake
```

After installation check if you're successful:

```
$ cmake --version
cmake version 3.0.2

CMake suite maintained and supported by Kitware (kitware.com/cmake).
```

#### libpq

If you dont already have postgresql installed, the easiest way to
install the headers necessary is to install postgres using brew:

```
$ brew install postgresql
```

#### FUSE

[FUSE for OSX](http://osxfuse.github.io) is a separate project from
FUSE. You can download the latest release from the
[osxfuse sourceforge files](http://sourceforge.net/projects/osxfuse/files/)
or the [osxfuse github releases page](https://github.com/osxfuse/osxfuse/releases).

We recommend installing a recent version (we have tested using 3.0.3
released on 15. May 2015).


## Compiling the source code

As TFS is in early stages of its lifecycle consider it a moving target, so we recommend
cloning the source repository instead of downloading a source archive.

`$ git clone https://github.com/tfoldi/fuse-tableaufs.git`

Then create a separate directory for building it (out-of-source builds
are the recommended way of working with CMake):

```
$ mkdir fuse-tableaufs-build
$ cd fuse-tableaufs-build
```

Then use cmake to generate the makefiles (replace `../fuse-tableaufs` with the relative
path of the source folder from the build folder):

```
$ cmake . ../fuse-tableaufs
$ make clean tableaufs
```

After a short compilation, the tableaufs executable is available in the
src folder.


## Installing


`$ sudo make install`

this will install the `tableaufs` command into `/usr/local/bin` and
symlink it as `/sbin/mount_tableaufs` for using with automount.



## Automounting

Work in progress...





