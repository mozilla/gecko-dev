// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

// Tracking of sent packets and detecting their loss.

#![allow(
    clippy::module_name_repetitions,
    reason = "<https://github.com/mozilla/neqo/issues/2284#issuecomment-2782711813>"
)]

#[cfg(feature = "bench")]
pub mod sent;
#[cfg(not(feature = "bench"))]
mod sent;
mod token;

use std::{
    cmp::{max, min},
    fmt::{self, Display, Formatter},
    ops::RangeInclusive,
    time::{Duration, Instant},
};

use enum_map::EnumMap;
use enumset::enum_set;
use neqo_common::{qdebug, qinfo, qlog::NeqoQlog, qtrace, qwarn};
pub use sent::SentPacket;
use sent::SentPackets;
use strum::IntoEnumIterator as _;
pub use token::{RecoveryToken, StreamRecoveryToken};

use crate::{
    ecn,
    packet::PacketNumber,
    path::{Path, PathRef},
    qlog::{self, QlogMetric},
    rtt::{RttEstimate, RttSource},
    stats::{Stats, StatsCell},
    tracking::{PacketNumberSpace, PacketNumberSpaceSet},
};

pub const PACKET_THRESHOLD: u64 = 3;
/// `ACK_ONLY_SIZE_LIMIT` is the minimum size of the congestion window.
/// If the congestion window is this small, we will only send ACK frames.
pub const ACK_ONLY_SIZE_LIMIT: usize = 256;
/// The maximum number of packets we send on a PTO.
pub const MAX_PTO_PACKET_COUNT: usize = 2;
/// The preferred limit on the number of packets that are tracked.
/// If we exceed this number, we start sending `PING` frames sooner to
/// force the peer to acknowledge some of them.
pub const MAX_OUTSTANDING_UNACK: usize = 200;
/// Disable PING until this many packets are outstanding.
pub const MIN_OUTSTANDING_UNACK: usize = 16;
/// The scale we use for the fast PTO feature.
pub const FAST_PTO_SCALE: u8 = 100;

/// `SendProfile` tells a sender how to send packets.
#[derive(Debug)]
pub struct SendProfile {
    /// The limit on the size of the packet.
    limit: usize,
    /// Whether this is a PTO, and what space the PTO is for.
    pto: Option<PacketNumberSpace>,
    /// What spaces should be probed.
    probe: PacketNumberSpaceSet,
    /// Whether pacing is active.
    paced: bool,
}

impl SendProfile {
    #[must_use]
    pub fn new_limited(limit: usize) -> Self {
        // When the limit is too low, we only send ACK frames.
        // Set the limit to `ACK_ONLY_SIZE_LIMIT - 1` to ensure that
        // ACK-only packets are still limited in size.
        Self {
            limit: max(ACK_ONLY_SIZE_LIMIT - 1, limit),
            pto: None,
            probe: PacketNumberSpaceSet::default(),
            paced: false,
        }
    }

    #[must_use]
    pub fn new_paced() -> Self {
        // When pacing, we still allow ACK frames to be sent.
        Self {
            limit: ACK_ONLY_SIZE_LIMIT - 1,
            pto: None,
            probe: PacketNumberSpaceSet::default(),
            paced: true,
        }
    }

    #[must_use]
    pub fn new_pto(pn_space: PacketNumberSpace, mtu: usize, probe: PacketNumberSpaceSet) -> Self {
        debug_assert!(mtu > ACK_ONLY_SIZE_LIMIT);
        debug_assert!(probe.contains(pn_space));
        Self {
            limit: mtu,
            pto: Some(pn_space),
            probe,
            paced: false,
        }
    }

    /// Whether probing this space is helpful.  This isn't necessarily the space
    /// that caused the timer to pop, but it is helpful to send a PING in a space
    /// that has the PTO timer armed.
    #[must_use]
    pub fn should_probe(&self, space: PacketNumberSpace) -> bool {
        self.probe.contains(space)
    }

    /// Determine whether an ACK-only packet should be sent for the given packet
    /// number space.
    /// Send only ACKs either: when the space available is too small, or when a PTO
    /// exists for a later packet number space (which should get the most space).
    #[must_use]
    pub fn ack_only(&self, space: PacketNumberSpace) -> bool {
        self.limit < ACK_ONLY_SIZE_LIMIT || self.pto.is_some_and(|sp| space < sp)
    }

    #[must_use]
    pub const fn paced(&self) -> bool {
        self.paced
    }

    #[must_use]
    pub const fn limit(&self) -> usize {
        self.limit
    }
}

#[derive(Debug)]
pub struct LossRecoverySpace {
    space: PacketNumberSpace,
    largest_acked: Option<PacketNumber>,
    largest_acked_sent_time: Option<Instant>,
    /// The time used to calculate the PTO timer for this space.
    /// This is the time that the last ACK-eliciting packet in this space
    /// was sent.  This might be the time that a probe was sent.
    last_ack_eliciting: Option<Instant>,
    /// The number of outstanding packets in this space that are in flight.
    /// This might be less than the number of ACK-eliciting packets,
    /// because PTO packets don't count.
    in_flight_outstanding: usize,
    /// The packets that we have sent and are tracking.
    sent_packets: SentPackets,
    /// The time that the first out-of-order packet was sent.
    /// This is `None` if there were no out-of-order packets detected.
    /// When set to `Some(T)`, time-based loss detection should be enabled.
    first_ooo_time: Option<Instant>,
}

impl LossRecoverySpace {
    #[must_use]
    pub fn new(space: PacketNumberSpace) -> Self {
        Self {
            space,
            largest_acked: None,
            largest_acked_sent_time: None,
            last_ack_eliciting: None,
            in_flight_outstanding: 0,
            sent_packets: SentPackets::default(),
            first_ooo_time: None,
        }
    }

    /// Find the time we sent the first packet that is lower than the
    /// largest acknowledged and that isn't yet declared lost.
    /// Use the value we prepared earlier in `detect_lost_packets`.
    #[must_use]
    pub const fn loss_recovery_timer_start(&self) -> Option<Instant> {
        self.first_ooo_time
    }

    #[must_use]
    pub const fn in_flight_outstanding(&self) -> bool {
        self.in_flight_outstanding > 0
    }

    pub fn pto_packets(&mut self) -> impl Iterator<Item = &SentPacket> {
        self.sent_packets.iter_mut().filter_map(|sent| {
            sent.pto().then(|| {
                qtrace!("PTO: marking packet {} lost ", sent.pn());
                &*sent
            })
        })
    }

