// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

//! Definitions to build a [`Client`].
//!
//! See [`ClientBuilder`].

use crate::{
    cipher_suite::CipherSuite,
    client::Client,
    client_config::ClientConfig,
    extension::{ExtensionType, MlsExtension},
    group::{
        mls_rules::{DefaultMlsRules, MlsRules},
        proposal::ProposalType,
    },
    identity::CredentialType,
    identity::SigningIdentity,
    protocol_version::ProtocolVersion,
    psk::{ExternalPskId, PreSharedKey},
    storage_provider::in_memory::{
        InMemoryGroupStateStorage, InMemoryKeyPackageStorage, InMemoryPreSharedKeyStorage,
    },
    tree_kem::{Capabilities, Lifetime},
    Sealed,
};

#[cfg(feature = "std")]
use crate::time::MlsTime;

use alloc::vec::Vec;

#[cfg(feature = "sqlite")]
use mls_rs_provider_sqlite::{
    SqLiteDataStorageEngine, SqLiteDataStorageError,
    {
        connection_strategy::ConnectionStrategy,
        storage::{SqLiteGroupStateStorage, SqLiteKeyPackageStorage, SqLitePreSharedKeyStorage},
    },
};

#[cfg(feature = "private_message")]
pub use crate::group::padding::PaddingMode;

/// Base client configuration type when instantiating `ClientBuilder`
pub type BaseConfig = Config<
    InMemoryKeyPackageStorage,
    InMemoryPreSharedKeyStorage,
    InMemoryGroupStateStorage,
    Missing,
    DefaultMlsRules,
    Missing,
>;

/// Base client configuration type when instantiating `ClientBuilder`
pub type BaseInMemoryConfig = Config<
    InMemoryKeyPackageStorage,
    InMemoryPreSharedKeyStorage,
    InMemoryGroupStateStorage,
    Missing,
    Missing,
    Missing,
>;

pub type EmptyConfig = Config<Missing, Missing, Missing, Missing, Missing, Missing>;

/// Base client configuration that is backed by SQLite storage.
#[cfg(feature = "sqlite")]
pub type BaseSqlConfig = Config<
    SqLiteKeyPackageStorage,
    SqLitePreSharedKeyStorage,
    SqLiteGroupStateStorage,
    Missing,
    DefaultMlsRules,
    Missing,
>;

/// Builder for [`Client`]
///
/// This is returned by [`Client::builder`] and allows to tweak settings the `Client` will use. At a
/// minimum, the builder must be told the [`CryptoProvider`] and [`IdentityProvider`] to use. Other
/// settings have default values. This means that the following
/// methods must be called before [`ClientBuilder::build`]:
///
/// - To specify the [`CryptoProvider`]: [`ClientBuilder::crypto_provider`]
/// - To specify the [`IdentityProvider`]: [`ClientBuilder::identity_provider`]
///
/// # Example
///
/// ```
/// use mls_rs::{
///     Client,
///     identity::{SigningIdentity, basic::{BasicIdentityProvider, BasicCredential}},
///     CipherSuite,
/// };
///
/// use mls_rs_crypto_openssl::OpensslCryptoProvider;
///
/// // Replace by code to load the certificate and secret key
/// let secret_key = b"never hard-code secrets".to_vec().into();
/// let public_key = b"test invalid public key".to_vec().into();
/// let basic_identity = BasicCredential::new(b"name".to_vec());
/// let signing_identity = SigningIdentity::new(basic_identity.into_credential(), public_key);
///
///
/// let _client = Client::builder()
///     .crypto_provider(OpensslCryptoProvider::default())
///     .identity_provider(BasicIdentityProvider::new())
///     .signing_identity(signing_identity, secret_key, CipherSuite::CURVE25519_AES128)
///     .build();
/// ```
///
/// # Spelling out a `Client` type
///
/// There are two main ways to spell out a `Client` type if needed (e.g. function return type).
///
/// The first option uses `impl MlsConfig`:
/// ```
/// use mls_rs::{
///     Client,
///     client_builder::MlsConfig,
///     identity::{SigningIdentity, basic::{BasicIdentityProvider, BasicCredential}},
///     CipherSuite,
/// };
///
/// use mls_rs_crypto_openssl::OpensslCryptoProvider;
///
/// fn make_client() -> Client<impl MlsConfig> {
///     // Replace by code to load the certificate and secret key
///     let secret_key = b"never hard-code secrets".to_vec().into();
///     let public_key = b"test invalid public key".to_vec().into();
///     let basic_identity = BasicCredential::new(b"name".to_vec());
///     let signing_identity = SigningIdentity::new(basic_identity.into_credential(), public_key);
///
///     Client::builder()
///         .crypto_provider(OpensslCryptoProvider::default())
///         .identity_provider(BasicIdentityProvider::new())
///         .signing_identity(signing_identity, secret_key, CipherSuite::CURVE25519_AES128)
///         .build()
/// }
///```
///
/// The second option is more verbose and consists in writing the full `Client` type:
/// ```
/// use mls_rs::{
///     Client,
///     client_builder::{BaseConfig, WithIdentityProvider, WithCryptoProvider},
///     identity::{SigningIdentity, basic::{BasicIdentityProvider, BasicCredential}},
///     CipherSuite,
/// };
///
/// use mls_rs_crypto_openssl::OpensslCryptoProvider;
///
/// type MlsClient = Client<
///     WithIdentityProvider<
///         BasicIdentityProvider,
///         WithCryptoProvider<OpensslCryptoProvider, BaseConfig>,
///     >,
/// >;
///
/// fn make_client_2() -> MlsClient {
///     // Replace by code to load the certificate and secret key
///     let secret_key = b"never hard-code secrets".to_vec().into();
///     let public_key = b"test invalid public key".to_vec().into();
///     let basic_identity = BasicCredential::new(b"name".to_vec());
///     let signing_identity = SigningIdentity::new(basic_identity.into_credential(), public_key);
///
///     Client::builder()
///         .crypto_provider(OpensslCryptoProvider::default())
///         .identity_provider(BasicIdentityProvider::new())
///         .signing_identity(signing_identity, secret_key, CipherSuite::CURVE25519_AES128)
///         .build()
/// }
///
/// ```
#[derive(Debug)]
pub struct ClientBuilder<C>(C);

