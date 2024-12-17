// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use super::{
    commit_sender,
    confirmation_tag::ConfirmationTag,
    framing::{
        ApplicationData, Content, ContentType, MlsMessage, MlsMessagePayload, PublicMessage, Sender,
    },
    message_signature::AuthenticatedContent,
    mls_rules::{CommitDirection, MlsRules},
    proposal_filter::ProposalBundle,
    state::GroupState,
    transcript_hash::InterimTranscriptHash,
    transcript_hashes, validate_group_info_member, GroupContext, GroupInfo, Welcome,
};
use crate::{
    client::MlsError,
    key_package::validate_key_package_properties,
    time::MlsTime,
    tree_kem::{
        leaf_node_validator::{LeafNodeValidator, ValidationContext},
        node::LeafIndex,
        path_secret::PathSecret,
        validate_update_path, TreeKemPrivate, TreeKemPublic, ValidatedUpdatePath,
    },
    CipherSuiteProvider, KeyPackage,
};
#[cfg(mls_build_async)]
use alloc::boxed::Box;
use alloc::vec::Vec;
use core::fmt::{self, Debug};
use mls_rs_core::{
    identity::IdentityProvider, protocol_version::ProtocolVersion, psk::PreSharedKeyStorage,
};

#[cfg(feature = "by_ref_proposal")]
use super::proposal_ref::ProposalRef;

#[cfg(not(feature = "by_ref_proposal"))]
use crate::group::proposal_cache::resolve_for_commit;

#[cfg(feature = "by_ref_proposal")]
use super::proposal::Proposal;

#[cfg(feature = "custom_proposal")]
use super::proposal_filter::ProposalInfo;

#[cfg(feature = "state_update")]
use mls_rs_core::{
    crypto::CipherSuite,
    group::{MemberUpdate, RosterUpdate},
};

#[cfg(all(feature = "state_update", feature = "psk"))]
use mls_rs_core::psk::ExternalPskId;

#[cfg(feature = "state_update")]
use crate::tree_kem::UpdatePath;

#[cfg(feature = "state_update")]
use super::{member_from_key_package, member_from_leaf_node};

#[cfg(all(feature = "state_update", feature = "custom_proposal"))]
use super::proposal::CustomProposal;

#[cfg(feature = "private_message")]
use crate::group::framing::PrivateMessage;

#[cfg(feature = "by_ref_proposal")]
use mls_rs_codec::{MlsDecode, MlsEncode, MlsSize};

#[derive(Debug)]
pub(crate) struct ProvisionalState {
    pub(crate) public_tree: TreeKemPublic,
    pub(crate) applied_proposals: ProposalBundle,
    pub(crate) group_context: GroupContext,
    pub(crate) external_init_index: Option<LeafIndex>,
    pub(crate) indexes_of_added_kpkgs: Vec<LeafIndex>,
    #[cfg(feature = "by_ref_proposal")]
    pub(crate) unused_proposals: Vec<crate::mls_rules::ProposalInfo<Proposal>>,
}

//By default, the path field of a Commit MUST be populated. The path field MAY be omitted if
//(a) it covers at least one proposal and (b) none of the proposals covered by the Commit are
//of "path required" types. A proposal type requires a path if it cannot change the group
//membership in a way that requires the forward secrecy and post-compromise security guarantees
//that an UpdatePath provides. The only proposal types defined in this document that do not
//require a path are:

// add
// psk
// reinit
pub(crate) fn path_update_required(proposals: &ProposalBundle) -> bool {
    let res = proposals.external_init_proposals().first().is_some();

    #[cfg(feature = "by_ref_proposal")]
    let res = res || !proposals.update_proposals().is_empty();

    res || proposals.length() == 0
        || proposals.group_context_extensions_proposal().is_some()
        || !proposals.remove_proposals().is_empty()
}

