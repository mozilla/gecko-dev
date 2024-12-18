// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use alloc::vec;
use alloc::vec::Vec;
use core::fmt::{self, Debug};
use mls_rs_codec::{MlsDecode, MlsEncode, MlsSize};
use mls_rs_core::{
    crypto::{CipherSuiteProvider, SignatureSecretKey},
    error::IntoAnyError,
};

use crate::{
    cipher_suite::CipherSuite,
    client::MlsError,
    client_config::ClientConfig,
    extension::RatchetTreeExt,
    identity::SigningIdentity,
    protocol_version::ProtocolVersion,
    signer::Signable,
    tree_kem::{
        kem::TreeKem, node::LeafIndex, path_secret::PathSecret, TreeKemPrivate, UpdatePath,
    },
    ExtensionList, MlsRules,
};

#[cfg(all(not(mls_build_async), feature = "rayon"))]
use {crate::iter::ParallelIteratorExt, rayon::prelude::*};

use crate::tree_kem::leaf_node::LeafNode;

#[cfg(not(feature = "private_message"))]
use crate::WireFormat;

#[cfg(feature = "psk")]
use crate::{
    group::{JustPreSharedKeyID, PskGroupId, ResumptionPSKUsage, ResumptionPsk},
    psk::ExternalPskId,
};

use super::{
    confirmation_tag::ConfirmationTag,
    framing::{Content, MlsMessage, MlsMessagePayload, Sender},
    key_schedule::{KeySchedule, WelcomeSecret},
    message_processor::{path_update_required, MessageProcessor},
    message_signature::AuthenticatedContent,
    mls_rules::CommitDirection,
    proposal::{Proposal, ProposalOrRef},
    ConfirmedTranscriptHash, EncryptedGroupSecrets, ExportedTree, Group, GroupContext, GroupInfo,
    Welcome,
};

#[cfg(not(feature = "by_ref_proposal"))]
use super::proposal_cache::prepare_commit;

#[cfg(feature = "custom_proposal")]
use super::proposal::CustomProposal;

#[derive(Clone, Debug, PartialEq, MlsSize, MlsEncode, MlsDecode)]
#[cfg_attr(feature = "arbitrary", derive(mls_rs_core::arbitrary::Arbitrary))]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
pub(crate) struct Commit {
    pub proposals: Vec<ProposalOrRef>,
    pub path: Option<UpdatePath>,
}

#[derive(Clone, PartialEq, Debug, MlsEncode, MlsDecode, MlsSize)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
pub(super) struct CommitGeneration {
    pub content: AuthenticatedContent,
    pub pending_private_tree: TreeKemPrivate,
    pub pending_commit_secret: PathSecret,
    pub commit_message_hash: CommitHash,
}

#[derive(Clone, PartialEq, MlsEncode, MlsDecode, MlsSize)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
pub(crate) struct CommitHash(
    #[mls_codec(with = "mls_rs_codec::byte_vec")]
    #[cfg_attr(feature = "serde", serde(with = "mls_rs_core::vec_serde"))]
    Vec<u8>,
);

impl Debug for CommitHash {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        mls_rs_core::debug::pretty_bytes(&self.0)
            .named("CommitHash")
            .fmt(f)
    }
}

impl CommitHash {
    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub(crate) async fn compute<CS: CipherSuiteProvider>(
        cs: &CS,
        commit: &MlsMessage,
    ) -> Result<Self, MlsError> {
        cs.hash(&commit.mls_encode_to_vec()?)
            .await
            .map_err(|e| MlsError::CryptoProviderError(e.into_any_error()))
            .map(Self)
    }
}

#[cfg_attr(
    all(feature = "ffi", not(test)),
    safer_ffi_gen::ffi_type(clone, opaque)
)]
#[derive(Clone, Debug)]
#[non_exhaustive]
/// Result of MLS commit operation using
/// [`Group::commit`](crate::group::Group::commit) or
/// [`CommitBuilder::build`](CommitBuilder::build).
pub struct CommitOutput {
    /// Commit message to send to other group members.
    pub commit_message: MlsMessage,
    /// Welcome messages to send to new group members. If the commit does not add members,
    /// this list is empty. Otherwise, if [`MlsRules::commit_options`] returns `single_welcome_message`
    /// set to true, then this list contains a single message sent to all members. Else, the list
    /// contains one message for each added member. Recipients of each message can be identified using
    /// [`MlsMessage::key_package_reference`] of their key packages and
    /// [`MlsMessage::welcome_key_package_references`].
    pub welcome_messages: Vec<MlsMessage>,
    /// Ratchet tree that can be sent out of band if
    /// `ratchet_tree_extension` is not used according to
    /// [`MlsRules::commit_options`].
    pub ratchet_tree: Option<ExportedTree<'static>>,
    /// A group info that can be provided to new members in order to enable external commit
    /// functionality. This value is set if [`MlsRules::commit_options`] returns
    /// `allow_external_commit` set to true.
    pub external_commit_group_info: Option<MlsMessage>,
    /// Proposals that were received in the prior epoch but not included in the following commit.
    #[cfg(feature = "by_ref_proposal")]
    pub unused_proposals: Vec<crate::mls_rules::ProposalInfo<Proposal>>,
}

#[cfg_attr(all(feature = "ffi", not(test)), ::safer_ffi_gen::safer_ffi_gen)]
impl CommitOutput {
    /// Commit message to send to other group members.
    #[cfg(feature = "ffi")]
    pub fn commit_message(&self) -> &MlsMessage {
        &self.commit_message
    }

    /// Welcome message to send to new group members.
    #[cfg(feature = "ffi")]
    pub fn welcome_messages(&self) -> &[MlsMessage] {
        &self.welcome_messages
    }

    /// Ratchet tree that can be sent out of band if
    /// `ratchet_tree_extension` is not used according to
    /// [`MlsRules::commit_options`].
    #[cfg(feature = "ffi")]
    pub fn ratchet_tree(&self) -> Option<&ExportedTree<'static>> {
        self.ratchet_tree.as_ref()
    }

    /// A group info that can be provided to new members in order to enable external commit
    /// functionality. This value is set if [`MlsRules::commit_options`] returns
    /// `allow_external_commit` set to true.
    #[cfg(feature = "ffi")]
    pub fn external_commit_group_info(&self) -> Option<&MlsMessage> {
        self.external_commit_group_info.as_ref()
    }

    /// Proposals that were received in the prior epoch but not included in the following commit.
    #[cfg(all(feature = "ffi", feature = "by_ref_proposal"))]
    pub fn unused_proposals(&self) -> &[crate::mls_rules::ProposalInfo<Proposal>] {
        &self.unused_proposals
    }
}