    #[must_use]
    pub fn pto_base_time(&self) -> Option<Instant> {
        if self.in_flight_outstanding() {
            debug_assert!(self.last_ack_eliciting.is_some());
            self.last_ack_eliciting
        } else if self.space == PacketNumberSpace::ApplicationData {
            None
        } else {
            // Nasty special case to prevent handshake deadlocks.
            // A client needs to keep the PTO timer armed to prevent a stall
            // of the handshake.  Technically, this has to stop once we receive
            // an ACK of Handshake or 1-RTT, or when we receive HANDSHAKE_DONE,
            // but a few extra probes won't hurt.
            // It only means that we fail anti-amplification tests.
            // A server shouldn't arm its PTO timer this way. The server sends
            // ack-eliciting, in-flight packets immediately so this only
            // happens when the server has nothing outstanding.  If we had
            // client authentication, this might cause some extra probes,
            // but they would be harmless anyway.
            self.last_ack_eliciting
        }
    }

    pub fn on_packet_sent(&mut self, sent_packet: SentPacket) {
        if sent_packet.ack_eliciting() {
            self.last_ack_eliciting = Some(sent_packet.time_sent());
            self.in_flight_outstanding += 1;
        } else if self.space != PacketNumberSpace::ApplicationData
            && self.last_ack_eliciting.is_none()
        {
            // For Initial and Handshake spaces, make sure that we have a PTO baseline
            // always. See `LossRecoverySpace::pto_base_time()` for details.
            self.last_ack_eliciting = Some(sent_packet.time_sent());
        }
        self.sent_packets.track(sent_packet);
    }

    /// If we are only sending ACK frames, send a PING frame after 2 PTOs so that
    /// the peer sends an ACK frame.  If we have received lots of packets and no ACK,
    /// send a PING frame after 1 PTO.  Note that this can't be within a PTO, or
    /// we would risk setting up a feedback loop; having this many packets
    /// outstanding can be normal and we don't want to PING too often.
    #[must_use]
    pub fn should_probe(&self, pto: Duration, now: Instant) -> bool {
        let n_pto = if self.sent_packets.len() >= MAX_OUTSTANDING_UNACK {
            1
        } else if self.sent_packets.len() >= MIN_OUTSTANDING_UNACK {
            2
        } else {
            return false;
        };
        self.last_ack_eliciting
            .is_some_and(|t| now > t + (pto * n_pto))
    }

    fn remove_outstanding(&mut self, count: usize) {
        debug_assert!(self.in_flight_outstanding >= count);
        self.in_flight_outstanding -= count;
        if self.in_flight_outstanding == 0 {
            qtrace!("remove_packet outstanding == 0 for space {}", self.space);
        }
    }

    fn remove_packet(&mut self, p: &SentPacket) {
        if p.ack_eliciting() {
            self.remove_outstanding(1);
        }
    }

    /// Remove all newly acknowledged packets.
    /// Returns all the acknowledged packets, with the largest packet number first.
    /// ...and a boolean indicating if any of those packets were ack-eliciting.
    /// This operates more efficiently because it assumes that the input is sorted
    /// in the order that an ACK frame is (from the top).
    fn remove_acked<R>(&mut self, acked_ranges: R, stats: &mut Stats) -> (Vec<SentPacket>, bool)
    where
        R: IntoIterator<Item = RangeInclusive<PacketNumber>>,
        R::IntoIter: ExactSizeIterator,
    {
        let acked = self.sent_packets.take_ranges(acked_ranges);
        let mut eliciting = false;
        for p in &acked {
            self.remove_packet(p);
            eliciting |= p.ack_eliciting();
            if p.lost() {
                stats.late_ack += 1;
            }
            if p.pto_fired() {
                stats.pto_ack += 1;
            }
        }
        (acked, eliciting)
    }

    /// Remove all tracked packets from the space.
    /// This is called by a client when 0-RTT packets are dropped, when a Retry is received
    /// and when keys are dropped.
    fn remove_ignored(&mut self) -> impl Iterator<Item = SentPacket> {
        self.in_flight_outstanding = 0;
        std::mem::take(&mut self.sent_packets).drain_all()
    }

    /// Remove the primary path marking on any packets this is tracking.
    fn migrate(&mut self) {
        for pkt in self.sent_packets.iter_mut() {
            pkt.clear_primary_path();
        }
    }

    /// Remove old packets that we've been tracking in case they get acknowledged.
    /// We try to keep these around until a probe is sent for them, so it is
    /// important that `cd` is set to at least the current PTO time; otherwise we
    /// might remove all in-flight packets and stop sending probes.
    fn remove_old_lost(&mut self, now: Instant, cd: Duration) {
        let removed = self.sent_packets.remove_expired(now, cd);
        self.remove_outstanding(removed);
    }

    /// Detect lost packets.
    /// `loss_delay` is the time we will wait before declaring something lost.
    /// `cleanup_delay` is the time we will wait before cleaning up a lost packet.
    pub fn detect_lost_packets(
        &mut self,
        now: Instant,
        loss_delay: Duration,
        cleanup_delay: Duration,
        lost_packets: &mut Vec<SentPacket>,
    ) {
        // Housekeeping.
        self.remove_old_lost(now, cleanup_delay);

        qtrace!(
            "detect lost {}: now={now:?} delay={loss_delay:?}",
            self.space,
        );
        self.first_ooo_time = None;

        let largest_acked = self.largest_acked;

        for packet in self
            .sent_packets
            .iter_mut()
            // BTreeMap iterates in order of ascending PN
            .take_while(|p| largest_acked.is_some_and(|largest_ack| p.pn() < largest_ack))
        {
            // Packets sent before now - loss_delay are deemed lost.
            if packet.time_sent() + loss_delay <= now {
                qtrace!(
                    "lost={}, time sent {:?} is before lost_delay {loss_delay:?}",
                    packet.pn(),
                    packet.time_sent()
                );
            } else if largest_acked >= Some(packet.pn() + PACKET_THRESHOLD) {
                qtrace!(
                    "lost={}, is >= {PACKET_THRESHOLD} from largest acked {largest_acked:?}",
                    packet.pn()
                );
            } else {
                if largest_acked.is_some() {
                    self.first_ooo_time = Some(packet.time_sent());
                }
                // No more packets can be declared lost after this one.
                break;
            }

            if packet.declare_lost(now) {
                lost_packets.push(packet.clone());
            }
        }
    }
}

#[derive(Debug)]
pub struct LossRecoverySpaces {
    spaces: EnumMap<PacketNumberSpace, Option<LossRecoverySpace>>,
}

impl LossRecoverySpaces {
    /// Drop a packet number space and return all the packets that were
    /// outstanding, so that those can be marked as lost.
    ///
    /// # Panics
    ///
    /// If the space has already been removed.
    pub fn drop_space(&mut self, space: PacketNumberSpace) -> impl IntoIterator<Item = SentPacket> {
        let sp = self.spaces[space].take();
        assert_ne!(
            space,
            PacketNumberSpace::ApplicationData,
            "discarding application space"
        );
        sp.expect("has not been removed").remove_ignored()
    }

