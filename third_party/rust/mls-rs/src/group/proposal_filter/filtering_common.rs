// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use crate::{
    client::MlsError,
    group::{proposal_filter::ProposalBundle, Sender},
    key_package::{validate_key_package_properties, KeyPackage},
    protocol_version::ProtocolVersion,
    time::MlsTime,
    tree_kem::{
        leaf_node_validator::{LeafNodeValidator, ValidationContext},
        node::LeafIndex,
        TreeKemPublic,
    },
    CipherSuiteProvider, ExtensionList,
};

use crate::tree_kem::leaf_node::LeafNode;

use super::ProposalInfo;

use crate::extension::{MlsExtension, RequiredCapabilitiesExt};

#[cfg(feature = "by_ref_proposal")]
use crate::extension::ExternalSendersExt;

use mls_rs_core::error::IntoAnyError;

use alloc::vec::Vec;
use mls_rs_core::{identity::IdentityProvider, psk::PreSharedKeyStorage};

use crate::group::{ExternalInit, ProposalType, RemoveProposal};

#[cfg(all(feature = "by_ref_proposal", feature = "psk"))]
use crate::group::proposal::PreSharedKeyProposal;

#[cfg(feature = "psk")]
use crate::group::{JustPreSharedKeyID, ResumptionPSKUsage, ResumptionPsk};

#[cfg(all(feature = "std", feature = "psk"))]
use std::collections::HashSet;

#[cfg(feature = "by_ref_proposal")]
use super::filtering::{apply_strategy, filter_out_invalid_proposers, FilterStrategy};

#[cfg(feature = "custom_proposal")]
use super::filtering::filter_out_unsupported_custom_proposals;

#[derive(Debug)]
pub(crate) struct ProposalApplier<'a, C, P, CSP> {
    pub original_tree: &'a TreeKemPublic,
    pub protocol_version: ProtocolVersion,
    pub cipher_suite_provider: &'a CSP,
    pub original_group_extensions: &'a ExtensionList,
    pub external_leaf: Option<&'a LeafNode>,
    pub identity_provider: &'a C,
    pub psk_storage: &'a P,
    #[cfg(feature = "by_ref_proposal")]
    pub group_id: &'a [u8],
}

#[derive(Debug)]
pub(crate) struct ApplyProposalsOutput {
    pub(crate) new_tree: TreeKemPublic,
    pub(crate) indexes_of_added_kpkgs: Vec<LeafIndex>,
    pub(crate) external_init_index: Option<LeafIndex>,
    #[cfg(feature = "by_ref_proposal")]
    pub(crate) applied_proposals: ProposalBundle,
    pub(crate) new_context_extensions: Option<ExtensionList>,
}

