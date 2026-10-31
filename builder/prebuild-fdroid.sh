#!/bin/bash
# phiola F-Droid builder: `prebuild` step

set -xeu

cd ..

git clone https://github.com/stsaz/netmill
git -C netmill checkout 9bcd8c6a17d2641d493974659f0a6c69e4d31cc3

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
