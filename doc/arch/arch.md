# phiola Architecture

![](phiola-arch.svg)


## Android

![](phiola-arch-android.svg)


## Track's Audio Conversion

Example of a track chain for audio conversion:

* User wants mono audio by specifying `channels=1`
* Input filter produces `float32/stereo`
* Output filter requires `int16` input

![](chain-aconv.svg)

1. `aconv` prepares output audio format by taking input format and applying user settings (`conf.oaudio.channels=1`).
2. Output filter (`aenc` or `apla`) requests audio conversion (`oaudio.conv_format.format=int16`).
3. `aconv` applies the conversion format specified by output filter.  The resulting conversion will be `float32/stereo => int16/mono`.  Adds `conv` filter to the chain.
4. `conv` performs the conversion and supplies data to output filter.
