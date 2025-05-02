// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

//! The public FOG APIs, for Rust consumers.

// Re-exporting for later use in generated code.
pub extern crate chrono;
pub extern crate once_cell;
pub extern crate uuid;

// Re-exporting for use in user tests.
pub use private::{DistributionData, ErrorType, RecordedEvent};

// Must appear before `metrics` or its use of `ffi`'s macros will fail.
#[macro_use]
mod ffi;

pub mod factory;
pub mod metrics;
pub mod pings;
pub mod private;

pub mod ipc;

#[cfg(test)]
mod common_test;

#[cfg(feature = "with_gecko")]
use nserror::{NS_ERROR_UNEXPECTED, NS_OK};
#[cfg(feature = "with_gecko")]
use nsstring::{nsACString, nsCStr};
#[cfg(feature = "with_gecko")]
use xpcom::interfaces::{nsIHandleReportCallback, nsIMemoryReporter, nsISupports};
#[cfg(feature = "with_gecko")]
use xpcom::RefPtr;

/// Collect a memory report for heap memory in bytes.
#[cfg(feature = "with_gecko")]
macro_rules! moz_collect_report {
    ($cb:ident, $path:expr, $amount:expr, $desc:expr, $data:expr) => {
        $cb.Callback(
            &nsCStr::new() as &nsACString,
            &nsCStr::from($path) as &nsACString,
            nsIMemoryReporter::KIND_HEAP,
            nsIMemoryReporter::UNITS_BYTES,
            $amount as i64,
            &nsCStr::from($desc) as &nsACString,
            $data,
        )
    };
}

#[cfg(feature = "with_gecko")]
#[no_mangle]
unsafe extern "C" fn fog_collect_reports(
    callback: *const nsIHandleReportCallback,
    data: *const nsISupports,
    _anonymize: bool,
) -> nserror::nsresult {
    let callback = match RefPtr::from_raw(callback) {
        Some(ptr) => ptr,
        None => return NS_ERROR_UNEXPECTED,
    };

    extern "C" {
        fn fog_malloc_size_of(ptr: *const xpcom::reexports::libc::c_void) -> usize;
        fn fog_malloc_enclosing_size_of(ptr: *const xpcom::reexports::libc::c_void) -> usize;
    }
    let mut ops = malloc_size_of::MallocSizeOfOps::new(
        fog_malloc_size_of,
        Some(fog_malloc_enclosing_size_of),
    );

    let map_size = metrics::__glean_metric_maps::fog_map_alloc_size(&mut ops);
    let metric_size = metrics::metric_memory_usage();
    let total_metric_size = map_size + metric_size;
    let ping_size = pings::fog_ping_alloc_size(&mut ops);

    moz_collect_report!(
        callback,
        "explicit/fog/metrics",
        total_metric_size,
        "Memory used by all FOG metrics",
        data
    );
    moz_collect_report!(
        callback,
        "explicit/fog/pings",
        ping_size,
        "Memory used by all FOG pings",
        data
    );

    NS_OK
}