    #[must_use]
    pub fn get(&self, space: PacketNumberSpace) -> Option<&LossRecoverySpace> {
        self.spaces[space].as_ref()
    }

    pub fn get_mut(&mut self, space: PacketNumberSpace) -> Option<&mut LossRecoverySpace> {
        self.spaces[space].as_mut()
    }

    fn iter(&self) -> impl Iterator<Item = &LossRecoverySpace> {
        self.spaces.iter().filter_map(|(_, recvd)| recvd.as_ref())
    }

    fn iter_mut(&mut self) -> impl Iterator<Item = &mut LossRecoverySpace> {
        self.spaces
            .iter_mut()
            .filter_map(|(_, recvd)| recvd.as_mut())
    }
}

impl Default for LossRecoverySpaces {
    fn default() -> Self {
        Self {
            spaces: EnumMap::from_array([
                Some(LossRecoverySpace::new(PacketNumberSpace::Initial)),
                Some(LossRecoverySpace::new(PacketNumberSpace::Handshake)),
                Some(LossRecoverySpace::new(PacketNumberSpace::ApplicationData)),
            ]),
        }
    }
}

#[derive(Debug)]
struct PtoState {
    /// The packet number space that caused the PTO to fire.
    space: PacketNumberSpace,
    /// The number of probes that we have sent.
    count: usize,
    packets: usize,
    /// The complete set of packet number spaces that can have probes sent.
    probe: PacketNumberSpaceSet,
}

impl PtoState {
    /// The number of packets we send on a PTO.
    fn pto_packet_count(space: PacketNumberSpace) -> usize {
        if space == PacketNumberSpace::ApplicationData {
            MAX_PTO_PACKET_COUNT
        } else {
            // For the Initial and Handshake spaces, we only send one packet on PTO. This avoids
            // sending useless PING-only packets when only a single packet was lost, which is the
            // common case. These PINGs use cwnd and amplification window space, and sending them
            // hence makes the handshake more brittle.
            1
        }
    }

    pub fn new(space: PacketNumberSpace, probe: PacketNumberSpaceSet) -> Self {
        debug_assert!(probe.contains(space));
        Self {
            space,
            count: 1,
            packets: Self::pto_packet_count(space),
            probe,
        }
    }

    pub fn pto(&mut self, space: PacketNumberSpace, probe: PacketNumberSpaceSet) {
        debug_assert!(probe.contains(space));
        self.space = space;
        self.count += 1;
        self.packets = Self::pto_packet_count(space);
        self.probe = probe;
    }

    pub const fn count(&self) -> usize {
        self.count
    }

    pub fn count_pto(&self, stats: &mut Stats) {
        stats.add_pto_count(self.count);
    }

    /// Generate a sending profile, indicating what space it should be from.
    /// This takes a packet from the supply if one remains, or returns `None`.
    pub fn send_profile(&mut self, mtu: usize) -> Option<SendProfile> {
        (self.packets > 0).then(|| {
            // This is a PTO, so ignore the limit.
            self.packets -= 1;
            SendProfile::new_pto(self.space, mtu, self.probe)
        })
    }
}

#[derive(Debug)]
pub struct LossRecovery {
    /// When the handshake was confirmed, if it has been.
    confirmed_time: Option<Instant>,
    pto_state: Option<PtoState>,
    spaces: LossRecoverySpaces,
    qlog: NeqoQlog,
    stats: StatsCell,
    /// The factor by which the PTO period is reduced.
    /// This enables faster probing at a cost in additional lost packets.
    fast_pto: u8,
}

impl LossRecovery {
    #[must_use]
    pub fn new(stats: StatsCell, fast_pto: u8) -> Self {
        Self {
            confirmed_time: None,
            pto_state: None,
            spaces: LossRecoverySpaces::default(),
            qlog: NeqoQlog::default(),
            stats,
            fast_pto,
        }
    }

    #[must_use]
    pub fn largest_acknowledged_pn(&self, pn_space: PacketNumberSpace) -> Option<PacketNumber> {
        self.spaces.get(pn_space)?.largest_acked
    }

    pub fn set_qlog(&mut self, qlog: NeqoQlog) {
        self.qlog = qlog;
    }

    /// Drop all 0rtt packets.
    pub fn drop_0rtt(&mut self, primary_path: &PathRef, now: Instant) -> Vec<SentPacket> {
        let Some(sp) = self.spaces.get_mut(PacketNumberSpace::ApplicationData) else {
            return Vec::new();
        };
        if sp.largest_acked.is_some() {
            qwarn!("0-RTT packets already acknowledged, not dropping");
            return Vec::new();
        }
        let mut dropped = sp.remove_ignored().collect::<Vec<_>>();
        let mut path = primary_path.borrow_mut();
        for p in &mut dropped {
            path.discard_packet(p, now, &mut self.stats.borrow_mut());
        }
        dropped
    }

    pub fn on_packet_sent(&mut self, path: &PathRef, mut sent_packet: SentPacket, now: Instant) {
        let pn_space = PacketNumberSpace::from(sent_packet.packet_type());
        qtrace!("[{self}] packet {pn_space}-{} sent", sent_packet.pn());
        if let Some(space) = self.spaces.get_mut(pn_space) {
            path.borrow_mut().packet_sent(&mut sent_packet, now);
            space.on_packet_sent(sent_packet);
        } else {
            qwarn!(
                "[{self}] ignoring {pn_space}-{} from dropped space",
                sent_packet.pn()
            );
        }
    }

    /// Whether to probe the path.
    #[must_use]
    pub fn should_probe(&self, pto: Duration, now: Instant) -> bool {
        self.spaces
            .get(PacketNumberSpace::ApplicationData)
            .is_some_and(|sp| sp.should_probe(pto, now))
    }

    /// Record an RTT sample.
    fn rtt_sample(
        &self,
        rtt: &mut RttEstimate,
        send_time: Instant,
        now: Instant,
        ack_delay: Duration,
    ) {
        let source = if self.confirmed_time.is_some_and(|t| t < send_time) {
            RttSource::AckConfirmed
        } else {
            RttSource::Ack
        };
        if let Some(sample) = now.checked_duration_since(send_time) {
            rtt.update(&self.qlog, sample, ack_delay, source, now);
        }
    }

    const fn confirmed(&self) -> bool {
        self.confirmed_time.is_some()
    }

