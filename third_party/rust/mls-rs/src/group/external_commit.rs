// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use mls_rs_core::{crypto::SignatureSecretKey, identity::SigningIdentity};

use crate::{
    client_config::ClientConfig,
    group::{
        cipher_suite_provider,
        epoch::SenderDataSecret,
        key_schedule::{InitSecret, KeySchedule},
        proposal::{ExternalInit, Proposal, RemoveProposal},
        EpochSecrets, ExternalPubExt, LeafIndex, LeafNode, MlsError, TreeKemPrivate,
    },
    Group, MlsMessage,
};

#[cfg(any(feature = "secret_tree_access", feature = "private_message"))]
use crate::group::secret_tree::SecretTree;

#[cfg(feature = "custom_proposal")]
use crate::group::{
    framing::MlsMessagePayload,
    message_processor::{EventOrContent, MessageProcessor},
    message_signature::AuthenticatedContent,
    message_verifier::verify_plaintext_authentication,
    CustomProposal,
};

use alloc::vec;
use alloc::vec::Vec;

#[cfg(feature = "psk")]
use mls_rs_core::psk::{ExternalPskId, PreSharedKey};

#[cfg(feature = "psk")]
use crate::group::{
    PreSharedKeyProposal, {JustPreSharedKeyID, PreSharedKeyID},
};

use super::{validate_group_info_joiner, ExportedTree};

/// A builder that aids with the construction of an external commit.
#[cfg_attr(all(feature = "ffi", not(test)), safer_ffi_gen::ffi_type(opaque))]
pub struct ExternalCommitBuilder<C: ClientConfig> {
    signer: SignatureSecretKey,
    signing_identity: SigningIdentity,
    config: C,
    tree_data: Option<ExportedTree<'static>>,
    to_remove: Option<u32>,
    #[cfg(feature = "psk")]
    external_psks: Vec<ExternalPskId>,
    authenticated_data: Vec<u8>,
    #[cfg(feature = "custom_proposal")]
    custom_proposals: Vec<Proposal>,
    #[cfg(feature = "custom_proposal")]
    received_custom_proposals: Vec<MlsMessage>,
}

impl<C: ClientConfig> ExternalCommitBuilder<C> {
    pub(crate) fn new(
        signer: SignatureSecretKey,
        signing_identity: SigningIdentity,
        config: C,
    ) -> Self {
        Self {
            tree_data: None,
            to_remove: None,
            authenticated_data: Vec::new(),
            signer,
            signing_identity,
            config,
            #[cfg(feature = "psk")]
            external_psks: Vec::new(),
            #[cfg(feature = "custom_proposal")]
            custom_proposals: Vec::new(),
            #[cfg(feature = "custom_proposal")]
            received_custom_proposals: Vec::new(),
        }
    }

    #[must_use]
    /// Use external tree data if the GroupInfo message does not contain a
    /// [`RatchetTreeExt`](crate::extension::built_in::RatchetTreeExt)
    pub fn with_tree_data(self, tree_data: ExportedTree<'static>) -> Self {
        Self {
            tree_data: Some(tree_data),
            ..self
        }
    }

    #[must_use]
    /// Propose the removal of an old version of the client as part of the external commit.
    /// Only one such proposal is allowed.
    pub fn with_removal(self, to_remove: u32) -> Self {
        Self {
            to_remove: Some(to_remove),
            ..self
        }
    }

    #[must_use]
    /// Add plaintext authenticated data to the resulting commit message.
    pub fn with_authenticated_data(self, data: Vec<u8>) -> Self {
        Self {
            authenticated_data: data,
            ..self
        }
    }

    #[cfg(feature = "psk")]
    #[must_use]
    /// Add an external psk to the group as part of the external commit.
    pub fn with_external_psk(mut self, psk: ExternalPskId) -> Self {
        self.external_psks.push(psk);
        self
    }

    #[cfg(feature = "custom_proposal")]
    #[must_use]
    /// Insert a [`CustomProposal`] into the current commit that is being built.
    pub fn with_custom_proposal(mut self, proposal: CustomProposal) -> Self {
        self.custom_proposals.push(Proposal::Custom(proposal));
        self
    }

