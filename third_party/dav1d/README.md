# dav1d

**dav1d** is a new **AV1** cross-platform **d**ecoder, open-source, and focused on speed and correctness.

The canonical repository URL for this repo is https://code.videolan.org/videolan/dav1d

This project is partially funded by the *Alliance for Open Media*/**AOM**.

## Goal and Features

The goal of this project is to provide a decoder for **most platforms**, and achieve the **highest speed** possible to overcome the lack of AV1 hardware decoder.

It aims to support all features from AV1, including all subsampling and bit-depth parameters.

In the future, this project will host simple tools or simple wrappings *(like, for example, an MFT transform)*.

## License

**dav1d** is released under a very liberal license, a contrario from the other VideoLAN projects, so that it can be embedded anywhere, including non-open-source software; or even drivers, for hybrid decoders.

The reasoning behind this decision is the same as for libvorbis, [RMS on vorbis](https://lwn.net/2001/0301/a/rms-ov-license.php3).

# Roadmap

The plan is the folllowing:

1. Complete C implementation of the decoder,
2. Provide a usable API,
3. Port to most platforms,
4. Make it fast, by writing asm.

By the end of 2018, we hope to have a completely usable version that will be faster than any other software open source implementation.

# Contribute

Currently, we are looking for help from:
- C developers,
- asm developers,
- platform-specific developers,
- testers.

Our contributions guidelines are strict. We want to build a coherent codebase to simplify maintenance and achieve the highest possible speed.

Notably, the codebase is in pure C and asm.

We are on IRC, on the **#dav1d** channel on *Freenode*.

See the [contributions document](CONTRIBUTING.md).

## CLA

There is no CLA.

People will keep their copyright and their authorship rights.

VideoLAN will only have the collective work rights.

## CoC

The [VideoLAN Code of Conduct](https://wiki.videolan.org/CoC) applies to this project.

# Compile

1. Install [Meson](https://mesonbuild.com/) (0.47 or higher), [Ninja](https://ninja-build.org/), and, for x86* targets, [nasm](https://nasm.us/) (2.13 or higher)
2. Run `meson build --buildtype release`
3. Build with `ninja -C build`

# Run tests

1. During initial build dir setup or `meson configure` specify `-Dbuild_tests=true`
2. In the build directory run `meson test` optionally with `-v` for more verbose output, especially useful
   for checkasm

# Run testdata based tests

1. Checkout the test data repository
   
   ```
   git clone https://code.videolan.org/videolan/dav1d-test-data.git tests/dav1d-test-data
   ```
2. During initial build dir setup or `meson configure` specify `-Dbuild_tests=true` and `-Dtestdata_tests=true`
3. In the build directory run `meson test` optionally with `-v` for more verbose output

# Support

This project is partially funded by the *Alliance for Open Media*/**AOM** and is supported by TwoOrioles and VideoLabs.

These companies can provide support and integration help, should you need it.


# FAQ

## Why do you not improve libaom rather than starting a new project?

- We believe that libaom is a very good library. It was however developed for research purposes during AV1 design.  
We think that an implementation written from scratch can achieve faster decoding, in the same way that *ffvp9* was faster than *libvpx*.

## Is dav1d a recursive acronym?

- Yes.

## Can I help?

- Yes. See the [contributions document](CONTRIBUTING.md).

## I am not a developer. Can I help?

- Yes. We need testers, bug reporters, and documentation writers.

## What about the AV1 patent license?

This project is an implementation of a decoder. It gives you no special rights on the AV1 patents.

Please read the [AV1 patent license](doc/PATENTS) that applies to the AV1 specification and codec.
