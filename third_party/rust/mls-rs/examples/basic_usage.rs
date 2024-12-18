// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use mls_rs::{
    client_builder::MlsConfig,
    error::MlsError,
    identity::{
        basic::{BasicCredential, BasicIdentityProvider},
        SigningIdentity,
    },
    CipherSuite, CipherSuiteProvider, Client, CryptoProvider, ExtensionList,
};

const CIPHERSUITE: CipherSuite = CipherSuite::CURVE25519_AES128;

fn make_client<P: CryptoProvider + Clone>(
    crypto_provider: P,
    name: &str,
) -> Result<Client<impl MlsConfig>, MlsError> {
    let cipher_suite = crypto_provider.cipher_suite_provider(CIPHERSUITE).unwrap();

    // Generate a signature key pair.
    let (secret, public) = cipher_suite.signature_key_generate().unwrap();

    // Create a basic credential for the session.
    // NOTE: BasicCredential is for demonstration purposes and not recommended for production.
    // X.509 credentials are recommended.
    let basic_identity = BasicCredential::new(name.as_bytes().to_vec());
    let signing_identity = SigningIdentity::new(basic_identity.into_credential(), public);

    Ok(Client::builder()
        .identity_provider(BasicIdentityProvider)
        .crypto_provider(crypto_provider)
        .signing_identity(signing_identity, secret, CIPHERSUITE)
        .build())
}

fn main() -> Result<(), MlsError> {
    let crypto_provider = mls_rs_crypto_openssl::OpensslCryptoProvider::default();

    // Create clients for Alice and Bob
    let alice = make_client(crypto_provider.clone(), "alice")?;
    let bob = make_client(crypto_provider.clone(), "bob")?;

    // Alice creates a new group.
    let mut alice_group = alice.create_group(ExtensionList::default())?;

    // Bob generates a key package that Alice needs to add Bob to the group.
    let bob_key_package = bob.generate_key_package_message()?;

    // Alice issues a commit that adds Bob to the group.
    let alice_commit = alice_group
        .commit_builder()
        .add_member(bob_key_package)?
        .build()?;

    // Alice confirms that the commit was accepted by the group so it can be applied locally.
    // This would normally happen after a server confirmed your commit was accepted and can
    // be broadcasted.
    alice_group.apply_pending_commit()?;

    // Bob joins the group with the welcome message created as part of Alice's commit.
    let (mut bob_group, _) = bob.join_group(None, &alice_commit.welcome_messages[0])?;

    // Alice encrypts an application message to Bob.
    let msg = alice_group.encrypt_application_message(b"hello world", Default::default())?;

    // Bob decrypts the application message from Alice.
    let msg = bob_group.process_incoming_message(msg)?;

    println!("Received message: {:?}", msg);

    // Alice and bob write the group state to their configured storage engine
    alice_group.write_to_storage()?;
    bob_group.write_to_storage()?;

    Ok(())
}
