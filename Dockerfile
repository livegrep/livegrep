FROM ubuntu:precise
MAINTAINER Nelson Elhage <nelhage@nelhage.com>

RUN apt-get update
RUN apt-get -y install python-software-properties
RUN apt-add-repository ppa:nelhage/livegrep
RUN apt-add-repository ppa:chris-lea/node.js
RUN sh -c 'echo deb http://us.archive.ubuntu.com/ubuntu/ precise universe > /etc/apt/sources.list.d/universe.list'
RUN apt-get update

RUN apt-get -y install libjson0-dev gflags-dev libgit2-dev re2-dev libboost-dev libsparsehash-dev nodejs build-essential libgoogle-perftools-dev libssl-dev

ADD https://github.com/nelhage/livegrep/archive/master.tar.gz /livegrep-master.tar.gz
RUN mkdir /livegrep
RUN sh -c 'cd /livegrep && tar --strip-components=1 -xzf /livegrep-master.tar.gz'
RUN sh -c 'cd /livegrep && make -j4'
RUN sh -c 'cd /livegrep && npm install'
