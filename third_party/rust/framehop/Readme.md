[![crates.io page](https://img.shields.io/crates/v/framehop.svg)](https://crates.io/crates/framehop)
[![docs.rs page](https://docs.rs/framehop/badge.svg)](https://docs.rs/framehop/)

# framehop

Framehop is a stack frame unwinder written in 100% Rust. It produces high quality stacks at high speed, on multiple platforms and architectures, without an expensive pre-processing step for unwind information. This makes it suitable for sampling profilers.

It currently supports unwinding x86_64 and aarch64, with unwind information formats commonly used on Windows, macOS, Linux and Android.

You give framehop register values, stack memory and unwind data, and framehop produces a list of return addresses.

Framehop can be used in the following scenarios:

 - Live unwinding of a remote process. This is how [`samply`](https://github.com/mstange/samply/) uses it.
 - Offline unwinding from saved registers and stack bytes, even on a different machine, a different OS, or a different CPU architecture.
 - Live unwinding inside the same process. This is currently unproven, but should work as long as you can do heap allocation before sampling, in order to allocate a cache and to update the list of modules. The actual unwinding does not require any heap allocation and should work even inside a signal handler, as long as you use `MustNotAllocateDuringUnwind`.

As a user of framehop, your responsibilities are the following:

 - You need to enumerate the modules (libraries) that are loaded in the sampled process ahead of time, or ideally maintain a live list which is updated whenever modules are loaded / unloaded.
 - You need to provide address ranges and unwind section data for those modules.
 - When sampling, you provide the register values and a callback  to read arbitrary stack memory without segfaulting.
 - On aarch64, picking the right bitmask to strip pointer authentication bits from return addresses is up to you.
 - You will need to do symbol resolution yourself, if you want function names. Framehop only produces addresses, it does not do any symbolication.

In turn, framehop solves the following problems:

 - It parses a number of different unwind information formats. At the moment, it supports the following:
   - Apple's Compact Unwinding Format, in `__unwind_info` (macOS)
   - DWARF CFI in `.eh_frame` (using `.eh_frame_hdr` as an index, if available)
   - DWARF CFI in `.debug_frame`
   - PE unwind info in `.pdata`, `.rdata` and `.xdata` (for Windows x86_64)
 - It supports correct unwinding even when the program is interrupted inside a function prologue or epilogue. On macOS, it has to analyze assembly instructions in order to do this.
 - On x86_64 and aarch64, it falls back to frame pointer unwinding if it cannot find unwind information for an address.
 - It caches the unwind rule for each address in a fixed-size cache, so that repeated unwinding from the same address is even faster.
 - It generates binary search indexes for unwind information formats which don't have them. Specifically, for `.debug_frame` and for `.eh_frame` without `.eh_frame_hdr`.
 - It does a reasonable job of detecting the end of the stack, so that you can differentiate between properly terminated stacks and prematurely truncated stacks.

Framehop is not suitable for debuggers or to implement exception handling. Debuggers usually need to recover all register values for every frame whereas framehop only cares about return addresses. And exception handling needs the ability to call destructors, which is also a non-goal for framehop.

## Speed

Framehop is so fast that stack walking is a miniscule part of sampling in both scenarios where I've tried it.

In [this samply example](https://share.firefox.dev/3s6mQKl) of profiling a single-threaded Rust application, walking the stack takes a quarter of the time it take to query macOS for the thread's register values. In [another samply example](https://share.firefox.dev/3ksWaPt) of profiling a Firefox build without frame pointers, the dwarf unwinding takes 4x as long as the querying of the register values, but is still overall cheaper than the cost of thread_suspend + thread_get_state + thread_resume.

In [this example of processing a `perf.data` file](https://share.firefox.dev/3vSQOTb), the bottleneck is reading the bytes from disk, rather than stackwalking. [With a warm file cache](https://share.firefox.dev/3Kt6sK1), the cost of stack walking is still comparable to the cost of copying the bytes from the file cache, and most of the stack walking time is spent reading return addresses from the stack bytes.

Framehop achieves this speed in the following ways:

 1. It only recovers registers which are needed for computing return addresses. On x86_64 that's `rip`, `rsp` and `rbp`, and on aarch64 that's `lr`, `sp` and `fp`. All other registers are not needed - in theory they could be used as inputs to DWARF CFI expressions, but in practice they are not.
 2. It uses zero-copy parsing wherever possible. For example, the bytes in `__unwind_info` are only accessed during unwinding, and the binary search happens right inside the original `__unwind_info` memory. For DWARF unwinding, framehop uses the excellent [`gimli` crate](https://github.com/gimli-rs/gimli/), which was written with performance in mind.
 3. It uses binary search to find the correct unwind rule in all supported unwind information formats. For formats without an built-in index, it creates an index when the module is added.
 4. It caches unwind rules based on address. In practice, the 509-slot cache achieves a hit rate of around 80% on complicated code like Firefox (with the cache being shared across all Firefox processes). When profiling simpler applications, the hit rate is likely much higher.

Furthermore, adding a module is fast too because framehop only does minimal up-front parsing and processing - really, the only thing it does is to create the index of FDE offsets for `.eh_frame` / `.debug_frame`.

## Current State and Roadmap

Framehop is still a work in progress. Its API is subject to change. The API churn probably won't quieten down at least until we have one or two 32 bit architectures implemented.

That said, framehop works remarkably well on the supported platforms, and is definitely worth a try if you can stomach the frequent API breakages. Please file issues if you run into any trouble or have suggestions.

Eventually I'd like to use framehop as a replacement for Lul in the Gecko profiler (Firefox's built-in profiler). For that we'll also want to add x86 support (for 32 bit Windows and Linux) and EHABI / EXIDX support (for 32 bit ARM Android).

## Example

```rust
use framehop::aarch64::{CacheAarch64, UnwindRegsAarch64, UnwinderAarch64};
use framehop::{ExplicitModuleSectionInfo, FrameAddress, Module};

let mut cache = CacheAarch64::<_>::new();
let mut unwinder = UnwinderAarch64::new();

let module = Module::new(
    "mybinary".to_string(),
    0x1003fc000..0x100634000,
    0x1003fc000,
    ExplicitModuleSectionInfo {
        base_svma: 0x100000000,
        text_svma: Some(0x100000b64..0x1001d2d18),
        text: Some(vec![/* __text */]),
        stubs_svma: Some(0x1001d2d18..0x1001d309c),
        stub_helper_svma: Some(0x1001d309c..0x1001d3438),
        got_svma: Some(0x100238000..0x100238010),
        unwind_info: Some(vec![/* __unwind_info */]),
        eh_frame_svma: Some(0x100237f80..0x100237ffc),
        eh_frame: Some(vec![/* __eh_frame */]),
        text_segment_svma: Some(0x1003fc000..0x100634000),
        text_segment: Some(vec![/* __TEXT */]),
        ..Default::default()
    },
);
unwinder.add_module(module);

let pc = 0x1003fc000 + 0x1292c0;
let lr = 0x1003fc000 + 0xe4830;
let sp = 0x10;
let fp = 0x20;
let stack = [
    1, 2, 3, 4, 0x40, 0x1003fc000 + 0x100dc4,
    5, 6, 0x70, 0x1003fc000 + 0x12ca28,
    7, 8, 9, 10, 0x0, 0x0,
];
let mut read_stack = |addr| stack.get((addr / 8) as usize).cloned().ok_or(());

use framehop::Unwinder;
let mut iter = unwinder.iter_frames(
    pc,
    UnwindRegsAarch64::new(lr, sp, fp),
    &mut cache,
    &mut read_stack,
);

let mut frames = Vec::new();
while let Ok(Some(frame)) = iter.next() {
    frames.push(frame);
}

assert_eq!(
    frames,
    vec![
        FrameAddress::from_instruction_pointer(0x1003fc000 + 0x1292c0),
        FrameAddress::from_return_address(0x1003fc000 + 0x100dc4).unwrap(),
        FrameAddress::from_return_address(0x1003fc000 + 0x12ca28).unwrap()
    ]
);
```

## Recommended Reading and Tools

Here's a list of articles I found useful during development:

 - [Reliable and Fast DWARF-Based Stack Unwinding](https://hal.inria.fr/hal-02297690/document), also available [as a presentation](https://deepspec.org/events/dsw18/zappa-nardelli-deepspec18.pdf). This is **the** unwinding reference document. If want to read just one thing, read this. This article explains the background super clearly, and is very approachable. It shows how assembly and unwind information correspond to each other and has lots of examples that are easy to understand.
 - [How fast can CFI/EXIDX-based stack unwinding be?](https://blog.mozilla.org/jseward/2013/08/29/how-fast-can-cfiexidx-based-stack-unwinding-be/), by Julian Seward
 - [Unwinding a Stack by Hand with Frame Pointers and ORC](https://blogs.oracle.com/linux/post/unwinding-stack-frame-pointers-and-orc), by Stephen Brennan
 - [Aarch64 DWARF register names](https://github.com/ARM-software/abi-aa/blob/main/aadwarf64/aadwarf64.rst#dwarf-register-names)

I used these tools very frequently:

 - [Hopper Disassembler](https://www.hopperapp.com/), to look at assembly code.
 - `llvm-dwarfdump --eh-frame mylib.so` to display DWARF unwind information.
 - `llvm-objdump --section-headers mylib.so` to display section information.
 - `unwindinfodump mylib.dylib` to display compact unwind information. (Install using `cargo install --examples macho-unwind-info`, see [macho-unwind-info](https://github.com/mstange/macho-unwind-info/blob/main/examples/unwindinfodump.rs).)

## License

Licensed under either of

  * Apache License, Version 2.0 ([`LICENSE-APACHE`](./LICENSE-APACHE) or http://www.apache.org/licenses/LICENSE-2.0)
  * MIT license ([`LICENSE-MIT`](./LICENSE-MIT) or http://opensource.org/licenses/MIT)

at your option.

Unless you explicitly state otherwise, any contribution intentionally submitted
for inclusion in the work by you, as defined in the Apache-2.0 license, shall be
dual licensed as above, without any additional terms or conditions.
