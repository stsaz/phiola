# phiola

![](res/phiola.svg)

phiola - fast audio player, recorder, converter and streaming server for Windows, Linux & Android.
It can play audio files from your device or remote server, record audio from your microphone or Internet radio stream, process and convert audio into another format, and more.
Its low CPU consumption conserves the notebook/phone battery.
You can issue commands to phiola via its CLI, TUI, GUI, system pipe and SDK interfaces.
Its fast startup time allows using it from custom scripts on a "play-and-exit" or "record-and-exit" basis.
It's completely portable (all codecs are bundled) - you can run it directly from a read-only flash drive.
It's a free and open-source project, and you can use it as a standalone application or as a library for your own software.

Screenshots of phiola GUI on Android, KDE/Linux and Windows 10:
![phiola GUI on Android, KDE/Linux and Windows 10](../screenshots/phiola-gui-screenshot-android-kdelinux-windows.png)

Contents:

* [Features](#features)
* [Install](#install)
* [How to Use CLI](#how-to-use-cli)
* [How to Use GUI](#how-to-use-gui)
* [How to Use on Android](#how-to-use-on-android)
* [How to Use SDK](#how-to-use-sdk)
* [Configuration](#configuration)
* [External Libraries](#external-libraries)
* [Build](#build)
* [Bug report](#bug-report)
* [Why use phiola](#why-use-phiola)


## Features

* Play audio: `.mp3`, `.ogg`(Vorbis/Opus), `.mp4`/`.mov`(AAC/ALAC/MP3), `.mkv`/`.webm`(AAC/ALAC/MP3/Vorbis/Opus/PCM), `.caf`(AAC/ALAC/PCM), `.avi`(AAC/MP3/PCM), `.ts`(AAC/MP3), `.aac`, `.mpc`; `.flac`, `.ape`, `.wv`, `.wav`.
* Record audio: `.m4a`(AAC), `.aac`(AAC-LC), `.ogg`(Vorbis), `.opus`; `.flac`, `.wav`
* Convert audio
* Broadcast audio over HTTP
* List/search file meta tags; edit file tags, write ReplayGain tags (`.mp3`, `.ogg/.opus`, `.flac`)
* List available audio devices
* Input: file, directory, ICY/HLS/HTTP/HTTPS URL, console (stdin), playlists: `.m3u`, `.pls`, `.cue`
* Command Line Interface for Desktop OS
* Terminal/Console UI for interaction at runtime
* GUI for Windows, Linux, Android
* Instant startup time: very short initial delay until the audio starts playing (e.g. Linux/PulseAudio: TUI: `~25ms`, GUI: `~50ms`)
* Fast (small footprint, low overhead): keeps your CPU, memory & disk I/O at absolute minimum; spends 99% of time inside codec algorithms

**Bonus:** Convenient API with plugin support which allows using all the above features from any C/C++/Java app!

Features and notes by platform:

| Feature              | Linux | Windows | Android |
| --- | --- | --- | --- |
| Dark themed GUI      | ✅ (GTK default) | incomplete | ✅ |
| File formats         | ✅ all supported | ✅ all supported | all supported except `.mpc`, `.ape`, `.wv` |
| Record from Internet | ✅ | ✅ | ❌ |
| Record what you hear | ✅ (PulseAudio) | ✅ | ❌ |
| Audio Streaming Server | ✅ (AAC,Opus) | ✅ (AAC,Opus) | ❌ |
| Requirements         | glibc-2.36 | Windows 7 | Android 8 (ARM64), Android 6 (ARM) |
| HW Requirements      | AMD64, ARM64 | AMD64 | ARM64, ARM(incomplete) |

> Although not officially supported, phiola should build fine for **macOS**, **FreeBSD** and **Windows XP** after tweaking the build script.

> Important: ALAC decoder is disabled by default in phiola/Android because of potential security issues (see https://github.com/macosforge/alac).


## Install

Download the latest package for your OS and CPU from [phiola Releases](https://github.com/stsaz/phiola/releases)

### Linux

For example, here's how you can install the latest release for AMD64 into `~/bin` directory:

```sh
wget https://github.com/stsaz/phiola/releases/download/v2.3.12/phiola-2.3.12-linux-amd64.tar.zst
mkdir -p ~/bin
tar xf phiola-2.3.12-linux-amd64.tar.zst -C ~/bin
ln -s ~/bin/phiola-2/phiola ~/bin/phiola
cp ~/bin/phiola-2/mod/gui/phiola.desktop ~/.local/share/applications
```

If you choose another directory rather than `~/bin`, then you should also edit `Icon=` value in `~/.local/share/applications/phiola.desktop`.

### Windows

The easiest way to install phiola on Windows is to execute `phiola-...-windows-x64-setup.exe` and specify the installation directory.

Another option is to manually unpack `phiola-...-windows-x64.zip` and add phiola directory to your `PATH` environment.

### Android

Install from IzzyOnDroid:

<p><a href="https://apt.izzysoft.de/packages/com.github.stsaz.phiola"><img src="https://github.com/stsaz/phiola/raw/resources/IzzyOnDroid.png" height="117px"></a></p>

Or install .apk manually:

* To be able to install .apk you need to enable "Allow installation from unknown sources" option in your phone's settings (you can disable it again after installation)
* Tap on .apk file to install it on your phone

Or you can install .apk file from the PC connected to your phone with `adb install`.


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

# Play on Linux directly via ALSA (and not PulseAudio)
phiola file.mp3 -audio alsa

# Play Internet radio and save the tracks as local files.
# These files will be named automatically using the meta data sent by server.
phiola http://server/stream -tee "@artist - @title.mp3"

# Play MP3 audio via HTTP and convert to a local 64-kbit/sec AAC file
phiola http://server/music.mp3 -dup @stdout.wav | phiola convert @stdin -aac_q 64 -o output.m4a
```

While audio is playing, you can control phiola via keyboard.  The most commonly used commands are:

| Key | Action |
| --- | --- |
| `Space`       | Play/Pause |
| `Right Arrow` | Seek forward |
| `n`           | Play next track |
| `p`           | Play previous track |
| `q`           | Quit |
| `h`           | Show all supported commands |


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

# Record with a specific audio format
phiola record -aformat int16 -rate 48000 -channels 2 -o audio.flac

# Record what is currently playing on your system ("what you hear")
# * WASAPI:
phiola record -loopback -o audio.flac
# * PulseAudio:
phiola record -dev $(phiola dev list -f Monitor -n) -o audio.flac

# Start recording in background, then stop recording from another process:
#   Step 1: record while listening for system-wide commands
phiola -Background record -o audio.flac -remote
#   Step 2: send 'stop' signal to the phiola instance that is recording audio
phiola remote stop

# Record and split the output files by 1 hour
phiola record -split 1:00:00 -o @nowdate-@nowtime.flac

# Record and pass the audio through Dynamic Audio Normalizer filter
phiola record -danorm 'frame 500 size 15' -o audio.flac

# Record and pass the audio to another program
phiola record -o @stdout.wav | your_program
```

> Note: the output audio format is chosen automatically by the file extension you specify.

> Note: it's not required to always type the whole name of a command or an option - you may type just its prefix (enough for phiola to recognize it), e.g. instead of `phiola record -aformat int16` you may type `phiola rec -af int16`.

Convert:

```sh
# Convert
phiola convert audio.flac -o audio.m4a

# Convert with parameters
phiola convert file.mp3 -aformat int16 -o file.wav
phiola convert file.wav -vorbis_q 7 -o file.ogg
phiola convert file.wav -rate 48000 -aac_q 5 -o file.m4a

# Convert multiple files from .wav to .flac
phiola convert *.wav -o .flac

# Convert all .wav files inside a directory,
#  preserving the original file names and file modification time
phiola convert "My Recordings" -include "*.wav" -o @filepath/@filename.flac -preserve_date

# Copy (without re-encoding) MP3 audio region from 1:00 to 2:00 to another file
phiola convert -copy -seek 1:0 -until 2:0 input.mp3 -o output.mp3

# Extract (100% accurately) several tracks from a .cue file
phiola convert input.cue -tracks 3,5-7,13 -o "@tracknumber. @artist - @title.flac"

# Increase .wav file's volume by 6 dB
phiola convert file.wav -gain 6 -o file-louder.wav
```

> Note: the output audio format is chosen automatically by the file extension you specify.

Show file info:

```sh
# Show meta info on all .wav files inside a directory
phiola info "My Recordings" -include "*.wav"

# Show meta info including all tags
phiola info -tags file.mp3

# Search for the .mp3 files containing a specific tag
phiola info -tags -inc "*.mp3" . | grep -B10 "Purple Haze"

# Analyze and show PCM info
phiola info -peaks file.mp3

# Analyze audio loudness
phiola info -loudness file.mp3
```

Edit file tags:

```sh
# Replace/add MP3 tags in-place
# WARNING: please test first before actually using on real files (or at least make backups)!
phiola tag -m "artist=Great Artist" -m "title=Cool Song" file.mp3

# Remove all existing tags; add new tags
phiola tag -clear -m "artist=Great Artist" -m "title=Cool Song" file.mp3

# Write ReplayGain tag for all .mp3 & .flac files in the directory
phiola tag -rg track_gain "My Music" -include "*.mp3" -include "*.flac"
```

Create/edit playlist files:

```sh
# Create a playlist from all .mp3 files in the directory
phiola list create "My Music" -include "*.mp3" -o my-music.m3u

# Sort entries (by file name) in all playlists in the current directory
phiola list sort *.m3u

# Automatically correct the file paths inside playlist (e.g. after some files were moved)
phiola list heal Music/playlist.m3u
```

Other use-cases:

```sh
# List all available audio playback and recording devices
phiola device list

# Auto-rename all .opus files according to a pattern
phiola rename *.opus -o "@tracknumber. @artist - @title"

# Start HTTP audio streaming server (Opus, 128kbps)
phiola server "My Music" -inc "*.flac" -shuffle -opus_q 128
```

Currently supported commands:

| Command | Description |
| --- | --- |
| [convert](src/exe/convert.h) | Convert audio |
| [device](src/exe/device.h)   | List audio devices |
| [gui](src/exe/gui.h)         | Start graphical interface |
| [info](src/exe/info.h)       | Analyze audio files |
| [list](src/exe/list.h)       | Process playlist files |
| [play](src/exe/play.h)       | Play audio |
| [record](src/exe/record.h)   | Record audio |
| [remote](src/exe/remote.h)   | Send remote command |
| [rename](src/exe/rename.h)   | Auto-rename files |
| [server](src/exe/server.h)   | Start audio streaming server |
| [tag](src/exe/tag.h)         | Edit file tags |

> For the details on each command you can click on the links above or execute `phiola COMMAND -h` on your PC.

See also: [phiola Wiki](https://github.com/stsaz/phiola/wiki)


## How to Use GUI

Start phiola GUI:

* On Windows: via `phiola-gui.exe`

* On Linux:

	```sh
	phiola gui
	```

Then add some files to playlist via drag-n-drop from your File Manager, or via `List->Add` menu command.

**Bonus:** you can modify the appearance by editing the GUI source file: `phiola-2/mod/gui/ui.conf`.  You can also modify the text by editing language files, e.g. `phiola-2/mod/gui/lang_en.conf`.  Restart phiola GUI after you make changes to those files.

### 100% Portable Mode

By default, phiola GUI saves and restores its state (including your playlists) on each restart at these locations:

* Windows: `%APPDATA%\phiola`
* Linux: `$HOME/.config/phiola`

But in case you don't want this, then just create an empty `[phiola directory]/mod/gui/user.conf` file.  As long as this file is present, phiola GUI will store its state there, and it won't touch anything inside your user directory.


## How to Use on Android

First time start:

* Run phiola app
* Tap on `Explorer` tab
* Android will ask you to grant/deny permissions to phiola.  Allow to read your storage files.
* Tap on the music file you want to listen
* Or long-press on the directory with your music, it will be added to the playlist; tap `Play`
* Tap on `Playlist` tab to switch the view to your playlist


## How to Use SDK

The best example how to use phiola software interface is to see the source code of phiola executor in `src/exe`, e.g. [src/exe/play.h](src/exe/play.h) contains the code that adds input files into a playlist and starts the playback.

* [src/phiola.h](src/phiola.h) describes all interfaces implemented either by phiola Core or dynamic modules
* [android/.../Phiola.java](android/phiola/src/main/java/com/github/stsaz/phiola/Phiola.java) is a Java interface
* [src/track.h](src/track.h) contains internal declarations for a phiola track, and you'll need it only if you want to write your own filter

A short description of how phiola works:

* User starts phiola app - the top-level module, which I call *Executor*, starts running.
* Executor loads *Core* module that provides access to all phiola's functions.
* Executor waits (if in graphical mode) and analyzes the user's command and decides what to do next.
* Most commonly (i.e. for audio playback), Executor prepares the queue of tracks and starts the playback.
* A *Track* represents a single job (a single file) and consists of several Filters linked together to form a conveyor.  While a track is running, Core consistently calls its Filters, going forth and back through the conveyor until all Filters complete their job.  Several Tracks can run in parallel.
* A *Filter* is a piece of code located inside a Module, and it performs a single task on a particular Track.
* A *Module* is a file (`.so/.dll`), it may contain one or several Filters, and it provides direct access for them to Executor, Core or other Modules.  All necessary Modules are loaded on-demand while the Tracks are running.
* Before being started, each Track is assigned to a Worker.  A *Worker* is a system thread (a logical CPU core) that actually executes the Filters code.
* When a Track is being started, stopped or updated, Executor's own Filters get called, so it can report the current status back to the user.


## Configuration

Global settings can be specified in `phiola-2/phiola.conf` file.
See the contents of `phiola-2/phiola-example.conf` for more info.


## External Libraries

phiola uses modified versions of these third party libraries:
[libALAC](https://github.com/macosforge/alac),
[libfdk-aac](https://github.com/mstorsjo/fdk-aac),
[libFLAC](https://github.com/xiph/flac),
[libebur128](https://github.com/jiixyj/libebur128),
libMAC,
[libmpg123](https://mpg123.de),
libmpc,
[libopus](https://github.com/xiph/opus),
[libvorbis](https://github.com/xiph/vorbis),
libwavpack,
[libsoxr](https://github.com/dofuuz/soxr),
[libzstd](https://github.com/facebook/zstd),
[libDynamicAudioNormalizer](https://github.com/lordmulder/DynamicAudioNormalizer).
And these unmodified libraries:
[openssl](https://www.openssl.org).
Many thanks to their creators for the great work!!!
Please consider their licenses before commercial use.
See contents of `alib3/` for more info.

Additionally:

* [ffbase](https://github.com/stsaz/ffbase) library and [ffsys](https://github.com/stsaz/ffsys) interface make it easy to write high level cross-platform code in C language
* [avpack](https://github.com/stsaz/avpack) library provides multimedia file read/write capabilities
* [ffaudio](https://github.com/stsaz/ffaudio) interface provides cross-platform audio I/O capabilities
* [ffgui](https://github.com/stsaz/ffgui) - cross-platform GUI
* [netmill](https://github.com/stsaz/netmill) provides network capabilities


## Build

[Build Instructions](BUILDING.md)


## Bug report

If you encounter a bug, please report it via GitHub Issues.
When filing a bug report try to provide enough information that can help the developers to understand and fix the problem.

When using CLI, additional debug messages may help sometimes - just add `-D` after `phiola` when executing a command, e.g.:

```sh
phiola -D play file.mp3
```

With phiola on Android, there's a button in "About" screen that will save system logs to a file.
It will contain the necessary information about the last time phiola crashed, for example.


## Why use phiola

phiola is not (and most likely will never be) a competitor to large commercial projects such as Winamp, but there are a few points where phiola is better:

* phiola and all its dependencies are 100% open-source.  This means that you don't run some private and potentially insecure code on your electronic hardware, especially when a music player is running on your computer/phone for 8-14 hours a day.

* phiola is very flexible, it can easily accommodate hundreds of different functions, it can even be included as a module into another project.  If someone decides to make a coffee machine that should also be able to play music, I'm sure phiola would be the best choice for this job :)

* phiola uses the minimum possible resources, but at the same time it's powerful and user-friendly.  Please check your CPU, memory and storage device usage and compare with popular audio software to see whether it's true.  I hope phiola can play its small role in preserving our environment by decreasing power consumption.


## License

phiola is licensed under BSD-2.
But consider licenses of the third party libraries before commercial use.
Playback control icons by [Icons8](https://icons8.com).
