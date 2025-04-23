/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use url::Url;

// Custom Handle type which trivially wraps an i64.
pub struct Handle(pub i64);

// We must implement the UniffiCustomTypeConverter trait for each custom type on the scaffolding side
uniffi::custom_type!(Handle, i64, {
    try_lift: |val| Ok(Handle(val)),
    lower: |obj| obj.0,
});

// Use `url::Url` as a custom type, with `String` as the Builtin
uniffi::custom_type!(Url, String, {
    remote,
    try_lift: |val| Ok(Url::parse(&val)?),
    lower: |obj| obj.to_string(),
});

// And a little struct and function that ties them together.
pub struct CustomTypesDemo {
    url: Url,
    handle: Handle,
}

pub fn get_custom_types_demo(v: Option<CustomTypesDemo>) -> CustomTypesDemo {
    v.unwrap_or_else(|| CustomTypesDemo {
        url: Url::parse("http://example.com/").unwrap(),
        handle: Handle(123),
    })
}

include!(concat!(env!("OUT_DIR"), "/custom-types.uniffi.rs"));
