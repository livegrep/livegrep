FROM ubuntu:20.04
RUN apt-get update \
    && apt-get -y dist-upgrade \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y git \
    && apt-get clean

ARG livegrep_version
COPY ./builds/${livegrep_version}.tgz /livegrep.tgz

RUN tar -C / -xzvf /livegrep.tgz
COPY ./docker/nginx/nginx.conf /${livegrep_version}/nginx.conf
RUN ln -nsf /${livegrep_version} /livegrep
