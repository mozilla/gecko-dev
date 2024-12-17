// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)
use alloc::{vec, vec::Vec};

use mls_rs_codec::{MlsDecode, MlsEncode, MlsSize};
use mls_rs_core::crypto::HpkeSecretKey;

use crate::{client::MlsError, crypto::CipherSuiteProvider};

use super::{
    math::leaf_lca_level,
    node::LeafIndex,
    path_secret::{PathSecret, PathSecretGenerator},
    TreeKemPublic,
};

#[derive(Clone, Debug, MlsEncode, MlsDecode, MlsSize, Eq, PartialEq)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
#[non_exhaustive]
pub struct TreeKemPrivate {
    pub self_index: LeafIndex,
    pub secret_keys: Vec<Option<HpkeSecretKey>>,
}

impl TreeKemPrivate {
    pub fn new_self_leaf(self_index: LeafIndex, leaf_secret: HpkeSecretKey) -> Self {
        TreeKemPrivate {
            self_index,
            secret_keys: vec![Some(leaf_secret)],
        }
    }

    pub fn new_for_external() -> Self {
        TreeKemPrivate {
            self_index: LeafIndex(0),
            secret_keys: Default::default(),
        }
    }

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub async fn update_secrets<P: CipherSuiteProvider>(
        &mut self,
        cipher_suite_provider: &P,
        signer_index: LeafIndex,
        path_secret: PathSecret,
        public_tree: &TreeKemPublic,
    ) -> Result<(), MlsError> {
        // Identify the lowest common
        // ancestor of the leaves at index and at GroupInfo.signer_index. Set the private key
        // for this node to the private key derived from the path_secret.
        let lca_index = leaf_lca_level(self.self_index.into(), signer_index.into()) as usize - 2;

        // For each parent of the common ancestor, up to the root of the tree, derive a new
        // path secret and set the private key for the node to the private key derived from the
        // path secret. The private key MUST be the private key that corresponds to the public
        // key in the node.

        let mut node_secret_gen =
            PathSecretGenerator::starting_with(cipher_suite_provider, path_secret);

        let path = public_tree.nodes.direct_copath(self.self_index);
        let filtered = &public_tree.nodes.filtered(self.self_index)?;
        self.secret_keys.resize(path.len() + 1, None);

        for (i, (n, f)) in path.iter().zip(filtered).enumerate().skip(lca_index) {
            if *f {
                continue;
            }

            let secret = node_secret_gen.next_secret().await?;

            let expected_pub_key = public_tree
                .nodes
                .borrow_node(n.path)?
                .as_ref()
                .map(|n| n.public_key())
                .ok_or(MlsError::PubKeyMismatch)?;

            let (secret_key, public_key) = secret.to_hpke_key_pair(cipher_suite_provider).await?;

            if expected_pub_key != &public_key {
                return Err(MlsError::PubKeyMismatch);
            }

            // It's ok to use index directly because of the resize above
            self.secret_keys[i + 1] = Some(secret_key);
        }

        Ok(())
    }

    #[cfg(feature = "by_ref_proposal")]
    pub fn update_leaf(&mut self, new_leaf: HpkeSecretKey) {
        self.secret_keys = vec![None; self.secret_keys.len()];
        self.secret_keys[0] = Some(new_leaf);
    }
}

#[cfg(test)]
impl TreeKemPrivate {
    pub fn new(self_index: LeafIndex) -> Self {
        TreeKemPrivate {
            self_index,
            secret_keys: Default::default(),
        }
    }
}

#[cfg(test)]
mod tests {
    use assert_matches::assert_matches;

    use crate::{
        cipher_suite::CipherSuite,
        client::test_utils::TEST_CIPHER_SUITE,
        crypto::test_utils::test_cipher_suite_provider,
        group::test_utils::{get_test_group_context, random_bytes},
        identity::basic::BasicIdentityProvider,
        tree_kem::{
            kem::TreeKem,
            leaf_node::test_utils::{
                default_properties, get_basic_test_node, get_basic_test_node_sig_key,
            },
            math::TreeIndex,
            node::LeafIndex,
        },
    };