impl Default for ClientBuilder<BaseConfig> {
    fn default() -> Self {
        Self::new()
    }
}

impl<C> ClientBuilder<C> {
    pub(crate) fn from_config(c: C) -> Self {
        Self(c)
    }
}

impl ClientBuilder<BaseConfig> {
    /// Create a new client builder with default in-memory providers
    pub fn new() -> Self {
        Self(Config(ConfigInner {
            settings: Default::default(),
            key_package_repo: Default::default(),
            psk_store: Default::default(),
            group_state_storage: Default::default(),
            identity_provider: Missing,
            mls_rules: DefaultMlsRules::new(),
            crypto_provider: Missing,
            signer: Default::default(),
            signing_identity: Default::default(),
            version: ProtocolVersion::MLS_10,
        }))
    }
}

impl ClientBuilder<EmptyConfig> {
    pub fn new_empty() -> Self {
        Self(Config(ConfigInner {
            settings: Default::default(),
            key_package_repo: Missing,
            psk_store: Missing,
            group_state_storage: Missing,
            identity_provider: Missing,
            mls_rules: Missing,
            crypto_provider: Missing,
            signer: Default::default(),
            signing_identity: Default::default(),
            version: ProtocolVersion::MLS_10,
        }))
    }
}

#[cfg(feature = "sqlite")]
impl ClientBuilder<BaseSqlConfig> {
    /// Create a new client builder with SQLite storage providers.
    pub fn new_sqlite<CS: ConnectionStrategy>(
        storage: SqLiteDataStorageEngine<CS>,
    ) -> Result<Self, SqLiteDataStorageError> {
        Ok(Self(Config(ConfigInner {
            settings: Default::default(),
            key_package_repo: storage.key_package_storage()?,
            psk_store: storage.pre_shared_key_storage()?,
            group_state_storage: storage.group_state_storage()?,
            identity_provider: Missing,
            mls_rules: DefaultMlsRules::new(),
            crypto_provider: Missing,
            signer: Default::default(),
            signing_identity: Default::default(),
            version: ProtocolVersion::MLS_10,
        })))
    }
}

impl<C: IntoConfig> ClientBuilder<C> {
    /// Add an extension type to the list of extension types supported by the client.
    pub fn extension_type(self, type_: ExtensionType) -> ClientBuilder<IntoConfigOutput<C>> {
        self.extension_types(Some(type_))
    }

