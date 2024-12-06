// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use assert_matches::assert_matches;
use cfg_if::cfg_if;
use mls_rs::client_builder::MlsConfig;
use mls_rs::error::MlsError;
use mls_rs::group::proposal::Proposal;
use mls_rs::group::ReceivedMessage;
use mls_rs::identity::SigningIdentity;
use mls_rs::mls_rules::CommitOptions;
use mls_rs::ExtensionList;
use mls_rs::MlsMessage;
use mls_rs::ProtocolVersion;
use mls_rs::{CipherSuite, Group};
use mls_rs::{Client, CryptoProvider};
use mls_rs_core::crypto::CipherSuiteProvider;
use rand::prelude::SliceRandom;
use rand::RngCore;

use mls_rs::test_utils::{all_process_message, get_test_basic_credential};

#[cfg(mls_build_async)]
use futures::Future;

cfg_if! {
    if #[cfg(target_arch = "wasm32")] {
        use mls_rs_crypto_webcrypto::WebCryptoProvider as TestCryptoProvider;
    } else {
        use mls_rs_crypto_openssl::OpensslCryptoProvider as TestCryptoProvider;
    }
}

#[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
async fn generate_client(
    cipher_suite: CipherSuite,
    protocol_version: ProtocolVersion,
    id: usize,
    encrypt_controls: bool,
) -> Client<impl MlsConfig> {
    mls_rs::test_utils::generate_basic_client(
        cipher_suite,
        protocol_version,
        id,
        None,
        encrypt_controls,
        &TestCryptoProvider::default(),
        None,
    )
    .await
}

#[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
pub async fn get_test_groups(
    version: ProtocolVersion,
    cipher_suite: CipherSuite,
    num_participants: usize,
    encrypt_controls: bool,
) -> Vec<Group<impl MlsConfig>> {
    mls_rs::test_utils::get_test_groups(
        version,
        cipher_suite,
        num_participants,
        None,
        encrypt_controls,
        &TestCryptoProvider::default(),
    )
    .await
}

use rand::seq::IteratorRandom;

#[cfg(target_arch = "wasm32")]
wasm_bindgen_test::wasm_bindgen_test_configure!(run_in_browser);

#[cfg(target_arch = "wasm32")]
use wasm_bindgen_test::wasm_bindgen_test as futures_test;

#[cfg(all(mls_build_async, not(target_arch = "wasm32")))]
use futures_test::test as futures_test;

#[cfg(feature = "private_message")]
#[cfg(mls_build_async)]
async fn test_on_all_params<F, Fut>(test: F)
where
    F: Fn(ProtocolVersion, CipherSuite, usize, bool) -> Fut,
    Fut: Future<Output = ()>,
{
    for version in ProtocolVersion::all() {
        for cs in TestCryptoProvider::all_supported_cipher_suites() {
            for encrypt_controls in [true, false] {
                test(version, cs, 10, encrypt_controls).await;
            }
        }
    }
}

#[cfg(feature = "private_message")]
#[cfg(not(mls_build_async))]
fn test_on_all_params<F>(test: F)
where
    F: Fn(ProtocolVersion, CipherSuite, usize, bool),
{
    for version in ProtocolVersion::all() {
        for cs in TestCryptoProvider::all_supported_cipher_suites() {
            for encrypt_controls in [true, false] {
                test(version, cs, 10, encrypt_controls);
            }
        }
    }
}

#[cfg(not(feature = "private_message"))]
#[cfg(mls_build_async)]
async fn test_on_all_params<F, Fut>(test: F)
where
    F: Fn(ProtocolVersion, CipherSuite, usize, bool) -> Fut,
    Fut: Future<Output = ()>,
{
    test_on_all_params_plaintext(test).await;
}

#[cfg(not(feature = "private_message"))]
#[cfg(not(mls_build_async))]
fn test_on_all_params<F>(test: F)
where
    F: Fn(ProtocolVersion, CipherSuite, usize, bool),
{
    test_on_all_params_plaintext(test);
}

