/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! **Golden Gate** 🌉 was created to bridge Desktop Sync to our suite of
//! Rust sync and storage components. But the UniFFI-cation of our
//! components made much of Golden Gate's logic obsolete. It is now mainly
//! a means to access LogSink, the logger for our components.

#[macro_use]
extern crate cstr;

pub mod error;
pub mod log;

pub use crate::log::LogSink;
pub use error::{Error, Result};
