// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use criterion::{BatchSize, BenchmarkId, Criterion};
use mls_rs::test_utils::benchmarks::{load_group_states, BENCH_CIPHER_SUITE};

fn bench(c: &mut Criterion) {
    let group_states = load_group_states();

    let mut bench_group = c.benchmark_group("group_commit");

    for (i, group_states) in group_states.into_iter().enumerate() {
        bench_group.bench_with_input(
            BenchmarkId::new(format!("{BENCH_CIPHER_SUITE:?}"), i),
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
