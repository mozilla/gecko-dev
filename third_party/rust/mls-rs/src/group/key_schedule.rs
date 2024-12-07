// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use crate::client::MlsError;
use crate::extension::ExternalPubExt;
use crate::group::{GroupContext, MembershipTag};
use crate::psk::secret::PskSecret;
#[cfg(feature = "psk")]
use crate::psk::PreSharedKey;
use crate::tree_kem::path_secret::PathSecret;
use crate::CipherSuiteProvider;

#[cfg(any(feature = "secret_tree_access", feature = "private_message"))]
use crate::group::SecretTree;

use alloc::vec;
use alloc::vec::Vec;
use core::fmt::{self, Debug};
use mls_rs_codec::{MlsDecode, MlsEncode, MlsSize};
use mls_rs_core::error::IntoAnyError;
use zeroize::Zeroizing;

use crate::crypto::{HpkeContextR, HpkeContextS, HpkePublicKey, HpkeSecretKey};

use super::epoch::{EpochSecrets, SenderDataSecret};
use super::message_signature::AuthenticatedContent;

#[derive(Clone, PartialEq, Eq, Default, MlsEncode, MlsDecode, MlsSize)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
pub struct KeySchedule {
    #[mls_codec(with = "mls_rs_codec::byte_vec")]
    #[cfg_attr(feature = "serde", serde(with = "mls_rs_core::zeroizing_serde"))]
    exporter_secret: Zeroizing<Vec<u8>>,
    #[mls_codec(with = "mls_rs_codec::byte_vec")]
    #[cfg_attr(feature = "serde", serde(with = "mls_rs_core::zeroizing_serde"))]
    pub authentication_secret: Zeroizing<Vec<u8>>,
    #[mls_codec(with = "mls_rs_codec::byte_vec")]
    #[cfg_attr(feature = "serde", serde(with = "mls_rs_core::zeroizing_serde"))]
    external_secret: Zeroizing<Vec<u8>>,
    #[mls_codec(with = "mls_rs_codec::byte_vec")]
    #[cfg_attr(feature = "serde", serde(with = "mls_rs_core::zeroizing_serde"))]
    membership_key: Zeroizing<Vec<u8>>,
    init_secret: InitSecret,
}

impl Debug for KeySchedule {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("KeySchedule")
            .field(
                "exporter_secret",
                &mls_rs_core::debug::pretty_bytes(&self.exporter_secret),
            )
            .field(
                "authentication_secret",
                &mls_rs_core::debug::pretty_bytes(&self.authentication_secret),
            )
            .field(
                "external_secret",
                &mls_rs_core::debug::pretty_bytes(&self.external_secret),
            )
            .field(
                "membership_key",
                &mls_rs_core::debug::pretty_bytes(&self.membership_key),
            )
            .field("init_secret", &self.init_secret)
            .finish()
    }
}

pub(crate) struct KeyScheduleDerivationResult {
    pub(crate) key_schedule: KeySchedule,
    pub(crate) confirmation_key: Zeroizing<Vec<u8>>,
    pub(crate) joiner_secret: JoinerSecret,
    pub(crate) epoch_secrets: EpochSecrets,
}

impl KeySchedule {
    pub fn new(init_secret: InitSecret) -> Self {
        KeySchedule {
            init_secret,
            ..Default::default()
        }
    }

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub async fn derive_for_external<P: CipherSuiteProvider>(
        &self,
        kem_output: &[u8],
        cipher_suite: &P,
    ) -> Result<KeySchedule, MlsError> {
        let (secret, public) = self.get_external_key_pair(cipher_suite).await?;

        let init_secret =
            InitSecret::decode_for_external(cipher_suite, kem_output, &secret, &public).await?;

        Ok(KeySchedule::new(init_secret))
    }