    use super::*;

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    async fn random_hpke_secret_key() -> HpkeSecretKey {
        let (secret, _) = test_cipher_suite_provider(TEST_CIPHER_SUITE)
            .kem_derive(&random_bytes(32))
            .await
            .unwrap();

        secret
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn test_create_self_leaf() {
        let secret = random_hpke_secret_key().await;

        let self_index = LeafIndex(42);

        let private_key = TreeKemPrivate::new_self_leaf(self_index, secret.clone());

        assert_eq!(private_key.self_index, self_index);
        assert_eq!(private_key.secret_keys.len(), 1);
        assert_eq!(private_key.secret_keys[0].as_ref().unwrap(), &secret)
    }

    // Create a ratchet tree for Alice, Bob and Charlie. Alice generates an update path for
    // Charlie. Return (Public Tree, Charlie's private key, update path, path secret)
    // The ratchet tree returned has leaf indexes as [alice, bob, charlie]
    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    async fn update_secrets_setup(
        cipher_suite: CipherSuite,
    ) -> (TreeKemPublic, TreeKemPrivate, TreeKemPrivate, PathSecret) {
        let cipher_suite_provider = test_cipher_suite_provider(cipher_suite);

        let (alice_leaf, alice_hpke_secret, alice_signing) =
            get_basic_test_node_sig_key(cipher_suite, "alice").await;

        let bob_leaf = get_basic_test_node(cipher_suite, "bob").await;

        let (charlie_leaf, charlie_hpke_secret, _charlie_signing) =
            get_basic_test_node_sig_key(cipher_suite, "charlie").await;

        // Create a new public tree with Alice
        let (mut public_tree, mut alice_private) = TreeKemPublic::derive(
            alice_leaf,
            alice_hpke_secret,
            &BasicIdentityProvider,
            &Default::default(),
        )
        .await
        .unwrap();

        // Add bob and charlie to the tree
        public_tree
            .add_leaves(
                vec![bob_leaf, charlie_leaf],
                &BasicIdentityProvider,
                &cipher_suite_provider,
            )
            .await
            .unwrap();

        // Alice's secret key is longer now
        alice_private.secret_keys.resize(3, None);

        // Generate an update path for Alice
        let encap_gen = TreeKem::new(&mut public_tree, &mut alice_private)
            .encap(
                &mut get_test_group_context(42, cipher_suite).await,
                &[],
                &alice_signing,
                default_properties(),
                None,
                &cipher_suite_provider,
                #[cfg(test)]
                &Default::default(),
            )
            .await
            .unwrap();

        // Get a path secret from Alice for Charlie
        let path_secret = encap_gen.path_secrets[1].clone().unwrap();

        // Private key for Charlie
        let charlie_private = TreeKemPrivate::new_self_leaf(LeafIndex(2), charlie_hpke_secret);

        (public_tree, charlie_private, alice_private, path_secret)
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn test_update_secrets() {
        let cipher_suite = TEST_CIPHER_SUITE;

        let (public_tree, mut charlie_private, alice_private, path_secret) =
            update_secrets_setup(cipher_suite).await;

        let existing_private = charlie_private.secret_keys.first().cloned().unwrap();

        // Add the secrets for Charlie to his private key
        charlie_private
            .update_secrets(
                &test_cipher_suite_provider(cipher_suite),
                LeafIndex(0),
                path_secret,
                &public_tree,
            )
            .await
            .unwrap();

        // Make sure that Charlie's private key didn't lose keys
        assert_eq!(charlie_private.secret_keys.len(), 3);

        // Check that the intersection of the secret keys of Alice and Charlie matches.
        // The intersection contains only the root.
        assert_eq!(alice_private.secret_keys[2], charlie_private.secret_keys[2]);

        assert_eq!(
            charlie_private.secret_keys[0].as_ref(),
            existing_private.as_ref()
        );
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn test_update_secrets_key_mismatch() {
        let cipher_suite = TEST_CIPHER_SUITE;

        let (mut public_tree, mut charlie_private, _, path_secret) =
            update_secrets_setup(cipher_suite).await;

        // Sabotage the public tree
        public_tree
            .nodes
            .borrow_as_parent_mut(public_tree.total_leaf_count().root())
            .unwrap()
            .public_key = random_bytes(32).into();

        // Add the secrets for Charlie to his private key
        let res = charlie_private
            .update_secrets(
                &test_cipher_suite_provider(cipher_suite),
                LeafIndex(0),
                path_secret,
                &public_tree,
            )
            .await;

        assert_matches!(res, Err(MlsError::PubKeyMismatch));
    }

    #[cfg(feature = "by_ref_proposal")]
    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    async fn setup_direct_path(self_index: LeafIndex, leaf_count: u32) -> TreeKemPrivate {
        let secret = random_hpke_secret_key().await;

        let mut private_key = TreeKemPrivate::new_self_leaf(self_index, secret.clone());

        private_key.secret_keys = (0..0.direct_copath(&leaf_count).len() + 1)
            .map(|_| Some(secret.clone()))
            .collect();

        private_key
    }

    #[cfg(feature = "by_ref_proposal")]
    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn test_update_leaf() {
        let self_leaf = LeafIndex(42);
        let mut private_key = setup_direct_path(self_leaf, 128).await;

        let new_secret = random_hpke_secret_key().await;

        private_key.update_leaf(new_secret.clone());

        // The update operation should have removed all the other keys in our direct path we
        // previously added
        assert!(private_key.secret_keys.iter().skip(1).all(|n| n.is_none()));

        // The secret key for our leaf should have been updated accordingly
        assert_eq!(private_key.secret_keys.first().unwrap(), &Some(new_secret));
    }
}