    /// Returns (acked packets, lost packets)
    pub fn on_ack_received<R>(
        &mut self,
        primary_path: &PathRef,
        pn_space: PacketNumberSpace,
        acked_ranges: R,
        ack_ecn: Option<ecn::Count>,
        ack_delay: Duration,
        now: Instant,
    ) -> (Vec<SentPacket>, Vec<SentPacket>)
    where
        R: IntoIterator<Item = RangeInclusive<PacketNumber>>,
        R::IntoIter: ExactSizeIterator,
    {
        let Some(space) = self.spaces.get_mut(pn_space) else {
            qinfo!("ACK on discarded space");
            return (Vec::new(), Vec::new());
        };

        let (acked_packets, any_ack_eliciting) =
            space.remove_acked(acked_ranges, &mut self.stats.borrow_mut());
        let Some(largest_acked_pkt) = acked_packets.first() else {
            // No new information.
            return (Vec::new(), Vec::new());
        };

        // Track largest PN acked per space
        let prev_largest_acked = space.largest_acked_sent_time;
        if Some(largest_acked_pkt.pn()) > space.largest_acked {
            space.largest_acked = Some(largest_acked_pkt.pn());

            // If the largest acknowledged is newly acked and any newly acked
            // packet was ack-eliciting, update the RTT. (-recovery 5.1)
            space.largest_acked_sent_time = Some(largest_acked_pkt.time_sent());
            if any_ack_eliciting && largest_acked_pkt.on_primary_path() {
                self.rtt_sample(
                    primary_path.borrow_mut().rtt_mut(),
                    largest_acked_pkt.time_sent(),
                    now,
                    ack_delay,
                );
            }
        }

        qdebug!(
            "[{self}] ACK for {pn_space:?} - largest_acked={}",
            largest_acked_pkt.pn()
        );

        // Perform loss detection.
        // PTO is used to remove lost packets from in-flight accounting.
        // We need to ensure that we have sent any PTO probes before they are removed
        // as we rely on the count of in-flight packets to determine whether to send
        // another probe.  Removing them too soon would result in not sending on PTO.
        let cleanup_delay = self.pto_period(primary_path.borrow().rtt());
        let Some(sp) = self.spaces.get_mut(pn_space) else {
            return (Vec::new(), Vec::new());
        };
        let loss_delay = primary_path.borrow().rtt().loss_delay();
        let mut lost = Vec::new();
        sp.detect_lost_packets(now, loss_delay, cleanup_delay, &mut lost);
        self.stats.borrow_mut().lost += lost.len();

        // Tell the congestion controller about any lost packets.
        // The PTO for congestion control is the raw number, without exponential
        // backoff, so that we can determine persistent congestion.
        primary_path.borrow_mut().on_packets_lost(
            prev_largest_acked,
            self.confirmed(),
            &lost,
            &mut self.stats.borrow_mut(),
            now,
        );

        // This must happen after on_packets_lost. If in recovery, this could
        // take us out, and then lost packets will start a new recovery period
        // when it shouldn't.
        primary_path.borrow_mut().on_packets_acked(
            &acked_packets,
            ack_ecn,
            now,
            &mut self.stats.borrow_mut(),
        );

        self.pto_state = None;

        (acked_packets, lost)
    }

    /// When receiving a retry, get all the sent packets so that they can be flushed.
    /// We also need to pretend that they never happened for the purposes of congestion control.
    pub fn retry(&mut self, primary_path: &PathRef, now: Instant) -> Vec<SentPacket> {
        self.pto_state = None;
        let mut dropped = self
            .spaces
            .iter_mut()
            .flat_map(LossRecoverySpace::remove_ignored)
            .collect::<Vec<_>>();
        let mut path = primary_path.borrow_mut();
        for p in &mut dropped {
            path.discard_packet(p, now, &mut self.stats.borrow_mut());
        }
        dropped
    }

    fn confirm(&mut self, rtt: &RttEstimate, now: Instant) {
        debug_assert!(self.confirmed_time.is_none());
        self.confirmed_time = Some(now);
        // Up until now, the ApplicationData space has been ignored for PTO.
        // So maybe fire a PTO.
        if let Some(pto) = self.pto_time(rtt, PacketNumberSpace::ApplicationData) {
            if pto < now {
                let probes = enum_set!(PacketNumberSpace::ApplicationData);
                self.fire_pto(PacketNumberSpace::ApplicationData, probes, now);
            }
        }
    }

    /// This function is called when the connection migrates.
    /// It marks all packets that are outstanding as having being sent on a non-primary path.
    /// This way failure to deliver on the old path doesn't count against the congestion
    /// control state on the new path and the RTT measurements don't apply either.
    pub fn migrate(&mut self) {
        for space in self.spaces.iter_mut() {
            space.migrate();
        }
    }

    /// Discard state for a given packet number space.
    pub fn discard(&mut self, primary_path: &PathRef, space: PacketNumberSpace, now: Instant) {
        qdebug!("[{self}] Reset loss recovery state for {space:?}");
        let mut path = primary_path.borrow_mut();
        for p in self.spaces.drop_space(space) {
            path.discard_packet(&p, now, &mut self.stats.borrow_mut());
        }

        // We just made progress, so discard PTO count.
        // The spec says that clients should not do this until confirming that
        // the server has completed address validation, but ignore that.
        self.pto_state = None;

        if space == PacketNumberSpace::Handshake {
            self.confirm(path.rtt(), now);
        }
    }

    /// Calculate when the next timeout is likely to be.  This is the earlier of the loss timer
    /// and the PTO timer; either or both might be disabled, so this can return `None`.
    #[must_use]
    pub fn next_timeout(&self, path: &Path) -> Option<Instant> {
        let rtt = path.rtt();
        let loss_time = self.earliest_loss_time(rtt);
        let pto_time = if path.pto_possible() {
            self.earliest_pto(rtt)
        } else {
            None
        };
        qtrace!("[{self}] next_timeout loss={loss_time:?} pto={pto_time:?}");
        match (loss_time, pto_time) {
            (Some(loss_time), Some(pto_time)) => Some(min(loss_time, pto_time)),
            (Some(loss_time), None) => Some(loss_time),
            (None, Some(pto_time)) => Some(pto_time),
            (None, None) => None,
        }
    }

    /// Find when the earliest sent packet should be considered lost.
    fn earliest_loss_time(&self, rtt: &RttEstimate) -> Option<Instant> {
        self.spaces
            .iter()
            .filter_map(LossRecoverySpace::loss_recovery_timer_start)
            .min()
            .map(|val| val + rtt.loss_delay())
    }

    /// Simple wrapper for the PTO calculation that avoids borrow check rules.
    fn pto_period_inner(
        rtt: &RttEstimate,
        pto_state: Option<&PtoState>,
        confirmed: bool,
        fast_pto: u8,
    ) -> Duration {
        // This is a complicated (but safe) way of calculating:
        //   base_pto * F * 2^pto_count
        // where F = fast_pto / FAST_PTO_SCALE (== 1 by default)
        let pto_count = pto_state.map_or(0, |p| u32::try_from(p.count).unwrap_or(0));
        rtt.pto(confirmed)
            .checked_mul(u32::from(fast_pto) << min(pto_count, u32::BITS - u8::BITS))
            .map_or(Duration::from_secs(3600), |p| p / u32::from(FAST_PTO_SCALE))
    }

