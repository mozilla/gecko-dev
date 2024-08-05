[![crates.io](https://img.shields.io/crates/v/breakpad-symbols.svg)](https://crates.io/crates/breakpad-symbols) [![](https://docs.rs/breakpad-symbols/badge.svg)](https://docs.rs/breakpad-symbols)

Fetching, parsing, and evaluation of Breakpad's [text format .sym files](https://chromium.googlesource.com/breakpad/breakpad/+/master/docs/symbol_files.md).

Fetches breakpad symbol files from disk or [a server the conforms the the Tecken protocol](https://tecken.readthedocs.io/en/latest/download.html), and provides an on-disk temp symbol file cache.

Permissively parses breakpad symbol files to smooth over the unfortunately-very-common situation of corrupt debuginfo. Will generally try to recover the parse by discarding corrupt lines or arbitrarily picking one value when conflicts are found.

Provides an API for resolving functions and source line info by address from symbol files.

Provides an API for evaluating breakpad CFI (and WIN) expressions.

This is primarily designed for use by [minidump-processor](https://crates.io/crates/minidump-processor).
