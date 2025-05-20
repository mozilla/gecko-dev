/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

pub struct Handle(u64);

uniffi::custom_type!(Handle, u64, {
    try_lift: |val| Ok(Handle(val)),
    lower: |handle| handle.0,
});

#[uniffi::export]
pub fn roundtrip_custom_type(handle: Handle) -> Handle {
    handle
}