/// Build a commit with multiple proposals by-value.
///
/// Proposals within a commit can be by-value or by-reference.
/// Proposals received during the current epoch will be added to the resulting
/// commit by-reference automatically so long as they pass the rules defined
/// in the current
/// [proposal rules](crate::client_builder::ClientBuilder::mls_rules).
pub struct CommitBuilder<'a, C>
where
    C: ClientConfig + Clone,
{
    group: &'a mut Group<C>,
    pub(super) proposals: Vec<Proposal>,
    authenticated_data: Vec<u8>,
    group_info_extensions: ExtensionList,
    new_signer: Option<SignatureSecretKey>,
    new_signing_identity: Option<SigningIdentity>,
}

impl<'a, C> CommitBuilder<'a, C>
where
    C: ClientConfig + Clone,
{
    /// Insert an [`AddProposal`](crate::group::proposal::AddProposal) into
    /// the current commit that is being built.
    pub fn add_member(mut self, key_package: MlsMessage) -> Result<CommitBuilder<'a, C>, MlsError> {
        let proposal = self.group.add_proposal(key_package)?;
        self.proposals.push(proposal);
        Ok(self)
    }

    /// Set group info extensions that will be inserted into the resulting
    /// [welcome messages](CommitOutput::welcome_messages) for new members.
    ///
    /// Group info extensions that are transmitted as part of a welcome message
    /// are encrypted along with other private values.
    ///
    /// These extensions can be retrieved as part of
    /// [`NewMemberInfo`](crate::group::NewMemberInfo) that is returned
    /// by joining the group via
    /// [`Client::join_group`](crate::Client::join_group).
    pub fn set_group_info_ext(self, extensions: ExtensionList) -> Self {
        Self {
            group_info_extensions: extensions,
            ..self
        }
    }

    /// Insert a [`RemoveProposal`](crate::group::proposal::RemoveProposal) into
    /// the current commit that is being built.
    pub fn remove_member(mut self, index: u32) -> Result<Self, MlsError> {
        let proposal = self.group.remove_proposal(index)?;
        self.proposals.push(proposal);
        Ok(self)
    }

    /// Insert a
    /// [`GroupContextExtensions`](crate::group::proposal::Proposal::GroupContextExtensions)
    /// into the current commit that is being built.
    pub fn set_group_context_ext(mut self, extensions: ExtensionList) -> Result<Self, MlsError> {
        let proposal = self.group.group_context_extensions_proposal(extensions);
        self.proposals.push(proposal);
        Ok(self)
    }

    /// Insert a
    /// [`PreSharedKeyProposal`](crate::group::proposal::PreSharedKeyProposal) with
    /// an external PSK into the current commit that is being built.
    #[cfg(feature = "psk")]
    pub fn add_external_psk(mut self, psk_id: ExternalPskId) -> Result<Self, MlsError> {
        let key_id = JustPreSharedKeyID::External(psk_id);
        let proposal = self.group.psk_proposal(key_id)?;
        self.proposals.push(proposal);
        Ok(self)
    }

    /// Insert a
    /// [`PreSharedKeyProposal`](crate::group::proposal::PreSharedKeyProposal) with
    /// a resumption PSK into the current commit that is being built.
    #[cfg(feature = "psk")]
    pub fn add_resumption_psk(mut self, psk_epoch: u64) -> Result<Self, MlsError> {
        let psk_id = ResumptionPsk {
            psk_epoch,
            usage: ResumptionPSKUsage::Application,
            psk_group_id: PskGroupId(self.group.group_id().to_vec()),
        };

        let key_id = JustPreSharedKeyID::Resumption(psk_id);
        let proposal = self.group.psk_proposal(key_id)?;
        self.proposals.push(proposal);
        Ok(self)
    }

    /// Insert a [`ReInitProposal`](crate::group::proposal::ReInitProposal) into
    /// the current commit that is being built.
    pub fn reinit(
        mut self,
        group_id: Option<Vec<u8>>,
        version: ProtocolVersion,
        cipher_suite: CipherSuite,
        extensions: ExtensionList,
    ) -> Result<Self, MlsError> {
        let proposal = self
            .group
            .reinit_proposal(group_id, version, cipher_suite, extensions)?;

        self.proposals.push(proposal);
        Ok(self)
    }

    /// Insert a [`CustomProposal`](crate::group::proposal::CustomProposal) into
    /// the current commit that is being built.
    #[cfg(feature = "custom_proposal")]
    pub fn custom_proposal(mut self, proposal: CustomProposal) -> Self {
        self.proposals.push(Proposal::Custom(proposal));
        self
    }

    /// Insert a proposal that was previously constructed such as when a
    /// proposal is returned from
    /// [`StateUpdate::unused_proposals`](super::StateUpdate::unused_proposals).
    pub fn raw_proposal(mut self, proposal: Proposal) -> Self {
        self.proposals.push(proposal);
        self
    }

    /// Insert proposals that were previously constructed such as when a
    /// proposal is returned from
    /// [`StateUpdate::unused_proposals`](super::StateUpdate::unused_proposals).
    pub fn raw_proposals(mut self, mut proposals: Vec<Proposal>) -> Self {
        self.proposals.append(&mut proposals);
        self
    }

    /// Add additional authenticated data to the commit.
    ///
    /// # Warning
    ///
    /// The data provided here is always sent unencrypted.
    pub fn authenticated_data(self, authenticated_data: Vec<u8>) -> Self {
        Self {
            authenticated_data,
            ..self
        }
    }

    /// Change the committer's signing identity as part of making this commit.
    /// This will only succeed if the [`IdentityProvider`](crate::IdentityProvider)
    /// in use by the group considers the credential inside this signing_identity
    /// [valid](crate::IdentityProvider::validate_member)
    /// and results in the same
    /// [identity](crate::IdentityProvider::identity)
    /// being used.
    pub fn set_new_signing_identity(
        self,
        signer: SignatureSecretKey,
        signing_identity: SigningIdentity,
    ) -> Self {
        Self {
            new_signer: Some(signer),
            new_signing_identity: Some(signing_identity),
            ..self
        }
    }

    /// Finalize the commit to send.
    ///
    /// # Errors
    ///
    /// This function will return an error if any of the proposals provided
    /// are not contextually valid according to the rules defined by the
    /// MLS RFC, or if they do not pass the custom rules defined by the current
    /// [proposal rules](crate::client_builder::ClientBuilder::mls_rules).
    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub async fn build(self) -> Result<CommitOutput, MlsError> {
        self.group
            .commit_internal(
                self.proposals,
                None,
                self.authenticated_data,
                self.group_info_extensions,
                self.new_signer,
                self.new_signing_identity,
            )
            .await
    }
}

