/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/// External types test lib
///
/// This imports types from the main test fixture crate, then defines functions that input/return
/// them.
use std::sync::Arc;
use uniffi_bindings_tests::{
    custom_types::Handle, enums::EnumWithData, interfaces::TestInterface, records::SimpleRec,
};

uniffi::setup_scaffolding!("uniffi_bindings_tests_external_types");

#[uniffi::export]
pub fn roundtrip_ext_record(rec: SimpleRec) -> SimpleRec {
    rec
}

#[uniffi::export]
pub fn roundtrip_ext_enum(en: EnumWithData) -> EnumWithData {
    en
}

#[uniffi::export]
pub fn roundtrip_ext_interface(int: Arc<TestInterface>) -> Arc<TestInterface> {
    int
}

#[uniffi::export]
pub fn roundtrip_ext_custom_type(custom: Handle) -> Handle {
    custom
}
