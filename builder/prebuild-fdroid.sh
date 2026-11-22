#!/bin/bash
# phiola F-Droid builder: `prebuild` step

set -xeu

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd ..
bash "$DIR/deps.sh" android
