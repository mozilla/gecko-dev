// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use alloc::vec::Vec;
use core::{
    fmt::{self, Debug},
    ops::{Deref, DerefMut},
};

use zeroize::Zeroizing;

use crate::{client::MlsError, tree_kem::math::TreeIndex, CipherSuiteProvider};

use mls_rs_codec::{MlsDecode, MlsEncode, MlsSize};
use mls_rs_core::error::IntoAnyError;

#[cfg(feature = "std")]
use std::collections::HashMap;

#[cfg(not(feature = "std"))]
use alloc::collections::BTreeMap;

use super::key_schedule::kdf_expand_with_label;

pub(crate) const MAX_RATCHET_BACK_HISTORY: u32 = 1024;

#[derive(Clone, Debug, PartialEq, MlsSize, MlsEncode, MlsDecode)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
#[repr(u8)]
enum SecretTreeNode {
    Secret(TreeSecret) = 0u8,
    Ratchet(SecretRatchets) = 1u8,
}

impl SecretTreeNode {
    fn into_secret(self) -> Option<TreeSecret> {
        if let SecretTreeNode::Secret(secret) = self {
            Some(secret)
        } else {
            None
        }
    }
}

#[derive(Clone, PartialEq, MlsEncode, MlsDecode, MlsSize)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
struct TreeSecret(
    #[mls_codec(with = "mls_rs_codec::byte_vec")]
    #[cfg_attr(feature = "serde", serde(with = "mls_rs_core::zeroizing_serde"))]
    Zeroizing<Vec<u8>>,
);

impl Debug for TreeSecret {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        mls_rs_core::debug::pretty_bytes(&self.0)
            .named("TreeSecret")
            .fmt(f)
    }
}

impl Deref for TreeSecret {
    type Target = Vec<u8>;

    fn deref(&self) -> &Self::Target {
        &self.0
    }
}

impl DerefMut for TreeSecret {
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.0
    }
}

impl AsRef<[u8]> for TreeSecret {
    fn as_ref(&self) -> &[u8] {
        &self.0
    }
}

impl From<Vec<u8>> for TreeSecret {
    fn from(vec: Vec<u8>) -> Self {
        TreeSecret(Zeroizing::new(vec))
    }
}

impl From<Zeroizing<Vec<u8>>> for TreeSecret {
    fn from(vec: Zeroizing<Vec<u8>>) -> Self {
        TreeSecret(vec)
    }
}

#[derive(Clone, Debug, PartialEq, MlsEncode, MlsDecode, MlsSize, Default)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
struct TreeSecretsVec<T: TreeIndex> {
    #[cfg(feature = "std")]
    inner: HashMap<T, SecretTreeNode>,
    #[cfg(not(feature = "std"))]
    inner: Vec<(T, SecretTreeNode)>,
}

#[cfg(feature = "std")]
impl<T: TreeIndex> TreeSecretsVec<T> {
    fn set_node(&mut self, index: T, value: SecretTreeNode) {
        self.inner.insert(index, value);
    }

    fn take_node(&mut self, index: &T) -> Option<SecretTreeNode> {
        self.inner.remove(index)
    }
}

#[cfg(not(feature = "std"))]
impl<T: TreeIndex> TreeSecretsVec<T> {
    fn set_node(&mut self, index: T, value: SecretTreeNode) {
        if let Some(i) = self.find_node(&index) {
            self.inner[i] = (index, value)
        } else {
            self.inner.push((index, value))
        }
    }

    fn take_node(&mut self, index: &T) -> Option<SecretTreeNode> {
        self.find_node(index).map(|i| self.inner.remove(i).1)
    }

    fn find_node(&self, index: &T) -> Option<usize> {
        use itertools::Itertools;

        self.inner
            .iter()
            .find_position(|(i, _)| i == index)
            .map(|(i, _)| i)
    }
}

#[derive(Clone, Debug, PartialEq, MlsEncode, MlsDecode, MlsSize)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
pub struct SecretTree<T: TreeIndex> {
    known_secrets: TreeSecretsVec<T>,
    leaf_count: T,
}

impl<T: TreeIndex> SecretTree<T> {
    pub(crate) fn empty() -> SecretTree<T> {
        SecretTree {
            known_secrets: Default::default(),
            leaf_count: T::zero(),
        }
    }
}

