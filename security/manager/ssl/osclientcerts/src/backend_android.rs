/* -*- Mode: rust; rust-indent-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use digest::{Digest, DynDigest};
use pkcs11_bindings::*;
use rand::rngs::OsRng;
use rand::RngCore;
use rsclientcerts::error::{Error, ErrorType};
use rsclientcerts::manager::{ClientCertsBackend, CryptokiObject, Sign, SlotType};
use rsclientcerts::util::*;
use std::convert::TryInto;
use std::ffi::{c_char, c_void, CString};
use std::iter::zip;

type FindObjectsCallback = Option<
    unsafe extern "C" fn(
        typ: u8,
        data_len: usize,
        data: *const u8,
        extra_len: usize,
        extra: *const u8,
        slot_type: u32,
        ctx: *mut c_void,
    ),
>;

// Wrapper of C AndroidDoFindObject function implemented in nsNSSIOLayer.cpp
fn AndroidDoFindObjectsWrapper(callback: FindObjectsCallback, ctx: &mut FindObjectsContext) {
    // `AndroidDoFindObjects` communicates with the
    // `ClientAuthCertificateManager` to find all available client
    // authentication certificates and corresponding keys and issuers.
    extern "C" {
        fn AndroidDoFindObjects(callback: FindObjectsCallback, ctx: *mut c_void);
    }

    unsafe {
        AndroidDoFindObjects(callback, ctx as *mut _ as *mut c_void);
    }
}

type SignCallback =
    Option<unsafe extern "C" fn(data_len: usize, data: *const u8, ctx: *mut c_void)>;

// Wrapper of C AndroidDoSign function implemented in nsNSSIOLayer.cpp
fn AndroidDoSignWrapper(
    cert_len: usize,
    cert: *const u8,
    data_len: usize,
    data: *const u8,
    algorithm: *const c_char,
    callback: SignCallback,
    ctx: &mut Vec<u8>,
) {
    // `AndroidDoSign` calls into `ClientAuthCertificateManager` to do the
    // actual work of creating signatures.
    extern "C" {
        fn AndroidDoSign(
            cert_len: usize,
            cert: *const u8,
            data_len: usize,
            data: *const u8,
            algorithm: *const c_char,
            callback: SignCallback,
            ctx: *mut c_void,
        );
    }

    unsafe {
        AndroidDoSign(
            cert_len,
            cert,
            data_len,
            data,
            algorithm,
            callback,
            ctx as *mut _ as *mut c_void,
        );
    }
}

pub struct Cert {
    class: Vec<u8>,
    token: Vec<u8>,
    id: Vec<u8>,
    label: Vec<u8>,
    value: Vec<u8>,
    issuer: Vec<u8>,
    serial_number: Vec<u8>,
    subject: Vec<u8>,
    slot_type: SlotType,
}

impl Cert {
    fn new(der: &[u8], slot_type: SlotType) -> Result<Cert, Error> {
        let (serial_number, issuer, subject) = read_encoded_certificate_identifiers(der)?;
        let id = sha2::Sha256::digest(der).to_vec();
        Ok(Cert {
            class: serialize_uint(CKO_CERTIFICATE)?,
            token: serialize_uint(CK_TRUE)?,
            id,
            label: b"android certificate".to_vec(),
            value: der.to_vec(),
            issuer,
            serial_number,
            subject,
            slot_type,
        })
    }

    fn class(&self) -> &[u8] {
        &self.class
    }

    fn token(&self) -> &[u8] {
        &self.token
    }

    fn id(&self) -> &[u8] {
        &self.id
    }

    fn label(&self) -> &[u8] {
        &self.label
    }

    fn value(&self) -> &[u8] {
        &self.value
    }

    fn issuer(&self) -> &[u8] {
        &self.issuer
    }

    fn serial_number(&self) -> &[u8] {
        &self.serial_number
    }

    fn subject(&self) -> &[u8] {
        &self.subject
    }
}

impl CryptokiObject for Cert {
    fn matches(&self, slot_type: SlotType, attrs: &[(CK_ATTRIBUTE_TYPE, Vec<u8>)]) -> bool {
        if self.slot_type != slot_type {
            return false;
        }
        for (attr_type, attr_value) in attrs {
            let comparison = match *attr_type {
                CKA_CLASS => self.class(),
                CKA_TOKEN => self.token(),
                CKA_LABEL => self.label(),
                CKA_ID => self.id(),
                CKA_VALUE => self.value(),
                CKA_ISSUER => self.issuer(),
                CKA_SERIAL_NUMBER => self.serial_number(),
                CKA_SUBJECT => self.subject(),
                _ => return false,
            };
            if attr_value.as_slice() != comparison {
                return false;
            }
        }
        true
    }

    fn get_attribute(&self, attribute: CK_ATTRIBUTE_TYPE) -> Option<&[u8]> {
        let result = match attribute {
            CKA_CLASS => self.class(),
            CKA_TOKEN => self.token(),
            CKA_LABEL => self.label(),
            CKA_ID => self.id(),
            CKA_VALUE => self.value(),
            CKA_ISSUER => self.issuer(),
            CKA_SERIAL_NUMBER => self.serial_number(),
            CKA_SUBJECT => self.subject(),
            _ => return None,
        };
        Some(result)
    }
}

pub struct Key {
    cert: Vec<u8>,
    class: Vec<u8>,
    token: Vec<u8>,
    id: Vec<u8>,
    private: Vec<u8>,
    key_type: Vec<u8>,
    modulus: Option<Vec<u8>>,
    ec_params: Option<Vec<u8>>,
    slot_type: SlotType,
}

impl Key {
    fn new(
        modulus: Option<&[u8]>,
        ec_params: Option<&[u8]>,
        cert: &[u8],
        slot_type: SlotType,
    ) -> Result<Key, Error> {
        let id = sha2::Sha256::digest(cert).to_vec();
        let key_type = if modulus.is_some() { CKK_RSA } else { CKK_EC };
        // If this is an EC key, the frontend will have provided an SPKI.
        // Extract the parameters of the algorithm to get the curve.
        let ec_params = match ec_params {
            None => None,
            Some(ec_params) => Some(read_spki_algorithm_parameters(ec_params)?),
        };
        Ok(Key {
            cert: cert.to_vec(),
            class: serialize_uint(CKO_PRIVATE_KEY)?,
            token: serialize_uint(CK_TRUE)?,
            id,
            private: serialize_uint(CK_TRUE)?,
            key_type: serialize_uint(key_type)?,
            modulus: modulus.map(|b| b.to_vec()),
            ec_params,
            slot_type,
        })
    }

    fn class(&self) -> &[u8] {
        &self.class
    }

    fn token(&self) -> &[u8] {
        &self.token
    }

    pub fn id(&self) -> &[u8] {
        &self.id
    }

    fn private(&self) -> &[u8] {
        &self.private
    }

    fn key_type(&self) -> &[u8] {
        &self.key_type
    }

    fn modulus(&self) -> Option<&[u8]> {
        match &self.modulus {
            Some(modulus) => Some(modulus.as_slice()),
            None => None,
        }
    }

    fn ec_params(&self) -> Option<&[u8]> {
        match &self.ec_params {
            Some(ec_params) => Some(ec_params.as_slice()),
            None => None,
        }
    }

    fn modulus_bit_length(&self) -> Result<usize, Error> {
        // This should only be called if we already know this is an RSA key.
        let Some(modulus) = self.modulus.as_ref() else {
            return Err(error_here!(ErrorType::LibraryFailure));
        };
        let mut bit_length = modulus.len() * 8;
        for byte in modulus {
            if *byte != 0 {
                let leading_zeros: usize = byte
                    .leading_zeros()
                    .try_into()
                    .map_err(|_| error_here!(ErrorType::LibraryFailure))?;
                bit_length -= leading_zeros;
                return Ok(bit_length);
            }
            bit_length -= 8;
        }
        Ok(bit_length)
    }
}

impl CryptokiObject for Key {
    fn matches(&self, slot_type: SlotType, attrs: &[(CK_ATTRIBUTE_TYPE, Vec<u8>)]) -> bool {
        if self.slot_type != slot_type {
            return false;
        }
        for (attr_type, attr_value) in attrs {
            let comparison = match *attr_type {
                CKA_CLASS => self.class(),
                CKA_TOKEN => self.token(),
                CKA_ID => self.id(),
                CKA_PRIVATE => self.private(),
                CKA_KEY_TYPE => self.key_type(),
                CKA_MODULUS => {
                    if let Some(modulus) = self.modulus() {
                        modulus
                    } else {
                        return false;
                    }
                }
                CKA_EC_PARAMS => {
                    if let Some(ec_params) = self.ec_params() {
                        ec_params
                    } else {
                        return false;
                    }
                }
                _ => return false,
            };
            if attr_value.as_slice() != comparison {
                return false;
            }
        }
        true
    }

    fn get_attribute(&self, attribute: CK_ATTRIBUTE_TYPE) -> Option<&[u8]> {
        match attribute {
            CKA_CLASS => Some(self.class()),
            CKA_TOKEN => Some(self.token()),
            CKA_ID => Some(self.id()),
            CKA_PRIVATE => Some(self.private()),
            CKA_KEY_TYPE => Some(self.key_type()),
            CKA_MODULUS => self.modulus(),
            CKA_EC_PARAMS => self.ec_params(),
            _ => None,
        }
    }
}

fn make_hasher(params: &CK_RSA_PKCS_PSS_PARAMS) -> Result<Box<dyn DynDigest>, Error> {
    match params.hashAlg {
        CKM_SHA256 => Ok(Box::new(sha2::Sha256::new())),
        CKM_SHA384 => Ok(Box::new(sha2::Sha384::new())),
        CKM_SHA512 => Ok(Box::new(sha2::Sha512::new())),
        _ => Err(error_here!(ErrorType::LibraryFailure)),
    }
}

// Implements MGF1 as per RFC 8017 appendix B.2.1.
fn mgf(
    mgf_seed: &[u8],
    mask_len: usize,
    h_len: usize,
    params: &CK_RSA_PKCS_PSS_PARAMS,
) -> Result<Vec<u8>, Error> {
    // 1.  If maskLen > 2^32 hLen, output "mask too long" and stop.
    // (in practice, `mask_len` is going to be much smaller than this, so use a
    // smaller, fixed limit to avoid problems on systems where usize is 32
    // bits)
    if mask_len > 1 << 30 {
        return Err(error_here!(ErrorType::LibraryFailure));
    }
    // 2.  Let T be the empty octet string.
    let mut t = Vec::with_capacity(mask_len);
    // 3.  For counter from 0 to \ceil (maskLen / hLen) - 1, do the
    //     following:
    for counter in 0..mask_len.div_ceil(h_len) {
        // A.  Convert counter to an octet string C of length 4 octets:
        //     C = I2OSP (counter, 4)
        // (counter fits in u32 due to the length check earlier)
        let c = u32::to_be_bytes(counter.try_into().unwrap());
        // B.  Concatenate the hash of the seed mgfSeed and C to the octet
        //     string T: T = T || Hash(mgfSeed || C)
        let mut hasher = make_hasher(params)?;
        hasher.update(mgf_seed);
        hasher.update(&c);
        t.extend_from_slice(&mut hasher.finalize());
    }
    // 4.  Output the leading maskLen octets of T as the octet string mask.
    t.truncate(mask_len);
    Ok(t)
}

// Implements EMSA-PSS-ENCODE as per RFC 8017 section 9.1.1.
// This is necessary because while Android does support RSA-PSS, it expects to
// be given the entire message to be signed, not just the hash of the message,
// which is what NSS gives us.
fn emsa_pss_encode(
    m_hash: &[u8],
    em_bits: usize,
    params: &CK_RSA_PKCS_PSS_PARAMS,
) -> Result<Vec<u8>, Error> {
    let em_len = em_bits.div_ceil(8);
    let s_len: usize = params
        .sLen
        .try_into()
        .map_err(|_| error_here!(ErrorType::LibraryFailure))?;

    //  1.   If the length of M is greater than the input limitation for
    //       the hash function (2^61 - 1 octets for SHA-1), output
    //       "message too long" and stop.
    // 2.   Let mHash = Hash(M), an octet string of length hLen.

    // 1 and 2 can be skipped because the message is already hashed as m_hash.

    // 3.   If emLen < hLen + sLen + 2, output "encoding error" and stop.
    if em_len < m_hash.len() + s_len + 2 {
        return Err(error_here!(ErrorType::LibraryFailure));
    }

    // 4.   Generate a random octet string salt of length sLen; if sLen =
    //      0, then salt is the empty string.
    let salt = {
        let mut salt = vec![0u8; s_len];
        OsRng.fill_bytes(&mut salt);
        salt
    };

    // 5.   Let M' = (0x)00 00 00 00 00 00 00 00 || mHash || salt;
    //      M' is an octet string of length 8 + hLen + sLen with eight
    //      initial zero octets.
    // 6.   Let H = Hash(M'), an octet string of length hLen.
    let mut hasher = make_hasher(params)?;
    let h_len = hasher.output_size();
    hasher.update(&[0, 0, 0, 0, 0, 0, 0, 0]);
    hasher.update(m_hash);
    hasher.update(&salt);
    let h = hasher.finalize().to_vec();

    // 7.   Generate an octet string PS consisting of emLen - sLen - hLen
    //      - 2 zero octets.  The length of PS may be 0.
    // 8.   Let DB = PS || 0x01 || salt; DB is an octet string of length
    //      emLen - hLen - 1.
    // (7 and 8 are unnecessary as separate steps - see step 10)

    // 9.   Let dbMask = MGF(H, emLen - hLen - 1).
    let mut db_mask = mgf(&h, em_len - h_len - 1, h_len, params)?;

    // 10.  Let maskedDB = DB \xor dbMask.
    // (in practice, this means xoring `0x01 || salt` with the last `s_len + 1`
    // bytes of `db_mask`)
    let salt_index = db_mask.len() - s_len;
    db_mask[salt_index - 1] ^= 1;
    for (db_mask_byte, salt_byte) in zip(&mut db_mask[salt_index..], &salt) {
        *db_mask_byte ^= salt_byte;
    }
    let mut masked_db = db_mask;

    // 11.  Set the leftmost 8emLen - emBits bits of the leftmost octet
    //      in maskedDB to zero.
    // (bit_diff can only be 0 through 7, so it fits in u32)
    let bit_diff: u32 = ((8 * em_len) - em_bits).try_into().unwrap();
    // (again, bit_diff can only b 0 through 7, so the shift is sound)
    masked_db[0] &= 0xffu8.checked_shr(bit_diff).unwrap();

    // 12.  Let EM = maskedDB || H || 0xbc.
    let mut em = masked_db;
    em.extend_from_slice(&h);
    em.push(0xbc);

    Ok(em)
}

fn new_cstring(val: &str) -> Result<CString, Error> {
    CString::new(val).map_err(|_| error_here!(ErrorType::LibraryFailure))
}

impl Sign for Key {
    fn get_signature_length(
        &mut self,
        data: &[u8],
        params: &Option<CK_RSA_PKCS_PSS_PARAMS>,
    ) -> Result<usize, Error> {
        // Unfortunately we don't have a way of getting the length of a signature without creating
        // one.
        let dummy_signature_bytes = self.sign(data, params)?;
        Ok(dummy_signature_bytes.len())
    }

    fn sign(
        &mut self,
        data: &[u8],
        params: &Option<CK_RSA_PKCS_PSS_PARAMS>,
    ) -> Result<Vec<u8>, Error> {
        let (data, algorithm) = match params {
            Some(params) => (
                emsa_pss_encode(data, self.modulus_bit_length()? - 1, &params)?,
                new_cstring("raw")?,
            ),
            None if self.modulus.is_some() => (data.to_vec(), new_cstring("NoneWithRSA")?),
            None if self.ec_params.is_some() => (data.to_vec(), new_cstring("NoneWithECDSA")?),
            _ => return Err(error_here!(ErrorType::LibraryFailure)),
        };
        let mut signature = Vec::new();
        AndroidDoSignWrapper(
            self.cert.len(),
            self.cert.as_ptr(),
            data.len(),
            data.as_ptr(),
            algorithm.as_c_str().as_ptr(),
            Some(sign_callback),
            &mut signature,
        );
        if let Some(ec_params) = self.ec_params.as_ref() {
            let coordinate_width = match ec_params.as_slice() {
                ENCODED_OID_BYTES_SECP256R1 => 32,
                ENCODED_OID_BYTES_SECP384R1 => 48,
                ENCODED_OID_BYTES_SECP521R1 => 66,
                _ => return Err(error_here!(ErrorType::LibraryFailure)),
            };
            signature = der_ec_sig_to_raw(&signature, coordinate_width)?;
        }
        if signature.len() > 0 {
            Ok(signature)
        } else {
            Err(error_here!(ErrorType::LibraryFailure))
        }
    }
}

unsafe extern "C" fn sign_callback(data_len: usize, data: *const u8, ctx: *mut c_void) {
    let signature: &mut Vec<u8> = std::mem::transmute(ctx);
    signature.clear();
    if data_len != 0 {
        signature.extend_from_slice(std::slice::from_raw_parts(data, data_len));
    }
}

unsafe extern "C" fn find_objects_callback(
    typ: u8,
    data_len: usize,
    data: *const u8,
    extra_len: usize,
    extra: *const u8,
    slot_type: u32,
    ctx: *mut c_void,
) {
    let data = if data_len == 0 {
        &[]
    } else {
        std::slice::from_raw_parts(data, data_len)
    };
    let extra = if extra_len == 0 {
        &[]
    } else {
        std::slice::from_raw_parts(extra, extra_len)
    };
    let slot_type = match slot_type {
        1 => SlotType::Modern,
        2 => SlotType::Legacy,
        _ => return,
    };
    let find_objects_context: &mut FindObjectsContext = std::mem::transmute(ctx);
    match typ {
        1 => match Cert::new(data, slot_type) {
            Ok(cert) => find_objects_context.certs.push(cert),
            Err(_) => {}
        },
        2 => match Key::new(Some(data), None, extra, slot_type) {
            Ok(key) => find_objects_context.keys.push(key),
            Err(_) => {}
        },
        3 => match Key::new(None, Some(data), extra, slot_type) {
            Ok(key) => find_objects_context.keys.push(key),
            Err(_) => {}
        },
        _ => {}
    }
}

struct FindObjectsContext {
    certs: Vec<Cert>,
    keys: Vec<Key>,
}

impl FindObjectsContext {
    fn new() -> FindObjectsContext {
        FindObjectsContext {
            certs: Vec::new(),
            keys: Vec::new(),
        }
    }
}

pub struct Backend {}

impl ClientCertsBackend for Backend {
    type Cert = Cert;
    type Key = Key;

    fn find_objects(&self) -> Result<(Vec<Cert>, Vec<Key>), Error> {
        let mut find_objects_context = FindObjectsContext::new();
        AndroidDoFindObjectsWrapper(Some(find_objects_callback), &mut find_objects_context);
        Ok((find_objects_context.certs, find_objects_context.keys))
    }
}
