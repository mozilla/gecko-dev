// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use alloc::vec::Vec;
use core::fmt::{self, Debug};
use mls_rs_codec::{MlsDecode, MlsEncode, MlsSize};
use mls_rs_core::extension::ExtensionList;

use crate::{signer::Signable, tree_kem::node::LeafIndex};

use super::{ConfirmationTag, GroupContext};

#[derive(Clone, PartialEq, MlsSize, MlsEncode, MlsDecode)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
// #[cfg_attr(
//     all(feature = "ffi", not(test)),
//     safer_ffi_gen::ffi_type(clone, opaque)
// )]
pub struct GroupInfo {
    pub(crate) group_context: GroupContext,
    pub(crate) extensions: ExtensionList,
    pub(crate) confirmation_tag: ConfirmationTag,
    pub(crate) signer: LeafIndex,
    #[mls_codec(with = "mls_rs_codec::byte_vec")]
    pub(crate) signature: Vec<u8>,
}

impl Debug for GroupInfo {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("GroupInfo")
            .field("group_context", &self.group_context)
            .field("extensions", &self.extensions)
            .field("confirmation_tag", &self.confirmation_tag)
            .field("signer", &self.signer)
            .field(
                "signature",
                &mls_rs_core::debug::pretty_bytes(&self.signature),
            )
            .finish()
    }
}

// #[cfg_attr(all(feature = "ffi", not(test)), ::safer_ffi_gen::safer_ffi_gen)]
impl GroupInfo {
    /// Group context.
    pub fn group_context(&self) -> &GroupContext {
        &self.group_context
    }

    /// Group info extensions (not to be confused with group context extensions),
    /// e.g. the ratchet tree.
    pub fn extensions(&self) -> &ExtensionList {
        &self.extensions
    }

    /// Leaf index of the sender who generated and signed this group info.
    pub fn sender(&self) -> u32 {
        *self.signer
    }
}

#[derive(MlsEncode, MlsSize)]
struct SignableGroupInfo<'a> {
    group_context: &'a GroupContext,
    extensions: &'a ExtensionList,
    confirmation_tag: &'a ConfirmationTag,
    signer: LeafIndex,
}

impl<'a> Signable<'a> for GroupInfo {
    const SIGN_LABEL: &'static str = "GroupInfoTBS";
    type SigningContext = ();

    fn signature(&self) -> &[u8] {
        &self.signature
    }

    fn signable_content(
        &self,
        _context: &Self::SigningContext,
    ) -> Result<Vec<u8>, mls_rs_codec::Error> {
        SignableGroupInfo {
            group_context: &self.group_context,
            extensions: &self.extensions,
            confirmation_tag: &self.confirmation_tag,
            signer: self.signer,
        }
        .mls_encode_to_vec()
    }

    fn write_signature(&mut self, signature: Vec<u8>) {
        self.signature = signature
    }
}
