# minidump

[![crates.io](https://img.shields.io/crates/v/minidump.svg)](https://crates.io/crates/minidump) [![](https://docs.rs/minidump/badge.svg)](https://docs.rs/minidump)

Basic parsing of the minidump format.

If you want richer analysis of the minidump (such as stackwalking and symbolication), use [minidump-processor](https://crates.io/crates/minidump-processor).


# Usage

The primary API for this library is the `Minidump` struct, which can be
instantiated by calling the `Minidump::read` or `Minidump::read_path` methods.

Succesfully parsing a Minidump struct means the minidump has a minimally valid
header and stream directory. Individual streams are only parsed when they're
requested.

Although you may enumerate the streams in a minidump with methods like
`Minidump::all_streams`, this is only really useful for debugging. Instead
you should statically request streams with `Minidump::get_stream`.

Depending on what analysis you're trying to perform, you may:

* Consider it an error for a stream to be missing (using `?` or `unwrap`)
* Branch on the presence of stream to conditionally refine your analysis
* Use a stream's `Default` implementation to get an "empty" instance
  (with `unwrap_or_default`)

```rust
use minidump::*;

fn main() -> Result<(), Error> {
    // Read the minidump from a file
    let mut dump = minidump::Minidump::read_path("../testdata/test.dmp")?;

    // Statically request (and require) several streams we care about:
    let system_info = dump.get_stream::<MinidumpSystemInfo>()?;
    let exception = dump.get_stream::<MinidumpException>()?;

    // Combine the contents of the streams to perform more refined analysis
    let crash_reason = exception.get_crash_reason(system_info.os, system_info.cpu);

    // Conditionally analyze a stream
    if let Ok(threads) = dump.get_stream::<MinidumpThreadList>() {
        // Use `Default` to try to make progress when a stream is missing.
        // This is especially natural for MinidumpMemoryList because
        // everything needs to handle memory lookups failing anyway.
        let mem = dump.get_memory().unwrap_or_default();

        for thread in &threads.threads {
            let stack = thread.stack_memory(&mem);
            // ...
        }
    }
    Ok(())
}
```