    /// Returns the derived epoch as well as the joiner secret required for building welcome
    /// messages
    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub(crate) async fn from_key_schedule<P: CipherSuiteProvider>(
        last_key_schedule: &KeySchedule,
        commit_secret: &PathSecret,
        context: &GroupContext,
        #[cfg(any(feature = "secret_tree_access", feature = "private_message"))]
        secret_tree_size: u32,
        psk_secret: &PskSecret,
        cipher_suite_provider: &P,
    ) -> Result<KeyScheduleDerivationResult, MlsError> {
        let joiner_seed = cipher_suite_provider
            .kdf_extract(&last_key_schedule.init_secret.0, commit_secret)
            .await
            .map_err(|e| MlsError::CryptoProviderError(e.into_any_error()))?;

        let joiner_secret = kdf_expand_with_label(
            cipher_suite_provider,
            &joiner_seed,
            b"joiner",
            &context.mls_encode_to_vec()?,
            None,
        )
        .await?
        .into();

        let key_schedule_result = Self::from_joiner(
            cipher_suite_provider,
            &joiner_secret,
            context,
            #[cfg(any(feature = "secret_tree_access", feature = "private_message"))]
            secret_tree_size,
            psk_secret,
        )
        .await?;

        Ok(KeyScheduleDerivationResult {
            key_schedule: key_schedule_result.key_schedule,
            confirmation_key: key_schedule_result.confirmation_key,
            joiner_secret,
            epoch_secrets: key_schedule_result.epoch_secrets,
        })
    }

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub(crate) async fn from_joiner<P: CipherSuiteProvider>(
        cipher_suite_provider: &P,
        joiner_secret: &JoinerSecret,
        context: &GroupContext,
        #[cfg(any(feature = "secret_tree_access", feature = "private_message"))]
        secret_tree_size: u32,
        psk_secret: &PskSecret,
    ) -> Result<KeyScheduleDerivationResult, MlsError> {
        let epoch_seed =
            get_pre_epoch_secret(cipher_suite_provider, psk_secret, joiner_secret).await?;
        let context = context.mls_encode_to_vec()?;

        let epoch_secret =
            kdf_expand_with_label(cipher_suite_provider, &epoch_seed, b"epoch", &context, None)
                .await?;

        Self::from_epoch_secret(
            cipher_suite_provider,
            &epoch_secret,
            #[cfg(any(feature = "secret_tree_access", feature = "private_message"))]
            secret_tree_size,
        )
        .await
    }

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub(crate) async fn from_random_epoch_secret<P: CipherSuiteProvider>(
        cipher_suite_provider: &P,
        #[cfg(any(feature = "secret_tree_access", feature = "private_message"))]
        secret_tree_size: u32,
    ) -> Result<KeyScheduleDerivationResult, MlsError> {
        let epoch_secret = cipher_suite_provider
            .random_bytes_vec(cipher_suite_provider.kdf_extract_size())
            .map(Zeroizing::new)
            .map_err(|e| MlsError::CryptoProviderError(e.into_any_error()))?;

        Self::from_epoch_secret(
            cipher_suite_provider,
            &epoch_secret,
            #[cfg(any(feature = "secret_tree_access", feature = "private_message"))]
            secret_tree_size,
        )
        .await
    }

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    async fn from_epoch_secret<P: CipherSuiteProvider>(
        cipher_suite_provider: &P,
        epoch_secret: &[u8],
        #[cfg(any(feature = "secret_tree_access", feature = "private_message"))]
        secret_tree_size: u32,
    ) -> Result<KeyScheduleDerivationResult, MlsError> {
        let secrets_producer = SecretsProducer::new(cipher_suite_provider, epoch_secret);

        let epoch_secrets = EpochSecrets {
            #[cfg(feature = "psk")]
            resumption_secret: PreSharedKey::from(secrets_producer.derive(b"resumption").await?),
            sender_data_secret: SenderDataSecret::from(
                secrets_producer.derive(b"sender data").await?,
            ),
            #[cfg(any(feature = "secret_tree_access", feature = "private_message"))]
            secret_tree: SecretTree::new(
                secret_tree_size,
                secrets_producer.derive(b"encryption").await?,
            ),
        };

        let key_schedule = Self {
            exporter_secret: secrets_producer.derive(b"exporter").await?,
            authentication_secret: secrets_producer.derive(b"authentication").await?,
            external_secret: secrets_producer.derive(b"external").await?,
            membership_key: secrets_producer.derive(b"membership").await?,
            init_secret: InitSecret(secrets_producer.derive(b"init").await?),
        };

        Ok(KeyScheduleDerivationResult {
            key_schedule,
            confirmation_key: secrets_producer.derive(b"confirm").await?,
            joiner_secret: Zeroizing::new(vec![]).into(),
            epoch_secrets,
        })
    }

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub async fn export_secret<P: CipherSuiteProvider>(
        &self,
        label: &[u8],
        context: &[u8],
        len: usize,
        cipher_suite: &P,
    ) -> Result<Zeroizing<Vec<u8>>, MlsError> {
        let secret = kdf_derive_secret(cipher_suite, &self.exporter_secret, label).await?;

        let context_hash = cipher_suite
            .hash(context)
            .await
            .map_err(|e| MlsError::CryptoProviderError(e.into_any_error()))?;

        kdf_expand_with_label(cipher_suite, &secret, b"exported", &context_hash, Some(len)).await
    }

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub async fn get_membership_tag<P: CipherSuiteProvider>(
        &self,
        content: &AuthenticatedContent,
        context: &GroupContext,
        cipher_suite_provider: &P,
    ) -> Result<MembershipTag, MlsError> {
        MembershipTag::create(
            content,
            context,
            &self.membership_key,
            cipher_suite_provider,
        )
        .await
    }

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub async fn get_external_key_pair<P: CipherSuiteProvider>(
        &self,
        cipher_suite: &P,
    ) -> Result<(HpkeSecretKey, HpkePublicKey), MlsError> {
        cipher_suite
            .kem_derive(&self.external_secret)
            .await
            .map_err(|e| MlsError::CryptoProviderError(e.into_any_error()))
    }

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub async fn get_external_key_pair_ext<P: CipherSuiteProvider>(
        &self,
        cipher_suite: &P,
    ) -> Result<ExternalPubExt, MlsError> {
        let (_external_secret, external_pub) = self.get_external_key_pair(cipher_suite).await?;

        Ok(ExternalPubExt { external_pub })
    }
}

