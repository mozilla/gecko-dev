// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use crate::{
    client::MlsError,
    group::{
        proposal::ReInitProposal,
        proposal_filter::{ProposalBundle, ProposalInfo},
        AddProposal, ProposalType, RemoveProposal, Sender, UpdateProposal,
    },
    iter::wrap_iter,
    protocol_version::ProtocolVersion,
    time::MlsTime,
    tree_kem::{
        leaf_node_validator::{LeafNodeValidator, ValidationContext},
        node::LeafIndex,
        TreeKemPublic,
    },
    CipherSuiteProvider, ExtensionList,
};

use super::filtering_common::{filter_out_invalid_psks, ApplyProposalsOutput, ProposalApplier};

#[cfg(feature = "by_ref_proposal")]
use crate::extension::ExternalSendersExt;

use alloc::vec::Vec;
use mls_rs_core::{error::IntoAnyError, identity::IdentityProvider, psk::PreSharedKeyStorage};

#[cfg(any(
    feature = "custom_proposal",
    not(any(mls_build_async, feature = "rayon"))
))]
use itertools::Itertools;

use crate::group::ExternalInit;

#[cfg(feature = "psk")]
use crate::group::proposal::PreSharedKeyProposal;

#[cfg(all(not(mls_build_async), feature = "rayon"))]
use {crate::iter::ParallelIteratorExt, rayon::prelude::*};

#[cfg(mls_build_async)]
use futures::{StreamExt, TryStreamExt};

impl<'a, C, P, CSP> ProposalApplier<'a, C, P, CSP>
where
    C: IdentityProvider,
    P: PreSharedKeyStorage,
    CSP: CipherSuiteProvider,
{
    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub(super) async fn apply_proposals_from_member(
        &self,
        strategy: FilterStrategy,
        commit_sender: LeafIndex,
        proposals: ProposalBundle,
        commit_time: Option<MlsTime>,
    ) -> Result<ApplyProposalsOutput, MlsError> {
        let proposals = filter_out_invalid_proposers(strategy, proposals)?;

        let mut proposals: ProposalBundle =
            filter_out_update_for_committer(strategy, commit_sender, proposals)?;

        // We ignore the strategy here because the check above ensures all updates are from members
        proposals.update_senders = proposals
            .updates
            .iter()
            .map(leaf_index_of_update_sender)
            .collect::<Result<_, _>>()?;

        let mut proposals = filter_out_removal_of_committer(strategy, commit_sender, proposals)?;

        filter_out_invalid_psks(
            strategy,
            self.cipher_suite_provider,
            &mut proposals,
            self.psk_storage,
        )
        .await?;

        #[cfg(feature = "by_ref_proposal")]
        let proposals = filter_out_invalid_group_extensions(
            strategy,
            proposals,
            self.identity_provider,
            commit_time,
        )
        .await?;

        let proposals = filter_out_extra_group_context_extensions(strategy, proposals)?;
        let proposals = filter_out_invalid_reinit(strategy, proposals, self.protocol_version)?;
        let proposals = filter_out_reinit_if_other_proposals(strategy.is_ignore(), proposals)?;

        let proposals = filter_out_external_init(strategy, proposals)?;

        self.apply_proposal_changes(strategy, proposals, commit_time)
            .await
    }

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub(super) async fn apply_proposal_changes(
        &self,
        strategy: FilterStrategy,
        proposals: ProposalBundle,
        commit_time: Option<MlsTime>,
    ) -> Result<ApplyProposalsOutput, MlsError> {
        match proposals.group_context_extensions_proposal().cloned() {
            Some(p) => {
                self.apply_proposals_with_new_capabilities(strategy, proposals, p, commit_time)
                    .await
            }
            None => {
                self.apply_tree_changes(
                    strategy,
                    proposals,
                    self.original_group_extensions,
                    commit_time,
                )
                .await
            }
        }
    }

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub(super) async fn apply_tree_changes(
        &self,
        strategy: FilterStrategy,
        proposals: ProposalBundle,
        group_extensions_in_use: &ExtensionList,
        commit_time: Option<MlsTime>,
    ) -> Result<ApplyProposalsOutput, MlsError> {
        let mut applied_proposals = self
            .validate_new_nodes(strategy, proposals, group_extensions_in_use, commit_time)
            .await?;

        let mut new_tree = self.original_tree.clone();

        let added = new_tree
            .batch_edit(
                &mut applied_proposals,
                group_extensions_in_use,
                self.identity_provider,
                self.cipher_suite_provider,
                strategy.is_ignore(),
            )
            .await?;

        let new_context_extensions = applied_proposals
            .group_context_extensions_proposal()
            .map(|gce| gce.proposal.clone());

        Ok(ApplyProposalsOutput {
            applied_proposals,
            new_tree,
            indexes_of_added_kpkgs: added,
            external_init_index: None,
            new_context_extensions,
        })
    }

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    async fn validate_new_nodes(
        &self,
        strategy: FilterStrategy,
        mut proposals: ProposalBundle,
        group_extensions_in_use: &ExtensionList,
        commit_time: Option<MlsTime>,
    ) -> Result<ProposalBundle, MlsError> {
        let leaf_node_validator = &LeafNodeValidator::new(
            self.cipher_suite_provider,
            self.identity_provider,
            Some(group_extensions_in_use),
        );

        let bad_indices: Vec<_> = wrap_iter(proposals.update_proposals())
            .zip(wrap_iter(proposals.update_proposal_senders()))
            .enumerate()
            .filter_map(|(i, (p, &sender_index))| async move {
                let res = {
                    let leaf = &p.proposal.leaf_node;

                    let res = leaf_node_validator
                        .check_if_valid(
                            leaf,
                            ValidationContext::Update((self.group_id, *sender_index, commit_time)),
                        )
                        .await;

                    let old_leaf = match self.original_tree.get_leaf_node(sender_index) {
                        Ok(leaf) => leaf,
                        Err(e) => return Some(Err(e)),
                    };

                    let valid_successor = self
                        .identity_provider
                        .valid_successor(
                            &old_leaf.signing_identity,
                            &leaf.signing_identity,
                            group_extensions_in_use,
                        )
                        .await
                        .map_err(|e| MlsError::IdentityProviderError(e.into_any_error()))
                        .and_then(|valid| valid.then_some(()).ok_or(MlsError::InvalidSuccessor));

                    res.and(valid_successor)
                };

                apply_strategy(strategy, p.is_by_reference(), res)
                    .map(|b| (!b).then_some(i))
                    .transpose()
            })
            .try_collect()
            .await?;

        bad_indices.into_iter().rev().for_each(|i| {
            proposals.remove::<UpdateProposal>(i);
            proposals.update_senders.remove(i);
        });

        let bad_indices: Vec<_> = wrap_iter(proposals.add_proposals())
            .enumerate()
            .filter_map(|(i, p)| async move {
                let res = self
                    .validate_new_node(leaf_node_validator, &p.proposal.key_package, commit_time)
                    .await;

                apply_strategy(strategy, p.is_by_reference(), res)
                    .map(|b| (!b).then_some(i))
                    .transpose()
            })
            .try_collect()
            .await?;

        bad_indices
            .into_iter()
            .rev()
            .for_each(|i| proposals.remove::<AddProposal>(i));

        Ok(proposals)
    }
}

