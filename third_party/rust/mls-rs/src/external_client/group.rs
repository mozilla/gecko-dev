// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use mls_rs_codec::{MlsDecode, MlsEncode, MlsSize};
use mls_rs_core::{
    crypto::SignatureSecretKey, error::IntoAnyError, extension::ExtensionList, group::Member,
    identity::IdentityProvider,
};

use crate::{
    cipher_suite::CipherSuite,
    client::MlsError,
    external_client::ExternalClientConfig,
    group::{
        cipher_suite_provider,
        confirmation_tag::ConfirmationTag,
        framing::PublicMessage,
        member_from_leaf_node,
        message_processor::{
            ApplicationMessageDescription, CommitMessageDescription, EventOrContent,
            MessageProcessor, ProposalMessageDescription, ProvisionalState,
        },
        snapshot::RawGroupState,
        state::GroupState,
        transcript_hash::InterimTranscriptHash,
        validate_group_info_joiner, ContentType, ExportedTree, GroupContext, GroupInfo, Roster,
        Welcome,
    },
    identity::SigningIdentity,
    protocol_version::ProtocolVersion,
    psk::AlwaysFoundPskStorage,
    tree_kem::{node::LeafIndex, path_secret::PathSecret, TreeKemPrivate},
    CryptoProvider, KeyPackage, MlsMessage,
};

#[cfg(feature = "by_ref_proposal")]
use crate::{
    group::{
        framing::{Content, MlsMessagePayload},
        message_processor::CachedProposal,
        message_signature::AuthenticatedContent,
        proposal::Proposal,
        proposal_ref::ProposalRef,
        Sender,
    },
    WireFormat,
};

#[cfg(all(feature = "by_ref_proposal", feature = "custom_proposal"))]
use crate::group::proposal::CustomProposal;

#[cfg(feature = "by_ref_proposal")]
use mls_rs_core::{crypto::CipherSuiteProvider, psk::ExternalPskId};

#[cfg(feature = "by_ref_proposal")]
use crate::{
    extension::ExternalSendersExt,
    group::proposal::{AddProposal, ReInitProposal, RemoveProposal},
};

#[cfg(all(feature = "by_ref_proposal", feature = "psk"))]
use crate::{
    group::proposal::PreSharedKeyProposal,
    psk::{
        JustPreSharedKeyID, PreSharedKeyID, PskGroupId, PskNonce, ResumptionPSKUsage, ResumptionPsk,
    },
};

#[cfg(feature = "private_message")]
use crate::group::framing::PrivateMessage;

use alloc::boxed::Box;

/// The result of processing an [ExternalGroup](ExternalGroup) message using
/// [process_incoming_message](ExternalGroup::process_incoming_message)
#[derive(Clone, Debug)]
#[allow(clippy::large_enum_variant)]
pub enum ExternalReceivedMessage {
    /// State update as the result of a successful commit.
    Commit(CommitMessageDescription),
    /// Received proposal and its unique identifier.
    Proposal(ProposalMessageDescription),
    /// Encrypted message that can not be processed.
    Ciphertext(ContentType),
    /// Validated GroupInfo object
    GroupInfo(GroupInfo),
    /// Validated welcome message
    Welcome,
    /// Validated key package
    KeyPackage(KeyPackage),
}

/// A handle to an observed group that can track plaintext control messages
/// and the resulting group state.
#[derive(Clone)]
pub struct ExternalGroup<C>
where
    C: ExternalClientConfig,
{
    pub(crate) config: C,
    pub(crate) cipher_suite_provider: <C::CryptoProvider as CryptoProvider>::CipherSuiteProvider,
    pub(crate) state: GroupState,
    pub(crate) signing_data: Option<(SignatureSecretKey, SigningIdentity)>,
}

impl<C: ExternalClientConfig + Clone> ExternalGroup<C> {
    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub(crate) async fn join(
        config: C,
        signing_data: Option<(SignatureSecretKey, SigningIdentity)>,
        group_info: MlsMessage,
        tree_data: Option<ExportedTree<'_>>,
    ) -> Result<Self, MlsError> {
        let protocol_version = group_info.version;

        if !config.version_supported(protocol_version) {
            return Err(MlsError::UnsupportedProtocolVersion(protocol_version));
        }

        let group_info = group_info
            .into_group_info()
            .ok_or(MlsError::UnexpectedMessageType)?;

        let cipher_suite_provider = cipher_suite_provider(
            config.crypto_provider(),
            group_info.group_context.cipher_suite,
        )?;

        let public_tree = validate_group_info_joiner(
            protocol_version,
            &group_info,
            tree_data,
            &config.identity_provider(),
            &cipher_suite_provider,
        )
        .await?;

        let interim_transcript_hash = InterimTranscriptHash::create(
            &cipher_suite_provider,
            &group_info.group_context.confirmed_transcript_hash,
            &group_info.confirmation_tag,
        )
        .await?;

        Ok(Self {
            config,
            signing_data,
            state: GroupState::new(
                group_info.group_context,
                public_tree,
                interim_transcript_hash,
                group_info.confirmation_tag,
            ),
            cipher_suite_provider,
        })
    }

