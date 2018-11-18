// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#![cfg_attr(feature = "oom_with_global_alloc",
            feature(global_allocator, alloc, alloc_system, allocator_api))]
#![cfg_attr(feature = "oom_with_hook", feature(alloc_error_hook))]

#[cfg(feature="servo")]
extern crate geckoservo;

extern crate mp4parse_capi;
extern crate nsstring;
extern crate nserror;
extern crate xpcom;
extern crate netwerk_helper;
extern crate prefs_parser;
#[cfg(feature = "gecko_profiler")]
extern crate profiler_helper;
extern crate mozurl;
#[cfg(feature = "quantum_render")]
extern crate webrender_bindings;
#[cfg(feature = "cubeb_pulse_rust")]
extern crate cubeb_pulse;
extern crate encoding_c;
extern crate encoding_glue;
#[cfg(feature = "cubeb-remoting")]
extern crate audioipc_client;
#[cfg(feature = "cubeb-remoting")]
extern crate audioipc_server;
extern crate env_logger;
extern crate u2fhid;
extern crate log;
extern crate cosec;
extern crate rsdparsa_capi;
#[cfg(feature = "spidermonkey_rust")]
extern crate jsrust_shared;

extern crate arrayvec;

use std::boxed::Box;
use std::env;
use std::ffi::{CStr, CString};
use std::os::raw::c_char;
use std::os::raw::c_int;
#[cfg(target_os = "android")]
use log::Level;
#[cfg(not(target_os = "android"))]
use log::Log;
use std::cmp;
use std::panic;
use std::ops::Deref;
use arrayvec::{Array, ArrayString};

extern "C" {
    fn gfx_critical_note(msg: *const c_char);
    #[cfg(target_os = "android")]
    fn __android_log_write(prio: c_int, tag: *const c_char, text: *const c_char) -> c_int;
}

struct GeckoLogger {
    logger: env_logger::Logger
}

impl GeckoLogger {
    fn new() -> GeckoLogger {
        let mut builder = env_logger::Builder::new();
        let default_level = if cfg!(debug_assertions) { "warn" } else { "error" };
        let logger = match env::var("RUST_LOG") {
            Ok(v) => builder.parse(&v).build(),
            _ => builder.parse(default_level).build(),
        };

        GeckoLogger {
            logger
        }
    }

    fn init() -> Result<(), log::SetLoggerError> {
        let gecko_logger = Self::new();

        log::set_max_level(gecko_logger.logger.filter());
        log::set_boxed_logger(Box::new(gecko_logger))
    }

    fn should_log_to_gfx_critical_note(record: &log::Record) -> bool {
        if record.level() == log::Level::Error &&
           record.target().contains("webrender") {
            true
        } else {
            false
        }
    }

    fn maybe_log_to_gfx_critical_note(&self, record: &log::Record) {
        if Self::should_log_to_gfx_critical_note(record) {
            let msg = CString::new(format!("{}", record.args())).unwrap();
            unsafe {
                gfx_critical_note(msg.as_ptr());
            }
        }
    }

    #[cfg(not(target_os = "android"))]
    fn log_out(&self, record: &log::Record) {
        self.logger.log(record);
    }

    #[cfg(target_os = "android")]
    fn log_out(&self, record: &log::Record) {
        let msg = CString::new(format!("{}", record.args())).unwrap();
        let tag = CString::new(record.module_path().unwrap()).unwrap();
        let prio = match record.metadata().level() {
            Level::Error => 6 /* ERROR */,
            Level::Warn => 5 /* WARN */,
            Level::Info => 4 /* INFO */,
            Level::Debug => 3 /* DEBUG */,
            Level::Trace => 2 /* VERBOSE */,
        };
        // Output log directly to android log, since env_logger can output log
        // only to stderr or stdout.
        unsafe {
            __android_log_write(prio, tag.as_ptr(), msg.as_ptr());
        }
    }
}

impl log::Log for GeckoLogger {
    fn enabled(&self, metadata: &log::Metadata) -> bool {
        self.logger.enabled(metadata)
    }

    fn log(&self, record: &log::Record) {
        // Forward log to gfxCriticalNote, if the log should be in gfx crash log.
        self.maybe_log_to_gfx_critical_note(record);
        self.log_out(record);
    }

    fn flush(&self) { }
}

#[no_mangle]
pub extern "C" fn GkRust_Init() {
    // Initialize logging.
    let _ = GeckoLogger::init();
}

#[no_mangle]
pub extern "C" fn GkRust_Shutdown() {
}

/// Used to implement `nsIDebug2::RustPanic` for testing purposes.
#[no_mangle]
pub extern "C" fn intentional_panic(message: *const c_char) {
    panic!("{}", unsafe { CStr::from_ptr(message) }.to_string_lossy());
}

extern "C" {
    // We can't use MOZ_CrashOOL directly because it may be weakly linked
    // to libxul, and rust can't handle that.
    fn GeckoCrashOOL(filename: *const c_char, line: c_int, reason: *const c_char) -> !;
}