#[derive(Clone, Debug, PartialEq, MlsSize, MlsEncode, MlsDecode)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
pub struct SecretRatchets {
    pub application: SecretKeyRatchet,
    pub handshake: SecretKeyRatchet,
}

impl SecretRatchets {
    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub async fn message_key_generation<P: CipherSuiteProvider>(
        &mut self,
        cipher_suite_provider: &P,
        generation: u32,
        key_type: KeyType,
    ) -> Result<MessageKeyData, MlsError> {
        match key_type {
            KeyType::Handshake => {
                self.handshake
                    .get_message_key(cipher_suite_provider, generation)
                    .await
            }
            KeyType::Application => {
                self.application
                    .get_message_key(cipher_suite_provider, generation)
                    .await
            }
        }
    }

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub async fn next_message_key<P: CipherSuiteProvider>(
        &mut self,
        cipher_suite: &P,
        key_type: KeyType,
    ) -> Result<MessageKeyData, MlsError> {
        match key_type {
            KeyType::Handshake => self.handshake.next_message_key(cipher_suite).await,
            KeyType::Application => self.application.next_message_key(cipher_suite).await,
        }
    }
}

impl<T: TreeIndex> SecretTree<T> {
    pub fn new(leaf_count: T, encryption_secret: Zeroizing<Vec<u8>>) -> SecretTree<T> {
        let mut known_secrets = TreeSecretsVec::default();

        let root_secret = SecretTreeNode::Secret(TreeSecret::from(encryption_secret));
        known_secrets.set_node(leaf_count.root(), root_secret);

        Self {
            known_secrets,
            leaf_count,
        }
    }

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    async fn consume_node<P: CipherSuiteProvider>(
        &mut self,
        cipher_suite_provider: &P,
        index: &T,
    ) -> Result<(), MlsError> {
        let node = self.known_secrets.take_node(index);

        if let Some(secret) = node.and_then(|n| n.into_secret()) {
            let left_index = index.left().ok_or(MlsError::LeafNodeNoChildren)?;
            let right_index = index.right().ok_or(MlsError::LeafNodeNoChildren)?;

            let left_secret =
                kdf_expand_with_label(cipher_suite_provider, &secret, b"tree", b"left", None)
                    .await?;

            let right_secret =
                kdf_expand_with_label(cipher_suite_provider, &secret, b"tree", b"right", None)
                    .await?;

            self.known_secrets
                .set_node(left_index, SecretTreeNode::Secret(left_secret.into()));

            self.known_secrets
                .set_node(right_index, SecretTreeNode::Secret(right_secret.into()));
        }

        Ok(())
    }

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    async fn take_leaf_ratchet<P: CipherSuiteProvider>(
        &mut self,
        cipher_suite: &P,
        leaf_index: &T,
    ) -> Result<SecretRatchets, MlsError> {
        let node_index = leaf_index;

        let node = match self.known_secrets.take_node(node_index) {
            Some(node) => node,
            None => {
                // Start at the root node and work your way down consuming any intermediates needed
                for i in node_index.direct_copath(&self.leaf_count).into_iter().rev() {
                    self.consume_node(cipher_suite, &i.path).await?;
                }

                self.known_secrets
                    .take_node(node_index)
                    .ok_or(MlsError::InvalidLeafConsumption)?
            }
        };

        Ok(match node {
            SecretTreeNode::Ratchet(ratchet) => ratchet,
            SecretTreeNode::Secret(secret) => SecretRatchets {
                application: SecretKeyRatchet::new(cipher_suite, &secret, KeyType::Application)
                    .await?,
                handshake: SecretKeyRatchet::new(cipher_suite, &secret, KeyType::Handshake).await?,
            },
        })
    }

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub async fn next_message_key<P: CipherSuiteProvider>(
        &mut self,
        cipher_suite: &P,
        leaf_index: T,
        key_type: KeyType,
    ) -> Result<MessageKeyData, MlsError> {
        let mut ratchet = self.take_leaf_ratchet(cipher_suite, &leaf_index).await?;
        let res = ratchet.next_message_key(cipher_suite, key_type).await?;

        self.known_secrets
            .set_node(leaf_index, SecretTreeNode::Ratchet(ratchet));

        Ok(res)
    }

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub async fn message_key_generation<P: CipherSuiteProvider>(
        &mut self,
        cipher_suite: &P,
        leaf_index: T,
        key_type: KeyType,
        generation: u32,
    ) -> Result<MessageKeyData, MlsError> {
        let mut ratchet = self.take_leaf_ratchet(cipher_suite, &leaf_index).await?;

        let res = ratchet
            .message_key_generation(cipher_suite, generation, key_type)
            .await?;

        self.known_secrets
            .set_node(leaf_index, SecretTreeNode::Ratchet(ratchet));

        Ok(res)
    }
}