    /// Get the current PTO period for the given packet number space.
    /// Unlike calling `RttEstimate::pto` directly, this includes exponential backoff.
    fn pto_period(&self, rtt: &RttEstimate) -> Duration {
        Self::pto_period_inner(
            rtt,
            self.pto_state.as_ref(),
            self.confirmed(),
            self.fast_pto,
        )
    }

    // Calculate PTO time for the given space.
    fn pto_time(&self, rtt: &RttEstimate, pn_space: PacketNumberSpace) -> Option<Instant> {
        self.spaces
            .get(pn_space)?
            .pto_base_time()
            .map(|t| t + self.pto_period(rtt))
    }

    /// Find the earliest PTO time for all active packet number spaces.
    /// Ignore Application if either Initial or Handshake have an active PTO.
    fn earliest_pto(&self, rtt: &RttEstimate) -> Option<Instant> {
        if self.confirmed() {
            self.pto_time(rtt, PacketNumberSpace::ApplicationData)
        } else {
            self.pto_time(rtt, PacketNumberSpace::Initial)
                .iter()
                .chain(self.pto_time(rtt, PacketNumberSpace::Handshake).iter())
                .min()
                .copied()
        }
    }

    fn fire_pto(
        &mut self,
        pn_space: PacketNumberSpace,
        allow_probes: PacketNumberSpaceSet,
        now: Instant,
    ) {
        if let Some(st) = &mut self.pto_state {
            st.pto(pn_space, allow_probes);
        } else {
            self.pto_state = Some(PtoState::new(pn_space, allow_probes));
        }

        if let Some(st) = &mut self.pto_state {
            st.count_pto(&mut self.stats.borrow_mut());
            qlog::metrics_updated(&self.qlog, &[QlogMetric::PtoCount(st.count())], now);
        }
    }

    /// This checks whether the PTO timer has fired and fires it if needed.
    /// When it has, mark packets as "lost" for the purposes of having frames
    /// regenerated in subsequent packets.  The packets aren't truly lost, so
    /// we have to clone the `SentPacket` instance.
    fn maybe_fire_pto(&mut self, primary_path: &PathRef, now: Instant, lost: &mut Vec<SentPacket>) {
        let mut pto_space = None;
        // The spaces in which we will allow probing.
        let mut allow_probes = PacketNumberSpaceSet::default();
        for pn_space in PacketNumberSpace::iter() {
            let Some(t) = self.pto_time(primary_path.borrow().rtt(), pn_space) else {
                continue;
            };
            allow_probes.insert(pn_space);
            if t > now {
                continue;
            }
            qdebug!("[{self}] PTO timer fired for {pn_space:?}");
            let Some(space) = self.spaces.get_mut(pn_space) else {
                continue;
            };
            let mut size = 0;
            let mtu = primary_path.borrow().plpmtu();
            lost.extend(
                space
                    .pto_packets()
                    // Do not consider all packets for retransmission on PTO. On
                    // a high bandwidth delay connection, that would be a lot of
                    // `SentPacket`s to clone.
                    //
                    // Given that we are sending at most `MAX_PTO_PACKET_COUNT`
                    // packets on PTO, consider as many packets for
                    // retransmission as would fit into those PTO packets.
                    .take_while(move |p| {
                        size += p.len();
                        size <= MAX_PTO_PACKET_COUNT * mtu
                    })
                    .cloned(),
            );
            pto_space = pto_space.or(Some(pn_space));
        }

        // This has to happen outside the loop. Increasing the PTO count here causes the
        // pto_time to increase which might cause PTO for later packet number spaces to not fire.
        if let Some(pn_space) = pto_space {
            qtrace!("[{self}] PTO {pn_space}, probing {allow_probes:?}");
            self.fire_pto(pn_space, allow_probes, now);
        }
    }

    pub fn timeout(&mut self, primary_path: &PathRef, now: Instant) -> Vec<SentPacket> {
        qtrace!("[{self}] timeout {now:?}");

        let loss_delay = primary_path.borrow().rtt().loss_delay();
        let confirmed = self.confirmed();

        let mut lost_packets = Vec::new();
        for space in self.spaces.iter_mut() {
            let first = lost_packets.len(); // The first packet lost in this space.
            let pto = Self::pto_period_inner(
                primary_path.borrow().rtt(),
                self.pto_state.as_ref(),
                confirmed,
                self.fast_pto,
            );
            space.detect_lost_packets(now, loss_delay, pto, &mut lost_packets);

            primary_path.borrow_mut().on_packets_lost(
                space.largest_acked_sent_time,
                confirmed,
                &lost_packets[first..],
                &mut self.stats.borrow_mut(),
                now,
            );
        }
        self.stats.borrow_mut().lost += lost_packets.len();

        self.maybe_fire_pto(primary_path, now, &mut lost_packets);
        lost_packets
    }

    /// Check how packets should be sent, based on whether there is a PTO,
    /// what the current congestion window is, and what the pacer says.
    #[expect(clippy::option_if_let_else, reason = "Alternative is less readable.")]
    pub fn send_profile(&mut self, path: &Path, now: Instant) -> SendProfile {
        qtrace!("[{self}] get send profile {now:?}");
        let sender = path.sender();
        let mtu = path.plpmtu();
        #[allow(
            clippy::allow_attributes,
            clippy::return_and_then,
            reason = "TODO: False positive on nightly; function isn't returning Option or Result"
        )]
        if let Some(profile) = self
            .pto_state
            .as_mut()
            .and_then(|pto| pto.send_profile(mtu))
        {
            profile
        } else {
            let limit = min(sender.cwnd_avail(), path.amplification_limit());
            if limit > mtu {
                // More than an MTU available; we might need to pace.
                if sender
                    .next_paced(path.rtt().estimate())
                    .is_some_and(|t| t > now)
                {
                    SendProfile::new_paced()
                } else {
                    SendProfile::new_limited(mtu)
                }
            } else if sender.recovery_packet() {
                // After entering recovery, allow a packet to be sent immediately.
                // This uses the PTO machinery, probing in all spaces. This will
                // result in a PING being sent in every active space.
                SendProfile::new_pto(PacketNumberSpace::Initial, mtu, PacketNumberSpaceSet::all())
            } else {
                SendProfile::new_limited(limit)
            }
        }
    }
}

impl Display for LossRecovery {
    fn fmt(&self, f: &mut Formatter) -> fmt::Result {
        write!(f, "LossRecovery")
    }
}

