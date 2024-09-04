// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

// Tracking of some useful statistics.

use std::{
    cell::RefCell,
    fmt::{self, Debug},
    ops::Deref,
    rc::Rc,
    time::Duration,
};

use neqo_common::qwarn;

use crate::{ecn::EcnCount, packet::PacketNumber};

pub const MAX_PTO_COUNTS: usize = 16;

#[derive(Default, Clone)]
#[cfg_attr(test, derive(PartialEq, Eq))]
#[allow(clippy::module_name_repetitions)]
pub struct FrameStats {
    pub all: usize,
    pub ack: usize,
    pub largest_acknowledged: PacketNumber,

    pub crypto: usize,
    pub stream: usize,
    pub reset_stream: usize,
    pub stop_sending: usize,

    pub ping: usize,
    pub padding: usize,

    pub max_streams: usize,
    pub streams_blocked: usize,
    pub max_data: usize,
    pub data_blocked: usize,
    pub max_stream_data: usize,
    pub stream_data_blocked: usize,

    pub new_connection_id: usize,
    pub retire_connection_id: usize,

    pub path_challenge: usize,
    pub path_response: usize,

    pub connection_close: usize,
    pub handshake_done: usize,
    pub new_token: usize,

    pub ack_frequency: usize,
    pub datagram: usize,
}

impl Debug for FrameStats {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        writeln!(
            f,
            "    crypto {} done {} token {} close {}",
            self.crypto, self.handshake_done, self.new_token, self.connection_close,
        )?;
        writeln!(
            f,
            "    ack {} (max {}) ping {} padding {}",
            self.ack, self.largest_acknowledged, self.ping, self.padding
        )?;
        writeln!(
            f,
            "    stream {} reset {} stop {}",
            self.stream, self.reset_stream, self.stop_sending,
        )?;
        writeln!(
            f,
            "    max: stream {} data {} stream_data {}",
            self.max_streams, self.max_data, self.max_stream_data,
        )?;
        writeln!(
            f,
            "    blocked: stream {} data {} stream_data {}",
            self.streams_blocked, self.data_blocked, self.stream_data_blocked,
        )?;
        writeln!(f, "    datagram {}", self.datagram)?;
        writeln!(
            f,
            "    ncid {} rcid {} pchallenge {} presponse {}",
            self.new_connection_id,
            self.retire_connection_id,
            self.path_challenge,
            self.path_response,
        )?;
        writeln!(f, "    ack_frequency {}", self.ack_frequency)
    }
}

/// Datagram stats
#[derive(Default, Clone)]
#[allow(clippy::module_name_repetitions)]
pub struct DatagramStats {
    /// The number of datagrams declared lost.
    pub lost: usize,
    /// The number of datagrams dropped due to being too large.
    pub dropped_too_big: usize,
    /// The number of datagrams dropped due to reaching the limit of the
    /// outgoing queue.
    pub dropped_queue_full: usize,
}

/// Connection statistics
#[derive(Default, Clone)]
pub struct Stats {
    info: String,

    /// Total packets received, including all the bad ones.
    pub packets_rx: usize,
    /// Duplicate packets received.
    pub dups_rx: usize,
    /// Dropped packets or dropped garbage.
    pub dropped_rx: usize,
    /// The number of packet that were saved for later processing.
    pub saved_datagrams: usize,

    /// Total packets sent.
    pub packets_tx: usize,
    /// Total number of packets that are declared lost.
    pub lost: usize,
    /// Late acknowledgments, for packets that were declared lost already.
    pub late_ack: usize,
    /// Acknowledgments for packets that contained data that was marked
    /// for retransmission when the PTO timer popped.
    pub pto_ack: usize,
    /// Number of PMTUD probes sent.
    pub pmtud_tx: usize,
    /// Number of PMTUD probes ACK'ed.
    pub pmtud_ack: usize,
    /// Number of PMTUD probes lost.
    pub pmtud_lost: usize,
    /// Number of times a path MTU changed unexpectedly.
    pub pmtud_change: usize,

    /// Whether the connection was resumed successfully.
    pub resumed: bool,

