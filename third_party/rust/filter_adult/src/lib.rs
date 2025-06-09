/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

mod error;

pub use error::{ApiError, ApiResult, Result};
use md5::{Digest, Md5};
use std::collections::HashSet;

uniffi::setup_scaffolding!("filter_adult");

#[derive(uniffi::Object)]
pub struct FilterAdultComponent {
    inner: FilterAdultInner,
}

#[uniffi::export]
impl FilterAdultComponent {
    /// Construct a new FilterAdultComponent
    #[uniffi::constructor]
    pub fn new() -> ApiResult<Self> {
        Ok(Self {
            inner: FilterAdultInner::new().unwrap(),
        })
    }

    /// Check if a URL is in the adult domain list
    pub fn contains(&self, base_domain_to_check: &str) -> ApiResult<bool> {
        Ok(self.inner.contains(base_domain_to_check))
    }
}

struct FilterAdultInner {
    hashes: HashSet<[u8; 16]>,
}

impl FilterAdultInner {
    pub fn new() -> Result<Self> {
        let adult_set_bytes = include_bytes!("adult_set.bin");
        let mut hashes = HashSet::new();
        for chunk in adult_set_bytes.chunks_exact(16) {
            let mut arr = [0; 16];
            arr.copy_from_slice(chunk);
            hashes.insert(arr);
        }

        Ok(Self { hashes })
    }

    pub fn contains(&self, base_domain_to_check: &str) -> bool {
        let mut hasher = Md5::new();
        hasher.update(base_domain_to_check);
        let bytes: [u8; 16] = hasher.finalize().into();
        self.hashes.contains(&bytes)
    }

    // Used purely for testing.
    #[allow(dead_code)]
    pub fn add_domain(&mut self, base_domain: &str) {
        let mut hasher = Md5::new();
        hasher.update(base_domain);
        let bytes: [u8; 16] = hasher.finalize().into();
        self.hashes.insert(bytes);
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    const TEST_ADULT_SITE_URL: &str = "https://some-adult-site.com/";

    #[test]
    fn test_contains_returns_false_for_unexpected_urls() {
        let inner = FilterAdultInner::new().expect("Could construct");
        assert!(!inner.contains(""), "Should not recognize invaild URLs");
    }

    #[test]
    fn test_contains_returns_false_for_non_adult_urls() {
        let inner = FilterAdultInner::new().expect("Could construct");
        assert!(
            !inner.contains("https://mozilla.org/"),
            "Should return false for a non-adult URL"
        );
    }

    #[test]
    fn test_contains_returns_true_for_adult_urls() {
        let mut inner = FilterAdultInner::new().expect("Could construct");

        assert!(
            !inner.contains(TEST_ADULT_SITE_URL),
            "Should not yet have the test adult site."
        );
        inner.add_domain(TEST_ADULT_SITE_URL);

        assert!(
            inner.contains(TEST_ADULT_SITE_URL),
            "Should have the test adult site."
        );
    }
}
