// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! CUBIC congestion control

use std::{
    fmt::{self, Display},
    time::{Duration, Instant},
};

use neqo_common::qtrace;

use crate::cc::classic_cc::WindowAdjustment;

/// > C is a constant fixed to determine the aggressiveness of window
/// > increase  in high BDP networks.
///
/// <https://datatracker.ietf.org/doc/html/rfc8312#section-4.1>
///
/// See discussion for rational for concrete value.
///
/// <https://datatracker.ietf.org/doc/html/rfc8312#section-5.1>
pub const CUBIC_C: f64 = 0.4;
/// TCP-friendly region additive factor
///
/// <https://datatracker.ietf.org/doc/html/rfc8312#section-4.2>
pub const CUBIC_ALPHA: f64 = 3.0 * (1.0 - 0.7) / (1.0 + 0.7);

/// `CUBIC_BETA` = 0.7;
///
/// > Principle 4: To balance between the scalability and convergence speed,
/// > CUBIC sets the multiplicative window decrease factor to 0.7 while Standard
/// > TCP uses 0.5.  While this improves the scalability of CUBIC, a side effect
/// > of this decision is slower convergence, especially under low statistical
/// > multiplexing environments.
///
/// <https://datatracker.ietf.org/doc/html/rfc8312#section-3>
pub const CUBIC_BETA_USIZE_DIVIDEND: usize = 7;
pub const CUBIC_BETA_USIZE_DIVISOR: usize = 10;

/// The fast convergence ratio further reduces the congestion window when a
/// congestion event occurs before reaching the previous `W_max`.
///
/// See formula defined below.
///
/// <https://www.rfc-editor.org/rfc/rfc8312#section-4.6>
pub const CUBIC_FAST_CONVERGENCE: f64 = 0.85; // (1.0 + CUBIC_BETA) / 2.0;

/// The minimum number of multiples of the datagram size that need
/// to be received to cause an increase in the congestion window.
/// When there is no loss, Cubic can return to exponential increase, but
/// this value reduces the magnitude of the resulting growth by a constant factor.
/// A value of 1.0 would mean a return to the rate used in slow start.
const EXPONENTIAL_GROWTH_REDUCTION: f64 = 2.0;

/// Convert an integer congestion window value into a floating point value.
/// This has the effect of reducing larger values to `1<<53`.
/// If you have a congestion window that large, something is probably wrong.
pub fn convert_to_f64(v: usize) -> f64 {
    let mut f_64 = f64::from(u32::try_from(v >> 21).unwrap_or(u32::MAX));
    f_64 *= 2_097_152.0; // f_64 <<= 21
    f_64 += f64::from(u32::try_from(v & 0x1f_ffff).unwrap());
    f_64
}

#[derive(Debug)]
pub struct Cubic {
    /// Maximum Window size two congestion events ago.
    ///
    /// > With fast convergence, when a congestion event occurs, before the
    /// > window reduction of the congestion window, a flow remembers the last
    /// > value of W_max before it updates W_max for the current congestion
    /// > event.
    ///
    /// <https://datatracker.ietf.org/doc/html/rfc8312#section-4.6>
    last_max_cwnd: f64,
    /// Estimate of Standard TCP congestion window for Cubic's TCP-friendly
    /// Region.
    ///
    /// > Standard TCP performs well in certain types of networks, for example,
    /// > under short RTT and small bandwidth (or small BDP) networks.  In
    /// > these networks, we use the TCP-friendly region to ensure that CUBIC
    /// > achieves at least the same throughput as Standard TCP.
    ///
    /// <https://datatracker.ietf.org/doc/html/rfc8312#section-4.2>
    estimated_tcp_cwnd: f64,
    /// > K is the time period that the above function takes to increase the
    /// > current window size to W_max if there are no further congestion events
    ///
    /// <https://datatracker.ietf.org/doc/html/rfc8312#section-4.1>
    k: f64,
    /// > W_max is the window size just before the window is reduced in the last
    /// > congestion event.
    ///
    /// <https://datatracker.ietf.org/doc/html/rfc8312#section-4.1>
    w_max: f64,
    /// > the elapsed time from the beginning of the current congestion
    /// > avoidance
    ///
    /// <https://datatracker.ietf.org/doc/html/rfc8312#section-4.1>
    ca_epoch_start: Option<Instant>,
    /// Number of bytes acked since the last Standard TCP congestion window increase.
    tcp_acked_bytes: f64,
}