/// Representation of changes made by a [commit](crate::Group::commit).
#[cfg(feature = "state_update")]
#[derive(Clone, Debug, PartialEq)]
pub struct StateUpdate {
    pub(crate) roster_update: RosterUpdate,
    #[cfg(feature = "psk")]
    pub(crate) added_psks: Vec<ExternalPskId>,
    pub(crate) pending_reinit: Option<CipherSuite>,
    pub(crate) active: bool,
    pub(crate) epoch: u64,
    #[cfg(feature = "custom_proposal")]
    pub(crate) custom_proposals: Vec<ProposalInfo<CustomProposal>>,
    #[cfg(feature = "by_ref_proposal")]
    pub(crate) unused_proposals: Vec<crate::mls_rules::ProposalInfo<Proposal>>,
}

#[cfg(not(feature = "state_update"))]
#[non_exhaustive]
#[derive(Clone, Debug, PartialEq)]
pub struct StateUpdate {}

#[cfg(feature = "state_update")]
impl StateUpdate {
    /// Changes to the roster as a result of proposals.
    pub fn roster_update(&self) -> &RosterUpdate {
        &self.roster_update
    }

    #[cfg(feature = "psk")]
    /// Pre-shared keys that have been added to the group.
    pub fn added_psks(&self) -> &[ExternalPskId] {
        &self.added_psks
    }

    /// Flag to indicate if the group is now pending reinitialization due to
    /// receiving a [`ReInit`](crate::group::proposal::Proposal::ReInit)
    /// proposal.
    pub fn is_pending_reinit(&self) -> bool {
        self.pending_reinit.is_some()
    }

    /// Flag to indicate the group is still active. This will be false if the
    /// member processing the commit has been removed from the group.
    pub fn is_active(&self) -> bool {
        self.active
    }

    /// The new epoch of the group state.
    pub fn new_epoch(&self) -> u64 {
        self.epoch
    }

    /// Custom proposals that were committed to.
    #[cfg(feature = "custom_proposal")]
    pub fn custom_proposals(&self) -> &[ProposalInfo<CustomProposal>] {
        &self.custom_proposals
    }

    /// Proposals that were received in the prior epoch but not committed to.
    #[cfg(feature = "by_ref_proposal")]
    pub fn unused_proposals(&self) -> &[crate::mls_rules::ProposalInfo<Proposal>] {
        &self.unused_proposals
    }

    pub fn pending_reinit_ciphersuite(&self) -> Option<CipherSuite> {
        self.pending_reinit
    }
}

// #[cfg_attr(
//     all(feature = "ffi", not(test)),
//     safer_ffi_gen::ffi_type(clone, opaque)
// )]
#[derive(Debug, Clone)]
#[allow(clippy::large_enum_variant)]
/// An event generated as a result of processing a message for a group with
/// [`Group::process_incoming_message`](crate::group::Group::process_incoming_message).
pub enum ReceivedMessage {
    /// An application message was decrypted.
    ApplicationMessage(ApplicationMessageDescription),
    /// A new commit was processed creating a new group state.
    Commit(CommitMessageDescription),
    /// A proposal was received.
    Proposal(ProposalMessageDescription),
    /// Validated GroupInfo object
    GroupInfo(GroupInfo),
    /// Validated welcome message
    Welcome,
    /// Validated key package
    KeyPackage(KeyPackage),
}

impl TryFrom<ApplicationMessageDescription> for ReceivedMessage {
    type Error = MlsError;

    fn try_from(value: ApplicationMessageDescription) -> Result<Self, Self::Error> {
        Ok(ReceivedMessage::ApplicationMessage(value))
    }
}

impl From<CommitMessageDescription> for ReceivedMessage {
    fn from(value: CommitMessageDescription) -> Self {
        ReceivedMessage::Commit(value)
    }
}

impl From<ProposalMessageDescription> for ReceivedMessage {
    fn from(value: ProposalMessageDescription) -> Self {
        ReceivedMessage::Proposal(value)
    }
}

impl From<GroupInfo> for ReceivedMessage {
    fn from(value: GroupInfo) -> Self {
        ReceivedMessage::GroupInfo(value)
    }
}

impl From<Welcome> for ReceivedMessage {
    fn from(_: Welcome) -> Self {
        ReceivedMessage::Welcome
    }
}

impl From<KeyPackage> for ReceivedMessage {
    fn from(value: KeyPackage) -> Self {
        ReceivedMessage::KeyPackage(value)
    }
}

