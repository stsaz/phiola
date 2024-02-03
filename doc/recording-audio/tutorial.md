# How To Record Sound On Windows And Linux via Command-Line

In this tutorial I'm going to show you how to record audio via command-line using phiola.
The tutorial covers different aspects such as using non-default sound capture devices, configuring audio format and streaming the recorded audio to another application.

Contents:

* [Install](#install)
* [Quick Start](#quick-start)
* [Choosing Output Format](#choosing-output-format)
* [Choosing Audio Input Device](#choosing-audio-input-device)
* [Configuring Audio Format](#configuring-audio-format)
* [Recording To Standard Output](#recording-to-standard-output)
* [Record From Playback ("What You Hear")](#record-from-playback-what-you-hear)
* [Recording in Background](#recording-in-background)


## Install

If you haven't yet installed phiola, please follow the [phiola audio recorder installation instructions](https://github.com/stsaz/phiola#install).


## Quick Start

Before we start using more advanced features, I want to show you the easiest way how to record sound from your default audio input device to a file.
Just execute the following command:

```sh
phiola rec -o audio.wav
```

The recording process starts, and the output file `audio.wav` is being filled with audio data.

The currently displayed dB values show how loud your current signal is.
If it reaches `0dB`, your signal is too loud and it will be clipped, resulting in quality loss.
Usually, the best recording quality is achieved when the sound level is almost `0dB` but not reaches it.

To stop recording, press `s` key (short for "Stop") on your keyboard.
All cached data is flushed to a file and phiola exits.

phiola can automatically stop recording after the given time is passed: use `-until [MIN:]SEC` parameter.
For example, here's how to record for 1 minute and then stop automatically:

```sh
phiola record -out rec.wav -until 1:0
```

`-until` option is quite convenient: you still can press "s" key to stop the recording before 1 minute passes.


## Choosing Output Format

If you want to use another file format, just specify its file extension as the output file.
For example, to use FLAC:

```sh
phiola record -o rec.flac
```

You can use any output file format supported by phiola: `flac`, `wav`, `ogg`, `opus`, `m4a`.

I recommend you to use FLAC for lossless audio recording.
FLAC encoder is very fast (CPU usage is low) and has a good compression ratio.

But if you need the fastest possible recording and you don't need to conserve disk space, use WAV format.
It supports all audio formats and requires zero encoding time.

When you want to save disk space (e.g. for very-very long recordings) and don't need to preserve the original audio quality, use OGG Vorbis for lossy audio recording.
The default setting `5` for Vorbis encoding quality gives a good starting point.
And the value of `7` will produce very good quality:

```sh
phiola record -vorbis_q 7 -o rec.ogg
```


## Choosing Audio Input Device

By default phiola uses the audio device set in your OS as default.
But what if you want to use another device?
Follow these steps:

* Get the list of sound devices
* Choose the needed device and pass it to phiola

### Step 1

This command shows the list of all available capture sound devices in your system:

```sh
phiola device list -cap
```

### Step 2

Now choose the device you want to use and pass its number with `-device NUMBER` to phiola.
For example, if I want to record from my USB soundcard that is displayed as number 4 in my list, I run phiola like this:

```sh
phiola record -device 4 -o rec.wav
```


## Configuring Audio Format

The default audio format used by phiola for sound recording is audio-CD quality (16-bit, 44.1kHz, stereo).
But it isn't always the best choice, and you may wish to set different audio settings: bit depth, sample rate, channels.
Consider this example:

```sh
phiola record -aformat int24 -rate 48000 -channels 1 -out rec.wav
```

phiola will try to open the audio device using the specified format (24-bit, 48kHz sample rate, 1 channel).
In case the audio device doesn't support this format natively, phiola will choose the best format supported by the device and then perform audio conversion so that the output file will be of the format you specified.
For the best recording quality please verify that you use correct audio format.

Just a small tip for those who don't like typing too many keys - you can shorten the commands and options like this:

```sh
phiola rec -af int24 -ra 48000 -ch 1 -o rec.wav
```

This is the exactly same command as before, only shorter.


## Recording To Standard Output

phiola can also stream the audio being recorded to stdout.
It may be useful if you want to immediately pass phiola's output to another program.
Here's an example:

```sh
phiola record -o @stdout.wav >rec.wav
```

Here phiola writes recorded data into standard output descriptor, and then it's being redirected to `rec.wav` file.
When phiola sees `@stdout` value, it writes the output data to stdout instead of a file.
And file extension (`.wav` in our case) must still be specified so phiola knows what output file format you want it to use.
Currently, only `.wav`, `.flac`, `.ogg`, `.opus` support streaming to stdout.

Keep in mind that since stdout is a non-seekable descriptor, the recorded output files may have incomplete headers.

Of course, you can also pass the recorded data to another application:

```sh
phiola record -o @stdout.wav | your_audio_app
```


## Record From Playback ("What You Hear")

You can capture the audio that is currently being played via playback device by other applications on your system.
On Windows it's called WASAPI loopback mode.
On Linux this is achieved via PulseAudio's "Monitor" capture device.
You can easily capture the sound you hear from your speakers with phiola on Windows:

```sh
phiola record -loopback -o rec.wav
```

The command is slightly more complex on Linux:

```sh
phiola record -device `phiola dev list -f Monitor -n` -o rec.wav
```

Here, the recording device number is selected automatically by `dev list` subcommand that returns the index of the device that contains the word "Monitor" in its name.


## Recording in Background

While recording, phiola waits for user input so he can stop the process at any time he wants.
The terminal needs to be attached to phiola in order to pass the user's "s" key-press.
On Windows this means that a command-line window must stay opened while phiola is running.
On bash/Linux you can easily start a process in background, but even so, you won't be able to stop the recording unless the terminal is attached.
phiola addresses these issues in a cross-platform and user-friendly way.

### Step 1

Run phiola instance in background (detached from the current terminal) by passing `-Background` option:

```sh
phiola -Background  record -o rec.wav -remote
```

Note capital `B` in `-Background` - this option takes effect before recording is even started; it must appear before `record` command and sub-options.

This command creates a new phiola process in background that, in turn, starts the audio recording.
By using `-remote` option we order the new phiola instance to listen for incoming remote commands, because we want to control (e.g. stop) this background process later.

I recommend using `-until` option when starting recording in background to prevent the cases when you forget about a background phiola instance up to the point when it consumes all your storage space unless explicitly stopped by your command.

### Step 2

When you want to stop recording, you send a "stop" command to the phiola instance running in background:

```sh
phiola remote stop
```

This command simulates an "s" key-press action.
The background phiola instance will receive this command and finalize the file being recorded (flushing all cached data and finalizing output file header).
