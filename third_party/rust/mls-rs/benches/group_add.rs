// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use criterion::{BatchSize, BenchmarkId, Criterion};
use mls_rs::{
    client_builder::MlsConfig,
    identity::{
        basic::{BasicCredential, BasicIdentityProvider},
        SigningIdentity,
    },
    mls_rules::{CommitOptions, DefaultMlsRules},
    CipherSuite, CipherSuiteProvider, Client, CryptoProvider,
};
use mls_rs_crypto_openssl::OpensslCryptoProvider;

fn bench(c: &mut Criterion) {
    let alice = make_client("alice")
        .create_group(Default::default())
        .unwrap();

    const MAX_ADD_COUNT: usize = 1000;

    let key_packages = (0..MAX_ADD_COUNT)
        .map(|i| {
            make_client(&format!("bob-{i}"))
                .generate_key_package_message()
                .unwrap()
        })
        .collect::<Vec<_>>();

    let mut group = c.benchmark_group("group_add");

    std::iter::successors(Some(1), |&i| Some(i * 10))
        .take_while(|&i| i <= MAX_ADD_COUNT)
        .for_each(|size| {
            group.bench_with_input(BenchmarkId::from_parameter(size), &size, |b, &size| {
                b.iter_batched_ref(
                    || alice.clone(),
                    |alice| {
                        key_packages[..size]
                            .iter()
                            .cloned()
                            .fold(alice.commit_builder(), |builder, key_package| {
                                builder.add_member(key_package).unwrap()
                            })
                            .build()
                            .unwrap();
                    },
                    BatchSize::SmallInput,
                );
            });
        });

    group.finish();
}

criterion::criterion_group!(benches, bench);
criterion::criterion_main!(benches);

fn make_client(name: &str) -> Client<impl MlsConfig> {
    let crypto_provider = OpensslCryptoProvider::new();
    let cipher_suite = CipherSuite::CURVE25519_AES128;

    let (secret_key, public_key) = crypto_provider
        .cipher_suite_provider(cipher_suite)
        .unwrap()
        .signature_key_generate()
        .unwrap();

    Client::builder()
        .crypto_provider(crypto_provider)
        .identity_provider(BasicIdentityProvider)
        .mls_rules(
            DefaultMlsRules::new()
                .with_commit_options(CommitOptions::new().with_ratchet_tree_extension(false)),
        )
        .signing_identity(
            SigningIdentity::new(
                BasicCredential::new(name.as_bytes().to_vec()).into_credential(),
                public_key,
            ),
            secret_key,
            cipher_suite,
        )
        .build()
}
