FROM ubuntu:16.04

ENV DEBIAN_FRONTEND noninteractive

RUN apt-get update && apt-get -y install libgflags-dev libgit2-dev libjson0-dev \
  libboost-system-dev libboost-filesystem-dev libsparsehash-dev \
  build-essential git mercurial libssl-dev cmake

ENV GOLANG_VERSION 1.7rc6

ADD http://golang.org/dl/go${GOLANG_VERSION}.linux-amd64.tar.gz /usr/local/src/go${GOLANG_VERSION}.tgz
RUN tar -C /usr/local -xzf /usr/local/src/go${GOLANG_VERSION}.tgz
ENV GOROOT /usr/local/go

ENV PATH $PATH:/usr/local/go/bin
ADD . /livegrep
WORKDIR /livegrep
RUN make -j4 all
