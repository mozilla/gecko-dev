// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use criterion::{black_box, criterion_group, criterion_main, Criterion};
use icu_calendar::{DateTime, Iso};

fn overview_bench(c: &mut Criterion) {
    c.bench_function("iso/from_minutes_since_local_unix_epoch", |b| {
        b.iter(|| {
            for i in -250..250 {
                let minutes = i * 10000;
                DateTime::<Iso>::from_minutes_since_local_unix_epoch(black_box(minutes));
            }
        });
    });
}

criterion_group!(benches, overview_bench,);
criterion_main!(benches);
