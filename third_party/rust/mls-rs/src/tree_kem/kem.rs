// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use crate::client::MlsError;
use crate::crypto::{CipherSuiteProvider, SignatureSecretKey};
use crate::group::GroupContext;
use crate::identity::SigningIdentity;
use crate::iter::wrap_iter;
use crate::tree_kem::math as tree_math;
use alloc::vec;
use alloc::vec::Vec;
use itertools::Itertools;
use mls_rs_codec::MlsEncode;
use tree_math::{CopathNode, TreeIndex};

#[cfg(all(not(mls_build_async), feature = "rayon"))]
use {crate::iter::ParallelIteratorExt, rayon::prelude::*};

#[cfg(mls_build_async)]
use futures::{StreamExt, TryStreamExt};

#[cfg(feature = "std")]
use std::collections::HashSet;

use super::hpke_encryption::HpkeEncryptable;
use super::leaf_node::ConfigProperties;
use super::node::NodeTypeResolver;
use super::{
    node::{LeafIndex, NodeIndex},
    path_secret::{PathSecret, PathSecretGenerator},
    TreeKemPrivate, TreeKemPublic, UpdatePath, UpdatePathNode, ValidatedUpdatePath,
};

#[cfg(test)]
use crate::{group::CommitModifiers, signer::Signable};

pub struct TreeKem<'a> {
    tree_kem_public: &'a mut TreeKemPublic,
    private_key: &'a mut TreeKemPrivate,
}

pub struct EncapGeneration {
    pub update_path: UpdatePath,
    pub path_secrets: Vec<Option<PathSecret>>,
    pub commit_secret: PathSecret,
}

impl<'a> TreeKem<'a> {
    pub fn new(
        tree_kem_public: &'a mut TreeKemPublic,
        private_key: &'a mut TreeKemPrivate,
    ) -> Self {
        TreeKem {
            tree_kem_public,
            private_key,
        }
    }

    #[allow(clippy::too_many_arguments)]
    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub async fn encap<P>(
        self,
        context: &mut GroupContext,
        excluding: &[LeafIndex],
        signer: &SignatureSecretKey,
        update_leaf_properties: ConfigProperties,
        signing_identity: Option<SigningIdentity>,
        cipher_suite_provider: &P,
        #[cfg(test)] commit_modifiers: &CommitModifiers,
    ) -> Result<EncapGeneration, MlsError>
    where
        P: CipherSuiteProvider + Send + Sync,
    {
        let self_index = self.private_key.self_index;
        let path = self.tree_kem_public.nodes.direct_copath(self_index);
        let filtered = self.tree_kem_public.nodes.filtered(self_index)?;

        self.private_key.secret_keys.resize(path.len() + 1, None);

        let mut secret_generator = PathSecretGenerator::new(cipher_suite_provider);
        let mut path_secrets = vec![];

        for (i, (node, f)) in path.iter().zip(&filtered).enumerate() {
            if !f {
                let secret = secret_generator.next_secret().await?;

                let (secret_key, public_key) =
                    secret.to_hpke_key_pair(cipher_suite_provider).await?;

                self.private_key.secret_keys[i + 1] = Some(secret_key);
                self.tree_kem_public.update_node(public_key, node.path)?;
                path_secrets.push(Some(secret));
            } else {
                self.private_key.secret_keys[i + 1] = None;
                path_secrets.push(None);
            }
        }

        #[cfg(test)]
        (commit_modifiers.modify_tree)(self.tree_kem_public);

        self.tree_kem_public
            .update_parent_hashes(self_index, false, cipher_suite_provider)
            .await?;

        let update_path_leaf = {
            let own_leaf = self.tree_kem_public.nodes.borrow_as_leaf_mut(self_index)?;

            self.private_key.secret_keys[0] = Some(
                own_leaf
                    .commit(
                        cipher_suite_provider,
                        &context.group_id,
                        *self_index,
                        update_leaf_properties,
                        signing_identity,
                        signer,
                    )
                    .await?,
            );

            #[cfg(test)]
            if let Some(signer) = (commit_modifiers.modify_leaf)(own_leaf, signer) {
                let context = &(context.group_id.as_slice(), *self_index).into();

                own_leaf
                    .sign(cipher_suite_provider, &signer, context)
                    .await
                    .unwrap();
            }

            own_leaf.clone()
        };

        // Tree modifications are all done so we can update the tree hash and encrypt with the new context
        self.tree_kem_public
            .update_hashes(&[self_index], cipher_suite_provider)
            .await?;

        context.tree_hash = self
            .tree_kem_public
            .tree_hash(cipher_suite_provider)
            .await?;

        let context_bytes = context.mls_encode_to_vec()?;

        let node_updates = self
            .encrypt_path_secrets(
                path,
                &path_secrets,
                &context_bytes,
                cipher_suite_provider,
                excluding,
            )
            .await?;

        #[cfg(test)]
        let node_updates = (commit_modifiers.modify_path)(node_updates);

        // Create an update path with the new node and parent node updates
        let update_path = UpdatePath {
            leaf_node: update_path_leaf,
            nodes: node_updates,
        };

        Ok(EncapGeneration {
            update_path,
            path_secrets,
            commit_secret: secret_generator.next_secret().await?,
        })
    }

