// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use crate::cipher_suite::CipherSuite;
use crate::client_builder::{recreate_config, BaseConfig, ClientBuilder, MakeConfig};
use crate::client_config::ClientConfig;
use crate::group::framing::MlsMessage;

#[cfg(feature = "by_ref_proposal")]
use crate::group::{
    framing::{Content, MlsMessagePayload, PublicMessage, Sender, WireFormat},
    message_signature::AuthenticatedContent,
    proposal::{AddProposal, Proposal},
};
use crate::group::{snapshot::Snapshot, ExportedTree, Group, NewMemberInfo};
use crate::identity::SigningIdentity;
use crate::key_package::{KeyPackageGeneration, KeyPackageGenerator};
use crate::protocol_version::ProtocolVersion;
use crate::tree_kem::node::NodeIndex;
use alloc::vec::Vec;
use mls_rs_codec::MlsDecode;
use mls_rs_core::crypto::{CryptoProvider, SignatureSecretKey};
use mls_rs_core::error::{AnyError, IntoAnyError};
use mls_rs_core::extension::{ExtensionError, ExtensionList, ExtensionType};
use mls_rs_core::group::{GroupStateStorage, ProposalType};
use mls_rs_core::identity::CredentialType;
use mls_rs_core::key_package::KeyPackageStorage;

use crate::group::external_commit::ExternalCommitBuilder;

#[cfg(feature = "by_ref_proposal")]
use alloc::boxed::Box;

