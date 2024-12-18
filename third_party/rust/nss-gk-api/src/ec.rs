// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use crate::{der, PR_FALSE};
use crate::err::{secstatus_to_res, Error, IntoResult};
use crate::init;

use crate::p11::PK11ObjectType::PK11_TypePrivKey;
use crate::p11::PK11_ExportDERPrivateKeyInfo;
use crate::p11::PK11_GenerateKeyPair;
use crate::p11::PK11_ImportDERPrivateKeyInfoAndReturnKey;
use crate::p11::PK11_PubDeriveWithKDF;
use crate::p11::PK11_ReadRawAttribute;
use crate::p11::PK11_ImportPublicKey;
use crate::p11::SECKEY_DecodeDERSubjectPublicKeyInfo;
use crate::p11::Slot;
use crate::p11::KU_ALL;

use crate::util::SECItemMut;

use crate::PrivateKey;
use crate::PublicKey;
use crate::SECItem;
use crate::SECItemBorrowed;

use pkcs11_bindings::CKA_VALUE;
use pkcs11_bindings::CKM_EC_EDWARDS_KEY_PAIR_GEN;
use pkcs11_bindings::CKM_EC_KEY_PAIR_GEN;
use pkcs11_bindings::CKM_EC_MONTGOMERY_KEY_PAIR_GEN;
use pkcs11_bindings::CK_FALSE;

use std::ptr;
//
// Constants
//

#[derive(Clone, Debug, Eq, PartialEq)]
pub enum EcCurve {
    P256,
    P384,
    P521,
    X25519,
    Ed25519,
}

pub type EcdhPublicKey = PublicKey;
pub type EcdhPrivateKey = PrivateKey;

pub struct EcdhKeypair {
    pub public: EcdhPublicKey,
    pub private: EcdhPrivateKey,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct Ecdh(EcCurve);

impl Ecdh {
    pub fn new(&self, curve: EcCurve) -> Self {
        Self(curve)
    }