impl Default for Cubic {
    fn default() -> Self {
        Self {
            last_max_cwnd: 0.0,
            estimated_tcp_cwnd: 0.0,
            k: 0.0,
            w_max: 0.0,
            ca_epoch_start: None,
            tcp_acked_bytes: 0.0,
        }
    }
}

impl Display for Cubic {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(
            f,
            "Cubic [last_max_cwnd: {}, k: {}, w_max: {}, ca_epoch_start: {:?}]",
            self.last_max_cwnd, self.k, self.w_max, self.ca_epoch_start
        )?;
        Ok(())
    }
}

#[allow(clippy::doc_markdown)]
impl Cubic {
    /// Original equations is:
    /// K = cubic_root(W_max*(1-beta_cubic)/C) (Eq. 2 RFC8312)
    /// W_max is number of segments of the maximum segment size (MSS).
    ///
    /// K is actually the time that W_cubic(t) = C*(t-K)^3 + W_max (Eq. 1) would
    /// take to increase to W_max. We use bytes not MSS units, therefore this
    /// equation will be: W_cubic(t) = C*MSS*(t-K)^3 + W_max.
    ///
    /// From that equation we can calculate K as:
    /// K = cubic_root((W_max - W_cubic) / C / MSS);
    ///
    /// <https://www.rfc-editor.org/rfc/rfc8312#section-4.1>
    fn calc_k(&self, curr_cwnd: f64, max_datagram_size: usize) -> f64 {
        ((self.w_max - curr_cwnd) / CUBIC_C / convert_to_f64(max_datagram_size)).cbrt()
    }

    /// W_cubic(t) = C*(t-K)^3 + W_max (Eq. 1)
    /// t is relative to the start of the congestion avoidance phase and it is in seconds.
    ///
    /// <https://www.rfc-editor.org/rfc/rfc8312#section-4.1>
    fn w_cubic(&self, t: f64, max_datagram_size: usize) -> f64 {
        (CUBIC_C * (t - self.k).powi(3)).mul_add(convert_to_f64(max_datagram_size), self.w_max)
    }

    fn start_epoch(
        &mut self,
        curr_cwnd_f64: f64,
        new_acked_f64: f64,
        max_datagram_size: usize,
        now: Instant,
    ) {
        self.ca_epoch_start = Some(now);
        // reset tcp_acked_bytes and estimated_tcp_cwnd;
        self.tcp_acked_bytes = new_acked_f64;
        self.estimated_tcp_cwnd = curr_cwnd_f64;
        if self.last_max_cwnd <= curr_cwnd_f64 {
            self.w_max = curr_cwnd_f64;
            self.k = 0.0;
        } else {
            self.w_max = self.last_max_cwnd;
            self.k = self.calc_k(curr_cwnd_f64, max_datagram_size);
        }
        qtrace!("[{self}] New epoch");
    }
}

