// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

//! Definitions to build an [`ExternalClient`].
//!
//! See [`ExternalClientBuilder`].

use crate::{
    crypto::SignaturePublicKey,
    extension::ExtensionType,
    external_client::{ExternalClient, ExternalClientConfig},
    group::{
        mls_rules::{DefaultMlsRules, MlsRules},
        proposal::ProposalType,
    },
    identity::CredentialType,
    protocol_version::ProtocolVersion,
    tree_kem::Capabilities,
    CryptoProvider, Sealed,
};
use std::{
    collections::HashMap,
    fmt::{self, Debug},
};

/// Base client configuration type when instantiating `ExternalClientBuilder`
pub type ExternalBaseConfig = Config<Missing, DefaultMlsRules, Missing>;

/// Builder for [`ExternalClient`]
///
/// This is returned by [`ExternalClient::builder`] and allows to tweak settings the
/// `ExternalClient` will use. At a minimum, the builder must be told the [`CryptoProvider`]
/// and [`IdentityProvider`] to use. Other settings have default values. This
/// means that the following methods must be called before [`ExternalClientBuilder::build`]:
///
/// - To specify the [`CryptoProvider`]: [`ExternalClientBuilder::crypto_provider`]
/// - To specify the [`IdentityProvider`]: [`ExternalClientBuilder::identity_provider`]
///
/// # Example
///
/// ```
/// use mls_rs::{
///     external_client::ExternalClient,
///     identity::basic::BasicIdentityProvider,
/// };
///
/// use mls_rs_crypto_openssl::OpensslCryptoProvider;
///
/// let _client = ExternalClient::builder()
///     .crypto_provider(OpensslCryptoProvider::default())
///     .identity_provider(BasicIdentityProvider::new())
///     .build();
/// ```
///
/// # Spelling out an `ExternalClient` type
///
/// There are two main ways to spell out an `ExternalClient` type if needed (e.g. function return type).
///
/// The first option uses `impl MlsConfig`:
/// ```
/// use mls_rs::{
///     external_client::{ExternalClient, builder::MlsConfig},
///     identity::basic::BasicIdentityProvider,
/// };
///
/// use mls_rs_crypto_openssl::OpensslCryptoProvider;
///
/// fn make_client() -> ExternalClient<impl MlsConfig> {
///     ExternalClient::builder()
///         .crypto_provider(OpensslCryptoProvider::default())
///         .identity_provider(BasicIdentityProvider::new())
///         .build()
/// }
///```
///
/// The second option is more verbose and consists in writing the full `ExternalClient` type:
/// ```
/// use mls_rs::{
///     external_client::{ExternalClient, builder::{ExternalBaseConfig, WithIdentityProvider, WithCryptoProvider}},
///     identity::basic::BasicIdentityProvider,
/// };
///
/// use mls_rs_crypto_openssl::OpensslCryptoProvider;
///
/// type MlsClient = ExternalClient<WithIdentityProvider<
///     BasicIdentityProvider,
///     WithCryptoProvider<OpensslCryptoProvider, ExternalBaseConfig>,
/// >>;
///
/// fn make_client_2() -> MlsClient {
///     ExternalClient::builder()
///         .crypto_provider(OpensslCryptoProvider::new())
///         .identity_provider(BasicIdentityProvider::new())
///         .build()
/// }
///
/// ```
#[derive(Debug)]
pub struct ExternalClientBuilder<C>(C);

impl Default for ExternalClientBuilder<ExternalBaseConfig> {
    fn default() -> Self {
        Self::new()
    }
}

impl ExternalClientBuilder<ExternalBaseConfig> {
    pub fn new() -> Self {
        Self(Config(ConfigInner {
            settings: Default::default(),
            identity_provider: Missing,
            mls_rules: DefaultMlsRules::new(),
            crypto_provider: Missing,
            signing_data: None,
        }))
    }
}

impl<C: IntoConfig> ExternalClientBuilder<C> {
    /// Add an extension type to the list of extension types supported by the client.
    pub fn extension_type(
        self,
        type_: ExtensionType,
    ) -> ExternalClientBuilder<IntoConfigOutput<C>> {
        self.extension_types(Some(type_))
    }

