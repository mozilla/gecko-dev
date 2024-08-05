// Copyright 2015 Ted Mielczarek. See the COPYRIGHT
// file at the top-level directory of this distribution.

//! A parser for the minidump file format.
//!
//! The `minidump` module provides a parser for the
//! [minidump][minidump] file format as produced by Microsoft's
//! [`MinidumpWriteDump`][minidumpwritedump] API and the
//! [Google Breakpad][breakpad] library.
//!
//!
//!
//! # Usage
//!
//! The primary API for this library is the [`Minidump`][] struct, which can be
//! instantiated by calling the [`Minidump::read`][] or [`Minidump::read_path`][]
//! methods.
//!
//! Successfully parsing a Minidump struct means the minidump has a minimally valid
//! header and stream directory. Individual streams are only parsed when they're
//! requested.
//!
//! Although you may enumerate the streams in a minidump with methods like
//! [`Minidump::all_streams`][], this is only really useful for debugging. Instead
//! you should statically request streams with [`Minidump::get_stream`][].
//! Depending on what analysis you're trying to perform, you may:
//!
//! * Consider it an error for a stream to be missing (using `?` or `unwrap`)
//! * Branch on the presence of stream to conditionally refine your analysis
//! * Use a stream's `Default` implementation to get an "empty" instance
//!   (with `unwrap_or_default`)
//!
//! ```
//! use minidump::*;
//!
//! fn main() -> Result<(), Error> {
//!     // Read the minidump from a file
//!     let mut dump = minidump::Minidump::read_path("../testdata/test.dmp")?;
//!
//!     // Statically request (and require) several streams we care about:
//!     let system_info = dump.get_stream::<MinidumpSystemInfo>()?;
//!     let exception = dump.get_stream::<MinidumpException>()?;
//!
//!     // Combine the contents of the streams to perform more refined analysis
//!     let crash_reason = exception.get_crash_reason(system_info.os, system_info.cpu);
//!
//!     // Conditionally analyze a stream
//!     if let Ok(threads) = dump.get_stream::<MinidumpThreadList>() {
//!         // Use `Default` to try to make progress when a stream is missing.
//!         // This is especially natural for MinidumpMemoryList because
//!         // everything needs to handle memory lookups failing anyway.
//!         let mem = dump.get_memory().unwrap_or_default();
//!
//!         for thread in &threads.threads {
//!             let stack = thread.stack_memory(&mem);
//!             // ...
//!         }
//!     }
//!
//!     Ok(())
//! }
//! ```
//!
//! Generally speaking, there isn't any reason to distinguish between a stream being
//! absent and it being corrupt. Just ask for what you want and we'll do our best
//! to give it to you.
//!
//! Everything else you would want to do with a Minidump is specific to the
//! individual streams:
//!
//! * [`MinidumpAssertion`][]
//! * [`MinidumpBreakpadInfo`][]
//! * [`MinidumpCrashpadInfo`][]
//! * [`MinidumpException`][]
//! * [`MinidumpLinuxCpuInfo`][]
//! * [`MinidumpLinuxEnviron`][]
//! * [`MinidumpLinuxLsbRelease`][]
//! * [`MinidumpLinuxMaps`][]
//! * [`MinidumpLinuxProcStatus`][]
//! * [`MinidumpMacCrashInfo`][]
//! * [`MinidumpMacBootargs`][]
//! * [`MinidumpMemoryList`][]
//! * [`MinidumpMemoryInfoList`][]
//! * [`MinidumpMiscInfo`][]
//! * [`MinidumpModuleList`][]
//! * [`MinidumpSystemInfo`][]
//! * [`MinidumpThreadList`][]
//! * [`MinidumpThreadNames`][]
//! * [`MinidumpUnloadedModuleList`][]
//! * [`MinidumpLinuxProcLimits`][]
//!
//!
//!
//!
//! # Notable Streams
//!
//! There's a lot of different Minidump Streams, but some are especially
//! notable/fundamental:
//!
//! [`MinidumpSystemInfo`][] includes details about the hardware and operating
//! system that the crash occured on. This information is often required to
//! properly interpret the other streams of the minidump, as they contain
//! platform-specific values.
//!
//! [`MinidumpException`][] includes actual details about where and why the crash
//! occured.
//!
//! [`MinidumpThreadList`][] includes the registers and stack memory of every thread
//! in the program at the time of the crash. This enables generating backtraces for
//! every thread.
//!
//! [`MinidumpMemoryList`][] maps the crashing program's runtime addresses (such as
//! `$rsp`) to ranges of memory in the Minidump.
//!
//! [`MinidumpModuleList`][] includes info on all the modules (libraries) that were
//! linked into the crashing program. This enables symbolication, as you can map
//! instruction addresses back to offsets in a specific library's binary.
//!
//!
//!
//!
//! # What is a Minidump?
//!
//! Minidumps capture the state of a crashing process (threads, stack memory,
//! registers, dlls), why it crashed (crashing thread, error codes, error
//! messages), and details about the system the program was running on (os, cpu).
//!
//! The information in a minidump is divided up into a series of
//! independent "streams". If you want a specific piece of information, you must
//! know the stream that contains it, and then look up that stream in the
//! minidump's directory. Most streams are pretty straight-forward -- you can guess
//! what you might find in [`MinidumpThreadList`][] or [`MinidumpSystemInfo`][]
//! -- but others -- like [`MinidumpMiscInfo`][] -- are a bit more random.
//!
//! This [format][minidump] was initially defined by Microsoft, as Windows has long
//! included [system apis to generate minidumps][minidumpwritedump]. But lots of
//! software gets made for operating systems other than Windows, where no such
//! native support for minidumps is present. [google-breakpad][breakpad] was
//! created to extend Microsoft's minidump format to other platforms, and defines
//! minidump generators for things like Linux and MacOS.
//!
//! I do not believe that Microsoft and Breakpad officially collaborate on the
//! format, it's just designed to be very extensible, so it's easy to add random
//! stuff to a minidump in ways that don't break old tools and likely won't
//! interfere with future versions. That said, Microsoft does now develop
//! cross-platform products that make use of Breakpad, such as VSCode, so at very
//! least their crash reporting infra deals with Breakpad minidumps.
//!
//! The rust-minidump crates are specifically designed to support Breakpad's
//! extended minidump format (and native Windows minidumps, which should in theory
//! just be a subset). That said, rust-minidump doesn't yet (and probably won't
//! ever) support *everything*. There's a lot of random stuff that either Microsoft
//! or Breakpad have defined over the years that we just, do not have any use for
//! at the moment. Not a lot of demand for handling minidumps for PlayStation 3,
//! SPARC, or Windows CE these days.
//!
//!
//!
//!
//!
//! # The Minidump Format
//!
//! This section is dedicated to describing how to parse minidumps, for anyone
//! wanting to maintain this code or write their own parser.
//!
//! Minidumps are a binary format. This format is simultaneously very simple and
//! very complicated.
//!
//! The simple part of a minidump is that it's basically just an array of pointers
//! to different typed "Streams" (system info, exception info, threads, memory
//! mappings, etc.). So if you want to lookup the system info, you just search the
//! array for a system info stream and interpret that range of memory as that
//! stream.
//!
//! The complicated part of a minidump is the fact that every stream contains
//! totally different information in totally different formats. Sure, there are
//! families of streams that have the same general structure, but you've still got
//! to write custom code to interpret the values meaningfully and figure out what
//! on earth that information is useful for.
//!
//! Sometimes the answer to "what is it useful for?" is "I don't know but maybe
//! we'll find a use for it later". This is genuinely useful because it allows us
//! to add new analyses long after a crash occurs and gain new insights that the
//! minidump format wasn't explicitly designed to provide.
//!
//! This is all to say that, beyond the basic layout of the minidump header and
//! directory, it's basically just a big ball of random formats with independent
//! formats and layout -- and everyone is technically free to come up with their
//! own custom Streams that they can just toss in there, so trying to cover
//! everything is kind of impossible? Lets see how far we get!
//!
//!
//!
//! ## The Minidump Header and Directory
//!
//! The first thing in a Minidump is the [`MINIDUMP_HEADER`][format::MINIDUMP_HEADER], which has the
//! following layout:
//!
//! ```
//! pub struct MINIDUMP_HEADER {
//!     pub signature: u32,
//!     pub version: u32,
//!     pub stream_count: u32,
//!     pub stream_directory_rva: RVA,
//!     pub checksum: u32,
//!     pub time_date_stamp: u32,
//!     pub flags: u64,
//! }
//!
//! /// Offset into the minidump
//! pub type RVA = u32;
//! ```
//!
//! The `signature` is always [`MINIDUMP_SIGNATURE`][format::MINIDUMP_SIGNATURE] = `0x504d444d`
//! ("MDMP" in ascii). You can use this to detect whether the minidump is little-endian or
//! big-endian (minidumps always have the endianess of platform they were generated
//! on, since they contain lots of raw memory from the process, but at this point
//! we don't know what that platform is).
//!
//! The lower 16 bits of `version` are always
//! [`MINIDUMP_VERSION`][format::MINIDUMP_VERSION] = 42899.
//! (The high bits contain implementation-specific values that you should just
//! ignore).
//!
//! `stream_directory_rva` and `stream_count` are the location (offset from the
//! start of the file, in bytes) and size of the stream directory, respectively.
//!
//! `checksum` is some kind of checksum of the minidump itself (which may be null),
//! but the algorithm isn't specified, and rust-minidump doesn't check it.
//!
//! `time_date_stamp` is a Windows `time_t` of when the miniump was generated.
//!
//! `flags` are a [`MINIDUMP_TYPE`][MINIDUMP_TYPE] which largely just specify what you can expect
//! to find in the minidump. This is unused by rust-minidump since this information
//! is generally redundant with the stream directory and flags within the streams
//! that we need to check anyway. (e.g. instead of checking that this is a
//! `MiniDumpWithUnloadedModules`, you can just check the directory for the
//! [`MinidumpUnloadedModuleList`][] stream.)
//!
//! At `stream_directory_rva` (typically immediately after the header) you will find
//! an array of `stream_count` [`MINIDUMP_DIRECTORY`][format::MINIDUMP_DIRECTORY] entries,
//! with the following layout:
//!
//! ```
//! pub struct MINIDUMP_DIRECTORY {
//!     /// The type of the stream
//!     pub stream_type: u32,
//!     /// The location of the stream contents within the dump.
//!     pub location: MINIDUMP_LOCATION_DESCRIPTOR,
//! }
//!
//! /// A "slice" of the minidump
//! pub struct MINIDUMP_LOCATION_DESCRIPTOR {
//!     /// The size of this data (in bytes)
//!     pub data_size: u32,
//!     /// The offset to this data within the minidump file.
//!     pub rva: RVA,
//! }
//!
//! /// Offset into the minidump
//! pub type RVA = u32;
//! ```
//!
//! Known `stream_type` values are defined in
//! [`MINIDUMP_STREAM_TYPE`][format::MINIDUMP_STREAM_TYPE], but users
//! are allowed to define their own stream types, so it's normal to see unknown
//! types (this is the primary mechanism breakpad uses to extend the format without
//! causing upstream problems).
//!
//! And that's it! Everything else in a minidump is just all the different types of
//! stream. As of this writing, rust-minidump is aware of 51 different types of
//! stream, and implements 18 of them (there's a long tail of platform-specific and
//! domain-specific streams, so that isn't as bad as it sounds).
//!
//!
//!
//!
//! ## Stream Format Families
//!
//! Although every stream can do whatever it wants, there's a lot of streams that
//! are basically "a struct" or "a list of structs", so the same header formats and
//! layouts are used in several places. (This is descriptive, so these aren't
//! necessarily official terms/concepts.)
//!
//!
//!
//! ### Plain Old Struct Streams
//!
//! A stream that's just a struct.
//!
//! That's it. Just read the struct out of the stream. Although it might contain
//! RVAs to other data, which may or may not be relative to the start of the stream
//! or the start of the file (annoyingly inconsistent between streams).
//!
//! Known members of this family:
//!
//! * [`MinidumpAssertion`][] (contains [`MINIDUMP_ASSERTION_INFO`][format::MINIDUMP_ASSERTION_INFO])
//! * [`MinidumpBreakpadInfo`][] (contains [`MINIDUMP_BREAKPAD_INFO`][format::MINIDUMP_BREAKPAD_INFO])
//! * [`MinidumpCrashpadInfo`][] (contains [`MINIDUMP_CRASHPAD_INFO`][format::MINIDUMP_CRASHPAD_INFO])
//! * [`MinidumpException`][] (contains [`MINIDUMP_EXCEPTION_STREAM`][format::MINIDUMP_EXCEPTION_STREAM])
//! * [`MinidumpSystemInfo`][] (contains [`MINIDUMP_SYSTEM_INFO`][format::MINIDUMP_SYSTEM_INFO])
//!
//!
//!
//! ### List Streams
//!
//! A list of some entry type.
//!
//! A `u32` count of entries followed by an array of entries. There may be padding
//! between the count and the entries. The array should be "right-justified" in the
//! stream (the stream ends exactly where the array does), so you can use the
//! difference between the array's expected size and the rest of the stream's size
//! to determine the padding.
//!
//! This format is used by a lot of the oldest (and therefore most important)
//! minidump streams.
//!
//! Known members of this family:
//!
//! * [`MinidumpMemoryList`] (entries are [`MINIDUMP_MEMORY_DESCRIPTOR`][format::MINIDUMP_MEMORY_DESCRIPTOR])
//! * [`MinidumpModuleList`] (entries are [`MINIDUMP_MODULE`][format::MINIDUMP_MODULE])
//! * [`MinidumpThreadList`] (entries are [`MINIDUMP_THREAD`][format::MINIDUMP_THREAD])
//! * [`MinidumpThreadNames`] (entries are [`MINIDUMP_THREAD_NAME`][format::MINIDUMP_THREAD_NAME])
//! * `MINIDUMP_THREAD_EX_LIST` (yes, the stream with "EX_LIST" in the name isn't an
//!   EX list, names are hard.)
//!
//! The stream [`MinidumpMemory64List`] is a variant of list stream. It starts with
//! a `u64` count of entries, a 64-bit shared RVA for all entries, then followed by
//! an array of entires [`MINIDUMP_MEMORY_DESCRIPTOR64`][format::MINIDUMP_MEMORY_DESCRIPTOR64].
//!
//!
//! ### EX List Streams
//!
//! A newer and more flexible version of list streams. (so EXtreme!!!)
//!
//! EX list streams start with this header:
//!
//! ```
//! struct EX_LIST_HEADER {
//!   /// Size (in bytes) of this header (array starts immediately after)
//!   pub size_of_header: u32,
//!   /// Size (in bytes) of an entry in the array
//!   pub size_of_entry: u32,
//!   /// The number of entries in the array
//!   pub number_of_entries: u32,
//! }
//! ```
//!
//! This design allows newer versions of the stream to be introduced, and for fields
//! to be added to the end of an entry type. I am not aware of an instance where
//! this flexibility has been used yet, but in theory you could identify "versions"
//! of the stream format by size, and older versions don't need to worry about
//! unknown future revisions, because they can just ignore the trailing bytes of
//! each entry.
//!
//! Known members of this family:
//!
//! * [`MinidumpMemoryInfoList`][] (entries are [`MINIDUMP_MEMORY_INFO`][format::MINIDUMP_MEMORY_INFO])
//! * [`MinidumpUnloadedModuleList`][] (entries are [`MINIDUMP_UNLOADED_MODULE`][format::MINIDUMP_UNLOADED_MODULE])
//! * [`MinidumpHandleDataStream`][] is a slight variation of this format with different
//!   filed names and a trailing `u32` member reserved for future use (entries
//!   are [`MINIDUMP_HANDLE_DESCRIPTOR`][format::MINIDUMP_HANDLE_DESCRIPTOR] and
//!   [`MINIDUMP_HANDLE_DESCRIPTOR_2`][format::MINIDUMP_HANDLE_DESCRIPTOR_2])
//! * [`MinidumpThreadInfoList`][] (entries are [`MINIDUMP_THREAD_INFO`][format::MINIDUMP_THREAD_INFO])
//!
//!
//!
//! ### Linux List Streams
//!
//! A dump of a special linux file like `/proc/cpuinfo`.
//!
//! These streams are plain text ([`strings::LinuxOsString`][]) files containing
//! line-delimited key-value pairs, like:
//!
//! ```text
//! processor       : 0
//! vendor_id       : GenuineIntel
//! cpu family      : 6
//! model           : 45
//! model name      : Intel(R) Xeon(R) CPU E5-2660 0 @ 2.20GHz
//! ```
//!
//! Whitespace and separators vary from stream to stream.
//!
//! Known members of this family:
//!
//! * [`MinidumpLinuxCpuInfo`][] (separator is `:`)
//! * [`MinidumpLinuxEnviron`][] (separator is `=`)
//! * [`MinidumpLinuxLsbRelease`][] (separator is `=`)
//! * [`MinidumpLinuxProcStatus`][] (separator is `:`)
//! * [`MinidumpLinuxProcLimits`][] (separator is ` `)
//!
//!
//!
//! [MINIDUMP_TYPE]: https://docs.microsoft.com/en-us/windows/win32/api/minidumpapiset/ne-minidumpapiset-minidump_type
//! [minidump]: https://msdn.microsoft.com/en-us/library/windows/desktop/ms680369%28v=vs.85%29.aspx
//! [minidumpwritedump]: https://msdn.microsoft.com/en-us/library/windows/desktop/ms680360%28v=vs.85%29.aspx
//! [breakpad]: https://chromium.googlesource.com/breakpad/breakpad/+/master/

#![warn(missing_debug_implementations)]

#[cfg(doctest)]
doc_comment::doctest!("../README.md");

pub use scroll::Endian;

mod context;
mod iostuff;
mod minidump;

pub use minidump_common::format;
pub use minidump_common::traits::Module;

pub use crate::iostuff::Readable;
pub use crate::minidump::*;

pub mod strings;
pub mod system_info;
