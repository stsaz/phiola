# Third-party audio libraries

In order to be used by phiola all these libraries must be built in a specific way.

* Use plain and simple make files rather than with the official (sometimes unnecessarily huge and complex) make files.
* Sometimes the call to configure script is necessary to generate header files (e.g. `config.h`).
* For each library there's a wrapper that provides a different API that's easier to use and more suitable for phiola.
* Patches must be applied for some of the wrappers to work correctly.
* Some functionality may be removed or disabled.
* The resulting binaries aren't compatible with any applications that use the official builds, so to eliminate ambiguity the file names have "-phi" suffix, e.g. "libNAME-phi.so".

## Libs

Codecs:

* alac-rev2
* fdk-aac-0.1.6
* flac-1.4.3
* MAC-433
* mpg123-1.32.10
* musepack-r475
* ogg-1.3.3
* opus-1.5.2
* vorbis-1.3.7
* wavpack-4.75.0

Filters:

* DynamicAudioNormalizer-2.10
* libebur128-1.2.6
* soxr-0.1.3


## Requirements

* Internet connection (if an archive file doesn't yet exist)
* make
* cmake (for libsoxr)
* dos2unix (for libMAC)
* patch
* gcc/clang
* g++/clang++


## Build

	make -j8


## LICENSE

This directory contains copies of original and auto-generated code from 3rd party libraries.  This code is the property of their owners.  This code and binary files created from this code are licensed accordingly to the licenses of those libraries.

All other code provided here is absolutely free.
