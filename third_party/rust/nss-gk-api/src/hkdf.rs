// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![allow(non_camel_case_types)]

use crate::SECItem;
use crate::SECItemType;

use crate::p11;
use crate::p11::SymKey;
use crate::p11::CKA_DERIVE;
use crate::p11::CKF_HKDF_SALT_DATA;
use crate::p11::CKF_HKDF_SALT_NULL;
use crate::p11::CKM_HKDF_DATA;
use crate::p11::CKM_HKDF_DERIVE;
use crate::p11::CK_BBOOL;
use crate::p11::CK_INVALID_HANDLE;
use crate::p11::CK_MECHANISM_TYPE;
use crate::ssl::CK_OBJECT_HANDLE;

use crate::SECItemBorrowed;

use pkcs11_bindings::CKA_SIGN;
use pkcs11_bindings::CKM_HKDF_KEY_GEN;
use pkcs11_bindings::CK_ULONG;

use std::convert::TryFrom;
use std::marker::PhantomData;
use std::mem;
use std::os::raw::c_int;
use std::os::raw::c_uint;
use std::ptr::null_mut;

#[derive(Clone, Copy, Debug)]

pub enum HkdfError {
    InvalidPrkLength,
    InvalidLength,
    InternalError,
}

#[derive(Clone, Copy, Debug)]
pub enum HkdfAlgorithm {
    HKDF_SHA2_256,
    HKDF_SHA2_384,
    HKDF_SHA2_512,
}

#[derive(Clone, Copy, Debug)]
pub enum KeyMechanism {
    Hkdf,
}

impl KeyMechanism {
    fn mech(self) -> CK_MECHANISM_TYPE {
        CK_MECHANISM_TYPE::from(match self {
            Self::Hkdf => CKM_HKDF_DERIVE,
        })
    }

    fn len(self) -> usize {
        match self {
            Self::Hkdf => 0, // Let the underlying module decide.
        }
    }
}
#[derive(Clone, Copy, Debug)]

pub(crate) struct ParamItem<'a, T: 'a> {
    item: SECItem,
    marker: PhantomData<&'a T>,
}

impl<'a, T: Sized + 'a> ParamItem<'a, T> {
    pub fn new(v: &'a mut T) -> Self {
        let item = SECItem {
            type_: SECItemType::siBuffer,
            data: (v as *mut T).cast::<u8>(),
            len: c_uint::try_from(mem::size_of::<T>()).unwrap(),
        };
        Self {
            item,
            marker: PhantomData::default(),
        }
    }

    pub fn ptr(&mut self) -> *mut SECItem {
        std::ptr::addr_of_mut!(self.item)
    }
}

pub struct Hkdf {
    kdf: HkdfAlgorithm,
}

impl Hkdf {
    pub fn new(kdf: HkdfAlgorithm) -> Self {
        Self { kdf }
    }

    pub fn import_secret(&self, ikm: &[u8]) -> Result<SymKey, HkdfError> {
        crate::init();

        let slot = p11::Slot::internal().map_err(|_| HkdfError::InternalError)?;
        let ikm_item = SECItemBorrowed::wrap(ikm);
        let ikm_item_ptr = ikm_item.as_ref() as *const _ as *mut _;

        let ptr = unsafe {
            p11::PK11_ImportSymKey(
                *slot,
                CK_MECHANISM_TYPE::from(CKM_HKDF_KEY_GEN),
                p11::PK11Origin::PK11_OriginUnwrap,
                p11::CK_ATTRIBUTE_TYPE::from(CKA_SIGN),
                ikm_item_ptr,
                null_mut(),
            )
        };
        let s = unsafe { SymKey::from_ptr(ptr).expect("HkdfError::InternalError") };
        Ok(s)
    }

    fn mech(&self) -> CK_MECHANISM_TYPE {
        CK_MECHANISM_TYPE::from(match self.kdf {
            HkdfAlgorithm::HKDF_SHA2_256 => p11::CKM_SHA256,
            HkdfAlgorithm::HKDF_SHA2_384 => p11::CKM_SHA384,
            HkdfAlgorithm::HKDF_SHA2_512 => p11::CKM_SHA512,
        })
    }