#[derive(Clone, Copy, Debug)]
pub enum FilterStrategy {
    IgnoreByRef,
    IgnoreNone,
}

impl FilterStrategy {
    pub(super) fn ignore(self, by_ref: bool) -> bool {
        match self {
            FilterStrategy::IgnoreByRef => by_ref,
            FilterStrategy::IgnoreNone => false,
        }
    }

    fn is_ignore(self) -> bool {
        match self {
            FilterStrategy::IgnoreByRef => true,
            FilterStrategy::IgnoreNone => false,
        }
    }
}

pub(crate) fn apply_strategy(
    strategy: FilterStrategy,
    by_ref: bool,
    r: Result<(), MlsError>,
) -> Result<bool, MlsError> {
    r.map(|_| true)
        .or_else(|error| strategy.ignore(by_ref).then_some(false).ok_or(error))
}

fn filter_out_update_for_committer(
    strategy: FilterStrategy,
    commit_sender: LeafIndex,
    mut proposals: ProposalBundle,
) -> Result<ProposalBundle, MlsError> {
    proposals.retain_by_type::<UpdateProposal, _, _>(|p| {
        apply_strategy(
            strategy,
            p.is_by_reference(),
            (p.sender != Sender::Member(*commit_sender))
                .then_some(())
                .ok_or(MlsError::InvalidCommitSelfUpdate),
        )
    })?;
    Ok(proposals)
}

fn filter_out_removal_of_committer(
    strategy: FilterStrategy,
    commit_sender: LeafIndex,
    mut proposals: ProposalBundle,
) -> Result<ProposalBundle, MlsError> {
    proposals.retain_by_type::<RemoveProposal, _, _>(|p| {
        apply_strategy(
            strategy,
            p.is_by_reference(),
            (p.proposal.to_remove != commit_sender)
                .then_some(())
                .ok_or(MlsError::CommitterSelfRemoval),
        )
    })?;
    Ok(proposals)
}