    #[cfg(any(mls_build_async, not(feature = "rayon")))]
    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    async fn encrypt_path_secrets<P: CipherSuiteProvider>(
        &self,
        path: Vec<CopathNode<NodeIndex>>,
        path_secrets: &[Option<PathSecret>],
        context_bytes: &[u8],
        cipher_suite: &P,
        excluding: &[LeafIndex],
    ) -> Result<Vec<UpdatePathNode>, MlsError> {
        let excluding = excluding.iter().copied().map(NodeIndex::from);

        #[cfg(feature = "std")]
        let excluding = excluding.collect::<HashSet<NodeIndex>>();
        #[cfg(not(feature = "std"))]
        let excluding = excluding.collect::<Vec<NodeIndex>>();

        let mut node_updates = Vec::new();

        for (index, path_secret) in path.into_iter().zip(path_secrets.iter()) {
            if let Some(path_secret) = path_secret {
                node_updates.push(
                    self.encrypt_copath_node_resolution(
                        cipher_suite,
                        path_secret,
                        index.copath,
                        context_bytes,
                        &excluding,
                    )
                    .await?,
                );
            }
        }

        Ok(node_updates)
    }

    #[cfg(all(not(mls_build_async), feature = "rayon"))]
    fn encrypt_path_secrets<P: CipherSuiteProvider>(
        &self,
        path: Vec<CopathNode<NodeIndex>>,
        path_secrets: &[Option<PathSecret>],
        context_bytes: &[u8],
        cipher_suite: &P,
        excluding: &[LeafIndex],
    ) -> Result<Vec<UpdatePathNode>, MlsError> {
        let excluding = excluding.iter().copied().map(NodeIndex::from);

        #[cfg(feature = "std")]
        let excluding = excluding.collect::<HashSet<NodeIndex>>();
        #[cfg(not(feature = "std"))]
        let excluding = excluding.collect::<Vec<NodeIndex>>();

        path.into_par_iter()
            .zip(path_secrets.par_iter())
            .filter_map(|(node, path_secret)| {
                path_secret.as_ref().map(|path_secret| {
                    self.encrypt_copath_node_resolution(
                        cipher_suite,
                        path_secret,
                        node.copath,
                        context_bytes,
                        &excluding,
                    )
                })
            })
            .collect()
    }

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub async fn decap<CP>(
        self,
        sender_index: LeafIndex,
        update_path: &ValidatedUpdatePath,
        added_leaves: &[LeafIndex],
        context_bytes: &[u8],
        cipher_suite_provider: &CP,
    ) -> Result<PathSecret, MlsError>
    where
        CP: CipherSuiteProvider,
    {
        let self_index = self.private_key.self_index;

        let lca_index =
            tree_math::leaf_lca_level(self_index.into(), sender_index.into()) as usize - 2;

        let mut path = self.tree_kem_public.nodes.direct_copath(self_index);
        let leaf = CopathNode::new(self_index.into(), 0);
        path.insert(0, leaf);
        let resolved_pos = self.find_resolved_pos(&path, lca_index)?;

        let ct_pos =
            self.find_ciphertext_pos(path[lca_index].path, path[resolved_pos].path, added_leaves)?;

        let lca_node = update_path.nodes[lca_index]
            .as_ref()
            .ok_or(MlsError::LcaNotFoundInDirectPath)?;

        let ct = lca_node
            .encrypted_path_secret
            .get(ct_pos)
            .ok_or(MlsError::LcaNotFoundInDirectPath)?;

        let secret = self.private_key.secret_keys[resolved_pos]
            .as_ref()
            .ok_or(MlsError::UpdateErrorNoSecretKey)?;

        let public = self
            .tree_kem_public
            .nodes
            .borrow_node(path[resolved_pos].path)?
            .as_ref()
            .ok_or(MlsError::UpdateErrorNoSecretKey)?
            .public_key();

        let lca_path_secret =
            PathSecret::decrypt(cipher_suite_provider, secret, public, context_bytes, ct).await?;

        // Derive the rest of the secrets for the tree and assign to the proper nodes
        let mut node_secret_gen =
            PathSecretGenerator::starting_with(cipher_suite_provider, lca_path_secret);

        // Update secrets based on the decrypted path secret in the update
        self.private_key.secret_keys.resize(path.len() + 1, None);

        for (i, update) in update_path.nodes.iter().enumerate().skip(lca_index) {
            if let Some(update) = update {
                let secret = node_secret_gen.next_secret().await?;

                // Verify the private key we calculated properly matches the public key we inserted into the tree. This guarantees
                // that we will be able to decrypt later.
                let (hpke_private, hpke_public) =
                    secret.to_hpke_key_pair(cipher_suite_provider).await?;

                if hpke_public != update.public_key {
                    return Err(MlsError::PubKeyMismatch);
                }

                self.private_key.secret_keys[i + 1] = Some(hpke_private);
            } else {
                self.private_key.secret_keys[i + 1] = None;
            }
        }

        node_secret_gen.next_secret().await
    }

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    async fn encrypt_copath_node_resolution<P: CipherSuiteProvider>(
        &self,
        cipher_suite_provider: &P,
        path_secret: &PathSecret,
        copath_index: NodeIndex,
        context: &[u8],
        #[cfg(feature = "std")] excluding: &HashSet<NodeIndex>,
        #[cfg(not(feature = "std"))] excluding: &[NodeIndex],
    ) -> Result<UpdatePathNode, MlsError> {
        let reso = self
            .tree_kem_public
            .nodes
            .get_resolution_index(copath_index)?;

        let make_ctxt = |idx| async move {
            let node = self
                .tree_kem_public
                .nodes
                .borrow_node(idx)?
                .as_non_empty()?;

            path_secret
                .encrypt(cipher_suite_provider, node.public_key(), context)
                .await
        };

        let ctxts = wrap_iter(reso).filter(|&idx| async move { !excluding.contains(&idx) });

        #[cfg(not(mls_build_async))]
        let ctxts = ctxts.map(make_ctxt);

        #[cfg(mls_build_async)]
        let ctxts = ctxts.then(make_ctxt);

        let ctxts = ctxts.try_collect().await?;

        let path_index = copath_index
            .parent_sibling(&self.tree_kem_public.total_leaf_count())
            .ok_or(MlsError::ExpectedNode)?
            .parent;

        Ok(UpdatePathNode {
            public_key: self
                .tree_kem_public
                .nodes
                .borrow_as_parent(path_index)?
                .public_key
                .clone(),
            encrypted_path_secret: ctxts,
        })
    }

