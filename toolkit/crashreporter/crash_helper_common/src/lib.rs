/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::ffi::OsString;

pub mod errors;
pub mod messages;

mod breakpad;
mod ipc_connector;
mod ipc_listener;
mod ipc_poller;
mod platform;

use errors::MessageError;

// Re-export the platform-specific types and functions
pub use crate::breakpad::{AncillaryData, BreakpadChar, BreakpadData, BreakpadRawData, Pid};
pub use crate::ipc_connector::{IPCConnector, IPCEvent};
pub use crate::ipc_listener::IPCListener;
pub use crate::ipc_poller::wait_for_events;

// OsString extensions to convert from/to C strings

pub trait BreakpadString {
    fn serialize(&self) -> Vec<u8>;
    fn deserialize(bytes: &[u8]) -> Result<OsString, MessageError>;
    fn from_ptr(ptr: *const BreakpadChar) -> OsString;
    unsafe fn into_raw(self) -> *mut BreakpadChar;
    unsafe fn from_raw(ptr: *mut BreakpadChar) -> OsString;
}

pub const IO_TIMEOUT: u16 = 60 * 1000;