#[cfg(feature = "by_ref_proposal")]
#[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
async fn filter_out_invalid_group_extensions<C>(
    strategy: FilterStrategy,
    mut proposals: ProposalBundle,
    identity_provider: &C,
    commit_time: Option<MlsTime>,
) -> Result<ProposalBundle, MlsError>
where
    C: IdentityProvider,
{
    let mut bad_indices = Vec::new();

    for (i, p) in proposals.by_type::<ExtensionList>().enumerate() {
        let ext = p.proposal.get_as::<ExternalSendersExt>();

        let res = match ext {
            Ok(None) => Ok(()),
            Ok(Some(extension)) => extension
                .verify_all(identity_provider, commit_time, &p.proposal)
                .await
                .map_err(|e| MlsError::IdentityProviderError(e.into_any_error())),
            Err(e) => Err(MlsError::from(e)),
        };

        if !apply_strategy(strategy, p.is_by_reference(), res)? {
            bad_indices.push(i);
        }
    }

    bad_indices
        .into_iter()
        .rev()
        .for_each(|i| proposals.remove::<ExtensionList>(i));

    Ok(proposals)
}

fn filter_out_extra_group_context_extensions(
    strategy: FilterStrategy,
    mut proposals: ProposalBundle,
) -> Result<ProposalBundle, MlsError> {
    let mut found = false;

    proposals.retain_by_type::<ExtensionList, _, _>(|p| {
        apply_strategy(
            strategy,
            p.is_by_reference(),
            (!core::mem::replace(&mut found, true))
                .then_some(())
                .ok_or(MlsError::MoreThanOneGroupContextExtensionsProposal),
        )
    })?;

    Ok(proposals)
}

fn filter_out_invalid_reinit(
    strategy: FilterStrategy,
    mut proposals: ProposalBundle,
    protocol_version: ProtocolVersion,
) -> Result<ProposalBundle, MlsError> {
    proposals.retain_by_type::<ReInitProposal, _, _>(|p| {
        apply_strategy(
            strategy,
            p.is_by_reference(),
            (p.proposal.version >= protocol_version)
                .then_some(())
                .ok_or(MlsError::InvalidProtocolVersionInReInit),
        )
    })?;

    Ok(proposals)
}

fn filter_out_reinit_if_other_proposals(
    filter: bool,
    mut proposals: ProposalBundle,
) -> Result<ProposalBundle, MlsError> {
    let proposal_count = proposals.length();

    let has_reinit_and_other_proposal =
        !proposals.reinit_proposals().is_empty() && proposal_count != 1;

    if has_reinit_and_other_proposal {
        let any_by_val = proposals.reinit_proposals().iter().any(|p| p.is_by_value());

        if any_by_val || !filter {
            return Err(MlsError::OtherProposalWithReInit);
        }

        let has_other_proposal_type = proposal_count > proposals.reinit_proposals().len();

        if has_other_proposal_type {
            proposals.reinitializations = Vec::new();
        } else {
            proposals.reinitializations.truncate(1);
        }
    }

    Ok(proposals)
}

fn filter_out_external_init(
    strategy: FilterStrategy,
    mut proposals: ProposalBundle,
) -> Result<ProposalBundle, MlsError> {
    proposals.retain_by_type::<ExternalInit, _, _>(|p| {
        apply_strategy(
            strategy,
            p.is_by_reference(),
            Err(MlsError::InvalidProposalTypeForSender),
        )
    })?;

    Ok(proposals)
}

pub(crate) fn proposer_can_propose(
    proposer: Sender,
    proposal_type: ProposalType,
    by_ref: bool,
) -> Result<(), MlsError> {
    let can_propose = match (proposer, by_ref) {
        (Sender::Member(_), false) => matches!(
            proposal_type,
            ProposalType::ADD
                | ProposalType::REMOVE
                | ProposalType::PSK
                | ProposalType::RE_INIT
                | ProposalType::GROUP_CONTEXT_EXTENSIONS
        ),
        (Sender::Member(_), true) => matches!(
            proposal_type,
            ProposalType::ADD
                | ProposalType::UPDATE
                | ProposalType::REMOVE
                | ProposalType::PSK
                | ProposalType::RE_INIT
                | ProposalType::GROUP_CONTEXT_EXTENSIONS
        ),
        #[cfg(feature = "by_ref_proposal")]
        (Sender::External(_), false) => false,
        #[cfg(feature = "by_ref_proposal")]
        (Sender::External(_), true) => matches!(
            proposal_type,
            ProposalType::ADD
                | ProposalType::REMOVE
                | ProposalType::RE_INIT
                | ProposalType::PSK
                | ProposalType::GROUP_CONTEXT_EXTENSIONS
        ),
        (Sender::NewMemberCommit, false) => matches!(
            proposal_type,
            ProposalType::REMOVE | ProposalType::PSK | ProposalType::EXTERNAL_INIT
        ),
        (Sender::NewMemberCommit, true) => false,
        (Sender::NewMemberProposal, false) => false,
        (Sender::NewMemberProposal, true) => matches!(proposal_type, ProposalType::ADD),
    };

    can_propose
        .then_some(())
        .ok_or(MlsError::InvalidProposalTypeForSender)
}