    #[cfg(all(feature = "custom_proposal", feature = "by_ref_proposal"))]
    #[must_use]
    /// Insert a [`CustomProposal`] received from a current group member into the current
    /// commit that is being built.
    ///
    /// # Warning
    ///
    /// The authenticity of the proposal is NOT fully verified. It is only verified the
    /// same way as by [`ExternalGroup`](`crate::external_client::ExternalGroup`).
    /// The proposal MUST be an MlsPlaintext, else the [`Self::build`] function will fail.
    pub fn with_received_custom_proposal(mut self, proposal: MlsMessage) -> Self {
        self.received_custom_proposals.push(proposal);
        self
    }

    /// Build the external commit using a GroupInfo message provided by an existing group member.
    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub async fn build(self, group_info: MlsMessage) -> Result<(Group<C>, MlsMessage), MlsError> {
        let protocol_version = group_info.version;

        if !self.config.version_supported(protocol_version) {
            return Err(MlsError::UnsupportedProtocolVersion(protocol_version));
        }

        let group_info = group_info
            .into_group_info()
            .ok_or(MlsError::UnexpectedMessageType)?;

        let cipher_suite = cipher_suite_provider(
            self.config.crypto_provider(),
            group_info.group_context.cipher_suite,
        )?;

        let external_pub_ext = group_info
            .extensions
            .get_as::<ExternalPubExt>()?
            .ok_or(MlsError::MissingExternalPubExtension)?;

        let public_tree = validate_group_info_joiner(
            protocol_version,
            &group_info,
            self.tree_data,
            &self.config.identity_provider(),
            &cipher_suite,
        )
        .await?;

        let (leaf_node, _) = LeafNode::generate(
            &cipher_suite,
            self.config.leaf_properties(),
            self.signing_identity,
            &self.signer,
            self.config.lifetime(),
        )
        .await?;

        let (init_secret, kem_output) =
            InitSecret::encode_for_external(&cipher_suite, &external_pub_ext.external_pub).await?;

        let epoch_secrets = EpochSecrets {
            #[cfg(feature = "psk")]
            resumption_secret: PreSharedKey::new(vec![]),
            sender_data_secret: SenderDataSecret::from(vec![]),
            #[cfg(any(feature = "secret_tree_access", feature = "private_message"))]
            secret_tree: SecretTree::empty(),
        };

        let (mut group, _) = Group::join_with(
            self.config,
            group_info,
            public_tree,
            KeySchedule::new(init_secret),
            epoch_secrets,
            TreeKemPrivate::new_for_external(),
            None,
            self.signer,
        )
        .await?;

        #[cfg(feature = "psk")]
        let psk_ids = self
            .external_psks
            .into_iter()
            .map(|psk_id| PreSharedKeyID::new(JustPreSharedKeyID::External(psk_id), &cipher_suite))
            .collect::<Result<Vec<_>, MlsError>>()?;

        let mut proposals = vec![Proposal::ExternalInit(ExternalInit { kem_output })];

        #[cfg(feature = "psk")]
        proposals.extend(
            psk_ids
                .into_iter()
                .map(|psk| Proposal::Psk(PreSharedKeyProposal { psk })),
        );

        #[cfg(feature = "custom_proposal")]
        {
            let mut custom_proposals = self.custom_proposals;
            proposals.append(&mut custom_proposals);
        }

        #[cfg(all(feature = "custom_proposal", feature = "by_ref_proposal"))]
        for message in self.received_custom_proposals {
            let MlsMessagePayload::Plain(plaintext) = message.payload else {
                return Err(MlsError::UnexpectedMessageType);
            };

            let auth_content = AuthenticatedContent::from(plaintext.clone());

            verify_plaintext_authentication(&cipher_suite, plaintext, None, None, &group.state)
                .await?;

            group
                .process_event_or_content(EventOrContent::Content(auth_content), true, None)
                .await?;
        }

        if let Some(r) = self.to_remove {
            proposals.push(Proposal::Remove(RemoveProposal {
                to_remove: LeafIndex(r),
            }));
        }

        let commit_output = group
            .commit_internal(
                proposals,
                Some(&leaf_node),
                self.authenticated_data,
                Default::default(),
                None,
                None,
            )
            .await?;

        group.apply_pending_commit().await?;

        Ok((group, commit_output.commit_message))
    }
}
