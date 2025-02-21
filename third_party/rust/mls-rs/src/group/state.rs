// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use mls_rs_codec::{MlsDecode, MlsEncode, MlsSize};
use mls_rs_core::group::Member;

use super::{
    confirmation_tag::ConfirmationTag, member_from_leaf_node, proposal::ReInitProposal,
    transcript_hash::InterimTranscriptHash,
};
use crate::{
    group::{GroupContext, TreeKemPublic},
    tree_kem::node::LeafIndex,
};

#[cfg_attr(
    all(feature = "ffi", not(test)),
    safer_ffi_gen::ffi_type(clone, opaque)
)]
#[derive(Clone, Debug, PartialEq, MlsSize, MlsEncode, MlsDecode)]
#[non_exhaustive]
pub struct GroupState {
    #[cfg(feature = "by_ref_proposal")]
    pub(crate) proposals: crate::group::ProposalCache,
    pub context: GroupContext,
    pub(crate) public_tree: TreeKemPublic,
    pub(crate) interim_transcript_hash: InterimTranscriptHash,
    pub(crate) pending_reinit: Option<ReInitProposal>,
    pub(crate) confirmation_tag: ConfirmationTag,
}

#[cfg(all(feature = "ffi", not(test)))]
impl GroupState {
    pub fn context(&self) -> &GroupContext {
        &self.context
    }
}

#[cfg_attr(all(feature = "ffi", not(test)), safer_ffi_gen::safer_ffi_gen)]
impl GroupState {
    pub fn member_at_index(&self, index: u32) -> Option<Member> {
        let leaf_index = LeafIndex(index);

        self.public_tree
            .get_leaf_node(leaf_index)
            .ok()
            .map(|ln| member_from_leaf_node(ln, leaf_index))
    }

    pub(crate) fn new(
        context: GroupContext,
        current_tree: TreeKemPublic,
        interim_transcript_hash: InterimTranscriptHash,
        confirmation_tag: ConfirmationTag,
    ) -> Self {
        Self {
            #[cfg(feature = "by_ref_proposal")]
            proposals: crate::group::ProposalCache::new(
                context.protocol_version,
                context.group_id.clone(),
            ),
            context,
            public_tree: current_tree,
            interim_transcript_hash,
            pending_reinit: None,
            confirmation_tag,
        }
    }
}