#[cfg(mls_build_async)]
async fn test_on_all_params_plaintext<F, Fut>(test: F)
where
    F: Fn(ProtocolVersion, CipherSuite, usize, bool) -> Fut,
    Fut: Future<Output = ()>,
{
    for version in ProtocolVersion::all() {
        for cs in TestCryptoProvider::all_supported_cipher_suites() {
            test(version, cs, 10, false).await;
        }
    }
}

#[cfg(not(mls_build_async))]
fn test_on_all_params_plaintext<F>(test: F)
where
    F: Fn(ProtocolVersion, CipherSuite, usize, bool),
{
    for version in ProtocolVersion::all() {
        for cs in TestCryptoProvider::all_supported_cipher_suites() {
            test(version, cs, 10, false);
        }
    }
}

#[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
async fn test_create(
    protocol_version: ProtocolVersion,
    cipher_suite: CipherSuite,
    _n_participants: usize,
    encrypt_controls: bool,
) {
    let alice = generate_client(cipher_suite, protocol_version, 0, encrypt_controls).await;
    let bob = generate_client(cipher_suite, protocol_version, 1, encrypt_controls).await;
    let bob_key_pkg = bob.generate_key_package_message().await.unwrap();

    // Alice creates a group and adds bob
    let mut alice_group = alice
        .create_group_with_id(b"group".to_vec(), ExtensionList::default())
        .await
        .unwrap();

    let welcome = &alice_group
        .commit_builder()
        .add_member(bob_key_pkg)
        .unwrap()
        .build()
        .await
        .unwrap()
        .welcome_messages[0];

    // Upon server confirmation, alice applies the commit to her own state
    alice_group.apply_pending_commit().await.unwrap();

    // Bob receives the welcome message and joins the group
    let (bob_group, _) = bob.join_group(None, welcome).await.unwrap();

    assert!(Group::equal_group_state(&alice_group, &bob_group));
}

#[maybe_async::test(not(mls_build_async), async(mls_build_async, futures_test))]
async fn test_create_group() {
    test_on_all_params(test_create).await;
}

#[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
async fn test_empty_commits(
    protocol_version: ProtocolVersion,
    cipher_suite: CipherSuite,
    participants: usize,
    encrypt_controls: bool,
) {
    let mut groups = get_test_groups(
        protocol_version,
        cipher_suite,
        participants,
        encrypt_controls,
    )
    .await;

    // Loop through each participant and send a path update

    for i in 0..groups.len() {
        // Create the commit
        let commit_output = groups[i].commit(Vec::new()).await.unwrap();

        assert!(commit_output.welcome_messages.is_empty());

        let index = groups[i].current_member_index() as usize;
        all_process_message(&mut groups, &commit_output.commit_message, index, true).await;

        for other_group in groups.iter() {
            assert!(Group::equal_group_state(other_group, &groups[i]));
        }
    }
}

#[maybe_async::test(not(mls_build_async), async(mls_build_async, futures_test))]
async fn test_group_path_updates() {
    test_on_all_params(test_empty_commits).await;
}

#[cfg(feature = "by_ref_proposal")]
#[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
async fn test_update_proposals(
    protocol_version: ProtocolVersion,
    cipher_suite: CipherSuite,
    participants: usize,
    encrypt_controls: bool,
) {
    let mut groups = get_test_groups(
        protocol_version,
        cipher_suite,
        participants,
        encrypt_controls,
    )
    .await;

    // Create an update from the ith member, have the ith + 1 member commit it
    for i in 0..groups.len() - 1 {
        let update_proposal_msg = groups[i].propose_update(Vec::new()).await.unwrap();

        let sender = groups[i].current_member_index() as usize;
        all_process_message(&mut groups, &update_proposal_msg, sender, false).await;

        // Everyone receives the commit
        let committer_index = i + 1;

        let commit_output = groups[committer_index].commit(Vec::new()).await.unwrap();

        assert!(commit_output.welcome_messages.is_empty());

        let commit = commit_output.commit_message;

        all_process_message(&mut groups, &commit, committer_index, true).await;

        groups
            .iter()
            .for_each(|g| assert!(Group::equal_group_state(g, &groups[0])));
    }
}

#[cfg(feature = "by_ref_proposal")]
#[maybe_async::test(not(mls_build_async), async(mls_build_async, futures_test))]
async fn test_group_update_proposals() {
    test_on_all_params(test_update_proposals).await;
}

