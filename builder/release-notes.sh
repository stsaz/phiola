#!/bin/bash
# phiola: Generate release notes for the given tag

if [[ $# != 1 ]]; then
	echo "Usage: $0 TAG"
	exit 1
fi

set -xe
TO="$1"

# Find previous tag
SINCE=$(git tag --sort=-version:refname | grep -A1 "^${TO}$" | tail -1)

{
	echo "Changes since ${SINCE}:"
	echo '```'
	git log --format=%B "$SINCE..$TO" \
		| grep -P '^[\+\!\-\*]' \
		| sed -e 's/^\+/1\+/' -e 's/^\!/2\!/' -e 's/^\*/3\*/' -e 's/^\-/4\-/' \
		| sort -V \
		| cut -c 2-
	echo '```'
} > release-notes.txt
