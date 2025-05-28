/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

use cstr::cstr;
use log;
use nserror::{nsresult, NS_ERROR_SERVICE_NOT_AVAILABLE};
use thin_vec::ThinVec;
use xpcom::{interfaces::nsICertStorage, RefPtr};

fn get_cert_storage() -> Result<RefPtr<nsICertStorage>, nsresult> {
    let cert_storage_uri = cstr!("@mozilla.org/security/certstorage;1");
    xpcom::get_service::<nsICertStorage>(cert_storage_uri).ok_or(NS_ERROR_SERVICE_NOT_AVAILABLE)
}

pub fn has_all_certs_by_hash(hashes: &ThinVec<ThinVec<u8>>) -> Result<bool, nsresult> {
    let cert_storage = get_cert_storage()?;
    let mut found: bool = false;
    unsafe {
        cert_storage
            .HasAllCertsByHash(hashes, &mut found)
            .to_result()?;
    }
    log::debug!("Looking for {} hashes, result: {}", hashes.len(), found);
    Ok(found)
}

pub fn get_cert_from_hash(hash: &ThinVec<u8>) -> Result<ThinVec<u8>, nsresult> {
    let cert_storage = get_cert_storage()?;
    let mut value = ThinVec::with_capacity(2000); // Avoid needing to reallocate
    unsafe {
        cert_storage.FindCertByHash(hash, &mut value).to_result()?;
    }
    log::debug!(
        "Looking for {:#02X?}, found result of size: {}",
        hash,
        value.len()
    );
    Ok(value)
}