#[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
async fn test_remove_proposals(
    protocol_version: ProtocolVersion,
    cipher_suite: CipherSuite,
    participants: usize,
    encrypt_controls: bool,
) {
    let mut groups = get_test_groups(
        protocol_version,
        cipher_suite,
        participants,
        encrypt_controls,
    )
    .await;

    // Remove people from the group one at a time
    while groups.len() > 1 {
        let removed_and_committer = (0..groups.len()).choose_multiple(&mut rand::thread_rng(), 2);

        let to_remove = removed_and_committer[0];
        let committer = removed_and_committer[1];
        let to_remove_index = groups[to_remove].current_member_index();

        let epoch_before_remove = groups[committer].current_epoch();

        let commit_output = groups[committer]
            .commit_builder()
            .remove_member(to_remove_index)
            .unwrap()
            .build()
            .await
            .unwrap();

        assert!(commit_output.welcome_messages.is_empty());

        let commit = commit_output.commit_message;
        let committer_index = groups[committer].current_member_index() as usize;
        all_process_message(&mut groups, &commit, committer_index, true).await;

        // Check that remove was effective
        for (i, group) in groups.iter().enumerate() {
            if i == to_remove {
                assert_eq!(group.current_epoch(), epoch_before_remove);
            } else {
                assert_eq!(group.current_epoch(), epoch_before_remove + 1);
                assert!(group.roster().member_with_index(to_remove_index).is_err());
            }
        }

        groups.retain(|group| group.current_member_index() != to_remove_index);

        for one_group in groups.iter() {
            assert!(Group::equal_group_state(one_group, &groups[0]))
        }
    }
}

#[maybe_async::test(not(mls_build_async), async(mls_build_async, futures_test))]
async fn test_group_remove_proposals() {
    test_on_all_params(test_remove_proposals).await;
}

#[cfg(feature = "private_message")]
#[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
async fn test_application_messages(
    protocol_version: ProtocolVersion,
    cipher_suite: CipherSuite,
    participants: usize,
    encrypt_controls: bool,
) {
    let message_count = 20;

    let mut groups = get_test_groups(
        protocol_version,
        cipher_suite,
        participants,
        encrypt_controls,
    )
    .await;

    // Loop through each participant and send application messages
    for i in 0..groups.len() {
        let mut test_message = vec![0; 1024];
        rand::thread_rng().fill_bytes(&mut test_message);

        for _ in 0..message_count {
            // Encrypt the application message
            let ciphertext = groups[i]
                .encrypt_application_message(&test_message, Vec::new())
                .await
                .unwrap();

            let sender_index = groups[i].current_member_index();

            for g in groups.iter_mut() {
                if g.current_member_index() != sender_index {
                    let decrypted = g
                        .process_incoming_message(ciphertext.clone())
                        .await
                        .unwrap();

                    assert_matches!(decrypted, ReceivedMessage::ApplicationMessage(m) if m.data() == test_message);
                }
            }
        }
    }
}

#[cfg(all(feature = "private_message", feature = "out_of_order"))]
#[maybe_async::test(not(mls_build_async), async(mls_build_async, futures_test))]
async fn test_out_of_order_application_messages() {
    let mut groups =
        get_test_groups(ProtocolVersion::MLS_10, CipherSuite::P256_AES128, 2, false).await;

    let mut alice_group = groups[0].clone();
    let bob_group = &mut groups[1];

    let ciphertext = alice_group
        .encrypt_application_message(&[0], Vec::new())
        .await
        .unwrap();

    let mut ciphertexts = vec![ciphertext];

    ciphertexts.push(
        alice_group
            .encrypt_application_message(&[1], Vec::new())
            .await
            .unwrap(),
    );

    let commit = alice_group.commit(Vec::new()).await.unwrap().commit_message;

    alice_group.apply_pending_commit().await.unwrap();

    bob_group.process_incoming_message(commit).await.unwrap();

    ciphertexts.push(
        alice_group
            .encrypt_application_message(&[2], Vec::new())
            .await
            .unwrap(),
    );

    ciphertexts.push(
        alice_group
            .encrypt_application_message(&[3], Vec::new())
            .await
            .unwrap(),
    );

    for i in [3, 2, 1, 0] {
        let res = bob_group
            .process_incoming_message(ciphertexts[i].clone())
            .await
            .unwrap();

        assert_matches!(
            res,
            ReceivedMessage::ApplicationMessage(m) if m.data() == [i as u8]
        );
    }
}