#[derive(Debug)]
#[cfg_attr(feature = "std", derive(thiserror::Error))]
#[cfg_attr(all(feature = "ffi", not(test)), safer_ffi_gen::enum_to_error_code)]
#[non_exhaustive]
pub enum MlsError {
    #[cfg_attr(feature = "std", error(transparent))]
    IdentityProviderError(AnyError),
    #[cfg_attr(feature = "std", error(transparent))]
    CryptoProviderError(AnyError),
    #[cfg_attr(feature = "std", error(transparent))]
    KeyPackageRepoError(AnyError),
    #[cfg_attr(feature = "std", error(transparent))]
    GroupStorageError(AnyError),
    #[cfg_attr(feature = "std", error(transparent))]
    PskStoreError(AnyError),
    #[cfg_attr(feature = "std", error(transparent))]
    MlsRulesError(AnyError),
    #[cfg_attr(feature = "std", error(transparent))]
    SerializationError(AnyError),
    #[cfg_attr(feature = "std", error(transparent))]
    ExtensionError(AnyError),
    #[cfg_attr(feature = "std", error("Cipher suite does not match"))]
    CipherSuiteMismatch,
    #[cfg_attr(feature = "std", error("Invalid commit, missing required path"))]
    CommitMissingPath,
    #[cfg_attr(feature = "std", error("plaintext message for incorrect epoch"))]
    InvalidEpoch,
    #[cfg_attr(feature = "std", error("invalid signature found"))]
    InvalidSignature,
    #[cfg_attr(feature = "std", error("invalid confirmation tag"))]
    InvalidConfirmationTag,
    #[cfg_attr(feature = "std", error("invalid membership tag"))]
    InvalidMembershipTag,
    #[cfg_attr(feature = "std", error("corrupt private key, missing required values"))]
    InvalidTreeKemPrivateKey,
    #[cfg_attr(feature = "std", error("key package not found, unable to process"))]
    WelcomeKeyPackageNotFound,
    #[cfg_attr(feature = "std", error("leaf not found in tree for index {0}"))]
    LeafNotFound(u32),
    #[cfg_attr(feature = "std", error("message from self can't be processed"))]
    CantProcessMessageFromSelf,
    #[cfg_attr(
        feature = "std",
        error("pending proposals found, commit required before application messages can be sent")
    )]
    CommitRequired,
    #[cfg_attr(
        feature = "std",
        error("ratchet tree not provided or discovered in GroupInfo")
    )]
    RatchetTreeNotFound,
    #[cfg_attr(feature = "std", error("External sender cannot commit"))]
    ExternalSenderCannotCommit,
    #[cfg_attr(feature = "std", error("Unsupported protocol version {0:?}"))]
    UnsupportedProtocolVersion(ProtocolVersion),
    #[cfg_attr(feature = "std", error("Protocol version mismatch"))]
    ProtocolVersionMismatch,
    #[cfg_attr(feature = "std", error("Unsupported cipher suite {0:?}"))]
    UnsupportedCipherSuite(CipherSuite),
    #[cfg_attr(feature = "std", error("Signing key of external sender is unknown"))]
    UnknownSigningIdentityForExternalSender,
    #[cfg_attr(
        feature = "std",
        error("External proposals are disabled for this group")
    )]
    ExternalProposalsDisabled,
    #[cfg_attr(
        feature = "std",
        error("Signing identity is not allowed to externally propose")
    )]
    InvalidExternalSigningIdentity,
    #[cfg_attr(feature = "std", error("Missing ExternalPub extension"))]
    MissingExternalPubExtension,
    #[cfg_attr(feature = "std", error("Epoch not found"))]
    EpochNotFound,
    #[cfg_attr(feature = "std", error("Unencrypted application message"))]
    UnencryptedApplicationMessage,
    #[cfg_attr(
        feature = "std",
        error("NewMemberCommit sender type can only be used to send Commit content")
    )]
    ExpectedCommitForNewMemberCommit,
    #[cfg_attr(
        feature = "std",
        error("NewMemberProposal sender type can only be used to send add proposals")
    )]
    ExpectedAddProposalForNewMemberProposal,
    #[cfg_attr(
        feature = "std",
        error("External commit missing ExternalInit proposal")
    )]
    ExternalCommitMissingExternalInit,
    #[cfg_attr(
        feature = "std",
        error(
            "A ReIinit has been applied. The next action must be creating or receiving a welcome."
        )
    )]
    GroupUsedAfterReInit,
    #[cfg_attr(feature = "std", error("Pending ReIinit not found."))]
    PendingReInitNotFound,
    #[cfg_attr(
        feature = "std",
        error("The extensions in the welcome message and in the reinit do not match.")
    )]
    ReInitExtensionsMismatch,
    #[cfg_attr(feature = "std", error("signer not found for given identity"))]
    SignerNotFound,
    #[cfg_attr(feature = "std", error("commit already pending"))]
    ExistingPendingCommit,
    #[cfg_attr(feature = "std", error("pending commit not found"))]
    PendingCommitNotFound,
    #[cfg_attr(feature = "std", error("unexpected message type for action"))]
    UnexpectedMessageType,
    #[cfg_attr(
        feature = "std",
        error("membership tag on MlsPlaintext for non-member sender")
    )]
    MembershipTagForNonMember,
    #[cfg_attr(feature = "std", error("No member found for given identity id."))]
    MemberNotFound,
    #[cfg_attr(feature = "std", error("group not found"))]
    GroupNotFound,
    #[cfg_attr(feature = "std", error("unexpected PSK ID"))]
    UnexpectedPskId,
    #[cfg_attr(feature = "std", error("invalid sender for content type"))]
    InvalidSender,
    #[cfg_attr(feature = "std", error("GroupID mismatch"))]
    GroupIdMismatch,
    #[cfg_attr(feature = "std", error("storage retention can not be zero"))]
    NonZeroRetentionRequired,
    #[cfg_attr(feature = "std", error("Too many PSK IDs to compute PSK secret"))]
    TooManyPskIds,
    #[cfg_attr(feature = "std", error("Missing required Psk"))]
    MissingRequiredPsk,
    #[cfg_attr(feature = "std", error("Old group state not found"))]
    OldGroupStateNotFound,
    #[cfg_attr(feature = "std", error("leaf secret already consumed"))]
    InvalidLeafConsumption,
    #[cfg_attr(feature = "std", error("key not available, invalid generation {0}"))]
    KeyMissing(u32),
    #[cfg_attr(
        feature = "std",
        error("requested generation {0} is too far ahead of current generation")
    )]
    InvalidFutureGeneration(u32),
    #[cfg_attr(feature = "std", error("leaf node has no children"))]
    LeafNodeNoChildren,
    #[cfg_attr(feature = "std", error("root node has no parent"))]
    LeafNodeNoParent,
    #[cfg_attr(feature = "std", error("index out of range"))]
    InvalidTreeIndex,
    #[cfg_attr(feature = "std", error("time overflow"))]
    TimeOverflow,
    #[cfg_attr(feature = "std", error("invalid leaf_node_source"))]
    InvalidLeafNodeSource,
    #[cfg_attr(feature = "std", error("key package has expired or is not valid yet"))]
    InvalidLifetime,
    #[cfg_attr(feature = "std", error("required extension not found"))]
    RequiredExtensionNotFound(ExtensionType),
    #[cfg_attr(feature = "std", error("required proposal not found"))]
    RequiredProposalNotFound(ProposalType),
    #[cfg_attr(feature = "std", error("required credential not found"))]
    RequiredCredentialNotFound(CredentialType),
    #[cfg_attr(feature = "std", error("capabilities must describe extensions used"))]
    ExtensionNotInCapabilities(ExtensionType),
    #[cfg_attr(feature = "std", error("expected non-blank node"))]
    ExpectedNode,
    #[cfg_attr(feature = "std", error("node index is out of bounds {0}"))]
    InvalidNodeIndex(NodeIndex),
    #[cfg_attr(feature = "std", error("unexpected empty node found"))]
    UnexpectedEmptyNode,
    #[cfg_attr(
        feature = "std",
        error("duplicate signature key, hpke key or identity found at index {0}")
    )]
    DuplicateLeafData(u32),
    #[cfg_attr(
        feature = "std",
        error("In-use credential type not supported by new leaf at index")
    )]
    InUseCredentialTypeUnsupportedByNewLeaf,
    #[cfg_attr(
        feature = "std",
        error("Not all members support the credential type used by new leaf")
    )]
    CredentialTypeOfNewLeafIsUnsupported,
    #[cfg_attr(
        feature = "std",
        error("the length of the update path is different than the length of the direct path")
    )]
    WrongPathLen,
    #[cfg_attr(
        feature = "std",
        error("same HPKE leaf key before and after applying the update path for leaf {0}")
    )]
    SameHpkeKey(u32),
    #[cfg_attr(feature = "std", error("init key is not valid for cipher suite"))]
    InvalidInitKey,
    #[cfg_attr(
        feature = "std",
        error("init key can not be equal to leaf node public key")
    )]
    InitLeafKeyEquality,
    #[cfg_attr(feature = "std", error("different identity in update for leaf {0}"))]
    DifferentIdentityInUpdate(u32),
    #[cfg_attr(feature = "std", error("update path pub key mismatch"))]
    PubKeyMismatch,
    #[cfg_attr(feature = "std", error("tree hash mismatch"))]
    TreeHashMismatch,
    #[cfg_attr(feature = "std", error("bad update: no suitable secret key"))]
    UpdateErrorNoSecretKey,
    #[cfg_attr(feature = "std", error("invalid lca, not found on direct path"))]
    LcaNotFoundInDirectPath,
    #[cfg_attr(feature = "std", error("update path parent hash mismatch"))]
    ParentHashMismatch,
    #[cfg_attr(feature = "std", error("unexpected pattern of unmerged leaves"))]
    UnmergedLeavesMismatch,
    #[cfg_attr(feature = "std", error("empty tree"))]
    UnexpectedEmptyTree,
    #[cfg_attr(feature = "std", error("trailing blanks"))]
    UnexpectedTrailingBlanks,
    // Proposal Rules errors
    #[cfg_attr(
        feature = "std",
        error("Commiter must not include any update proposals generated by the commiter")
    )]
    InvalidCommitSelfUpdate,
    #[cfg_attr(feature = "std", error("A PreSharedKey proposal must have a PSK of type External or type Resumption and usage Application"))]
    InvalidTypeOrUsageInPreSharedKeyProposal,
    #[cfg_attr(feature = "std", error("psk nonce length does not match cipher suite"))]
    InvalidPskNonceLength,
    #[cfg_attr(
        feature = "std",
        error("ReInit proposal protocol version is less than the version of the original group")
    )]
    InvalidProtocolVersionInReInit,
    #[cfg_attr(feature = "std", error("More than one proposal applying to leaf: {0}"))]
    MoreThanOneProposalForLeaf(u32),
    #[cfg_attr(
        feature = "std",
        error("More than one GroupContextExtensions proposal")
    )]
    MoreThanOneGroupContextExtensionsProposal,
    #[cfg_attr(feature = "std", error("Invalid proposal type for sender"))]
    InvalidProposalTypeForSender,
    #[cfg_attr(
        feature = "std",
        error("External commit must have exactly one ExternalInit proposal")
    )]
    ExternalCommitMustHaveExactlyOneExternalInit,
    #[cfg_attr(feature = "std", error("External commit must have a new leaf"))]
    ExternalCommitMustHaveNewLeaf,
    #[cfg_attr(
        feature = "std",
        error("External commit contains removal of other identity")
    )]
    ExternalCommitRemovesOtherIdentity,
    #[cfg_attr(
        feature = "std",
        error("External commit contains more than one Remove proposal")
    )]
    ExternalCommitWithMoreThanOneRemove,
    #[cfg_attr(feature = "std", error("Duplicate PSK IDs"))]
    DuplicatePskIds,
    #[cfg_attr(
        feature = "std",
        error("Invalid proposal type {0:?} in external commit")
    )]
    InvalidProposalTypeInExternalCommit(ProposalType),
    #[cfg_attr(feature = "std", error("Committer can not remove themselves"))]
    CommitterSelfRemoval,
    #[cfg_attr(
        feature = "std",
        error("Only members can commit proposals by reference")
    )]
    OnlyMembersCanCommitProposalsByRef,
    #[cfg_attr(feature = "std", error("Other proposal with ReInit"))]
    OtherProposalWithReInit,
    #[cfg_attr(feature = "std", error("Unsupported group extension {0:?}"))]
    UnsupportedGroupExtension(ExtensionType),
    #[cfg_attr(feature = "std", error("Unsupported custom proposal type {0:?}"))]
    UnsupportedCustomProposal(ProposalType),
    #[cfg_attr(feature = "std", error("by-ref proposal not found"))]
    ProposalNotFound,
    #[cfg_attr(
        feature = "std",
        error("Removing non-existing member (or removing a member twice)")
    )]
    RemovingNonExistingMember,
    #[cfg_attr(feature = "std", error("Updated identity not a valid successor"))]
    InvalidSuccessor,
    #[cfg_attr(
        feature = "std",
        error("Updating non-existing member (or updating a member twice)")
    )]
    UpdatingNonExistingMember,
    #[cfg_attr(feature = "std", error("Failed generating next path secret"))]
    FailedGeneratingPathSecret,
    #[cfg_attr(feature = "std", error("Invalid group info"))]
    InvalidGroupInfo,
    #[cfg_attr(feature = "std", error("Invalid welcome message"))]
    InvalidWelcomeMessage,
}