#[derive(Clone, Copy)]
pub enum KeyType {
    Handshake,
    Application,
}

#[cfg_attr(
    all(feature = "ffi", not(test)),
    safer_ffi_gen::ffi_type(clone, opaque)
)]
#[derive(Clone, PartialEq, Eq, MlsEncode, MlsDecode, MlsSize)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
/// AEAD key derived by the MLS secret tree.
pub struct MessageKeyData {
    #[mls_codec(with = "mls_rs_codec::byte_vec")]
    #[cfg_attr(feature = "serde", serde(with = "mls_rs_core::zeroizing_serde"))]
    pub(crate) nonce: Zeroizing<Vec<u8>>,
    #[mls_codec(with = "mls_rs_codec::byte_vec")]
    #[cfg_attr(feature = "serde", serde(with = "mls_rs_core::zeroizing_serde"))]
    pub(crate) key: Zeroizing<Vec<u8>>,
    pub(crate) generation: u32,
}

impl Debug for MessageKeyData {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("MessageKeyData")
            .field("nonce", &mls_rs_core::debug::pretty_bytes(&self.nonce))
            .field("key", &mls_rs_core::debug::pretty_bytes(&self.key))
            .field("generation", &self.generation)
            .finish()
    }
}

#[cfg_attr(all(feature = "ffi", not(test)), safer_ffi_gen::safer_ffi_gen)]
impl MessageKeyData {
    /// AEAD nonce.
    #[cfg_attr(not(feature = "secret_tree_access"), allow(dead_code))]
    pub fn nonce(&self) -> &[u8] {
        &self.nonce
    }

    /// AEAD key.
    #[cfg_attr(not(feature = "secret_tree_access"), allow(dead_code))]
    pub fn key(&self) -> &[u8] {
        &self.key
    }

    /// Generation of this key within the key schedule.
    #[cfg_attr(not(feature = "secret_tree_access"), allow(dead_code))]
    pub fn generation(&self) -> u32 {
        self.generation
    }
}

#[derive(Debug, Clone, PartialEq)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
pub struct SecretKeyRatchet {
    secret: TreeSecret,
    generation: u32,
    #[cfg(all(feature = "out_of_order", feature = "std"))]
    history: HashMap<u32, MessageKeyData>,
    #[cfg(all(feature = "out_of_order", not(feature = "std")))]
    history: BTreeMap<u32, MessageKeyData>,
}

impl MlsSize for SecretKeyRatchet {
    fn mls_encoded_len(&self) -> usize {
        let len = mls_rs_codec::byte_vec::mls_encoded_len(&self.secret)
            + self.generation.mls_encoded_len();

        #[cfg(feature = "out_of_order")]
        return len + mls_rs_codec::iter::mls_encoded_len(self.history.values());
        #[cfg(not(feature = "out_of_order"))]
        return len;
    }
}

#[cfg(feature = "out_of_order")]
impl MlsEncode for SecretKeyRatchet {
    fn mls_encode(&self, writer: &mut Vec<u8>) -> Result<(), mls_rs_codec::Error> {
        mls_rs_codec::byte_vec::mls_encode(&self.secret, writer)?;
        self.generation.mls_encode(writer)?;
        mls_rs_codec::iter::mls_encode(self.history.values(), writer)
    }
}

#[cfg(not(feature = "out_of_order"))]
impl MlsEncode for SecretKeyRatchet {
    fn mls_encode(&self, writer: &mut Vec<u8>) -> Result<(), mls_rs_codec::Error> {
        mls_rs_codec::byte_vec::mls_encode(&self.secret, writer)?;
        self.generation.mls_encode(writer)
    }
}

