// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![expect(clippy::unwrap_used, reason = "OK in a bench.")]

use std::{env, hint::black_box, path::PathBuf, str::FromStr as _, time::Duration};

use criterion::{criterion_group, criterion_main, BatchSize, Criterion, Throughput};
use neqo_bin::{client, server};
use tokio::runtime::Builder;

struct Benchmark {
    name: String,
    requests: Vec<usize>,
    upload: bool,
}

fn transfer(c: &mut Criterion) {
    neqo_crypto::init_db(PathBuf::from_str("../test-fixture/db").unwrap()).unwrap();

    let done_sender = spawn_server();
    let mtu = env::var("MTU").map_or_else(|_| String::new(), |mtu| format!("/mtu-{mtu}"));
    for Benchmark {
        name,
        requests,
        upload,
    } in [
        Benchmark {
            name: format!("1-conn/1-100mb-resp{mtu} (aka. Download)"),
            requests: vec![100 * 1024 * 1024],
            upload: false,
        },
        Benchmark {
            name: format!("1-conn/10_000-parallel-1b-resp{mtu} (aka. RPS)"),
            requests: vec![1; 10_000],
            upload: false,
        },
        Benchmark {
            name: format!("1-conn/1-1b-resp{mtu} (aka. HPS)"),
            requests: vec![1; 1],
            upload: false,
        },
        Benchmark {
            name: format!("1-conn/1-100mb-req{mtu} (aka. Upload)"),
            requests: vec![100 * 1024 * 1024],
            upload: true,
        },
    ] {
        let mut group = c.benchmark_group(name);
        group.throughput(if requests[0] > 1 {
            assert_eq!(requests.len(), 1);
            Throughput::Bytes(requests[0] as u64)
        } else {
            Throughput::Elements(requests.len() as u64)
        });
        group.bench_function("client", |b| {
            b.to_async(Builder::new_current_thread().enable_all().build().unwrap())
                .iter_batched(
                    || client::client(client::Args::new(&requests, upload)),
                    |client| {
                        black_box(async move {
                            client.await.unwrap();
                        })
                    },
                    BatchSize::PerIteration,
                );
        });
        group.finish();
    }

    done_sender.send(()).unwrap();
}

fn spawn_server() -> tokio::sync::oneshot::Sender<()> {
    let (done_sender, mut done_receiver) = tokio::sync::oneshot::channel();
    std::thread::spawn(move || {
        Builder::new_current_thread()
            .enable_all()
            .build()
            .unwrap()
            .block_on(async {
                let mut server = Box::pin(server::server(server::Args::default()));
                tokio::select! {
                    _ = &mut done_receiver => {}
                    res = &mut server  => panic!("expect server not to terminate: {res:?}"),
                };
            });
    });
    done_sender
}

criterion_group! {
    name = benches;
    config = Criterion::default().warm_up_time(Duration::from_secs(5)).measurement_time(Duration::from_secs(60));
    targets = transfer
}
criterion_main!(benches);