    /// Process a message that was sent to the group.
    ///
    /// * Proposals will be stored in the group state and processed by the
    /// same rules as a standard group.
    ///
    /// * Commits will result in the same outcome as a standard group.
    /// However, the integrity of the resulting group state can only be partially
    /// verified, since the external group does have access to the group
    /// secrets required to do a complete check.
    ///
    /// * Application messages are always encrypted so they result in a no-op
    /// that returns [ExternalReceivedMessage::Ciphertext]
    ///
    /// # Warning
    ///
    /// Processing an encrypted commit or proposal message has the same result
    /// as processing an encrypted application message. Proper tracking of
    /// the group state requires that all proposal and commit messages are
    /// readable.
    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub async fn process_incoming_message(
        &mut self,
        message: MlsMessage,
    ) -> Result<ExternalReceivedMessage, MlsError> {
        MessageProcessor::process_incoming_message(
            self,
            message,
            #[cfg(feature = "by_ref_proposal")]
            self.config.cache_proposals(),
        )
        .await
    }

    /// Replay a proposal message into the group skipping all validation steps.
    #[cfg(feature = "by_ref_proposal")]
    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub async fn insert_proposal_from_message(
        &mut self,
        message: MlsMessage,
    ) -> Result<(), MlsError> {
        let ptxt = match message.payload {
            MlsMessagePayload::Plain(p) => Ok(p),
            _ => Err(MlsError::UnexpectedMessageType),
        }?;

        let auth_content: AuthenticatedContent = ptxt.into();

        let proposal_ref =
            ProposalRef::from_content(&self.cipher_suite_provider, &auth_content).await?;

        let sender = auth_content.content.sender;

        let proposal = match auth_content.content.content {
            Content::Proposal(p) => Ok(*p),
            _ => Err(MlsError::UnexpectedMessageType),
        }?;

        self.group_state_mut()
            .proposals
            .insert(proposal_ref, proposal, sender);

        Ok(())
    }

    /// Force insert a proposal directly into the internal state of the group
    /// with no validation.
    #[cfg(feature = "by_ref_proposal")]
    pub fn insert_proposal(&mut self, proposal: CachedProposal) {
        self.group_state_mut().proposals.insert(
            proposal.proposal_ref,
            proposal.proposal,
            proposal.sender,
        )
    }

    /// Create an external proposal to request that a group add a new member
    ///
    /// # Warning
    ///
    /// In order for the proposal generated by this function to be successfully
    /// committed, the group needs to have `signing_identity` as an entry
    /// within an [ExternalSendersExt](crate::extension::built_in::ExternalSendersExt)
    /// as part of its group context extensions.
    #[cfg(feature = "by_ref_proposal")]
    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub async fn propose_add(
        &mut self,
        key_package: MlsMessage,
        authenticated_data: Vec<u8>,
    ) -> Result<MlsMessage, MlsError> {
        let key_package = key_package
            .into_key_package()
            .ok_or(MlsError::UnexpectedMessageType)?;

        self.propose(
            Proposal::Add(alloc::boxed::Box::new(AddProposal { key_package })),
            authenticated_data,
        )
        .await
    }

    /// Create an external proposal to request that a group remove an existing member
    ///
    /// # Warning
    ///
    /// In order for the proposal generated by this function to be successfully
    /// committed, the group needs to have `signing_identity` as an entry
    /// within an [ExternalSendersExt](crate::extension::built_in::ExternalSendersExt)
    /// as part of its group context extensions.
    #[cfg(feature = "by_ref_proposal")]
    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub async fn propose_remove(
        &mut self,
        index: u32,
        authenticated_data: Vec<u8>,
    ) -> Result<MlsMessage, MlsError> {
        let to_remove = LeafIndex(index);

        // Verify that this leaf is actually in the tree
        self.group_state().public_tree.get_leaf_node(to_remove)?;

        self.propose(
            Proposal::Remove(RemoveProposal { to_remove }),
            authenticated_data,
        )
        .await
    }

    /// Create an external proposal to request that a group inserts an external
    /// pre shared key into its state.
    ///
    /// # Warning
    ///
    /// In order for the proposal generated by this function to be successfully
    /// committed, the group needs to have `signing_identity` as an entry
    /// within an [ExternalSendersExt](crate::extension::built_in::ExternalSendersExt)
    /// as part of its group context extensions.
    #[cfg(all(feature = "by_ref_proposal", feature = "psk"))]
    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub async fn propose_external_psk(
        &mut self,
        psk: ExternalPskId,
        authenticated_data: Vec<u8>,
    ) -> Result<MlsMessage, MlsError> {
        let proposal = self.psk_proposal(JustPreSharedKeyID::External(psk))?;
        self.propose(proposal, authenticated_data).await
    }

    /// Create an external proposal to request that a group adds a pre shared key
    /// from a previous epoch to the current group state.
    ///
    /// # Warning
    ///
    /// In order for the proposal generated by this function to be successfully
    /// committed, the group needs to have `signing_identity` as an entry
    /// within an [ExternalSendersExt](crate::extension::built_in::ExternalSendersExt)
    /// as part of its group context extensions.
    #[cfg(all(feature = "by_ref_proposal", feature = "psk"))]
    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub async fn propose_resumption_psk(
        &mut self,
        psk_epoch: u64,
        authenticated_data: Vec<u8>,
    ) -> Result<MlsMessage, MlsError> {
        let key_id = ResumptionPsk {
            psk_epoch,
            usage: ResumptionPSKUsage::Application,
            psk_group_id: PskGroupId(self.group_context().group_id().to_vec()),
        };

        let proposal = self.psk_proposal(JustPreSharedKeyID::Resumption(key_id))?;
        self.propose(proposal, authenticated_data).await
    }