pub(crate) fn filter_out_invalid_proposers(
    strategy: FilterStrategy,
    mut proposals: ProposalBundle,
) -> Result<ProposalBundle, MlsError> {
    for i in (0..proposals.add_proposals().len()).rev() {
        let p = &proposals.add_proposals()[i];
        let res = proposer_can_propose(p.sender, ProposalType::ADD, p.is_by_reference());

        if !apply_strategy(strategy, p.is_by_reference(), res)? {
            proposals.remove::<AddProposal>(i);
        }
    }

    for i in (0..proposals.update_proposals().len()).rev() {
        let p = &proposals.update_proposals()[i];
        let res = proposer_can_propose(p.sender, ProposalType::UPDATE, p.is_by_reference());

        if !apply_strategy(strategy, p.is_by_reference(), res)? {
            proposals.remove::<UpdateProposal>(i);
            proposals.update_senders.remove(i);
        }
    }

    for i in (0..proposals.remove_proposals().len()).rev() {
        let p = &proposals.remove_proposals()[i];
        let res = proposer_can_propose(p.sender, ProposalType::REMOVE, p.is_by_reference());

        if !apply_strategy(strategy, p.is_by_reference(), res)? {
            proposals.remove::<RemoveProposal>(i);
        }
    }

    #[cfg(feature = "psk")]
    for i in (0..proposals.psk_proposals().len()).rev() {
        let p = &proposals.psk_proposals()[i];
        let res = proposer_can_propose(p.sender, ProposalType::PSK, p.is_by_reference());

        if !apply_strategy(strategy, p.is_by_reference(), res)? {
            proposals.remove::<PreSharedKeyProposal>(i);
        }
    }

    for i in (0..proposals.reinit_proposals().len()).rev() {
        let p = &proposals.reinit_proposals()[i];
        let res = proposer_can_propose(p.sender, ProposalType::RE_INIT, p.is_by_reference());

        if !apply_strategy(strategy, p.is_by_reference(), res)? {
            proposals.remove::<ReInitProposal>(i);
        }
    }

    for i in (0..proposals.external_init_proposals().len()).rev() {
        let p = &proposals.external_init_proposals()[i];
        let res = proposer_can_propose(p.sender, ProposalType::EXTERNAL_INIT, p.is_by_reference());

        if !apply_strategy(strategy, p.is_by_reference(), res)? {
            proposals.remove::<ExternalInit>(i);
        }
    }

    for i in (0..proposals.group_context_ext_proposals().len()).rev() {
        let p = &proposals.group_context_ext_proposals()[i];
        let gce_type = ProposalType::GROUP_CONTEXT_EXTENSIONS;
        let res = proposer_can_propose(p.sender, gce_type, p.is_by_reference());

        if !apply_strategy(strategy, p.is_by_reference(), res)? {
            proposals.remove::<ExtensionList>(i);
        }
    }

    Ok(proposals)
}

fn leaf_index_of_update_sender(p: &ProposalInfo<UpdateProposal>) -> Result<LeafIndex, MlsError> {
    match p.sender {
        Sender::Member(i) => Ok(LeafIndex(i)),
        _ => Err(MlsError::InvalidProposalTypeForSender),
    }
}

#[cfg(feature = "custom_proposal")]
pub(super) fn filter_out_unsupported_custom_proposals(
    proposals: &mut ProposalBundle,
    tree: &TreeKemPublic,
    strategy: FilterStrategy,
) -> Result<(), MlsError> {
    let supported_types = proposals
        .custom_proposal_types()
        .filter(|t| tree.can_support_proposal(*t))
        .collect_vec();

    proposals.retain_custom(|p| {
        let proposal_type = p.proposal.proposal_type();

        apply_strategy(
            strategy,
            p.is_by_reference(),
            supported_types
                .contains(&proposal_type)
                .then_some(())
                .ok_or(MlsError::UnsupportedCustomProposal(proposal_type)),
        )
    })
}
