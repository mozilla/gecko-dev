/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use url::Url as ExtUrl;

pub struct Url(ExtUrl);
pub struct Handle(u64);
pub struct TimeIntervalMs(i64);
pub struct TimeIntervalSecDbl(f64);
pub struct TimeIntervalSecFlt(f32);

uniffi::custom_type!(Handle, u64, {
    try_lift: |val| Ok(Handle(val)),
    lower: |handle| handle.0,
});
uniffi::custom_newtype!(TimeIntervalMs, i64);
uniffi::custom_newtype!(TimeIntervalSecDbl, f64);
uniffi::custom_newtype!(TimeIntervalSecFlt, f32);
uniffi::custom_type!(Url, String, {
    try_lift: |s| Ok(Url(ExtUrl::parse(&s)?)),
    lower:    |u| u.0.to_string(),
});

#[uniffi::export]
pub fn roundtrip_custom_type(handle: Handle) -> Handle {
    handle
}

#[uniffi::export]
pub fn roundtrip_url(url: Url) -> Url {
    url
}

#[uniffi::export]
pub fn roundtrip_time_interval_ms(time: TimeIntervalMs) -> TimeIntervalMs {
    time
}

#[uniffi::export]
pub fn roundtrip_time_interval_sec_dbl(time: TimeIntervalSecDbl) -> TimeIntervalSecDbl {
    time
}

#[derive(uniffi::Record)]
pub struct CustomTypesDemo {
    pub url: Url,
    pub handle: Handle,
    pub time_interval_ms: TimeIntervalMs,
    pub time_interval_sec_dbl: TimeIntervalSecDbl,
    pub time_interval_sec_flt: TimeIntervalSecFlt,
}

#[uniffi::export]
pub fn roundtrip_time_interval_sec_flt(time: TimeIntervalSecFlt) -> TimeIntervalSecFlt {
    time
}

#[uniffi::export]
pub fn get_custom_types_demo() -> CustomTypesDemo {
    CustomTypesDemo {
        url: Url(ExtUrl::parse("https://example.com/").unwrap()),
        handle: Handle(123),
        time_interval_ms: TimeIntervalMs(456000),
        time_interval_sec_dbl: TimeIntervalSecDbl(456.0),
        time_interval_sec_flt: TimeIntervalSecFlt(789.0),
    }
}
