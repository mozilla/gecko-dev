// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use criterion::{BatchSize, BenchmarkId, Criterion, Throughput};
use mls_rs::test_utils::benchmarks::load_group_states;
use mls_rs::CipherSuite;
use rand::RngCore;

fn bench(c: &mut Criterion) {
    let cipher_suite = CipherSuite::CURVE25519_AES128;
    let group_states = load_group_states(cipher_suite).pop().unwrap();

    let mut bytes = vec![0; 1000000];
    rand::thread_rng().fill_bytes(&mut bytes);

    let bytes = &bytes;
    let mut n = 100;
    let mut bench_group = c.benchmark_group("group_application");

    while n <= 1000000 {
        bench_group.throughput(Throughput::Bytes(n as u64));
        bench_group.bench_with_input(
            BenchmarkId::new(format!("{cipher_suite:?}"), n),
            &n,
            |b, _| {
                b.iter_batched_ref(
                    || group_states.clone(),
                    move |group_states| {
                        let msg = group_states
                            .sender
                            .encrypt_application_message(&bytes[..n], vec![])
                            .unwrap();

                        group_states.receiver.process_incoming_message(msg).unwrap();
                    },
                    BatchSize::SmallInput,
                )
            },
        );

        n *= 10;
    }
    bench_group.finish();
}

criterion::criterion_group!(benches, bench);
criterion::criterion_main!(benches);
