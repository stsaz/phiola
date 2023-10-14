# phiola

![](res/phiola.svg)

phiola *beta* - fast audio player, recorder, converter for Windows, Linux & Android.
Its low CPU consumption conserves the notebook/phone battery.
You can issue commands to phiola via its CLI, TUI, GUI, system pipe and SDK interfaces.
Its fast startup time allows using it from custom scripts on a "play-and-exit" or "record-and-exit" basis.
It's completely portable (all codecs are bundled) - you can run it directly from a read-only flash drive.
It's a free and open-source project, and you can use it as a standalone application or as a library for your own software.

*beta* note: still need to port some stuff from fmedia and re-test everything.

Contents:

* [Features](#features)
* [Install](#install)
* [How to Use CLI](#how-to-use-cli)
* [How to Use GUI](#how-to-use-gui)
* [How to Use on Android](#how-to-use-on-android)
* [How to Use SDK](#how-to-use-sdk)
* [External Libraries](#external-libraries)
* [Build](#build)


## Features

* Play audio: `.mp3`, `.ogg`(Vorbis/Opus), `.mp4`/`.mov`(AAC/ALAC/MPEG), `.mkv`/`.webm`(AAC/ALAC/MPEG/Vorbis/Opus/PCM), `.caf`(AAC/ALAC/PCM), `.avi`(AAC/MPEG/PCM), `.aac`, `.mpc`; `.flac`, `.ape`, `.wv`, `.wav`.  Note: on Android phiola can play only what your Android supports!
* Record audio: `.m4a`(AAC), `.ogg`, `.opus`; `.flac`, `.wav`
* Convert audio
* Input: file, directory, HTTP/HTTPS URL, console (stdin), playlists: `.m3u`, `.pls`, `.cue`
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


## How to Use CLI

> Important: enclose in quotes the file names containing spaces or special characters, e.g.: `phiola play "My Music"`; `phiola play "My Recordings" -include "*.wav"`.

Play:

```sh
# Play files, directories and URLs
phiola play file.mp3 *.flac "My Music" http://server/music.m3u
# or just
phiola file.mp3 *.flac "My Music" http://server/music.m3u

# Play all files within directory in random order and auto-skip the first 20 seconds from each track
phiola "My Music" -random -seek 0:20
```

> While audio is playing, you can control phiola via keyboard, e.g. press `Right Arrow` to seek forward; press `Space` to pause/unpause the current track; `n` to start playing the next track; `q` to exit; `h` to see all supported TUI commands.

Record:

```sh
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
```

Convert:

```sh
# Convert
phiola convert audio.flac -o audio.m4a

# Convert multiple files from .wav to .flac
phiola convert *.wav -o .flac

# Convert all .wav files inside a directory,
#  preserving the original file names and file modification time
phiola convert "My Recordings" -include "*.wav" -o @filepath/@filename.flac -preserve_date
```

Other use-cases:

```sh
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


## How to Use GUI

Start phiola GUI:

* On Windows: via `phiola-gui.exe`

* On Linux:

	```sh
	phiola gui
	```

Then add some files to playlist via drag-n-drop from your File Manager, or via `List->Add` menu command.

**Bonus:** you can modify the appearance by editing the GUI source file: `phiola-2/mod/gui/ui.conf`.  You can also modify the text by editing language files, e.g. `phiola-2/mod/gui/lang_en.conf`.  Restart phiola GUI after you make changes to those files.


## How to Use on Android

First time start:

* Run phiola app
* Tap on `Explorer` tab
* Android will ask you to grant/deny permissions to phiola.  Allow to read your storage files.
* Tap on the music file you want to listen
* Or long-press on the directory with your music, it will be added to the playlist; tap `Play`
* Tap on `Playlist` tab to switch the view to your playlist


## How to Use SDK

The best example how to use phiola software interface is to see the source code of phiola executor in `src/exe`, e.g. `src/exe/play.h` contains the code that adds input files into a playlist and starts the playback.

* `src/phiola.h` describes all interfaces implemented either by phiola Core or dynamic modules
* `android/phiola/src/main/java/com/github/stsaz/phiola/Phiola.java` is a Java interface
* `src/track.h` is needed if you want to write your own module


## External Libraries

phiola uses modified versions of these third party libraries: libALAC, libfdk-aac, libFLAC, libMAC, libmpg123, libmpc, libogg, libopus, libvorbisenc, libvorbis, libwavpack, libsoxr, libzstd, libDynamicAudioNormalizer.  And unmodified libraries: libssl & libcrypto.  Many thanks to their creators for the great work!!!  Please consider their licenses before commercial use.  See contents of `alib3/` for more info.

Additionally:

* [ffbase](https://github.com/stsaz/ffbase) library and [ffos](https://github.com/stsaz/ffos) interface make it easy to write high level cross-platform code in C language
* [avpack](https://github.com/stsaz/avpack) library provides multimedia file read/write capabilities
* [ffaudio](https://github.com/stsaz/ffaudio) interface provides cross-platform audio I/O capabilities
* [netmill](https://github.com/stsaz/netmill) provides network capabilities


## Build

[Build Instructions](BUILDING.md)


## License

phiola is licensed under BSD-2.
But consider licenses of the third party libraries before commercial use.