impl<'a, C, P, CSP> ProposalApplier<'a, C, P, CSP>
where
    C: IdentityProvider,
    P: PreSharedKeyStorage,
    CSP: CipherSuiteProvider,
{
    #[allow(clippy::too_many_arguments)]
    pub(crate) fn new(
        original_tree: &'a TreeKemPublic,
        protocol_version: ProtocolVersion,
        cipher_suite_provider: &'a CSP,
        original_group_extensions: &'a ExtensionList,
        external_leaf: Option<&'a LeafNode>,
        identity_provider: &'a C,
        psk_storage: &'a P,
        #[cfg(feature = "by_ref_proposal")] group_id: &'a [u8],
    ) -> Self {
        Self {
            original_tree,
            protocol_version,
            cipher_suite_provider,
            original_group_extensions,
            external_leaf,
            identity_provider,
            psk_storage,
            #[cfg(feature = "by_ref_proposal")]
            group_id,
        }
    }

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub(crate) async fn apply_proposals(
        &self,
        #[cfg(feature = "by_ref_proposal")] strategy: FilterStrategy,
        commit_sender: &Sender,
        #[cfg(not(feature = "by_ref_proposal"))] proposals: &ProposalBundle,
        #[cfg(feature = "by_ref_proposal")] proposals: ProposalBundle,
        commit_time: Option<MlsTime>,
    ) -> Result<ApplyProposalsOutput, MlsError> {
        let output = match commit_sender {
            Sender::Member(sender) => {
                self.apply_proposals_from_member(
                    #[cfg(feature = "by_ref_proposal")]
                    strategy,
                    LeafIndex(*sender),
                    proposals,
                    commit_time,
                )
                .await
            }
            Sender::NewMemberCommit => {
                self.apply_proposals_from_new_member(proposals, commit_time)
                    .await
            }
            #[cfg(feature = "by_ref_proposal")]
            Sender::External(_) => Err(MlsError::ExternalSenderCannotCommit),
            #[cfg(feature = "by_ref_proposal")]
            Sender::NewMemberProposal => Err(MlsError::ExternalSenderCannotCommit),
        }?;

        #[cfg(all(feature = "by_ref_proposal", feature = "custom_proposal"))]
        let mut output = output;

        #[cfg(all(feature = "by_ref_proposal", feature = "custom_proposal"))]
        filter_out_unsupported_custom_proposals(
            &mut output.applied_proposals,
            &output.new_tree,
            strategy,
        )?;

        #[cfg(all(not(feature = "by_ref_proposal"), feature = "custom_proposal"))]
        filter_out_unsupported_custom_proposals(proposals, &output.new_tree)?;

        Ok(output)
    }

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    // The lint below is triggered by the `proposals` parameter which may or may not be a borrow.
    #[allow(clippy::needless_borrow)]
    async fn apply_proposals_from_new_member(
        &self,
        #[cfg(not(feature = "by_ref_proposal"))] proposals: &ProposalBundle,
        #[cfg(feature = "by_ref_proposal")] proposals: ProposalBundle,
        commit_time: Option<MlsTime>,
    ) -> Result<ApplyProposalsOutput, MlsError> {
        let external_leaf = self
            .external_leaf
            .ok_or(MlsError::ExternalCommitMustHaveNewLeaf)?;

        ensure_exactly_one_external_init(&proposals)?;

        ensure_at_most_one_removal_for_self(
            &proposals,
            external_leaf,
            self.original_tree,
            self.identity_provider,
            self.original_group_extensions,
        )
        .await?;

        ensure_proposals_in_external_commit_are_allowed(&proposals)?;
        ensure_no_proposal_by_ref(&proposals)?;

        #[cfg(feature = "by_ref_proposal")]
        let mut proposals = filter_out_invalid_proposers(FilterStrategy::IgnoreNone, proposals)?;

        filter_out_invalid_psks(
            #[cfg(feature = "by_ref_proposal")]
            FilterStrategy::IgnoreNone,
            self.cipher_suite_provider,
            #[cfg(feature = "by_ref_proposal")]
            &mut proposals,
            #[cfg(not(feature = "by_ref_proposal"))]
            proposals,
            self.psk_storage,
        )
        .await?;

        let mut output = self
            .apply_proposal_changes(
                #[cfg(feature = "by_ref_proposal")]
                FilterStrategy::IgnoreNone,
                proposals,
                commit_time,
            )
            .await?;

        output.external_init_index = Some(
            insert_external_leaf(
                &mut output.new_tree,
                external_leaf.clone(),
                self.identity_provider,
                self.original_group_extensions,
            )
            .await?,
        );

        Ok(output)
    }

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub(super) async fn apply_proposals_with_new_capabilities(
        &self,
        #[cfg(feature = "by_ref_proposal")] strategy: FilterStrategy,
        #[cfg(not(feature = "by_ref_proposal"))] proposals: &ProposalBundle,
        #[cfg(feature = "by_ref_proposal")] proposals: ProposalBundle,
        group_context_extensions_proposal: ProposalInfo<ExtensionList>,
        commit_time: Option<MlsTime>,
    ) -> Result<ApplyProposalsOutput, MlsError>
    where
        C: IdentityProvider,
    {
        #[cfg(feature = "by_ref_proposal")]
        let mut proposals_clone = proposals.clone();

        // Apply adds, updates etc. in the context of new extensions
        let output = self
            .apply_tree_changes(
                #[cfg(feature = "by_ref_proposal")]
                strategy,
                proposals,
                &group_context_extensions_proposal.proposal,
                commit_time,
            )
            .await?;

        // Verify that capabilities and extensions are supported after modifications.
        // TODO: The newly inserted nodes have already been validated by `apply_tree_changes`
        // above. We should investigate if there is an easy way to avoid the double check.
        let must_check = group_context_extensions_proposal
            .proposal
            .has_extension(RequiredCapabilitiesExt::extension_type());

        #[cfg(feature = "by_ref_proposal")]
        let must_check = must_check
            || group_context_extensions_proposal
                .proposal
                .has_extension(ExternalSendersExt::extension_type());

        let new_capabilities_supported = if must_check {
            let leaf_validator = LeafNodeValidator::new(
                self.cipher_suite_provider,
                self.identity_provider,
                Some(&group_context_extensions_proposal.proposal),
            );

            output
                .new_tree
                .non_empty_leaves()
                .try_for_each(|(_, leaf)| {
                    leaf_validator.validate_required_capabilities(leaf)?;

                    #[cfg(feature = "by_ref_proposal")]
                    leaf_validator.validate_external_senders_ext_credentials(leaf)?;

                    Ok(())
                })
        } else {
            Ok(())
        };

        let new_extensions_supported = group_context_extensions_proposal
            .proposal
            .iter()
            .map(|extension| extension.extension_type)
            .filter(|&ext_type| !ext_type.is_default())
            .find(|ext_type| {
                !output
                    .new_tree
                    .non_empty_leaves()
                    .all(|(_, leaf)| leaf.capabilities.extensions.contains(ext_type))
            })
            .map_or(Ok(()), |ext| Err(MlsError::UnsupportedGroupExtension(ext)));

        #[cfg(not(feature = "by_ref_proposal"))]
        {
            new_capabilities_supported.and(new_extensions_supported)?;
            Ok(output)
        }

        #[cfg(feature = "by_ref_proposal")]
        // If extensions are good, return `Ok`. If not and the strategy is to filter, remove the group
        // context extensions proposal and try applying all proposals again in the context of the old
        // extensions. Else, return an error.
        match new_capabilities_supported.and(new_extensions_supported) {
            Ok(()) => Ok(output),
            Err(e) => {
                if strategy.ignore(group_context_extensions_proposal.is_by_reference()) {
                    proposals_clone.group_context_extensions.clear();

                    self.apply_tree_changes(
                        strategy,
                        proposals_clone,
                        self.original_group_extensions,
                        commit_time,
                    )
                    .await
                } else {
                    Err(e)
                }
            }
        }
    }

    #[cfg(any(mls_build_async, not(feature = "rayon")))]
    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub async fn validate_new_node<Ip: IdentityProvider, Cp: CipherSuiteProvider>(
        &self,
        leaf_node_validator: &LeafNodeValidator<'_, Ip, Cp>,
        key_package: &KeyPackage,
        commit_time: Option<MlsTime>,
    ) -> Result<(), MlsError> {
        leaf_node_validator
            .check_if_valid(&key_package.leaf_node, ValidationContext::Add(commit_time))
            .await?;

        validate_key_package_properties(
            key_package,
            self.protocol_version,
            self.cipher_suite_provider,
        )
        .await
    }

    #[cfg(all(not(mls_build_async), feature = "rayon"))]
    pub fn validate_new_node<Ip: IdentityProvider, Cp: CipherSuiteProvider>(
        &self,
        leaf_node_validator: &LeafNodeValidator<'_, Ip, Cp>,
        key_package: &KeyPackage,
        commit_time: Option<MlsTime>,
    ) -> Result<(), MlsError> {
        let (a, b) = rayon::join(
            || {
                leaf_node_validator
                    .check_if_valid(&key_package.leaf_node, ValidationContext::Add(commit_time))
            },
            || {
                validate_key_package_properties(
                    key_package,
                    self.protocol_version,
                    self.cipher_suite_provider,
                )
            },
        );
        a?;
        b
    }
}

