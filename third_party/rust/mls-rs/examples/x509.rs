// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use mls_rs::{CipherSuite, Client};
use mls_rs_crypto_openssl::OpensslCryptoProvider;

const CIPHERSUITE: CipherSuite = CipherSuite::CURVE25519_AES128;

fn main() {
    let crypto_provider = OpensslCryptoProvider::new();

    let secret_key = mls_rs_crypto_openssl::x509::signature_secret_key_from_bytes(include_bytes!(
        "../../mls-rs-crypto-openssl/test_data/x509/leaf/key.pem"
    ))
    .unwrap();

    let signing_identity = mls_rs_crypto_openssl::x509::signing_identity_from_certificate(
        include_bytes!("../../mls-rs-crypto-openssl/test_data/x509/leaf/cert.der"),
    )
    .unwrap();

    let alice_client = Client::builder()
        .crypto_provider(crypto_provider)
        .identity_provider(
            mls_rs_crypto_openssl::x509::identity_provider_from_certificate(include_bytes!(
                "../../mls-rs-crypto-openssl/test_data/x509/root_ca/cert.der"
            ))
            .unwrap(),
        )
        .signing_identity(signing_identity, secret_key, CIPHERSUITE)
        .build();

    let mut alice_group = alice_client.create_group(Default::default()).unwrap();

    alice_group.commit(Vec::new()).unwrap();
    alice_group.apply_pending_commit().unwrap();
}
