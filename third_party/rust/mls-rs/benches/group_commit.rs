// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use criterion::{BatchSize, BenchmarkId, Criterion};
use mls_rs::{test_utils::benchmarks::load_group_states, CipherSuite};

fn bench(c: &mut Criterion) {
    let cipher_suite = CipherSuite::CURVE25519_AES128;
    let group_states = load_group_states(cipher_suite);
    let mut bench_group = c.benchmark_group("group_commit");

    for (i, group_states) in group_states.into_iter().enumerate() {
        bench_group.bench_with_input(
            BenchmarkId::new(format!("{cipher_suite:?}"), i),
            &i,
            |b, _| {
                b.iter_batched_ref(
                    || group_states.sender.clone(),
                    move |sender| sender.commit(vec![]).unwrap(),
                    BatchSize::SmallInput,
                )
            },
        );
    }
}

criterion::criterion_group!(benches, bench);
criterion::criterion_main!(benches);
