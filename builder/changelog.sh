#!/bin/bash
# phiola: Generate changelog from git commit messages between two refs

set -xe

if [[ $# != 2 ]]; then
    echo "Usage: $0 SINCE TO"
    exit 1
fi

SINCE=$1
TO=$2

git log --format=%B $SINCE..$TO \
    | grep -P '^[\+\!\-\*]' \
    | sed -e 's/^\+/1\+/' -e 's/^\!/2\!/'  -e 's/^\*/3\*/' -e 's/^\-/4\-/' \
    | LC_TYPE=C sort -V \
    | cut -c 2-
