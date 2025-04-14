// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![expect(clippy::missing_errors_doc, reason = "OK here.")]

use std::fmt;

use crate::{
    constants::{Cipher, Version},
    err::{sec::SEC_ERROR_BAD_DATA, Error, Res},
    p11::SymKey,
};

pub const AEAD_NULL_TAG: &[u8] = &[0x0a; 16];

pub struct AeadNull {}

impl AeadNull {
    pub const fn new(
        _version: Version,
        _cipher: Cipher,
        _secret: &SymKey,
        _prefix: &str,
    ) -> Res<Self> {
        Ok(Self {})
    }

    #[must_use]
    pub const fn expansion(&self) -> usize {
        AEAD_NULL_TAG.len()
    }

    pub fn encrypt<'a>(
        &self,
        _count: u64,
        _aad: &[u8],
        input: &[u8],
        output: &'a mut [u8],
    ) -> Res<&'a [u8]> {
        let l = input.len();
        output[..l].copy_from_slice(input);
        output[l..l + self.expansion()].copy_from_slice(AEAD_NULL_TAG);
        Ok(&output[..l + self.expansion()])
    }

    pub fn encrypt_in_place<'a>(
        &self,
        _count: u64,
        _aad: &[u8],
        data: &'a mut [u8],
    ) -> Res<&'a mut [u8]> {
        let pos = data.len() - self.expansion();
        data[pos..].copy_from_slice(AEAD_NULL_TAG);
        Ok(data)
    }

    fn decrypt_check(&self, _count: u64, _aad: &[u8], input: &[u8]) -> Res<usize> {
        if input.len() < self.expansion() {
            return Err(Error::from(SEC_ERROR_BAD_DATA));
        }

        let len_encrypted = input
            .len()
            .checked_sub(self.expansion())
            .ok_or_else(|| Error::from(SEC_ERROR_BAD_DATA))?;
        // Check that:
        // 1) expansion is all zeros and
        // 2) if the encrypted data is also supplied that at least some values are no zero
        //    (otherwise padding will be interpreted as a valid packet)
        if &input[len_encrypted..] == AEAD_NULL_TAG
            && (len_encrypted == 0 || input[..len_encrypted].iter().any(|x| *x != 0x0))
        {
            Ok(len_encrypted)
        } else {
            Err(Error::from(SEC_ERROR_BAD_DATA))
        }
    }

    pub fn decrypt<'a>(
        &self,
        count: u64,
        aad: &[u8],
        input: &[u8],
        output: &'a mut [u8],
    ) -> Res<&'a [u8]> {
        self.decrypt_check(count, aad, input).map(|len| {
            output[..len].copy_from_slice(&input[..len]);
            &output[..len]
        })
    }

    pub fn decrypt_in_place<'a>(
        &self,
        count: u64,
        aad: &[u8],
        data: &'a mut [u8],
    ) -> Res<&'a mut [u8]> {
        self.decrypt_check(count, aad, data)
            .map(move |len| &mut data[..len])
    }
}

impl fmt::Debug for AeadNull {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "[NULL AEAD]")
    }
}
