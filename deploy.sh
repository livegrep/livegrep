#!/bin/sh

set -eux

tarball="$1"

basedir="${tarball%.*}"

tar -C /opt/services/livegrep -xzv < "$tarball"
ln -nsf "$basedir" /opt/services/livegrep/current
sudo svc -t /etc/service/livegrep*