    /// Add multiple extension types to the list of extension types supported by the client.
    pub fn extension_types<I>(self, types: I) -> ClientBuilder<IntoConfigOutput<C>>
    where
        I: IntoIterator<Item = ExtensionType>,
    {
        let mut c = self.0.into_config();
        c.0.settings.extension_types.extend(types);
        ClientBuilder(c)
    }

    /// Add a custom proposal type to the list of proposals types supported by the client.
    pub fn custom_proposal_type(self, type_: ProposalType) -> ClientBuilder<IntoConfigOutput<C>> {
        self.custom_proposal_types(Some(type_))
    }

    /// Add multiple custom proposal types to the list of proposal types supported by the client.
    pub fn custom_proposal_types<I>(self, types: I) -> ClientBuilder<IntoConfigOutput<C>>
    where
        I: IntoIterator<Item = ProposalType>,
    {
        let mut c = self.0.into_config();
        c.0.settings.custom_proposal_types.extend(types);
        ClientBuilder(c)
    }

    /// Add a protocol version to the list of protocol versions supported by the client.
    ///
    /// If no protocol version is explicitly added, the client will support all protocol versions
    /// supported by this crate.
    pub fn protocol_version(self, version: ProtocolVersion) -> ClientBuilder<IntoConfigOutput<C>> {
        self.protocol_versions(Some(version))
    }

    /// Add multiple protocol versions to the list of protocol versions supported by the client.
    ///
    /// If no protocol version is explicitly added, the client will support all protocol versions
    /// supported by this crate.
    pub fn protocol_versions<I>(self, versions: I) -> ClientBuilder<IntoConfigOutput<C>>
    where
        I: IntoIterator<Item = ProtocolVersion>,
    {
        let mut c = self.0.into_config();
        c.0.settings.protocol_versions.extend(versions);
        ClientBuilder(c)
    }

    /// Add a key package extension to the list of key package extensions supported by the client.
    pub fn key_package_extension<T>(
        self,
        extension: T,
    ) -> Result<ClientBuilder<IntoConfigOutput<C>>, ExtensionError>
    where
        T: MlsExtension,
        Self: Sized,
    {
        let mut c = self.0.into_config();
        c.0.settings.key_package_extensions.set_from(extension)?;
        Ok(ClientBuilder(c))
    }

    /// Add multiple key package extensions to the list of key package extensions supported by the
    /// client.
    pub fn key_package_extensions(
        self,
        extensions: ExtensionList,
    ) -> ClientBuilder<IntoConfigOutput<C>> {
        let mut c = self.0.into_config();
        c.0.settings.key_package_extensions.append(extensions);
        ClientBuilder(c)
    }

    /// Add a leaf node extension to the list of leaf node extensions supported by the client.
    pub fn leaf_node_extension<T>(
        self,
        extension: T,
    ) -> Result<ClientBuilder<IntoConfigOutput<C>>, ExtensionError>
    where
        T: MlsExtension,
        Self: Sized,
    {
        let mut c = self.0.into_config();
        c.0.settings.leaf_node_extensions.set_from(extension)?;
        Ok(ClientBuilder(c))
    }

    /// Add multiple leaf node extensions to the list of leaf node extensions supported by the
    /// client.
    pub fn leaf_node_extensions(
        self,
        extensions: ExtensionList,
    ) -> ClientBuilder<IntoConfigOutput<C>> {
        let mut c = self.0.into_config();
        c.0.settings.leaf_node_extensions.append(extensions);
        ClientBuilder(c)
    }

    /// Set the lifetime duration in seconds of key packages generated by the client.
    pub fn key_package_lifetime(self, duration_in_s: u64) -> ClientBuilder<IntoConfigOutput<C>> {
        let mut c = self.0.into_config();
        c.0.settings.lifetime_in_s = duration_in_s;
        ClientBuilder(c)
    }

    /// Set the key package repository to be used by the client.
    ///
    /// By default, an in-memory repository is used.
    pub fn key_package_repo<K>(self, key_package_repo: K) -> ClientBuilder<WithKeyPackageRepo<K, C>>
    where
        K: KeyPackageStorage,
    {
        let Config(c) = self.0.into_config();

        ClientBuilder(Config(ConfigInner {
            settings: c.settings,
            key_package_repo,
            psk_store: c.psk_store,
            group_state_storage: c.group_state_storage,
            identity_provider: c.identity_provider,
            mls_rules: c.mls_rules,
            crypto_provider: c.crypto_provider,
            signer: c.signer,
            signing_identity: c.signing_identity,
            version: c.version,
        }))
    }