impl IntoAnyError for MlsError {
    #[cfg(feature = "std")]
    fn into_dyn_error(self) -> Result<Box<dyn std::error::Error + Send + Sync>, Self> {
        Ok(self.into())
    }
}

impl From<mls_rs_codec::Error> for MlsError {
    #[inline]
    fn from(e: mls_rs_codec::Error) -> Self {
        MlsError::SerializationError(e.into_any_error())
    }
}

impl From<ExtensionError> for MlsError {
    #[inline]
    fn from(e: ExtensionError) -> Self {
        MlsError::ExtensionError(e.into_any_error())
    }
}

/// MLS client used to create key packages and manage groups.
///
/// [`Client::builder`] can be used to instantiate it.
///
/// Clients are able to support multiple protocol versions, ciphersuites
/// and underlying identities used to join groups and generate key packages.
/// Applications may decide to create one or many clients depending on their
/// specific needs.
#[cfg_attr(all(feature = "ffi", not(test)), safer_ffi_gen::ffi_type(opaque))]
#[derive(Clone, Debug)]
pub struct Client<C> {
    pub(crate) config: C,
    pub(crate) signing_identity: Option<(SigningIdentity, CipherSuite)>,
    pub(crate) signer: Option<SignatureSecretKey>,
    pub(crate) version: ProtocolVersion,
}

