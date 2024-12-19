// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

// Tracking of sent packets and detecting their loss.

use std::{
    cmp::{max, min},
    time::{Duration, Instant},
};

use neqo_common::{qlog::NeqoQlog, qtrace};

use crate::{
    ackrate::{AckRate, PeerAckDelay},
    packet::PacketBuilder,
    qlog::{self, QlogMetric},
    recovery::RecoveryToken,
    stats::FrameStats,
};

/// The smallest time that the system timer (via `sleep()`, `nanosleep()`,
/// `select()`, or similar) can reliably deliver; see `neqo_common::hrtime`.
pub const GRANULARITY: Duration = Duration::from_millis(1);
// Defined in -recovery 6.2 as 333ms but using lower value.
pub const INITIAL_RTT: Duration = Duration::from_millis(100);

/// The source of the RTT measurement.
#[derive(Debug, PartialEq, Eq, PartialOrd, Ord, Clone, Copy)]
pub enum RttSource {
    /// RTT guess from a retry or dropping a packet number space.
    Guesstimate,
    /// Ack on an unconfirmed connection.
    Ack,
    /// Ack on a confirmed connection.
    AckConfirmed,
}

#[derive(Debug)]
#[allow(clippy::module_name_repetitions)]
pub struct RttEstimate {
    first_sample_time: Option<Instant>,
    latest_rtt: Duration,
    smoothed_rtt: Duration,
    rttvar: Duration,
    min_rtt: Duration,
    ack_delay: PeerAckDelay,
    best_source: RttSource,
}

impl RttEstimate {
    fn init(&mut self, rtt: Duration) {
        // Only allow this when there are no samples.
        debug_assert!(self.first_sample_time.is_none());
        self.latest_rtt = rtt;
        self.min_rtt = rtt;
        self.smoothed_rtt = rtt;
        self.rttvar = rtt / 2;
    }

    #[cfg(test)]
    pub const fn from_duration(rtt: Duration) -> Self {
        Self {
            first_sample_time: None,
            latest_rtt: rtt,
            smoothed_rtt: rtt,
            rttvar: Duration::from_millis(0),
            min_rtt: rtt,
            ack_delay: PeerAckDelay::Fixed(Duration::from_millis(25)),
            best_source: RttSource::Ack,
        }
    }

    pub fn set_initial(&mut self, rtt: Duration) {
        qtrace!("initial RTT={:?}", rtt);
        if rtt >= GRANULARITY {
            // Ignore if the value is too small.
            self.init(rtt);
        }
    }

    /// For a new path, prime the RTT based on the state of another path.
    pub fn prime_rtt(&mut self, other: &Self) {
        self.set_initial(other.smoothed_rtt + other.rttvar);
        self.ack_delay = other.ack_delay.clone();
    }

    pub fn set_ack_delay(&mut self, ack_delay: PeerAckDelay) {
        self.ack_delay = ack_delay;
    }

    pub fn update_ack_delay(&mut self, cwnd: usize, mtu: usize) {
        self.ack_delay.update(cwnd, mtu, self.smoothed_rtt);
    }

    pub fn is_guesstimate(&self) -> bool {
        self.best_source == RttSource::Guesstimate
    }

    pub fn update(
        &mut self,
        qlog: &NeqoQlog,
        mut rtt_sample: Duration,
        ack_delay: Duration,
        source: RttSource,
        now: Instant,
    ) {
        debug_assert!(source >= self.best_source);
        self.best_source = max(self.best_source, source);

        // Limit ack delay by max_ack_delay if confirmed.
        let mad = self.ack_delay.max();
        let ack_delay = if self.best_source == RttSource::AckConfirmed && ack_delay > mad {
            mad
        } else {
            ack_delay
        };

        // min_rtt ignores ack delay.
        self.min_rtt = min(self.min_rtt, rtt_sample);
        // Adjust for ack delay unless it goes below `min_rtt`.
        if rtt_sample - self.min_rtt >= ack_delay {
            rtt_sample -= ack_delay;
        }

        if self.first_sample_time.is_none() {
            self.init(rtt_sample);
            self.first_sample_time = Some(now);
        } else {
            // Calculate EWMA RTT (based on {{?RFC6298}}).
            let rttvar_sample = if self.smoothed_rtt > rtt_sample {
                self.smoothed_rtt - rtt_sample
            } else {
                rtt_sample - self.smoothed_rtt
            };

            self.latest_rtt = rtt_sample;
            self.rttvar = (self.rttvar * 3 + rttvar_sample) / 4;
            self.smoothed_rtt = (self.smoothed_rtt * 7 + rtt_sample) / 8;
        }
        qtrace!(
            "RTT latest={:?} -> estimate={:?}~{:?}",
            self.latest_rtt,
            self.smoothed_rtt,
            self.rttvar
        );
        qlog::metrics_updated(
            qlog,
            &[
                QlogMetric::LatestRtt(self.latest_rtt),
                QlogMetric::MinRtt(self.min_rtt),
                QlogMetric::SmoothedRtt(self.smoothed_rtt),
                QlogMetric::RttVariance(self.rttvar),
            ],
            now,
        );
    }

    /// Get the estimated value.
    pub const fn estimate(&self) -> Duration {
        self.smoothed_rtt
    }

    pub fn pto(&self, confirmed: bool) -> Duration {
        let mut t = self.estimate() + max(4 * self.rttvar, GRANULARITY);
        if confirmed {
            t += self.ack_delay.max();
        }
        t
    }

    /// Calculate the loss delay based on the current estimate and the last
    /// RTT measurement received.
    pub fn loss_delay(&self) -> Duration {
        // kTimeThreshold = 9/8
        // loss_delay = kTimeThreshold * max(latest_rtt, smoothed_rtt)
        // loss_delay = max(loss_delay, kGranularity)
        let rtt = max(self.latest_rtt, self.smoothed_rtt);
        max(rtt * 9 / 8, GRANULARITY)
    }

    pub const fn first_sample_time(&self) -> Option<Instant> {
        self.first_sample_time
    }

    #[cfg(test)]
    pub const fn latest(&self) -> Duration {
        self.latest_rtt
    }

    pub const fn rttvar(&self) -> Duration {
        self.rttvar
    }

    pub const fn minimum(&self) -> Duration {
        self.min_rtt
    }

    pub fn write_frames(
        &mut self,
        builder: &mut PacketBuilder,
        tokens: &mut Vec<RecoveryToken>,
        stats: &mut FrameStats,
    ) {
        self.ack_delay.write_frames(builder, tokens, stats);
    }

    pub fn frame_lost(&mut self, lost: &AckRate) {
        self.ack_delay.frame_lost(lost);
    }

    pub fn frame_acked(&mut self, acked: &AckRate) {
        self.ack_delay.frame_acked(acked);
    }
}

impl Default for RttEstimate {
    fn default() -> Self {
        Self {
            first_sample_time: None,
            latest_rtt: INITIAL_RTT,
            smoothed_rtt: INITIAL_RTT,
            rttvar: INITIAL_RTT / 2,
            min_rtt: INITIAL_RTT,
            ack_delay: PeerAckDelay::default(),
            best_source: RttSource::Guesstimate,
        }
    }
}
