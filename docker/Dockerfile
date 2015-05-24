FROM centos
MAINTAINER Tamas Foldi <tfoldi@starschema.net>
LABEL Description="Tableau File System (fuse-tableaufs) bundled with apache+mod_dav" Vendor="Starschema" Version="1.0" 
RUN yum -y update && yum -y install cmake make gcc postgresql-devel fuse-devel git httpd vim-enhanced && mkdir -p /mnt/tableau
RUN cd /usr/src && git clone https://github.com/tfoldi/fuse-tableaufs.git  && \
 cd /usr/src/fuse-tableaufs && cmake . && make && make install && \
 cp docker/tableaufs.conf /etc/httpd/conf.d && \
 cp docker/tableaufs.password /etc && \
 install -m +x docker/tableau-webdav.sh /usr/bin 
EXPOSE 80
