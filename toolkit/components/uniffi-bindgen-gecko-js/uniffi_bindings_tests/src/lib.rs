/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

uniffi::setup_scaffolding!("uniffi_bindings_tests");

#[cfg(feature = "callback_interfaces")]
pub mod callback_interfaces;

#[cfg(feature = "complex_fns")]
pub mod complex_fns;

#[cfg(feature = "compound_types")]
pub mod compound_types;

#[cfg(feature = "custom_types")]
pub mod custom_types;

#[cfg(feature = "enums")]
pub mod enums;

#[cfg(feature = "errors")]
pub mod errors;

#[cfg(feature = "futures")]
pub mod futures;

#[cfg(feature = "interfaces")]
pub mod interfaces;

#[cfg(feature = "primitive_types")]
pub mod primitive_types;

#[cfg(feature = "records")]
pub mod records;

#[cfg(feature = "simple_fns")]
pub mod simple_fns;
