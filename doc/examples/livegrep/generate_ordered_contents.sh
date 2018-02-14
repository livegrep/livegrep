#!/bin/bash
#
# A simple example script for sorting paths in a directory to be indexed according to
# their relevance. This can be used to help populate the ordered-contents field in
# the fs_paths entry JSON.

if [ -z "$1" ]
then
    echo 'usage: generate_ordered_contents.sh DIRECTORY'
    exit 2
fi

cd "$1"

find . -type f | awk '
         {score = 100}
  /test/ {score -= 10}
  /BUILD/ {score -= 5}
        {print score, $0}
' | sort -k1nr | sed 's/^[-0-9]* //'
