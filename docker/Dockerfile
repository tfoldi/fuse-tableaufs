FROM centos
MAINTAINER Tamas Foldi <tfoldi@starschema.net>
LABEL Description="Tableau File System (fuse-tableaufs) bundled with samba" Vendor="Starschema" Version="1.0" 
RUN yum -y update && yum -y install cmake make gcc postgresql-devel fuse-devel git vim-enhanced samba && mkdir -p /mnt/tableau
RUN cd /usr/src && git clone https://github.com/tfoldi/fuse-tableaufs.git  && \
 cd /usr/src/fuse-tableaufs && cmake . && make && make install && \
 cp docker/smb.conf /etc/samba && \
 install -m 755 docker/tableau-samba.sh /usr/bin 
EXPOSE 138/udp 139 445 445/udp
