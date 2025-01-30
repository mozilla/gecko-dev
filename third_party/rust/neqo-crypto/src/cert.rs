// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::ptr::NonNull;

use neqo_common::qerror;

use crate::{
    experimental_api, null_safe_slice,
    p11::{ItemArray, ItemArrayIterator, SECItem, SECItemArray},
    ssl::{PRFileDesc, SSL_PeerSignedCertTimestamps, SSL_PeerStapledOCSPResponses},
};

experimental_api!(SSL_PeerCertificateChainDER(
    fd: *mut PRFileDesc,
    out: *mut *mut SECItemArray,
));

pub struct CertificateInfo {
    certs: ItemArray,
    /// `stapled_ocsp_responses` and `signed_cert_timestamp` are properties
    /// associated with each of the certificates. Right now, NSS only
    /// reports the value for the end-entity certificate (the first).
    stapled_ocsp_responses: Option<Vec<Vec<u8>>>,
    signed_cert_timestamp: Option<Vec<u8>>,
}

fn peer_certificate_chain(fd: *mut PRFileDesc) -> Option<ItemArray> {
    let mut chain_ptr: *mut SECItemArray = std::ptr::null_mut();
    let rv = unsafe { SSL_PeerCertificateChainDER(fd, &mut chain_ptr) };
    if rv.is_ok() {
        ItemArray::from_ptr(chain_ptr).ok()
    } else {
        None
    }
}

// As explained in rfc6961, an OCSPResponseList can have at most
// 2^24 items. Casting its length is therefore safe even on 32 bits targets.
fn stapled_ocsp_responses(fd: *mut PRFileDesc) -> Option<Vec<Vec<u8>>> {
    let ocsp_nss = unsafe { SSL_PeerStapledOCSPResponses(fd) };
    match NonNull::new(ocsp_nss as *mut SECItemArray) {
        Some(ocsp_ptr) => {
            let mut ocsp_helper: Vec<Vec<u8>> = Vec::new();
            let Ok(len) = isize::try_from(unsafe { ocsp_ptr.as_ref().len }) else {
                qerror!([format!("{fd:p}")], "Received illegal OSCP length");
                return None;
            };
            for idx in 0..len {
                let itemp: *const SECItem = unsafe { ocsp_ptr.as_ref().items.offset(idx).cast() };
                let item = unsafe { null_safe_slice((*itemp).data, (*itemp).len) };
                ocsp_helper.push(item.to_owned());
            }
            Some(ocsp_helper)
        }
        None => None,
    }
}

fn signed_cert_timestamp(fd: *mut PRFileDesc) -> Option<Vec<u8>> {
    let sct_nss = unsafe { SSL_PeerSignedCertTimestamps(fd) };
    NonNull::new(sct_nss as *mut SECItem).map(|sct_ptr| {
        if unsafe { sct_ptr.as_ref().len == 0 || sct_ptr.as_ref().data.is_null() } {
            Vec::new()
        } else {
            let sct_slice = unsafe { null_safe_slice(sct_ptr.as_ref().data, sct_ptr.as_ref().len) };
            sct_slice.to_owned()
        }
    })
}

impl CertificateInfo {
    pub(crate) fn new(fd: *mut PRFileDesc) -> Option<Self> {
        peer_certificate_chain(fd).map(|certs| Self {
            certs,
            stapled_ocsp_responses: stapled_ocsp_responses(fd),
            signed_cert_timestamp: signed_cert_timestamp(fd),
        })
    }
}

impl CertificateInfo {
    #[must_use]
    pub fn iter(&self) -> ItemArrayIterator<'_> {
        self.certs.into_iter()
    }
}

impl<'a> IntoIterator for &'a CertificateInfo {
    type IntoIter = ItemArrayIterator<'a>;
    type Item = &'a [u8];
    fn into_iter(self) -> Self::IntoIter {
        self.iter()
    }
}

impl CertificateInfo {
    #[must_use]
    pub const fn stapled_ocsp_responses(&self) -> &Option<Vec<Vec<u8>>> {
        &self.stapled_ocsp_responses
    }

    #[must_use]
    pub const fn signed_cert_timestamp(&self) -> &Option<Vec<u8>> {
        &self.signed_cert_timestamp
    }
}