    #[cfg(all(feature = "by_ref_proposal", feature = "psk"))]
    fn psk_proposal(&self, key_id: JustPreSharedKeyID) -> Result<Proposal, MlsError> {
        Ok(Proposal::Psk(PreSharedKeyProposal {
            psk: PreSharedKeyID {
                key_id,
                psk_nonce: PskNonce::random(&self.cipher_suite_provider)
                    .map_err(|e| MlsError::CryptoProviderError(e.into_any_error()))?,
            },
        }))
    }

    /// Create an external proposal to request that a group sets extensions stored in the group
    /// state.
    ///
    /// # Warning
    ///
    /// In order for the proposal generated by this function to be successfully
    /// committed, the group needs to have `signing_identity` as an entry
    /// within an [ExternalSendersExt](crate::extension::built_in::ExternalSendersExt)
    /// as part of its group context extensions.
    #[cfg(feature = "by_ref_proposal")]
    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub async fn propose_group_context_extensions(
        &mut self,
        extensions: ExtensionList,
        authenticated_data: Vec<u8>,
    ) -> Result<MlsMessage, MlsError> {
        let proposal = Proposal::GroupContextExtensions(extensions);
        self.propose(proposal, authenticated_data).await
    }

    /// Create an external proposal to request that a group is reinitialized.
    ///
    /// # Warning
    ///
    /// In order for the proposal generated by this function to be successfully
    /// committed, the group needs to have `signing_identity` as an entry
    /// within an [ExternalSendersExt](crate::extension::built_in::ExternalSendersExt)
    /// as part of its group context extensions.
    #[cfg(feature = "by_ref_proposal")]
    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub async fn propose_reinit(
        &mut self,
        group_id: Option<Vec<u8>>,
        version: ProtocolVersion,
        cipher_suite: CipherSuite,
        extensions: ExtensionList,
        authenticated_data: Vec<u8>,
    ) -> Result<MlsMessage, MlsError> {
        let group_id = group_id.map(Ok).unwrap_or_else(|| {
            self.cipher_suite_provider
                .random_bytes_vec(self.cipher_suite_provider.kdf_extract_size())
                .map_err(|e| MlsError::CryptoProviderError(e.into_any_error()))
        })?;

        let proposal = Proposal::ReInit(ReInitProposal {
            group_id,
            version,
            cipher_suite,
            extensions,
        });

        self.propose(proposal, authenticated_data).await
    }

    /// Create a custom proposal message.
    ///
    /// # Warning
    ///
    /// In order for the proposal generated by this function to be successfully
    /// committed, the group needs to have `signing_identity` as an entry
    /// within an [ExternalSendersExt](crate::extension::built_in::ExternalSendersExt)
    /// as part of its group context extensions.
    #[cfg(all(feature = "by_ref_proposal", feature = "custom_proposal"))]
    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub async fn propose_custom(
        &mut self,
        proposal: CustomProposal,
        authenticated_data: Vec<u8>,
    ) -> Result<MlsMessage, MlsError> {
        self.propose(Proposal::Custom(proposal), authenticated_data)
            .await
    }

    #[cfg(feature = "by_ref_proposal")]
    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    async fn propose(
        &mut self,
        proposal: Proposal,
        authenticated_data: Vec<u8>,
    ) -> Result<MlsMessage, MlsError> {
        let (signer, signing_identity) =
            self.signing_data.as_ref().ok_or(MlsError::SignerNotFound)?;

        let external_senders_ext = self
            .state
            .context
            .extensions
            .get_as::<ExternalSendersExt>()?
            .ok_or(MlsError::ExternalProposalsDisabled)?;

        let sender_index = external_senders_ext
            .allowed_senders
            .iter()
            .position(|allowed_signer| signing_identity == allowed_signer)
            .ok_or(MlsError::InvalidExternalSigningIdentity)?;

        let sender = Sender::External(sender_index as u32);

        let auth_content = AuthenticatedContent::new_signed(
            &self.cipher_suite_provider,
            &self.state.context,
            sender,
            Content::Proposal(Box::new(proposal.clone())),
            signer,
            WireFormat::PublicMessage,
            authenticated_data,
        )
        .await?;

        self.state.proposals.insert(
            ProposalRef::from_content(&self.cipher_suite_provider, &auth_content).await?,
            proposal,
            sender,
        );

        let plaintext = PublicMessage {
            content: auth_content.content,
            auth: auth_content.auth,
            membership_tag: None,
        };

        Ok(MlsMessage::new(
            self.group_context().version(),
            MlsMessagePayload::Plain(plaintext),
        ))
    }

    /// Delete all sent and received proposals cached for commit.
    #[cfg(feature = "by_ref_proposal")]
    pub fn clear_proposal_cache(&mut self) {
        self.state.proposals.clear()
    }

    #[inline(always)]
    pub(crate) fn group_state(&self) -> &GroupState {
        &self.state
    }

    /// Get the current group context summarizing various information about the group.
    #[inline(always)]
    pub fn group_context(&self) -> &GroupContext {
        &self.group_state().context
    }

