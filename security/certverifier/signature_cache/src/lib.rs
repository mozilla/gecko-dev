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

/// TrustCache is a simple least-recently-used cache. The input is a sha512
/// hash representing the parameters involved in a certificate trust lookup
/// (trust category (i.e. TLS or email signing), end-entity or CA, policy,
/// etc.), which maps to a mozilla::pkix::TrustLevel value represented as a u8.
pub struct TrustCache {
    cache: Mutex<LruCache<[u8; SHA512_LENGTH_IN_BYTES], u8>>,
}

impl TrustCache {
    /// Make a new TrustCache with the specified number of slots.
    fn new(capacity: u16) -> TrustCache {
        TrustCache {
            cache: Mutex::new(LruCache::new(capacity as usize)),
        }
    }

    /// Look up a trust hash in the cache. Returns `None` if there is no cached
    /// trust information. Otherwise, returns `Some(trust)`, where `trust` is
    /// the previously-cached TrustLevel.
    fn get(&mut self, sha512_hash: &[u8; SHA512_LENGTH_IN_BYTES]) -> Option<u8> {
        let Ok(mut cache) = self.cache.lock() else {
            return None;
        };
        cache.get(sha512_hash).cloned()
    }

    /// Insert a trust hash into the cache.
    fn insert(&self, sha512_hash: [u8; SHA512_LENGTH_IN_BYTES], trust: u8) {
        let Ok(mut cache) = self.cache.lock() else {
            return;
        };
        let _ = cache.insert(sha512_hash, trust);
    }

    /// Clear the cache, removing all entries.
    fn clear(&mut self) {
        let Ok(mut cache) = self.cache.lock() else {
            return;
        };
        cache.clear();
    }
}

/// Create a new TrustCache.
#[no_mangle]
pub extern "C" fn trust_cache_new(capacity: u16) -> *mut TrustCache {
    // This pattern returns a TrustCache that will be owned by the caller.
    // That is, the TrustCache will live until `trust_cache_free` is
    // called on it.
    Box::into_raw(Box::new(TrustCache::new(capacity)))
}

/// Free a TrustCache.
///
/// # Safety
///
/// This function must only be called with a null pointer or a pointer returned from
/// `trust_cache_new`.
#[no_mangle]
pub unsafe extern "C" fn trust_cache_free(trust_cache: *mut TrustCache) {
    if trust_cache.is_null() {
        return;
    }
    // This takes a TrustCache that was created by calling
    // `trust_cache_new` and ensures its resources are destroyed.
    let _ = Box::from_raw(trust_cache);
}

/// Look up a trust hash in a TrustCache.
///
/// # Safety
///
/// This function must only be called with a pointer returned from
/// `trust_cache_new` and a pointer to `SHA512_LENGTH_IN_BYTES` bytes.
#[no_mangle]
pub unsafe extern "C" fn trust_cache_get(
    trust_cache: *mut TrustCache,
    sha512_hash: *const u8,
    trust: *mut u8,
) -> bool {
    if trust_cache.is_null() || sha512_hash.is_null() || trust.is_null() {
        return false;
    }
    let sha512_hash = std::slice::from_raw_parts(sha512_hash, SHA512_LENGTH_IN_BYTES);
    let Ok(sha512_hash) = sha512_hash.try_into() else {
        return false;
    };
    let trust_cache = &mut *trust_cache;
    let Some(cached_trust) = trust_cache.get(&sha512_hash) else {
        return false;
    };
    *trust = cached_trust;
    true
}

/// Add a trust hash to a TrustCache.
///
/// # Safety
///
/// This function must only be called with a pointer returned from
/// `trust_cache_new` and a pointer to `SHA512_LENGTH_IN_BYTES` bytes.
#[no_mangle]
pub unsafe extern "C" fn trust_cache_insert(
    trust_cache: *mut TrustCache,
    sha512_hash: *const u8,
    trust: u8,
) {
    if trust_cache.is_null() || sha512_hash.is_null() {
        return;
    }
    let sha512_hash = std::slice::from_raw_parts(sha512_hash, SHA512_LENGTH_IN_BYTES);
    let Ok(sha512_hash) = sha512_hash.try_into() else {
        return;
    };
    let trust_cache = &mut *trust_cache;
    trust_cache.insert(sha512_hash, trust);
}

/// Clear a trust cache, removing all entries.
///
/// # Safety
///
/// This function must only be called with a pointer returned from
/// `trust_cache_new`.
#[no_mangle]
pub unsafe extern "C" fn trust_cache_clear(trust_cache: *mut TrustCache) {
    if trust_cache.is_null() {
        return;
    }
    let trust_cache = &mut *trust_cache;
    trust_cache.clear();
}
