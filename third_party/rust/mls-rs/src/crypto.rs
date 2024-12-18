// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

pub(crate) use mls_rs_core::crypto::CipherSuiteProvider;

pub use mls_rs_core::crypto::{
    HpkeCiphertext, HpkeContextR, HpkeContextS, HpkePublicKey, HpkeSecretKey, SignaturePublicKey,
    SignatureSecretKey,
};

pub use mls_rs_core::secret::Secret;

#[cfg(test)]
pub(crate) mod test_utils {
    use cfg_if::cfg_if;
    use mls_rs_core::crypto::CryptoProvider;

    cfg_if! {
        if #[cfg(target_arch = "wasm32")] {
            pub use mls_rs_crypto_webcrypto::WebCryptoProvider as TestCryptoProvider;
        } else {
            pub use mls_rs_crypto_openssl::OpensslCryptoProvider as TestCryptoProvider;
        }
    }

    use crate::cipher_suite::CipherSuite;

    pub fn test_cipher_suite_provider(
        cipher_suite: CipherSuite,
    ) -> <TestCryptoProvider as CryptoProvider>::CipherSuiteProvider {
        TestCryptoProvider::new()
            .cipher_suite_provider(cipher_suite)
            .unwrap()
    }

    #[allow(unused)]
    pub fn try_test_cipher_suite_provider(
        cipher_suite: u16,
    ) -> Option<<TestCryptoProvider as CryptoProvider>::CipherSuiteProvider> {
        TestCryptoProvider::new().cipher_suite_provider(CipherSuite::from(cipher_suite))
    }
}
