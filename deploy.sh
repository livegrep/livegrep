#!/bin/sh

set -eux

host="$1"
tarball="$2"

basedir="${tarball%.*}"

ssh "$host" tar -C /opt/services/livegrep -xzv < "$tarball"
ssh "$host" ln -nsf "$basedir" /opt/services/livegrep/current
ssh "$host" sudo svc -t /etc/service/livegrep*