// #[cfg_attr(
//     all(feature = "ffi", not(test)),
//     safer_ffi_gen::ffi_type(clone, opaque)
// )]
#[derive(Clone, PartialEq, Eq)]
/// Description of a MLS application message.
pub struct ApplicationMessageDescription {
    /// Index of this user in the group state.
    pub sender_index: u32,
    /// Received application data.
    data: ApplicationData,
    /// Plaintext authenticated data in the received MLS packet.
    pub authenticated_data: Vec<u8>,
}

impl Debug for ApplicationMessageDescription {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("ApplicationMessageDescription")
            .field("sender_index", &self.sender_index)
            .field("data", &self.data)
            .field(
                "authenticated_data",
                &mls_rs_core::debug::pretty_bytes(&self.authenticated_data),
            )
            .finish()
    }
}

// #[cfg_attr(all(feature = "ffi", not(test)), safer_ffi_gen::safer_ffi_gen)]
impl ApplicationMessageDescription {
    pub fn data(&self) -> &[u8] {
        self.data.as_bytes()
    }
}

// #[cfg_attr(
//     all(feature = "ffi", not(test)),
//     safer_ffi_gen::ffi_type(clone, opaque)
// )]
#[derive(Clone, PartialEq)]
#[non_exhaustive]
/// Description of a processed MLS commit message.
pub struct CommitMessageDescription {
    /// True if this is the result of an external commit.
    pub is_external: bool,
    /// The index in the group state of the member who performed this commit.
    pub committer: u32,
    /// A full description of group state changes as a result of this commit.
    pub state_update: StateUpdate,
    /// Plaintext authenticated data in the received MLS packet.
    pub authenticated_data: Vec<u8>,
}

impl Debug for CommitMessageDescription {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("CommitMessageDescription")
            .field("is_external", &self.is_external)
            .field("committer", &self.committer)
            .field("state_update", &self.state_update)
            .field(
                "authenticated_data",
                &mls_rs_core::debug::pretty_bytes(&self.authenticated_data),
            )
            .finish()
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
/// Proposal sender type.
pub enum ProposalSender {
    /// A current member of the group by index in the group state.
    Member(u32),
    /// An external entity by index within an
    /// [`ExternalSendersExt`](crate::extension::built_in::ExternalSendersExt).
    External(u32),
    /// A new member proposing their addition to the group.
    NewMember,
}

impl TryFrom<Sender> for ProposalSender {
    type Error = MlsError;

    fn try_from(value: Sender) -> Result<Self, Self::Error> {
        match value {
            Sender::Member(index) => Ok(Self::Member(index)),
            #[cfg(feature = "by_ref_proposal")]
            Sender::External(index) => Ok(Self::External(index)),
            #[cfg(feature = "by_ref_proposal")]
            Sender::NewMemberProposal => Ok(Self::NewMember),
            Sender::NewMemberCommit => Err(MlsError::InvalidSender),
        }
    }
}

#[cfg(feature = "by_ref_proposal")]
// #[cfg_attr(
//     all(feature = "ffi", not(test)),
//     safer_ffi_gen::ffi_type(clone, opaque)
// )]
#[derive(Clone)]
#[non_exhaustive]
/// Description of a processed MLS proposal message.
pub struct ProposalMessageDescription {
    /// Sender of the proposal.
    pub sender: ProposalSender,
    /// Proposal content.
    pub proposal: Proposal,
    /// Plaintext authenticated data in the received MLS packet.
    pub authenticated_data: Vec<u8>,
    /// Proposal reference.
    pub proposal_ref: ProposalRef,
}

#[cfg(feature = "by_ref_proposal")]
impl Debug for ProposalMessageDescription {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("ProposalMessageDescription")
            .field("sender", &self.sender)
            .field("proposal", &self.proposal)
            .field(
                "authenticated_data",
                &mls_rs_core::debug::pretty_bytes(&self.authenticated_data),
            )
            .field("proposal_ref", &self.proposal_ref)
            .finish()
    }
}