    /// The current, estimated round-trip time on the primary path.
    pub rtt: Duration,
    /// The current, estimated round-trip time variation on the primary path.
    pub rttvar: Duration,
    /// Whether the first RTT sample was guessed from a discarded packet.
    pub rtt_init_guess: bool,

    /// Count PTOs. Single PTOs, 2 PTOs in a row, 3 PTOs in row, etc. are counted
    /// separately.
    pub pto_counts: [usize; MAX_PTO_COUNTS],

    /// Count frames received.
    pub frame_rx: FrameStats,
    /// Count frames sent.
    pub frame_tx: FrameStats,

    /// The number of incoming datagrams dropped due to reaching the limit
    /// of the incoming queue.
    pub incoming_datagram_dropped: usize,

    pub datagram_tx: DatagramStats,

    /// Number of paths known to be ECN capable.
    pub ecn_paths_capable: usize,
    /// Number of paths known to be ECN incapable.
    pub ecn_paths_not_capable: usize,
    /// ECN counts for outgoing UDP datagrams, returned by remote through QUIC ACKs.
    ///
    /// Note: Given that QUIC ACKs only carry [`Ect0`], [`Ect1`] and [`Ce`], but
    /// never [`NotEct`], the [`NotEct`] value will always be 0.
    ///
    /// See also <https://www.rfc-editor.org/rfc/rfc9000.html#section-19.3.2>.
    ///
    /// [`Ect0`]: neqo_common::tos::IpTosEcn::Ect0
    /// [`Ect1`]: neqo_common::tos::IpTosEcn::Ect1
    /// [`Ce`]: neqo_common::tos::IpTosEcn::Ce
    /// [`NotEct`]: neqo_common::tos::IpTosEcn::NotEct
    pub ecn_tx: EcnCount,
    /// ECN counts for incoming UDP datagrams, read from IP TOS header.
    pub ecn_rx: EcnCount,
}

impl Stats {
    pub fn init(&mut self, info: String) {
        self.info = info;
    }

    pub fn pkt_dropped(&mut self, reason: impl AsRef<str>) {
        self.dropped_rx += 1;
        qwarn!(
            [self.info],
            "Dropped received packet: {}; Total: {}",
            reason.as_ref(),
            self.dropped_rx
        );
    }

    /// # Panics
    ///
    /// When preconditions are violated.
    pub fn add_pto_count(&mut self, count: usize) {
        debug_assert!(count > 0);
        if count >= MAX_PTO_COUNTS {
            // We can't move this count any further, so stop.
            return;
        }
        self.pto_counts[count - 1] += 1;
        if count > 1 {
            debug_assert!(self.pto_counts[count - 2] > 0);
            self.pto_counts[count - 2] -= 1;
        }
    }
}

impl Debug for Stats {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        writeln!(f, "stats for {}", self.info)?;
        writeln!(
            f,
            "  rx: {} drop {} dup {} saved {}",
            self.packets_rx, self.dropped_rx, self.dups_rx, self.saved_datagrams
        )?;
        writeln!(
            f,
            "  tx: {} lost {} lateack {} ptoack {}",
            self.packets_tx, self.lost, self.late_ack, self.pto_ack
        )?;
        writeln!(
            f,
            "  pmtud: {} sent {} acked {} lost {} change",
            self.pmtud_tx, self.pmtud_ack, self.pmtud_lost, self.pmtud_change
        )?;
        writeln!(f, "  resumed: {}", self.resumed)?;
        writeln!(f, "  frames rx:")?;
        self.frame_rx.fmt(f)?;
        writeln!(f, "  frames tx:")?;
        self.frame_tx.fmt(f)?;
        writeln!(
            f,
            "  ecn: {:?} for tx {:?} for rx {} capable paths {} not capable paths",
            self.ecn_tx, self.ecn_rx, self.ecn_paths_capable, self.ecn_paths_not_capable
        )
    }
}

#[derive(Default, Clone)]
#[allow(clippy::module_name_repetitions)]
pub struct StatsCell {
    stats: Rc<RefCell<Stats>>,
}

impl Deref for StatsCell {
    type Target = RefCell<Stats>;
    fn deref(&self) -> &Self::Target {
        &self.stats
    }
}

impl Debug for StatsCell {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        self.stats.borrow().fmt(f)
    }
}