impl Client<()> {
    /// Returns a [`ClientBuilder`]
    /// used to configure client preferences and providers.
    pub fn builder() -> ClientBuilder<BaseConfig> {
        ClientBuilder::new()
    }
}

#[cfg_attr(all(feature = "ffi", not(test)), safer_ffi_gen::safer_ffi_gen)]
impl<C> Client<C>
where
    C: ClientConfig + Clone,
{
    pub(crate) fn new(
        config: C,
        signer: Option<SignatureSecretKey>,
        signing_identity: Option<(SigningIdentity, CipherSuite)>,
        version: ProtocolVersion,
    ) -> Self {
        Client {
            config,
            signer,
            signing_identity,
            version,
        }
    }

    #[cfg_attr(all(feature = "ffi", not(test)), safer_ffi_gen::safer_ffi_gen_ignore)]
    pub fn to_builder(&self) -> ClientBuilder<MakeConfig<C>> {
        ClientBuilder::from_config(recreate_config(
            self.config.clone(),
            self.signer.clone(),
            self.signing_identity.clone(),
            self.version,
        ))
    }

    /// Creates a new key package message that can be used to to add this
    /// client to a [Group](crate::group::Group). Each call to this function
    /// will produce a unique value that is signed by `signing_identity`.
    ///
    /// The secret keys for the resulting key package message will be stored in
    /// the [KeyPackageStorage](crate::KeyPackageStorage)
    /// that was used to configure the client and will
    /// automatically be erased when this key package is used to
    /// [join a group](Client::join_group).
    ///
    /// # Warning
    ///
    /// A key package message may only be used once.
    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub async fn generate_key_package_message(&self) -> Result<MlsMessage, MlsError> {
        Ok(self.generate_key_package().await?.key_package_message())
    }

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    async fn generate_key_package(&self) -> Result<KeyPackageGeneration, MlsError> {
        let (signing_identity, cipher_suite) = self.signing_identity()?;

        let cipher_suite_provider = self
            .config
            .crypto_provider()
            .cipher_suite_provider(cipher_suite)
            .ok_or(MlsError::UnsupportedCipherSuite(cipher_suite))?;

        let key_package_generator = KeyPackageGenerator {
            protocol_version: self.version,
            cipher_suite_provider: &cipher_suite_provider,
            signing_key: self.signer()?,
            signing_identity,
            identity_provider: &self.config.identity_provider(),
        };

        let key_pkg_gen = key_package_generator
            .generate(
                self.config.lifetime(),
                self.config.capabilities(),
                self.config.key_package_extensions(),
                self.config.leaf_node_extensions(),
            )
            .await?;

        let (id, key_package_data) = key_pkg_gen.to_storage()?;

        self.config
            .key_package_repo()
            .insert(id, key_package_data)
            .await
            .map_err(|e| MlsError::KeyPackageRepoError(e.into_any_error()))?;

        Ok(key_pkg_gen)
    }

    /// Create a group with a specific group_id.
    ///
    /// This function behaves the same way as
    /// [create_group](Client::create_group) except that it
    /// specifies a specific unique group identifier to be used.
    ///
    /// # Warning
    ///
    /// It is recommended to use [create_group](Client::create_group)
    /// instead of this function because it guarantees that group_id values
    /// are globally unique.
    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub async fn create_group_with_id(
        &self,
        group_id: Vec<u8>,
        group_context_extensions: ExtensionList,
    ) -> Result<Group<C>, MlsError> {
        let (signing_identity, cipher_suite) = self.signing_identity()?;

        Group::new(
            self.config.clone(),
            Some(group_id),
            cipher_suite,
            self.version,
            signing_identity.clone(),
            group_context_extensions,
            self.signer()?.clone(),
        )
        .await
    }

    /// Create a MLS group.
    ///
    /// The `cipher_suite` provided must be supported by the
    /// [CipherSuiteProvider](crate::CipherSuiteProvider)
    /// that was used to build the client.
    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub async fn create_group(
        &self,
        group_context_extensions: ExtensionList,
    ) -> Result<Group<C>, MlsError> {
        let (signing_identity, cipher_suite) = self.signing_identity()?;

        Group::new(
            self.config.clone(),
            None,
            cipher_suite,
            self.version,
            signing_identity.clone(),
            group_context_extensions,
            self.signer()?.clone(),
        )
        .await
    }

    /// Join a MLS group via a welcome message created by a
    /// [Commit](crate::group::CommitOutput).
    ///
    /// `tree_data` is required to be provided out of band if the client that
    /// created `welcome_message` did not use the `ratchet_tree_extension`
    /// according to [`MlsRules::commit_options`](`crate::MlsRules::commit_options`).
    /// at the time the welcome message was created. `tree_data` can
    /// be exported from a group using the
    /// [export tree function](crate::group::Group::export_tree).
    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub async fn join_group(
        &self,
        tree_data: Option<ExportedTree<'_>>,
        welcome_message: &MlsMessage,
    ) -> Result<(Group<C>, NewMemberInfo), MlsError> {
        Group::join(
            welcome_message,
            tree_data,
            self.config.clone(),
            self.signer()?.clone(),
        )
        .await
    }

    /// 0-RTT add to an existing [group](crate::group::Group)
    ///
    /// External commits allow for immediate entry into a
    /// [group](crate::group::Group), even if all of the group members
    /// are currently offline and unable to process messages. Sending an
    /// external commit is only allowed for groups that have provided
    /// a public `group_info_message` containing an
    /// [ExternalPubExt](crate::extension::ExternalPubExt), which can be
    /// generated by an existing group member using the
    /// [group_info_message](crate::group::Group::group_info_message)
    /// function.
    ///
    /// `tree_data` may be provided following the same rules as [Client::join_group]
    ///
    /// If PSKs are provided in `external_psks`, the
    /// [PreSharedKeyStorage](crate::PreSharedKeyStorage)
    /// used to configure the client will be searched to resolve their values.
    ///
    /// `to_remove` may be used to remove an existing member provided that the
    /// identity of the existing group member at that [index](crate::group::Member::index)
    /// is a [valid successor](crate::IdentityProvider::valid_successor)
    /// of `signing_identity` as defined by the
    /// [IdentityProvider](crate::IdentityProvider) that this client
    /// was configured with.
    ///
    /// # Warning
    ///
    /// Only one external commit can be performed against a given group info.
    /// There may also be security trade-offs to this approach.
    ///
    // TODO: Add a comment about forward secrecy and a pointer to the future
    // book chapter on this topic
    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub async fn commit_external(
        &self,
        group_info_msg: MlsMessage,
    ) -> Result<(Group<C>, MlsMessage), MlsError> {
        ExternalCommitBuilder::new(
            self.signer()?.clone(),
            self.signing_identity()?.0.clone(),
            self.config.clone(),
        )
        .build(group_info_msg)
        .await
    }

    pub fn external_commit_builder(&self) -> Result<ExternalCommitBuilder<C>, MlsError> {
        Ok(ExternalCommitBuilder::new(
            self.signer()?.clone(),
            self.signing_identity()?.0.clone(),
            self.config.clone(),
        ))
    }

    /// Load an existing group state into this client using the
    /// [GroupStateStorage](crate::GroupStateStorage) that
    /// this client was configured to use.
    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    #[inline(never)]
    pub async fn load_group(&self, group_id: &[u8]) -> Result<Group<C>, MlsError> {
        let snapshot = self
            .config
            .group_state_storage()
            .state(group_id)
            .await
            .map_err(|e| MlsError::GroupStorageError(e.into_any_error()))?
            .ok_or(MlsError::GroupNotFound)?;

        let snapshot = Snapshot::mls_decode(&mut &*snapshot)?;

        Group::from_snapshot(self.config.clone(), snapshot).await
    }

    /// Request to join an existing [group](crate::group::Group).
    ///
    /// An existing group member will need to perform a
    /// [commit](crate::Group::commit) to complete the add and the resulting
    /// welcome message can be used by [join_group](Client::join_group).
    #[cfg(feature = "by_ref_proposal")]
    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub async fn external_add_proposal(
        &self,
        group_info: &MlsMessage,
        tree_data: Option<crate::group::ExportedTree<'_>>,
        authenticated_data: Vec<u8>,
    ) -> Result<MlsMessage, MlsError> {
        let protocol_version = group_info.version;

        if !self.config.version_supported(protocol_version) && protocol_version == self.version {
            return Err(MlsError::UnsupportedProtocolVersion(protocol_version));
        }

        let group_info = group_info
            .as_group_info()
            .ok_or(MlsError::UnexpectedMessageType)?;

        let cipher_suite = group_info.group_context.cipher_suite;

        let cipher_suite_provider = self
            .config
            .crypto_provider()
            .cipher_suite_provider(cipher_suite)
            .ok_or(MlsError::UnsupportedCipherSuite(cipher_suite))?;

        crate::group::validate_group_info_joiner(
            protocol_version,
            group_info,
            tree_data,
            &self.config.identity_provider(),
            &cipher_suite_provider,
        )
        .await?;

        let key_package = self.generate_key_package().await?.key_package;

        (key_package.cipher_suite == cipher_suite)
            .then_some(())
            .ok_or(MlsError::UnsupportedCipherSuite(cipher_suite))?;

        let message = AuthenticatedContent::new_signed(
            &cipher_suite_provider,
            &group_info.group_context,
            Sender::NewMemberProposal,
            Content::Proposal(Box::new(Proposal::Add(Box::new(AddProposal {
                key_package,
            })))),
            self.signer()?,
            WireFormat::PublicMessage,
            authenticated_data,
        )
        .await?;

        let plaintext = PublicMessage {
            content: message.content,
            auth: message.auth,
            membership_tag: None,
        };

        Ok(MlsMessage {
            version: protocol_version,
            payload: MlsMessagePayload::Plain(plaintext),
        })
    }

    fn signer(&self) -> Result<&SignatureSecretKey, MlsError> {
        self.signer.as_ref().ok_or(MlsError::SignerNotFound)
    }

    #[cfg_attr(all(feature = "ffi", not(test)), safer_ffi_gen::safer_ffi_gen_ignore)]
    pub fn signing_identity(&self) -> Result<(&SigningIdentity, CipherSuite), MlsError> {
        self.signing_identity
            .as_ref()
            .map(|(id, cs)| (id, *cs))
            .ok_or(MlsError::SignerNotFound)
    }

    /// Returns key package extensions used by this client
    pub fn key_package_extensions(&self) -> ExtensionList {
        self.config.key_package_extensions()
    }

    /// The [KeyPackageStorage] that this client was configured to use.
    #[cfg_attr(all(feature = "ffi", not(test)), safer_ffi_gen::safer_ffi_gen_ignore)]
    pub fn key_package_store(&self) -> <C as ClientConfig>::KeyPackageRepository {
        self.config.key_package_repo()
    }

    /// The [PreSharedKeyStorage](crate::PreSharedKeyStorage) that
    /// this client was configured to use.
    #[cfg_attr(all(feature = "ffi", not(test)), safer_ffi_gen::safer_ffi_gen_ignore)]
    pub fn secret_store(&self) -> <C as ClientConfig>::PskStore {
        self.config.secret_store()
    }

    /// The [GroupStateStorage] that this client was configured to use.
    #[cfg_attr(all(feature = "ffi", not(test)), safer_ffi_gen::safer_ffi_gen_ignore)]
    pub fn group_state_storage(&self) -> <C as ClientConfig>::GroupStateStorage {
        self.config.group_state_storage()
    }
}

