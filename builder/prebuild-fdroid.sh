#!/bin/bash
# phiola F-Droid builder: `prebuild` step

set -xeu

cd ..

git clone https://github.com/stsaz/netmill
git -C netmill checkout d946fa7f4234db5f5965125883be7b9140fe84bb

git clone https://github.com/stsaz/avpack
git -C avpack checkout dd36adefeb24f6002a1a882061510a3988c4be9f

git clone https://github.com/stsaz/ffaudio
git -C ffaudio checkout 6a951156e68e33d3925076e680ac56478a648c74

git clone https://github.com/stsaz/ffpack
git -C ffpack checkout d16dbae8ac965dc1cf279564aac2b571476b9380

git clone https://github.com/stsaz/ffsys
git -C ffsys checkout 1f5881e8252db1ceb00a690c6098d424841a9df6

git clone https://github.com/stsaz/ffbase
git -C ffbase checkout 00dc6ca9ffced98699898cc4405f041a9d0406d3
