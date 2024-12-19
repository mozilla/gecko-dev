// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::{borrow::Cow, cmp::min};

use crate::STREAM_IO_BUFFER_SIZE;

#[derive(Debug)]
pub struct SendData {
    data: Cow<'static, [u8]>,
    offset: usize,
    remaining: usize,
    total: usize,
}

impl From<&[u8]> for SendData {
    fn from(data: &[u8]) -> Self {
        Self::from(data.to_vec())
    }
}

impl From<Vec<u8>> for SendData {
    fn from(data: Vec<u8>) -> Self {
        let remaining = data.len();
        Self {
            total: data.len(),
            data: Cow::Owned(data),
            offset: 0,
            remaining,
        }
    }
}

impl From<&str> for SendData {
    fn from(data: &str) -> Self {
        Self::from(data.as_bytes())
    }
}

impl SendData {
    pub const fn zeroes(total: usize) -> Self {
        const MESSAGE: &[u8] = &[0; STREAM_IO_BUFFER_SIZE];
        Self {
            data: Cow::Borrowed(MESSAGE),
            offset: 0,
            remaining: total,
            total,
        }
    }

    fn slice(&self) -> &[u8] {
        let end = min(self.data.len(), self.offset + self.remaining);
        &self.data[self.offset..end]
    }

    pub fn send(&mut self, mut f: impl FnMut(&[u8]) -> usize) -> bool {
        while self.remaining > 0 {
            match f(self.slice()) {
                0 => {
                    return false;
                }
                sent => {
                    self.remaining -= sent;
                    self.offset = (self.offset + sent) % self.data.len();
                }
            }
        }

        self.remaining == 0
    }

    pub const fn len(&self) -> usize {
        self.total
    }
}