#[cfg(feature = "psk")]
#[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
pub(crate) async fn filter_out_invalid_psks<P, CP>(
    #[cfg(feature = "by_ref_proposal")] strategy: FilterStrategy,
    cipher_suite_provider: &CP,
    #[cfg(not(feature = "by_ref_proposal"))] proposals: &ProposalBundle,
    #[cfg(feature = "by_ref_proposal")] proposals: &mut ProposalBundle,
    psk_storage: &P,
) -> Result<(), MlsError>
where
    P: PreSharedKeyStorage,
    CP: CipherSuiteProvider,
{
    let kdf_extract_size = cipher_suite_provider.kdf_extract_size();

    #[cfg(feature = "std")]
    let mut ids_seen = HashSet::new();

    #[cfg(not(feature = "std"))]
    let mut ids_seen = Vec::new();

    #[cfg(feature = "by_ref_proposal")]
    let mut bad_indices = Vec::new();

    for i in 0..proposals.psk_proposals().len() {
        let p = &proposals.psks[i];

        let valid = matches!(
            p.proposal.psk.key_id,
            JustPreSharedKeyID::External(_)
                | JustPreSharedKeyID::Resumption(ResumptionPsk {
                    usage: ResumptionPSKUsage::Application,
                    ..
                })
        );

        let nonce_length = p.proposal.psk.psk_nonce.0.len();
        let nonce_valid = nonce_length == kdf_extract_size;

        #[cfg(feature = "std")]
        let is_new_id = ids_seen.insert(p.proposal.psk.clone());

        #[cfg(not(feature = "std"))]
        let is_new_id = !ids_seen.contains(&p.proposal.psk);

        let external_id_is_valid = match &p.proposal.psk.key_id {
            JustPreSharedKeyID::External(id) => psk_storage
                .contains(id)
                .await
                .map_err(|e| MlsError::PskStoreError(e.into_any_error()))
                .and_then(|found| {
                    if found {
                        Ok(())
                    } else {
                        Err(MlsError::MissingRequiredPsk)
                    }
                }),
            JustPreSharedKeyID::Resumption(_) => Ok(()),
        };

        #[cfg(not(feature = "by_ref_proposal"))]
        if !valid {
            return Err(MlsError::InvalidTypeOrUsageInPreSharedKeyProposal);
        } else if !nonce_valid {
            return Err(MlsError::InvalidPskNonceLength);
        } else if !is_new_id {
            return Err(MlsError::DuplicatePskIds);
        } else if external_id_is_valid.is_err() {
            return external_id_is_valid;
        }

        #[cfg(feature = "by_ref_proposal")]
        {
            let res = if !valid {
                Err(MlsError::InvalidTypeOrUsageInPreSharedKeyProposal)
            } else if !nonce_valid {
                Err(MlsError::InvalidPskNonceLength)
            } else if !is_new_id {
                Err(MlsError::DuplicatePskIds)
            } else {
                external_id_is_valid
            };

            if !apply_strategy(strategy, p.is_by_reference(), res)? {
                bad_indices.push(i)
            }
        }

        #[cfg(not(feature = "std"))]
        ids_seen.push(p.proposal.psk.clone());
    }

    #[cfg(feature = "by_ref_proposal")]
    bad_indices
        .into_iter()
        .rev()
        .for_each(|i| proposals.remove::<PreSharedKeyProposal>(i));

    Ok(())
}