    /// Add multiple extension types to the list of extension types supported by the client.
    pub fn extension_types<I>(self, types: I) -> ExternalClientBuilder<IntoConfigOutput<C>>
    where
        I: IntoIterator<Item = ExtensionType>,
    {
        let mut c = self.0.into_config();
        c.0.settings.extension_types.extend(types);
        ExternalClientBuilder(c)
    }

    /// Add a custom proposal type to the list of proposals types supported by the client.
    pub fn custom_proposal_type(
        self,
        type_: ProposalType,
    ) -> ExternalClientBuilder<IntoConfigOutput<C>> {
        self.custom_proposal_types(Some(type_))
    }

    /// Add multiple custom proposal types to the list of proposal types supported by the client.
    pub fn custom_proposal_types<I>(self, types: I) -> ExternalClientBuilder<IntoConfigOutput<C>>
    where
        I: IntoIterator<Item = ProposalType>,
    {
        let mut c = self.0.into_config();
        c.0.settings.custom_proposal_types.extend(types);
        ExternalClientBuilder(c)
    }

    /// Add a protocol version to the list of protocol versions supported by the client.
    ///
    /// If no protocol version is explicitly added, the client will support all protocol versions
    /// supported by this crate.
    pub fn protocol_version(
        self,
        version: ProtocolVersion,
    ) -> ExternalClientBuilder<IntoConfigOutput<C>> {
        self.protocol_versions(Some(version))
    }

    /// Add multiple protocol versions to the list of protocol versions supported by the client.
    ///
    /// If no protocol version is explicitly added, the client will support all protocol versions
    /// supported by this crate.
    pub fn protocol_versions<I>(self, versions: I) -> ExternalClientBuilder<IntoConfigOutput<C>>
    where
        I: IntoIterator<Item = ProtocolVersion>,
    {
        let mut c = self.0.into_config();
        c.0.settings.protocol_versions.extend(versions);
        ExternalClientBuilder(c)
    }

    /// Add an external signing key to be used by the client.
    pub fn external_signing_key(
        self,
        id: Vec<u8>,
        key: SignaturePublicKey,
    ) -> ExternalClientBuilder<IntoConfigOutput<C>> {
        let mut c = self.0.into_config();
        c.0.settings.external_signing_keys.insert(id, key);
        ExternalClientBuilder(c)
    }

    /// Specify the number of epochs before the current one to keep.
    ///
    /// By default, all epochs are kept.
    pub fn max_epoch_jitter(self, max_jitter: u64) -> ExternalClientBuilder<IntoConfigOutput<C>> {
        let mut c = self.0.into_config();
        c.0.settings.max_epoch_jitter = Some(max_jitter);
        ExternalClientBuilder(c)
    }

    /// Specify whether processed proposals should be cached by the external group. In case they
    /// are not cached by the group, they should be cached externally and inserted using
    /// `ExternalGroup::insert_proposal` before processing the next commit.
    pub fn cache_proposals(
        self,
        cache_proposals: bool,
    ) -> ExternalClientBuilder<IntoConfigOutput<C>> {
        let mut c = self.0.into_config();
        c.0.settings.cache_proposals = cache_proposals;
        ExternalClientBuilder(c)
    }

    /// Set the identity validator to be used by the client.
    pub fn identity_provider<I>(
        self,
        identity_provider: I,
    ) -> ExternalClientBuilder<WithIdentityProvider<I, C>>
    where
        I: IdentityProvider,
    {
        let Config(c) = self.0.into_config();
        ExternalClientBuilder(Config(ConfigInner {
            settings: c.settings,
            identity_provider,
            mls_rules: c.mls_rules,
            crypto_provider: c.crypto_provider,
            signing_data: c.signing_data,
        }))
    }