#[derive(MlsEncode, MlsSize)]
struct Label<'a> {
    length: u16,
    #[mls_codec(with = "mls_rs_codec::byte_vec")]
    label: Vec<u8>,
    #[mls_codec(with = "mls_rs_codec::byte_vec")]
    context: &'a [u8],
}

impl<'a> Label<'a> {
    fn new(length: u16, label: &'a [u8], context: &'a [u8]) -> Self {
        Self {
            length,
            label: [b"MLS 1.0 ", label].concat(),
            context,
        }
    }
}

#[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
pub(crate) async fn kdf_expand_with_label<P: CipherSuiteProvider>(
    cipher_suite_provider: &P,
    secret: &[u8],
    label: &[u8],
    context: &[u8],
    len: Option<usize>,
) -> Result<Zeroizing<Vec<u8>>, MlsError> {
    let extract_size = cipher_suite_provider.kdf_extract_size();
    let len = len.unwrap_or(extract_size);
    let label = Label::new(len as u16, label, context);

    cipher_suite_provider
        .kdf_expand(secret, &label.mls_encode_to_vec()?, len)
        .await
        .map_err(|e| MlsError::CryptoProviderError(e.into_any_error()))
}

#[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
pub(crate) async fn kdf_derive_secret<P: CipherSuiteProvider>(
    cipher_suite_provider: &P,
    secret: &[u8],
    label: &[u8],
) -> Result<Zeroizing<Vec<u8>>, MlsError> {
    kdf_expand_with_label(cipher_suite_provider, secret, label, &[], None).await
}

#[derive(Clone, PartialEq, MlsSize, MlsEncode, MlsDecode)]
pub(crate) struct JoinerSecret(#[mls_codec(with = "mls_rs_codec::byte_vec")] Zeroizing<Vec<u8>>);

impl Debug for JoinerSecret {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        mls_rs_core::debug::pretty_bytes(&self.0)
            .named("JoinerSecret")
            .fmt(f)
    }
}

impl From<Zeroizing<Vec<u8>>> for JoinerSecret {
    fn from(bytes: Zeroizing<Vec<u8>>) -> Self {
        Self(bytes)
    }
}

#[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
pub(crate) async fn get_pre_epoch_secret<P: CipherSuiteProvider>(
    cipher_suite_provider: &P,
    psk_secret: &PskSecret,
    joiner_secret: &JoinerSecret,
) -> Result<Zeroizing<Vec<u8>>, MlsError> {
    cipher_suite_provider
        .kdf_extract(&joiner_secret.0, psk_secret)
        .await
        .map_err(|e| MlsError::CryptoProviderError(e.into_any_error()))
}

struct SecretsProducer<'a, P: CipherSuiteProvider> {
    cipher_suite_provider: &'a P,
    epoch_secret: &'a [u8],
}

