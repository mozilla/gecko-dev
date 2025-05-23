/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#[uniffi::export(callback_interface)]
pub trait TestCallbackInterface {
    fn get_value(&self) -> u32;
}

#[uniffi::export]
fn invoke_test_callback_interface_method(cbi: Box<dyn TestCallbackInterface>) -> u32 {
    cbi.get_value()
}