/// Truncate a string at the closest unicode character boundary
/// ```
/// assert_eq!(str_truncate_valid("éà", 3), "é");
/// assert_eq!(str_truncate_valid("éà", 4), "éè");
/// ```
fn str_truncate_valid(s: &str, mut mid: usize) -> &str {
    loop {
        if let Some(res) = s.get(..mid) {
            return res;
        }
        mid -= 1;
    }
}

/// Similar to ArrayString, but with terminating nul character.
#[derive(Debug, PartialEq)]
struct ArrayCString<A: Array<Item = u8>> {
    inner: ArrayString<A>,
}

impl<S: AsRef<str>, A: Array<Item = u8>> From<S> for ArrayCString<A> {
    /// Contrary to ArrayString::from, truncates at the closest unicode
    /// character boundary.
    /// ```
    /// assert_eq!(ArrayCString::<[_; 4]>::from("éà"),
    ///            ArrayCString::<[_; 4]>::from("é"));
    /// assert_eq!(&*ArrayCString::<[_; 4]>::from("éà"), "é\0");
    /// ```
    fn from(s: S) -> Self {
        let s = s.as_ref();
        let len = cmp::min(s.len(), A::capacity() - 1);
        let mut result = Self {
            inner: ArrayString::from(str_truncate_valid(s, len)).unwrap(),
        };
        result.inner.push('\0');
        result
    }
}

impl<A: Array<Item = u8>> Deref for ArrayCString<A> {
    type Target = str;

    fn deref(&self) -> &str {
        self.inner.as_str()
    }
}

fn panic_hook(info: &panic::PanicInfo) {
    // Try to handle &str/String payloads, which should handle 99% of cases.
    let payload = info.payload();
    let message = if let Some(s) = payload.downcast_ref::<&str>() {
        s
    } else if let Some(s) = payload.downcast_ref::<String>() {
        s.as_str()
    } else {
        // Not the most helpful thing, but seems unlikely to happen
        // in practice.
        "Unhandled rust panic payload!"
    };
    let (filename, line) = if let Some(loc) = info.location() {
        (loc.file(), loc.line())
    } else {
        ("unknown.rs", 0)
    };
    // Copy the message and filename to the stack in order to safely add
    // a terminating nul character (since rust strings don't come with one
    // and GeckoCrashOOL wants one).
    let message = ArrayCString::<[_; 512]>::from(message);
    let filename = ArrayCString::<[_; 512]>::from(filename);
    unsafe {
        GeckoCrashOOL(filename.as_ptr() as *const c_char, line as c_int,
                      message.as_ptr() as *const c_char);
    }
}

/// Configure a panic hook to redirect rust panics to Gecko's MOZ_CrashOOL.
#[no_mangle]
pub extern "C" fn install_rust_panic_hook() {
    panic::set_hook(Box::new(panic_hook));
}

// Wrap the rust system allocator to override the OOM handler, redirecting
// to Gecko's, which interacts with the crash reporter.
// This relies on unstable APIs that have not changed between 1.24 and 1.27.
// In 1.27, the API changed, so we'll need to adapt to those changes before
// we can ship with 1.27. As of writing, there might still be further changes
// to those APIs before 1.27 is released, so we wait for those.
#[cfg(feature = "oom_with_global_alloc")]
mod global_alloc {
    extern crate alloc;
    extern crate alloc_system;

    use self::alloc::allocator::{Alloc, AllocErr, Layout};
    use self::alloc_system::System;

    pub struct GeckoHeap;

    extern "C" {
        fn GeckoHandleOOM(size: usize) -> !;
    }

    unsafe impl<'a> Alloc for &'a GeckoHeap {
        unsafe fn alloc(&mut self, layout: Layout) -> Result<*mut u8, AllocErr> {
            System.alloc(layout)
        }

        unsafe fn dealloc(&mut self, ptr: *mut u8, layout: Layout) {
            System.dealloc(ptr, layout)
        }

        fn oom(&mut self, e: AllocErr) -> ! {
            match e {
                AllocErr::Exhausted { request } => unsafe { GeckoHandleOOM(request.size()) },
                _ => System.oom(e),
            }
        }

        unsafe fn realloc(
            &mut self,
            ptr: *mut u8,
            layout: Layout,
            new_layout: Layout,
        ) -> Result<*mut u8, AllocErr> {
            System.realloc(ptr, layout, new_layout)
        }

        unsafe fn alloc_zeroed(&mut self, layout: Layout) -> Result<*mut u8, AllocErr> {
            System.alloc_zeroed(layout)
        }
    }

}

#[cfg(feature = "oom_with_global_alloc")]
#[global_allocator]
static HEAP: global_alloc::GeckoHeap = global_alloc::GeckoHeap;

#[cfg(feature = "oom_with_hook")]
mod oom_hook {
    use std::alloc::{Layout, set_alloc_error_hook};

    extern "C" {
        fn GeckoHandleOOM(size: usize) -> !;
    }

    pub fn hook(layout: Layout) {
        unsafe {
            GeckoHandleOOM(layout.size());
        }
    }

    pub fn install() {
        set_alloc_error_hook(hook);
    }
}

#[no_mangle]
pub extern "C" fn install_rust_oom_hook() {
    #[cfg(feature = "oom_with_hook")]
    oom_hook::install();
}