    pub fn generate_keypair(&self, curve: EcCurve) -> Result<EcdhKeypair, crate::Error> {
        return ecdh_keygen(curve);
    }
}

// Object identifiers in DER tag-length-value form
pub const OID_EC_PUBLIC_KEY_BYTES: &[u8] = &[
    /* RFC 5480 (id-ecPublicKey) */
    0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02, 0x01,
];
pub const OID_SECP256R1_BYTES: &[u8] = &[
    /* RFC 5480 (secp256r1) */
    0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07,
];
pub const OID_SECP384R1_BYTES: &[u8] = &[
    /* RFC 5480 (secp384r1) */
    0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x34,
];
pub const OID_SECP521R1_BYTES: &[u8] = &[
    /* RFC 5480 (secp521r1) */
    0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x35,
];

pub const OID_ED25519_BYTES: &[u8] = &[/* RFC 8410 (id-ed25519) */ 0x2b, 0x65, 0x70];
pub const OID_RS256_BYTES: &[u8] = &[
    /* RFC 4055 (sha256WithRSAEncryption) */
    0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0b,
];

pub const OID_X25519_BYTES: &[u8] = &[
    /* https://tools.ietf.org/html/draft-josefsson-pkix-newcurves-01
     * 1.3.6.1.4.1.11591.15.1 */
    0x2b, 0x06, 0x01, 0x04, 0x01, 0xDA, 0x47, 0x0F, 0x01,
];

pub fn object_id(val: &[u8]) -> Result<Vec<u8>, Error> {
    let mut out = Vec::with_capacity(der::MAX_TAG_AND_LENGTH_BYTES + val.len());
    der::write_tag_and_length(&mut out, der::TAG_OBJECT_ID, val.len())?;
    out.extend_from_slice(val);
    Ok(out)
}

fn ec_curve_to_oid(alg: &EcCurve) -> Result<Vec<u8>, Error> {
    match alg {
        EcCurve::X25519 => Ok(OID_X25519_BYTES.to_vec()),
        EcCurve::Ed25519 => Ok(OID_ED25519_BYTES.to_vec()),
        EcCurve::P256 => Ok(OID_SECP256R1_BYTES.to_vec()),
        EcCurve::P384 => Ok(OID_SECP384R1_BYTES.to_vec()),
        EcCurve::P521 => Ok(OID_SECP521R1_BYTES.to_vec()),
    }
}

fn ec_curve_to_ckm(alg: &EcCurve) -> pkcs11_bindings::CK_MECHANISM_TYPE {
    match alg {
        EcCurve::P256 | EcCurve::P384 | EcCurve::P521 => CKM_EC_KEY_PAIR_GEN.into(),
        EcCurve::Ed25519 => CKM_EC_EDWARDS_KEY_PAIR_GEN.into(),
        EcCurve::X25519 => CKM_EC_MONTGOMERY_KEY_PAIR_GEN.into(),
    }
}

//
// Curve functions
//

pub fn ecdh_keygen(curve: EcCurve) -> Result<EcdhKeypair, crate::Error> {
    init();

    // Get the OID for the Curve
    let curve_oid = ec_curve_to_oid(&curve)?;
    let oid_bytes = object_id(&curve_oid)?;
    let mut oid = SECItemBorrowed::wrap(&oid_bytes);
    let oid_ptr: *mut SECItem = oid.as_mut();

    // Get the Mechanism based on the Curve and its use
    let ckm = ec_curve_to_ckm(&curve);

    // Get the PKCS11 slot
    let slot = Slot::internal()?;

    // Create a pointer for the public key
    let mut pk_ptr = ptr::null_mut();

    // https://github.com/mozilla/nss-gk-api/issues/1
    unsafe {
        let sk = PK11_GenerateKeyPair(
            *slot,
            ckm,
            oid_ptr.cast(),
            &mut pk_ptr,
            CK_FALSE.into(),
            CK_FALSE.into(),
            ptr::null_mut(),
        )
        .into_result()?;

        let pk = EcdhPublicKey::from_ptr(pk_ptr)?;

        let kp = EcdhKeypair {
            public: pk,
            private: sk,
        };

        Ok(kp)
    }
}

pub fn export_ec_private_key_pkcs8(key: PrivateKey) -> Result<Vec<u8>, Error> {
    init();
    unsafe {
        let sk: crate::ScopedSECItem = PK11_ExportDERPrivateKeyInfo(*key, ptr::null_mut())
            .into_result()
            .unwrap();
        return Ok(sk.into_vec());
    }
}

pub fn import_ec_public_key_from_spki(spki: &[u8]) -> Result<PublicKey, Error> {
    init();
    let mut spki_item = SECItemBorrowed::wrap(&spki);
    let spki_item_ptr = spki_item.as_mut();
    let slot = Slot::internal()?;
    unsafe {
        let spki = SECKEY_DecodeDERSubjectPublicKeyInfo(spki_item_ptr)
            .into_result()
            .unwrap();
        let pk: PublicKey = crate::p11::SECKEY_ExtractPublicKey(spki.as_mut().unwrap())
            .into_result()
            .unwrap();

        let handle = PK11_ImportPublicKey(*slot, *pk, PR_FALSE);
        if handle == pkcs11_bindings::CK_INVALID_HANDLE
        {
            return Err(Error::InvalidInput)
        }

        Ok(pk)
    }
}

pub fn import_ec_private_key_pkcs8(pki: &[u8]) -> Result<PrivateKey, Error> {
    init();

    // Get the PKCS11 slot
    let slot = Slot::internal()?;
    let mut der_pki = SECItemBorrowed::wrap(&pki);
    let der_pki_ptr: *mut SECItem = der_pki.as_mut();

    // Create a pointer for the private key
    let mut pk_ptr = ptr::null_mut();

    unsafe {
        secstatus_to_res(PK11_ImportDERPrivateKeyInfoAndReturnKey(
            *slot,
            der_pki_ptr,
            ptr::null_mut(),
            ptr::null_mut(),
            0,
            0,
            KU_ALL,
            &mut pk_ptr,
            ptr::null_mut(),
        ))
        .expect("PKCS8 encoded key import has failed");

        let sk = EcdhPrivateKey::from_ptr(pk_ptr)?;
        Ok(sk)
    }
}

pub fn export_ec_private_key_from_raw(key: PrivateKey) -> Result<Vec<u8>, Error> {
    init();
    let mut key_item = SECItemMut::make_empty();
    unsafe { PK11_ReadRawAttribute(PK11_TypePrivKey, key.cast(), CKA_VALUE, key_item.as_mut()) };
    Ok(key_item.as_slice().to_owned())
}

pub fn ecdh(sk: PrivateKey, pk: PublicKey) -> Result<Vec<u8>, Error> {
    init();
    let sym_key = unsafe {
        PK11_PubDeriveWithKDF(
            sk.cast(),
            pk.cast(),
            0,
            ptr::null_mut(),
            ptr::null_mut(),
            pkcs11_bindings::CKM_ECDH1_DERIVE,
            pkcs11_bindings::CKM_SHA512_HMAC,
            pkcs11_bindings::CKA_SIGN,
            0,
            pkcs11_bindings::CKD_NULL,
            ptr::null_mut(),
            ptr::null_mut(),
        )
        .into_result()?
    };

    let key = sym_key.key_data().unwrap();
    Ok(key.to_vec())
}

pub fn convert_to_public(sk: PrivateKey) -> Result<PublicKey, Error> {
    init();
    unsafe {
        let pk = crate::p11::SECKEY_ConvertToPublicKey(*sk).into_result()?;
        Ok(pk)
    }
}

pub fn sign(
    private_key: PrivateKey,
    data: &[u8],
    mechanism: std::os::raw::c_ulong,
) -> Result<Vec<u8>, Error> {
    init();
    let data_signature = vec![0u8; 0x40];

    let mut data_to_sign = SECItemBorrowed::wrap(&data);
    let mut signature = SECItemBorrowed::wrap(&data_signature);
    unsafe {
        secstatus_to_res(crate::p11::PK11_SignWithMechanism(
            private_key.as_mut().unwrap(),
            mechanism,
            std::ptr::null_mut(),
            signature.as_mut(),
            data_to_sign.as_mut(),
        ))
        .expect("Signature has failed");

        let signature = signature.as_slice().to_vec();
        Ok(signature)
    }
}

pub fn sign_ecdsa(private_key: PrivateKey, data: &[u8]) -> Result<Vec<u8>, Error> {
    sign(private_key, data, crate::p11::CKM_ECDSA.into())
}

pub fn sign_eddsa(private_key: PrivateKey, data: &[u8]) -> Result<Vec<u8>, Error> {
    sign(private_key, data, crate::p11::CKM_EDDSA.into())
}

pub fn verify(
    public_key: PublicKey,
    data: &[u8],
    signature: &[u8],
    mechanism: std::os::raw::c_ulong,
) -> Result<bool, Error> {
    init();
    unsafe {
        let mut data_to_sign = SECItemBorrowed::wrap(&data);
        let mut signature = SECItemBorrowed::wrap(&signature);

        let rv = crate::p11::PK11_VerifyWithMechanism(
            public_key.as_mut().unwrap(),
            mechanism.into(),
            std::ptr::null_mut(),
            signature.as_mut(),
            data_to_sign.as_mut(),
            std::ptr::null_mut(),
        );

        match rv {
            0 => Ok(true),
            _ => Ok(false),
        }
    }
}

pub fn verify_ecdsa(public_key: PublicKey, data: &[u8], signature: &[u8]) -> Result<bool, Error> {
    verify(public_key, data, signature, crate::p11::CKM_ECDSA.into())
}

pub fn verify_eddsa(public_key: PublicKey, data: &[u8], signature: &[u8]) -> Result<bool, Error> {
    verify(public_key, data, signature, crate::p11::CKM_EDDSA.into())
}
