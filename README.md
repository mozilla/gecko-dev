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
7. run `./mach run`

**Other OS**

1. `cp mozconfig.sample mozconfig`
2. run `./mach bootstrap` and select (2) Firefox Desktop
3. run `./mach build`
4. run `./mach run`

### Troubleshooting Tips

* If you change your PATH to point to a different version of say Python or Rust you need to rerun `./mach bootstrap` to get the build system to pick up the change.

### Merging from upstream

1. Checkout the `release` branch, pull from upstream `release` branch:

```
git checkout release
git pull https://github.com/mozilla/gecko-dev.git release
```

2. Switch to a new branch, merge from the `release` branch.

```
git checkout -b replay-merge
git merge release
```

3. Fix merge conflicts.
4. Fix build breaks.
5. Make sure the output binary is `replay` and not `firefox`.
6. Get e2e tests etc. to pass.
7. At this point it is reasonably safe to merge into the `webreplay-release` branch.

```
git checkout webreplay-release
git merge replay-merge
git push
```

8. Make sure automatic updates work with the new browser.
9. Run live test harness and make sure crash rate is acceptable.