    /// Set the PSK store to be used by the client.
    ///
    /// By default, an in-memory store is used.
    pub fn psk_store<P>(self, psk_store: P) -> ClientBuilder<WithPskStore<P, C>>
    where
        P: PreSharedKeyStorage,
    {
        let Config(c) = self.0.into_config();

        ClientBuilder(Config(ConfigInner {
            settings: c.settings,
            key_package_repo: c.key_package_repo,
            psk_store,
            group_state_storage: c.group_state_storage,
            identity_provider: c.identity_provider,
            mls_rules: c.mls_rules,
            crypto_provider: c.crypto_provider,
            signer: c.signer,
            signing_identity: c.signing_identity,
            version: c.version,
        }))
    }

    /// Set the group state storage to be used by the client.
    ///
    /// By default, an in-memory storage is used.
    pub fn group_state_storage<G>(
        self,
        group_state_storage: G,
    ) -> ClientBuilder<WithGroupStateStorage<G, C>>
    where
        G: GroupStateStorage,
    {
        let Config(c) = self.0.into_config();

        ClientBuilder(Config(ConfigInner {
            settings: c.settings,
            key_package_repo: c.key_package_repo,
            psk_store: c.psk_store,
            group_state_storage,
            identity_provider: c.identity_provider,
            crypto_provider: c.crypto_provider,
            mls_rules: c.mls_rules,
            signer: c.signer,
            signing_identity: c.signing_identity,
            version: c.version,
        }))
    }

    /// Set the identity validator to be used by the client.
    pub fn identity_provider<I>(
        self,
        identity_provider: I,
    ) -> ClientBuilder<WithIdentityProvider<I, C>>
    where
        I: IdentityProvider,
    {
        let Config(c) = self.0.into_config();

        ClientBuilder(Config(ConfigInner {
            settings: c.settings,
            key_package_repo: c.key_package_repo,
            psk_store: c.psk_store,
            group_state_storage: c.group_state_storage,
            identity_provider,
            mls_rules: c.mls_rules,
            crypto_provider: c.crypto_provider,
            signer: c.signer,
            signing_identity: c.signing_identity,
            version: c.version,
        }))
    }

    /// Set the crypto provider to be used by the client.
    pub fn crypto_provider<Cp>(
        self,
        crypto_provider: Cp,
    ) -> ClientBuilder<WithCryptoProvider<Cp, C>>
    where
        Cp: CryptoProvider,
    {
        let Config(c) = self.0.into_config();

        ClientBuilder(Config(ConfigInner {
            settings: c.settings,
            key_package_repo: c.key_package_repo,
            psk_store: c.psk_store,
            group_state_storage: c.group_state_storage,
            identity_provider: c.identity_provider,
            mls_rules: c.mls_rules,
            crypto_provider,
            signer: c.signer,
            signing_identity: c.signing_identity,
            version: c.version,
        }))
    }

    /// Set the user-defined proposal rules to be used by the client.
    ///
    /// User-defined rules are used when sending and receiving commits before
    /// enforcing general MLS protocol rules. If the rule set returns an error when
    /// receiving a commit, the entire commit is considered invalid. If the
    /// rule set would return an error when sending a commit, individual proposals
    /// may be filtered out to compensate.
    pub fn mls_rules<Pr>(self, mls_rules: Pr) -> ClientBuilder<WithMlsRules<Pr, C>>
    where
        Pr: MlsRules,
    {
        let Config(c) = self.0.into_config();

        ClientBuilder(Config(ConfigInner {
            settings: c.settings,
            key_package_repo: c.key_package_repo,
            psk_store: c.psk_store,
            group_state_storage: c.group_state_storage,
            identity_provider: c.identity_provider,
            mls_rules,
            crypto_provider: c.crypto_provider,
            signer: c.signer,
            signing_identity: c.signing_identity,
            version: c.version,
        }))
    }

    /// Set the protocol version used by the client. By default, the client uses version MLS 1.0
    pub fn used_protocol_version(
        self,
        version: ProtocolVersion,
    ) -> ClientBuilder<IntoConfigOutput<C>> {
        let mut c = self.0.into_config();
        c.0.version = version;
        ClientBuilder(c)
    }