    /// Set the crypto provider to be used by the client.
    ///
    // TODO add a comment once we have a default provider
    pub fn crypto_provider<Cp>(
        self,
        crypto_provider: Cp,
    ) -> ExternalClientBuilder<WithCryptoProvider<Cp, C>>
    where
        Cp: CryptoProvider,
    {
        let Config(c) = self.0.into_config();
        ExternalClientBuilder(Config(ConfigInner {
            settings: c.settings,
            identity_provider: c.identity_provider,
            mls_rules: c.mls_rules,
            crypto_provider,
            signing_data: c.signing_data,
        }))
    }

    /// Set the user-defined proposal rules to be used by the client.
    ///
    /// User-defined rules are used when sending and receiving commits before
    /// enforcing general MLS protocol rules. If the rule set returns an error when
    /// receiving a commit, the entire commit is considered invalid. If the
    /// rule set would return an error when sending a commit, individual proposals
    /// may be filtered out to compensate.
    pub fn mls_rules<Pr>(self, mls_rules: Pr) -> ExternalClientBuilder<WithMlsRules<Pr, C>>
    where
        Pr: MlsRules,
    {
        let Config(c) = self.0.into_config();
        ExternalClientBuilder(Config(ConfigInner {
            settings: c.settings,
            identity_provider: c.identity_provider,
            mls_rules,
            crypto_provider: c.crypto_provider,
            signing_data: c.signing_data,
        }))
    }

    /// Set the signature secret key used by the client to send external proposals.
    pub fn signer(
        self,
        signer: SignatureSecretKey,
        signing_identity: SigningIdentity,
    ) -> ExternalClientBuilder<IntoConfigOutput<C>> {
        let mut c = self.0.into_config();
        c.0.signing_data = Some((signer, signing_identity));
        ExternalClientBuilder(c)
    }
}

impl<C: IntoConfig> ExternalClientBuilder<C>
where
    C::IdentityProvider: IdentityProvider + Clone,
    C::MlsRules: MlsRules + Clone,
    C::CryptoProvider: CryptoProvider + Clone,
{
    pub(crate) fn build_config(self) -> IntoConfigOutput<C> {
        let mut c = self.0.into_config();

        if c.0.settings.protocol_versions.is_empty() {
            c.0.settings.protocol_versions = ProtocolVersion::all().collect();
        }

        c
    }

    /// Build an external client.
    ///
    /// See [`ExternalClientBuilder`] documentation if the return type of this function needs to be
    /// spelled out.
    pub fn build(self) -> ExternalClient<IntoConfigOutput<C>> {
        let mut c = self.build_config();
        let signing_data = c.0.signing_data.take();
        ExternalClient::new(c, signing_data)
    }
}

/// Marker type for required `ExternalClientBuilder` services that have not been specified yet.
#[derive(Debug)]
pub struct Missing;

/// Change the identity validator used by a client configuration.
///
/// See [`ExternalClientBuilder::identity_provider`].
pub type WithIdentityProvider<I, C> =
    Config<I, <C as IntoConfig>::MlsRules, <C as IntoConfig>::CryptoProvider>;

/// Change the proposal filter used by a client configuration.
///
/// See [`ExternalClientBuilder::mls_rules`].
pub type WithMlsRules<Pr, C> =
    Config<<C as IntoConfig>::IdentityProvider, Pr, <C as IntoConfig>::CryptoProvider>;

/// Change the crypto provider used by a client configuration.
///
/// See [`ExternalClientBuilder::crypto_provider`].
pub type WithCryptoProvider<Cp, C> =
    Config<<C as IntoConfig>::IdentityProvider, <C as IntoConfig>::MlsRules, Cp>;

/// Helper alias for `Config`.
pub type IntoConfigOutput<C> = Config<
    <C as IntoConfig>::IdentityProvider,
    <C as IntoConfig>::MlsRules,
    <C as IntoConfig>::CryptoProvider,
>;

