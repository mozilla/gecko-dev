// Copyright (c) 2024 Mozilla Corporation and contributors.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use mls_platform_api::MessageOrAck;
use mls_platform_api::PlatformError;

//
// Scenario
//
// * Alice, Bob and Charlie create signing identity (generate_signature_keypair)
// * Alice, Bob and Charlie create credentials (generate_credential_basic)
// * Bob and Charlie create key packages (generate_key_package)
// * Alice creates a group (group_create)
// * Alice adds Bob to the group (group_add)
//   - Alice has a pending commit and cannot do another operation
// * Alice clears the pending commit
// * Alice can add Charlie to the group

#[test]
fn test_clear_pending_commit() -> Result<(), PlatformError> {
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
    println!("Members (alice, before adding bob): {members:?}");

    //
    // Alice adds Bob to a group
    //
    println!("\nAlice adds Bob to the Group");
    let _commit_output = mls_platform_api::mls_group_add(
        &mut state_global,
        &gide.group_id,
        &alice_id,
        vec![bob_kp],
    )?;

    // Check if there's a pending commit
    let pending =
        mls_platform_api::mls_has_pending_commit(&state_global, &gide.group_id, &alice_id)?;
    assert!(pending);

    // Try to add Charlie while there's a pending commit - should fail
    println!("\nAlice tries to add Charlie while there's a pending commit");
    let result = mls_platform_api::mls_group_add(
        &mut state_global,
        &gide.group_id,
        &alice_id,
        vec![charlie_kp.clone()],
    );

    // Verify we get an error
    assert!(result.is_err());
    println!("Got expected error when trying to add with pending commit: {result:?}");

    // Discard the pending commit
    println!("\nAlice discards the pending commit");
    mls_platform_api::mls_clear_pending_commit(&mut state_global, &gide.group_id, &alice_id)?;

    // Check if there's a pending commit again
    let pending =
        mls_platform_api::mls_has_pending_commit(&state_global, &gide.group_id, &alice_id)?;
    assert!(!pending);

    // Alice can now add Charlie to the group
    println!("\nAlice adds Charlie to the Group");
    let commit_output = mls_platform_api::mls_group_add(
        &mut state_global,
        &gide.group_id,
        &alice_id,
        vec![charlie_kp.clone()],
    )?;

    // Alice process her own commit
    println!("\nAlice process her commit to add Bob to the Group");
    mls_platform_api::mls_receive(
        &state_global,
        &alice_id,
        &MessageOrAck::MlsMessage(commit_output.commit),
    )?;

    // List the members of the group
    let members = mls_platform_api::mls_group_details(&state_global, &gide.group_id, &alice_id)?;
    println!("Members (alice, after adding charlie): {members:?}");

    Ok(())
}
