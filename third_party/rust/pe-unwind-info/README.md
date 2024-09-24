[![crates.io page](https://img.shields.io/crates/v/pe-unwind-info.svg)](https://crates.io/crates/pe-unwind-info)
[![docs.rs page](https://docs.rs/pe-unwind-info/badge.svg)](https://docs.rs/pe-unwind-info/)

# pe-unwind-info

A zero-copy parser for the contents of the `.pdata` section and unwind info structures (typically
addressed by the contents of the `.pdata` section).

This library provides low-level, efficient parsers for the function tables in `.pdata` as well as
unwind info structures in other places. On top of this functionality, higher-level functionality to unwind an entire
frame (given a module's contents) is provided. This only copies data as necessary. No heap
allocations are needed.

This currently targets `x86_64` PE modules. `ARM64` support will be added soon.

This library assumes all information is little-endian: as far as I can tell, Windows always either
targets little-endian-only CPUs or configures CPUs which support little- and big-endian to be
little-endian.
