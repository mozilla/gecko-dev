// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! A simulated transfer benchmark, asserting a minimum bandwidth.
//!
//! This is using [`test_fixture::sim`], i.e. does no I/O beyond the process
//! boundary and runs in simulated time. Given that [`test_fixture::sim`] is
//! deterministic, there is no need for multiple benchmark iterations. Still it
//! is a Rust benchmark instead of a unit test due to its runtime (> 10s) even
//! in Rust release mode.

use std::time::Duration;

use neqo_transport::{ConnectionParameters, State};
use test_fixture::{
    boxed,
    sim::{
        connection::{ConnectionNode, ReachState, ReceiveData, SendData},
        network::{Delay, Mtu, TailDrop},
        Simulator,
    },
};

#[expect(clippy::cast_precision_loss, reason = "OK in a bench.")]
pub fn main() {
    const MIB: usize = 1_024 * 1_024;
    const GIB: usize = 1_024 * MIB;

    const MBIT: usize = 1_000 * 1_000;
    const GBIT: usize = 1_000 * MBIT;

    const TRANSFER_AMOUNT: usize = GIB;
    const LINK_BANDWIDTH: usize = GBIT;
    const LINK_RTT_MS: usize = 40;
    const MINIMUM_EXPECTED_UTILIZATION: f64 = 0.85;

    let gbit_link = || {
        let rate_byte = LINK_BANDWIDTH / 8;
        // Router buffer set to bandwidth-delay product.
        let capacity_byte = rate_byte * LINK_RTT_MS / 1_000;
        TailDrop::new(rate_byte, capacity_byte, Duration::ZERO)
    };

    let simulated_time = Simulator::new(
        "gbit-bandwidth",
        boxed![
            ConnectionNode::new_client(
                ConnectionParameters::default(),
                boxed![ReachState::new(State::Confirmed)],
                boxed![ReceiveData::new(TRANSFER_AMOUNT)]
            ),
            Mtu::new(1500),
            gbit_link(),
            Delay::new(Duration::from_millis(LINK_RTT_MS as u64 / 2)),
            ConnectionNode::new_server(
                ConnectionParameters::default(),
                boxed![ReachState::new(State::Confirmed)],
                boxed![SendData::new(TRANSFER_AMOUNT)]
            ),
            Mtu::new(1500),
            gbit_link(),
            Delay::new(Duration::from_millis(LINK_RTT_MS as u64 / 2)),
        ],
    )
    .setup()
    .run();

    let achieved_bandwidth = TRANSFER_AMOUNT as f64 * 8.0 / simulated_time.as_secs_f64();

    assert!(
        LINK_BANDWIDTH as f64 * MINIMUM_EXPECTED_UTILIZATION < achieved_bandwidth,
        "expected to reach {MINIMUM_EXPECTED_UTILIZATION} of maximum bandwidth ({} Mbit/s) but got {} Mbit/s",
        LINK_BANDWIDTH  / MBIT,
        achieved_bandwidth / MBIT as f64,
    );
}