#[cfg(test)]
mod tests {
    use std::{
        cell::RefCell,
        ops::{Deref, DerefMut, RangeInclusive},
        rc::Rc,
        time::{Duration, Instant},
    };

    use neqo_common::qlog::NeqoQlog;
    use test_fixture::{now, DEFAULT_ADDR};

    use super::{
        LossRecovery, LossRecoverySpace, PacketNumberSpace, SendProfile, SentPacket, FAST_PTO_SCALE,
    };
    use crate::{
        cid::{ConnectionId, ConnectionIdEntry},
        ecn,
        packet::{PacketNumber, PacketType},
        path::{Path, PathRef},
        stats::{Stats, StatsCell},
        ConnectionParameters,
    };

    // Shorthand for a time in milliseconds.
    const fn ms(t: u64) -> Duration {
        Duration::from_millis(t)
    }

    const ON_SENT_SIZE: usize = 100;
    /// An initial RTT for using with `setup_lr`.
    const TEST_RTT: Duration = ms(7000);
    const TEST_RTTVAR: Duration = ms(3500);

    struct Fixture {
        lr: LossRecovery,
        path: PathRef,
    }

    // This shadows functions on the base object so that the path and RTT estimator
    // is used consistently in the tests.  It also simplifies the function signatures.
    impl Fixture {
        pub fn on_ack_received(
            &mut self,
            pn_space: PacketNumberSpace,
            acked_ranges: Vec<RangeInclusive<PacketNumber>>,
            ack_ecn: Option<ecn::Count>,
            ack_delay: Duration,
            now: Instant,
        ) -> (Vec<SentPacket>, Vec<SentPacket>) {
            self.lr
                .on_ack_received(&self.path, pn_space, acked_ranges, ack_ecn, ack_delay, now)
        }

        pub fn on_packet_sent(&mut self, sent_packet: SentPacket, now: Instant) {
            self.lr.on_packet_sent(&self.path, sent_packet, now);
        }

        pub fn timeout(&mut self, now: Instant) -> Vec<SentPacket> {
            self.lr.timeout(&self.path, now)
        }

        pub fn next_timeout(&self) -> Option<Instant> {
            self.lr.next_timeout(&self.path.borrow())
        }

        pub fn discard(&mut self, space: PacketNumberSpace, now: Instant) {
            self.lr.discard(&self.path, space, now);
        }

        pub fn pto_time(&self, space: PacketNumberSpace) -> Option<Instant> {
            self.lr.pto_time(self.path.borrow().rtt(), space)
        }

        pub fn send_profile(&mut self, now: Instant) -> SendProfile {
            self.lr.send_profile(&self.path.borrow(), now)
        }
    }

    impl Default for Fixture {
        fn default() -> Self {
            let stats = StatsCell::default();
            let mut path = Path::temporary(
                DEFAULT_ADDR,
                DEFAULT_ADDR,
                &ConnectionParameters::default(),
                NeqoQlog::default(),
                now(),
                &mut stats.borrow_mut(),
            );
            path.make_permanent(
                None,
                ConnectionIdEntry::new(0, ConnectionId::from(&[1, 2, 3]), [0; 16]),
            );
            path.set_primary(true, now());
            path.rtt_mut().set_initial(TEST_RTT);
            Self {
                lr: LossRecovery::new(stats, FAST_PTO_SCALE),
                path: Rc::new(RefCell::new(path)),
            }
        }
    }

    // Most uses of the fixture only care about the loss recovery piece,
    // but the internal functions need the other bits.
    impl Deref for Fixture {
        type Target = LossRecovery;
        fn deref(&self) -> &Self::Target {
            &self.lr
        }
    }

    impl DerefMut for Fixture {
        fn deref_mut(&mut self) -> &mut Self::Target {
            &mut self.lr
        }
    }

    fn assert_rtts(
        lr: &Fixture,
        latest_rtt: Duration,
        smoothed_rtt: Duration,
        rttvar: Duration,
        min_rtt: Duration,
    ) {
        let p = lr.path.borrow();
        let rtt = p.rtt();
        println!(
            "rtts: {:?} {:?} {:?} {:?}",
            rtt.latest(),
            rtt.estimate(),
            rtt.rttvar(),
            rtt.minimum(),
        );
        assert_eq!(rtt.latest(), latest_rtt, "latest RTT");
        assert_eq!(rtt.estimate(), smoothed_rtt, "smoothed RTT");
        assert_eq!(rtt.rttvar(), rttvar, "RTT variance");
        assert_eq!(rtt.minimum(), min_rtt, "min RTT");
    }

    fn assert_sent_times(
        lr: &Fixture,
        initial: Option<Instant>,
        handshake: Option<Instant>,
        app_data: Option<Instant>,
    ) {
        let est = |sp| {
            lr.spaces
                .get(sp)
                .and_then(LossRecoverySpace::loss_recovery_timer_start)
        };
        println!(
            "loss times: {:?} {:?} {:?}",
            est(PacketNumberSpace::Initial),
            est(PacketNumberSpace::Handshake),
            est(PacketNumberSpace::ApplicationData),
        );
        assert_eq!(
            est(PacketNumberSpace::Initial),
            initial,
            "Initial earliest sent time"
        );
        assert_eq!(
            est(PacketNumberSpace::Handshake),
            handshake,
            "Handshake earliest sent time"
        );
        assert_eq!(
            est(PacketNumberSpace::ApplicationData),
            app_data,
            "AppData earliest sent time"
        );
    }

    fn assert_no_sent_times(lr: &Fixture) {
        assert_sent_times(lr, None, None, None);
    }

    // In most of the tests below, packets are sent at a fixed cadence, with PACING between each.
    const PACING: Duration = ms(7);
    fn pn_time(pn: u64) -> Instant {
        now() + (PACING * pn.try_into().unwrap())
    }

    fn pace(lr: &mut Fixture, count: u64) {
        for pn in 0..count {
            lr.on_packet_sent(
                SentPacket::new(
                    PacketType::Short,
                    pn,
                    pn_time(pn),
                    true,
                    Vec::new(),
                    ON_SENT_SIZE,
                ),
                Instant::now(),
            );
        }
    }

    const ACK_DELAY: Duration = ms(24);
    /// Acknowledge PN with the identified delay.
    fn ack(lr: &mut Fixture, pn: u64, delay: Duration) {
        lr.on_ack_received(
            PacketNumberSpace::ApplicationData,
            vec![pn..=pn],
            None,
            ACK_DELAY,
            pn_time(pn) + delay,
        );
    }

    fn add_sent(lrs: &mut LossRecoverySpace, max_pn: PacketNumber) {
        for pn in 0..=max_pn {
            lrs.on_packet_sent(SentPacket::new(
                PacketType::Short,
                pn,
                pn_time(pn),
                true,
                Vec::new(),
                ON_SENT_SIZE,
            ));
        }
    }