#[cfg(feature = "by_ref_proposal")]
#[derive(MlsSize, MlsEncode, MlsDecode)]
pub struct CachedProposal {
    pub(crate) proposal: Proposal,
    pub(crate) proposal_ref: ProposalRef,
    pub(crate) sender: Sender,
}

#[cfg(feature = "by_ref_proposal")]
impl CachedProposal {
    /// Deserialize the proposal
    pub fn from_bytes(bytes: &[u8]) -> Result<Self, MlsError> {
        Ok(Self::mls_decode(&mut &*bytes)?)
    }

    /// Serialize the proposal
    pub fn to_bytes(&self) -> Result<Vec<u8>, MlsError> {
        Ok(self.mls_encode_to_vec()?)
    }
}

#[cfg(feature = "by_ref_proposal")]
impl ProposalMessageDescription {
    pub fn cached_proposal(self) -> CachedProposal {
        let sender = match self.sender {
            ProposalSender::Member(i) => Sender::Member(i),
            ProposalSender::External(i) => Sender::External(i),
            ProposalSender::NewMember => Sender::NewMemberProposal,
        };

        CachedProposal {
            proposal: self.proposal,
            proposal_ref: self.proposal_ref,
            sender,
        }
    }

    pub fn proposal_ref(&self) -> Vec<u8> {
        self.proposal_ref.to_vec()
    }
}

#[cfg(not(feature = "by_ref_proposal"))]
// #[cfg_attr(
//     all(feature = "ffi", not(test)),
//     safer_ffi_gen::ffi_type(clone, opaque)
// )]
#[derive(Debug, Clone)]
/// Description of a processed MLS proposal message.
pub struct ProposalMessageDescription {}

#[allow(clippy::large_enum_variant)]
pub(crate) enum EventOrContent<E> {
    #[cfg_attr(
        not(all(feature = "private_message", feature = "external_client")),
        allow(dead_code)
    )]
    Event(E),
    Content(AuthenticatedContent),
}

#[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
#[cfg_attr(all(target_arch = "wasm32", mls_build_async), maybe_async::must_be_async(?Send))]
#[cfg_attr(
    all(not(target_arch = "wasm32"), mls_build_async),
    maybe_async::must_be_async
)]
pub(crate) trait MessageProcessor: Send + Sync {
    type OutputType: TryFrom<ApplicationMessageDescription, Error = MlsError>
        + From<CommitMessageDescription>
        + From<ProposalMessageDescription>
        + From<GroupInfo>
        + From<Welcome>
        + From<KeyPackage>
        + Send;

    type MlsRules: MlsRules;
    type IdentityProvider: IdentityProvider;
    type CipherSuiteProvider: CipherSuiteProvider;
    type PreSharedKeyStorage: PreSharedKeyStorage;

    async fn process_incoming_message(
        &mut self,
        message: MlsMessage,
        #[cfg(feature = "by_ref_proposal")] cache_proposal: bool,
    ) -> Result<Self::OutputType, MlsError> {
        self.process_incoming_message_with_time(
            message,
            #[cfg(feature = "by_ref_proposal")]
            cache_proposal,
            None,
        )
        .await
    }

    async fn process_incoming_message_with_time(
        &mut self,
        message: MlsMessage,
        #[cfg(feature = "by_ref_proposal")] cache_proposal: bool,
        time_sent: Option<MlsTime>,
    ) -> Result<Self::OutputType, MlsError> {
        let event_or_content = self.get_event_from_incoming_message(message).await?;

        self.process_event_or_content(
            event_or_content,
            #[cfg(feature = "by_ref_proposal")]
            cache_proposal,
            time_sent,
        )
        .await
    }

