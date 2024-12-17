// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#[cfg(feature = "psk")]
use crate::psk::PreSharedKey;
#[cfg(any(feature = "secret_tree_access", feature = "private_message"))]
use crate::tree_kem::node::NodeIndex;
#[cfg(feature = "prior_epoch")]
use crate::{crypto::SignaturePublicKey, group::GroupContext, tree_kem::node::LeafIndex};
use alloc::vec::Vec;
use core::{
    fmt::{self, Debug},
    ops::Deref,
};
use mls_rs_codec::{MlsDecode, MlsEncode, MlsSize};
use zeroize::Zeroizing;

#[cfg(all(feature = "prior_epoch", feature = "private_message"))]
use super::ciphertext_processor::GroupStateProvider;

#[cfg(any(feature = "secret_tree_access", feature = "private_message"))]
use crate::group::secret_tree::SecretTree;

#[cfg(feature = "prior_epoch")]
#[derive(Debug, Clone, MlsEncode, MlsDecode, MlsSize, PartialEq)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
pub(crate) struct PriorEpoch {
    pub(crate) context: GroupContext,
    pub(crate) self_index: LeafIndex,
    pub(crate) secrets: EpochSecrets,
    pub(crate) signature_public_keys: Vec<Option<SignaturePublicKey>>,
}

#[cfg(feature = "prior_epoch")]
impl PriorEpoch {
    #[inline(always)]
    pub(crate) fn epoch_id(&self) -> u64 {
        self.context.epoch
    }

    #[inline(always)]
    pub(crate) fn group_id(&self) -> &[u8] {
        &self.context.group_id
    }
}

#[cfg(all(feature = "private_message", feature = "prior_epoch"))]
impl GroupStateProvider for PriorEpoch {
    fn group_context(&self) -> &GroupContext {
        &self.context
    }

    fn self_index(&self) -> LeafIndex {
        self.self_index
    }

    fn epoch_secrets_mut(&mut self) -> &mut EpochSecrets {
        &mut self.secrets
    }

    fn epoch_secrets(&self) -> &EpochSecrets {
        &self.secrets
    }
}

#[derive(Debug, Clone, PartialEq, MlsEncode, MlsDecode, MlsSize)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
pub(crate) struct EpochSecrets {
    #[cfg(feature = "psk")]
    #[mls_codec(with = "mls_rs_codec::byte_vec")]
    pub(crate) resumption_secret: PreSharedKey,
    #[mls_codec(with = "mls_rs_codec::byte_vec")]
    pub(crate) sender_data_secret: SenderDataSecret,
    #[cfg(any(feature = "secret_tree_access", feature = "private_message"))]
    pub(crate) secret_tree: SecretTree<NodeIndex>,
}

#[derive(Clone, PartialEq, MlsEncode, MlsDecode, MlsSize)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
pub(crate) struct SenderDataSecret(
    #[mls_codec(with = "mls_rs_codec::byte_vec")]
    #[cfg_attr(feature = "serde", serde(with = "mls_rs_core::zeroizing_serde"))]
    Zeroizing<Vec<u8>>,
);

impl Debug for SenderDataSecret {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        mls_rs_core::debug::pretty_bytes(&self.0)
            .named("SenderDataSecret")
            .fmt(f)
    }
}

impl AsRef<[u8]> for SenderDataSecret {
    fn as_ref(&self) -> &[u8] {
        &self.0
    }
}

impl Deref for SenderDataSecret {
    type Target = Vec<u8>;

    fn deref(&self) -> &Self::Target {
        &self.0
    }
}

impl From<Vec<u8>> for SenderDataSecret {
    fn from(bytes: Vec<u8>) -> Self {
        Self(Zeroizing::new(bytes))
    }
}

impl From<Zeroizing<Vec<u8>>> for SenderDataSecret {
    fn from(bytes: Zeroizing<Vec<u8>>) -> Self {
        Self(bytes)
    }
}

#[cfg(test)]
pub(crate) mod test_utils {
    use mls_rs_core::crypto::CipherSuiteProvider;

    use super::*;
    use crate::cipher_suite::CipherSuite;
    use crate::crypto::test_utils::test_cipher_suite_provider;

    #[cfg(any(feature = "secret_tree_access", feature = "private_message"))]
    use crate::group::secret_tree::test_utils::get_test_tree;

    #[cfg(feature = "prior_epoch")]
    use crate::group::test_utils::get_test_group_context_with_id;

    use crate::group::test_utils::random_bytes;

    pub(crate) fn get_test_epoch_secrets(cipher_suite: CipherSuite) -> EpochSecrets {
        let cs_provider = test_cipher_suite_provider(cipher_suite);

        #[cfg(any(feature = "secret_tree_access", feature = "private_message"))]
        let secret_tree = get_test_tree(random_bytes(cs_provider.kdf_extract_size()), 2);

        EpochSecrets {
            #[cfg(feature = "psk")]
            resumption_secret: random_bytes(cs_provider.kdf_extract_size()).into(),
            sender_data_secret: random_bytes(cs_provider.kdf_extract_size()).into(),
            #[cfg(any(feature = "secret_tree_access", feature = "private_message"))]
            secret_tree,
        }
    }

    #[cfg(feature = "prior_epoch")]
    pub(crate) fn get_test_epoch_with_id(
        group_id: Vec<u8>,
        cipher_suite: CipherSuite,
        id: u64,
    ) -> PriorEpoch {
        PriorEpoch {
            context: get_test_group_context_with_id(group_id, id, cipher_suite),
            self_index: LeafIndex(0),
            secrets: get_test_epoch_secrets(cipher_suite),
            signature_public_keys: Default::default(),
        }
    }
}