    /// Set the signing identity used by the client as well as the matching signer and cipher suite.
    /// This must be called in order to create groups and key packages.
    pub fn signing_identity(
        self,
        signing_identity: SigningIdentity,
        signer: SignatureSecretKey,
        cipher_suite: CipherSuite,
    ) -> ClientBuilder<IntoConfigOutput<C>> {
        let mut c = self.0.into_config();
        c.0.signer = Some(signer);
        c.0.signing_identity = Some((signing_identity, cipher_suite));
        ClientBuilder(c)
    }

    /// Set the signer used by the client. This must be called in order to join groups.
    pub fn signer(self, signer: SignatureSecretKey) -> ClientBuilder<IntoConfigOutput<C>> {
        let mut c = self.0.into_config();
        c.0.signer = Some(signer);
        ClientBuilder(c)
    }

    #[cfg(any(test, feature = "test_util"))]
    pub(crate) fn key_package_not_before(
        self,
        key_package_not_before: u64,
    ) -> ClientBuilder<IntoConfigOutput<C>> {
        let mut c = self.0.into_config();
        c.0.settings.key_package_not_before = Some(key_package_not_before);
        ClientBuilder(c)
    }
}

impl<C: IntoConfig> ClientBuilder<C>
where
    C::KeyPackageRepository: KeyPackageStorage + Clone,
    C::PskStore: PreSharedKeyStorage + Clone,
    C::GroupStateStorage: GroupStateStorage + Clone,
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

    /// Build a client.
    ///
    /// See [`ClientBuilder`] documentation if the return type of this function needs to be spelled
    /// out.
    pub fn build(self) -> Client<IntoConfigOutput<C>> {
        let mut c = self.build_config();
        let version = c.0.version;
        let signer = c.0.signer.take();
        let signing_identity = c.0.signing_identity.take();

        Client::new(c, signer, signing_identity, version)
    }
}

impl<C: IntoConfig<PskStore = InMemoryPreSharedKeyStorage>> ClientBuilder<C> {
    /// Add a PSK to the in-memory PSK store.
    pub fn psk(
        self,
        psk_id: ExternalPskId,
        psk: PreSharedKey,
    ) -> ClientBuilder<IntoConfigOutput<C>> {
        let mut c = self.0.into_config();
        c.0.psk_store.insert(psk_id, psk);
        ClientBuilder(c)
    }
}

/// Marker type for required `ClientBuilder` services that have not been specified yet.
#[derive(Debug)]
pub struct Missing;

/// Change the key package repository used by a client configuration.
///
/// See [`ClientBuilder::key_package_repo`].
pub type WithKeyPackageRepo<K, C> = Config<
    K,
    <C as IntoConfig>::PskStore,
    <C as IntoConfig>::GroupStateStorage,
    <C as IntoConfig>::IdentityProvider,
    <C as IntoConfig>::MlsRules,
    <C as IntoConfig>::CryptoProvider,
>;

/// Change the PSK store used by a client configuration.
///
/// See [`ClientBuilder::psk_store`].
pub type WithPskStore<P, C> = Config<
    <C as IntoConfig>::KeyPackageRepository,
    P,
    <C as IntoConfig>::GroupStateStorage,
    <C as IntoConfig>::IdentityProvider,
    <C as IntoConfig>::MlsRules,
    <C as IntoConfig>::CryptoProvider,
>;

/// Change the group state storage used by a client configuration.
///
/// See [`ClientBuilder::group_state_storage`].
pub type WithGroupStateStorage<G, C> = Config<
    <C as IntoConfig>::KeyPackageRepository,
    <C as IntoConfig>::PskStore,
    G,
    <C as IntoConfig>::IdentityProvider,
    <C as IntoConfig>::MlsRules,
    <C as IntoConfig>::CryptoProvider,
>;

/// Change the identity validator used by a client configuration.
///
/// See [`ClientBuilder::identity_provider`].
pub type WithIdentityProvider<I, C> = Config<
    <C as IntoConfig>::KeyPackageRepository,
    <C as IntoConfig>::PskStore,
    <C as IntoConfig>::GroupStateStorage,
    I,
    <C as IntoConfig>::MlsRules,
    <C as IntoConfig>::CryptoProvider,
>;

/// Change the proposal rules used by a client configuration.
///
/// See [`ClientBuilder::mls_rules`].
pub type WithMlsRules<Pr, C> = Config<
    <C as IntoConfig>::KeyPackageRepository,
    <C as IntoConfig>::PskStore,
    <C as IntoConfig>::GroupStateStorage,
    <C as IntoConfig>::IdentityProvider,
    Pr,
    <C as IntoConfig>::CryptoProvider,
