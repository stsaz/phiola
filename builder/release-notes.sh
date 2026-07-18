#!/bin/bash
# phiola: Generate release notes for the given tag

if [[ $# -lt 1 || $# -gt 2 ]]; then
	echo "Usage: $0 TAG [SINCE]"
	exit 1
fi

set -xe
TAG="$1"

if [[ $# -eq 2 ]]; then
	SINCE="$2"
else
	# Find previous tag
	SINCE=$(git tag --sort=-version:refname | grep -A1 "^${TAG}$" | tail -1)
fi

# "vX.Y" -> "X.Y"
VERSION="${TAG#v}"

# RPM: "X.Y-betaZ" -> "X.Y.0.betaZ"
RPM_VER="${VERSION//-/.0.}"

URL="https://github.com/stsaz/phiola/releases/download/$TAG"

{
	echo "Changes since ${SINCE}:"
	echo '```'
	git log --format=%B "$SINCE..$TAG" \
		| grep -P '^[\+\!\-\*]' \
		| sed -e 's/^\+/1\+/' -e 's/^\!/2\!/' -e 's/^\*/3\*/' -e 's/^\-/4\-/' \
		| sort -V \
		| cut -c 2-
	echo '```'
	echo ""
	echo "## Download Options"
	echo ""
	echo "| Platform | Download Options |"
	echo "| --- | --- |"
	echo "| Linux (x86_64) | [AMD64 deb]($URL/phiola_${VERSION}_amd64.deb) / [x86_64 rpm]($URL/phiola-$RPM_VER-1.x86_64.rpm) / [x86_64 Package]($URL/phiola-$VERSION-linux-x86_64.tar.zst) |"
	echo "| Linux (aarch64/RPi) | [ARM64 deb]($URL/phiola_${VERSION}_arm64.deb) / [Aarch64 rpm]($URL/phiola-$RPM_VER-1.aarch64.rpm) / [Aarch64 Package]($URL/phiola-$VERSION-linux-aarch64.tar.zst) |"
	echo "| Windows (x64) | [x64 Installer]($URL/phiola-$VERSION-windows-x64-setup.exe) / [x64 ZIP]($URL/phiola-$VERSION-windows-x64.zip) |"
} > release-notes.txt