    async fn get_event_from_incoming_message(
        &mut self,
        message: MlsMessage,
    ) -> Result<EventOrContent<Self::OutputType>, MlsError> {
        self.check_metadata(&message)?;

        match message.payload {
            MlsMessagePayload::Plain(plaintext) => {
                self.verify_plaintext_authentication(plaintext).await
            }
            #[cfg(feature = "private_message")]
            MlsMessagePayload::Cipher(cipher_text) => self.process_ciphertext(&cipher_text).await,
            MlsMessagePayload::GroupInfo(group_info) => {
                validate_group_info_member(
                    self.group_state(),
                    message.version,
                    &group_info,
                    self.cipher_suite_provider(),
                )
                .await?;

                Ok(EventOrContent::Event(group_info.into()))
            }
            MlsMessagePayload::Welcome(welcome) => {
                self.validate_welcome(&welcome, message.version)?;

                Ok(EventOrContent::Event(welcome.into()))
            }
            MlsMessagePayload::KeyPackage(key_package) => {
                self.validate_key_package(&key_package, message.version)
                    .await?;

                Ok(EventOrContent::Event(key_package.into()))
            }
        }
    }

    async fn process_event_or_content(
        &mut self,
        event_or_content: EventOrContent<Self::OutputType>,
        #[cfg(feature = "by_ref_proposal")] cache_proposal: bool,
        time_sent: Option<MlsTime>,
    ) -> Result<Self::OutputType, MlsError> {
        let msg = match event_or_content {
            EventOrContent::Event(event) => event,
            EventOrContent::Content(content) => {
                self.process_auth_content(
                    content,
                    #[cfg(feature = "by_ref_proposal")]
                    cache_proposal,
                    time_sent,
                )
                .await?
            }
        };

        Ok(msg)
    }

    async fn process_auth_content(
        &mut self,
        auth_content: AuthenticatedContent,
        #[cfg(feature = "by_ref_proposal")] cache_proposal: bool,
        time_sent: Option<MlsTime>,
    ) -> Result<Self::OutputType, MlsError> {
        let event = match auth_content.content.content {
            #[cfg(feature = "private_message")]
            Content::Application(data) => {
                let authenticated_data = auth_content.content.authenticated_data;
                let sender = auth_content.content.sender;

                self.process_application_message(data, sender, authenticated_data)
                    .and_then(Self::OutputType::try_from)
            }
            Content::Commit(_) => self
                .process_commit(auth_content, time_sent)
                .await
                .map(Self::OutputType::from),
            #[cfg(feature = "by_ref_proposal")]
            Content::Proposal(ref proposal) => self
                .process_proposal(&auth_content, proposal, cache_proposal)
                .await
                .map(Self::OutputType::from),
        }?;

        Ok(event)
    }

    #[cfg(feature = "private_message")]
    fn process_application_message(
        &self,
        data: ApplicationData,
        sender: Sender,
        authenticated_data: Vec<u8>,
    ) -> Result<ApplicationMessageDescription, MlsError> {
        let Sender::Member(sender_index) = sender else {
            return Err(MlsError::InvalidSender);
        };

        Ok(ApplicationMessageDescription {
            authenticated_data,
            sender_index,
            data,
        })
    }

    #[cfg(feature = "by_ref_proposal")]
    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    async fn process_proposal(
        &mut self,
        auth_content: &AuthenticatedContent,
        proposal: &Proposal,
        cache_proposal: bool,
    ) -> Result<ProposalMessageDescription, MlsError> {
        let proposal_ref =
            ProposalRef::from_content(self.cipher_suite_provider(), auth_content).await?;

        let group_state = self.group_state_mut();

        if cache_proposal {
            let proposal_ref = proposal_ref.clone();

            group_state.proposals.insert(
                proposal_ref.clone(),
                proposal.clone(),
                auth_content.content.sender,
            );
        }

        Ok(ProposalMessageDescription {
            authenticated_data: auth_content.content.authenticated_data.clone(),
            proposal: proposal.clone(),
            sender: auth_content.content.sender.try_into()?,
            proposal_ref,
        })
    }