#[cfg(test)]
pub(crate) mod test_utils {
    use super::*;
    use crate::identity::test_utils::get_test_signing_identity;

    pub use crate::client_builder::test_utils::{TestClientBuilder, TestClientConfig};

    pub const TEST_PROTOCOL_VERSION: ProtocolVersion = ProtocolVersion::MLS_10;
    pub const TEST_CIPHER_SUITE: CipherSuite = CipherSuite::P256_AES128;
    pub const TEST_CUSTOM_PROPOSAL_TYPE: ProposalType = ProposalType::new(65001);

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub async fn test_client_with_key_pkg(
        protocol_version: ProtocolVersion,
        cipher_suite: CipherSuite,
        identity: &str,
    ) -> (Client<TestClientConfig>, MlsMessage) {
        test_client_with_key_pkg_custom(protocol_version, cipher_suite, identity, |_| {}).await
    }

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub async fn test_client_with_key_pkg_custom<F>(
        protocol_version: ProtocolVersion,
        cipher_suite: CipherSuite,
        identity: &str,
        mut config: F,
    ) -> (Client<TestClientConfig>, MlsMessage)
    where
        F: FnMut(&mut TestClientConfig),
    {
        let (identity, secret_key) =
            get_test_signing_identity(cipher_suite, identity.as_bytes()).await;

        let mut client = TestClientBuilder::new_for_test()
            .used_protocol_version(protocol_version)
            .signing_identity(identity.clone(), secret_key, cipher_suite)
            .build();

        config(&mut client.config);

        let key_package = client.generate_key_package_message().await.unwrap();

        (client, key_package)
    }
}