impl<C> Group<C>
where
    C: ClientConfig + Clone,
{
    /// Perform a commit of received proposals.
    ///
    /// This function is the equivalent of [`Group::commit_builder`] immediately
    /// followed by [`CommitBuilder::build`]. Any received proposals since the
    /// last commit will be included in the resulting message by-reference.
    ///
    /// Data provided in the `authenticated_data` field will be placed into
    /// the resulting commit message unencrypted.
    ///
    /// # Pending Commits
    ///
    /// When a commit is created, it is not applied immediately in order to
    /// allow for the resolution of conflicts when multiple members of a group
    /// attempt to make commits at the same time. For example, a central relay
    /// can be used to decide which commit should be accepted by the group by
    /// determining a consistent view of commit packet order for all clients.
    ///
    /// Pending commits are stored internally as part of the group's state
    /// so they do not need to be tracked outside of this library. Any commit
    /// message that is processed before calling [Group::apply_pending_commit]
    /// will clear the currently pending commit.
    ///
    /// # Empty Commits
    ///
    /// Sending a commit that contains no proposals is a valid operation
    /// within the MLS protocol. It is useful for providing stronger forward
    /// secrecy and post-compromise security, especially for long running
    /// groups when group membership does not change often.
    ///
    /// # Path Updates
    ///
    /// Path updates provide forward secrecy and post-compromise security
    /// within the MLS protocol.
    /// The `path_required` option returned by [`MlsRules::commit_options`](`crate::MlsRules::commit_options`)
    /// controls the ability of a group to send a commit without a path update.
    /// An update path will automatically be sent if there are no proposals
    /// in the commit, or if any proposal other than
    /// [`Add`](crate::group::proposal::Proposal::Add),
    /// [`Psk`](crate::group::proposal::Proposal::Psk),
    /// or [`ReInit`](crate::group::proposal::Proposal::ReInit) are part of the commit.
    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub async fn commit(&mut self, authenticated_data: Vec<u8>) -> Result<CommitOutput, MlsError> {
        self.commit_internal(
            vec![],
            None,
            authenticated_data,
            Default::default(),
            None,
            None,
        )
        .await
    }

    /// Create a new commit builder that can include proposals
    /// by-value.
    pub fn commit_builder(&mut self) -> CommitBuilder<C> {
        CommitBuilder {
            group: self,
            proposals: Default::default(),
            authenticated_data: Default::default(),
            group_info_extensions: Default::default(),
            new_signer: Default::default(),
            new_signing_identity: Default::default(),
        }
    }

    /// Returns commit and optional [`MlsMessage`] containing a welcome message
    /// for newly added members.
    #[allow(clippy::too_many_arguments)]
    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub(super) async fn commit_internal(
        &mut self,
        proposals: Vec<Proposal>,
        external_leaf: Option<&LeafNode>,
        authenticated_data: Vec<u8>,
        mut welcome_group_info_extensions: ExtensionList,
        new_signer: Option<SignatureSecretKey>,
        new_signing_identity: Option<SigningIdentity>,
    ) -> Result<CommitOutput, MlsError> {
        if self.pending_commit.is_some() {
            return Err(MlsError::ExistingPendingCommit);
        }

        if self.state.pending_reinit.is_some() {
            return Err(MlsError::GroupUsedAfterReInit);
        }

        let mls_rules = self.config.mls_rules();

        let is_external = external_leaf.is_some();

        // Construct an initial Commit object with the proposals field populated from Proposals
        // received during the current epoch, and an empty path field. Add passed in proposals
        // by value
        let sender = if is_external {
            Sender::NewMemberCommit
        } else {
            Sender::Member(*self.private_tree.self_index)
        };

        let new_signer_ref = new_signer.as_ref().unwrap_or(&self.signer);
        let old_signer = &self.signer;

        #[cfg(feature = "std")]
        let time = Some(crate::time::MlsTime::now());

        #[cfg(not(feature = "std"))]
        let time = None;

        #[cfg(feature = "by_ref_proposal")]
        let proposals = self.state.proposals.prepare_commit(sender, proposals);

        #[cfg(not(feature = "by_ref_proposal"))]
        let proposals = prepare_commit(sender, proposals);

        let mut provisional_state = self
            .state
            .apply_resolved(
                sender,
                proposals,
                external_leaf,
                &self.config.identity_provider(),
                &self.cipher_suite_provider,
                &self.config.secret_store(),
                &mls_rules,
                time,
                CommitDirection::Send,
            )
            .await?;

        let (mut provisional_private_tree, _) =
            self.provisional_private_tree(&provisional_state)?;

        if is_external {
            provisional_private_tree.self_index = provisional_state
                .external_init_index
                .ok_or(MlsError::ExternalCommitMissingExternalInit)?;

            self.private_tree.self_index = provisional_private_tree.self_index;
        }

        let mut provisional_group_context = provisional_state.group_context;

        // Decide whether to populate the path field: If the path field is required based on the
        // proposals that are in the commit (see above), then it MUST be populated. Otherwise, the
        // sender MAY omit the path field at its discretion.
        let commit_options = mls_rules
            .commit_options(
                &provisional_state.public_tree.roster(),
                &provisional_group_context.extensions,
                &provisional_state.applied_proposals,
            )
            .map_err(|e| MlsError::MlsRulesError(e.into_any_error()))?;

        let perform_path_update = commit_options.path_required
            || path_update_required(&provisional_state.applied_proposals);

        let (update_path, path_secrets, commit_secret) = if perform_path_update {
            // If populating the path field: Create an UpdatePath using the new tree. Any new
            // member (from an add proposal) MUST be excluded from the resolution during the
            // computation of the UpdatePath. The GroupContext for this operation uses the
            // group_id, epoch, tree_hash, and confirmed_transcript_hash values in the initial
            // GroupContext object. The leaf_key_package for this UpdatePath must have a
            // parent_hash extension.
            let encap_gen = TreeKem::new(
                &mut provisional_state.public_tree,
                &mut provisional_private_tree,
            )
            .encap(
                &mut provisional_group_context,
                &provisional_state.indexes_of_added_kpkgs,
                new_signer_ref,
                self.config.leaf_properties(),
                new_signing_identity,
                &self.cipher_suite_provider,
                #[cfg(test)]
                &self.commit_modifiers,
            )
            .await?;

            (
                Some(encap_gen.update_path),
                Some(encap_gen.path_secrets),
                encap_gen.commit_secret,
            )
        } else {
            // Update the tree hash, since it was not updated by encap.
            provisional_state
                .public_tree
                .update_hashes(
                    &[provisional_private_tree.self_index],
                    &self.cipher_suite_provider,
                )
                .await?;

            provisional_group_context.tree_hash = provisional_state
                .public_tree
                .tree_hash(&self.cipher_suite_provider)
                .await?;

            (None, None, PathSecret::empty(&self.cipher_suite_provider))
        };

        #[cfg(feature = "psk")]
        let (psk_secret, psks) = self
            .get_psk(&provisional_state.applied_proposals.psks)
            .await?;

        #[cfg(not(feature = "psk"))]
        let psk_secret = self.get_psk();

        let added_key_pkgs: Vec<_> = provisional_state
            .applied_proposals
            .additions
            .iter()
            .map(|info| info.proposal.key_package.clone())
            .collect();

        let commit = Commit {
            proposals: provisional_state.applied_proposals.into_proposals_or_refs(),
            path: update_path,
        };

        let mut auth_content = AuthenticatedContent::new_signed(
            &self.cipher_suite_provider,
            self.context(),
            sender,
            Content::Commit(alloc::boxed::Box::new(commit)),
            old_signer,
            #[cfg(feature = "private_message")]
            self.encryption_options()?.control_wire_format(sender),
            #[cfg(not(feature = "private_message"))]
            WireFormat::PublicMessage,
            authenticated_data,
        )
        .await?;

        // Use the signature, the commit_secret and the psk_secret to advance the key schedule and
        // compute the confirmation_tag value in the MlsPlaintext.
        let confirmed_transcript_hash = ConfirmedTranscriptHash::create(
            self.cipher_suite_provider(),
            &self.state.interim_transcript_hash,
            &auth_content,
        )
        .await?;

        provisional_group_context.confirmed_transcript_hash = confirmed_transcript_hash;

        let key_schedule_result = KeySchedule::from_key_schedule(
            &self.key_schedule,
            &commit_secret,
            &provisional_group_context,
            #[cfg(any(feature = "secret_tree_access", feature = "private_message"))]
            self.state.public_tree.total_leaf_count(),
            &psk_secret,
            &self.cipher_suite_provider,
        )
        .await?;

        let confirmation_tag = ConfirmationTag::create(
            &key_schedule_result.confirmation_key,
            &provisional_group_context.confirmed_transcript_hash,
            &self.cipher_suite_provider,
        )
        .await?;

        auth_content.auth.confirmation_tag = Some(confirmation_tag.clone());

        let ratchet_tree_ext = commit_options
            .ratchet_tree_extension
            .then(|| RatchetTreeExt {
                tree_data: ExportedTree::new(provisional_state.public_tree.nodes.clone()),
            });

        // Generate external commit group info if required by commit_options
        let external_commit_group_info = match commit_options.allow_external_commit {
            true => {
                let mut extensions = ExtensionList::new();

                extensions.set_from({
                    key_schedule_result
                        .key_schedule
                        .get_external_key_pair_ext(&self.cipher_suite_provider)
                        .await?
                })?;

                if let Some(ref ratchet_tree_ext) = ratchet_tree_ext {
                    extensions.set_from(ratchet_tree_ext.clone())?;
                }

                let info = self
                    .make_group_info(
                        &provisional_group_context,
                        extensions,
                        &confirmation_tag,
                        new_signer_ref,
                    )
                    .await?;

                let msg =
                    MlsMessage::new(self.protocol_version(), MlsMessagePayload::GroupInfo(info));

                Some(msg)
            }
            false => None,
        };

        // Build the group info that will be placed into the welcome messages.
        // Add the ratchet tree extension if necessary
        if let Some(ratchet_tree_ext) = ratchet_tree_ext {
            welcome_group_info_extensions.set_from(ratchet_tree_ext)?;
        }

        let welcome_group_info = self
            .make_group_info(
                &provisional_group_context,
                welcome_group_info_extensions,
                &confirmation_tag,
                new_signer_ref,
            )
            .await?;

        // Encrypt the GroupInfo using the key and nonce derived from the joiner_secret for
        // the new epoch
        let welcome_secret = WelcomeSecret::from_joiner_secret(
            &self.cipher_suite_provider,
            &key_schedule_result.joiner_secret,
            &psk_secret,
        )
        .await?;

        let encrypted_group_info = welcome_secret
            .encrypt(&welcome_group_info.mls_encode_to_vec()?)
            .await?;

        // Encrypt path secrets and joiner secret to new members
        let path_secrets = path_secrets.as_ref();

        #[cfg(not(any(mls_build_async, not(feature = "rayon"))))]
        let encrypted_path_secrets: Vec<_> = added_key_pkgs
            .into_par_iter()
            .zip(provisional_state.indexes_of_added_kpkgs)
            .map(|(key_package, leaf_index)| {
                self.encrypt_group_secrets(
                    &key_package,
                    leaf_index,
                    &key_schedule_result.joiner_secret,
                    path_secrets,
                    #[cfg(feature = "psk")]
                    psks.clone(),
                    &encrypted_group_info,
                )
            })
            .try_collect()?;

        #[cfg(any(mls_build_async, not(feature = "rayon")))]
        let encrypted_path_secrets = {
            let mut secrets = Vec::new();

            for (key_package, leaf_index) in added_key_pkgs
                .into_iter()
                .zip(provisional_state.indexes_of_added_kpkgs)
            {
                secrets.push(
                    self.encrypt_group_secrets(
                        &key_package,
                        leaf_index,
                        &key_schedule_result.joiner_secret,
                        path_secrets,
                        #[cfg(feature = "psk")]
                        psks.clone(),
                        &encrypted_group_info,
                    )
                    .await?,
                );
            }

            secrets
        };

        let welcome_messages =
            if commit_options.single_welcome_message && !encrypted_path_secrets.is_empty() {
                vec![self.make_welcome_message(encrypted_path_secrets, encrypted_group_info)]
            } else {
                encrypted_path_secrets
                    .into_iter()
                    .map(|s| self.make_welcome_message(vec![s], encrypted_group_info.clone()))
                    .collect()
            };

        let commit_message = self.format_for_wire(auth_content.clone()).await?;

        let pending_commit = CommitGeneration {
            content: auth_content,
            pending_private_tree: provisional_private_tree,
            pending_commit_secret: commit_secret,
            commit_message_hash: CommitHash::compute(&self.cipher_suite_provider, &commit_message)
                .await?,
        };

        self.pending_commit = Some(pending_commit);

        let ratchet_tree = (!commit_options.ratchet_tree_extension)
            .then(|| ExportedTree::new(provisional_state.public_tree.nodes));

        if let Some(signer) = new_signer {
            self.signer = signer;
        }

        Ok(CommitOutput {
            commit_message,
            welcome_messages,
            ratchet_tree,
            external_commit_group_info,
            #[cfg(feature = "by_ref_proposal")]
            unused_proposals: provisional_state.unused_proposals,
        })
    }

    // Construct a GroupInfo reflecting the new state
    // Group ID, epoch, tree, and confirmed transcript hash from the new state
    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    async fn make_group_info(
        &self,
        group_context: &GroupContext,
        extensions: ExtensionList,
        confirmation_tag: &ConfirmationTag,
        signer: &SignatureSecretKey,
    ) -> Result<GroupInfo, MlsError> {
        let mut group_info = GroupInfo {
            group_context: group_context.clone(),
            extensions,
            confirmation_tag: confirmation_tag.clone(), // The confirmation_tag from the MlsPlaintext object
            signer: LeafIndex(self.current_member_index()),
            signature: vec![],
        };

        group_info.grease(self.cipher_suite_provider())?;

        // Sign the GroupInfo using the member's private signing key
        group_info
            .sign(&self.cipher_suite_provider, signer, &())
            .await?;

        Ok(group_info)
    }

    fn make_welcome_message(
        &self,
        secrets: Vec<EncryptedGroupSecrets>,
        encrypted_group_info: Vec<u8>,
    ) -> MlsMessage {
        MlsMessage::new(
            self.context().protocol_version,
            MlsMessagePayload::Welcome(Welcome {
                cipher_suite: self.context().cipher_suite,
                secrets,
                encrypted_group_info,
            }),
        )
    }
}

