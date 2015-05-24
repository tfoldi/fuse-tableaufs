FROM centos
RUN yum -y update && yum -y install cmake make gcc postgresql-devel fuse-devel git vim-enhanced && mkdir -p /mnt/tableau
RUN cd /usr/src && git clone https://github.com/tfoldi/fuse-tableaufs.git
RUN cd /usr/src/fuse-tableaufs && cmake . && make && make install