    fn match_acked(acked: &[SentPacket], expected: &[PacketNumber]) {
        assert_eq!(
            acked.iter().map(SentPacket::pn).collect::<Vec<_>>(),
            expected
        );
    }

    #[test]
    fn remove_acked() {
        let mut lrs = LossRecoverySpace::new(PacketNumberSpace::ApplicationData);
        let mut stats = Stats::default();
        add_sent(&mut lrs, 10);
        let (acked, _) = lrs.remove_acked(vec![], &mut stats);
        assert!(acked.is_empty());
        let (acked, _) = lrs.remove_acked(vec![7..=8, 2..=4], &mut stats);
        match_acked(&acked, &[8, 7, 4, 3, 2]);
        let (acked, _) = lrs.remove_acked(vec![8..=11], &mut stats);
        match_acked(&acked, &[10, 9]);
        let (acked, _) = lrs.remove_acked(vec![0..=2], &mut stats);
        match_acked(&acked, &[1, 0]);
        let (acked, _) = lrs.remove_acked(vec![5..=6], &mut stats);
        match_acked(&acked, &[6, 5]);
    }

    #[test]
    fn initial_rtt() {
        let mut lr = Fixture::default();
        pace(&mut lr, 1);
        let rtt = ms(100);
        ack(&mut lr, 0, rtt);
        assert_rtts(&lr, rtt, rtt, rtt / 2, rtt);
        assert_no_sent_times(&lr);
    }

    /// Send `n` packets (using PACING), then acknowledge the first.
    fn setup_lr(n: u64) -> Fixture {
        let mut lr = Fixture::default();
        pace(&mut lr, n);
        ack(&mut lr, 0, TEST_RTT);
        assert_rtts(&lr, TEST_RTT, TEST_RTT, TEST_RTTVAR, TEST_RTT);
        assert_no_sent_times(&lr);
        lr
    }

    // The ack delay is removed from any RTT estimate.
    #[test]
    fn ack_delay_adjusted() {
        let mut lr = setup_lr(2);
        ack(&mut lr, 1, TEST_RTT + ACK_DELAY);
        // RTT stays the same, but the RTTVAR is adjusted downwards.
        assert_rtts(&lr, TEST_RTT, TEST_RTT, TEST_RTTVAR * 3 / 4, TEST_RTT);
        assert_no_sent_times(&lr);
    }

    // The ack delay is ignored when it would cause a sample to be less than min_rtt.
    #[test]
    fn ack_delay_ignored() {
        let mut lr = setup_lr(2);
        let extra = ms(8);
        assert!(extra < ACK_DELAY);
        ack(&mut lr, 1, TEST_RTT + extra);
        let expected_rtt = TEST_RTT + (extra / 8);
        let expected_rttvar = (TEST_RTTVAR * 3 + extra) / 4;
        assert_rtts(
            &lr,
            TEST_RTT + extra,
            expected_rtt,
            expected_rttvar,
            TEST_RTT,
        );
        assert_no_sent_times(&lr);
    }

    // A lower observed RTT is used as min_rtt (and ack delay is ignored).
    #[test]
    fn reduce_min_rtt() {
        let mut lr = setup_lr(2);
        let delta = ms(4);
        let reduced_rtt = TEST_RTT - delta;
        ack(&mut lr, 1, reduced_rtt);
        let expected_rtt = TEST_RTT - (delta / 8);
        let expected_rttvar = (TEST_RTTVAR * 3 + delta) / 4;
        assert_rtts(&lr, reduced_rtt, expected_rtt, expected_rttvar, reduced_rtt);
        assert_no_sent_times(&lr);
    }

    // Acknowledging something again has no effect.
    #[test]
    fn no_new_acks() {
        let mut lr = setup_lr(1);
        let check = |lr: &Fixture| {
            assert_rtts(lr, TEST_RTT, TEST_RTT, TEST_RTTVAR, TEST_RTT);
            assert_no_sent_times(lr);
        };
        check(&lr);

        ack(&mut lr, 0, ms(1339)); // much delayed ACK
        check(&lr);

        ack(&mut lr, 0, ms(3)); // time travel!
        check(&lr);
    }

    // Test time loss detection as part of handling a regular ACK.
    #[test]
    fn time_loss_detection_gap() {
        let mut lr = Fixture::default();
        // Create a single packet gap, and have pn 0 time out.
        // This can't use the default pacing, which is too tight.
        // So send two packets with 1/4 RTT between them.  Acknowledge pn 1 after 1 RTT.
        // pn 0 should then be marked lost because it is then outstanding for 5RTT/4
        // the loss time for packets is 9RTT/8.
        lr.on_packet_sent(
            SentPacket::new(
                PacketType::Short,
                0,
                pn_time(0),
                true,
                Vec::new(),
                ON_SENT_SIZE,
            ),
            Instant::now(),
        );
        lr.on_packet_sent(
            SentPacket::new(
                PacketType::Short,
                1,
                pn_time(0) + TEST_RTT / 4,
                true,
                Vec::new(),
                ON_SENT_SIZE,
            ),
            Instant::now(),
        );
        let (_, lost) = lr.on_ack_received(
            PacketNumberSpace::ApplicationData,
            vec![1..=1],
            None,
            ACK_DELAY,
            pn_time(0) + (TEST_RTT * 5 / 4),
        );
        assert_eq!(lost.len(), 1);
        assert_no_sent_times(&lr);
    }

    // Test time loss detection as part of an explicit timeout.
    #[test]
    fn time_loss_detection_timeout() {
        let mut lr = setup_lr(3);

        // We want to declare PN 2 as acknowledged before we declare PN 1 as lost.
        // For this to work, we need PACING above to be less than 1/8 of an RTT.
        let pn1_sent_time = pn_time(1);
        let pn1_loss_time = pn1_sent_time + (TEST_RTT * 9 / 8);
        let pn2_ack_time = pn_time(2) + TEST_RTT;
        assert!(pn1_loss_time > pn2_ack_time);

        let (_, lost) = lr.on_ack_received(
            PacketNumberSpace::ApplicationData,
            vec![2..=2],
            None,
            ACK_DELAY,
            pn2_ack_time,
        );
        assert!(lost.is_empty());
        // Run the timeout function here to force time-based loss recovery to be enabled.
        let lost = lr.timeout(pn2_ack_time);
        assert!(lost.is_empty());
        assert_sent_times(&lr, None, None, Some(pn1_sent_time));

        // After time elapses, pn 1 is marked lost.
        let callback_time = lr.next_timeout();
        assert_eq!(callback_time, Some(pn1_loss_time));
        let packets = lr.timeout(pn1_loss_time);
        assert_eq!(packets.len(), 1);
        // Checking for expiration with zero delay lets us check the loss time.
        assert!(packets[0].expired(pn1_loss_time, Duration::new(0, 0)));
        assert_no_sent_times(&lr);
    }