impl<'a, P: CipherSuiteProvider> SecretsProducer<'a, P> {
    fn new(cipher_suite_provider: &'a P, epoch_secret: &'a [u8]) -> Self {
        Self {
            cipher_suite_provider,
            epoch_secret,
        }
    }

    // TODO document somewhere in the crypto provider that the RFC defines the length of all secrets as
    // KDF extract size but then inputs secrets as MAC keys etc, therefore, we require that these
    // lengths match in the crypto provider
    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    async fn derive(&self, label: &[u8]) -> Result<Zeroizing<Vec<u8>>, MlsError> {
        kdf_derive_secret(self.cipher_suite_provider, self.epoch_secret, label).await
    }
}

const EXPORTER_CONTEXT: &[u8] = b"MLS 1.0 external init secret";

#[derive(Clone, Eq, PartialEq, MlsEncode, MlsDecode, MlsSize, Default)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
pub struct InitSecret(
    #[mls_codec(with = "mls_rs_codec::byte_vec")]
    #[cfg_attr(feature = "serde", serde(with = "mls_rs_core::zeroizing_serde"))]
    Zeroizing<Vec<u8>>,
);

impl Debug for InitSecret {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        mls_rs_core::debug::pretty_bytes(&self.0)
            .named("InitSecret")
            .fmt(f)
    }
}

impl InitSecret {
    /// Returns init secret and KEM output to be used when creating an external commit.
    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub async fn encode_for_external<P: CipherSuiteProvider>(
        cipher_suite: &P,
        external_pub: &HpkePublicKey,
    ) -> Result<(Self, Vec<u8>), MlsError> {
        let (kem_output, context) = cipher_suite
            .hpke_setup_s(external_pub, &[])
            .await
            .map_err(|e| MlsError::CryptoProviderError(e.into_any_error()))?;

        let init_secret = context
            .export(EXPORTER_CONTEXT, cipher_suite.kdf_extract_size())
            .await
            .map_err(|e| MlsError::CryptoProviderError(e.into_any_error()))?;

        Ok((InitSecret(Zeroizing::new(init_secret)), kem_output))
    }

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub async fn decode_for_external<P: CipherSuiteProvider>(
        cipher_suite: &P,
        kem_output: &[u8],
        external_secret: &HpkeSecretKey,
        external_pub: &HpkePublicKey,
    ) -> Result<Self, MlsError> {
        let context = cipher_suite
            .hpke_setup_r(kem_output, external_secret, external_pub, &[])
            .await
            .map_err(|e| MlsError::CryptoProviderError(e.into_any_error()))?;

        context
            .export(EXPORTER_CONTEXT, cipher_suite.kdf_extract_size())
            .await
            .map(Zeroizing::new)
            .map(InitSecret)
            .map_err(|e| MlsError::CryptoProviderError(e.into_any_error()))
    }
}

pub(crate) struct WelcomeSecret<'a, P: CipherSuiteProvider> {
    cipher_suite: &'a P,
    key: Zeroizing<Vec<u8>>,
    nonce: Zeroizing<Vec<u8>>,
}

impl<'a, P: CipherSuiteProvider> WelcomeSecret<'a, P> {
    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub(crate) async fn from_joiner_secret(
        cipher_suite: &'a P,
        joiner_secret: &JoinerSecret,
        psk_secret: &PskSecret,
    ) -> Result<WelcomeSecret<'a, P>, MlsError> {
        let welcome_secret = get_welcome_secret(cipher_suite, joiner_secret, psk_secret).await?;

        let key_len = cipher_suite.aead_key_size();
        let key = kdf_expand_with_label(cipher_suite, &welcome_secret, b"key", &[], Some(key_len))
            .await?;

        let nonce_len = cipher_suite.aead_nonce_size();

        let nonce = kdf_expand_with_label(
            cipher_suite,
            &welcome_secret,
            b"nonce",
            &[],
            Some(nonce_len),
        )
        .await?;

        Ok(Self {
            cipher_suite,
            key,
            nonce,
        })
    }

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub(crate) async fn encrypt(&self, plaintext: &[u8]) -> Result<Vec<u8>, MlsError> {
        self.cipher_suite
            .aead_seal(&self.key, plaintext, None, &self.nonce)
            .await
            .map_err(|e| MlsError::CryptoProviderError(e.into_any_error()))
    }

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub(crate) async fn decrypt(&self, ciphertext: &[u8]) -> Result<Zeroizing<Vec<u8>>, MlsError> {
        self.cipher_suite
            .aead_open(&self.key, ciphertext, None, &self.nonce)
            .await
            .map_err(|e| MlsError::CryptoProviderError(e.into_any_error()))
    }
}