    #[cfg(feature = "state_update")]
    async fn make_state_update(
        &self,
        provisional: &ProvisionalState,
        path: Option<&UpdatePath>,
        sender: LeafIndex,
    ) -> Result<StateUpdate, MlsError> {
        let added = provisional
            .applied_proposals
            .additions
            .iter()
            .zip(provisional.indexes_of_added_kpkgs.iter())
            .map(|(p, index)| member_from_key_package(&p.proposal.key_package, *index))
            .collect::<Vec<_>>();

        let mut added = added;

        let old_tree = &self.group_state().public_tree;

        let removed = provisional
            .applied_proposals
            .removals
            .iter()
            .map(|p| {
                let index = p.proposal.to_remove;
                let node = old_tree.nodes.borrow_as_leaf(index)?;
                Ok(member_from_leaf_node(node, index))
            })
            .collect::<Result<_, MlsError>>()?;

        #[cfg(feature = "by_ref_proposal")]
        let mut updated = provisional
            .applied_proposals
            .update_senders
            .iter()
            .map(|index| {
                let prior = old_tree
                    .get_leaf_node(*index)
                    .map(|n| member_from_leaf_node(n, *index))?;

                let new = provisional
                    .public_tree
                    .get_leaf_node(*index)
                    .map(|n| member_from_leaf_node(n, *index))?;

                Ok::<_, MlsError>(MemberUpdate::new(prior, new))
            })
            .collect::<Result<Vec<_>, _>>()?;

        #[cfg(not(feature = "by_ref_proposal"))]
        let mut updated = Vec::new();

        if let Some(path) = path {
            if !provisional
                .applied_proposals
                .external_initializations
                .is_empty()
            {
                added.push(member_from_leaf_node(&path.leaf_node, sender))
            } else {
                let prior = old_tree
                    .get_leaf_node(sender)
                    .map(|n| member_from_leaf_node(n, sender))?;

                let new = member_from_leaf_node(&path.leaf_node, sender);

                updated.push(MemberUpdate::new(prior, new))
            }
        }

        #[cfg(feature = "psk")]
        let psks = provisional
            .applied_proposals
            .psks
            .iter()
            .filter_map(|psk| psk.proposal.external_psk_id().cloned())
            .collect::<Vec<_>>();

        let roster_update = RosterUpdate::new(added, removed, updated);

        let update = StateUpdate {
            roster_update,
            #[cfg(feature = "psk")]
            added_psks: psks,
            pending_reinit: provisional
                .applied_proposals
                .reinitializations
                .first()
                .map(|ri| ri.proposal.new_cipher_suite()),
            active: true,
            epoch: provisional.group_context.epoch,
            #[cfg(feature = "custom_proposal")]
            custom_proposals: provisional.applied_proposals.custom_proposals.clone(),
            #[cfg(feature = "by_ref_proposal")]
            unused_proposals: provisional.unused_proposals.clone(),
        };

        Ok(update)
    }