#[cfg(test)]
pub(crate) mod test_utils {
    use alloc::vec::Vec;

    use crate::{
        crypto::SignatureSecretKey,
        tree_kem::{leaf_node::LeafNode, TreeKemPublic, UpdatePathNode},
    };

    #[derive(Copy, Clone, Debug)]
    pub struct CommitModifiers {
        pub modify_leaf: fn(&mut LeafNode, &SignatureSecretKey) -> Option<SignatureSecretKey>,
        pub modify_tree: fn(&mut TreeKemPublic),
        pub modify_path: fn(Vec<UpdatePathNode>) -> Vec<UpdatePathNode>,
    }

    impl Default for CommitModifiers {
        fn default() -> Self {
            Self {
                modify_leaf: |_, _| None,
                modify_tree: |_| (),
                modify_path: |a| a,
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use alloc::boxed::Box;

    use mls_rs_core::{
        error::IntoAnyError,
        extension::ExtensionType,
        identity::{CredentialType, IdentityProvider},
        time::MlsTime,
    };

    use crate::{
        crypto::test_utils::{test_cipher_suite_provider, TestCryptoProvider},
        group::{mls_rules::DefaultMlsRules, test_utils::test_group_custom},
        mls_rules::CommitOptions,
        Client,
    };

    #[cfg(feature = "by_ref_proposal")]
    use crate::extension::ExternalSendersExt;

    use crate::{
        client::test_utils::{test_client_with_key_pkg, TEST_CIPHER_SUITE, TEST_PROTOCOL_VERSION},
        client_builder::{
            test_utils::TestClientConfig, BaseConfig, ClientBuilder, WithCryptoProvider,
            WithIdentityProvider,
        },
        client_config::ClientConfig,
        extension::test_utils::{TestExtension, TEST_EXTENSION_TYPE},
        group::{
            proposal::ProposalType,
            test_utils::{test_group_custom_config, test_n_member_group},
        },
        identity::test_utils::get_test_signing_identity,
        identity::{basic::BasicIdentityProvider, test_utils::get_test_basic_credential},
        key_package::test_utils::test_key_package_message,
    };

    use crate::extension::RequiredCapabilitiesExt;

    #[cfg(feature = "psk")]
    use crate::{
        group::proposal::PreSharedKeyProposal,
        psk::{JustPreSharedKeyID, PreSharedKey, PreSharedKeyID},
    };

    use super::*;

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    async fn test_commit_builder_group() -> Group<TestClientConfig> {
        test_group_custom_config(TEST_PROTOCOL_VERSION, TEST_CIPHER_SUITE, |b| {
            b.custom_proposal_type(ProposalType::from(42))
                .extension_type(TEST_EXTENSION_TYPE.into())
        })
        .await
        .group
    }

    fn assert_commit_builder_output<C: ClientConfig>(
        group: Group<C>,
        mut commit_output: CommitOutput,
        expected: Vec<Proposal>,
        welcome_count: usize,
    ) {
        let plaintext = commit_output.commit_message.into_plaintext().unwrap();

        let commit_data = match plaintext.content.content {
            Content::Commit(commit) => commit,
            #[cfg(any(feature = "private_message", feature = "by_ref_proposal"))]
            _ => panic!("Found non-commit data"),
        };

        assert_eq!(commit_data.proposals.len(), expected.len());

        commit_data.proposals.into_iter().for_each(|proposal| {
            let proposal = match proposal {
                ProposalOrRef::Proposal(p) => p,
                #[cfg(feature = "by_ref_proposal")]
                ProposalOrRef::Reference(_) => panic!("found proposal reference"),
            };

            #[cfg(feature = "psk")]
            if let Some(psk_id) = match proposal.as_ref() {
                Proposal::Psk(PreSharedKeyProposal { psk: PreSharedKeyID { key_id: JustPreSharedKeyID::External(psk_id), .. },}) => Some(psk_id),
                _ => None,
            } {
                let found = expected.iter().any(|item| matches!(item, Proposal::Psk(PreSharedKeyProposal { psk: PreSharedKeyID { key_id: JustPreSharedKeyID::External(id), .. }}) if id == psk_id));

                assert!(found)
            } else {
                assert!(expected.contains(&proposal));
            }

            #[cfg(not(feature = "psk"))]
            assert!(expected.contains(&proposal));
        });

        if welcome_count > 0 {
            let welcome_msg = commit_output.welcome_messages.pop().unwrap();

            assert_eq!(welcome_msg.version, group.state.context.protocol_version);

            let welcome_msg = welcome_msg.into_welcome().unwrap();

            assert_eq!(welcome_msg.cipher_suite, group.state.context.cipher_suite);
            assert_eq!(welcome_msg.secrets.len(), welcome_count);
        } else {
            assert!(commit_output.welcome_messages.is_empty());
        }
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn test_commit_builder_add() {
        let mut group = test_commit_builder_group().await;

        let test_key_package =
            test_key_package_message(TEST_PROTOCOL_VERSION, TEST_CIPHER_SUITE, "alice").await;

        let commit_output = group
            .commit_builder()
            .add_member(test_key_package.clone())
            .unwrap()
            .build()
            .await
            .unwrap();

        let expected_add = group.add_proposal(test_key_package).unwrap();

        assert_commit_builder_output(group, commit_output, vec![expected_add], 1)
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn test_commit_builder_add_with_ext() {
        let mut group = test_commit_builder_group().await;

        let (bob_client, bob_key_package) =
            test_client_with_key_pkg(TEST_PROTOCOL_VERSION, TEST_CIPHER_SUITE, "bob").await;

        let ext = TestExtension { foo: 42 };
        let mut extension_list = ExtensionList::default();
        extension_list.set_from(ext.clone()).unwrap();

        let welcome_message = group
            .commit_builder()
            .add_member(bob_key_package)
            .unwrap()
            .set_group_info_ext(extension_list)
            .build()
            .await
            .unwrap()
            .welcome_messages
            .remove(0);

        let (_, context) = bob_client.join_group(None, &welcome_message).await.unwrap();

        assert_eq!(
            context
                .group_info_extensions
                .get_as::<TestExtension>()
                .unwrap()
                .unwrap(),
            ext
        );
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn test_commit_builder_remove() {
        let mut group = test_commit_builder_group().await;
        let test_key_package =
            test_key_package_message(TEST_PROTOCOL_VERSION, TEST_CIPHER_SUITE, "alice").await;

        group
            .commit_builder()
            .add_member(test_key_package)
            .unwrap()
            .build()
            .await
            .unwrap();

        group.apply_pending_commit().await.unwrap();

        let commit_output = group
            .commit_builder()
            .remove_member(1)
            .unwrap()
            .build()
            .await
            .unwrap();

        let expected_remove = group.remove_proposal(1).unwrap();

        assert_commit_builder_output(group, commit_output, vec![expected_remove], 0);
    }

    #[cfg(feature = "psk")]
    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn test_commit_builder_psk() {
        let mut group = test_commit_builder_group().await;
        let test_psk = ExternalPskId::new(vec![1]);

        group
            .config
            .secret_store()
            .insert(test_psk.clone(), PreSharedKey::from(vec![1]));

        let commit_output = group
            .commit_builder()
            .add_external_psk(test_psk.clone())
            .unwrap()
            .build()
            .await
            .unwrap();

        let key_id = JustPreSharedKeyID::External(test_psk);
        let expected_psk = group.psk_proposal(key_id).unwrap();

        assert_commit_builder_output(group, commit_output, vec![expected_psk], 0)
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn test_commit_builder_group_context_ext() {
        let mut group = test_commit_builder_group().await;
        let mut test_ext = ExtensionList::default();
        test_ext
            .set_from(RequiredCapabilitiesExt::default())
            .unwrap();

        let commit_output = group
            .commit_builder()
            .set_group_context_ext(test_ext.clone())
            .unwrap()
            .build()
            .await
            .unwrap();

        let expected_ext = group.group_context_extensions_proposal(test_ext);

        assert_commit_builder_output(group, commit_output, vec![expected_ext], 0);
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn test_commit_builder_reinit() {
        let mut group = test_commit_builder_group().await;
        let test_group_id = "foo".as_bytes().to_vec();
        let test_cipher_suite = TEST_CIPHER_SUITE;
        let test_protocol_version = TEST_PROTOCOL_VERSION;
        let mut test_ext = ExtensionList::default();

        test_ext
            .set_from(RequiredCapabilitiesExt::default())
            .unwrap();

        let commit_output = group
            .commit_builder()
            .reinit(
                Some(test_group_id.clone()),
                test_protocol_version,
                test_cipher_suite,
                test_ext.clone(),
            )
            .unwrap()
            .build()
            .await
            .unwrap();

        let expected_reinit = group
            .reinit_proposal(
                Some(test_group_id),
                test_protocol_version,
                test_cipher_suite,
                test_ext,
            )
            .unwrap();

        assert_commit_builder_output(group, commit_output, vec![expected_reinit], 0);
    }

    #[cfg(feature = "custom_proposal")]
    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn test_commit_builder_custom_proposal() {
        let mut group = test_commit_builder_group().await;

        let proposal = CustomProposal::new(42.into(), vec![0, 1]);

        let commit_output = group
            .commit_builder()
            .custom_proposal(proposal.clone())
            .build()
            .await
            .unwrap();

        assert_commit_builder_output(group, commit_output, vec![Proposal::Custom(proposal)], 0);
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn test_commit_builder_chaining() {
        let mut group = test_commit_builder_group().await;
        let kp1 = test_key_package_message(TEST_PROTOCOL_VERSION, TEST_CIPHER_SUITE, "alice").await;
        let kp2 = test_key_package_message(TEST_PROTOCOL_VERSION, TEST_CIPHER_SUITE, "bob").await;

        let expected_adds = vec![
            group.add_proposal(kp1.clone()).unwrap(),
            group.add_proposal(kp2.clone()).unwrap(),
        ];

        let commit_output = group
            .commit_builder()
            .add_member(kp1)
            .unwrap()
            .add_member(kp2)
            .unwrap()
            .build()
            .await
            .unwrap();

        assert_commit_builder_output(group, commit_output, expected_adds, 2);
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn test_commit_builder_empty_commit() {
        let mut group = test_commit_builder_group().await;

        let commit_output = group.commit_builder().build().await.unwrap();

        assert_commit_builder_output(group, commit_output, vec![], 0);
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn test_commit_builder_authenticated_data() {
        let mut group = test_commit_builder_group().await;
        let test_data = "test".as_bytes().to_vec();

        let commit_output = group
            .commit_builder()
            .authenticated_data(test_data.clone())
            .build()
            .await
            .unwrap();

        assert_eq!(
            commit_output
                .commit_message
                .into_plaintext()
                .unwrap()
                .content
                .authenticated_data,
            test_data
        );
    }

    #[cfg(feature = "by_ref_proposal")]
    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn test_commit_builder_multiple_welcome_messages() {
        let mut group = test_group_custom_config(TEST_PROTOCOL_VERSION, TEST_CIPHER_SUITE, |b| {
            let options = CommitOptions::new().with_single_welcome_message(false);
            b.mls_rules(DefaultMlsRules::new().with_commit_options(options))
        })
        .await;

        let (alice, alice_kp) =
            test_client_with_key_pkg(TEST_PROTOCOL_VERSION, TEST_CIPHER_SUITE, "a").await;

        let (bob, bob_kp) =
            test_client_with_key_pkg(TEST_PROTOCOL_VERSION, TEST_CIPHER_SUITE, "b").await;

        group
            .group
            .propose_add(alice_kp.clone(), vec![])
            .await
            .unwrap();

        group
            .group
            .propose_add(bob_kp.clone(), vec![])
            .await
            .unwrap();

        let output = group.group.commit(Vec::new()).await.unwrap();
        let welcomes = output.welcome_messages;

        let cs = test_cipher_suite_provider(TEST_CIPHER_SUITE);

        for (client, kp) in [(alice, alice_kp), (bob, bob_kp)] {
            let kp_ref = kp.key_package_reference(&cs).await.unwrap().unwrap();

            let welcome = welcomes
                .iter()
                .find(|w| w.welcome_key_package_references().contains(&&kp_ref))
                .unwrap();

            client.join_group(None, welcome).await.unwrap();

            assert_eq!(welcome.clone().into_welcome().unwrap().secrets.len(), 1);
        }
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn commit_can_change_credential() {
        let cs = TEST_CIPHER_SUITE;
        let mut groups = test_n_member_group(TEST_PROTOCOL_VERSION, cs, 3).await;
        let (identity, secret_key) = get_test_signing_identity(cs, b"member").await;

        let commit_output = groups[0]
            .group
            .commit_builder()
            .set_new_signing_identity(secret_key, identity.clone())
            .build()
            .await
            .unwrap();

        // Check that the credential was updated by in the committer's state.
        groups[0].process_pending_commit().await.unwrap();
        let new_member = groups[0].group.roster().member_with_index(0).unwrap();

        assert_eq!(
            new_member.signing_identity.credential,
            get_test_basic_credential(b"member".to_vec())
        );

        assert_eq!(
            new_member.signing_identity.signature_key,
            identity.signature_key
        );

        // Check that the credential was updated in another member's state.
        groups[1]
            .process_message(commit_output.commit_message)
            .await
            .unwrap();

        let new_member = groups[1].group.roster().member_with_index(0).unwrap();

        assert_eq!(
            new_member.signing_identity.credential,
            get_test_basic_credential(b"member".to_vec())
        );

        assert_eq!(
            new_member.signing_identity.signature_key,
            identity.signature_key
        );
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn commit_includes_tree_if_no_ratchet_tree_ext() {
        let mut group = test_group_custom(
            TEST_PROTOCOL_VERSION,
            TEST_CIPHER_SUITE,
            Default::default(),
            None,
            Some(CommitOptions::new().with_ratchet_tree_extension(false)),
        )
        .await
        .group;

        let commit = group.commit(vec![]).await.unwrap();

        group.apply_pending_commit().await.unwrap();

        let new_tree = group.export_tree();

        assert_eq!(new_tree, commit.ratchet_tree.unwrap())
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn commit_does_not_include_tree_if_ratchet_tree_ext() {
        let mut group = test_group_custom(
            TEST_PROTOCOL_VERSION,
            TEST_CIPHER_SUITE,
            Default::default(),
            None,
            Some(CommitOptions::new().with_ratchet_tree_extension(true)),
        )
        .await
        .group;

        let commit = group.commit(vec![]).await.unwrap();

        assert!(commit.ratchet_tree.is_none());
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn commit_includes_external_commit_group_info_if_requested() {
        let mut group = test_group_custom(
            TEST_PROTOCOL_VERSION,
            TEST_CIPHER_SUITE,
            Default::default(),
            None,
            Some(
                CommitOptions::new()
                    .with_allow_external_commit(true)
                    .with_ratchet_tree_extension(false),
            ),
        )
        .await
        .group;

        let commit = group.commit(vec![]).await.unwrap();

        let info = commit
            .external_commit_group_info
            .unwrap()
            .into_group_info()
            .unwrap();

        assert!(!info.extensions.has_extension(ExtensionType::RATCHET_TREE));
        assert!(info.extensions.has_extension(ExtensionType::EXTERNAL_PUB));
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn commit_includes_external_commit_and_tree_if_requested() {
        let mut group = test_group_custom(
            TEST_PROTOCOL_VERSION,
            TEST_CIPHER_SUITE,
            Default::default(),
            None,
            Some(
                CommitOptions::new()
                    .with_allow_external_commit(true)
                    .with_ratchet_tree_extension(true),
            ),
        )
        .await
        .group;

        let commit = group.commit(vec![]).await.unwrap();

        let info = commit
            .external_commit_group_info
            .unwrap()
            .into_group_info()
            .unwrap();

        assert!(info.extensions.has_extension(ExtensionType::RATCHET_TREE));
        assert!(info.extensions.has_extension(ExtensionType::EXTERNAL_PUB));
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn commit_does_not_include_external_commit_group_info_if_not_requested() {
        let mut group = test_group_custom(
            TEST_PROTOCOL_VERSION,
            TEST_CIPHER_SUITE,
            Default::default(),
            None,
            Some(CommitOptions::new().with_allow_external_commit(false)),
        )
        .await
        .group;

        let commit = group.commit(vec![]).await.unwrap();

        assert!(commit.external_commit_group_info.is_none());
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn member_identity_is_validated_against_new_extensions() {
        let alice = client_with_test_extension(b"alice").await;
        let mut alice = alice.create_group(ExtensionList::new()).await.unwrap();

        let bob = client_with_test_extension(b"bob").await;
        let bob_kp = bob.generate_key_package_message().await.unwrap();

        let mut extension_list = ExtensionList::new();
        let extension = TestExtension { foo: b'a' };
        extension_list.set_from(extension).unwrap();

        let res = alice
            .commit_builder()
            .add_member(bob_kp)
            .unwrap()
            .set_group_context_ext(extension_list.clone())
            .unwrap()
            .build()
            .await;

        assert!(res.is_err());

        let alex = client_with_test_extension(b"alex").await;

        alice
            .commit_builder()
            .add_member(alex.generate_key_package_message().await.unwrap())
            .unwrap()
            .set_group_context_ext(extension_list.clone())
            .unwrap()
            .build()
            .await
            .unwrap();
    }

    #[cfg(feature = "by_ref_proposal")]
    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn server_identity_is_validated_against_new_extensions() {
        let alice = client_with_test_extension(b"alice").await;
        let mut alice = alice.create_group(ExtensionList::new()).await.unwrap();

        let mut extension_list = ExtensionList::new();
        let extension = TestExtension { foo: b'a' };
        extension_list.set_from(extension).unwrap();

        let (alex_server, _) = get_test_signing_identity(TEST_CIPHER_SUITE, b"alex").await;

        let mut alex_extensions = extension_list.clone();

        alex_extensions
            .set_from(ExternalSendersExt {
                allowed_senders: vec![alex_server],
            })
            .unwrap();

        let res = alice
            .commit_builder()
            .set_group_context_ext(alex_extensions)
            .unwrap()
            .build()
            .await;

        assert!(res.is_err());

        let (bob_server, _) = get_test_signing_identity(TEST_CIPHER_SUITE, b"bob").await;

        let mut bob_extensions = extension_list;

        bob_extensions
            .set_from(ExternalSendersExt {
                allowed_senders: vec![bob_server],
            })
            .unwrap();

        alice
            .commit_builder()
            .set_group_context_ext(bob_extensions)
            .unwrap()
            .build()
            .await
            .unwrap();
    }

    #[derive(Debug, Clone)]
    struct IdentityProviderWithExtension(BasicIdentityProvider);

    #[derive(Clone, Debug)]
    #[cfg_attr(feature = "std", derive(thiserror::Error))]
    #[cfg_attr(feature = "std", error("test error"))]
    struct IdentityProviderWithExtensionError {}

    impl IntoAnyError for IdentityProviderWithExtensionError {
        #[cfg(feature = "std")]
        fn into_dyn_error(self) -> Result<Box<dyn std::error::Error + Send + Sync>, Self> {
            Ok(self.into())
        }
    }

    impl IdentityProviderWithExtension {
        // True if the identity starts with the character `foo` from `TestExtension` or if `TestExtension`
        // is not set.
        #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
        async fn starts_with_foo(
            &self,
            identity: &SigningIdentity,
            _timestamp: Option<MlsTime>,
            extensions: Option<&ExtensionList>,
        ) -> bool {
            if let Some(extensions) = extensions {
                if let Some(ext) = extensions.get_as::<TestExtension>().unwrap() {
                    self.identity(identity, extensions).await.unwrap()[0] == ext.foo
                } else {
                    true
                }
            } else {
                true
            }
        }
    }

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    #[cfg_attr(mls_build_async, maybe_async::must_be_async)]
    impl IdentityProvider for IdentityProviderWithExtension {
        type Error = IdentityProviderWithExtensionError;

        async fn validate_member(
            &self,
            identity: &SigningIdentity,
            timestamp: Option<MlsTime>,
            extensions: Option<&ExtensionList>,
        ) -> Result<(), Self::Error> {
            self.starts_with_foo(identity, timestamp, extensions)
                .await
                .then_some(())
                .ok_or(IdentityProviderWithExtensionError {})
        }

        async fn validate_external_sender(
            &self,
            identity: &SigningIdentity,
            timestamp: Option<MlsTime>,
            extensions: Option<&ExtensionList>,
        ) -> Result<(), Self::Error> {
            (!self.starts_with_foo(identity, timestamp, extensions).await)
                .then_some(())
                .ok_or(IdentityProviderWithExtensionError {})
        }

        async fn identity(
            &self,
            signing_identity: &SigningIdentity,
            extensions: &ExtensionList,
        ) -> Result<Vec<u8>, Self::Error> {
            self.0
                .identity(signing_identity, extensions)
                .await
                .map_err(|_| IdentityProviderWithExtensionError {})
        }

        async fn valid_successor(
            &self,
            _predecessor: &SigningIdentity,
            _successor: &SigningIdentity,
            _extensions: &ExtensionList,
        ) -> Result<bool, Self::Error> {
            Ok(true)
        }

        fn supported_types(&self) -> Vec<CredentialType> {
            self.0.supported_types()
        }
    }

    type ExtensionClientConfig = WithIdentityProvider<
        IdentityProviderWithExtension,
        WithCryptoProvider<TestCryptoProvider, BaseConfig>,
    >;

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    async fn client_with_test_extension(name: &[u8]) -> Client<ExtensionClientConfig> {
        let (identity, secret_key) = get_test_signing_identity(TEST_CIPHER_SUITE, name).await;

        ClientBuilder::new()
            .crypto_provider(TestCryptoProvider::new())
            .extension_types(vec![TEST_EXTENSION_TYPE.into()])
            .identity_provider(IdentityProviderWithExtension(BasicIdentityProvider::new()))
            .signing_identity(identity, secret_key, TEST_CIPHER_SUITE)
            .build()
    }
}
