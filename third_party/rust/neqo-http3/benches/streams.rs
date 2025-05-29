// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![expect(clippy::unwrap_used, reason = "OK in a bench.")]

use std::{hint::black_box, iter::repeat_with, time::Duration};

use criterion::{criterion_group, criterion_main, BatchSize, Criterion};
use neqo_crypto::AuthenticationStatus;
use neqo_http3::{Http3Client, Http3Parameters, Http3Server, Priority};
use neqo_transport::{ConnectionParameters, StreamType};
use test_fixture::{
    fixture_init, http3_client_with_params, http3_server_with_params, now, DEFAULT_SERVER_NAME,
};

const STREAM_TYPE: StreamType = StreamType::BiDi;
const STREAMS_MAX: u64 = 1 << 60;

fn exchange_packets(client: &mut Http3Client, server: &mut Http3Server, is_handshake: bool) {
    let mut out = None;
    let mut auth_needed = is_handshake;
    loop {
        out = client.process(out, now()).dgram();
        let client_out_is_none = out.is_none();
        if auth_needed && client.peer_certificate().is_some() {
            client.authenticated(AuthenticationStatus::Ok, now());
            auth_needed = false;
        }
        out = server.process(out, now()).dgram();
        if client_out_is_none && out.is_none() {
            break;
        }
    }
}

fn use_streams(client: &mut Http3Client, server: &mut Http3Server, streams: usize, data: &[u8]) {
    let stream_ids = repeat_with(|| {
        client
            .fetch(
                now(),
                "GET",
                &("https", DEFAULT_SERVER_NAME, "/"),
                &[],
                Priority::default(),
            )
            .unwrap()
    })
    .take(streams)
    .collect::<Vec<_>>();
    exchange_packets(client, server, false);
    for stream_id in &stream_ids {
        client.send_data(*stream_id, data).unwrap();
    }
    exchange_packets(client, server, false);
    for stream_id in &stream_ids {
        client.stream_close_send(*stream_id).unwrap();
    }
    exchange_packets(client, server, false);
}

fn connect() -> (Http3Client, Http3Server) {
    let cp = ConnectionParameters::default()
        .max_streams(STREAM_TYPE, STREAMS_MAX)
        .pmtud(false)
        .pacing(false)
        .mlkem(false)
        .sni_slicing(false);
    let h3p = Http3Parameters::default().connection_parameters(cp);
    let mut client = http3_client_with_params(h3p.clone());
    let mut server = http3_server_with_params(h3p);
    exchange_packets(&mut client, &mut server, true);
    (client, server)
}

fn criterion_benchmark(c: &mut Criterion) {
    fixture_init();

    for (streams, data_size) in [(1_000, 1), (1_000, 1_000)] {
        let mut group = c.benchmark_group(format!("{streams} streams of {data_size} bytes"));

        // High variance benchmark. Increase default sample size (100).
        group.sample_size(500);

        group.bench_function("multistream", |b| {
            let data = vec![0; data_size];
            b.iter_batched_ref(
                connect,
                |_| black_box(|(client, server)| use_streams(client, server, streams, &data)),
                BatchSize::SmallInput,
            );
        });
        group.finish();
    }
}

criterion_group! {
    name = benches;
    config = Criterion::default().warm_up_time(Duration::from_secs(5)).measurement_time(Duration::from_secs(60));
    targets = criterion_benchmark
}
criterion_main!(benches);