impl MlsDecode for SecretKeyRatchet {
    fn mls_decode(reader: &mut &[u8]) -> Result<Self, mls_rs_codec::Error> {
        Ok(Self {
            secret: mls_rs_codec::byte_vec::mls_decode(reader)?,
            generation: u32::mls_decode(reader)?,
            #[cfg(all(feature = "std", feature = "out_of_order"))]
            history: mls_rs_codec::iter::mls_decode_collection(reader, |data| {
                let mut items = HashMap::default();

                while !data.is_empty() {
                    let item = MessageKeyData::mls_decode(data)?;
                    items.insert(item.generation, item);
                }

                Ok(items)
            })?,
            #[cfg(all(not(feature = "std"), feature = "out_of_order"))]
            history: mls_rs_codec::iter::mls_decode_collection(reader, |data| {
                let mut items = alloc::collections::BTreeMap::default();

                while !data.is_empty() {
                    let item = MessageKeyData::mls_decode(data)?;
                    items.insert(item.generation, item);
                }

                Ok(items)
            })?,
        })
    }
}

impl SecretKeyRatchet {
    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    async fn new<P: CipherSuiteProvider>(
        cipher_suite_provider: &P,
        secret: &[u8],
        key_type: KeyType,
    ) -> Result<Self, MlsError> {
        let label = match key_type {
            KeyType::Handshake => b"handshake".as_slice(),
            KeyType::Application => b"application".as_slice(),
        };

        let secret = kdf_expand_with_label(cipher_suite_provider, secret, label, &[], None)
            .await
            .map_err(|e| MlsError::CryptoProviderError(e.into_any_error()))?;

        Ok(Self {
            secret: TreeSecret::from(secret),
            generation: 0,
            #[cfg(feature = "out_of_order")]
            history: Default::default(),
        })
    }

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    async fn get_message_key<P: CipherSuiteProvider>(
        &mut self,
        cipher_suite_provider: &P,
        generation: u32,
    ) -> Result<MessageKeyData, MlsError> {
        #[cfg(feature = "out_of_order")]
        if generation < self.generation {
            return self
                .history
                .remove_entry(&generation)
                .map(|(_, mk)| mk)
                .ok_or(MlsError::KeyMissing(generation));
        }

        #[cfg(not(feature = "out_of_order"))]
        if generation < self.generation {
            return Err(MlsError::KeyMissing(generation));
        }

        let max_generation_allowed = self.generation + MAX_RATCHET_BACK_HISTORY;

        if generation > max_generation_allowed {
            return Err(MlsError::InvalidFutureGeneration(generation));
        }

        #[cfg(not(feature = "out_of_order"))]
        while self.generation < generation {
            self.next_message_key(cipher_suite_provider)?;
        }

        #[cfg(feature = "out_of_order")]
        while self.generation < generation {
            let key_data = self.next_message_key(cipher_suite_provider).await?;
            self.history.insert(key_data.generation, key_data);
        }

        self.next_message_key(cipher_suite_provider).await
    }

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    async fn next_message_key<P: CipherSuiteProvider>(
        &mut self,
        cipher_suite_provider: &P,
    ) -> Result<MessageKeyData, MlsError> {
        let generation = self.generation;

        let key = MessageKeyData {
            nonce: self
                .derive_secret(
                    cipher_suite_provider,
                    b"nonce",
                    cipher_suite_provider.aead_nonce_size(),
                )
                .await?,
            key: self
                .derive_secret(
                    cipher_suite_provider,
                    b"key",
                    cipher_suite_provider.aead_key_size(),
                )
                .await?,
            generation,
        };

        self.secret = self
            .derive_secret(
                cipher_suite_provider,
                b"secret",
                cipher_suite_provider.kdf_extract_size(),
            )
            .await?
            .into();

        self.generation = generation + 1;

        Ok(key)
    }

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    async fn derive_secret<P: CipherSuiteProvider>(
        &self,
        cipher_suite_provider: &P,
        label: &[u8],
        len: usize,
    ) -> Result<Zeroizing<Vec<u8>>, MlsError> {
        kdf_expand_with_label(
            cipher_suite_provider,
            self.secret.as_ref(),
            label,
            &self.generation.to_be_bytes(),
            Some(len),
        )
        .await
        .map_err(|e| MlsError::CryptoProviderError(e.into_any_error()))
    }
}

#[cfg(test)]
pub(crate) mod test_utils {
    use alloc::{string::String, vec::Vec};
    use mls_rs_core::crypto::CipherSuiteProvider;
    use zeroize::Zeroizing;

    use crate::{crypto::test_utils::try_test_cipher_suite_provider, tree_kem::math::TreeIndex};