#[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
async fn get_welcome_secret<P: CipherSuiteProvider>(
    cipher_suite: &P,
    joiner_secret: &JoinerSecret,
    psk_secret: &PskSecret,
) -> Result<Zeroizing<Vec<u8>>, MlsError> {
    let epoch_seed = get_pre_epoch_secret(cipher_suite, psk_secret, joiner_secret).await?;
    kdf_derive_secret(cipher_suite, &epoch_seed, b"welcome").await
}

#[cfg(test)]
pub(crate) mod test_utils {
    use alloc::vec;
    use alloc::vec::Vec;
    use mls_rs_core::crypto::CipherSuiteProvider;
    use zeroize::Zeroizing;

    use crate::{cipher_suite::CipherSuite, crypto::test_utils::test_cipher_suite_provider};

    use super::{InitSecret, JoinerSecret, KeySchedule};

    #[cfg(all(feature = "rfc_compliant", not(mls_build_async)))]
    use mls_rs_core::error::IntoAnyError;

    #[cfg(all(feature = "rfc_compliant", not(mls_build_async)))]
    use super::MlsError;

    impl From<JoinerSecret> for Vec<u8> {
        fn from(mut value: JoinerSecret) -> Self {
            core::mem::take(&mut value.0)
        }
    }

    pub(crate) fn get_test_key_schedule(cipher_suite: CipherSuite) -> KeySchedule {
        let key_size = test_cipher_suite_provider(cipher_suite).kdf_extract_size();
        let fake_secret = Zeroizing::new(vec![1u8; key_size]);

        KeySchedule {
            exporter_secret: fake_secret.clone(),
            authentication_secret: fake_secret.clone(),
            external_secret: fake_secret.clone(),
            membership_key: fake_secret,
            init_secret: InitSecret::new(vec![0u8; key_size]),
        }
    }

    impl InitSecret {
        pub fn new(init_secret: Vec<u8>) -> Self {
            InitSecret(Zeroizing::new(init_secret))
        }

        #[cfg(all(feature = "rfc_compliant", test, not(mls_build_async)))]
        #[cfg_attr(coverage_nightly, coverage(off))]
        pub fn random<P: CipherSuiteProvider>(cipher_suite: &P) -> Result<Self, MlsError> {
            cipher_suite
                .random_bytes_vec(cipher_suite.kdf_extract_size())
                .map(Zeroizing::new)
                .map(InitSecret)
                .map_err(|e| MlsError::CryptoProviderError(e.into_any_error()))
        }
    }

    #[cfg(feature = "rfc_compliant")]
    impl KeySchedule {
        pub fn set_membership_key(&mut self, key: Vec<u8>) {
            self.membership_key = Zeroizing::new(key)
        }
    }
}

#[cfg(test)]
mod tests {
    use crate::client::test_utils::TEST_PROTOCOL_VERSION;
    use crate::crypto::test_utils::try_test_cipher_suite_provider;
    use crate::group::key_schedule::{
        get_welcome_secret, kdf_derive_secret, kdf_expand_with_label,
    };
    use crate::group::GroupContext;
    use alloc::string::String;
    use alloc::vec::Vec;
    use mls_rs_codec::MlsEncode;
    use mls_rs_core::crypto::CipherSuiteProvider;
    use mls_rs_core::extension::ExtensionList;

    #[cfg(all(not(mls_build_async), feature = "rfc_compliant"))]
    use crate::{
        crypto::test_utils::{test_cipher_suite_provider, TestCryptoProvider},
        group::{
            key_schedule::KeyScheduleDerivationResult, test_utils::random_bytes, InitSecret,
            PskSecret,
        },
    };

    #[cfg(all(not(mls_build_async), feature = "rfc_compliant"))]
    use alloc::{string::ToString, vec};

    #[cfg(target_arch = "wasm32")]
    use wasm_bindgen_test::wasm_bindgen_test as test;
    use zeroize::Zeroizing;

    use super::test_utils::get_test_key_schedule;
    use super::KeySchedule;

    #[derive(serde::Deserialize, serde::Serialize)]
    struct TestCase {
        cipher_suite: u16,
        #[serde(with = "hex::serde")]
        group_id: Vec<u8>,
        #[serde(with = "hex::serde")]
        initial_init_secret: Vec<u8>,
        epochs: Vec<KeyScheduleEpoch>,
    }