impl<Ip, Pr, Cp> ExternalClientConfig for ConfigInner<Ip, Pr, Cp>
where
    Ip: IdentityProvider + Clone,
    Pr: MlsRules + Clone,
    Cp: CryptoProvider + Clone,
{
    type IdentityProvider = Ip;
    type MlsRules = Pr;
    type CryptoProvider = Cp;

    fn supported_extensions(&self) -> Vec<ExtensionType> {
        self.settings.extension_types.clone()
    }

    fn supported_protocol_versions(&self) -> Vec<ProtocolVersion> {
        self.settings.protocol_versions.clone()
    }

    fn identity_provider(&self) -> Self::IdentityProvider {
        self.identity_provider.clone()
    }

    fn crypto_provider(&self) -> Self::CryptoProvider {
        self.crypto_provider.clone()
    }

    fn external_signing_key(&self, external_key_id: &[u8]) -> Option<SignaturePublicKey> {
        self.settings
            .external_signing_keys
            .get(external_key_id)
            .cloned()
    }

    fn mls_rules(&self) -> Self::MlsRules {
        self.mls_rules.clone()
    }

    fn max_epoch_jitter(&self) -> Option<u64> {
        self.settings.max_epoch_jitter
    }

    fn cache_proposals(&self) -> bool {
        self.settings.cache_proposals
    }

    fn supported_custom_proposals(&self) -> Vec<ProposalType> {
        self.settings.custom_proposal_types.clone()
    }
}

impl<Ip, Mpf, Cp> Sealed for Config<Ip, Mpf, Cp> {}

impl<Ip, Pr, Cp> MlsConfig for Config<Ip, Pr, Cp>
where
    Ip: IdentityProvider + Clone,
    Pr: MlsRules + Clone,
    Cp: CryptoProvider + Clone,
{
    type Output = ConfigInner<Ip, Pr, Cp>;

    fn get(&self) -> &Self::Output {
        &self.0
    }
}

/// Helper trait to allow consuming crates to easily write an external client type as
/// `ExternalClient<impl MlsConfig>`
///
/// It is not meant to be implemented by consuming crates. `T: MlsConfig` implies
/// `T: ExternalClientConfig`.
pub trait MlsConfig: Send + Sync + Clone + Sealed {
    #[doc(hidden)]
    type Output: ExternalClientConfig;

    #[doc(hidden)]
    fn get(&self) -> &Self::Output;
}

/// Blanket implementation so that `T: MlsConfig` implies `T: ExternalClientConfig`
impl<T: MlsConfig> ExternalClientConfig for T {
    type IdentityProvider = <T::Output as ExternalClientConfig>::IdentityProvider;
    type MlsRules = <T::Output as ExternalClientConfig>::MlsRules;
    type CryptoProvider = <T::Output as ExternalClientConfig>::CryptoProvider;

    fn supported_extensions(&self) -> Vec<ExtensionType> {
        self.get().supported_extensions()
    }

    fn supported_protocol_versions(&self) -> Vec<ProtocolVersion> {
        self.get().supported_protocol_versions()
    }

    fn supported_custom_proposals(&self) -> Vec<ProposalType> {
        self.get().supported_custom_proposals()
    }

    fn identity_provider(&self) -> Self::IdentityProvider {
        self.get().identity_provider()
    }

    fn crypto_provider(&self) -> Self::CryptoProvider {
        self.get().crypto_provider()
    }

    fn external_signing_key(&self, external_key_id: &[u8]) -> Option<SignaturePublicKey> {
        self.get().external_signing_key(external_key_id)
    }

    fn mls_rules(&self) -> Self::MlsRules {
        self.get().mls_rules()
    }

    fn cache_proposals(&self) -> bool {
        self.get().cache_proposals()
    }

    fn max_epoch_jitter(&self) -> Option<u64> {
        self.get().max_epoch_jitter()
    }

    fn capabilities(&self) -> Capabilities {
        self.get().capabilities()
    }

    fn version_supported(&self, version: ProtocolVersion) -> bool {
        self.get().version_supported(version)
    }

    fn supported_credentials(&self) -> Vec<CredentialType> {
        self.get().supported_credentials()
    }
}

#[derive(Clone)]
pub(crate) struct Settings {
    pub(crate) extension_types: Vec<ExtensionType>,
    pub(crate) custom_proposal_types: Vec<ProposalType>,
    pub(crate) protocol_versions: Vec<ProtocolVersion>,
    pub(crate) external_signing_keys: HashMap<Vec<u8>, SignaturePublicKey>,
    pub(crate) max_epoch_jitter: Option<u64>,
    pub(crate) cache_proposals: bool,
}

