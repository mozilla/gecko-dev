/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

uniffi::setup_scaffolding!("uniffi_bindings_tests_collision");

// Same name as in uniffi_bindings_tests!
#[uniffi::export(callback_interface)]
pub trait TestCallbackInterface {
    fn get_value(&self) -> String;
}

#[uniffi::export]
pub fn invoke_collision_callback(cb: Box<dyn TestCallbackInterface>) -> String {
    cb.get_value()
}
