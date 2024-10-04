/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#[macro_use]
extern crate cstr;
#[macro_use]
extern crate gecko_profiler;
#[macro_use]
extern crate log;
#[macro_use]
extern crate style;

mod error_reporter;
#[allow(non_snake_case)]
pub mod glue;
mod stylesheet_loader;

// FIXME(bholley): This should probably go away once we harmonize the allocators.
#[no_mangle]
pub extern "C" fn je_malloc_usable_size(_: *const ::libc::c_void) -> ::libc::size_t {
    0
}
