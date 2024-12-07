// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use super::{
    confirmation_tag::ConfirmationTag, proposal::ReInitProposal,
    transcript_hash::InterimTranscriptHash,
};
use crate::group::{GroupContext, TreeKemPublic};

#[derive(Clone, Debug, PartialEq)]
#[non_exhaustive]
pub struct GroupState {
    #[cfg(feature = "by_ref_proposal")]
    pub(crate) proposals: crate::group::ProposalCache,
    pub(crate) context: GroupContext,
    pub(crate) public_tree: TreeKemPublic,
    pub(crate) interim_transcript_hash: InterimTranscriptHash,
    pub(crate) pending_reinit: Option<ReInitProposal>,
    pub(crate) confirmation_tag: ConfirmationTag,
}

impl GroupState {
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
