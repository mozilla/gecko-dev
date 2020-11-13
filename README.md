This branch contains modifications to the mozilla:release branch for compiling
the Record Replay gecko based browser.

### Getting started:

**macOS**

1. Make sure that you are using Python v2.7
2. Make sure that are using Rust 1.46 (can be configured via `rustup default 1.46.0-x86_64-apple-darwin`)
3. `cp mozconfig.macsample mozconfig`
4. untar `MacOSX10.11.sdk.tar.xz`
5. run `./mach bootstrap` and select (2) Firefox Desktop
6. run `./mach build`
7. run `RECORD_REPLAY_NO_UPDATE=1 ./mach run`

**Other OS**

1. `cp mozconfig.sample mozconfig`
2. run `./mach bootstrap` and select (2) Firefox Desktop
3. run `./mach build`
4. run `RECORD_REPLAY_NO_UPDATE=1 ./mach run`

### Troubleshooting Tips
* If you change your PATH to point to a different version of say Python or Rust you need to rerun `./mach bootstrap` to get the build system to pick up the change.