    use super::{KeyType, SecretKeyRatchet, SecretTree};

    pub(crate) fn get_test_tree<T: TreeIndex>(secret: Vec<u8>, leaf_count: T) -> SecretTree<T> {
        SecretTree::new(leaf_count, Zeroizing::new(secret))
    }

    impl SecretTree<u32> {
        pub(crate) fn get_root_secret(&self) -> Vec<u8> {
            self.known_secrets
                .clone()
                .take_node(&self.leaf_count.root())
                .unwrap()
                .into_secret()
                .unwrap()
                .to_vec()
        }
    }

    #[derive(Debug, serde::Serialize, serde::Deserialize)]
    pub struct RatchetInteropTestCase {
        #[serde(with = "hex::serde")]
        secret: Vec<u8>,
        label: String,
        generation: u32,
        length: usize,
        #[serde(with = "hex::serde")]
        out: Vec<u8>,
    }

    #[derive(Debug, serde::Serialize, serde::Deserialize)]
    pub struct InteropTestCase {
        cipher_suite: u16,
        derive_tree_secret: RatchetInteropTestCase,
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn test_basic_crypto_test_vectors() {
        let test_cases: Vec<InteropTestCase> =
            load_test_case_json!(basic_crypto, Vec::<InteropTestCase>::new());

        for test_case in test_cases {
            if let Some(cs) = try_test_cipher_suite_provider(test_case.cipher_suite) {
                test_case.derive_tree_secret.verify(&cs).await
            }
        }
    }

    impl RatchetInteropTestCase {
        #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
        pub async fn verify<P: CipherSuiteProvider>(&self, cs: &P) {
            let mut ratchet = SecretKeyRatchet::new(cs, &self.secret, KeyType::Application)
                .await
                .unwrap();

            ratchet.secret = self.secret.clone().into();
            ratchet.generation = self.generation;

            let computed = ratchet
                .derive_secret(cs, self.label.as_bytes(), self.length)
                .await
                .unwrap();

            assert_eq!(&computed.to_vec(), &self.out);
        }
    }
}

#[cfg(test)]
mod tests {
    use alloc::vec;

    use crate::{
        cipher_suite::CipherSuite,
        client::test_utils::TEST_CIPHER_SUITE,
        crypto::test_utils::{
            test_cipher_suite_provider, try_test_cipher_suite_provider, TestCryptoProvider,
        },
        tree_kem::node::NodeIndex,
    };

    #[cfg(not(mls_build_async))]
    use crate::group::test_utils::random_bytes;

    use super::{test_utils::get_test_tree, *};

    use assert_matches::assert_matches;

