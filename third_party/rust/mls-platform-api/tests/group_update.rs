// Copyright (c) 2024 Mozilla Corporation and contributors.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use mls_platform_api::ClientConfig;
use mls_platform_api::ClientIdentifiers;
use mls_platform_api::MessageOrAck;
use mls_platform_api::PlatformError;

//
// Scenario
//
// * Alice, Bob, Charlie and Diana create signing identity (generate_signature_keypair)
// * Alice, Bob, Charlie and Diana create credentials (generate_credential_basic)
// * Bob and Charlie create key packages (generate_key_package)
// * Alice creates a group (group_create)
// * Alice adds Bob to the group (group_add)
//   - Alice receives her add commit (receive for commit)
//   - Bob joins the group (group_join)
// * Bob sends an application message (send)
//   - Alice receives the application message (receive for application message)
// * Bob produces group update with group info (group_update with external join)
//   - Bob receives his update commit
//   - Alice receives his update commit
// * Diana sends a commit to do an external join (group_update for external join)
//   - Alice receives the commit
//   - Bob receives the commit

#[test]
fn test_group_external_join() -> Result<(), PlatformError> {
    // Default group configuration
    let group_config = mls_platform_api::GroupConfig::default();

    // Storage states
    let mut state_global = mls_platform_api::state_access("global.db", &[0u8; 32])?;

    // Credentials
    let alice_cred = mls_platform_api::mls_generate_credential_basic("alice".as_bytes())?;
    let bob_cred = mls_platform_api::mls_generate_credential_basic("bob".as_bytes())?;
    let diana_cred = mls_platform_api::mls_generate_credential_basic("diana".as_bytes())?;

    println!("\nAlice credential: {}", hex::encode(&alice_cred));
    println!("Bob credential: {}", hex::encode(&bob_cred));
    println!("Diana credential: {}", hex::encode(&diana_cred));

    // Create signature keypairs and store them in the state
    let alice_id =
        mls_platform_api::mls_generate_signature_keypair(&state_global, group_config.ciphersuite)?;

    let bob_id =
        mls_platform_api::mls_generate_signature_keypair(&state_global, group_config.ciphersuite)?;

    let diana_id =
        mls_platform_api::mls_generate_signature_keypair(&state_global, group_config.ciphersuite)?;

    println!("\nAlice identifier: {}", hex::encode(&alice_id));
    println!("Bob identifier: {}", hex::encode(&bob_id));
    println!("Diana identifier: {}", hex::encode(&diana_id));

    // Create Key Package for Bob
    let bob_kp = mls_platform_api::mls_generate_key_package(
        &state_global,
        &bob_id,
        &bob_cred,
        &Default::default(),
    )?;

    // Create a group with Alice
    let gide = mls_platform_api::mls_group_create(
        &mut state_global,
        &alice_id,
        &alice_cred,
        None,
        None,
        &Default::default(),
    )?;

    println!("\nGroup created by Alice: {}", hex::encode(&gide.group_id));

    // List the members of the group
    let members = mls_platform_api::mls_group_members(&state_global, &gide.group_id, &alice_id)?;
    println!("Members (alice, before adding bob): {members:?}");

    //
    // Alice adds Bob to a group
    //
    println!("\nAlice adds Bob to the Group");
    let commit_output = mls_platform_api::mls_group_add(
        &mut state_global,
        &gide.group_id,
        &alice_id,
        vec![bob_kp],
    )?;

    let welcome = commit_output
        .welcome
        .first()
        .expect("No welcome messages found")
        .clone();

    // Alice process her own commit
    // println!("\nAlice process her commit to add Bob to the Group");
    mls_platform_api::mls_receive(
        &state_global,
        &alice_id,
        &MessageOrAck::MlsMessage(commit_output.commit.clone()),
    )?;

    // List the members of the group
    let members = mls_platform_api::mls_group_members(&state_global, &gide.group_id, &alice_id)?;
    println!("Members (alice, after adding bob): {members:?}");

    // Bob joins
    println!("\nBob joins the group created by Alice");
    mls_platform_api::mls_group_join(&state_global, &bob_id, &welcome, None)?;

    // List the members of the group
    let members = mls_platform_api::mls_group_members(&state_global, &gide.group_id, &bob_id)?;
    println!("Members (bob, after joining the group): {members:?}");

    //
    // Bob produces group info to allow an external join from Diana
    //
    let client_config = ClientConfig {
        allow_external_commits: true,
        ..Default::default()
    };

    println!("\nBob produce a group info so that someone can do an External join");
    let commit_4_output = mls_platform_api::mls_group_update(
        &mut state_global,
        &gide.group_id,
        &bob_id,
        None,
        None,
        None,
        &client_config,
    )?;

    // Alice receives Bob's commit
    mls_platform_api::mls_receive(
        &state_global,
        &alice_id,
        &MessageOrAck::MlsMessage(commit_4_output.commit.clone()),
    )?;

    let members_alice =
        mls_platform_api::mls_group_members(&state_global, &gide.group_id, &alice_id)?;
    println!(
        "Members (alice, after receiving the commit allowing external join): {members_alice:?}"
    );

    // Bob receives own commit
    mls_platform_api::mls_receive(
        &state_global,
        &bob_id,
        &MessageOrAck::MlsMessage(commit_4_output.commit.clone()),
    )?;

    let members_bob = mls_platform_api::mls_group_members(&state_global, &gide.group_id, &bob_id)?;
    println!("Members (bob, after commit allowing external join): {members_bob:?}");

    //
    // Diana joins the group with an external commit
    //
    println!("\nDiana uses the group info created by Bob to do an External join");
    let external_commit_output = mls_platform_api::mls_group_external_commit(
        &state_global,
        &diana_id,
        &diana_cred,
        &commit_4_output
            .group_info
            .expect("alice should produce group info"),
        // use tree in extension for now
        None,
    )?;

    println!("Externally joined group {:?}", &external_commit_output.gid);

    let members_diana =
        mls_platform_api::mls_group_members(&state_global, &gide.group_id, &diana_id)?;
    println!("Members (diane, after joining): {members_diana:?}");

    // Alice receives Diana's commit
    println!("\nAlice receives the External Join from Diana");
    mls_platform_api::mls_receive(
        &state_global,
        &alice_id,
        &MessageOrAck::MlsMessage(external_commit_output.external_commit.clone()),
    )?;

    let members_alice =
        mls_platform_api::mls_group_members(&state_global, &gide.group_id, &alice_id)?;
    println!("Members (alice, after receiving the commit from Diana): {members_alice:?}");

    // Bob receives Diana's commit
    println!("\nBob receives the External Join from Diana");
    mls_platform_api::mls_receive(
        &state_global,
        &bob_id,
        &MessageOrAck::MlsMessage(external_commit_output.external_commit.clone()),
    )?;

    let members_bob = mls_platform_api::mls_group_members(&state_global, &gide.group_id, &bob_id)?;
    println!("Members (bob, after receiving the commit from Diana): {members_bob:?}");

    // Check if Diana is in the members list
    let diana_present = members_diana
        .group_members
        .iter()
        .any(|ClientIdentifiers { identity, .. }| identity == &diana_id);

    // Test that alice was removed from the group
    assert!(
        diana_present,
        "Diana should be in the group members after external join"
    );

    // Test that membership are all the same
    assert!(members_alice == members_bob);
    assert!(members_diana == members_bob);

    Ok(())
}
