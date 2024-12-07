// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use mls_rs::client_builder::Preferences;
use mls_rs::group::{ReceivedMessage, StateUpdate};
use mls_rs::{CipherSuite, ExtensionList, Group, MlsMessage, ProtocolVersion};

use crate::test_client::{generate_client, TestClientConfig};

#[derive(serde::Serialize, serde::Deserialize, Debug, Default, Clone)]
pub struct TestCase {
    pub cipher_suite: u16,

    pub external_psks: Vec<TestExternalPsk>,
    #[serde(with = "hex::serde")]
    pub key_package: Vec<u8>,
    #[serde(with = "hex::serde")]
    pub signature_priv: Vec<u8>,
    #[serde(with = "hex::serde")]
    pub encryption_priv: Vec<u8>,
    #[serde(with = "hex::serde")]
    pub init_priv: Vec<u8>,

    #[serde(with = "hex::serde")]
    pub welcome: Vec<u8>,
    pub ratchet_tree: Option<TestRatchetTree>,
    #[serde(with = "hex::serde")]
    pub initial_epoch_authenticator: Vec<u8>,

    pub epochs: Vec<TestEpoch>,
}

#[derive(serde::Serialize, serde::Deserialize, Debug, Default, Clone)]
pub struct TestExternalPsk {
    #[serde(with = "hex::serde")]
    pub psk_id: Vec<u8>,
    #[serde(with = "hex::serde")]
    pub psk: Vec<u8>,
}

#[derive(serde::Serialize, serde::Deserialize, Debug, Default, Clone)]
pub struct TestEpoch {
    pub proposals: Vec<TestMlsMessage>,
    #[serde(with = "hex::serde")]
    pub commit: Vec<u8>,
    #[serde(with = "hex::serde")]
    pub epoch_authenticator: Vec<u8>,
}