#[cfg(feature = "private_message")]
#[maybe_async::test(not(mls_build_async), async(mls_build_async, futures_test))]
async fn test_group_application_messages() {
    test_on_all_params(test_application_messages).await
}

#[cfg(feature = "private_message")]
#[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
async fn processing_message_from_self_returns_error(
    protocol_version: ProtocolVersion,
    cipher_suite: CipherSuite,
    _n_participants: usize,
    encrypt_controls: bool,
) {
    let mut creator_group =
        get_test_groups(protocol_version, cipher_suite, 1, encrypt_controls).await;
    let creator_group = &mut creator_group[0];

    let msg = creator_group
        .encrypt_application_message(b"hello self", vec![])
        .await
        .unwrap();

    let error = creator_group
        .process_incoming_message(msg)
        .await
        .unwrap_err();

    assert_matches!(error, MlsError::CantProcessMessageFromSelf);
}

#[cfg(feature = "private_message")]
#[maybe_async::test(not(mls_build_async), async(mls_build_async, futures_test))]
async fn test_processing_message_from_self_returns_error() {
    test_on_all_params(processing_message_from_self_returns_error).await;
}

#[cfg(feature = "by_ref_proposal")]
#[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
async fn external_commits_work(
    protocol_version: ProtocolVersion,
    cipher_suite: CipherSuite,
    _n_participants: usize,
    _encrypt_controls: bool,
) {
    let creator = generate_client(cipher_suite, protocol_version, 0, false).await;

    let creator_group = creator
        .create_group_with_id(b"group".to_vec(), ExtensionList::default())
        .await
        .unwrap();

    const PARTICIPANT_COUNT: usize = 10;

    let mut others = Vec::new();

    for i in 1..PARTICIPANT_COUNT {
        others.push(generate_client(cipher_suite, protocol_version, i, Default::default()).await)
    }

    let mut groups = vec![creator_group];

    for client in &others {
        let existing_group = groups.choose_mut(&mut rand::thread_rng()).unwrap();

        let group_info = existing_group
            .group_info_message_allowing_ext_commit(true)
            .await
            .unwrap();

        let (new_group, commit) = client
            .external_commit_builder()
            .unwrap()
            .build(group_info)
            .await
            .unwrap();

        for group in groups.iter_mut() {
            group
                .process_incoming_message(commit.clone())
                .await
                .unwrap();
        }

        groups.push(new_group);
    }

    assert!(groups
        .iter()
        .all(|group| group.roster().members_iter().count() == PARTICIPANT_COUNT));

    for i in 0..groups.len() {
        let message = groups[i].propose_remove(0, Vec::new()).await.unwrap();

        for (_, group) in groups.iter_mut().enumerate().filter(|&(j, _)| i != j) {
            let processed = group
                .process_incoming_message(message.clone())
                .await
                .unwrap();

            if let ReceivedMessage::Proposal(p) = &processed {
                if let Proposal::Remove(r) = &p.proposal {
                    if r.to_remove() == 0 {
                        continue;
                    }
                }
            }

            panic!("expected a proposal, got {processed:?}");
        }
    }
}

#[cfg(feature = "by_ref_proposal")]
#[maybe_async::test(not(mls_build_async), async(mls_build_async, futures_test))]
async fn test_external_commits() {
    test_on_all_params_plaintext(external_commits_work).await
}

#[maybe_async::test(not(mls_build_async), async(mls_build_async, futures_test))]
async fn test_remove_nonexisting_leaf() {
    let mut groups =
        get_test_groups(ProtocolVersion::MLS_10, CipherSuite::P256_AES128, 10, false).await;

    groups[0]
        .commit_builder()
        .remove_member(5)
        .unwrap()
        .build()
        .await
        .unwrap();
    groups[0].apply_pending_commit().await.unwrap();

    // Leaf index out of bounds
    assert!(groups[0].commit_builder().remove_member(13).is_err());

    // Removing blank leaf causes error
    assert!(groups[0].commit_builder().remove_member(5).is_err());
}

