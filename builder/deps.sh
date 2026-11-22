#!/bin/bash
# phiola builder: download dependency repositories into the current dir

set -xeu

if [[ $# != 1 ]]; then
  echo "Usage: deps.sh <target>"
  exit 1
fi
TARGET="$1"

git clone https://github.com/stsaz/netmill
git -C netmill checkout 7f78e024f37497c977ae9106b013043cc1f34af0

git clone https://github.com/stsaz/avpack
git -C avpack checkout 88c02531561207c04afd456c2f148a93e0942b29

git clone https://github.com/stsaz/ffaudio
git -C ffaudio checkout 6a951156e68e33d3925076e680ac56478a648c74

git clone https://github.com/stsaz/ffpack
git -C ffpack checkout d16dbae8ac965dc1cf279564aac2b571476b9380

git clone https://github.com/stsaz/ffsys
git -C ffsys checkout 1f5881e8252db1ceb00a690c6098d424841a9df6

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
