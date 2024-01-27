# Troubleshooting

* Windows: Recording fails with "Access is denied" error


## Windows: Recording fails with "Access is denied" error

If phiola shows this error when starting audio recording:

	open device #0: IAudioClient_Initialize: -2147024891 (0x80070005) Access is denied

It's probably because Windows denies access to your microphone device.  Here's how to fix it:

1. Open `Microphone Privacy Settings` window.
2. Enable `Allow apps to access your microphone` checkbox.
3. Enable `Allow desktop apps to access your microphone` checkbox.

More info and screenshots are here: https://github.com/stsaz/fmedia/issues/71#issuecomment-1115407629
