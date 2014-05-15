FROM ubuntu:trusty
MAINTAINER Nelson Elhage <nelhage@nelhage.com>

RUN apt-get update
RUN apt-get -y install python-software-properties
RUN sh -c 'echo deb http://us.archive.ubuntu.com/ubuntu/ precise universe > /etc/apt/sources.list.d/universe.list'
RUN apt-get update

RUN apt-get -y install libjson0-dev libgflags-dev libgit2-dev libboost-dev libsparsehash-dev nodejs build-essential libgoogle-perftools-dev libssl-dev
RUN apt-get -y install libboost-filesystem-dev libboost-system-dev
RUN apt-get -y install golang
RUN apt-get -y install mercurial git

RUN git clone https://github.com/nelhage/livegrep /livegrep
WORKDIR /livegrep
RUN make -j4 all
RUN mkdir -p gopath/src/github.com/nelhage
RUN ln -s /livegrep gopath/src/github.com/nelhage/livegrep
RUN sh -c 'env GOPATH=/livegrep/gopath go get github.com/nelhage/livegrep/livegrep'

