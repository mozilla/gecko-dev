## [0.26.1] - 2018-10-10

### Fixed
- Fixed support for finding libraries in `bin` directories on Windows

## [0.26.0] - 2018-10-07

### Changed
- Added support for finding libraries with version suffixes on Linux when using runtime linking (e.g., `libclang.so.1`)

## [0.25.0] - 2018-10-06

### Changed
- Added support for versioned libraries on BSDs

## [0.24.0] - 2018-09-15

### Changed
- Reworked finding of libraries (see `README.md` for details)

### Added
- Added support for `clang` 7.0.x

## [0.23.0] - 2018-06-16

### Changed
- Changed `Clang::find` to skip dynamic libraries for an incorrect architecture on Windows

## [0.22.0] - 2018-03-11

### Added
- Added support for `clang` 6.0.x
- Bumped `libc` version to `0.2.39`
- Bumped `libloading` version to `0.5.0`

## [0.21.2] - 2018-02-17

### Changed
- Added original errors to error messages
- Added support for searching for libraries in `LD_LIBRARY_PATH` directories

## [0.21.1] - 2017-11-24

### Changed
- Improved finding of versioned libraries (e.g., `libclang-3.9.so`)

### Fixed
* Fixed compilation failures on the beta and nightly channels caused by a [compiler bug](https://github.com/KyleMayes/clang-sys/pull/69)

## [0.21.0] - 2017-10-11

### Changed
* Replaced `bitflags` usage with constants which avoids crashes on 32-bit Linux platforms

## [0.20.1] - 2017-09-16

### Fixed
- Fixed static linking

## [0.20.0] - 2017-09-14

### Added
- Added support for `clang` 5.0.x
- Added `clang` as a link target of this package
- Added dummy implementations of `is_loaded` for builds with the `static` Cargo feature enabled

## [0.19.0] - 2017-07-02

### Changed
- Bumped `bitflags` version to `0.9.1`
- Added `args` parameter to `Clang::new` function which passes arguments to the Clang executable

## [0.18.0] - 2017-05-16

### Changed
- Improved finding of versioned libraries (e.g., `libclang.so.3.9`)

## [0.17.0] - 2017-05-08

### Changed
- Changed storage type of include search paths from `Vec<PathBuf>` to `Option<Vec<PathBuf>>`

## [0.16.0] - 2017-05-02

### Changed
- Bumped `libloading` version to `0.4.0`

## [0.15.2] - 2017-04-28

### Fixed
- Fixed finding of `libclang.so.1` on Linux

## [0.15.1] - 2017-03-29

### Fixed
- Fixed static linking when libraries are in [different directories](https://github.com/KyleMayes/clang-sys/issues/50)

## [0.15.0] - 2017-03-13

### Added
- Added support for `clang` 4.0.x

### Changed
- Changed functions in the `Functions` struct to be `unsafe` (`runtime` feature only)
- Changed `Clang::find` method to ignore directories and non-executable files
- Changed `Clang::find` to skip dynamic libraries for an incorrect architecture on FreeBSD and Linux
- Bumped `bitflags` version to `0.7.0`

## [0.14.0] - 2017-01-30

### Changed
- Changed all enum types from tuple structs to raw integers to avoid
  [segmentation faults](https://github.com/rust-lang/rust/issues/39394) on some platforms

## [0.13.0] - 2017-01-29

### Changed
- Changed all opaque pointers types from tuple structs to raw pointers to avoid
  [segmentation faults](https://github.com/rust-lang/rust/issues/39394) on some platforms

## [0.12.0] - 2016-12-13

### Changed
- Altered the runtime linking API to allow for testing the presence of functions

## [0.11.1] - 2016-12-07

### Added
- Added support for linking to Clang on Windows from unofficial LLVM sources such as MSYS and MinGW

## [0.11.0] - 2016-10-07

### Changed
- Changed all enums from Rust enums to typed constants to avoid
  [undefined behavior](https://github.com/KyleMayes/clang-sys/issues/42)

## [0.10.1] - 2016-08-21

### Changed
- Changed static linking on FreeBSD and OS X to link against `libc++` instead of `libstd++`

## [0.10.0] - 2016-08-01

### Changed
- Added `runtime` Cargo feature that links to `libclang` shared library at runtime
- Added `from_raw` method to `CXTypeLayoutError` enum
- Added implementations of `Deref` for opaque FFI structs
- Changed `Default` implementations for structs to zero out the struct

## [0.9.0] - 2016-07-21

### Added
- Added documentation bindings

## [0.8.1] - 2016-07-20

### Changed
- Added `CLANG_PATH` environment variable for providing a path to `clang` executable
- Added usage of `llvm-config` to search for `clang`
- Added usage of `xcodebuild` to search for `clang` on OS X

## [0.8.0] - 2016-07-18

### Added
- Added support for `clang` 3.9.x

### Changed
- Bumped `libc` version to `0.2.14`

### Fixed
- Fixed `LIBCLANG_PATH` usage on Windows to search both the `bin` and `lib` directories
- Fixed search path parsing on OS X
- Fixed search path parsing on Windows
- Fixed default search path ordering on OS X

## [0.7.2] - 2016-06-17

### Fixed
- Fixed finding of `clang` executables when system has executables matching `clang-*`
  (e.g., `clang-format`)

## [0.7.1] - 2016-06-10

### Changed
- Bumped `libc` version to `0.2.12`

### Fixed
- Fixed finding of `clang` executables suffixed by their version (e.g., `clang-3.5`)

## [0.7.0] - 2016-05-31

### Changed
- Changed `Clang` struct `version` field type to `Option<CXVersion>`

## [0.6.0] - 2016-05-26

### Added
- Added `support` module

### Fixed
- Fixed `libclang` linking on FreeBSD
- Fixed `libclang` linking on Windows with the MSVC toolchain
- Improved `libclang` static linking

## [0.5.4] - 20160-5-19

### Changed
- Added implementations of `Default` for FFI structs

## [0.5.3] - 2016-05-17

### Changed
- Bumped `bitflags` version to `0.7.0`

## [0.5.2] - 2016-05-12

### Fixed
- Fixed `libclang` static linking

## [0.5.1] - 2016-05-10

### Fixed
- Fixed `libclang` linking on OS X
- Fixed `libclang` linking on Windows

## [0.5.0] - 2016-05-10

### Removed
- Removed `rustc_version` dependency
- Removed support for `LIBCLANG_STATIC` environment variable

### Changed
- Bumped `bitflags` version to `0.6.0`
- Bumped `libc` version to `0.2.11`
- Improved `libclang` search path
- Improved `libclang` static linking

## [0.4.2] - 2016-04-20

### Changed
- Bumped `libc` version to `0.2.10`

## [0.4.1] - 2016-04-02

### Changed
- Bumped `libc` version to `0.2.9`
- Bumped `rustc_version` version to `0.1.7`

## [0.4.0] - 2016-03-28

### Removed
- Removed support for `clang` 3.4.x

## [0.3.1] - 2016-03-21

### Added
- Added support for finding `libclang`

## [0.3.0] - 2016-03-16

### Removed
- Removed build system types and functions

### Added
- Added support for `clang` 3.4.x

### Changed
- Bumped `bitflags` version to `0.5.0`
- Bumped `libc` version to `0.2.8`

## [0.2.1] - 2016-02-13

### Changed
- Simplified internal usage of conditional compilation
- Bumped `bitflags` version to `0.4.0`
- Bumped `libc` version to `0.2.7`
- Bumped `rustc_version` version to `0.1.6`

## [0.2.0] - 2016-02-12

### Added
- Added support for `clang` 3.8.x

## [0.1.2] - 2015-12-29

### Added
- Added derivations of `Debug` for FFI structs

## [0.1.1] - 2015-12-26

### Added
- Added derivations of `PartialOrd` and `Ord` for FFI enums

## [0.1.0] - 2015-12-22
- Initial release