    /// Export the current ratchet tree used within the group.
    pub fn export_tree(&self) -> Result<Vec<u8>, MlsError> {
        self.group_state()
            .public_tree
            .nodes
            .mls_encode_to_vec()
            .map_err(Into::into)
    }

    /// Get the current roster of the group.
    #[inline(always)]
    pub fn roster(&self) -> Roster {
        self.group_state().public_tree.roster()
    }

    /// Get the
    /// [transcript hash](https://messaginglayersecurity.rocks/mls-protocol/draft-ietf-mls-protocol.html#name-transcript-hashes)
    /// for the current epoch that the group is in.
    #[inline(always)]
    pub fn transcript_hash(&self) -> &Vec<u8> {
        &self.group_state().context.confirmed_transcript_hash
    }

    /// Get the
    /// [tree hash](https://www.rfc-editor.org/rfc/rfc9420.html#name-tree-hashes)
    /// for the current epoch that the group is in.
    #[inline(always)]
    pub fn tree_hash(&self) -> &[u8] {
        &self.group_state().context.tree_hash
    }

    /// Find a member based on their identity.
    ///
    /// Identities are matched based on the
    /// [IdentityProvider](crate::IdentityProvider)
    /// that this group was configured with.
    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub async fn get_member_with_identity(
        &self,
        identity_id: &SigningIdentity,
    ) -> Result<Member, MlsError> {
        let identity = self
            .identity_provider()
            .identity(identity_id, self.group_context().extensions())
            .await
            .map_err(|error| MlsError::IdentityProviderError(error.into_any_error()))?;

        let tree = &self.group_state().public_tree;

        #[cfg(feature = "tree_index")]
        let index = tree.get_leaf_node_with_identity(&identity);

        #[cfg(not(feature = "tree_index"))]
        let index = tree
            .get_leaf_node_with_identity(
                &identity,
                &self.identity_provider(),
                self.group_context().extensions(),
            )
            .await?;

        let index = index.ok_or(MlsError::MemberNotFound)?;
        let node = self.group_state().public_tree.get_leaf_node(index)?;

        Ok(member_from_leaf_node(node, index))
    }
}

#[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
#[cfg_attr(all(target_arch = "wasm32", mls_build_async), maybe_async::must_be_async(?Send))]
#[cfg_attr(
    all(not(target_arch = "wasm32"), mls_build_async),
    maybe_async::must_be_async
)]
impl<C> MessageProcessor for ExternalGroup<C>
where
    C: ExternalClientConfig + Clone,
{
    type MlsRules = C::MlsRules;
    type IdentityProvider = C::IdentityProvider;
    type PreSharedKeyStorage = AlwaysFoundPskStorage;
    type OutputType = ExternalReceivedMessage;
    type CipherSuiteProvider = <C::CryptoProvider as CryptoProvider>::CipherSuiteProvider;

    fn mls_rules(&self) -> Self::MlsRules {
        self.config.mls_rules()
    }

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    async fn verify_plaintext_authentication(
        &self,
        message: PublicMessage,
    ) -> Result<EventOrContent<Self::OutputType>, MlsError> {
        let auth_content = crate::group::message_verifier::verify_plaintext_authentication(
            &self.cipher_suite_provider,
            message,
            None,
            None,
            &self.state,
        )
        .await?;

        Ok(EventOrContent::Content(auth_content))
    }

    #[cfg(feature = "private_message")]
    async fn process_ciphertext(
        &mut self,
        cipher_text: &PrivateMessage,
    ) -> Result<EventOrContent<Self::OutputType>, MlsError> {
        Ok(EventOrContent::Event(ExternalReceivedMessage::Ciphertext(
            cipher_text.content_type,
        )))
    }

    async fn update_key_schedule(
        &mut self,
        _secrets: Option<(TreeKemPrivate, PathSecret)>,
        interim_transcript_hash: InterimTranscriptHash,
        confirmation_tag: &ConfirmationTag,
        provisional_public_state: ProvisionalState,
    ) -> Result<(), MlsError> {
        self.state.context = provisional_public_state.group_context;
        #[cfg(feature = "by_ref_proposal")]
        self.state.proposals.clear();
        self.state.interim_transcript_hash = interim_transcript_hash;
        self.state.public_tree = provisional_public_state.public_tree;
        self.state.confirmation_tag = confirmation_tag.clone();

        Ok(())
    }

    fn identity_provider(&self) -> Self::IdentityProvider {
        self.config.identity_provider()
    }

    fn psk_storage(&self) -> Self::PreSharedKeyStorage {
        AlwaysFoundPskStorage
    }

    fn group_state(&self) -> &GroupState {
        &self.state
    }

    fn group_state_mut(&mut self) -> &mut GroupState {
        &mut self.state
    }

    fn can_continue_processing(&self, _provisional_state: &ProvisionalState) -> bool {
        true
    }

    #[cfg(feature = "private_message")]
    fn min_epoch_available(&self) -> Option<u64> {
        self.config
            .max_epoch_jitter()
            .map(|j| self.state.context.epoch - j)
    }

    fn cipher_suite_provider(&self) -> &Self::CipherSuiteProvider {
        &self.cipher_suite_provider
    }
}

