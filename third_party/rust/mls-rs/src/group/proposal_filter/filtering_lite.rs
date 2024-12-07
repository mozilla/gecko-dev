// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use crate::{
    client::MlsError,
    group::proposal_filter::ProposalBundle,
    iter::wrap_iter,
    protocol_version::ProtocolVersion,
    time::MlsTime,
    tree_kem::{leaf_node_validator::LeafNodeValidator, node::LeafIndex},
    CipherSuiteProvider, ExtensionList,
};

use super::filtering_common::{filter_out_invalid_psks, ApplyProposalsOutput, ProposalApplier};

#[cfg(feature = "by_ref_proposal")]
use {crate::extension::ExternalSendersExt, mls_rs_core::error::IntoAnyError};

use mls_rs_core::{identity::IdentityProvider, psk::PreSharedKeyStorage};

#[cfg(feature = "custom_proposal")]
use itertools::Itertools;

#[cfg(all(not(mls_build_async), feature = "rayon"))]
use rayon::prelude::*;

#[cfg(mls_build_async)]
use futures::{StreamExt, TryStreamExt};

#[cfg(feature = "custom_proposal")]
use crate::tree_kem::TreeKemPublic;

#[cfg(feature = "psk")]
use crate::group::{
    proposal::PreSharedKeyProposal, JustPreSharedKeyID, ResumptionPSKUsage, ResumptionPsk,
};

#[cfg(all(feature = "std", feature = "psk"))]
use std::collections::HashSet;

impl<'a, C, P, CSP> ProposalApplier<'a, C, P, CSP>
where
    C: IdentityProvider,
    P: PreSharedKeyStorage,
    CSP: CipherSuiteProvider,
{
    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub(super) async fn apply_proposals_from_member(
        &self,
        commit_sender: LeafIndex,
        proposals: &ProposalBundle,
        commit_time: Option<MlsTime>,
    ) -> Result<ApplyProposalsOutput, MlsError> {
        filter_out_removal_of_committer(commit_sender, proposals)?;
        filter_out_invalid_psks(self.cipher_suite_provider, proposals, self.psk_storage).await?;

        #[cfg(feature = "by_ref_proposal")]
        filter_out_invalid_group_extensions(proposals, self.identity_provider, commit_time).await?;

        filter_out_extra_group_context_extensions(proposals)?;
        filter_out_invalid_reinit(proposals, self.protocol_version)?;
        filter_out_reinit_if_other_proposals(proposals)?;

        self.apply_proposal_changes(proposals, commit_time).await
    }

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub(super) async fn apply_proposal_changes(
        &self,
        proposals: &ProposalBundle,
        commit_time: Option<MlsTime>,
    ) -> Result<ApplyProposalsOutput, MlsError> {
        match proposals.group_context_extensions_proposal().cloned() {
            Some(p) => {
                self.apply_proposals_with_new_capabilities(proposals, p, commit_time)
                    .await
            }
            None => {
                self.apply_tree_changes(proposals, self.original_group_extensions, commit_time)
                    .await
            }
        }
    }

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub(super) async fn apply_tree_changes(
        &self,
        proposals: &ProposalBundle,
        group_extensions_in_use: &ExtensionList,
        commit_time: Option<MlsTime>,
    ) -> Result<ApplyProposalsOutput, MlsError> {
        self.validate_new_nodes(proposals, group_extensions_in_use, commit_time)
            .await?;

        let mut new_tree = self.original_tree.clone();

        let added = new_tree
            .batch_edit_lite(
                proposals,
                group_extensions_in_use,
                self.identity_provider,
                self.cipher_suite_provider,
            )
            .await?;

        let new_context_extensions = proposals
            .group_context_extensions
            .first()
            .map(|gce| gce.proposal.clone());

        Ok(ApplyProposalsOutput {
            new_tree,
            indexes_of_added_kpkgs: added,
            external_init_index: None,
            new_context_extensions,
        })
    }

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    async fn validate_new_nodes(
        &self,
        proposals: &ProposalBundle,
        group_extensions_in_use: &ExtensionList,
        commit_time: Option<MlsTime>,
    ) -> Result<(), MlsError> {
        let leaf_node_validator = &LeafNodeValidator::new(
            self.cipher_suite_provider,
            self.identity_provider,
            Some(group_extensions_in_use),
        );

        let adds = wrap_iter(proposals.add_proposals());

        #[cfg(mls_build_async)]
        let adds = adds.map(Ok);

        { adds }
            .try_for_each(|p| {
                self.validate_new_node(leaf_node_validator, &p.proposal.key_package, commit_time)
            })
            .await
    }
}

fn filter_out_removal_of_committer(
    commit_sender: LeafIndex,
    proposals: &ProposalBundle,
) -> Result<(), MlsError> {
    for p in &proposals.removals {
        (p.proposal.to_remove != commit_sender)
            .then_some(())
            .ok_or(MlsError::CommitterSelfRemoval)?;
    }

    Ok(())
}

#[cfg(feature = "by_ref_proposal")]
#[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
async fn filter_out_invalid_group_extensions<C>(
    proposals: &ProposalBundle,
    identity_provider: &C,
    commit_time: Option<MlsTime>,
) -> Result<(), MlsError>
where
    C: IdentityProvider,
{
    if let Some(p) = proposals.group_context_extensions.first() {
        if let Some(ext) = p.proposal.get_as::<ExternalSendersExt>()? {
            ext.verify_all(identity_provider, commit_time, p.proposal())
                .await
                .map_err(|e| MlsError::IdentityProviderError(e.into_any_error()))?;
        }
    }

    Ok(())
}

fn filter_out_extra_group_context_extensions(proposals: &ProposalBundle) -> Result<(), MlsError> {
    (proposals.group_context_extensions.len() < 2)
        .then_some(())
        .ok_or(MlsError::MoreThanOneGroupContextExtensionsProposal)
}

fn filter_out_invalid_reinit(
    proposals: &ProposalBundle,
    protocol_version: ProtocolVersion,
) -> Result<(), MlsError> {
    if let Some(p) = proposals.reinitializations.first() {
        (p.proposal.version >= protocol_version)
            .then_some(())
            .ok_or(MlsError::InvalidProtocolVersionInReInit)?;
    }

    Ok(())
}

fn filter_out_reinit_if_other_proposals(proposals: &ProposalBundle) -> Result<(), MlsError> {
    (proposals.reinitializations.is_empty() || proposals.length() == 1)
        .then_some(())
        .ok_or(MlsError::OtherProposalWithReInit)
}

#[cfg(feature = "custom_proposal")]
pub(super) fn filter_out_unsupported_custom_proposals(
    proposals: &ProposalBundle,
    tree: &TreeKemPublic,
) -> Result<(), MlsError> {
    let supported_types = proposals
        .custom_proposal_types()
        .filter(|t| tree.can_support_proposal(*t))
        .collect_vec();

    for p in &proposals.custom_proposals {
        let proposal_type = p.proposal.proposal_type();

        supported_types
            .contains(&proposal_type)
            .then_some(())
            .ok_or(MlsError::UnsupportedCustomProposal(proposal_type))?;
    }

    Ok(())
}
