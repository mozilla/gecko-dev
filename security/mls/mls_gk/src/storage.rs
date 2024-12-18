/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use nsstring::nsACString;
use std::fs;
use std::io;

pub fn get_storage_path(storage_prefix: &nsACString) -> String {
    format!("{storage_prefix}.sqlite.enc")
}

pub fn get_key_path(storage_prefix: &nsACString) -> String {
    format!("{storage_prefix}.key")
}

fn read_existing_storage_key(key_path: &str) -> io::Result<[u8; 32]> {
    let key_hex = fs::read_to_string(key_path)?;
    let bytes = hex::decode(&key_hex).map_err(|e| io::Error::other(e))?;
    bytes[..].try_into().map_err(|e| io::Error::other(e))
}

pub fn get_storage_key(storage_prefix: &nsACString) -> io::Result<[u8; 32]> {
    // Get the key path
    let key_path = get_key_path(storage_prefix);

    // Try to read the existing key
    if let Ok(key) = read_existing_storage_key(&key_path) {
        return Ok(key);
    }

    // We failed to read the key, so it must either not exist, or is invalid.
    // Generate a new one.
    nss_gk_api::init();
    let key: [u8; 32] = nss_gk_api::p11::random(32)[..]
        .try_into()
        .expect("nss returned the wrong number of bytes");

    // Write the key to the file
    std::fs::write(key_path, &hex::encode(&key))?;

    // Return the key
    Ok(key)
}