/// Serializable snapshot of an [ExternalGroup](ExternalGroup) state.
#[derive(Debug, MlsEncode, MlsSize, MlsDecode, PartialEq, Clone)]
pub struct ExternalSnapshot {
    version: u16,
    state: RawGroupState,
    signing_data: Option<(SignatureSecretKey, SigningIdentity)>,
}

impl ExternalSnapshot {
    /// Serialize the snapshot
    pub fn to_bytes(&self) -> Result<Vec<u8>, MlsError> {
        Ok(self.mls_encode_to_vec()?)
    }

    /// Deserialize the snapshot
    pub fn from_bytes(bytes: &[u8]) -> Result<Self, MlsError> {
        Ok(Self::mls_decode(&mut &*bytes)?)
    }
}

impl<C> ExternalGroup<C>
where
    C: ExternalClientConfig + Clone,
{
    /// Create a snapshot of this group's current internal state.
    pub fn snapshot(&self) -> ExternalSnapshot {
        ExternalSnapshot {
            state: RawGroupState::export(self.group_state()),
            version: 1,
            signing_data: self.signing_data.clone(),
        }
    }

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub(crate) async fn from_snapshot(
        config: C,
        snapshot: ExternalSnapshot,
    ) -> Result<Self, MlsError> {
        #[cfg(feature = "tree_index")]
        let identity_provider = config.identity_provider();

        let cipher_suite_provider = cipher_suite_provider(
            config.crypto_provider(),
            snapshot.state.context.cipher_suite,
        )?;

        Ok(ExternalGroup {
            config,
            signing_data: snapshot.signing_data,
            state: snapshot
                .state
                .import(
                    #[cfg(feature = "tree_index")]
                    &identity_provider,
                )
                .await?,
            cipher_suite_provider,
        })
    }
}

impl From<CommitMessageDescription> for ExternalReceivedMessage {
    fn from(value: CommitMessageDescription) -> Self {
        ExternalReceivedMessage::Commit(value)
    }
}

impl TryFrom<ApplicationMessageDescription> for ExternalReceivedMessage {
    type Error = MlsError;

    fn try_from(_: ApplicationMessageDescription) -> Result<Self, Self::Error> {
        Err(MlsError::UnencryptedApplicationMessage)
    }
}

impl From<ProposalMessageDescription> for ExternalReceivedMessage {
    fn from(value: ProposalMessageDescription) -> Self {
        ExternalReceivedMessage::Proposal(value)
    }
}

impl From<GroupInfo> for ExternalReceivedMessage {
    fn from(value: GroupInfo) -> Self {
        ExternalReceivedMessage::GroupInfo(value)
    }
}

impl From<Welcome> for ExternalReceivedMessage {
    fn from(_: Welcome) -> Self {
        ExternalReceivedMessage::Welcome
    }
}

impl From<KeyPackage> for ExternalReceivedMessage {
    fn from(value: KeyPackage) -> Self {
        ExternalReceivedMessage::KeyPackage(value)
    }
}

#[cfg(test)]
pub(crate) mod test_utils {
    use crate::{
        external_client::tests_utils::{TestExternalClientBuilder, TestExternalClientConfig},
        group::test_utils::TestGroup,
    };

    use super::ExternalGroup;

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub(crate) async fn make_external_group(
        group: &TestGroup,
    ) -> ExternalGroup<TestExternalClientConfig> {
        make_external_group_with_config(
            group,
            TestExternalClientBuilder::new_for_test().build_config(),
        )
        .await
    }

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub(crate) async fn make_external_group_with_config(
        group: &TestGroup,
        config: TestExternalClientConfig,
    ) -> ExternalGroup<TestExternalClientConfig> {
        ExternalGroup::join(
            config,
            None,
            group
                .group
                .group_info_message_allowing_ext_commit(true)
                .await
                .unwrap(),
            None,
        )
        .await
        .unwrap()
    }
}

