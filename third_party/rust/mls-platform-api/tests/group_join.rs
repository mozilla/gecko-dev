// Copyright (c) 2024 Mozilla Corporation and contributors.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

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

#[test]
fn test_group_join() -> Result<(), PlatformError> {
    // Default group configuration
    let group_config = mls_platform_api::GroupConfig::default();

    // Storage states
    let mut state_global = mls_platform_api::state_access("global.db", &[0u8; 32])?;

    // Credentials
    let alice_cred = mls_platform_api::mls_generate_credential_basic("alice".as_bytes())?;
    let bob_cred = mls_platform_api::mls_generate_credential_basic("bob".as_bytes())?;

    println!("\nAlice credential: {}", hex::encode(&alice_cred));
    println!("Bob credential: {}", hex::encode(&bob_cred));

    // Create signature keypairs and store them in the state
    let alice_id =
        mls_platform_api::mls_generate_identity(&state_global, group_config.ciphersuite)?;

    let bob_id = mls_platform_api::mls_generate_identity(&state_global, group_config.ciphersuite)?;

    println!("\nAlice identifier: {}", hex::encode(&alice_id));
    println!("Bob identifier: {}", hex::encode(&bob_id));

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
    let members = mls_platform_api::mls_group_details(&state_global, &gide.group_id, &alice_id)?;
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
    println!("\nAlice process her commit to add Bob to the Group");
    mls_platform_api::mls_receive(
        &state_global,
        &alice_id,
        &MessageOrAck::MlsMessage(commit_output.commit.clone()),
    )?;

    // List the members of the group
    let members = mls_platform_api::mls_group_details(&state_global, &gide.group_id, &alice_id)?;
    println!("Members (alice, after adding bob): {members:?}");

    // Bob joins
    println!("\nBob joins the group created by Alice");
    let gide_2 = mls_platform_api::mls_group_join(&state_global, &bob_id, &welcome, None)?;

    // List the members of the group
    let members_2 = mls_platform_api::mls_group_details(&state_global, &gide.group_id, &bob_id)?;
    println!("Members (bob, after joining the group): {members_2:?}");

    // Assert that the group identifier is the same for Alice and Bob
    assert!(gide.group_id == gide_2.group_id);

    // Assert that the membership is the same for Alice and Bob
    assert!(members == members_2);
    Ok(())
}

#[test]
fn test_group_join_multiple_adds() -> Result<(), PlatformError> {
    // Default group configuration
    let group_config = mls_platform_api::GroupConfig::default();

    // Storage states
    let mut state_global = mls_platform_api::state_access("global.db", &[0u8; 32])?;

    // Credentials
    let alice_cred = mls_platform_api::mls_generate_credential_basic("alice".as_bytes())?;
    let bob_cred = mls_platform_api::mls_generate_credential_basic("bob".as_bytes())?;
    let charlie_cred = mls_platform_api::mls_generate_credential_basic("charlie".as_bytes())?;

    println!("\nAlice credential: {}", hex::encode(&alice_cred));
    println!("Bob credential: {}", hex::encode(&bob_cred));
    println!("Charlie credential: {}", hex::encode(&charlie_cred));

    // Create signature keypairs and store them in the state
    let alice_id =
        mls_platform_api::mls_generate_identity(&state_global, group_config.ciphersuite)?;

    let bob_id = mls_platform_api::mls_generate_identity(&state_global, group_config.ciphersuite)?;
    let charlie_id =
        mls_platform_api::mls_generate_identity(&state_global, group_config.ciphersuite)?;

    println!("\nAlice identifier: {}", hex::encode(&alice_id));
    println!("Bob identifier: {}", hex::encode(&bob_id));
    println!("Charlie identifier: {}", hex::encode(&charlie_id));

    // Create Key Package for Bob
    let bob_kp = mls_platform_api::mls_generate_key_package(
        &state_global,
        &bob_id,
        &bob_cred,
        &Default::default(),
    )?;

    // Create Key Package for Charlie
    let charlie_kp = mls_platform_api::mls_generate_key_package(
        &state_global,
        &charlie_id,
        &charlie_cred,
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
    let members = mls_platform_api::mls_group_details(&state_global, &gide.group_id, &alice_id)?;
    println!("Members (alice, before adding bob and charlie): {members:?}");

    //
    // Alice adds Bob and Charlie to a group
    //
    println!("\nAlice adds Bob and Charlie to the Group");
    let commit_output = mls_platform_api::mls_group_add(
        &mut state_global,
        &gide.group_id,
        &alice_id,
        vec![bob_kp, charlie_kp],
    )?;

    let welcome = commit_output
        .welcome
        .first()
        .expect("No welcome messages found")
        .clone();

    // Alice process her own commit
    println!("\nAlice process her commit to add Bob and Charlie to the Group");
    mls_platform_api::mls_receive(
        &state_global,
        &alice_id,
        &MessageOrAck::MlsMessage(commit_output.commit.clone()),
    )?;

    // List the members of the group from Alice's perspective
    let members = mls_platform_api::mls_group_details(&state_global, &gide.group_id, &alice_id)?;
    println!("Members (alice, after adding bob and charlie): {members:?}");

    // Bob joins
    println!("\nBob joins the group created by Alice");
    let gide_2 = mls_platform_api::mls_group_join(&state_global, &bob_id, &welcome, None)?;

    // Charlie joins
    println!("\nCharlie joins the group created by Alice");
    let gide_3 = mls_platform_api::mls_group_join(&state_global, &charlie_id, &welcome, None)?;

    // List the members of the group from Bob's perspective
    let members_2 = mls_platform_api::mls_group_details(&state_global, &gide.group_id, &bob_id)?;
    println!("Members (bob, after joining the group): {members_2:?}");

    // List the members of the group from Charlie's perspective
    let members_3 =
        mls_platform_api::mls_group_details(&state_global, &gide.group_id, &charlie_id)?;
    println!("Members (charlie, after joining the group): {members_3:?}");

    // Assert that the group identifier is the same for Alice, Bob and Charlie
    assert!(gide.group_id == gide_2.group_id);
    assert!(gide.group_id == gide_3.group_id);

    // Assert that the membership is the same for Alice, Bob and Charlie
    assert!(members == members_2);
    assert!(members == members_3);

    Ok(())
}
