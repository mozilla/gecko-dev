// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![allow(non_camel_case_types)]

use crate::err::IntoResult;
use crate::hash;
use crate::hash::HashAlgorithm;
use crate::p11;
use crate::p11::PK11Origin;
use crate::p11::PK11_CreateContextBySymKey;
use crate::p11::PK11_DigestFinal;
use crate::p11::PK11_DigestOp;
use crate::p11::PK11_ImportSymKey;
use crate::p11::Slot;
use crate::Error;
use crate::SECItemBorrowed;
use pkcs11_bindings::CKA_SIGN;
use std::convert::TryFrom;
use std::ptr;

//
// Constants
//

pub enum HmacAlgorithm {
    HMAC_SHA2_256,
    HMAC_SHA2_384,
    HMAC_SHA2_512,
}

fn hmac_alg_to_ckm(alg: &HmacAlgorithm) -> p11::CK_MECHANISM_TYPE {
    match alg {
        HmacAlgorithm::HMAC_SHA2_256 => p11::CKM_SHA256_HMAC.into(),
        HmacAlgorithm::HMAC_SHA2_384 => p11::CKM_SHA384_HMAC.into(),
        HmacAlgorithm::HMAC_SHA2_512 => p11::CKM_SHA512_HMAC.into(),
    }
}

pub fn hmac_alg_to_hash_alg(alg: &HmacAlgorithm) -> Result<HashAlgorithm, Error> {
    match alg {
        HmacAlgorithm::HMAC_SHA2_256 => Ok(HashAlgorithm::SHA2_256),
        HmacAlgorithm::HMAC_SHA2_384 => Ok(HashAlgorithm::SHA2_384),
        HmacAlgorithm::HMAC_SHA2_512 => Ok(HashAlgorithm::SHA2_512),
    }
}

pub fn hmac_alg_to_hmac_len(alg: &HmacAlgorithm) -> Result<usize, Error> {
    let hash_alg = hmac_alg_to_hash_alg(&alg)?;
    hash::hash_alg_to_hash_len(&hash_alg)
}

pub fn hmac(alg: &HmacAlgorithm, key: &[u8], data: &[u8]) -> Result<Vec<u8>, Error> {
    crate::init();

    let data_len = match u32::try_from(data.len()) {
        Ok(data_len) => data_len,
        _ => return Err(Error::InternalError),
    };

    let slot = Slot::internal()?;
    let sym_key = unsafe {
        PK11_ImportSymKey(
            *slot,
            hmac_alg_to_ckm(&alg),
            PK11Origin::PK11_OriginUnwrap,
            CKA_SIGN,
            SECItemBorrowed::wrap(key).as_mut(),
            ptr::null_mut(),
        )
        .into_result()?
    };
    let param = SECItemBorrowed::make_empty();
    let context = unsafe {
        PK11_CreateContextBySymKey(hmac_alg_to_ckm(&alg), CKA_SIGN, *sym_key, param.as_ref())
            .into_result()?
    };
    unsafe { PK11_DigestOp(*context, data.as_ptr(), data_len).into_result()? };
    let expected_len = hmac_alg_to_hmac_len(alg)?;
    let mut digest = vec![0u8; expected_len];
    let mut digest_len = 0u32;
    unsafe {
        PK11_DigestFinal(
            *context,
            digest.as_mut_ptr(),
            &mut digest_len,
            digest.len() as u32,
        )
        .into_result()?
    }
    assert_eq!(digest_len as usize, expected_len);
    Ok(digest)
}
