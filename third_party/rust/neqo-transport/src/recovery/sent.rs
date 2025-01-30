// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

// A collection for sent packets.

use std::{
    collections::BTreeMap,
    ops::RangeInclusive,
    time::{Duration, Instant},
};

use neqo_common::IpTosEcn;

use crate::{
    packet::{PacketNumber, PacketType},
    recovery::RecoveryToken,
};

#[derive(Debug, Clone)]
pub struct SentPacket {
    pt: PacketType,
    pn: PacketNumber,
    ecn_mark: IpTosEcn,
    ack_eliciting: bool,
    time_sent: Instant,
    primary_path: bool,
    tokens: Vec<RecoveryToken>,

    time_declared_lost: Option<Instant>,
    /// After a PTO, this is true when the packet has been released.
    pto: bool,

    len: usize,
}

impl SentPacket {
    #[must_use]
    pub const fn new(
        pt: PacketType,
        pn: PacketNumber,
        ecn_mark: IpTosEcn,
        time_sent: Instant,
        ack_eliciting: bool,
        tokens: Vec<RecoveryToken>,
        len: usize,
    ) -> Self {
        Self {
            pt,
            pn,
            ecn_mark,
            time_sent,
            ack_eliciting,
            primary_path: true,
            tokens,
            time_declared_lost: None,
            pto: false,
            len,
        }
    }

    /// The type of this packet.
    #[must_use]
    pub const fn packet_type(&self) -> PacketType {
        self.pt
    }

    /// The number of the packet.
    #[must_use]
    pub const fn pn(&self) -> PacketNumber {
        self.pn
    }

    /// The ECN mark of the packet.
    #[must_use]
    pub const fn ecn_mark(&self) -> IpTosEcn {
        self.ecn_mark
    }

    /// The time that this packet was sent.
    #[must_use]
    pub const fn time_sent(&self) -> Instant {
        self.time_sent
    }

    /// Returns `true` if the packet will elicit an ACK.
    #[must_use]
    pub const fn ack_eliciting(&self) -> bool {
        self.ack_eliciting
    }

    /// Returns `true` if the packet was sent on the primary path.
    #[must_use]
    pub const fn on_primary_path(&self) -> bool {
        self.primary_path
    }

    /// The length of the packet that was sent.
    #[allow(clippy::len_without_is_empty)]
    #[must_use]
    pub const fn len(&self) -> usize {
        self.len
    }

    /// Access the recovery tokens that this holds.
    #[must_use]
    pub fn tokens(&self) -> &[RecoveryToken] {
        &self.tokens
    }

    /// Clears the flag that had this packet on the primary path.
    /// Used when migrating to clear out state.
    pub fn clear_primary_path(&mut self) {
        self.primary_path = false;
    }

    /// For Initial packets, it is possible that the packet builder needs to amend the length.
    pub fn track_padding(&mut self, padding: usize) {
        debug_assert_eq!(self.pt, PacketType::Initial);
        self.len += padding;
    }

    /// Whether the packet has been declared lost.
    #[must_use]
    pub const fn lost(&self) -> bool {
        self.time_declared_lost.is_some()
    }

    /// Whether accounting for the loss or acknowledgement in the
    /// congestion controller is pending.
    /// Returns `true` if the packet counts as being "in flight",
    /// and has not previously been declared lost.
    /// Note that this should count packets that contain only ACK and PADDING,
    /// but we don't send PADDING, so we don't track that.
    #[must_use]
    pub const fn cc_outstanding(&self) -> bool {
        self.ack_eliciting() && self.on_primary_path() && !self.lost()
    }

    /// Whether the packet should be tracked as in-flight.
    #[must_use]
    pub const fn cc_in_flight(&self) -> bool {
        self.ack_eliciting() && self.on_primary_path()
    }

    /// Declare the packet as lost.  Returns `true` if this is the first time.
    pub fn declare_lost(&mut self, now: Instant) -> bool {
        if self.lost() {
            false
        } else {
            self.time_declared_lost = Some(now);
            true
        }
    }

    /// Ask whether this tracked packet has been declared lost for long enough
    /// that it can be expired and no longer tracked.
    #[must_use]
    pub fn expired(&self, now: Instant, expiration_period: Duration) -> bool {
        self.time_declared_lost
            .is_some_and(|loss_time| (loss_time + expiration_period) <= now)
    }

