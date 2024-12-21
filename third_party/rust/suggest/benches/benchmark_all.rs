/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

use criterion::{
    criterion_group, criterion_main, measurement::Measurement, BatchSize, BenchmarkGroup, Criterion,
};
use std::sync::Once;
use suggest::benchmarks::{geoname, ingest, query, BenchmarkWithInput};

pub fn geoname(c: &mut Criterion) {
    setup_viaduct();
    let group = c.benchmark_group("geoname");
    run_benchmarks(group, geoname::all_benchmarks())
}

pub fn ingest(c: &mut Criterion) {
    setup_viaduct();
    let mut group = c.benchmark_group("ingest");
    // This needs to be 10 for now, or else the `ingest-amp-wikipedia` benchmark would take around
    // 100s to run which feels like too long.  `ingest-amp-mobile` also would take a around 50s.
    group.sample_size(10);
    run_benchmarks(group, ingest::all_benchmarks())
}

pub fn query(c: &mut Criterion) {
    setup_viaduct();
    let group = c.benchmark_group("query");
    run_benchmarks(group, query::all_benchmarks())
}

fn run_benchmarks<B: BenchmarkWithInput, M: Measurement>(
    mut group: BenchmarkGroup<M>,
    benchmarks: Vec<(&'static str, B)>,
) {
    for (name, benchmark) in benchmarks {
        let g_input = benchmark.global_input();
        group.bench_function(name.to_string(), |b| {
            b.iter_batched(
                || benchmark.iteration_input(),
                |i_input| benchmark.benchmarked_code(&g_input, i_input),
                // See https://docs.rs/criterion/latest/criterion/enum.BatchSize.html#variants for
                // a discussion of this.  PerIteration is chosen for these benchmarks because the
                // input holds a database file handle
                BatchSize::PerIteration,
            );
        });
    }
    group.finish();
}

fn setup_viaduct() {
    static INIT: Once = Once::new();
    INIT.call_once(viaduct_reqwest::use_reqwest_backend);
}

criterion_group!(benches, geoname, ingest, query);
criterion_main!(benches);