#[cfg(feature = "psk")]
#[maybe_async::test(not(mls_build_async), async(mls_build_async, futures_test))]
async fn reinit_works() {
    let suite1 = CipherSuite::P256_AES128;

    let Some(suite2) = CipherSuite::all()
        .find(|cs| cs != &suite1 && TestCryptoProvider::all_supported_cipher_suites().contains(cs))
    else {
        return;
    };

    let version = ProtocolVersion::MLS_10;

    let alice1 = generate_client(suite1, version, 1, Default::default()).await;
    let bob1 = generate_client(suite1, version, 2, Default::default()).await;

    // Create a group with 2 parties
    let mut alice_group = alice1.create_group(ExtensionList::new()).await.unwrap();
    let kp = bob1.generate_key_package_message().await.unwrap();

    let welcome = &alice_group
        .commit_builder()
        .add_member(kp)
        .unwrap()
        .build()
        .await
        .unwrap()
        .welcome_messages[0];

    alice_group.apply_pending_commit().await.unwrap();

    let (mut bob_group, _) = bob1.join_group(None, welcome).await.unwrap();

    // Alice proposes reinit
    let reinit_proposal_message = alice_group
        .propose_reinit(
            None,
            ProtocolVersion::MLS_10,
            suite2,
            ExtensionList::default(),
            Vec::new(),
        )
        .await
        .unwrap();

    // Bob commits the reinit
    bob_group
        .process_incoming_message(reinit_proposal_message)
        .await
        .unwrap();

    let commit = bob_group.commit(Vec::new()).await.unwrap().commit_message;

    // Both process Bob's commit

    #[cfg(feature = "state_update")]
    {
        let state_update = bob_group.apply_pending_commit().await.unwrap().state_update;
        assert!(!state_update.is_active() && state_update.is_pending_reinit());
    }

    #[cfg(not(feature = "state_update"))]
    bob_group.apply_pending_commit().await.unwrap();

    let message = alice_group.process_incoming_message(commit).await.unwrap();

    #[cfg(feature = "state_update")]
    if let ReceivedMessage::Commit(commit_description) = message {
        assert!(
            !commit_description.state_update.is_active()
                && commit_description.state_update.is_pending_reinit()
        );
    }

    #[cfg(not(feature = "state_update"))]
    assert_matches!(message, ReceivedMessage::Commit(_));

    // They can't create new epochs anymore
    let res = alice_group.commit(Vec::new()).await;
    assert!(res.is_err());

    let res = bob_group.commit(Vec::new()).await;
    assert!(res.is_err());

    // Get reinit clients for alice and bob
    let (secret_key, public_key) = TestCryptoProvider::new()
        .cipher_suite_provider(suite2)
        .unwrap()
        .signature_key_generate()
        .await
        .unwrap();

    let identity = SigningIdentity::new(get_test_basic_credential(b"bob".to_vec()), public_key);

    let bob2 = bob_group
        .get_reinit_client(Some(secret_key), Some(identity))
        .unwrap();

    let (secret_key, public_key) = TestCryptoProvider::new()
        .cipher_suite_provider(suite2)
        .unwrap()
        .signature_key_generate()
        .await
        .unwrap();

    let identity = SigningIdentity::new(get_test_basic_credential(b"alice".to_vec()), public_key);

    let alice2 = alice_group
        .get_reinit_client(Some(secret_key), Some(identity))
        .unwrap();

    // Bob produces key package, alice commits, bob joins
    let kp = bob2.generate_key_package().await.unwrap();
    let (mut alice_group, welcome) = alice2.commit(vec![kp]).await.unwrap();
    let (mut bob_group, _) = bob2.join(&welcome[0], None).await.unwrap();

    assert!(bob_group.cipher_suite() == suite2);

    // They can talk
    let carol = generate_client(suite2, version, 3, Default::default()).await;

    let kp = carol.generate_key_package_message().await.unwrap();

    let commit_output = alice_group
        .commit_builder()
        .add_member(kp)
        .unwrap()
        .build()
        .await
        .unwrap();

    alice_group.apply_pending_commit().await.unwrap();

    bob_group
        .process_incoming_message(commit_output.commit_message)
        .await
        .unwrap();

    carol
        .join_group(None, &commit_output.welcome_messages[0])
        .await
        .unwrap();
}

