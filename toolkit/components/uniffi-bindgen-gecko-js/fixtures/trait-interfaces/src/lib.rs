/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::sync::Arc;

#[uniffi::export]
pub trait Calc: Send + Sync {
    fn add(&self, a: u32, b: u32) -> u32;
}

#[uniffi::export]
pub fn make_calculator() -> Arc<dyn Calc> {
    Arc::new(Calculator)
}

struct Calculator;

impl Calc for Calculator {
    fn add(&self, a: u32, b: u32) -> u32 {
        a + b
    }
}

#[uniffi::export]
pub fn make_buggy_calculator() -> Arc<dyn Calc> {
    Arc::new(BuggyCalculator)
}

struct BuggyCalculator;

impl Calc for BuggyCalculator {
    fn add(&self, a: u32, b: u32) -> u32 {
        a + b + 1
    }
}

uniffi::setup_scaffolding!();