    pub fn extract(&self, salt: &[u8], ikm: &SymKey) -> Result<SymKey, HkdfError> {
        crate::init();

        let salt_type = if salt.is_empty() {
            CKF_HKDF_SALT_NULL
        } else {
            CKF_HKDF_SALT_DATA
        };
        let mut params = p11::CK_HKDF_PARAMS {
            bExtract: CK_BBOOL::from(true),
            bExpand: CK_BBOOL::from(false),
            prfHashMechanism: self.mech(),
            ulSaltType: CK_ULONG::from(salt_type),
            pSalt: salt.as_ptr() as *mut _, // const-cast = bad API
            ulSaltLen: CK_ULONG::try_from(salt.len()).unwrap(),
            hSaltKey: CK_OBJECT_HANDLE::from(CK_INVALID_HANDLE),
            pInfo: null_mut(),
            ulInfoLen: 0,
        };
        let mut params_item = ParamItem::new(&mut params);
        let ptr = unsafe {
            p11::PK11_Derive(
                **ikm,
                CK_MECHANISM_TYPE::from(CKM_HKDF_DERIVE),
                params_item.ptr(),
                CK_MECHANISM_TYPE::from(CKM_HKDF_DERIVE),
                CK_MECHANISM_TYPE::from(CKA_DERIVE),
                0,
            )
        };

        let prk = unsafe { SymKey::from_ptr(ptr) }.expect("HkdfError::InternalError");
        Ok(prk)
    }

    // NB: `info` must outlive the returned value.
    fn expand_params(&self, info: &[u8]) -> p11::CK_HKDF_PARAMS {
        p11::CK_HKDF_PARAMS {
            bExtract: CK_BBOOL::from(false),
            bExpand: CK_BBOOL::from(true),
            prfHashMechanism: self.mech(),
            ulSaltType: CK_ULONG::from(CKF_HKDF_SALT_NULL),
            pSalt: null_mut(),
            ulSaltLen: 0,
            hSaltKey: CK_OBJECT_HANDLE::from(CK_INVALID_HANDLE),
            pInfo: info.as_ptr() as *mut _, // const-cast = bad API
            ulInfoLen: CK_ULONG::try_from(info.len()).unwrap(),
        }
    }

    pub fn expand_key(
        &self,
        prk: &SymKey,
        info: &[u8],
        key_mech: KeyMechanism,
    ) -> Result<SymKey, HkdfError> {
        crate::init();

        let mut params = self.expand_params(info);
        let mut params_item = ParamItem::new(&mut params);
        let ptr = unsafe {
            p11::PK11_Derive(
                **prk,
                CK_MECHANISM_TYPE::from(CKM_HKDF_DERIVE),
                params_item.ptr(),
                key_mech.mech(),
                CK_MECHANISM_TYPE::from(CKA_DERIVE),
                c_int::try_from(key_mech.len()).unwrap(),
            )
        };
        let okm = unsafe { SymKey::from_ptr(ptr) }.expect("HkdfError::InternalError");
        Ok(okm)
    }

    pub fn expand_data(&self, prk: &SymKey, info: &[u8], len: usize) -> Result<Vec<u8>, HkdfError> {
        crate::init();

        let mut params = self.expand_params(info);
        let mut params_item = ParamItem::new(&mut params);
        let ptr = unsafe {
            p11::PK11_Derive(
                **prk,
                CK_MECHANISM_TYPE::from(CKM_HKDF_DATA),
                params_item.ptr(),
                CK_MECHANISM_TYPE::from(CKM_HKDF_DERIVE),
                CK_MECHANISM_TYPE::from(CKA_DERIVE),
                c_int::try_from(len).unwrap(),
            )
        };
        let k = unsafe { SymKey::from_ptr(ptr) }.expect("HkdfError::InternalError");
        let r = Vec::from(k.key_data().expect("HkdfError::InternalError"));
        Ok(r)
    }
}