>;

/// Change the crypto provider used by a client configuration.
///
/// See [`ClientBuilder::crypto_provider`].
pub type WithCryptoProvider<Cp, C> = Config<
    <C as IntoConfig>::KeyPackageRepository,
    <C as IntoConfig>::PskStore,
    <C as IntoConfig>::GroupStateStorage,
    <C as IntoConfig>::IdentityProvider,
    <C as IntoConfig>::MlsRules,
    Cp,
>;

/// Helper alias for `Config`.
pub type IntoConfigOutput<C> = Config<
    <C as IntoConfig>::KeyPackageRepository,
    <C as IntoConfig>::PskStore,
    <C as IntoConfig>::GroupStateStorage,
    <C as IntoConfig>::IdentityProvider,
    <C as IntoConfig>::MlsRules,
    <C as IntoConfig>::CryptoProvider,
>;

/// Helper alias to make a `Config` from a `ClientConfig`
pub type MakeConfig<C> = Config<
    <C as ClientConfig>::KeyPackageRepository,
    <C as ClientConfig>::PskStore,
    <C as ClientConfig>::GroupStateStorage,
    <C as ClientConfig>::IdentityProvider,
    <C as ClientConfig>::MlsRules,
    <C as ClientConfig>::CryptoProvider,
>;

impl<Kpr, Ps, Gss, Ip, Pr, Cp> ClientConfig for ConfigInner<Kpr, Ps, Gss, Ip, Pr, Cp>
where
    Kpr: KeyPackageStorage + Clone,
    Ps: PreSharedKeyStorage + Clone,
    Gss: GroupStateStorage + Clone,
    Ip: IdentityProvider + Clone,
    Pr: MlsRules + Clone,
    Cp: CryptoProvider + Clone,
{
    type KeyPackageRepository = Kpr;
    type PskStore = Ps;
    type GroupStateStorage = Gss;
    type IdentityProvider = Ip;
    type MlsRules = Pr;
    type CryptoProvider = Cp;

    fn supported_extensions(&self) -> Vec<ExtensionType> {
        self.settings.extension_types.clone()
    }

    fn supported_protocol_versions(&self) -> Vec<ProtocolVersion> {
        self.settings.protocol_versions.clone()
    }

    fn key_package_repo(&self) -> Self::KeyPackageRepository {
        self.key_package_repo.clone()
    }

    fn mls_rules(&self) -> Self::MlsRules {
        self.mls_rules.clone()
    }

    fn secret_store(&self) -> Self::PskStore {
        self.psk_store.clone()
    }

    fn group_state_storage(&self) -> Self::GroupStateStorage {
        self.group_state_storage.clone()
    }

    fn identity_provider(&self) -> Self::IdentityProvider {
        self.identity_provider.clone()
    }

    fn crypto_provider(&self) -> Self::CryptoProvider {
        self.crypto_provider.clone()
    }

    fn key_package_extensions(&self) -> ExtensionList {
        self.settings.key_package_extensions.clone()
    }

    fn leaf_node_extensions(&self) -> ExtensionList {
        self.settings.leaf_node_extensions.clone()
    }

    fn lifetime(&self) -> Lifetime {
        #[cfg(feature = "std")]
        let now_timestamp = MlsTime::now().seconds_since_epoch();

        #[cfg(not(feature = "std"))]
        let now_timestamp = 0;

        #[cfg(test)]
        let now_timestamp = self
            .settings
            .key_package_not_before
            .unwrap_or(now_timestamp);

        Lifetime {
            not_before: now_timestamp,
            not_after: now_timestamp + self.settings.lifetime_in_s,
        }
    }

    fn supported_custom_proposals(&self) -> Vec<crate::group::proposal::ProposalType> {
        self.settings.custom_proposal_types.clone()
    }
}

impl<Kpr, Ps, Gss, Ip, Pr, Cp> Sealed for Config<Kpr, Ps, Gss, Ip, Pr, Cp> {}

