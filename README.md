# phiola

![](res/phiola.svg)

phiola *beta* - fast audio player, recorder, converter for Windows, Linux & Android.
Its low CPU consumption conserves the notebook/phone battery.
Its fast startup time allows using it from custom scripts on a "play-and-exit" or "record-and-exit" basis.
It's completely portable (all codecs are bundled) - you can run it directly from a read-only flash drive.
It's a free and open-source project, and you can use it as a standalone application or as a library for your own software.

*beta* note: still need to port some stuff from fmedia and re-test everything.

Contents:

* [Features](#features)
* [How to Use](#how-to-use)
* [Libraries](#libraries)
* [Build](#build)


# Features

* Play audio: .mp3, .ogg(Vorbis/Opus), .mp4/.mov(AAC/ALAC/MPEG), .mkv/.webm(AAC/ALAC/MPEG/Vorbis/Opus/PCM), .caf(AAC/ALAC/PCM), .avi(AAC/MPEG/PCM), .aac, .mpc; .flac, .ape, .wv, .wav
* Record audio: .m4a(AAC), .ogg, .opus; .flac, .wav
* Convert audio
* List available audio devices
* Command Line Interface for Desktop OS
* GUI for Android
* Terminal/Console UI for interaction at runtime
* Fast (low footprint): keeps your CPU, memory & disk I/O at absolute minimum; spends 99% of time inside codec algorithms

**Bonus:** Convenient API with plugin-support which allows using all the above features from any C/C++/Java app!


# How to Use

Just a few quick examples:

```sh
# Play files and directories
phiola play file.mp3 *.flac 'My Music'
# or just
phiola file.mp3 *.flac 'My Music'

# Record until stopped
phiola record -o audio.flac

# Convert
phiola convert audio.flac -o audio.m4a

# List audio devices
phiola device list

# Show meta info on all .wav files inside a directory
phiola info 'My Recordings' -include '*.wav'
```

Currently supported commands (click to view the details):

* [convert](src/exe/convert.h) - Convert audio
* [device](src/exe/device.h)   - List audio devices
* [info](src/exe/info.h)       - Show file meta data
* [play](src/exe/play.h)       - Play audio
* [record](src/exe/record.h)   - Record audio


## Libraries

phiola uses modified versions of these third party libraries: libALAC, libfdk-aac, libFLAC, libMAC, libmpg123, libmpc, libogg, libopus, libvorbisenc, libvorbis, libwavpack, libzstd.  Many thanks to their creators for the great work!!!  Please consider their licenses before commercial use.  See contents of `alib3/` for more info.


## Build

[Build Instructions](BUILDING.md)


## License

phiola is licensed under BSD-2.
But consider licenses of the third party libraries before commercial use.