#[cfg(not(feature = "psk"))]
#[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
pub(crate) async fn filter_out_invalid_psks<P, CP>(
    #[cfg(feature = "by_ref_proposal")] _: FilterStrategy,
    _: &CP,
    #[cfg(not(feature = "by_ref_proposal"))] _: &ProposalBundle,
    #[cfg(feature = "by_ref_proposal")] _: &mut ProposalBundle,
    _: &P,
) -> Result<(), MlsError>
where
    P: PreSharedKeyStorage,
    CP: CipherSuiteProvider,
{
    Ok(())
}

fn ensure_exactly_one_external_init(proposals: &ProposalBundle) -> Result<(), MlsError> {
    (proposals.by_type::<ExternalInit>().count() == 1)
        .then_some(())
        .ok_or(MlsError::ExternalCommitMustHaveExactlyOneExternalInit)
}

/// Non-default proposal types are by default allowed. Custom MlsRules may disallow
/// specific custom proposals in external commits
fn ensure_proposals_in_external_commit_are_allowed(
    proposals: &ProposalBundle,
) -> Result<(), MlsError> {
    let supported_default_types = [
        ProposalType::EXTERNAL_INIT,
        ProposalType::REMOVE,
        ProposalType::PSK,
    ];

    let unsupported_type = proposals
        .proposal_types()
        .find(|ty| !supported_default_types.contains(ty) && ProposalType::DEFAULT.contains(ty));

    match unsupported_type {
        Some(kind) => Err(MlsError::InvalidProposalTypeInExternalCommit(kind)),
        None => Ok(()),
    }
}