    #[derive(serde::Deserialize, serde::Serialize)]
    struct KeyScheduleEpoch {
        #[serde(with = "hex::serde")]
        commit_secret: Vec<u8>,
        #[serde(with = "hex::serde")]
        psk_secret: Vec<u8>,
        #[serde(with = "hex::serde")]
        confirmed_transcript_hash: Vec<u8>,
        #[serde(with = "hex::serde")]
        tree_hash: Vec<u8>,

        #[serde(with = "hex::serde")]
        group_context: Vec<u8>,

        #[serde(with = "hex::serde")]
        joiner_secret: Vec<u8>,
        #[serde(with = "hex::serde")]
        welcome_secret: Vec<u8>,
        #[serde(with = "hex::serde")]
        init_secret: Vec<u8>,

        #[serde(with = "hex::serde")]
        sender_data_secret: Vec<u8>,
        #[cfg(any(feature = "secret_tree_access", feature = "private_message"))]
        #[serde(with = "hex::serde")]
        encryption_secret: Vec<u8>,
        #[serde(with = "hex::serde")]
        exporter_secret: Vec<u8>,
        #[serde(with = "hex::serde")]
        epoch_authenticator: Vec<u8>,
        #[serde(with = "hex::serde")]
        external_secret: Vec<u8>,
        #[serde(with = "hex::serde")]
        confirmation_key: Vec<u8>,
        #[serde(with = "hex::serde")]
        membership_key: Vec<u8>,
        #[cfg(feature = "psk")]
        #[serde(with = "hex::serde")]
        resumption_psk: Vec<u8>,

        #[serde(with = "hex::serde")]
        external_pub: Vec<u8>,

        exporter: KeyScheduleExporter,
    }

    #[derive(serde::Deserialize, serde::Serialize)]
    struct KeyScheduleExporter {
        label: String,
        #[serde(with = "hex::serde")]
        context: Vec<u8>,
        length: usize,
        #[serde(with = "hex::serde")]
        secret: Vec<u8>,
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn test_key_schedule() {
        let test_cases: Vec<TestCase> =
            load_test_case_json!(key_schedule_test_vector, generate_test_vector());

        for test_case in test_cases {
            let Some(cs_provider) = try_test_cipher_suite_provider(test_case.cipher_suite) else {
                continue;
            };

            let mut key_schedule = get_test_key_schedule(cs_provider.cipher_suite());
            key_schedule.init_secret.0 = Zeroizing::new(test_case.initial_init_secret);

            for (i, epoch) in test_case.epochs.into_iter().enumerate() {
                let context = GroupContext {
                    protocol_version: TEST_PROTOCOL_VERSION,
                    cipher_suite: cs_provider.cipher_suite(),
                    group_id: test_case.group_id.clone(),
                    epoch: i as u64,
                    tree_hash: epoch.tree_hash,
                    confirmed_transcript_hash: epoch.confirmed_transcript_hash.into(),
                    extensions: ExtensionList::new(),
                };

                assert_eq!(context.mls_encode_to_vec().unwrap(), epoch.group_context);

                let psk = epoch.psk_secret.into();
                let commit = epoch.commit_secret.into();

                let key_schedule_res = KeySchedule::from_key_schedule(
                    &key_schedule,
                    &commit,
                    &context,
                    #[cfg(any(feature = "secret_tree_access", feature = "private_message"))]
                    32,
                    &psk,
                    &cs_provider,
                )
                .await
                .unwrap();

                key_schedule = key_schedule_res.key_schedule;

                let welcome =
                    get_welcome_secret(&cs_provider, &key_schedule_res.joiner_secret, &psk)
                        .await
                        .unwrap();

                assert_eq!(*welcome, epoch.welcome_secret);

                let expected: Vec<u8> = key_schedule_res.joiner_secret.into();
                assert_eq!(epoch.joiner_secret, expected);

                assert_eq!(&key_schedule.init_secret.0.to_vec(), &epoch.init_secret);

                assert_eq!(
                    epoch.sender_data_secret,
                    *key_schedule_res.epoch_secrets.sender_data_secret.to_vec()
                );

                #[cfg(any(feature = "secret_tree_access", feature = "private_message"))]
                assert_eq!(
                    epoch.encryption_secret,
                    *key_schedule_res.epoch_secrets.secret_tree.get_root_secret()
                );

                assert_eq!(epoch.exporter_secret, key_schedule.exporter_secret.to_vec());

                assert_eq!(
                    epoch.epoch_authenticator,
                    key_schedule.authentication_secret.to_vec()
                );

                assert_eq!(epoch.external_secret, key_schedule.external_secret.to_vec());

                assert_eq!(
                    epoch.confirmation_key,
                    key_schedule_res.confirmation_key.to_vec()
                );

                assert_eq!(epoch.membership_key, key_schedule.membership_key.to_vec());

                #[cfg(feature = "psk")]
                {
                    let expected: Vec<u8> =
                        key_schedule_res.epoch_secrets.resumption_secret.to_vec();

                    assert_eq!(epoch.resumption_psk, expected);
                }

                let (_external_sec, external_pub) = key_schedule
                    .get_external_key_pair(&cs_provider)
                    .await
                    .unwrap();

                assert_eq!(epoch.external_pub, *external_pub);

                let exp = epoch.exporter;

                let exported = key_schedule
                    .export_secret(exp.label.as_bytes(), &exp.context, exp.length, &cs_provider)
                    .await
                    .unwrap();

                assert_eq!(exported.to_vec(), exp.secret);
            }
        }
    }