#[cfg(test)]
mod tests {
    use super::test_utils::*;

    use super::*;
    use crate::{
        crypto::test_utils::TestCryptoProvider,
        identity::test_utils::{get_test_basic_credential, get_test_signing_identity},
        tree_kem::leaf_node::LeafNodeSource,
    };
    use assert_matches::assert_matches;

    use crate::{
        group::{
            message_processor::ProposalMessageDescription,
            proposal::Proposal,
            test_utils::{test_group, test_group_custom_config},
            ReceivedMessage,
        },
        psk::{ExternalPskId, PreSharedKey},
    };

    use alloc::vec;

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn test_keygen() {
        // This is meant to test the inputs to the internal key package generator
        // See KeyPackageGenerator tests for key generation specific tests
        for (protocol_version, cipher_suite) in ProtocolVersion::all().flat_map(|p| {
            TestCryptoProvider::all_supported_cipher_suites()
                .into_iter()
                .map(move |cs| (p, cs))
        }) {
            let (identity, secret_key) = get_test_signing_identity(cipher_suite, b"foo").await;

            let client = TestClientBuilder::new_for_test()
                .signing_identity(identity.clone(), secret_key, cipher_suite)
                .build();

            // TODO: Tests around extensions
            let key_package = client.generate_key_package_message().await.unwrap();

            assert_eq!(key_package.version, protocol_version);

            let key_package = key_package.into_key_package().unwrap();

            assert_eq!(key_package.cipher_suite, cipher_suite);

            assert_eq!(
                &key_package.leaf_node.signing_identity.credential,
                &get_test_basic_credential(b"foo".to_vec())
            );

            assert_eq!(key_package.leaf_node.signing_identity, identity);

            let capabilities = key_package.leaf_node.ungreased_capabilities();
            assert_eq!(capabilities, client.config.capabilities());

            let client_lifetime = client.config.lifetime();
            assert_matches!(key_package.leaf_node.leaf_node_source, LeafNodeSource::KeyPackage(lifetime) if (lifetime.not_after - lifetime.not_before) == (client_lifetime.not_after - client_lifetime.not_before));
        }
    }

