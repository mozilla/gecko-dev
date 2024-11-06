// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::time::Duration;

use criterion::{criterion_group, criterion_main, BatchSize::SmallInput, Criterion};
use neqo_transport::{ConnectionParameters, State};
use test_fixture::{
    boxed,
    sim::{
        connection::{ConnectionNode, ReachState, ReceiveData, SendData},
        network::{Delay, TailDrop},
        ReadySimulator, Simulator,
    },
};

const ZERO: Duration = Duration::from_millis(0);
const JITTER: Duration = Duration::from_millis(10);
const TRANSFER_AMOUNT: usize = 1 << 22; // 4Mbyte

#[allow(clippy::needless_pass_by_value)] // Passing String where &str would do is fine here.
fn benchmark_transfer(c: &mut Criterion, label: &str, seed: Option<impl AsRef<str>>) {
    for pacing in [false, true] {
        let mut group = c.benchmark_group(format!("transfer/pacing-{pacing}"));
        // Don't let criterion calculate throughput, as that's based on wall-clock time, not
        // simulator time.
        group.noise_threshold(0.03);
        group.bench_function(label, |b| {
            b.iter_batched(
                || {
                    let nodes = boxed![
                        ConnectionNode::new_client(
                            ConnectionParameters::default().pmtud(true).pacing(pacing),
                            boxed![ReachState::new(State::Confirmed)],
                            boxed![SendData::new(TRANSFER_AMOUNT)]
                        ),
                        TailDrop::dsl_uplink(),
                        Delay::new(ZERO..JITTER),
                        ConnectionNode::new_server(
                            ConnectionParameters::default().pmtud(true).pacing(pacing),
                            boxed![ReachState::new(State::Confirmed)],
                            boxed![ReceiveData::new(TRANSFER_AMOUNT)]
                        ),
                        TailDrop::dsl_downlink(),
                        Delay::new(ZERO..JITTER),
                    ];
                    let mut sim = Simulator::new(label, nodes);
                    if let Some(seed) = &seed {
                        sim.seed_str(seed);
                    }
                    sim.setup()
                },
                ReadySimulator::run,
                SmallInput,
            );
        });
        group.finish();
    }
}

fn benchmark_transfer_variable(c: &mut Criterion) {
    benchmark_transfer(c, "varying-seeds", std::env::var("SIMULATION_SEED").ok());
}

fn benchmark_transfer_fixed(c: &mut Criterion) {
    benchmark_transfer(
        c,
        "same-seed",
        Some("62df6933ba1f543cece01db8f27fb2025529b27f93df39e19f006e1db3b8c843"),
    );
}

criterion_group! {
    name = transfer;
    config = Criterion::default().warm_up_time(Duration::from_secs(5)).measurement_time(Duration::from_secs(15));
    targets = benchmark_transfer_variable, benchmark_transfer_fixed
}
criterion_main!(transfer);