    #[inline]
    fn find_resolved_pos(
        &self,
        path: &[CopathNode<NodeIndex>],
        mut lca_index: usize,
    ) -> Result<usize, MlsError> {
        while self.tree_kem_public.nodes.is_blank(path[lca_index].path)? {
            lca_index -= 1;
        }

        // If we don't have the key, we should be an unmerged leaf at the resolved node. (If
        // we're not, an error will be thrown later.)
        if self.private_key.secret_keys[lca_index].is_none() {
            lca_index = 0;
        }

        Ok(lca_index)
    }

    #[inline]
    fn find_ciphertext_pos(
        &self,
        lca: NodeIndex,
        resolved: NodeIndex,
        excluding: &[LeafIndex],
    ) -> Result<usize, MlsError> {
        let reso = self.tree_kem_public.nodes.get_resolution_index(lca)?;

        let (ct_pos, _) = reso
            .iter()
            .filter(|idx| **idx % 2 == 1 || !excluding.contains(&LeafIndex(**idx / 2)))
            .find_position(|idx| idx == &&resolved)
            .ok_or(MlsError::UpdateErrorNoSecretKey)?;

        Ok(ct_pos)
    }
}

#[cfg(test)]
mod tests {
    use super::{tree_math, TreeKem};
    use crate::{
        cipher_suite::CipherSuite,
        client::test_utils::TEST_CIPHER_SUITE,
        crypto::test_utils::{test_cipher_suite_provider, TestCryptoProvider},
        extension::test_utils::TestExtension,
        group::test_utils::{get_test_group_context, random_bytes},
        identity::basic::BasicIdentityProvider,
        tree_kem::{
            leaf_node::{
                test_utils::{get_basic_test_node_sig_key, get_test_capabilities},
                ConfigProperties,
            },
            node::LeafIndex,
            Capabilities, TreeKemPrivate, TreeKemPublic, UpdatePath, ValidatedUpdatePath,
        },
        ExtensionList,
    };
    use alloc::{format, vec, vec::Vec};
    use mls_rs_codec::MlsEncode;
    use mls_rs_core::crypto::CipherSuiteProvider;
    use tree_math::TreeIndex;

