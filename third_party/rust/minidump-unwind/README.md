# minidump-unwind

[![crates.io](https://img.shields.io/crates/v/minidump-unwind.svg)](https://crates.io/crates/minidump-unwind) [![](https://docs.rs/minidump-unwind/badge.svg)](https://docs.rs/minidump-unwind)

A library for unwinding and producing stack traces from minidump files. This crate provides APIs for
producing symbolicated stack traces for the threads in a minidump. The primary entrypoint is the
`walk_stack` function.

If you want lower-level access to the minidump's contents, use the [minidump](https://crates.io/crates/minidump) crate.

If you want higher-level functionality build on top of this crate, see
[minidump-processor](https://crates.io/crates/minidump-processor).

## Example Usage

```rust
use minidump::{
    Minidump, MinidumpException, MinidumpMiscInfo, MinidumpModuleList, MinidumpSystemInfo,
    UnifiedMemoryList
};
use minidump_unwind::{CallStack, http_symbol_supplier, Symbolizer, SystemInfo, walk_stack};

#[tokio::main]
async fn main() {
    // Read the minidump
    let dump = Minidump::read_path("../testdata/test.dmp").unwrap();

    // Configure the symbolizer and processor
    let symbols_urls = vec![String::from("https://symbols.totallyrealwebsite.org")];
    let symbols_paths = vec![];
    let mut symbols_cache = std::env::temp_dir();
    symbols_cache.push("minidump-cache");
    let symbols_tmp = std::env::temp_dir();
    let timeout = std::time::Duration::from_secs(1000);

    // Specify a symbol supplier (here we're using the most powerful one, the http supplier)
    let provider = Symbolizer::new(http_symbol_supplier(
        symbols_paths,
        symbols_urls,
        symbols_cache,
        symbols_tmp,
        timeout,
    ));

    let system_info: MinidumpSystemInfo = dump.get_stream().unwrap();
    let misc_info: Option<MinidumpMiscInfo> = dump.get_stream().ok();
    let modules: MinidumpModuleList = dump.get_stream().unwrap();
    let exception: MinidumpException = dump.get_stream().unwrap();
    let exception_context = exception.context(&system_info, misc_info.as_ref()).unwrap();
    let memory_list = dump.get_stream().map(UnifiedMemoryList::Memory)
        .or_else(|_| dump.get_stream().map(UnifiedMemoryList::Memory64))
        .unwrap();

    let stack_memory = memory_list.memory_at_address(exception_context.get_stack_pointer());

    let mut stack = CallStack::with_context(exception_context.into_owned());

    walk_stack(
        0,
        (),
        &mut stack,
        stack_memory,
        &modules,
        &SystemInfo {
            os: system_info.os,
            os_version: None,
            os_build: None,
            cpu: system_info.cpu,
            cpu_info: system_info.cpu_info().map(|info| info.into_owned()),
            cpu_microcode_version: None,
            cpu_count: 1,
        },
        &provider,
    ).await;

    for frame in stack.frames {
        println!("{:?}", frame);
    }
}
```
