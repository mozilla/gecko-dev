This branch contains modifications to the mozilla:release branch for compiling
the Record Replay gecko based browser.

### Getting started:

**Mac OSX**

1. `cp mozconfig.macsample mozconfig`
2. untar `MacOSX10.11.sdk.tar.xz`
3. run `./mach bootstrap` and select (2) Firefox Desktop
4. run `./mach build`
5. run `RECORD_REPLAY_NO_UPDATE=1 ./mach run`

**Other OS**

1. `cp mozconfig.sample mozconfig`
2. run `./mach bootstrap` and select (2) Firefox Desktop
3. run `./mach build`
4. run `RECORD_REPLAY_NO_UPDATE=1 ./mach run`
