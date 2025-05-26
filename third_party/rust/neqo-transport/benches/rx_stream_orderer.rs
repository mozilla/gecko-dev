// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::{hint::black_box, time::Duration};

use criterion::{criterion_group, criterion_main, Criterion};
use neqo_transport::recv_stream::RxStreamOrderer;

fn rx_stream_orderer() {
    let mut rx = RxStreamOrderer::new();
    let data: &[u8] = &[0; 1337];

    for i in 0..100_000 {
        rx.inbound_frame(i * 1337, data);
    }
}

fn criterion_benchmark(c: &mut Criterion) {
    c.bench_function("RxStreamOrderer::inbound_frame()", |b| {
        b.iter(black_box(rx_stream_orderer));
    });
}

criterion_group! {
    name = benches;
    config = Criterion::default().warm_up_time(Duration::from_secs(5)).measurement_time(Duration::from_secs(60));
    targets = criterion_benchmark
}
criterion_main!(benches);
