This branch contains modifications to the mozilla:release branch for compiling
the Record Replay gecko based browser.

### Getting started:

**Mac OSX**

1. `cp mozconfig.macsample mozconfig` 
2. untar `MacOSX10.11.sdk.tar.xz`
3. run `./mach bootstrap` and select (2) Firefox Desktop
4. run `./mach build`

**Other OS**

1. `cp mozconfig.sample mozconfig` 
2. run `./mach bootstrap` and select (2) Firefox Desktop
3. run `./mach build`

### Testing

Locally built browsers will try to download updates after opening.
Set the RECORD_REPLAY_NO_UPDATE environment variable when running to prevent this.

If your browser has updated itself and you want to regenerate the
install directory without having to rebuild everything, try:

```
rm -rf rr-opt/dist/Replay.app
```