#[cfg(test)]
mod tests {
    use super::test_utils::make_external_group;
    use crate::{
        cipher_suite::CipherSuite,
        client::{
            test_utils::{TEST_CIPHER_SUITE, TEST_PROTOCOL_VERSION},
            MlsError,
        },
        crypto::{test_utils::TestCryptoProvider, SignatureSecretKey},
        extension::ExternalSendersExt,
        external_client::{
            group::test_utils::make_external_group_with_config,
            tests_utils::{TestExternalClientBuilder, TestExternalClientConfig},
            ExternalGroup, ExternalReceivedMessage, ExternalSnapshot,
        },
        group::{
            framing::{Content, MlsMessagePayload},
            proposal::{AddProposal, Proposal, ProposalOrRef},
            proposal_ref::ProposalRef,
            test_utils::{test_group, TestGroup},
            ProposalMessageDescription,
        },
        identity::{test_utils::get_test_signing_identity, SigningIdentity},
        key_package::test_utils::{test_key_package, test_key_package_message},
        protocol_version::ProtocolVersion,
        ExtensionList, MlsMessage,
    };
    use assert_matches::assert_matches;
    use mls_rs_codec::{MlsDecode, MlsEncode};

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    async fn test_group_with_one_commit(v: ProtocolVersion, cs: CipherSuite) -> TestGroup {
        let mut group = test_group(v, cs).await;
        group.group.commit(Vec::new()).await.unwrap();
        group.process_pending_commit().await.unwrap();
        group
    }

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    async fn test_group_two_members(
        v: ProtocolVersion,
        cs: CipherSuite,
        #[cfg(feature = "by_ref_proposal")] ext_identity: Option<SigningIdentity>,
    ) -> TestGroup {
        let mut group = test_group_with_one_commit(v, cs).await;

        let bob_key_package = test_key_package_message(v, cs, "bob").await;

        let mut commit_builder = group
            .group
            .commit_builder()
            .add_member(bob_key_package)
            .unwrap();

        #[cfg(feature = "by_ref_proposal")]
        if let Some(ext_signer) = ext_identity {
            let mut ext_list = ExtensionList::new();

            ext_list
                .set_from(ExternalSendersExt {
                    allowed_senders: vec![ext_signer],
                })
                .unwrap();

            commit_builder = commit_builder.set_group_context_ext(ext_list).unwrap();
        }

        commit_builder.build().await.unwrap();

        group.process_pending_commit().await.unwrap();
        group
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn external_group_can_be_created() {
        for (v, cs) in ProtocolVersion::all().flat_map(|v| {
            TestCryptoProvider::all_supported_cipher_suites()
                .into_iter()
                .map(move |cs| (v, cs))
        }) {
            make_external_group(&test_group_with_one_commit(v, cs).await).await;
        }
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn external_group_can_process_commit() {
        let mut alice = test_group_with_one_commit(TEST_PROTOCOL_VERSION, TEST_CIPHER_SUITE).await;
        let mut server = make_external_group(&alice).await;
        let commit_output = alice.group.commit(Vec::new()).await.unwrap();
        alice.group.apply_pending_commit().await.unwrap();

        server
            .process_incoming_message(commit_output.commit_message)
            .await
            .unwrap();

        assert_eq!(alice.group.state, server.state);
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn external_group_can_process_proposals_by_reference() {
        let mut alice = test_group_with_one_commit(TEST_PROTOCOL_VERSION, TEST_CIPHER_SUITE).await;
        let mut server = make_external_group(&alice).await;

        let bob_key_package =
            test_key_package(TEST_PROTOCOL_VERSION, TEST_CIPHER_SUITE, "bob").await;

        let add_proposal = Proposal::Add(Box::new(AddProposal {
            key_package: bob_key_package,
        }));

        let packet = alice.propose(add_proposal.clone()).await;

        let proposal_process = server.process_incoming_message(packet).await.unwrap();

        assert_matches!(
            proposal_process,
            ExternalReceivedMessage::Proposal(ProposalMessageDescription { ref proposal, ..}) if proposal == &add_proposal
        );

        let commit_output = alice.group.commit(vec![]).await.unwrap();
        alice.group.apply_pending_commit().await.unwrap();

        let commit_result = server
            .process_incoming_message(commit_output.commit_message)
            .await
            .unwrap();

        #[cfg(feature = "state_update")]
        assert_matches!(
            commit_result,
            ExternalReceivedMessage::Commit(commit_description)
                if commit_description.state_update.roster_update.added().iter().any(|added| added.index == 1)
        );

        #[cfg(not(feature = "state_update"))]
        assert_matches!(commit_result, ExternalReceivedMessage::Commit(_));

        assert_eq!(alice.group.state, server.state);
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn external_group_can_process_commit_adding_member() {
        let mut alice = test_group_with_one_commit(TEST_PROTOCOL_VERSION, TEST_CIPHER_SUITE).await;
        let mut server = make_external_group(&alice).await;
        let (_, commit) = alice.join("bob").await;

        let update = match server.process_incoming_message(commit).await.unwrap() {
            ExternalReceivedMessage::Commit(update) => update.state_update,
            _ => panic!("Expected processed commit"),
        };

        #[cfg(feature = "state_update")]
        assert_eq!(update.roster_update.added().len(), 1);

        assert_eq!(server.state.public_tree.get_leaf_nodes().len(), 2);

        assert_eq!(alice.group.state, server.state);
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn external_group_rejects_commit_not_for_current_epoch() {
        let mut alice = test_group_with_one_commit(TEST_PROTOCOL_VERSION, TEST_CIPHER_SUITE).await;
        let mut server = make_external_group(&alice).await;

        let mut commit_output = alice.group.commit(vec![]).await.unwrap();

        match commit_output.commit_message.payload {
            MlsMessagePayload::Plain(ref mut plain) => plain.content.epoch = 0,
            _ => panic!("Unexpected non-plaintext data"),
        };

        let res = server
            .process_incoming_message(commit_output.commit_message)
            .await;

        assert_matches!(res, Err(MlsError::InvalidEpoch));
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn external_group_can_reject_message_with_invalid_signature() {
        let mut alice = test_group_with_one_commit(TEST_PROTOCOL_VERSION, TEST_CIPHER_SUITE).await;

        let mut server = make_external_group_with_config(
            &alice,
            TestExternalClientBuilder::new_for_test().build_config(),
        )
        .await;

        let mut commit_output = alice.group.commit(Vec::new()).await.unwrap();

        match commit_output.commit_message.payload {
            MlsMessagePayload::Plain(ref mut plain) => plain.auth.signature = Vec::new().into(),
            _ => panic!("Unexpected non-plaintext data"),
        };

        let res = server
            .process_incoming_message(commit_output.commit_message)
            .await;

        assert_matches!(res, Err(MlsError::InvalidSignature));
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn external_group_rejects_unencrypted_application_message() {
        let mut alice = test_group_with_one_commit(TEST_PROTOCOL_VERSION, TEST_CIPHER_SUITE).await;
        let mut server = make_external_group(&alice).await;

        let plaintext = alice
            .make_plaintext(Content::Application(b"hello".to_vec().into()))
            .await;

        let res = server.process_incoming_message(plaintext).await;

        assert_matches!(res, Err(MlsError::UnencryptedApplicationMessage));
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn external_group_will_reject_unsupported_cipher_suites() {
        let alice = test_group_with_one_commit(TEST_PROTOCOL_VERSION, TEST_CIPHER_SUITE).await;

        let config =
            TestExternalClientBuilder::new_for_test_disabling_cipher_suite(TEST_CIPHER_SUITE)
                .build_config();

        let res = ExternalGroup::join(
            config,
            None,
            alice
                .group
                .group_info_message_allowing_ext_commit(true)
                .await
                .unwrap(),
            None,
        )
        .await
        .map(|_| ());

        assert_matches!(
            res,
            Err(MlsError::UnsupportedCipherSuite(TEST_CIPHER_SUITE))
        );
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn external_group_will_reject_unsupported_protocol_versions() {
        let alice = test_group_with_one_commit(TEST_PROTOCOL_VERSION, TEST_CIPHER_SUITE).await;

        let config = TestExternalClientBuilder::new_for_test().build_config();

        let mut group_info = alice
            .group
            .group_info_message_allowing_ext_commit(true)
            .await
            .unwrap();

        group_info.version = ProtocolVersion::from(64);

        let res = ExternalGroup::join(config, None, group_info, None)
            .await
            .map(|_| ());

        assert_matches!(
            res,
            Err(MlsError::UnsupportedProtocolVersion(v)) if v ==
                ProtocolVersion::from(64)
        );
    }

    #[cfg(feature = "by_ref_proposal")]
    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    async fn setup_extern_proposal_test(
        extern_proposals_allowed: bool,
    ) -> (SigningIdentity, SignatureSecretKey, TestGroup) {
        let (server_identity, server_key) =
            get_test_signing_identity(TEST_CIPHER_SUITE, b"server").await;

        let alice = test_group_two_members(
            TEST_PROTOCOL_VERSION,
            TEST_CIPHER_SUITE,
            extern_proposals_allowed.then(|| server_identity.clone()),
        )
        .await;

        (server_identity, server_key, alice)
    }

    #[cfg(feature = "by_ref_proposal")]
    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    async fn test_external_proposal(
        server: &mut ExternalGroup<TestExternalClientConfig>,
        alice: &mut TestGroup,
        external_proposal: MlsMessage,
    ) {
        let auth_content = external_proposal.clone().into_plaintext().unwrap().into();

        let proposal_ref = ProposalRef::from_content(&server.cipher_suite_provider, &auth_content)
            .await
            .unwrap();

        // Alice receives the proposal
        alice.process_message(external_proposal).await.unwrap();

        // Alice commits the proposal
        let commit_output = alice.group.commit(vec![]).await.unwrap();

        let commit = match commit_output
            .commit_message
            .clone()
            .into_plaintext()
            .unwrap()
            .content
            .content
        {
            Content::Commit(commit) => commit,
            _ => panic!("not a commit"),
        };

        // The proposal should be in the resulting commit
        assert!(commit
            .proposals
            .contains(&ProposalOrRef::Reference(proposal_ref)));

        alice.process_pending_commit().await.unwrap();

        server
            .process_incoming_message(commit_output.commit_message)
            .await
            .unwrap();

        assert_eq!(alice.group.state, server.state);
    }

    #[cfg(feature = "by_ref_proposal")]
    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn external_group_can_propose_add() {
        let (server_identity, server_key, mut alice) = setup_extern_proposal_test(true).await;

        let mut server = make_external_group(&alice).await;

        server.signing_data = Some((server_key, server_identity));

        let charlie_key_package =
            test_key_package_message(TEST_PROTOCOL_VERSION, TEST_CIPHER_SUITE, "charlie").await;

        let external_proposal = server
            .propose_add(charlie_key_package, vec![])
            .await
            .unwrap();

        test_external_proposal(&mut server, &mut alice, external_proposal).await
    }

    #[cfg(feature = "by_ref_proposal")]
    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn external_group_can_propose_remove() {
        let (server_identity, server_key, mut alice) = setup_extern_proposal_test(true).await;

        let mut server = make_external_group(&alice).await;

        server.signing_data = Some((server_key, server_identity));

        let external_proposal = server.propose_remove(1, vec![]).await.unwrap();

        test_external_proposal(&mut server, &mut alice, external_proposal).await
    }

    #[cfg(feature = "by_ref_proposal")]
    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn external_group_external_proposal_not_allowed() {
        let (signing_id, secret_key, alice) = setup_extern_proposal_test(false).await;
        let mut server = make_external_group(&alice).await;

        server.signing_data = Some((secret_key, signing_id));

        let charlie_key_package =
            test_key_package_message(TEST_PROTOCOL_VERSION, TEST_CIPHER_SUITE, "charlie").await;

        let res = server.propose_add(charlie_key_package, vec![]).await;

        assert_matches!(res, Err(MlsError::ExternalProposalsDisabled));
    }

    #[cfg(feature = "by_ref_proposal")]
    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn external_group_external_signing_identity_invalid() {
        let (server_identity, server_key) =
            get_test_signing_identity(TEST_CIPHER_SUITE, b"server").await;

        let alice = test_group_two_members(
            TEST_PROTOCOL_VERSION,
            TEST_CIPHER_SUITE,
            Some(
                get_test_signing_identity(TEST_CIPHER_SUITE, b"not server")
                    .await
                    .0,
            ),
        )
        .await;

        let mut server = make_external_group(&alice).await;

        server.signing_data = Some((server_key, server_identity));

        let res = server.propose_remove(1, vec![]).await;

        assert_matches!(res, Err(MlsError::InvalidExternalSigningIdentity));
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn external_group_errors_on_old_epoch() {
        let mut alice = test_group_with_one_commit(TEST_PROTOCOL_VERSION, TEST_CIPHER_SUITE).await;

        let mut server = make_external_group_with_config(
            &alice,
            TestExternalClientBuilder::new_for_test()
                .max_epoch_jitter(0)
                .build_config(),
        )
        .await;

        let old_application_msg = alice
            .group
            .encrypt_application_message(&[], vec![])
            .await
            .unwrap();

        let commit_output = alice.group.commit(vec![]).await.unwrap();

        server
            .process_incoming_message(commit_output.commit_message)
            .await
            .unwrap();

        let res = server.process_incoming_message(old_application_msg).await;

        assert_matches!(res, Err(MlsError::InvalidEpoch));
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn proposals_can_be_cached_externally() {
        let mut alice = test_group_with_one_commit(TEST_PROTOCOL_VERSION, TEST_CIPHER_SUITE).await;

        let mut server = make_external_group_with_config(
            &alice,
            TestExternalClientBuilder::new_for_test()
                .cache_proposals(false)
                .build_config(),
        )
        .await;

        let proposal = alice.group.propose_update(vec![]).await.unwrap();

        let commit_output = alice.group.commit(vec![]).await.unwrap();

        server
            .process_incoming_message(proposal.clone())
            .await
            .unwrap();

        server.insert_proposal_from_message(proposal).await.unwrap();

        server
            .process_incoming_message(commit_output.commit_message)
            .await
            .unwrap();
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn external_group_can_observe_since_creation() {
        let mut alice = test_group(TEST_PROTOCOL_VERSION, TEST_CIPHER_SUITE).await;

        let info = alice
            .group
            .group_info_message_allowing_ext_commit(true)
            .await
            .unwrap();

        let config = TestExternalClientBuilder::new_for_test().build_config();
        let mut server = ExternalGroup::join(config, None, info, None).await.unwrap();

        for _ in 0..2 {
            let commit = alice.group.commit(vec![]).await.unwrap().commit_message;
            alice.process_pending_commit().await.unwrap();
            server.process_incoming_message(commit).await.unwrap();
        }
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn external_group_can_be_serialized_to_tls_encoding() {
        let server =
            make_external_group(&test_group(TEST_PROTOCOL_VERSION, TEST_CIPHER_SUITE).await).await;

        let snapshot = server.snapshot().mls_encode_to_vec().unwrap();
        let snapshot_restored = ExternalSnapshot::mls_decode(&mut snapshot.as_slice()).unwrap();

        let server_restored =
            ExternalGroup::from_snapshot(server.config.clone(), snapshot_restored)
                .await
                .unwrap();

        assert_eq!(server.group_state(), server_restored.group_state());
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn external_group_can_validate_info() {
        let alice = test_group_with_one_commit(TEST_PROTOCOL_VERSION, TEST_CIPHER_SUITE).await;
        let mut server = make_external_group(&alice).await;

        let info = alice
            .group
            .group_info_message_allowing_ext_commit(false)
            .await
            .unwrap();

        let update = server.process_incoming_message(info.clone()).await.unwrap();
        let info = info.into_group_info().unwrap();

        assert_matches!(update, ExternalReceivedMessage::GroupInfo(update_info) if update_info == info);
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn external_group_can_validate_key_package() {
        let alice = test_group_with_one_commit(TEST_PROTOCOL_VERSION, TEST_CIPHER_SUITE).await;
        let mut server = make_external_group(&alice).await;

        let kp = test_key_package_message(TEST_PROTOCOL_VERSION, TEST_CIPHER_SUITE, "john").await;

        let update = server.process_incoming_message(kp.clone()).await.unwrap();
        let kp = kp.into_key_package().unwrap();

        assert_matches!(update, ExternalReceivedMessage::KeyPackage(update_kp) if update_kp == kp);
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn external_group_can_validate_welcome() {
        let mut alice = test_group_with_one_commit(TEST_PROTOCOL_VERSION, TEST_CIPHER_SUITE).await;
        let mut server = make_external_group(&alice).await;

        let [welcome] = alice
            .group
            .commit_builder()
            .add_member(
                test_key_package_message(TEST_PROTOCOL_VERSION, TEST_CIPHER_SUITE, "john").await,
            )
            .unwrap()
            .build()
            .await
            .unwrap()
            .welcome_messages
            .try_into()
            .unwrap();

        let update = server.process_incoming_message(welcome).await.unwrap();

        assert_matches!(update, ExternalReceivedMessage::Welcome);
    }
}
