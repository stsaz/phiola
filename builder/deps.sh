#!/bin/bash
# phiola builder: download dependency repositories into the current dir

set -xeu

if [[ $# != 1 ]]; then
  echo "Usage: deps.sh <target>"
  exit 1
fi
TARGET="$1"

git clone https://github.com/stsaz/netmill
git -C netmill checkout b9a91599cf60fcb2ce262b7d8ad4262429ac9f90

git clone https://github.com/stsaz/avpack
git -C avpack checkout 88c02531561207c04afd456c2f148a93e0942b29

git clone https://github.com/stsaz/ffaudio
git -C ffaudio checkout 6a951156e68e33d3925076e680ac56478a648c74

git clone https://github.com/stsaz/ffpack
git -C ffpack checkout 5e2cfa08583fcf7f00adbdf285876620d73557cf

git clone https://github.com/stsaz/ffsys
git -C ffsys checkout aa2f6730a2441ae1b137d32c1da16640e688e810

git clone https://github.com/stsaz/ffbase
git -C ffbase checkout 2a7030bb4df67529f6180ae2f86b0a494df179ad

if [[ "$TARGET" == linux* || "$TARGET" == windows* ]]; then
  git clone https://github.com/stsaz/ffgui
  git -C ffgui checkout f5b0014bc9e8a1cccf97df2c5b84b72045ef6e68
fi

if [[ "$TARGET" == windows* ]]; then
  git clone https://github.com/stsaz/winapi-dark-theme
  git -C winapi-dark-theme checkout c0003bb96468d265be0172bd5625a296d984ee6a
fi