    #[cfg(feature = "by_ref_proposal")]
    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn new_member_add_proposal_adds_to_group() {
        let mut alice_group = test_group(TEST_PROTOCOL_VERSION, TEST_CIPHER_SUITE).await;

        let (bob_identity, secret_key) = get_test_signing_identity(TEST_CIPHER_SUITE, b"bob").await;

        let bob = TestClientBuilder::new_for_test()
            .signing_identity(bob_identity.clone(), secret_key, TEST_CIPHER_SUITE)
            .build();

        let proposal = bob
            .external_add_proposal(
                &alice_group.group.group_info_message(true).await.unwrap(),
                None,
                vec![],
            )
            .await
            .unwrap();

        let message = alice_group
            .group
            .process_incoming_message(proposal)
            .await
            .unwrap();

        assert_matches!(
            message,
            ReceivedMessage::Proposal(ProposalMessageDescription {
                proposal: Proposal::Add(p), ..}
            ) if p.key_package.leaf_node.signing_identity == bob_identity
        );

        alice_group.group.commit(vec![]).await.unwrap();
        alice_group.group.apply_pending_commit().await.unwrap();

        // Check that the new member is in the group
        assert!(alice_group
            .group
            .roster()
            .members_iter()
            .any(|member| member.signing_identity == bob_identity))
    }

