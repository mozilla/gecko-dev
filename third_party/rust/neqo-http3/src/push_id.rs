// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::ops::{Add, Sub};

#[derive(Clone, Copy, Debug, Eq, PartialEq, PartialOrd)]
pub struct PushId(u64);

impl PushId {
    #[must_use]
    pub const fn new(id: u64) -> Self {
        Self(id)
    }

    pub fn next(&mut self) {
        self.0 += 1;
    }
}

impl From<u64> for PushId {
    fn from(id: u64) -> Self {
        Self(id)
    }
}

impl From<PushId> for u64 {
    fn from(id: PushId) -> Self {
        id.0
    }
}

impl ::std::fmt::Display for PushId {
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        write!(f, "{}", self.0)
    }
}

impl Sub for PushId {
    type Output = Self;

    fn sub(self, rhs: Self) -> Self {
        Self(self.0 - rhs.0)
    }
}

impl Add<u64> for PushId {
    type Output = Self;

    fn add(self, rhs: u64) -> Self {
        Self(self.0 + rhs)
    }
}