impl WindowAdjustment for Cubic {
    // This is because of the cast in the last line from f64 to usize.
    #[allow(clippy::cast_possible_truncation)]
    #[allow(clippy::cast_sign_loss)]
    fn bytes_for_cwnd_increase(
        &mut self,
        curr_cwnd: usize,
        new_acked_bytes: usize,
        min_rtt: Duration,
        max_datagram_size: usize,
        now: Instant,
    ) -> usize {
        let curr_cwnd_f64 = convert_to_f64(curr_cwnd);
        let new_acked_f64 = convert_to_f64(new_acked_bytes);
        if self.ca_epoch_start.is_none() {
            // This is a start of a new congestion avoidance phase.
            self.start_epoch(curr_cwnd_f64, new_acked_f64, max_datagram_size, now);
        } else {
            self.tcp_acked_bytes += new_acked_f64;
        }

        // Cubic concave or convex region
        //
        // <https://datatracker.ietf.org/doc/html/rfc8312#section-4.3>
        // <https://datatracker.ietf.org/doc/html/rfc8312#section-4.4>
        let time_ca = self
            .ca_epoch_start
            .map_or(min_rtt, |t| {
                if now + min_rtt < t {
                    // This only happens when processing old packets
                    // that were saved and replayed with old timestamps.
                    min_rtt
                } else {
                    now + min_rtt - t
                }
            })
            .as_secs_f64();
        let target_cubic = self.w_cubic(time_ca, max_datagram_size);

        // Cubic TCP-friendly region
        //
        //  <https://datatracker.ietf.org/doc/html/rfc8312#section-4.2>
        let max_datagram_size = convert_to_f64(max_datagram_size);
        let tcp_cnt = self.estimated_tcp_cwnd / CUBIC_ALPHA;
        let incr = (self.tcp_acked_bytes / tcp_cnt).floor();
        if incr > 0.0 {
            self.tcp_acked_bytes -= incr * tcp_cnt;
            self.estimated_tcp_cwnd += incr * max_datagram_size;
        }

        // Take the larger cwnd of Cubic concave or convex and Cubic
        // TCP-friendly region.
        //
        // > When receiving an ACK in congestion avoidance (cwnd could be
        // > greater than or less than W_max), CUBIC checks whether W_cubic(t) is
        // > less than W_est(t).  If so, CUBIC is in the TCP-friendly region and
        // > cwnd SHOULD be set to W_est(t) at each reception of an ACK.
        //
        // <https://datatracker.ietf.org/doc/html/rfc8312#section-4.2>
        let target_cwnd = target_cubic.max(self.estimated_tcp_cwnd);

        // Calculate the number of bytes that would need to be acknowledged for an increase
        // of `max_datagram_size` to match the increase of `target - cwnd / cwnd` as defined
        // in the specification (Sections 4.4 and 4.5).
        // The amount of data required therefore reduces asymptotically as the target increases.
        // If the target is not significantly higher than the congestion window, require a very
        // large amount of acknowledged data (effectively block increases).
        let mut acked_to_increase =
            max_datagram_size * curr_cwnd_f64 / (target_cwnd - curr_cwnd_f64).max(1.0);

        // Limit increase to max 1 MSS per EXPONENTIAL_GROWTH_REDUCTION ack packets.
        // This effectively limits target_cwnd to (1 + 1 / EXPONENTIAL_GROWTH_REDUCTION) cwnd.
        acked_to_increase = acked_to_increase.max(EXPONENTIAL_GROWTH_REDUCTION * max_datagram_size);
        acked_to_increase as usize
    }

    fn reduce_cwnd(
        &mut self,
        curr_cwnd: usize,
        acked_bytes: usize,
        max_datagram_size: usize,
    ) -> (usize, usize) {
        let curr_cwnd_f64 = convert_to_f64(curr_cwnd);
        // Fast Convergence
        //
        // If congestion event occurs before the maximum congestion window before the last
        // congestion event, we reduce the the maximum congestion window and thereby W_max.
        // check cwnd + MAX_DATAGRAM_SIZE instead of cwnd because with cwnd in bytes, cwnd may be
        // slightly off.
        //
        // <https://www.rfc-editor.org/rfc/rfc8312#section-4.6>
        self.last_max_cwnd =
            if curr_cwnd_f64 + convert_to_f64(max_datagram_size) < self.last_max_cwnd {
                curr_cwnd_f64 * CUBIC_FAST_CONVERGENCE
            } else {
                curr_cwnd_f64
            };
        self.ca_epoch_start = None;
        (
            curr_cwnd * CUBIC_BETA_USIZE_DIVIDEND / CUBIC_BETA_USIZE_DIVISOR,
            acked_bytes * CUBIC_BETA_USIZE_DIVIDEND / CUBIC_BETA_USIZE_DIVISOR,
        )
    }

    fn on_app_limited(&mut self) {
        // Reset ca_epoch_start. Let it start again when the congestion controller
        // exits the app-limited period.
        self.ca_epoch_start = None;
    }

    #[cfg(test)]
    fn last_max_cwnd(&self) -> f64 {
        self.last_max_cwnd
    }

    #[cfg(test)]
    fn set_last_max_cwnd(&mut self, last_max_cwnd: f64) {
        self.last_max_cwnd = last_max_cwnd;
    }
}