    #[cfg(feature = "psk")]
    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    async fn join_via_external_commit(do_remove: bool, with_psk: bool) -> Result<(), MlsError> {
        // An external commit cannot be the first commit in a group as it requires
        // interim_transcript_hash to be computed from the confirmed_transcript_hash and
        // confirmation_tag, which is not the case for the initial interim_transcript_hash.

        let psk = PreSharedKey::from(b"psk".to_vec());
        let psk_id = ExternalPskId::new(b"psk id".to_vec());

        let mut alice_group =
            test_group_custom_config(TEST_PROTOCOL_VERSION, TEST_CIPHER_SUITE, |c| {
                c.psk(psk_id.clone(), psk.clone())
            })
            .await;

        let (mut bob_group, _) = alice_group
            .join_with_custom_config("bob", false, |c| {
                c.0.psk_store.insert(psk_id.clone(), psk.clone());
            })
            .await
            .unwrap();

        let group_info_msg = alice_group
            .group
            .group_info_message_allowing_ext_commit(true)
            .await
            .unwrap();

        let new_client_id = if do_remove { "bob" } else { "charlie" };

        let (new_client_identity, secret_key) =
            get_test_signing_identity(TEST_CIPHER_SUITE, new_client_id.as_bytes()).await;

        let new_client = TestClientBuilder::new_for_test()
            .psk(psk_id.clone(), psk)
            .signing_identity(new_client_identity.clone(), secret_key, TEST_CIPHER_SUITE)
            .build();

        let mut builder = new_client.external_commit_builder().unwrap();

        if do_remove {
            builder = builder.with_removal(1);
        }

        if with_psk {
            builder = builder.with_external_psk(psk_id);
        }

        let (new_group, external_commit) = builder.build(group_info_msg).await?;

        let num_members = if do_remove { 2 } else { 3 };

        assert_eq!(new_group.roster().members_iter().count(), num_members);

        let _ = alice_group
            .group
            .process_incoming_message(external_commit.clone())
            .await
            .unwrap();

        let bob_current_epoch = bob_group.group.current_epoch();

        let message = bob_group
            .group
            .process_incoming_message(external_commit)
            .await
            .unwrap();

        assert!(alice_group.group.roster().members_iter().count() == num_members);

        if !do_remove {
            assert!(bob_group.group.roster().members_iter().count() == num_members);
        } else {
            // Bob was removed so his epoch must stay the same
            assert_eq!(bob_group.group.current_epoch(), bob_current_epoch);

            #[cfg(feature = "state_update")]
            assert_matches!(message, ReceivedMessage::Commit(desc) if !desc.state_update.active);

            #[cfg(not(feature = "state_update"))]
            assert_matches!(message, ReceivedMessage::Commit(_));
        }

        // Comparing epoch authenticators is sufficient to check that members are in sync.
        assert_eq!(
            alice_group.group.epoch_authenticator().unwrap(),
            new_group.epoch_authenticator().unwrap()
        );

        Ok(())
    }

    #[cfg(feature = "psk")]
    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn test_external_commit() {
        // New member can join
        join_via_external_commit(false, false).await.unwrap();
        // New member can remove an old copy of themselves
        join_via_external_commit(true, false).await.unwrap();
        // New member can inject a PSK
        join_via_external_commit(false, true).await.unwrap();
        // All works together
        join_via_external_commit(true, true).await.unwrap();
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn creating_an_external_commit_requires_a_group_info_message() {
        let (alice_identity, secret_key) =
            get_test_signing_identity(TEST_CIPHER_SUITE, b"alice").await;

        let alice = TestClientBuilder::new_for_test()
            .signing_identity(alice_identity.clone(), secret_key, TEST_CIPHER_SUITE)
            .build();

        let msg = alice.generate_key_package_message().await.unwrap();
        let res = alice.commit_external(msg).await.map(|_| ());

        assert_matches!(res, Err(MlsError::UnexpectedMessageType));
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn external_commit_with_invalid_group_info_fails() {
        let mut alice_group = test_group(TEST_PROTOCOL_VERSION, TEST_CIPHER_SUITE).await;
        let mut bob_group = test_group(TEST_PROTOCOL_VERSION, TEST_CIPHER_SUITE).await;

        bob_group.group.commit(vec![]).await.unwrap();
        bob_group.group.apply_pending_commit().await.unwrap();

        let group_info_msg = bob_group
            .group
            .group_info_message_allowing_ext_commit(true)
            .await
            .unwrap();

        let (carol_identity, secret_key) =
            get_test_signing_identity(TEST_CIPHER_SUITE, b"carol").await;

        let carol = TestClientBuilder::new_for_test()
            .signing_identity(carol_identity, secret_key, TEST_CIPHER_SUITE)
            .build();

        let (_, external_commit) = carol
            .external_commit_builder()
            .unwrap()
            .build(group_info_msg)
            .await
            .unwrap();

        // If Carol tries to join Alice's group using the group info from Bob's group, that fails.
        let res = alice_group
            .group
            .process_incoming_message(external_commit)
            .await;
        assert_matches!(res, Err(_));
    }

    #[test]
    fn builder_can_be_obtained_from_client_to_edit_properties_for_new_client() {
        let alice = TestClientBuilder::new_for_test()
            .extension_type(33.into())
            .build();
        let bob = alice.to_builder().extension_type(34.into()).build();
        assert_eq!(bob.config.supported_extensions(), [33, 34].map(Into::into));
    }
}
