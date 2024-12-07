// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use mls_rs::{test_utils::benchmarks::load_group_states, CipherSuite};

use criterion::{BenchmarkId, Criterion};

fn bench_serialize(c: &mut Criterion) {
    use criterion::BatchSize;

    let cs = CipherSuite::CURVE25519_AES128;
    let group_states = load_group_states(cs);
    let mut bench_group = c.benchmark_group("group_serialize");

    for (i, group_states) in group_states.into_iter().enumerate() {
        bench_group.bench_with_input(BenchmarkId::new(format!("{cs:?}"), i), &i, |b, _| {
            b.iter_batched_ref(
                || group_states.sender.clone(),
                move |sender| sender.write_to_storage().unwrap(),
                BatchSize::SmallInput,
            )
        });
    }

    bench_group.finish();
}

criterion::criterion_group!(benches, bench_serialize);
criterion::criterion_main!(benches);
