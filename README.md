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
* [Install](#install)
* [How to Use](#how-to-use)
* [How to Use on Android](#how-to-use-on-android)
* [Libraries](#libraries)
* [Build](#build)


## Features

* Play audio: `.mp3`, `.ogg`(Vorbis/Opus), `.mp4`/`.mov`(AAC/ALAC/MPEG), `.mkv`/`.webm`(AAC/ALAC/MPEG/Vorbis/Opus/PCM), `.caf`(AAC/ALAC/PCM), `.avi`(AAC/MPEG/PCM), `.aac`, `.mpc`; `.flac`, `.ape`, `.wv`, `.wav`.  Note: on Android phiola can play only what your Android supports!
* Record audio: `.m4a`(AAC), `.ogg`, `.opus`; `.flac`, `.wav`
* Convert audio
* Input: file, directory, HTTP URL, console (stdin), playlists: `.m3u`, `.pls`, `.cue`
* List available audio devices
* Command Line Interface for Desktop OS
* Terminal/Console UI for interaction at runtime
* GUI for Windows, Linux, Android
* Instant startup time: very short initial delay until the audio starts playing (e.g. Linux/PulseAudio: TUI: `~25ms`, GUI: `~50ms`)
* Fast (low footprint): keeps your CPU, memory & disk I/O at absolute minimum; spends 99% of time inside codec algorithms

**Bonus:** Convenient API with plugin support which allows using all the above features from any C/C++/Java app!


## Install

* Download the latest package for your OS and CPU from [phiola beta Releases](https://github.com/stsaz/phiola/releases)

Linux:

* Unpack the archive somewhere, e.g. to `~/bin`:

	```sh
	mkdir -p ~/bin
	tar xf phiola-VERSION-linux-amd64.tar.zst -C ~/bin
	```

* Create a symbolic link:

	```sh
	ln -s ~/bin/phiola-2/phiola ~/bin/phiola
	```

Windows:

* Unpack the archive somewhere, e.g. to `C:\Program Files`
* Add `C:\Program Files\phiola-2` to your `PATH` environment
* Optionally, create a desktop shortcut to `phiola.exe gui`

Android:

* To be able to install .apk you need to enable "Allow installation from unknown sources" option in your phone's settings (you can disable it again after installation)
* Tap on .apk file to install it on your phone
* Or you can install .apk file from PC with `adb install`


## How to Use

> Important: enclose in quotes the file names containing spaces or special characters, e.g.: `phiola play "My Music"`; `phiola play "My Recordings" -include "*.wav"`.

Just a few quick examples:

```sh
# Play files, directories and URLs
phiola play file.mp3 *.flac "My Music" http://server/music.m3u
# or just
phiola file.mp3 *.flac "My Music" http://server/music.m3u

# Record from the default audio device until stopped
phiola record -o audio.flac

# Record for 1 minute, then stop automatically
phiola record -o audio.flac -until 1:0

# Record and set meta data
phiola record -meta "artist=Great Artist" -o audio.flac

# Record to the automatically named output file
phiola record -o @nowdate-@nowtime.flac

# Start recording, then stop recording from another process:
#   Step 1: start recording and listen for system-wide commands
phiola record -o audio.flac -remote
#   Step 2: send 'stop' signal to the phiola instance that is recording audio
phiola remote stop

# Convert
phiola convert audio.flac -o audio.m4a

# Convert multiple files from .wav to .flac
phiola convert *.wav -o .flac

# Convert all .wav files inside a directory,
#  preserving the original file names and file modification time
phiola convert "My Recordings" -include "*.wav" -o @filepath/@filename.flac -preserve-date

# List audio devices
phiola device list

# Show meta info on all .wav files inside a directory
phiola info "My Recordings" -include "*.wav"
```

Currently supported commands:

* [convert](src/exe/convert.h) - Convert audio
* [device](src/exe/device.h)   - List audio devices
* [gui](src/exe/gui.h)         - Start graphical interface
* [info](src/exe/info.h)       - Show file meta data
* [play](src/exe/play.h)       - Play audio
* [record](src/exe/record.h)   - Record audio
* [remote](src/exe/remote.h)   - Send remote command

> For the details on each command you can click on the links above or execute `phiola COMMAND -h` on your PC.


## How to Use on Android

First time start:

* Run phiola app
* Tap on `Explorer` tab
* Android will ask you to grant/deny permissions to phiola.  Allow to read your storage files.
* Tap on the music file you want to listen
* Or long-press on the directory with your music, it will be added to the playlist; tap `Play`
* Tap on `Playlist` tab to switch the view to your playlist


## Libraries

phiola uses modified versions of these third party libraries: libALAC, libfdk-aac, libFLAC, libMAC, libmpg123, libmpc, libogg, libopus, libvorbisenc, libvorbis, libwavpack, libsoxr, libzstd, libDynamicAudioNormalizer.  Many thanks to their creators for the great work!!!  Please consider their licenses before commercial use.  See contents of `alib3/` for more info.


## Build

[Build Instructions](BUILDING.md)


## License

phiola is licensed under BSD-2.
But consider licenses of the third party libraries before commercial use.
