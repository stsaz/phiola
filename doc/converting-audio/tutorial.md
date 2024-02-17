# How to Convert Audio Files on Windows and Linux via Command-Line

In this article I'm going to show you how to convert audio files using phiola audio convertor.
First, you'll learn how to easily convert a single file to a new format.
Then we'll discuss more complex cases, such as batch conversion and extracting the tracks from CUE file.

Contents:

* [Install](#install)
* [Quick Start](#quick-start)
* [Encoder Quality Settings](#encoder-quality-settings)
* [Copy Audio Frames without Re-Encoding](#copy-audio-frames-without-re-encoding)
* [Batch Convert](#batch-convert)
* [Extract Audio Tracks From Cue Sheet](#extract-audio-tracks-from-cue-sheet)
	* [FLAC and PCM](#flac-and-pcm)
	* [AAC, MP3, Vorbis, Opus](#aac,-mp3,-vorbis,-opus)
	* [Extract from multiple .cue files](#extract-from-multiple-.cue-files)
* [Audio Data Processing](#audio-data-processing)


## Install

If you haven't yet installed phiola, please follow the [phiola audio convertor installation instructions](https://github.com/stsaz/phiola#install).


## Quick Start

Converting audio files with phiola is very easy, just take a look at this example:

```sh
phiola convert file.wav -o file.flac
```

This command will convert `file.wav` into FLAC format.
phiola determines output audio format by the file extension you specify:

| Audio Format | File Extension |
| --- | --- |
| PCM    | `.wav` |
| FLAC   | `.flac` |
| AAC    | `.m4a` |
| Opus   | `.opus` |
| Vorbis | `.ogg` |


## Encoder Quality Settings

Lossy encoders allow you to set the encoding quality or audio bitrate.
For example, to encode to AAC-LC at 256kbps bitrate:

```sh
phiola convert file.wav -aac_quality 256 -o file.m4a
```

Here's the table with more options:

| Target Format   | phiola Options |
| --- | --- |
| AAC-LC @256kbps | `-aac_quality 256` |
| AAC-LC q=VBR:5  | `-aac_quality 5` |
| Vorbis q=7      | `-vorbis_quality 7` |
| Opus @256kbps   | `-opus_quality 256` |


## Copy Audio Frames without Re-Encoding

You may copy AAC, MP3, Vorbis and Opus audio data as-is, without re-encoding.
For example:

```sh
phiola convert file.mp3 -seek 1:00 -until 2:00 -copy -o file-cut.mp3
```

This command will copy MP3 audio data from `1:00` to `2:00` into a new file.
The new file will contain the exact same data (and therefore, quality) as the source.


## Batch Convert

phiola can convert many many files with a single command:

```sh
phiola convert *.wav -o .ogg
```

phiola will read all `.wav` files inside the current directory (excluding sub-directories) and convert each of them to OGG Vorbis.
Note that we don't specify the file name component in `-o .ogg` but just the output file extension.
phiola will automatically name the output files using the name of each input file.
For example, if you have a file `song1.wav`, it will be converted to `song1.ogg`.

Here's another example:

```sh
phiola convert Music/*.wav -o Encoded/.ogg
```

Now phiola will convert the `.wav` files inside `Music/` directory and place the output files into `Encoded/` directory.`

If you want to convert the whole directory tree with a single command, then use this pattern:

```sh
phiola convert Music/ -include "*.wav" -o @filepath/.ogg
```

Here phiola will recursively search for `.wav` files inside `Music/` directory (including sub-directories) and convert them.
`@filepath` variable will be substituted with the original file path, so that `Music/Artist/Song.wav` will be converted to `Music/Artist/Song.ogg`.


## Extract Audio Tracks From Cue Sheet

If you need to copy one or several tracks from `.cue` sheet file, use this command:

```sh
phiola convert album.cue -tracks 1,3,5 -o "@tracknumber. @artist - @title.flac"
```

Here phiola will parse input `.cue` file, select tracks #1, #3 and #5 from it and convert them to FLAC.
When using the variables like `@tracknumber` `@artist` and `@title` we instruct phiola to automatically substitute them with the meta data corresponding to each track in CUE file.
In case your CUE file doesn't contain per-track meta data, you can use simply `@tracknumber.flac`.

### FLAC and PCM

phiola is 100% accurate extractor when the source is lossless (e.g. FLAC), because FLAC->FLAC conversion won't result in any audio quality loss.
Also, phiola will produce files with the exact same length as the original source, sample to sample.
All meta information from the CUE file will be preserved and copied into new files.

### AAC, MP3, Vorbis, Opus

When extracting from lossy formats like `.mp3` we need to use `-copy` switch so that phiola will just copy the compressed audio frames rather than re-encoding the audio (which would result in some audio quality loss).

```sh
phiola convert album.cue -tracks 1,3,5 -copy -o "@tracknumber. @artist - @title.mp3"
```

### Extract from multiple .cue files

It's possible to process multiple CUE sheets with just a single command:

```sh
phiola convert album1.cue album2.cue album3.cue -o "@artist/@date - @album/@tracknumber. @title.flac'
```

This command will parse `.cue` files one by one and extract all tracks from them into current directory.
Output files will be automatically named from meta data, e.g. `Jimi Hendrix/1967 - Are You Experienced/01. Purple Haze.flac`.


## Audio Data Processing

The command below is an example how to order phiola to convert audio samples:

```sh
phiola file-int24-48000.flac -aformat int16 -rate 44100 -o file-int16-44100.flac
```

The input FLAC file (24bit/48kHz) will be converted to FLAC file (16bit/44.1kHz).

And in case you want to make your file louder:

```sh
phiola file.flac -gain 6 -o file-louder.flac
```

`-gain 6` parameter will increase the audio volume by +6.0dB.