    // Verify that the tree is in the correct state after generating an update path
    fn verify_tree_update_path(
        tree: &TreeKemPublic,
        update_path: &UpdatePath,
        index: LeafIndex,
        capabilities: Option<Capabilities>,
        extensions: Option<ExtensionList>,
    ) {
        // Make sure the update path is based on the direct path of the sender
        let direct_path = tree.nodes.direct_copath(index);

        for (i, n) in direct_path.iter().enumerate() {
            assert_eq!(
                *tree
                    .nodes
                    .borrow_node(n.path)
                    .unwrap()
                    .as_ref()
                    .unwrap()
                    .public_key(),
                update_path.nodes[i].public_key
            );
        }

        // Verify that the leaf from the update path has been installed
        assert_eq!(
            tree.nodes.borrow_as_leaf(index).unwrap(),
            &update_path.leaf_node
        );

        // Verify that updated capabilities were installed
        if let Some(capabilities) = capabilities {
            assert_eq!(update_path.leaf_node.capabilities, capabilities);
        }

        // Verify that update extensions were installed
        if let Some(extensions) = extensions {
            assert_eq!(update_path.leaf_node.extensions, extensions);
        }

        // Verify that we have a public keys up to the root
        let root = tree.total_leaf_count().root();
        assert!(tree.nodes.borrow_node(root).unwrap().is_some());
    }

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    async fn verify_tree_private_path(
        cipher_suite: &CipherSuite,
        public_tree: &TreeKemPublic,
        private_tree: &TreeKemPrivate,
        index: LeafIndex,
    ) {
        let provider = test_cipher_suite_provider(*cipher_suite);

        assert_eq!(private_tree.self_index, index);

        // Make sure we have private values along the direct path, and the public keys match
        let path_iter = public_tree
            .nodes
            .direct_copath(index)
            .into_iter()
            .enumerate();

        for (i, n) in path_iter {
            let secret_key = private_tree.secret_keys[i + 1].as_ref().unwrap();

            let public_key = public_tree
                .nodes
                .borrow_node(n.path)
                .unwrap()
                .as_ref()
                .unwrap()
                .public_key();

            let test_data = random_bytes(32);

            let sealed = provider
                .hpke_seal(public_key, &[], None, &test_data)
                .await
                .unwrap();

            let opened = provider
                .hpke_open(&sealed, secret_key, public_key, &[], None)
                .await
                .unwrap();

            assert_eq!(test_data, opened);
        }
    }

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    async fn encap_decap(
        cipher_suite: CipherSuite,
        size: usize,
        capabilities: Option<Capabilities>,
        extensions: Option<ExtensionList>,
    ) {
        let cipher_suite_provider = test_cipher_suite_provider(cipher_suite);

        // Generate signing keys and key package generations, and private keys for multiple
        // participants in order to set up state

        let mut leaf_nodes = Vec::new();
        let mut private_keys = Vec::new();

        for index in 1..size {
            let (leaf_node, hpke_secret, _) =
                get_basic_test_node_sig_key(cipher_suite, &format!("{index}")).await;

            let private_key = TreeKemPrivate::new_self_leaf(LeafIndex(index as u32), hpke_secret);

            leaf_nodes.push(leaf_node);
            private_keys.push(private_key);
        }

        let (encap_node, encap_hpke_secret, encap_signer) =
            get_basic_test_node_sig_key(cipher_suite, "encap").await;

        // Build a test tree we can clone for all leaf nodes
        let (mut test_tree, mut encap_private_key) = TreeKemPublic::derive(
            encap_node,
            encap_hpke_secret,
            &BasicIdentityProvider,
            &Default::default(),
        )
        .await
        .unwrap();

        test_tree
            .add_leaves(leaf_nodes, &BasicIdentityProvider, &cipher_suite_provider)
            .await
            .unwrap();

        // Clone the tree for the first leaf, generate a new key package for that leaf
        let mut encap_tree = test_tree.clone();

        let update_leaf_properties = ConfigProperties {
            capabilities: capabilities.clone().unwrap_or_else(get_test_capabilities),
            extensions: extensions.clone().unwrap_or_default(),
        };

        // Perform the encap function
        let encap_gen = TreeKem::new(&mut encap_tree, &mut encap_private_key)
            .encap(
                &mut get_test_group_context(42, cipher_suite).await,
                &[],
                &encap_signer,
                update_leaf_properties,
                None,
                &cipher_suite_provider,
                #[cfg(test)]
                &Default::default(),
            )
            .await
            .unwrap();

        // Verify that the state of the tree matches the produced update path
        verify_tree_update_path(
            &encap_tree,
            &encap_gen.update_path,
            LeafIndex(0),
            capabilities,
            extensions,
        );

        // Verify that the private key matches the data in the public key
        verify_tree_private_path(&cipher_suite, &encap_tree, &encap_private_key, LeafIndex(0))
            .await;

        let filtered = test_tree.nodes.filtered(LeafIndex(0)).unwrap();
        let mut unfiltered_nodes = vec![None; filtered.len()];
        filtered
            .into_iter()
            .enumerate()
            .filter(|(_, f)| !*f)
            .zip(encap_gen.update_path.nodes.iter())
            .for_each(|((i, _), node)| {
                unfiltered_nodes[i] = Some(node.clone());
            });

        // Apply the update path to the rest of the leaf nodes using the decap function
        let validated_update_path = ValidatedUpdatePath {
            leaf_node: encap_gen.update_path.leaf_node,
            nodes: unfiltered_nodes,
        };

        encap_tree
            .update_hashes(&[LeafIndex(0)], &cipher_suite_provider)
            .await
            .unwrap();

        let mut receiver_trees: Vec<TreeKemPublic> = (1..size).map(|_| test_tree.clone()).collect();

        for (i, tree) in receiver_trees.iter_mut().enumerate() {
            tree.apply_update_path(
                LeafIndex(0),
                &validated_update_path,
                &Default::default(),
                BasicIdentityProvider,
                &cipher_suite_provider,
            )
            .await
            .unwrap();

            let mut context = get_test_group_context(42, cipher_suite).await;
            context.tree_hash = tree.tree_hash(&cipher_suite_provider).await.unwrap();

            TreeKem::new(tree, &mut private_keys[i])
                .decap(
                    LeafIndex(0),
                    &validated_update_path,
                    &[],
                    &context.mls_encode_to_vec().unwrap(),
                    &cipher_suite_provider,
                )
                .await
                .unwrap();

            tree.update_hashes(&[LeafIndex(0)], &cipher_suite_provider)
                .await
                .unwrap();

            assert_eq!(tree, &encap_tree);
        }
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn test_encap_decap() {
        for cipher_suite in TestCryptoProvider::all_supported_cipher_suites() {
            encap_decap(cipher_suite, 10, None, None).await;
        }
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn test_encap_capabilities() {
        let cipher_suite = TEST_CIPHER_SUITE;
        let mut capabilities = get_test_capabilities();
        capabilities.extensions.push(42.into());

        encap_decap(cipher_suite, 10, Some(capabilities.clone()), None).await;
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn test_encap_extensions() {
        let cipher_suite = TEST_CIPHER_SUITE;
        let mut extensions = ExtensionList::default();
        extensions.set_from(TestExtension { foo: 10 }).unwrap();

        encap_decap(cipher_suite, 10, None, Some(extensions)).await;
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn test_encap_capabilities_extensions() {
        let cipher_suite = TEST_CIPHER_SUITE;
        let mut capabilities = get_test_capabilities();
        capabilities.extensions.push(42.into());

        let mut extensions = ExtensionList::default();
        extensions.set_from(TestExtension { foo: 10 }).unwrap();

        encap_decap(cipher_suite, 10, Some(capabilities), Some(extensions)).await;
    }
}
