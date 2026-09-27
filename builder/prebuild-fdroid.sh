#!/bin/bash
# phiola F-Droid builder: `prebuild` step

set -xeu

cd ..

git clone https://github.com/stsaz/netmill
git -C netmill checkout 9057306955a03ca634dca9592354f78bbcc28c98

git clone https://github.com/stsaz/avpack
git -C avpack checkout b0110b43fefbe3804c36304c7722fc0e82e56552

git clone https://github.com/stsaz/ffaudio
git -C ffaudio checkout 6a951156e68e33d3925076e680ac56478a648c74

git clone https://github.com/stsaz/ffpack
git -C ffpack checkout d16dbae8ac965dc1cf279564aac2b571476b9380

git clone https://github.com/stsaz/ffsys
git -C ffsys checkout 055668856113357f2110df8feec2f1d1d740cabd

git clone https://github.com/stsaz/ffbase
git -C ffbase checkout 00dc6ca9ffced98699898cc4405f041a9d0406d3