    async fn process_commit(
        &mut self,
        auth_content: AuthenticatedContent,
        time_sent: Option<MlsTime>,
    ) -> Result<CommitMessageDescription, MlsError> {
        if self.group_state().pending_reinit.is_some() {
            return Err(MlsError::GroupUsedAfterReInit);
        }

        // Update the new GroupContext's confirmed and interim transcript hashes using the new Commit.
        let (interim_transcript_hash, confirmed_transcript_hash) = transcript_hashes(
            self.cipher_suite_provider(),
            &self.group_state().interim_transcript_hash,
            &auth_content,
        )
        .await?;

        #[cfg(any(feature = "private_message", feature = "by_ref_proposal"))]
        let commit = match auth_content.content.content {
            Content::Commit(commit) => Ok(commit),
            _ => Err(MlsError::UnexpectedMessageType),
        }?;

        #[cfg(not(any(feature = "private_message", feature = "by_ref_proposal")))]
        let Content::Commit(commit) = auth_content.content.content;

        let group_state = self.group_state();
        let id_provider = self.identity_provider();

        #[cfg(feature = "by_ref_proposal")]
        let proposals = group_state
            .proposals
            .resolve_for_commit(auth_content.content.sender, commit.proposals)?;

        #[cfg(not(feature = "by_ref_proposal"))]
        let proposals = resolve_for_commit(auth_content.content.sender, commit.proposals)?;

        let mut provisional_state = group_state
            .apply_resolved(
                auth_content.content.sender,
                proposals,
                commit.path.as_ref().map(|path| &path.leaf_node),
                &id_provider,
                self.cipher_suite_provider(),
                &self.psk_storage(),
                &self.mls_rules(),
                time_sent,
                CommitDirection::Receive,
            )
            .await?;

        let sender = commit_sender(&auth_content.content.sender, &provisional_state)?;

        #[cfg(feature = "state_update")]
        let mut state_update = self
            .make_state_update(&provisional_state, commit.path.as_ref(), sender)
            .await?;

        #[cfg(not(feature = "state_update"))]
        let state_update = StateUpdate {};

        //Verify that the path value is populated if the proposals vector contains any Update
        // or Remove proposals, or if it's empty. Otherwise, the path value MAY be omitted.
        if path_update_required(&provisional_state.applied_proposals) && commit.path.is_none() {
            return Err(MlsError::CommitMissingPath);
        }

        if !self.can_continue_processing(&provisional_state) {
            #[cfg(feature = "state_update")]
            {
                state_update.active = false;
            }

            return Ok(CommitMessageDescription {
                is_external: matches!(auth_content.content.sender, Sender::NewMemberCommit),
                authenticated_data: auth_content.content.authenticated_data,
                committer: *sender,
                state_update,
            });
        }

        let update_path = match commit.path {
            Some(update_path) => Some(
                validate_update_path(
                    &self.identity_provider(),
                    self.cipher_suite_provider(),
                    update_path,
                    &provisional_state,
                    sender,
                    time_sent,
                )
                .await?,
            ),
            None => None,
        };

        let new_secrets = match update_path {
            Some(update_path) => {
                self.apply_update_path(sender, &update_path, &mut provisional_state)
                    .await
            }
            None => Ok(None),
        }?;

        // Update the transcript hash to get the new context.
        provisional_state.group_context.confirmed_transcript_hash = confirmed_transcript_hash;

        // Update the parent hashes in the new context
        provisional_state
            .public_tree
            .update_hashes(&[sender], self.cipher_suite_provider())
            .await?;

        // Update the tree hash in the new context
        provisional_state.group_context.tree_hash = provisional_state
            .public_tree
            .tree_hash(self.cipher_suite_provider())
            .await?;

        if let Some(reinit) = provisional_state.applied_proposals.reinitializations.pop() {
            self.group_state_mut().pending_reinit = Some(reinit.proposal);

            #[cfg(feature = "state_update")]
            {
                state_update.active = false;
            }
        }

        if let Some(confirmation_tag) = &auth_content.auth.confirmation_tag {
            // Update the key schedule to calculate new private keys
            self.update_key_schedule(
                new_secrets,
                interim_transcript_hash,
                confirmation_tag,
                provisional_state,
            )
            .await?;

            Ok(CommitMessageDescription {
                is_external: matches!(auth_content.content.sender, Sender::NewMemberCommit),
                authenticated_data: auth_content.content.authenticated_data,
                committer: *sender,
                state_update,
            })
        } else {
            Err(MlsError::InvalidConfirmationTag)
        }
    }

    fn group_state(&self) -> &GroupState;
    fn group_state_mut(&mut self) -> &mut GroupState;
    fn mls_rules(&self) -> Self::MlsRules;
    fn identity_provider(&self) -> Self::IdentityProvider;
    fn cipher_suite_provider(&self) -> &Self::CipherSuiteProvider;
    fn psk_storage(&self) -> Self::PreSharedKeyStorage;
    fn can_continue_processing(&self, provisional_state: &ProvisionalState) -> bool;

    #[cfg(feature = "private_message")]
    fn min_epoch_available(&self) -> Option<u64>;