    #[cfg(all(not(mls_build_async), feature = "rfc_compliant"))]
    #[cfg_attr(coverage_nightly, coverage(off))]
    fn generate_test_vector() -> Vec<TestCase> {
        let mut test_cases = vec![];

        for cipher_suite in TestCryptoProvider::all_supported_cipher_suites() {
            let cs_provider = test_cipher_suite_provider(cipher_suite);
            let key_size = cs_provider.kdf_extract_size();

            let mut group_context = GroupContext {
                protocol_version: TEST_PROTOCOL_VERSION,
                cipher_suite: cs_provider.cipher_suite(),
                group_id: b"my group 5".to_vec(),
                epoch: 0,
                tree_hash: random_bytes(key_size),
                confirmed_transcript_hash: random_bytes(key_size).into(),
                extensions: Default::default(),
            };

            let initial_init_secret = InitSecret::random(&cs_provider).unwrap();
            let mut key_schedule = get_test_key_schedule(cs_provider.cipher_suite());
            key_schedule.init_secret = initial_init_secret.clone();

            let commit_secret = random_bytes(key_size).into();
            let psk_secret = PskSecret::new(&cs_provider);

            let key_schedule_res = KeySchedule::from_key_schedule(
                &key_schedule,
                &commit_secret,
                &group_context,
                #[cfg(any(feature = "secret_tree_access", feature = "private_message"))]
                32,
                &psk_secret,
                &cs_provider,
            )
            .unwrap();

            key_schedule = key_schedule_res.key_schedule.clone();

            let epoch1 = KeyScheduleEpoch::new(
                key_schedule_res,
                psk_secret,
                commit_secret.to_vec(),
                &group_context,
                &cs_provider,
            );

            group_context.epoch += 1;
            group_context.confirmed_transcript_hash = random_bytes(key_size).into();
            group_context.tree_hash = random_bytes(key_size);

            let commit_secret = random_bytes(key_size).into();
            let psk_secret = PskSecret::new(&cs_provider);

            let key_schedule_res = KeySchedule::from_key_schedule(
                &key_schedule,
                &commit_secret,
                &group_context,
                #[cfg(any(feature = "secret_tree_access", feature = "private_message"))]
                32,
                &psk_secret,
                &cs_provider,
            )
            .unwrap();

            let epoch2 = KeyScheduleEpoch::new(
                key_schedule_res,
                psk_secret,
                commit_secret.to_vec(),
                &group_context,
                &cs_provider,
            );

            let test_case = TestCase {
                cipher_suite: cs_provider.cipher_suite().into(),
                group_id: group_context.group_id.clone(),
                initial_init_secret: initial_init_secret.0.to_vec(),
                epochs: vec![epoch1, epoch2],
            };

            test_cases.push(test_case);
        }

        test_cases
    }

    #[cfg(not(all(not(mls_build_async), feature = "rfc_compliant")))]
    fn generate_test_vector() -> Vec<TestCase> {
        panic!("Tests cannot be generated in async mode");
    }