impl<Kpr, Ps, Gss, Ip, Pr, Cp> MlsConfig for Config<Kpr, Ps, Gss, Ip, Pr, Cp>
where
    Kpr: KeyPackageStorage + Clone,

    Ps: PreSharedKeyStorage + Clone,
    Gss: GroupStateStorage + Clone,
    Ip: IdentityProvider + Clone,
    Pr: MlsRules + Clone,
    Cp: CryptoProvider + Clone,
{
    type Output = ConfigInner<Kpr, Ps, Gss, Ip, Pr, Cp>;

    fn get(&self) -> &Self::Output {
        &self.0
    }
}

/// Helper trait to allow consuming crates to easily write a client type as `Client<impl MlsConfig>`
///
/// It is not meant to be implemented by consuming crates. `T: MlsConfig` implies `T: ClientConfig`.
pub trait MlsConfig: Clone + Send + Sync + Sealed {
    #[doc(hidden)]
    type Output: ClientConfig;

    #[doc(hidden)]
    fn get(&self) -> &Self::Output;
}

/// Blanket implementation so that `T: MlsConfig` implies `T: ClientConfig`
impl<T: MlsConfig> ClientConfig for T {
    type KeyPackageRepository = <T::Output as ClientConfig>::KeyPackageRepository;
    type PskStore = <T::Output as ClientConfig>::PskStore;
    type GroupStateStorage = <T::Output as ClientConfig>::GroupStateStorage;
    type IdentityProvider = <T::Output as ClientConfig>::IdentityProvider;
    type MlsRules = <T::Output as ClientConfig>::MlsRules;
    type CryptoProvider = <T::Output as ClientConfig>::CryptoProvider;

    fn supported_extensions(&self) -> Vec<ExtensionType> {
        self.get().supported_extensions()
    }

    fn supported_custom_proposals(&self) -> Vec<ProposalType> {
        self.get().supported_custom_proposals()
    }

    fn supported_protocol_versions(&self) -> Vec<ProtocolVersion> {
        self.get().supported_protocol_versions()
    }

    fn key_package_repo(&self) -> Self::KeyPackageRepository {
        self.get().key_package_repo()
    }

    fn mls_rules(&self) -> Self::MlsRules {
        self.get().mls_rules()
    }

    fn secret_store(&self) -> Self::PskStore {
        self.get().secret_store()
    }

    fn group_state_storage(&self) -> Self::GroupStateStorage {
        self.get().group_state_storage()
    }

    fn identity_provider(&self) -> Self::IdentityProvider {
        self.get().identity_provider()
    }

    fn crypto_provider(&self) -> Self::CryptoProvider {
        self.get().crypto_provider()
    }

    fn key_package_extensions(&self) -> ExtensionList {
        self.get().key_package_extensions()
    }

    fn leaf_node_extensions(&self) -> ExtensionList {
        self.get().leaf_node_extensions()
    }

    fn lifetime(&self) -> Lifetime {
        self.get().lifetime()
    }

    fn capabilities(&self) -> Capabilities {
        self.get().capabilities()
    }

    fn version_supported(&self, version: ProtocolVersion) -> bool {
        self.get().version_supported(version)
    }

    fn supported_credential_types(&self) -> Vec<CredentialType> {
        self.get().supported_credential_types()
    }
}

#[derive(Clone, Debug)]
pub(crate) struct Settings {
    pub(crate) extension_types: Vec<ExtensionType>,
    pub(crate) protocol_versions: Vec<ProtocolVersion>,
    pub(crate) custom_proposal_types: Vec<ProposalType>,
    pub(crate) key_package_extensions: ExtensionList,
    pub(crate) leaf_node_extensions: ExtensionList,
    pub(crate) lifetime_in_s: u64,
    #[cfg(any(test, feature = "test_util"))]
    pub(crate) key_package_not_before: Option<u64>,
}

impl Default for Settings {
    fn default() -> Self {
        Self {
            extension_types: Default::default(),
            protocol_versions: Default::default(),
            key_package_extensions: Default::default(),
            leaf_node_extensions: Default::default(),
            lifetime_in_s: 365 * 24 * 3600,
            custom_proposal_types: Default::default(),
            #[cfg(any(test, feature = "test_util"))]
            key_package_not_before: None,
        }
    }
}