    fn check_metadata(&self, message: &MlsMessage) -> Result<(), MlsError> {
        let context = &self.group_state().context;

        if message.version != context.protocol_version {
            return Err(MlsError::ProtocolVersionMismatch);
        }

        if let Some((group_id, epoch, content_type)) = match &message.payload {
            MlsMessagePayload::Plain(plaintext) => Some((
                &plaintext.content.group_id,
                plaintext.content.epoch,
                plaintext.content.content_type(),
            )),
            #[cfg(feature = "private_message")]
            MlsMessagePayload::Cipher(ciphertext) => Some((
                &ciphertext.group_id,
                ciphertext.epoch,
                ciphertext.content_type,
            )),
            _ => None,
        } {
            if group_id != &context.group_id {
                return Err(MlsError::GroupIdMismatch);
            }

            match content_type {
                ContentType::Commit => {
                    if context.epoch != epoch {
                        Err(MlsError::InvalidEpoch)
                    } else {
                        Ok(())
                    }
                }
                #[cfg(feature = "by_ref_proposal")]
                ContentType::Proposal => {
                    if context.epoch != epoch {
                        Err(MlsError::InvalidEpoch)
                    } else {
                        Ok(())
                    }
                }
                #[cfg(feature = "private_message")]
                ContentType::Application => {
                    if let Some(min) = self.min_epoch_available() {
                        if epoch < min {
                            Err(MlsError::InvalidEpoch)
                        } else {
                            Ok(())
                        }
                    } else {
                        Ok(())
                    }
                }
            }?;

            // Proposal and commit messages must be sent in the current epoch
            let check_epoch = content_type == ContentType::Commit;

            #[cfg(feature = "by_ref_proposal")]
            let check_epoch = check_epoch || content_type == ContentType::Proposal;

            if check_epoch && epoch != context.epoch {
                return Err(MlsError::InvalidEpoch);
            }

            // Unencrypted application messages are not allowed
            #[cfg(feature = "private_message")]
            if !matches!(&message.payload, MlsMessagePayload::Cipher(_))
                && content_type == ContentType::Application
            {
                return Err(MlsError::UnencryptedApplicationMessage);
            }
        }

        Ok(())
    }

    fn validate_welcome(
        &self,
        welcome: &Welcome,
        version: ProtocolVersion,
    ) -> Result<(), MlsError> {
        let state = self.group_state();

        (welcome.cipher_suite == state.context.cipher_suite
            && version == state.context.protocol_version)
            .then_some(())
            .ok_or(MlsError::InvalidWelcomeMessage)
    }

    async fn validate_key_package(
        &self,
        key_package: &KeyPackage,
        version: ProtocolVersion,
    ) -> Result<(), MlsError> {
        let cs = self.cipher_suite_provider();
        let id = self.identity_provider();

        validate_key_package(key_package, version, cs, &id).await
    }

    #[cfg(feature = "private_message")]
    async fn process_ciphertext(
        &mut self,
        cipher_text: &PrivateMessage,
    ) -> Result<EventOrContent<Self::OutputType>, MlsError>;

    async fn verify_plaintext_authentication(
        &self,
        message: PublicMessage,
    ) -> Result<EventOrContent<Self::OutputType>, MlsError>;

    async fn apply_update_path(
        &mut self,
        sender: LeafIndex,
        update_path: &ValidatedUpdatePath,
        provisional_state: &mut ProvisionalState,
    ) -> Result<Option<(TreeKemPrivate, PathSecret)>, MlsError> {
        provisional_state
            .public_tree
            .apply_update_path(
                sender,
                update_path,
                &provisional_state.group_context.extensions,
                self.identity_provider(),
                self.cipher_suite_provider(),
            )
            .await
            .map(|_| None)
    }

    async fn update_key_schedule(
        &mut self,
        secrets: Option<(TreeKemPrivate, PathSecret)>,
        interim_transcript_hash: InterimTranscriptHash,
        confirmation_tag: &ConfirmationTag,
        provisional_public_state: ProvisionalState,
    ) -> Result<(), MlsError>;
}

#[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
pub(crate) async fn validate_key_package<C: CipherSuiteProvider, I: IdentityProvider>(
    key_package: &KeyPackage,
    version: ProtocolVersion,
    cs: &C,
    id: &I,
) -> Result<(), MlsError> {
    let validator = LeafNodeValidator::new(cs, id, None);

    #[cfg(feature = "std")]
    let context = Some(MlsTime::now());

    #[cfg(not(feature = "std"))]
    let context = None;

    let context = ValidationContext::Add(context);

    validator
        .check_if_valid(&key_package.leaf_node, context)
        .await?;

    validate_key_package_properties(key_package, version, cs).await?;

    Ok(())
}