#[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
async fn ensure_at_most_one_removal_for_self<C>(
    proposals: &ProposalBundle,
    external_leaf: &LeafNode,
    tree: &TreeKemPublic,
    identity_provider: &C,
    extensions: &ExtensionList,
) -> Result<(), MlsError>
where
    C: IdentityProvider,
{
    let mut removals = proposals.by_type::<RemoveProposal>();

    match (removals.next(), removals.next()) {
        (Some(removal), None) => {
            ensure_removal_is_for_self(
                &removal.proposal,
                external_leaf,
                tree,
                identity_provider,
                extensions,
            )
            .await
        }
        (Some(_), Some(_)) => Err(MlsError::ExternalCommitWithMoreThanOneRemove),
        (None, _) => Ok(()),
    }
}

#[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
async fn ensure_removal_is_for_self<C>(
    removal: &RemoveProposal,
    external_leaf: &LeafNode,
    tree: &TreeKemPublic,
    identity_provider: &C,
    extensions: &ExtensionList,
) -> Result<(), MlsError>
where
    C: IdentityProvider,
{
    let existing_signing_id = &tree.get_leaf_node(removal.to_remove)?.signing_identity;

    identity_provider
        .valid_successor(
            existing_signing_id,
            &external_leaf.signing_identity,
            extensions,
        )
        .await
        .map_err(|e| MlsError::IdentityProviderError(e.into_any_error()))?
        .then_some(())
        .ok_or(MlsError::ExternalCommitRemovesOtherIdentity)
}

/// Non-default by-ref proposal types are by default allowed. Custom MlsRules may disallow
/// specific custom by-ref proposals.
fn ensure_no_proposal_by_ref(proposals: &ProposalBundle) -> Result<(), MlsError> {
    proposals
        .iter_proposals()
        .all(|p| !ProposalType::DEFAULT.contains(&p.proposal.proposal_type()) || p.is_by_value())
        .then_some(())
        .ok_or(MlsError::OnlyMembersCanCommitProposalsByRef)
}

#[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
async fn insert_external_leaf<I: IdentityProvider>(
    tree: &mut TreeKemPublic,
    leaf_node: LeafNode,
    identity_provider: &I,
    extensions: &ExtensionList,
) -> Result<LeafIndex, MlsError> {
    tree.add_leaf(leaf_node, identity_provider, extensions, None)
        .await
}
