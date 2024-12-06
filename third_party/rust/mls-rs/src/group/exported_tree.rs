// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use alloc::{borrow::Cow, vec::Vec};
use mls_rs_codec::{MlsDecode, MlsEncode, MlsSize};

use crate::{client::MlsError, tree_kem::node::NodeVec};

#[cfg_attr(
    all(feature = "ffi", not(test)),
    safer_ffi_gen::ffi_type(clone, opaque)
)]
#[derive(Debug, MlsSize, MlsEncode, MlsDecode, PartialEq, Clone)]
pub struct ExportedTree<'a>(pub(crate) Cow<'a, NodeVec>);

#[cfg_attr(all(feature = "ffi", not(test)), ::safer_ffi_gen::safer_ffi_gen)]
impl<'a> ExportedTree<'a> {
    pub(crate) fn new(node_data: NodeVec) -> Self {
        Self(Cow::Owned(node_data))
    }

    pub(crate) fn new_borrowed(node_data: &'a NodeVec) -> Self {
        Self(Cow::Borrowed(node_data))
    }

    pub fn to_bytes(&self) -> Result<Vec<u8>, MlsError> {
        self.mls_encode_to_vec().map_err(Into::into)
    }

    pub fn byte_size(&self) -> usize {
        self.mls_encoded_len()
    }

    pub fn into_owned(self) -> ExportedTree<'static> {
        ExportedTree(Cow::Owned(self.0.into_owned()))
    }
}

#[cfg_attr(all(feature = "ffi", not(test)), ::safer_ffi_gen::safer_ffi_gen)]
impl ExportedTree<'static> {
    pub fn from_bytes(bytes: &[u8]) -> Result<Self, MlsError> {
        Self::mls_decode(&mut &*bytes).map_err(Into::into)
    }
}

impl From<ExportedTree<'_>> for NodeVec {
    fn from(value: ExportedTree) -> Self {
        value.0.into_owned()
    }
}
