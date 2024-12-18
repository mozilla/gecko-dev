// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use crate::Error;

use alloc::vec::Vec;

pub trait Writer {
    fn write(&mut self, bytes: &[u8]) -> Result<(), Error>;
}

impl<T: Writer + ?Sized> Writer for &mut T {
    #[inline]
    fn write(&mut self, bytes: &[u8]) -> Result<(), Error> {
        (**self).write(bytes)
    }
}

impl Writer for &mut [u8] {
    fn write(&mut self, bytes: &[u8]) -> Result<(), Error> {
        if bytes.len() > self.len() {
            return Err(Error::UnexpectedEOF);
        }

        let (a, b) = core::mem::take(self).split_at_mut(bytes.len());
        a.copy_from_slice(bytes);
        *self = b;

        Ok(())
    }
}

impl Writer for Vec<u8> {
    #[inline]
    fn write(&mut self, bytes: &[u8]) -> Result<(), Error> {
        self.extend_from_slice(bytes);
        Ok(())
    }
}
