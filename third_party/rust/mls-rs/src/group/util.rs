// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use mls_rs_core::{
    error::IntoAnyError, identity::IdentityProvider, key_package::KeyPackageStorage,
};

use crate::{
    cipher_suite::CipherSuite,
    client::MlsError,
    extension::RatchetTreeExt,
    key_package::KeyPackageGeneration,
    protocol_version::ProtocolVersion,
    signer::Signable,
    tree_kem::{node::LeafIndex, tree_validator::TreeValidator, TreeKemPublic},
    CipherSuiteProvider, CryptoProvider,
};

#[cfg(feature = "by_ref_proposal")]
use crate::extension::ExternalSendersExt;

use super::{
    framing::Sender, message_signature::AuthenticatedContent,
    transcript_hash::InterimTranscriptHash, ConfirmedTranscriptHash, EncryptedGroupSecrets,
    ExportedTree, GroupInfo, GroupState,
};

use super::message_processor::ProvisionalState;

#[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
pub(crate) async fn validate_group_info_common<C: CipherSuiteProvider>(
    msg_version: ProtocolVersion,
    group_info: &GroupInfo,
    tree: &TreeKemPublic,
    cs: &C,
) -> Result<(), MlsError> {
    if msg_version != group_info.group_context.protocol_version {
        return Err(MlsError::ProtocolVersionMismatch);
    }

    if group_info.group_context.cipher_suite != cs.cipher_suite() {
        return Err(MlsError::CipherSuiteMismatch);
    }

    let sender_leaf = &tree.get_leaf_node(group_info.signer)?;

    group_info
        .verify(cs, &sender_leaf.signing_identity.signature_key, &())
        .await?;

    Ok(())
}

#[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
pub(crate) async fn validate_group_info_member<C: CipherSuiteProvider>(
    self_state: &GroupState,
    msg_version: ProtocolVersion,
    group_info: &GroupInfo,
    cs: &C,
) -> Result<(), MlsError> {
    validate_group_info_common(msg_version, group_info, &self_state.public_tree, cs).await?;

    let self_tree = ExportedTree::new_borrowed(&self_state.public_tree.nodes);

    if let Some(tree) = group_info.extensions.get_as::<RatchetTreeExt>()? {
        (tree.tree_data == self_tree)
            .then_some(())
            .ok_or(MlsError::InvalidGroupInfo)?;
    }

    (group_info.group_context == self_state.context
        && group_info.confirmation_tag == self_state.confirmation_tag)
        .then_some(())
        .ok_or(MlsError::InvalidGroupInfo)?;

    Ok(())
}

#[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
pub(crate) async fn validate_group_info_joiner<C, I>(
    msg_version: ProtocolVersion,
    group_info: &GroupInfo,
    tree: Option<ExportedTree<'_>>,
    id_provider: &I,
    cs: &C,
) -> Result<TreeKemPublic, MlsError>
where
    C: CipherSuiteProvider,
    I: IdentityProvider,
{
    let tree = match group_info.extensions.get_as::<RatchetTreeExt>()? {
        Some(ext) => ext.tree_data,
        None => tree.ok_or(MlsError::RatchetTreeNotFound)?,
    };

    let context = &group_info.group_context;

    let mut tree =
        TreeKemPublic::import_node_data(tree.into(), id_provider, &context.extensions).await?;

    // Verify the integrity of the ratchet tree
    TreeValidator::new(cs, context, id_provider)
        .validate(&mut tree)
        .await?;

    #[cfg(feature = "by_ref_proposal")]
    if let Some(ext_senders) = context.extensions.get_as::<ExternalSendersExt>()? {
        // TODO do joiners verify group against current time??
        ext_senders
            .verify_all(id_provider, None, &context.extensions)
            .await
            .map_err(|e| MlsError::IdentityProviderError(e.into_any_error()))?;
    }

    validate_group_info_common(msg_version, group_info, &tree, cs).await?;

    Ok(tree)
}

pub(crate) fn commit_sender(
    sender: &Sender,
    provisional_state: &ProvisionalState,
) -> Result<LeafIndex, MlsError> {
    match sender {
        Sender::Member(index) => Ok(LeafIndex(*index)),
        #[cfg(feature = "by_ref_proposal")]
        Sender::External(_) => Err(MlsError::ExternalSenderCannotCommit),
        #[cfg(feature = "by_ref_proposal")]
        Sender::NewMemberProposal => Err(MlsError::ExpectedAddProposalForNewMemberProposal),
        Sender::NewMemberCommit => provisional_state
            .external_init_index
            .ok_or(MlsError::ExternalCommitMissingExternalInit),
    }
}

#[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
pub(super) async fn transcript_hashes<P: CipherSuiteProvider>(
    cipher_suite_provider: &P,
    prev_interim_transcript_hash: &InterimTranscriptHash,
    content: &AuthenticatedContent,
) -> Result<(InterimTranscriptHash, ConfirmedTranscriptHash), MlsError> {
    let confirmed_transcript_hash = ConfirmedTranscriptHash::create(
        cipher_suite_provider,
        prev_interim_transcript_hash,
        content,
    )
    .await?;

    let confirmation_tag = content
        .auth
        .confirmation_tag
        .as_ref()
        .ok_or(MlsError::InvalidConfirmationTag)?;

    let interim_transcript_hash = InterimTranscriptHash::create(
        cipher_suite_provider,
        &confirmed_transcript_hash,
        confirmation_tag,
    )
    .await?;

    Ok((interim_transcript_hash, confirmed_transcript_hash))
}

#[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
pub(crate) async fn find_key_package_generation<'a, K: KeyPackageStorage>(
    key_package_repo: &K,
    secrets: &'a [EncryptedGroupSecrets],
) -> Result<(&'a EncryptedGroupSecrets, KeyPackageGeneration), MlsError> {
    for secret in secrets {
        if let Some(val) = key_package_repo
            .get(&secret.new_member)
            .await
            .map_err(|e| MlsError::KeyPackageRepoError(e.into_any_error()))
            .and_then(|maybe_data| {
                if let Some(data) = maybe_data {
                    KeyPackageGeneration::from_storage(secret.new_member.to_vec(), data)
                        .map(|kpg| Some((secret, kpg)))
                } else {
                    Ok::<_, MlsError>(None)
                }
            })?
        {
            return Ok(val);
        }
    }

    Err(MlsError::WelcomeKeyPackageNotFound)
}

pub(crate) fn cipher_suite_provider<P>(
    crypto: P,
    cipher_suite: CipherSuite,
) -> Result<P::CipherSuiteProvider, MlsError>
where
    P: CryptoProvider,
{
    crypto
        .cipher_suite_provider(cipher_suite)
        .ok_or(MlsError::UnsupportedCipherSuite(cipher_suite))
}