pub(crate) fn recreate_config<T: ClientConfig>(
    c: T,
    signer: Option<SignatureSecretKey>,
    signing_identity: Option<(SigningIdentity, CipherSuite)>,
    version: ProtocolVersion,
) -> MakeConfig<T> {
    Config(ConfigInner {
        settings: Settings {
            extension_types: c.supported_extensions(),
            protocol_versions: c.supported_protocol_versions(),
            custom_proposal_types: c.supported_custom_proposals(),
            key_package_extensions: c.key_package_extensions(),
            leaf_node_extensions: c.leaf_node_extensions(),
            lifetime_in_s: {
                let l = c.lifetime();
                l.not_after - l.not_before
            },
            #[cfg(any(test, feature = "test_util"))]
            key_package_not_before: None,
        },
        key_package_repo: c.key_package_repo(),
        psk_store: c.secret_store(),
        group_state_storage: c.group_state_storage(),
        identity_provider: c.identity_provider(),
        mls_rules: c.mls_rules(),
        crypto_provider: c.crypto_provider(),
        signer,
        signing_identity,
        version,
    })
}

/// Definitions meant to be private that are inaccessible outside this crate. They need to be marked
/// `pub` because they appear in public definitions.
mod private {
    use mls_rs_core::{
        crypto::{CipherSuite, SignatureSecretKey},
        identity::SigningIdentity,
        protocol_version::ProtocolVersion,
    };

    use crate::client_builder::{IntoConfigOutput, Settings};

    #[derive(Clone, Debug)]
    pub struct Config<Kpr, Ps, Gss, Ip, Pr, Cp>(pub(crate) ConfigInner<Kpr, Ps, Gss, Ip, Pr, Cp>);

    #[derive(Clone, Debug)]
    pub struct ConfigInner<Kpr, Ps, Gss, Ip, Pr, Cp> {
        pub(crate) settings: Settings,
        pub(crate) key_package_repo: Kpr,
        pub(crate) psk_store: Ps,
        pub(crate) group_state_storage: Gss,
        pub(crate) identity_provider: Ip,
        pub(crate) mls_rules: Pr,
        pub(crate) crypto_provider: Cp,
        pub(crate) signer: Option<SignatureSecretKey>,
        pub(crate) signing_identity: Option<(SigningIdentity, CipherSuite)>,
        pub(crate) version: ProtocolVersion,
    }

    pub trait IntoConfig {
        type KeyPackageRepository;
        type PskStore;
        type GroupStateStorage;
        type IdentityProvider;
        type MlsRules;
        type CryptoProvider;

        fn into_config(self) -> IntoConfigOutput<Self>;
    }

    impl<Kpr, Ps, Gss, Ip, Pr, Cp> IntoConfig for Config<Kpr, Ps, Gss, Ip, Pr, Cp> {
        type KeyPackageRepository = Kpr;
        type PskStore = Ps;
        type GroupStateStorage = Gss;
        type IdentityProvider = Ip;
        type MlsRules = Pr;
        type CryptoProvider = Cp;

        fn into_config(self) -> Self {
            self
        }
    }
}

use mls_rs_core::{
    crypto::{CryptoProvider, SignatureSecretKey},
    extension::{ExtensionError, ExtensionList},
    group::GroupStateStorage,
    identity::IdentityProvider,
    key_package::KeyPackageStorage,
    psk::PreSharedKeyStorage,
};
use private::{Config, ConfigInner, IntoConfig};

#[cfg(test)]
pub(crate) mod test_utils {
    use crate::{
        client_builder::{BaseConfig, ClientBuilder, WithIdentityProvider},
        crypto::test_utils::TestCryptoProvider,
        identity::{
            basic::BasicIdentityProvider,
            test_utils::{get_test_signing_identity, BasicWithCustomProvider},
        },
        CipherSuite,
    };

    use super::WithCryptoProvider;

    pub type TestClientConfig = WithIdentityProvider<
        BasicWithCustomProvider,
        WithCryptoProvider<TestCryptoProvider, BaseConfig>,
    >;

    pub type TestClientBuilder = ClientBuilder<TestClientConfig>;

    impl TestClientBuilder {
        pub fn new_for_test() -> Self {
            ClientBuilder::new()
                .crypto_provider(TestCryptoProvider::new())
                .identity_provider(BasicWithCustomProvider::new(BasicIdentityProvider::new()))
        }

        #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
        pub async fn with_random_signing_identity(
            self,
            identity: &str,
            cipher_suite: CipherSuite,
        ) -> Self {
            let (signing_identity, signer) =
                get_test_signing_identity(cipher_suite, identity.as_bytes()).await;
            self.signing_identity(signing_identity, signer, cipher_suite)
        }
    }
}
