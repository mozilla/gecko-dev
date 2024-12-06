// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use core::ops::Deref;

use alloc::vec::Vec;

use mls_rs_crypto_traits::{Curve, DhType};

use mls_rs_core::{
    crypto::{CipherSuite, HpkePublicKey, HpkeSecretKey},
    error::IntoAnyError,
};

use crate::ec::{
    generate_keypair, private_key_bytes_to_public, private_key_ecdh, private_key_from_bytes,
    pub_key_from_uncompressed, EcError, EcPublicKey,
};

#[derive(Debug)]
#[cfg_attr(feature = "std", derive(thiserror::Error))]
pub enum EcdhKemError {
    #[cfg_attr(feature = "std", error(transparent))]
    EcError(EcError),
    #[cfg_attr(feature = "std", error("unsupported cipher suite"))]
    UnsupportedCipherSuite,
}

impl From<EcError> for EcdhKemError {
    fn from(e: EcError) -> Self {
        EcdhKemError::EcError(e)
    }
}

impl IntoAnyError for EcdhKemError {
    #[cfg(feature = "std")]
    fn into_dyn_error(self) -> Result<Box<dyn std::error::Error + Send + Sync>, Self> {
        Ok(self.into())
    }
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct Ecdh(Curve);

impl Deref for Ecdh {
    type Target = Curve;

    fn deref(&self) -> &Self::Target {
        &self.0
    }
}

impl Ecdh {
    pub fn new(cipher_suite: CipherSuite) -> Option<Self> {
        Curve::from_ciphersuite(cipher_suite, false).map(Self)
    }
}

#[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
#[cfg_attr(all(target_arch = "wasm32", mls_build_async), maybe_async::must_be_async(?Send))]
#[cfg_attr(
    all(not(target_arch = "wasm32"), mls_build_async),
    maybe_async::must_be_async
)]
impl DhType for Ecdh {
    type Error = EcdhKemError;

    async fn dh(
        &self,
        secret_key: &HpkeSecretKey,
        public_key: &HpkePublicKey,
    ) -> Result<Vec<u8>, Self::Error> {
        Ok(private_key_ecdh(
            &private_key_from_bytes(secret_key.to_vec(), self.0)?,
            &self.to_ec_public_key(public_key)?,
        )?)
    }
    async fn to_public(&self, secret_key: &HpkeSecretKey) -> Result<HpkePublicKey, Self::Error> {
        Ok(private_key_bytes_to_public(secret_key.to_vec(), self.0)?.into())
    }

    async fn generate(&self) -> Result<(HpkeSecretKey, HpkePublicKey), Self::Error> {
        let key_pair = generate_keypair(self.0)?;
        Ok((key_pair.secret.into(), key_pair.public.into()))
    }

    fn bitmask_for_rejection_sampling(&self) -> Option<u8> {
        self.curve_bitmask()
    }

    fn public_key_validate(&self, key: &HpkePublicKey) -> Result<(), Self::Error> {
        self.to_ec_public_key(key).map(|_| ())
    }

    fn secret_key_size(&self) -> usize {
        self.0.secret_key_size()
    }
}

impl Ecdh {
    fn to_ec_public_key(&self, public_key: &HpkePublicKey) -> Result<EcPublicKey, EcdhKemError> {
        Ok(pub_key_from_uncompressed(public_key.to_vec(), self.0)?)
    }
}

#[cfg(all(test, not(mls_build_async)))]
mod test {
    use mls_rs_core::crypto::{CipherSuite, HpkePublicKey, HpkeSecretKey};
    use mls_rs_crypto_traits::DhType;
    use serde::Deserialize;

    use alloc::vec::Vec;

    use crate::ecdh::Ecdh;

    #[allow(dead_code)]
    fn get_ecdhs() -> Vec<Ecdh> {
        [CipherSuite::P256_AES128, CipherSuite::CURVE25519_AES128]
            .into_iter()
            .map(|c| Ecdh::new(c).unwrap())
            .collect()
    }

    #[derive(Deserialize)]
    struct TestCase {
        pub ciphersuite: u16,
        #[serde(with = "hex::serde")]
        pub alice_pub: Vec<u8>,
        #[serde(with = "hex::serde")]
        pub alice_pri: Vec<u8>,
        #[serde(with = "hex::serde")]
        pub bob_pub: Vec<u8>,
        #[serde(with = "hex::serde")]
        pub bob_pri: Vec<u8>,
        #[serde(with = "hex::serde")]
        pub shared_secret: Vec<u8>,
    }

    fn run_test_case(test_case: TestCase) {
        let ecdh = Ecdh::new(test_case.ciphersuite.into()).unwrap();

        // Import the keys into their structures
        let alice_pub: HpkePublicKey = test_case.alice_pub.into();
        let bob_pub: HpkePublicKey = test_case.bob_pub.into();
        let alice_pri: HpkeSecretKey = test_case.alice_pri.into();
        let bob_pri: HpkeSecretKey = test_case.bob_pri.into();

        assert_eq!(ecdh.to_public(&alice_pri).unwrap(), alice_pub);
        assert_eq!(ecdh.to_public(&bob_pri).unwrap(), bob_pub);

        assert_eq!(
            ecdh.dh(&alice_pri, &bob_pub).unwrap(),
            test_case.shared_secret
        );

        assert_eq!(
            ecdh.dh(&bob_pri, &alice_pub).unwrap(),
            test_case.shared_secret
        );
    }

    #[test]
    fn test_algo_test_cases() {
        let test_case_file = include_str!("../test_data/test_ecdh.json");
        let test_cases: Vec<TestCase> = serde_json::from_str(test_case_file).unwrap();

        for case in test_cases {
            run_test_case(case);
        }
    }

    // TODO: discuss if we need this test
    // #[test]
    // fn test_mismatched_curve() {
    //     for ecdh in get_ecdhs() {
    //         let secret_key = ecdh.generate().unwrap().0;

    //         for other_ecdh in get_ecdhs().into_iter().filter(|c| c != &ecdh) {
    //             let other_public_key = other_ecdh.generate().unwrap().1;
    //             assert!(ecdh.dh(&secret_key, &other_public_key).is_err());
    //         }
    //     }
    // }
}