    /// Whether the packet contents were cleared out after a PTO.
    #[must_use]
    pub const fn pto_fired(&self) -> bool {
        self.pto
    }

    /// On PTO, we need to get the recovery tokens so that we can ensure that
    /// the frames we sent can be sent again in the PTO packet(s).  Do that just once.
    #[must_use]
    pub fn pto(&mut self) -> bool {
        if self.pto || self.lost() {
            false
        } else {
            self.pto = true;
            true
        }
    }
}

/// A collection for packets that we have sent that haven't been acknowledged.
#[derive(Debug, Default)]
pub struct SentPackets {
    /// The collection.
    packets: BTreeMap<u64, SentPacket>,
}

impl SentPackets {
    #[allow(clippy::len_without_is_empty)]
    #[must_use]
    pub fn len(&self) -> usize {
        self.packets.len()
    }

    pub fn track(&mut self, packet: SentPacket) {
        self.packets.insert(packet.pn, packet);
    }

    pub fn iter_mut(&mut self) -> impl Iterator<Item = &mut SentPacket> {
        self.packets.values_mut()
    }

    /// Take values from specified ranges of packet numbers.
    /// The values returned will be reversed, so that the most recent packet appears first.
    /// This is because ACK frames arrive with ranges starting from the largest acknowledged
    /// and we want to match that.
    pub fn take_ranges<R>(&mut self, acked_ranges: R) -> Vec<SentPacket>
    where
        R: IntoIterator<Item = RangeInclusive<PacketNumber>>,
        R::IntoIter: ExactSizeIterator,
    {
        let mut result = Vec::new();

        // Start with all packets. We will add unacknowledged packets back.
        //  [---------------------------packets----------------------------]
        let mut packets = std::mem::take(&mut self.packets);

        let mut previous_range_start: Option<PacketNumber> = None;

        for range in acked_ranges {
            // Split off at the end of the acked range.
            //
            //  [---------packets--------][----------after_acked_range---------]
            let after_acked_range = packets.split_off(&(*range.end() + 1));

            // Split off at the start of the acked range.
            //
            //  [-packets-][-acked_range-][----------after_acked_range---------]
            let acked_range = packets.split_off(range.start());

            // According to RFC 9000 19.3.1 ACK ranges are in descending order:
            //
            // > Each ACK Range consists of alternating Gap and ACK Range Length
            // > values in **descending packet number order**.
            //
            // <https://www.rfc-editor.org/rfc/rfc9000.html#section-19.3.1>
            debug_assert!(previous_range_start.map_or(true, |s| s > *range.end()));
            previous_range_start = Some(*range.start());

            // Thus none of the following ACK ranges will acknowledge packets in
            // `after_acked_range`. Let's put those back early.
            //
            //  [-packets-][-acked_range-][------------self.packets------------]
            if self.packets.is_empty() {
                // Don't re-insert un-acked packets into empty collection, but
                // instead replace the empty one entirely.
                self.packets = after_acked_range;
            } else {
                // Need to extend existing one. Not the first iteration, thus
                // `after_acked_range` should be small.
                self.packets.extend(after_acked_range);
            }

            // Take the acked packets.
            result.extend(acked_range.into_values().rev());
        }

        // Put remaining non-acked packets back.
        //
        // This is inefficient if the acknowledged packets include the last sent
        // packet AND there is a large unacknowledged span of packets. That's
        // rare enough that we won't do anything special for that case.
        self.packets.extend(packets);

        result
    }

    /// Empty out the packets, but keep the offset.
    pub fn drain_all(&mut self) -> impl Iterator<Item = SentPacket> {
        std::mem::take(&mut self.packets).into_values()
    }

    /// See `LossRecoverySpace::remove_old_lost` for details on `now` and `cd`.
    /// Returns the number of ack-eliciting packets removed.
    pub fn remove_expired(&mut self, now: Instant, cd: Duration) -> usize {
        let mut it = self.packets.iter();
        // If the first item is not expired, do nothing (the most common case).
        if it.next().is_some_and(|(_, p)| p.expired(now, cd)) {
            // Find the index of the first unexpired packet.
            let to_remove = if let Some(first_keep) =
                it.find_map(|(i, p)| if p.expired(now, cd) { None } else { Some(*i) })
            {
                // Some packets haven't expired, so keep those.
                let keep = self.packets.split_off(&first_keep);
                std::mem::replace(&mut self.packets, keep)
            } else {
                // All packets are expired.
                std::mem::take(&mut self.packets)
            };
            to_remove
                .into_values()
                .filter(SentPacket::ack_eliciting)
                .count()
        } else {
            0
        }
    }
}

