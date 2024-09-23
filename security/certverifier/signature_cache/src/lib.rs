/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

extern crate hashlink;

use hashlink::LruCache;

use std::sync::Mutex;

const SHA512_LENGTH_IN_BYTES: usize = 64;

/// SignatureCache is a simple least-recently-used cache. The input is a sha512
/// hash representing the parameters defining a signature. A hit in the cache
/// indicates that the signature previously verified successfully.
pub struct SignatureCache {
    cache: Mutex<LruCache<[u8; SHA512_LENGTH_IN_BYTES], ()>>,
}

impl SignatureCache {
    /// Make a new SignatureCache with the specified number of slots.
    fn new(capacity: u16) -> SignatureCache {
        SignatureCache {
            cache: Mutex::new(LruCache::new(capacity as usize)),
        }
    }

    /// Look up a signature hash in the cache. Returns true if the signature
    /// previously verified correctly, and false otherwise.
    fn get(&mut self, sha512_hash: &[u8; SHA512_LENGTH_IN_BYTES]) -> bool {
        let Ok(mut cache) = self.cache.lock() else {
            return false;
        };
        cache.get(sha512_hash).is_some()
    }

    /// Insert a signature hash into the cache.
    fn insert(&self, sha512_hash: [u8; SHA512_LENGTH_IN_BYTES]) {
        let Ok(mut cache) = self.cache.lock() else {
            return;
        };
        let _ = cache.insert(sha512_hash, ());
    }
}

/// Create a new SignatureCache.
#[no_mangle]
pub extern "C" fn signature_cache_new(capacity: u16) -> *mut SignatureCache {
    // This pattern returns a SignatureCache that will be owned by the caller.
    // That is, the SignatureCache will live until `signature_cache_free` is
    // called on it.
    Box::into_raw(Box::new(SignatureCache::new(capacity)))
}

/// Free a SignatureCache.
///
/// # Safety
///
/// This function must only be called with a null pointer or a pointer returned from
/// `signature_cache_new`.
#[no_mangle]
pub unsafe extern "C" fn signature_cache_free(signature_cache: *mut SignatureCache) {
    if signature_cache.is_null() {
        return;
    }
    // This takes a SignatureCache that was created by calling
    // `signature_cache_new` and ensures its resources are destroyed.
    let _ = Box::from_raw(signature_cache);
}

/// Look up a signature parameter hash in a SignatureCache.
///
/// # Safety
///
/// This function must only be called with a pointer returned from
/// `signature_cache_new` and a pointer to `SHA512_LENGTH_IN_BYTES` bytes.
#[no_mangle]
pub unsafe extern "C" fn signature_cache_get(
    signature_cache: *mut SignatureCache,
    sha512_hash: *const u8,
) -> bool {
    if signature_cache.is_null() || sha512_hash.is_null() {
        return false;
    }
    let sha512_hash = std::slice::from_raw_parts(sha512_hash, SHA512_LENGTH_IN_BYTES);
    let Ok(sha512_hash) = sha512_hash.try_into() else {
        return false;
    };
    let signature_cache = &mut *signature_cache;
    signature_cache.get(&sha512_hash)
}

/// Add a signature parameter hash to a SignatureCache.
///
/// # Safety
///
/// This function must only be called with a pointer returned from
/// `signature_cache_new` and a pointer to `SHA512_LENGTH_IN_BYTES` bytes.
#[no_mangle]
pub unsafe extern "C" fn signature_cache_insert(
    signature_cache: *mut SignatureCache,
    sha512_hash: *const u8,
) {
    if signature_cache.is_null() || sha512_hash.is_null() {
        return;
    }
    let sha512_hash = std::slice::from_raw_parts(sha512_hash, SHA512_LENGTH_IN_BYTES);
    let Ok(sha512_hash) = sha512_hash.try_into() else {
        return;
    };
    let signature_cache = &mut *signature_cache;
    signature_cache.insert(sha512_hash);
}
