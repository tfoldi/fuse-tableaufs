# FUSE-TableauFS: File System on Tableau Repository [![Build Status](https://travis-ci.org/tfoldi/fuse-tableaufs.svg?branch=master)](https://travis-ci.org/tfoldi/fuse-tableaufs)

TableauFS is a FUSE based userspace file system driver built on top of Tableau's repository server. It allows to mount tableau servers with its datasources and workbooks directly to the file system. File information and contents are retrieved on-access without any local persistence or caching.  By default the file system connects directly to the postgresql database using `readonly` credentials, however, read-write mode is also implemented using twlwgadmin user. 

![working with files and directories on tableaufs](http://cdn.starschema.net/tableaufs.PNG)

## Use cases
TableauFS helps Tableau Server administrators in several cases:

 - Save/copy workbooks or data sources to other, persistent file systems
 - Run workbook audit tools like [Workbook Audit Tool](http://databoss.starschema.net/how-to-use-twb-auditor-with-tableaufs-audit-tableau-server-files-directly/) or  [TWIS](http://www.betterbi.biz/TWIS.html)
 - [Version control objects with any file based version control system (git, git-annex, subversion, mercurial, p4, etc.)](http://databoss.starschema.net/version-control-and-point-in-time-recovery-of-tableau-server-objects/)
 - Directly modify files in the repository without using any API
 - Migrate contents between servers
 - Grep strings in workbook definitions

Your Tableau published objects will be visible as ordinary files, thus, the possibilities are unlimited. For more information please visit http://databoss.starschema.net/introducing-tableaufs-file-system-on-tableau-server-repository/ 

 
## Installation

### System requirements
At the moment only Linux, FreeBSD and OSX are supported. If you have to use files from Windows you can still run it form docker or mount thru WebDAV. If required, I can make a dokan or callbackfs based windows port, however, at the moment I am happy with this *nix only version.
### Dependencies
To compile the application you need at `cmake` (=> 2.6), `libpq` (>= 9.0) and `fuse`  (=>2.9).

### Build
Just type `cmake . && make && make install`and you will have everything installed. 

### Configuring tableau server database

To exploit all features (include read-write mode) you need `tblwgadmin` or similar user with superuser privilege while for read only access `readonly` user is almost enough.  
![Enable and grant select to readonly user](http://databoss.starschema.net/wp-content/uploads/2015/05/enable-and-grant-select-to-readonly-user.png)

The steps here:

 1. Enable readonly user with `tabadmin dbpass --username readonly <password>`  
 2. Check your pgsql admin password in `tabsvc.yml` file. The default location is C:\ProgramData\Tableau\Tableau Server\config but depending on your ProgramData folder, this can be different. lease note that ProgramData folder can be hidden.
 3. Go to Tableau Server\9.0\pgsql\bin folder and issue `psql -h localhost -p 8060 -U tblwgadmin workgroup` command and paste the password from `tabsvc.yml`
 4. Execute the `grant select on pg_largeobject to readonly; `statement

to leverage the full read only experience. It will not harm your system (this is still read only) but unsupported.



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

Read operations are fully supported while write support is implemented but still highly experimental. For rw mode you need read-write access to workbooks, datasources and pg_largeobjects tables.
Last modification time is read from last\_updated columns while file sizes are actual sizes of the pg\_largeobjects.

## Questions 

**Is this supported or allowed by Tableau or any 3rd party?**

Tableau FS uses the supported `readonly` repository user by default which is supported and allowed by Tableau. The file system part is completely unsupported, however, we are trying to help you on best effort basis.

Accessing your repository in read-write mode is not supported.

**I need this on windows, can I?**

You should ramp up a unix server, install a webdav server on it (like apache+mod_dav) and mount it. The Docker image with webdav + tableaufs is here: https://registry.hub.docker.com/u/tfoldi/fuse-tableaufs/

**Do you have a binary installer?**

No, but you can find a Dockerfile in docker folder, that should help.

**Can I move my files to a version control repo?**

Sure you can, I will add some documents on how to do it soon.

**I have strange error regarding fuse device when I execute TableauFS from docker**

Add `--privileged` to your  `docker exec` command line.

## Issues / Known bugs


Please send a pull request or open an issue if you have any problems.

## License

Copyright . 2015 Tamas Foldi ([@tfoldi](http://twitter.com/tfoldi)), [Starschema](http://www.starschema.net/) Ltd.

Distributed under BSD License.