#[derive(serde::Serialize, serde::Deserialize, Debug, Default, Clone)]
pub struct TestMlsMessage(#[serde(with = "hex::serde")] pub Vec<u8>);

#[derive(serde::Serialize, serde::Deserialize, Debug, Default, Clone)]
pub struct TestRatchetTree(#[serde(with = "hex::serde")] pub Vec<u8>);

impl TestEpoch {
    pub fn new(
        proposals: Vec<MlsMessage>,
        commit: &MlsMessage,
        epoch_authenticator: Vec<u8>,
    ) -> Self {
        let proposals = proposals
            .into_iter()
            .map(|p| TestMlsMessage(p.to_bytes().unwrap()))
            .collect();

        Self {
            proposals,
            commit: commit.to_bytes().unwrap(),
            epoch_authenticator,
        }
    }
}

#[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
pub async fn get_test_groups(
    protocol_version: ProtocolVersion,
    cipher_suite: CipherSuite,
    num_participants: usize,
    preferences: Preferences,
) -> Vec<Group<TestClientConfig>> {
    // Create the group with Alice as the group initiator
    let creator = generate_client(cipher_suite, b"alice".to_vec(), preferences.clone());

    let mut creator_group = creator
        .client
        .create_group_with_id(
            protocol_version,
            cipher_suite,
            b"group".to_vec(),
            creator.identity,
            ExtensionList::default(),
        )
        .await
        .unwrap();

    // Generate random clients that will be members of the group
    let receiver_clients = (0..num_participants - 1)
        .map(|i| {
            generate_client(
                cipher_suite,
                format!("bob{i}").into_bytes(),
                preferences.clone(),
            )
        })
        .collect::<Vec<_>>();

    let mut receiver_keys = Vec::new();

    for client in &receiver_clients {
        let keys = client
            .client
            .generate_key_package_message(protocol_version, cipher_suite, client.identity.clone())
            .await
            .unwrap();

        receiver_keys.push(keys);
    }

    // Add the generated clients to the group the creator made
    let mut commit_builder = creator_group.commit_builder();

    for key in &receiver_keys {
        commit_builder = commit_builder.add_member(key.clone()).unwrap();
    }

    let welcome = commit_builder.build().await.unwrap().welcome_message;

    // Creator can confirm the commit was processed by the server
    #[cfg(feature = "state_update")]
    {
        let commit_description = creator_group.apply_pending_commit().await.unwrap();

        assert!(commit_description.state_update.is_active());
        assert_eq!(commit_description.state_update.new_epoch(), 1);
    }

    #[cfg(not(feature = "state_update"))]
    creator_group.apply_pending_commit().await.unwrap();

    for client in &receiver_clients {
        let res = creator_group
            .member_with_identity(client.identity.credential.as_basic().unwrap().identifier())
            .await;

        assert!(res.is_ok());
    }

    #[cfg(feature = "state_update")]
    assert!(commit_description
        .state_update
        .roster_update()
        .removed()
        .is_empty());

    // Export the tree for receivers
    let tree_data = creator_group.export_tree().unwrap();

    // All the receivers will be able to join the group
    let mut receiver_groups = Vec::new();

    for client in &receiver_clients {
        let test_client = client
            .client
            .join_group(Some(&tree_data), welcome.clone().unwrap())
            .await
            .unwrap()
            .0;

        receiver_groups.push(test_client);
    }

    for one_receiver in &receiver_groups {
        assert!(Group::equal_group_state(&creator_group, one_receiver));
    }

    receiver_groups.insert(0, creator_group);

    receiver_groups
}

#[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
pub async fn all_process_commit_with_update(
    groups: &mut [Group<TestClientConfig>],
    commit: &MlsMessage,
    sender: usize,
) -> Vec<StateUpdate> {
    let mut state_updates = Vec::new();

    for g in groups {
        let state_update = if sender != g.current_member_index() as usize {
            let processed_msg = g.process_incoming_message(commit.clone()).await.unwrap();

            match processed_msg {
                ReceivedMessage::Commit(update) => update.state_update,
                _ => panic!("Expected commit, got {processed_msg:?}"),
            }
        } else {
            g.apply_pending_commit().await.unwrap().state_update
        };

        state_updates.push(state_update);
    }

    state_updates
}

#[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
pub async fn all_process_message(
    groups: &mut [Group<TestClientConfig>],
    message: &MlsMessage,
    sender: usize,
    is_commit: bool,
) {
    for group in groups {
        if sender != group.current_member_index() as usize {
            group
                .process_incoming_message(message.clone())
                .await
                .unwrap();
        } else if is_commit {
            group.apply_pending_commit().await.unwrap();
        }
    }
}

#[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
pub async fn add_random_members(
    first_id: usize,
    num_added: usize,
    committer: usize,
    groups: &mut Vec<Group<TestClientConfig>>,
    test_case: Option<&mut TestCase>,
) {
    let cipher_suite = groups[committer].cipher_suite();
    let committer_index = groups[committer].current_member_index() as usize;

    let mut key_packages = Vec::new();
    let mut new_clients = Vec::new();

    for i in 0..num_added {
        let id = first_id + i;
        let new_client = generate_client(
            cipher_suite,
            format!("dave-{id}").into(),
            Preferences::default(),
        );

        let key_package = new_client
            .client
            .generate_key_package_message(
                ProtocolVersion::MLS_10,
                cipher_suite,
                new_client.identity.clone(),
            )
            .await
            .unwrap();

        key_packages.push(key_package);
        new_clients.push(new_client);
    }

    let committer_group = &mut groups[committer];
    let mut commit = committer_group.commit_builder();

    for key_package in key_packages {
        commit = commit.add_member(key_package).unwrap();
    }

    let commit_output = commit.build().await.unwrap();

    all_process_message(groups, &commit_output.commit_message, committer_index, true).await;

    let auth = groups[committer].epoch_authenticator().unwrap().to_vec();
    let epoch = TestEpoch::new(vec![], &commit_output.commit_message, auth);

    if let Some(tc) = test_case {
        tc.epochs.push(epoch)
    };

    let tree_data = groups[committer].export_tree().unwrap();

    let mut new_groups = Vec::new();

    for client in &new_clients {
        let tree_data = tree_data.clone();
        let commit = commit_output.welcome_message.clone().unwrap();

        let client = client
            .client
            .join_group(Some(&tree_data.clone()), commit)
            .await
            .unwrap()
            .0;

        new_groups.push(client);
    }

    groups.append(&mut new_groups);
}

#[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
pub async fn remove_members(
    removed_members: Vec<usize>,
    committer: usize,
    groups: &mut Vec<Group<TestClientConfig>>,
    test_case: Option<&mut TestCase>,
) {
    let remove_indexes = removed_members
        .iter()
        .map(|removed| groups[*removed].current_member_index())
        .collect::<Vec<u32>>();

    let mut commit_builder = groups[committer].commit_builder();

    for index in remove_indexes {
        commit_builder = commit_builder.remove_member(index).unwrap();
    }

    let commit = commit_builder.build().await.unwrap().commit_message;
    let committer_index = groups[committer].current_member_index() as usize;
    all_process_message(groups, &commit, committer_index, true).await;

    let auth = groups[committer].epoch_authenticator().unwrap().to_vec();
    let epoch = TestEpoch::new(vec![], &commit, auth);

    if let Some(tc) = test_case {
        tc.epochs.push(epoch)
    };

    let mut index = 0;

    groups.retain(|_| {
        index += 1;
        !(removed_members.contains(&(index - 1)))
    });
}