    #[test]
    fn big_gap_loss() {
        let mut lr = setup_lr(5); // This sends packets 0-4 and acknowledges pn 0.

        // Acknowledge just 2-4, which will cause pn 1 to be marked as lost.
        assert_eq!(super::PACKET_THRESHOLD, 3);
        let (_, lost) = lr.on_ack_received(
            PacketNumberSpace::ApplicationData,
            vec![2..=4],
            None,
            ACK_DELAY,
            pn_time(4),
        );
        assert_eq!(lost.len(), 1);
    }

    #[test]
    #[should_panic(expected = "discarding application space")]
    fn drop_app() {
        let mut lr = Fixture::default();
        lr.discard(PacketNumberSpace::ApplicationData, now());
    }

    #[test]
    fn ack_after_drop() {
        let mut lr = Fixture::default();
        lr.discard(PacketNumberSpace::Initial, now());
        let (acked, lost) = lr.on_ack_received(
            PacketNumberSpace::Initial,
            vec![],
            None,
            Duration::from_millis(0),
            pn_time(0),
        );
        assert!(acked.is_empty());
        assert!(lost.is_empty());
    }

    #[test]
    fn drop_spaces() {
        let mut lr = Fixture::default();
        lr.on_packet_sent(
            SentPacket::new(
                PacketType::Initial,
                0,
                pn_time(0),
                true,
                Vec::new(),
                ON_SENT_SIZE,
            ),
            Instant::now(),
        );
        lr.on_packet_sent(
            SentPacket::new(
                PacketType::Handshake,
                0,
                pn_time(1),
                true,
                Vec::new(),
                ON_SENT_SIZE,
            ),
            Instant::now(),
        );
        lr.on_packet_sent(
            SentPacket::new(
                PacketType::Short,
                0,
                pn_time(2),
                true,
                Vec::new(),
                ON_SENT_SIZE,
            ),
            Instant::now(),
        );

        // Now put all spaces on the LR timer so we can see them.
        for sp in &[
            PacketType::Initial,
            PacketType::Handshake,
            PacketType::Short,
        ] {
            let sent_pkt = SentPacket::new(*sp, 1, pn_time(3), true, Vec::new(), ON_SENT_SIZE);
            let pn_space = PacketNumberSpace::from(sent_pkt.packet_type());
            lr.on_packet_sent(sent_pkt, Instant::now());
            lr.on_ack_received(
                pn_space,
                vec![1..=1],
                None,
                Duration::from_secs(0),
                pn_time(3),
            );
            let mut lost = Vec::new();
            lr.spaces.get_mut(pn_space).unwrap().detect_lost_packets(
                pn_time(3),
                TEST_RTT,
                TEST_RTT * 3, // unused
                &mut lost,
            );
            assert!(lost.is_empty());
        }

        lr.discard(PacketNumberSpace::Initial, pn_time(3));
        assert_sent_times(&lr, None, Some(pn_time(1)), Some(pn_time(2)));

        lr.discard(PacketNumberSpace::Handshake, pn_time(3));
        assert_sent_times(&lr, None, None, Some(pn_time(2)));

        // There are cases where we send a packet that is not subsequently tracked.
        // So check that this works.
        lr.on_packet_sent(
            SentPacket::new(
                PacketType::Initial,
                0,
                pn_time(3),
                true,
                Vec::new(),
                ON_SENT_SIZE,
            ),
            Instant::now(),
        );
        assert_sent_times(&lr, None, None, Some(pn_time(2)));
    }

    #[test]
    fn rearm_pto_after_confirmed() {
        let mut lr = Fixture::default();
        lr.on_packet_sent(
            SentPacket::new(
                PacketType::Initial,
                0,
                now(),
                true,
                Vec::new(),
                ON_SENT_SIZE,
            ),
            Instant::now(),
        );
        // Set the RTT to the initial value so that discarding doesn't
        // alter the estimate.
        let rtt = lr.path.borrow().rtt().estimate();
        lr.on_ack_received(
            PacketNumberSpace::Initial,
            vec![0..=0],
            None,
            Duration::new(0, 0),
            now() + rtt,
        );

        lr.on_packet_sent(
            SentPacket::new(
                PacketType::Handshake,
                0,
                now(),
                true,
                Vec::new(),
                ON_SENT_SIZE,
            ),
            Instant::now(),
        );
        lr.on_packet_sent(
            SentPacket::new(PacketType::Short, 0, now(), true, Vec::new(), ON_SENT_SIZE),
            Instant::now(),
        );

        assert!(lr.pto_time(PacketNumberSpace::ApplicationData).is_some());
        lr.discard(PacketNumberSpace::Initial, pn_time(1));
        assert!(lr.pto_time(PacketNumberSpace::ApplicationData).is_some());

        // Expiring state after the PTO on the ApplicationData space has
        // expired should result in setting a PTO state.
        let default_pto = lr.path.borrow().rtt().pto(true);
        let expected_pto = pn_time(2) + default_pto;
        lr.discard(PacketNumberSpace::Handshake, expected_pto);
        let profile = lr.send_profile(now());
        assert!(profile.pto.is_some());
        assert!(!profile.should_probe(PacketNumberSpace::Initial));
        assert!(!profile.should_probe(PacketNumberSpace::Handshake));
        assert!(profile.should_probe(PacketNumberSpace::ApplicationData));
    }

    #[test]
    fn no_pto_if_amplification_limited() {
        let mut lr = Fixture::default();
        // Eat up the amplification limit by telling the path that we've sent a giant packet.
        {
            const SPARE: usize = 10;
            let mut path = lr.path.borrow_mut();
            let limit = path.amplification_limit();
            path.add_sent(limit - SPARE);
            assert_eq!(path.amplification_limit(), SPARE);
        }

        lr.on_packet_sent(
            SentPacket::new(
                PacketType::Initial,
                0,
                now(),
                true,
                Vec::new(),
                ON_SENT_SIZE,
            ),
            Instant::now(),
        );

        let handshake_pto = lr.path.borrow().rtt().pto(false);
        let expected_pto = now() + handshake_pto;
        assert_eq!(lr.pto_time(PacketNumberSpace::Initial), Some(expected_pto));
        let profile = lr.send_profile(now());
        assert!(profile.ack_only(PacketNumberSpace::Initial));
        assert!(profile.pto.is_none());
        assert!(!profile.should_probe(PacketNumberSpace::Initial));
        assert!(!profile.should_probe(PacketNumberSpace::Handshake));
        assert!(!profile.should_probe(PacketNumberSpace::ApplicationData));
    }
}