    #[cfg(all(not(mls_build_async), feature = "rfc_compliant"))]
    impl KeyScheduleEpoch {
        #[cfg_attr(coverage_nightly, coverage(off))]
        fn new<P: CipherSuiteProvider>(
            key_schedule_res: KeyScheduleDerivationResult,
            psk_secret: PskSecret,
            commit_secret: Vec<u8>,
            group_context: &GroupContext,
            cs: &P,
        ) -> Self {
            let (_external_sec, external_pub) = key_schedule_res
                .key_schedule
                .get_external_key_pair(cs)
                .unwrap();

            let mut exporter = KeyScheduleExporter {
                label: "exporter label 15".to_string(),
                context: b"exporter context".to_vec(),
                length: 64,
                secret: vec![],
            };

            exporter.secret = key_schedule_res
                .key_schedule
                .export_secret(
                    exporter.label.as_bytes(),
                    &exporter.context,
                    exporter.length,
                    cs,
                )
                .unwrap()
                .to_vec();

            let welcome_secret =
                get_welcome_secret(cs, &key_schedule_res.joiner_secret, &psk_secret)
                    .unwrap()
                    .to_vec();

            KeyScheduleEpoch {
                commit_secret,
                welcome_secret,
                psk_secret: psk_secret.to_vec(),
                group_context: group_context.mls_encode_to_vec().unwrap(),
                joiner_secret: key_schedule_res.joiner_secret.into(),
                init_secret: key_schedule_res.key_schedule.init_secret.0.to_vec(),
                sender_data_secret: key_schedule_res.epoch_secrets.sender_data_secret.to_vec(),
                #[cfg(any(feature = "secret_tree_access", feature = "private_message"))]
                encryption_secret: key_schedule_res.epoch_secrets.secret_tree.get_root_secret(),
                exporter_secret: key_schedule_res.key_schedule.exporter_secret.to_vec(),
                epoch_authenticator: key_schedule_res.key_schedule.authentication_secret.to_vec(),
                external_secret: key_schedule_res.key_schedule.external_secret.to_vec(),
                confirmation_key: key_schedule_res.confirmation_key.to_vec(),
                membership_key: key_schedule_res.key_schedule.membership_key.to_vec(),
                #[cfg(feature = "psk")]
                resumption_psk: key_schedule_res.epoch_secrets.resumption_secret.to_vec(),
                external_pub: external_pub.to_vec(),
                exporter,
                confirmed_transcript_hash: group_context.confirmed_transcript_hash.to_vec(),
                tree_hash: group_context.tree_hash.clone(),
            }
        }
    }

    #[derive(Debug, serde::Serialize, serde::Deserialize)]
    struct ExpandWithLabelTestCase {
        #[serde(with = "hex::serde")]
        secret: Vec<u8>,
        label: String,
        #[serde(with = "hex::serde")]
        context: Vec<u8>,
        length: usize,
        #[serde(with = "hex::serde")]
        out: Vec<u8>,
    }

    #[derive(Debug, serde::Serialize, serde::Deserialize)]
    struct DeriveSecretTestCase {
        #[serde(with = "hex::serde")]
        secret: Vec<u8>,
        label: String,
        #[serde(with = "hex::serde")]
        out: Vec<u8>,
    }

    #[derive(Debug, serde::Serialize, serde::Deserialize)]
    pub struct InteropTestCase {
        cipher_suite: u16,
        expand_with_label: ExpandWithLabelTestCase,
        derive_secret: DeriveSecretTestCase,
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn test_basic_crypto_test_vectors() {
        // The test vector can be found here https://github.com/mlswg/mls-implementations/blob/main/test-vectors/crypto-basics.json
        let test_cases: Vec<InteropTestCase> =
            load_test_case_json!(basic_crypto, Vec::<InteropTestCase>::new());

        for test_case in test_cases {
            if let Some(cs) = try_test_cipher_suite_provider(test_case.cipher_suite) {
                let test_exp = &test_case.expand_with_label;

                let computed = kdf_expand_with_label(
                    &cs,
                    &test_exp.secret,
                    test_exp.label.as_bytes(),
                    &test_exp.context,
                    Some(test_exp.length),
                )
                .await
                .unwrap();

                assert_eq!(&computed.to_vec(), &test_exp.out);

                let test_derive = &test_case.derive_secret;

                let computed =
                    kdf_derive_secret(&cs, &test_derive.secret, test_derive.label.as_bytes())
                        .await
                        .unwrap();

                assert_eq!(&computed.to_vec(), &test_derive.out);
            }
        }
    }
}
