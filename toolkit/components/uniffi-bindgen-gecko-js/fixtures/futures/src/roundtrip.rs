/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! Some barely async functions that round-trip data.
//!
//! The purpose of these is to test the lift/lower implementations in the async code.

use std::{collections::HashMap, sync::Arc};


#[uniffi::export]
async fn roundtrip_u8(v: u8) -> u8 {
    v
}

#[uniffi::export]
async fn roundtrip_i8(v: i8) -> i8 {
    v
}

#[uniffi::export]
async fn roundtrip_u16(v: u16) -> u16 {
    v
}

#[uniffi::export]
async fn roundtrip_i16(v: i16) -> i16 {
    v
}

#[uniffi::export]
async fn roundtrip_u32(v: u32) -> u32 {
    v
}

#[uniffi::export]
async fn roundtrip_i32(v: i32) -> i32 {
    v
}

#[uniffi::export]
async fn roundtrip_u64(v: u64) -> u64 {
    v
}

#[uniffi::export]
async fn roundtrip_i64(v: i64) -> i64 {
    v
}

#[uniffi::export]
async fn roundtrip_f32(v: f32) -> f32 {
    v
}

#[uniffi::export]
async fn roundtrip_f64(v: f64) -> f64 {
    v
}

#[uniffi::export]
async fn roundtrip_string(v: String) -> String {
    v
}

#[uniffi::export]
async fn roundtrip_vec(v: Vec<u32>) -> Vec<u32> {
    v
}

#[uniffi::export]
async fn roundtrip_map(v: HashMap<String, String>) -> HashMap<String, String> {
    v
}

#[derive(uniffi::Object)]
struct Traveller {
    name: String,
}

#[uniffi::export]
impl Traveller {
    #[uniffi::constructor]
    pub fn new(name: String) -> Arc<Self> {
        Arc::new(Traveller { name })
    }

    pub fn name(&self) -> String {
        self.name.clone()
    }
}

#[uniffi::export]
async fn roundtrip_obj(v: Arc<Traveller>) -> Arc<Traveller> {
    v
}