impl Debug for Settings {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("Settings")
            .field("extension_types", &self.extension_types)
            .field("custom_proposal_types", &self.custom_proposal_types)
            .field("protocol_versions", &self.protocol_versions)
            .field(
                "external_signing_keys",
                &mls_rs_core::debug::pretty_with(|f| {
                    f.debug_map()
                        .entries(
                            self.external_signing_keys
                                .iter()
                                .map(|(k, v)| (mls_rs_core::debug::pretty_bytes(k), v)),
                        )
                        .finish()
                }),
            )
            .field("max_epoch_jitter", &self.max_epoch_jitter)
            .field("cache_proposals", &self.cache_proposals)
            .finish()
    }
}

impl Default for Settings {
    fn default() -> Self {
        Self {
            cache_proposals: true,
            extension_types: vec![],
            protocol_versions: vec![],
            external_signing_keys: Default::default(),
            max_epoch_jitter: None,
            custom_proposal_types: vec![],
        }
    }
}

/// Definitions meant to be private that are inaccessible outside this crate. They need to be marked
/// `pub` because they appear in public definitions.
mod private {
    use mls_rs_core::{crypto::SignatureSecretKey, identity::SigningIdentity};

    use super::{IntoConfigOutput, Settings};

    #[derive(Clone, Debug)]
    pub struct Config<Ip, Pr, Cp>(pub(crate) ConfigInner<Ip, Pr, Cp>);

    #[derive(Clone, Debug)]
    pub struct ConfigInner<Ip, Mpf, Cp> {
        pub(crate) settings: Settings,
        pub(crate) identity_provider: Ip,
        pub(crate) mls_rules: Mpf,
        pub(crate) crypto_provider: Cp,
        pub(crate) signing_data: Option<(SignatureSecretKey, SigningIdentity)>,
    }

    pub trait IntoConfig {
        type IdentityProvider;
        type MlsRules;
        type CryptoProvider;

        fn into_config(self) -> IntoConfigOutput<Self>;
    }

    impl<Ip, Pr, Cp> IntoConfig for Config<Ip, Pr, Cp> {
        type IdentityProvider = Ip;
        type MlsRules = Pr;
        type CryptoProvider = Cp;

        fn into_config(self) -> Self {
            self
        }
    }
}

use mls_rs_core::{
    crypto::SignatureSecretKey,
    identity::{IdentityProvider, SigningIdentity},
};
use private::{Config, ConfigInner, IntoConfig};

#[cfg(test)]
pub(crate) mod test_utils {
    use crate::{
        cipher_suite::CipherSuite, crypto::test_utils::TestCryptoProvider,
        identity::basic::BasicIdentityProvider,
    };

    use super::{
        ExternalBaseConfig, ExternalClientBuilder, WithCryptoProvider, WithIdentityProvider,
    };

    pub type TestExternalClientConfig = WithIdentityProvider<
        BasicIdentityProvider,
        WithCryptoProvider<TestCryptoProvider, ExternalBaseConfig>,
    >;

    pub type TestExternalClientBuilder = ExternalClientBuilder<TestExternalClientConfig>;

    impl TestExternalClientBuilder {
        pub fn new_for_test() -> Self {
            ExternalClientBuilder::new()
                .crypto_provider(TestCryptoProvider::default())
                .identity_provider(BasicIdentityProvider::new())
        }

        pub fn new_for_test_disabling_cipher_suite(cipher_suite: CipherSuite) -> Self {
            let crypto_provider = TestCryptoProvider::with_enabled_cipher_suites(
                TestCryptoProvider::all_supported_cipher_suites()
                    .into_iter()
                    .filter(|cs| cs != &cipher_suite)
                    .collect(),
            );

            ExternalClientBuilder::new()
                .crypto_provider(crypto_provider)
                .identity_provider(BasicIdentityProvider::new())
        }
    }
}
