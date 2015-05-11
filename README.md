# FUSE-TableauFS: File System on Tableau Repository
TableauFS is a FUSE based userspace file system driver built on top of Tableau's repository server. It allows to mount tableau servers with its datasources and workbooks directly to the file system. File information and contents are retrieved on-access without any local persistence or caching.  The file system connects directly to the postgresql database using `readonly` credentials. 

![working with files and directories on tableaufs](http://cdn.starschema.net/tableaufs.PNG)

## Use cases
TableauFS helps Tableau Server administrators in several cases:

 - Save/copy workbooks or data sources to other, persistent file systems
 - Run workbook audit tools like [Workbook Audit Tool](http://community.tableau.com/thread/118450) or  [TWIS](http://www.betterbi.biz/TWIS.html)
 - Version control objects with any file based version control system (git, git-annex, subversion, mercurial, p4, etc.)
 - Directly modify files in the repository without using any API
 - Migrate contents between servers
 - Grep strings in workbook definitions

Your Tableau published objects will be visible as ordinary files, thus, the possibilities are unlimited.

 
## Installation

### System requirements
At the moment only Linux, FreeBSD and OSX are supported. If you have to use files from Windows you can still run it form docker or mount thru WebDAV. If required, I can make a dokan or callbackfs based windows port, however, at the moment I am happy with this *nix only version.
### Dependencies
To compile the application you need at `cmake` (=> 2.6), `libpq` (>= 9.0) and `fuse`  (=>2.9).

### Build
Just type `cmake && make && make install`and you will have everything installed. 

## Usage
To mount a tableau server type:

    # tableaufs -o pghost=tabsrver.local,pgport=8060,pguser=readonly,pgpass=readonly /mnt/tableau-dev

where -o stands for file system options. After executing you will see the file system in the mount list:

    tableaufs on /mnt/tableau-dev type fuse.tableaufs (rw,nosuid,nodev,relatime,user_id=0,group_id=0)

To unmount, simple `umount /mnt/tableau-dev`

## Directory structure & File operations

TableauFS maps Tableau repository to the following directory structure:

    /Sitename/Projectname/Workbook 1.tbw[x] 
    /Sitename/Projectname/Workbook 2.tbw[x] 
    /Sitename/Projectname/Datasource 1.tds[x] 

At the moment only read operations are supported but adding write support is a question about ten lines of code.

Last modification time, file size and other file system stat 

## Questions 

**Is this supported or allowed by Tableau or any 3rd party?**

Tableau FS uses only the supported `readonly` repository user which is supported and allowed by Tableau. For the file system part is completely unsupported, however, we are trying to help you on best effort basis.

**I need this on windows, can I?**

You should ramp up a unix server, install a webdav server on it (like apache+mod_dav) and mount it. Native windows driver is also on my backlog without any ETA.

**Do you have a binary installer?**

No, but if you need I can send you a `Dockerfile` if that helps. 

**Can I move my files to a version control repo?**

Sure you can, I will add some documents on how to do it soon.

**I have strange error regarding fuse device when I execute TableauFS from docker**

Add `--privileged` to your  `docker exec` command line.

## Issues / Known bugs

 - Objects with `/`  in their file names are not supported.

Please send a pull request or open an issue if you have any problems.

## License

Copyright . 2015 Tamas Foldi ([@tfoldi](http://twitter.com/tfoldi)), [Starschema](http://www.starschema.net/) Ltd.

Distributed under BSD License.