#[cfg(test)]
mod tests {
    use std::{
        cell::OnceCell,
        convert::TryFrom,
        time::{Duration, Instant},
    };

    use neqo_common::IpTosEcn;

    use super::{SentPacket, SentPackets};
    use crate::packet::{PacketNumber, PacketType};

    const PACKET_GAP: Duration = Duration::from_secs(1);
    fn start_time() -> Instant {
        thread_local!(static STARTING_TIME: OnceCell<Instant> = const { OnceCell::new() });
        STARTING_TIME.with(|t| *t.get_or_init(Instant::now))
    }

    fn pkt(n: u32) -> SentPacket {
        SentPacket::new(
            PacketType::Short,
            PacketNumber::from(n),
            IpTosEcn::default(),
            start_time() + (PACKET_GAP * n),
            true,
            Vec::new(),
            100,
        )
    }

    fn pkts() -> SentPackets {
        let mut pkts = SentPackets::default();
        pkts.track(pkt(0));
        pkts.track(pkt(1));
        pkts.track(pkt(2));
        assert_eq!(pkts.len(), 3);
        pkts
    }

    trait HasPacketNumber {
        fn pn(&self) -> PacketNumber;
    }
    impl HasPacketNumber for SentPacket {
        fn pn(&self) -> PacketNumber {
            self.pn
        }
    }
    impl HasPacketNumber for &'_ SentPacket {
        fn pn(&self) -> PacketNumber {
            self.pn
        }
    }
    impl HasPacketNumber for &'_ mut SentPacket {
        fn pn(&self) -> PacketNumber {
            self.pn
        }
    }

    fn remove_one(pkts: &mut SentPackets, idx: PacketNumber) {
        assert_eq!(pkts.len(), 3);
        let store = pkts.take_ranges([idx..=idx]);
        let mut it = store.into_iter();
        assert_eq!(idx, it.next().unwrap().pn());
        assert!(it.next().is_none());
        std::mem::drop(it);
        assert_eq!(pkts.len(), 2);
    }

    fn assert_zero_and_two<'a, 'b: 'a>(
        mut it: impl Iterator<Item = impl HasPacketNumber + 'b> + 'a,
    ) {
        assert_eq!(it.next().unwrap().pn(), 0);
        assert_eq!(it.next().unwrap().pn(), 2);
        assert!(it.next().is_none());
    }

    #[test]
    fn iterate_skipped() {
        let mut pkts = pkts();
        for (i, p) in pkts.packets.values().enumerate() {
            assert_eq!(i, usize::try_from(p.pn).unwrap());
        }
        remove_one(&mut pkts, 1);

        // Validate the merged result multiple ways.
        assert_zero_and_two(pkts.iter_mut());

        {
            // Reverse the expectations here as this iterator reverses its output.
            let store = pkts.take_ranges([0..=2]);
            let mut it = store.into_iter();
            assert_eq!(it.next().unwrap().pn(), 2);
            assert_eq!(it.next().unwrap().pn(), 0);
            assert!(it.next().is_none());
        };

        // The None values are still there in this case, so offset is 0.
        assert_eq!(pkts.packets.len(), 0);
        assert_eq!(pkts.len(), 0);
    }

    #[test]
    fn drain() {
        let mut pkts = pkts();
        remove_one(&mut pkts, 1);

        assert_zero_and_two(pkts.drain_all());
        assert_eq!(pkts.len(), 0);
    }

    #[test]
    fn remove_expired() {
        let mut pkts = pkts();
        remove_one(&mut pkts, 0);

        for p in pkts.iter_mut() {
            p.declare_lost(p.time_sent); // just to keep things simple.
        }

        // Expire up to pkt(1).
        let count = pkts.remove_expired(start_time() + PACKET_GAP, Duration::new(0, 0));
        assert_eq!(count, 1);
        assert_eq!(pkts.len(), 1);
    }

    #[test]
    fn first_skipped_ok() {
        let mut pkts = SentPackets::default();
        pkts.track(pkt(4)); // This is fine.
        assert_eq!(pkts.len(), 1);
    }

    #[test]
    fn ignore_unknown() {
        let mut pkts = SentPackets::default();
        pkts.track(pkt(0));
        assert!(pkts.take_ranges([1..=1]).is_empty());
    }
}