    #[cfg(target_arch = "wasm32")]
    use wasm_bindgen_test::wasm_bindgen_test as test;

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn test_secret_tree() {
        test_secret_tree_custom(16u32, (0..16).map(|i| 2 * i).collect(), true).await;
        test_secret_tree_custom(1u64 << 62, (1..62).map(|i| 1u64 << i).collect(), false).await;
    }

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    async fn test_secret_tree_custom<T: TreeIndex>(
        leaf_count: T,
        leaves_to_check: Vec<T>,
        all_deleted: bool,
    ) {
        for cipher_suite in TestCryptoProvider::all_supported_cipher_suites() {
            let cs_provider = test_cipher_suite_provider(cipher_suite);

            let test_secret = vec![0u8; cs_provider.kdf_extract_size()];
            let mut test_tree = get_test_tree(test_secret, leaf_count.clone());

            let mut secrets = Vec::<SecretRatchets>::new();

            for i in &leaves_to_check {
                let secret = test_tree
                    .take_leaf_ratchet(&test_cipher_suite_provider(cipher_suite), i)
                    .await
                    .unwrap();

                secrets.push(secret);
            }

            // Verify the tree is now completely empty
            assert!(!all_deleted || test_tree.known_secrets.inner.is_empty());

            // Verify that all the secrets are unique
            let count = secrets.len();
            secrets.dedup();
            assert_eq!(count, secrets.len());
        }
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn test_secret_key_ratchet() {
        for cipher_suite in TestCryptoProvider::all_supported_cipher_suites() {
            let provider = test_cipher_suite_provider(cipher_suite);

            let mut app_ratchet = SecretKeyRatchet::new(
                &provider,
                &vec![0u8; provider.kdf_extract_size()],
                KeyType::Application,
            )
            .await
            .unwrap();

            let mut handshake_ratchet = SecretKeyRatchet::new(
                &provider,
                &vec![0u8; provider.kdf_extract_size()],
                KeyType::Handshake,
            )
            .await
            .unwrap();

            let app_key_one = app_ratchet.next_message_key(&provider).await.unwrap();
            let app_key_two = app_ratchet.next_message_key(&provider).await.unwrap();
            let app_keys = vec![app_key_one, app_key_two];

            let handshake_key_one = handshake_ratchet.next_message_key(&provider).await.unwrap();
            let handshake_key_two = handshake_ratchet.next_message_key(&provider).await.unwrap();
            let handshake_keys = vec![handshake_key_one, handshake_key_two];

            // Verify that the keys have different outcomes due to their different labels
            assert_ne!(app_keys, handshake_keys);

            // Verify that the keys at each generation are different
            assert_ne!(handshake_keys[0], handshake_keys[1]);
        }
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn test_get_key() {
        for cipher_suite in TestCryptoProvider::all_supported_cipher_suites() {
            let provider = test_cipher_suite_provider(cipher_suite);

            let mut ratchet = SecretKeyRatchet::new(
                &test_cipher_suite_provider(cipher_suite),
                &vec![0u8; provider.kdf_extract_size()],
                KeyType::Application,
            )
            .await
            .unwrap();

            let mut ratchet_clone = ratchet.clone();

            // This will generate keys 0 and 1 in ratchet_clone
            let _ = ratchet_clone.next_message_key(&provider).await.unwrap();
            let clone_2 = ratchet_clone.next_message_key(&provider).await.unwrap();

            // Going back in time should result in an error
            let res = ratchet_clone.get_message_key(&provider, 0).await;
            assert!(res.is_err());

            // Calling get key should be the same as calling next until hitting the desired generation
            let second_key = ratchet
                .get_message_key(&provider, ratchet_clone.generation - 1)
                .await
                .unwrap();

            assert_eq!(clone_2, second_key)
        }
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn test_secret_ratchet() {
        for cipher_suite in TestCryptoProvider::all_supported_cipher_suites() {
            let provider = test_cipher_suite_provider(cipher_suite);

            let mut ratchet = SecretKeyRatchet::new(
                &provider,
                &vec![0u8; provider.kdf_extract_size()],
                KeyType::Application,
            )
            .await
            .unwrap();

            let original_secret = ratchet.secret.clone();
            let _ = ratchet.next_message_key(&provider).await.unwrap();
            let new_secret = ratchet.secret;
            assert_ne!(original_secret, new_secret)
        }
    }

    #[cfg(feature = "out_of_order")]
    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn test_out_of_order_keys() {
        let cipher_suite = TEST_CIPHER_SUITE;
        let provider = test_cipher_suite_provider(cipher_suite);

        let mut ratchet = SecretKeyRatchet::new(&provider, &[0u8; 32], KeyType::Handshake)
            .await
            .unwrap();
        let mut ratchet_clone = ratchet.clone();

        // Ask for all the keys in order from the original ratchet
        let mut ordered_keys = Vec::<MessageKeyData>::new();

        for i in 0..=MAX_RATCHET_BACK_HISTORY {
            ordered_keys.push(ratchet.get_message_key(&provider, i).await.unwrap());
        }

        // Ask for a key at index MAX_RATCHET_BACK_HISTORY in the clone
        let last_key = ratchet_clone
            .get_message_key(&provider, MAX_RATCHET_BACK_HISTORY)
            .await
            .unwrap();

        assert_eq!(last_key, ordered_keys[ordered_keys.len() - 1]);

        // Get all the other keys
        let mut back_history_keys = Vec::<MessageKeyData>::new();

        for i in 0..MAX_RATCHET_BACK_HISTORY - 1 {
            back_history_keys.push(ratchet_clone.get_message_key(&provider, i).await.unwrap());
        }

        assert_eq!(
            back_history_keys,
            ordered_keys[..(MAX_RATCHET_BACK_HISTORY as usize) - 1]
        );
    }

    #[cfg(not(feature = "out_of_order"))]
    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn out_of_order_keys_should_throw_error() {
        let cipher_suite = TEST_CIPHER_SUITE;
        let provider = test_cipher_suite_provider(cipher_suite);

        let mut ratchet = SecretKeyRatchet::new(&provider, &[0u8; 32], KeyType::Handshake)
            .await
            .unwrap();

        ratchet.get_message_key(&provider, 10).await.unwrap();
        let res = ratchet.get_message_key(&provider, 9).await;
        assert_matches!(res, Err(MlsError::KeyMissing(9)))
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn test_too_out_of_order() {
        let cipher_suite = TEST_CIPHER_SUITE;
        let provider = test_cipher_suite_provider(cipher_suite);

        let mut ratchet = SecretKeyRatchet::new(&provider, &[0u8; 32], KeyType::Handshake)
            .await
            .unwrap();

        let res = ratchet
            .get_message_key(&provider, MAX_RATCHET_BACK_HISTORY + 1)
            .await;

        let invalid_generation = MAX_RATCHET_BACK_HISTORY + 1;

        assert_matches!(
            res,
            Err(MlsError::InvalidFutureGeneration(invalid))
            if invalid == invalid_generation
        )
    }

    #[derive(Debug, PartialEq, serde::Serialize, serde::Deserialize)]
    struct Ratchet {
        application_keys: Vec<Vec<u8>>,
        handshake_keys: Vec<Vec<u8>>,
    }

    #[derive(Debug, serde::Serialize, serde::Deserialize)]
    struct TestCase {
        cipher_suite: u16,
        #[serde(with = "hex::serde")]
        encryption_secret: Vec<u8>,
        ratchets: Vec<Ratchet>,
    }

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    async fn get_ratchet_data(
        secret_tree: &mut SecretTree<NodeIndex>,
        cipher_suite: CipherSuite,
    ) -> Vec<Ratchet> {
        let provider = test_cipher_suite_provider(cipher_suite);
        let mut ratchet_data = Vec::new();

        for index in 0..16 {
            let mut ratchets = secret_tree
                .take_leaf_ratchet(&provider, &(index * 2))
                .await
                .unwrap();

            let mut application_keys = Vec::new();

            for _ in 0..20 {
                let key = ratchets
                    .handshake
                    .next_message_key(&provider)
                    .await
                    .unwrap()
                    .mls_encode_to_vec()
                    .unwrap();

                application_keys.push(key);
            }

            let mut handshake_keys = Vec::new();

            for _ in 0..20 {
                let key = ratchets
                    .handshake
                    .next_message_key(&provider)
                    .await
                    .unwrap()
                    .mls_encode_to_vec()
                    .unwrap();

                handshake_keys.push(key);
            }

            ratchet_data.push(Ratchet {
                application_keys,
                handshake_keys,
            });
        }

        ratchet_data
    }

    #[cfg(not(mls_build_async))]
    #[cfg_attr(coverage_nightly, coverage(off))]
    fn generate_test_vector() -> Vec<TestCase> {
        CipherSuite::all()
            .map(|cipher_suite| {
                let provider = test_cipher_suite_provider(cipher_suite);
                let encryption_secret = random_bytes(provider.kdf_extract_size());

                let mut secret_tree =
                    SecretTree::new(16, Zeroizing::new(encryption_secret.clone()));

                TestCase {
                    cipher_suite: cipher_suite.into(),
                    encryption_secret,
                    ratchets: get_ratchet_data(&mut secret_tree, cipher_suite),
                }
            })
            .collect()
    }

    #[cfg(mls_build_async)]
    fn generate_test_vector() -> Vec<TestCase> {
        panic!("Tests cannot be generated in async mode");
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn test_secret_tree_test_vectors() {
        let test_cases: Vec<TestCase> = load_test_case_json!(secret_tree, generate_test_vector());

        for case in test_cases {
            let Some(cs_provider) = try_test_cipher_suite_provider(case.cipher_suite) else {
                continue;
            };

            let mut secret_tree = SecretTree::new(16, Zeroizing::new(case.encryption_secret));
            let ratchet_data = get_ratchet_data(&mut secret_tree, cs_provider.cipher_suite()).await;

            assert_eq!(ratchet_data, case.ratchets);
        }
    }
}

#[cfg(all(test, feature = "rfc_compliant", feature = "std"))]
mod interop_tests {
    #[cfg(not(mls_build_async))]
    use mls_rs_core::crypto::{CipherSuite, CipherSuiteProvider};
    use zeroize::Zeroizing;

    use crate::{
        crypto::test_utils::try_test_cipher_suite_provider,
        group::{ciphertext_processor::InteropSenderData, secret_tree::KeyType},
    };

    use super::SecretTree;

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn interop_test_vector() {
        // The test vector can be found here https://github.com/mlswg/mls-implementations/blob/main/test-vectors/secret-tree.json
        let test_cases = load_interop_test_cases();

        for case in test_cases {
            let Some(cs) = try_test_cipher_suite_provider(case.cipher_suite) else {
                continue;
            };

            case.sender_data.verify(&cs).await;

            let mut tree = SecretTree::new(
                case.leaves.len() as u32,
                Zeroizing::new(case.encryption_secret),
            );

            for (index, leaves) in case.leaves.iter().enumerate() {
                for leaf in leaves.iter() {
                    let key = tree
                        .message_key_generation(
                            &cs,
                            (index as u32) * 2,
                            KeyType::Application,
                            leaf.generation,
                        )
                        .await
                        .unwrap();

                    assert_eq!(key.key.to_vec(), leaf.application_key);
                    assert_eq!(key.nonce.to_vec(), leaf.application_nonce);

                    let key = tree
                        .message_key_generation(
                            &cs,
                            (index as u32) * 2,
                            KeyType::Handshake,
                            leaf.generation,
                        )
                        .await
                        .unwrap();

                    assert_eq!(key.key.to_vec(), leaf.handshake_key);
                    assert_eq!(key.nonce.to_vec(), leaf.handshake_nonce);
                }
            }
        }
    }

    #[derive(Debug, serde::Serialize, serde::Deserialize)]
    struct InteropTestCase {
        cipher_suite: u16,
        #[serde(with = "hex::serde")]
        encryption_secret: Vec<u8>,
        sender_data: InteropSenderData,
        leaves: Vec<Vec<InteropLeaf>>,
    }

    #[derive(Debug, serde::Serialize, serde::Deserialize)]
    struct InteropLeaf {
        generation: u32,
        #[serde(with = "hex::serde")]
        application_key: Vec<u8>,
        #[serde(with = "hex::serde")]
        application_nonce: Vec<u8>,
        #[serde(with = "hex::serde")]
        handshake_key: Vec<u8>,
        #[serde(with = "hex::serde")]
        handshake_nonce: Vec<u8>,
    }

    fn load_interop_test_cases() -> Vec<InteropTestCase> {
        load_test_case_json!(secret_tree_interop, generate_test_vector())
    }

    #[cfg(not(mls_build_async))]
    #[cfg_attr(coverage_nightly, coverage(off))]
    fn generate_test_vector() -> Vec<InteropTestCase> {
        let mut test_cases = vec![];

        for cs in CipherSuite::all() {
            let Some(cs) = try_test_cipher_suite_provider(*cs) else {
                continue;
            };

            let gens = [0, 15];
            let tree_sizes = [1, 8, 32];

            for n_leaves in tree_sizes {
                let encryption_secret = cs.random_bytes_vec(cs.kdf_extract_size()).unwrap();

                let mut tree = SecretTree::new(n_leaves, Zeroizing::new(encryption_secret.clone()));

                let leaves = (0..n_leaves)
                    .map(|leaf| {
                        gens.into_iter()
                            .map(|gen| {
                                let index = leaf * 2u32;

                                let handshake_key = tree
                                    .message_key_generation(&cs, index, KeyType::Handshake, gen)
                                    .unwrap();

                                let app_key = tree
                                    .message_key_generation(&cs, index, KeyType::Application, gen)
                                    .unwrap();

                                InteropLeaf {
                                    generation: gen,
                                    application_key: app_key.key.to_vec(),
                                    application_nonce: app_key.nonce.to_vec(),
                                    handshake_key: handshake_key.key.to_vec(),
                                    handshake_nonce: handshake_key.nonce.to_vec(),
                                }
                            })
                            .collect()
                    })
                    .collect();

                let case = InteropTestCase {
                    cipher_suite: *cs.cipher_suite(),
                    encryption_secret,
                    sender_data: InteropSenderData::new(&cs),
                    leaves,
                };

                test_cases.push(case);
            }
        }

        test_cases
    }

    #[cfg(mls_build_async)]
    fn generate_test_vector() -> Vec<InteropTestCase> {
        panic!("Tests cannot be generated in async mode");
    }
}
