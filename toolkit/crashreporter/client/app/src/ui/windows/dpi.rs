/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! DPI management utilities.

use windows_sys::Win32::{
    Foundation::HWND,
    UI::{HiDpi::GetDpiForWindow, WindowsAndMessaging::USER_DEFAULT_SCREEN_DPI as DEFAULT_DPI},
};

// To simplify layout code (avoiding passing values through many functions), we provide an API
// which can set a contextual Dpi.
thread_local! {
    static CONTEXT_DPI: std::cell::Cell<Dpi> = Dpi::default().into();
}

/// A DPI value.
#[derive(Clone, Copy, Debug)]
pub struct Dpi(u32);

impl Default for Dpi {
    fn default() -> Self {
        Dpi(DEFAULT_DPI)
    }
}

impl Dpi {
    /// Create a new Dpi.
    pub fn new(dpi: u32) -> Self {
        Dpi(dpi)
    }

    /// Get the Dpi for the given window.
    pub fn for_window(hwnd: HWND) -> Self {
        Dpi(unsafe { GetDpiForWindow(hwnd) })
    }

    /// Scale a pixel value according to this Dpi.
    pub fn scale(&self, value: u32) -> u32 {
        if self.0 == DEFAULT_DPI {
            value
        } else {
            (value as u64 * self.0 as u64 / DEFAULT_DPI as u64) as u32
        }
    }

    /// Call the given capture with this Dpi set as the contextual Dpi.
    pub fn with_context<F, R>(&self, f: F) -> R
    where
        F: FnOnce() -> R,
    {
        let old = CONTEXT_DPI.replace(*self);
        let ret = f();
        CONTEXT_DPI.set(old);
        ret
    }

    /// Scale a pixel value according to the contextual Dpi.
    pub fn context_scale(value: u32) -> u32 {
        CONTEXT_DPI.get().scale(value)
    }
}