#[cfg(feature = "by_ref_proposal")]
#[maybe_async::test(not(mls_build_async), async(mls_build_async, futures_test))]
async fn external_joiner_can_process_siblings_update() {
    let mut groups =
        get_test_groups(ProtocolVersion::MLS_10, CipherSuite::P256_AES128, 3, false).await;

    // Remove leaf 1 s.t. the external joiner joins in its place
    let c = groups[0]
        .commit_builder()
        .remove_member(1)
        .unwrap()
        .build()
        .await
        .unwrap();

    all_process_message(&mut groups, &c.commit_message, 0, true).await;

    let info = groups[0]
        .group_info_message_allowing_ext_commit(true)
        .await
        .unwrap();

    // Create the external joiner and join
    let new_client = generate_client(
        CipherSuite::P256_AES128,
        ProtocolVersion::MLS_10,
        0xabba,
        false,
    )
    .await;

    let (mut group, commit) = new_client.commit_external(info).await.unwrap();

    all_process_message(&mut groups, &commit, 1, false).await;
    groups.remove(1);

    // New client's sibling proposes an update to blank their common parent
    let p = groups[0].propose_update(Vec::new()).await.unwrap();
    all_process_message(&mut groups, &p, 0, false).await;
    group.process_incoming_message(p).await.unwrap();

    // Some other member commits
    let c = groups[1].commit(Vec::new()).await.unwrap().commit_message;
    all_process_message(&mut groups, &c, 2, true).await;
    group.process_incoming_message(c).await.unwrap();
}

#[maybe_async::test(not(mls_build_async), async(mls_build_async, futures_test))]
async fn weird_tree_scenario() {
    let mut groups =
        get_test_groups(ProtocolVersion::MLS_10, CipherSuite::P256_AES128, 17, false).await;

    let to_remove = [0u32, 2, 5, 7, 8, 9, 15];

    let mut builder = groups[14].commit_builder();

    for idx in to_remove.iter() {
        builder = builder.remove_member(*idx).unwrap();
    }

    let commit = builder.build().await.unwrap();

    for idx in to_remove.into_iter().rev() {
        groups.remove(idx as usize);
    }

    all_process_message(&mut groups, &commit.commit_message, 14, true).await;

    let mut builder = groups.last_mut().unwrap().commit_builder();

    for idx in 0..7 {
        builder = builder
            .add_member(fake_key_package(5555555 + idx).await)
            .unwrap()
    }

    let commit = builder.remove_member(1).unwrap().build().await.unwrap();

    let idx = groups.last().unwrap().current_member_index() as usize;

    all_process_message(&mut groups, &commit.commit_message, idx, true).await;
}

#[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
async fn fake_key_package(id: usize) -> MlsMessage {
    generate_client(CipherSuite::P256_AES128, ProtocolVersion::MLS_10, id, false)
        .await
        .generate_key_package_message()
        .await
        .unwrap()
}

#[maybe_async::test(not(mls_build_async), async(mls_build_async, futures_test))]
async fn external_info_from_commit_allows_to_join() {
    let cs = CipherSuite::P256_AES128;
    let version = ProtocolVersion::MLS_10;

    let mut alice = mls_rs::test_utils::get_test_groups(
        version,
        cs,
        1,
        Some(CommitOptions::new().with_allow_external_commit(true)),
        false,
        &TestCryptoProvider::default(),
    )
    .await
    .remove(0);

    let commit = alice.commit(vec![]).await.unwrap();
    alice.apply_pending_commit().await.unwrap();
    let bob = generate_client(cs, version, 0xdead, false).await;

    let (_bob, commit) = bob
        .commit_external(commit.external_commit_group_info.unwrap())
        .await
        .unwrap();

    alice.process_incoming_message(commit).await.unwrap();
}
