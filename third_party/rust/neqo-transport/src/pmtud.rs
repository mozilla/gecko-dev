// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::{
    iter::zip,
    net::IpAddr,
    time::{Duration, Instant},
};

use neqo_common::{qdebug, qinfo};
use static_assertions::const_assert;

use crate::{frame::FRAME_TYPE_PING, packet::PacketBuilder, recovery::SentPacket, Stats};

// Values <= 1500 based on: A. Custura, G. Fairhurst and I. Learmonth, "Exploring Usable Path MTU in
// the Internet," 2018 Network Traffic Measurement and Analysis Conference (TMA), Vienna, Austria,
// 2018, pp. 1-8, doi: 10.23919/TMA.2018.8506538. keywords:
// {Servers;Probes;Tools;Clamps;Middleboxes;Standards},
const MTU_SIZES_V4: &[usize] = &[
    1280, 1380, 1420, 1472, 1500, 2047, 4095, 8191, 16383, 32767, 65535,
];
const MTU_SIZES_V6: &[usize] = &[
    1280, 1380,
    1420, // 1420 is not in the paper for v6, but adding it makes the arrays the same length
    1470, 1500, 2047, 4095, 8191, 16383, 32767, 65535,
];
const_assert!(MTU_SIZES_V4.len() == MTU_SIZES_V6.len());
const SEARCH_TABLE_LEN: usize = MTU_SIZES_V4.len();

// From https://datatracker.ietf.org/doc/html/rfc8899#section-5.1
const MAX_PROBES: usize = 3;
const PMTU_RAISE_TIMER: Duration = Duration::from_secs(600);

#[derive(Debug, PartialEq, Clone, Copy)]
enum Probe {
    NotNeeded,
    Needed,
    Sent,
}

#[derive(Debug)]
pub struct Pmtud {
    search_table: &'static [usize],
    header_size: usize,
    mtu: usize,
    probe_index: usize,
    probe_count: usize,
    probe_state: Probe,
    loss_counts: [usize; SEARCH_TABLE_LEN],
    raise_timer: Option<Instant>,
}

impl Pmtud {
    /// Returns the MTU search table for the given remote IP address family.
    const fn search_table(remote_ip: IpAddr) -> &'static [usize] {
        match remote_ip {
            IpAddr::V4(_) => MTU_SIZES_V4,
            IpAddr::V6(_) => MTU_SIZES_V6,
        }
    }

    /// Size of the IPv4/IPv6 and UDP headers, in bytes.
    const fn header_size(remote_ip: IpAddr) -> usize {
        match remote_ip {
            IpAddr::V4(_) => 20 + 8,
            IpAddr::V6(_) => 40 + 8,
        }
    }

    #[must_use]
    pub const fn new(remote_ip: IpAddr) -> Self {
        let search_table = Self::search_table(remote_ip);
        let probe_index = 0;
        Self {
            search_table,
            header_size: Self::header_size(remote_ip),
            mtu: search_table[probe_index],
            probe_index,
            probe_count: 0,
            probe_state: Probe::NotNeeded,
            loss_counts: [0; SEARCH_TABLE_LEN],
            raise_timer: None,
        }
    }

    /// Checks whether the PMTUD raise timer should be fired, and does so if needed.
    pub fn maybe_fire_raise_timer(&mut self, now: Instant) {
        if self.probe_state == Probe::NotNeeded && self.raise_timer.map_or(false, |t| now >= t) {
            qdebug!("PMTUD raise timer fired");
            self.raise_timer = None;
            self.start();
        }
    }

    /// Returns the current Packetization Layer Path MTU, i.e., the maximum UDP payload that can be
    /// sent. During probing, this may be smaller than the actual path MTU.
    #[must_use]
    pub const fn plpmtu(&self) -> usize {
        self.mtu - self.header_size
    }

    /// Returns true if a PMTUD probe should be sent.
    #[must_use]
    pub fn needs_probe(&self) -> bool {
        self.probe_state == Probe::Needed
    }

    /// Returns true if a PMTUD probe was sent.
    #[must_use]
    pub fn probe_sent(&self) -> bool {
        self.probe_state == Probe::Sent
    }

    /// Returns the size of the current PMTUD probe.
    #[must_use]
    pub const fn probe_size(&self) -> usize {
        self.search_table[self.probe_index] - self.header_size
    }

    /// Sends a PMTUD probe.
    pub fn send_probe(&mut self, builder: &mut PacketBuilder, stats: &mut Stats) {
        // The packet may include ACK-eliciting data already, but rather than check for that, it
        // seems OK to burn one byte here to simply include a PING.
        builder.encode_varint(FRAME_TYPE_PING);
        stats.frame_tx.ping += 1;
        stats.frame_tx.all += 1;
        stats.pmtud_tx += 1;
        self.probe_count += 1;
        self.probe_state = Probe::Sent;
        qdebug!(
            "Sending PMTUD probe of size {}, count {}",
            self.search_table[self.probe_index],
            self.probe_count
        );
    }

    #[allow(rustdoc::private_intra_doc_links)]
    /// Provides a [`Fn`] that returns true if the packet is a PMTUD probe.
    ///
    /// Allows filtering packets without holding a reference to [`Pmtud`]. When
    /// in doubt, use [`Pmtud::is_probe`].
    pub fn is_probe_filter(&self) -> impl Fn(&SentPacket) -> bool {
        let probe_state = self.probe_state;
        let probe_size = self.probe_size();

        move |p: &SentPacket| -> bool { probe_state == Probe::Sent && p.len() == probe_size }
    }

    /// Returns true if the packet is a PMTUD probe.
    fn is_probe(&self, p: &SentPacket) -> bool {
        self.is_probe_filter()(p)
    }

    /// Count the PMTUD probes included in `pkts`.
    fn count_probes(&self, pkts: &[SentPacket]) -> usize {
        pkts.iter().filter(|p| self.is_probe(p)).count()
    }

    /// Checks whether a PMTUD probe has been acknowledged, and if so, updates the PMTUD state.
    /// May also initiate a new probe process for a larger MTU.
    pub fn on_packets_acked(&mut self, acked_pkts: &[SentPacket], stats: &mut Stats) {
        // Reset the loss counts for all packets sizes <= the size of the largest ACKed packet.
        let max_len = acked_pkts.iter().map(SentPacket::len).max().unwrap_or(0);
        if max_len == 0 {
            // No packets were ACKed, nothing to do.
            return;
        }

        let idx = self
            .search_table
            .iter()
            .position(|&sz| sz > max_len + self.header_size)
            .unwrap_or(SEARCH_TABLE_LEN);
        self.loss_counts.iter_mut().take(idx).for_each(|c| *c = 0);

        let acked = self.count_probes(acked_pkts);
        if acked == 0 {
            return;
        }

        // A probe was ACKed, confirm the new MTU and try to probe upwards further.
        //
        // TODO: Maybe we should be tracking stats on a per-probe-size basis rather than just the
        // total number of successful probes.
        stats.pmtud_ack += acked;
        self.mtu = self.search_table[self.probe_index];
        qdebug!("PMTUD probe of size {} succeeded", self.mtu);
        self.start();
    }

    /// Stops the PMTUD process, setting the MTU to the largest successful probe size.
    fn stop(&mut self, idx: usize, now: Instant) {
        self.probe_state = Probe::NotNeeded; // We don't need to send any more probes
        self.probe_index = idx; // Index of the last successful probe
        self.mtu = self.search_table[idx]; // Leading to this MTU
        self.probe_count = 0; // Reset the count
        self.loss_counts.fill(0); // Reset the loss counts
        self.raise_timer = Some(now + PMTU_RAISE_TIMER);
        qinfo!(
            "PMTUD stopped, PLPMTU is now {}, raise timer {:?}",
            self.mtu,
            self.raise_timer.unwrap()
        );
    }

    /// Checks whether a PMTUD probe has been lost. If it has been lost more than `MAX_PROBES`
    /// times, the PMTUD process is stopped.
    pub fn on_packets_lost(
        &mut self,
        lost_packets: &[SentPacket],
        stats: &mut Stats,
        now: Instant,
    ) {
        if lost_packets.is_empty() {
            return;
        }

        let mut increase = [0; SEARCH_TABLE_LEN];
        let mut loss_counts_updated = false;
        for p in lost_packets {
            let Some(idx) = self
                .search_table
                .iter()
                .position(|&sz| p.len() <= sz - self.header_size)
            else {
                continue;
            };
            // Count each lost packet size <= the current MTU only once. Otherwise a burst loss of
            // >= MAX_PROBES MTU-sized packets triggers a PMTUD restart. Counting only one of them
            // here requires three consecutive loss instances of such sizes to trigger a PMTUD
            // restart.
            //
            // Also, ignore losses of packets <= the minimum QUIC packet size, (`searchtable[0]`),
            // since they just increase loss counts across the board, adding to spurious
            // PMTUD restarts.
            if idx > 0 && (increase[idx] == 0 || p.len() > self.plpmtu()) {
                loss_counts_updated = true;
                increase[idx] += 1;
            }
        }

        if !loss_counts_updated {
            return;
        }

        let mut accum = 0;
        for (c, incr) in zip(&mut self.loss_counts, increase) {
            accum += incr;
            *c += accum;
        }

        // Track lost probes
        let lost = self.count_probes(lost_packets);
        stats.pmtud_lost += lost;

        // Check if any packet sizes have been lost MAX_PROBES times or more.
        //
        // TODO: It's not clear that MAX_PROBES is the right number for losses of packets that
        // aren't PMTU probes. We might want to be more conservative, to avoid spurious PMTUD
        // restarts.
        let Some(first_failed) = self.loss_counts.iter().position(|&c| c >= MAX_PROBES) else {
            // If not, keep going.
            if lost > 0 {
                // Don't stop the PMTUD process.
                self.probe_state = Probe::Needed;
            }
            return;
        };

        let last_ok = first_failed - 1;
        qdebug!(
            "Packet of size > {} lost >= {} times",
            self.search_table[last_ok],
            MAX_PROBES
        );
        if self.probe_state == Probe::NotNeeded {
            // We saw multiple losses of packets <= the current MTU outside of PMTU discovery,
            // so we need to probe again. To limit connectivity disruptions, we start the PMTU
            // discovery from the smallest packet up, rather than the failed packet size down.
            //
            // TODO: If we are declaring losses, that means that we're getting packets through.
            // The size of those will put a floor on the MTU. We're currently conservative and
            // start from scratch, but we don't strictly need to do that.
            self.restart(stats);
        } else {
            // We saw multiple losses of packets > the current MTU during PMTU discovery, so
            // we're done.
            self.stop(last_ok, now);
        }
    }

    fn restart(&mut self, stats: &mut Stats) {
        self.probe_index = 0;
        self.mtu = self.search_table[self.probe_index];
        self.loss_counts.fill(0);
        self.raise_timer = None;
        stats.pmtud_change += 1;
        qdebug!("PMTUD restarted, PLPMTU is now {}", self.mtu);
        self.start();
    }

    /// Starts the next upward PMTUD probe.
    pub fn start(&mut self) {
        if self.probe_index < SEARCH_TABLE_LEN - 1 {
            self.probe_state = Probe::Needed; // We need to send a probe
            self.probe_count = 0; // For the first time
            self.probe_index += 1; // At this size
            qdebug!(
                "PMTUD started with probe size {}",
                self.search_table[self.probe_index],
            );
        } else {
            // If we're at the end of the search table, we're done.
            self.probe_state = Probe::NotNeeded;
        }
    }

    /// Returns the default PLPMTU for the given remote IP address.
    #[must_use]
    pub const fn default_plpmtu(remote_ip: IpAddr) -> usize {
        let search_table = Self::search_table(remote_ip);
        search_table[0] - Self::header_size(remote_ip)
    }
}

#[cfg(all(not(feature = "disable-encryption"), test))]
mod tests {
    use std::{
        iter::zip,
        net::{IpAddr, Ipv4Addr, Ipv6Addr},
        time::Instant,
    };

    use neqo_common::{qdebug, Encoder, IpTosEcn};
    use test_fixture::{fixture_init, now};

    use crate::{
        crypto::CryptoDxState,
        packet::{PacketBuilder, PacketType},
        pmtud::{Probe, PMTU_RAISE_TIMER, SEARCH_TABLE_LEN},
        recovery::{SendProfile, SentPacket},
        Pmtud, Stats,
    };

    const V4: IpAddr = IpAddr::V4(Ipv4Addr::new(0, 0, 0, 0));
    const V6: IpAddr = IpAddr::V6(Ipv6Addr::new(0, 0, 0, 0, 0, 0, 0, 0));

    fn make_sentpacket(pn: u64, now: Instant, len: usize) -> SentPacket {
        SentPacket::new(
            PacketType::Short,
            pn,
            IpTosEcn::default(),
            now,
            true,
            Vec::new(),
            len,
        )
    }

    fn assert_mtu(pmtud: &Pmtud, mtu: usize) {
        let idx = pmtud
            .search_table
            .iter()
            .position(|x| *x == pmtud.mtu)
            .unwrap();
        assert!(mtu >= pmtud.search_table[idx]);
        if idx < SEARCH_TABLE_LEN - 1 {
            assert!(mtu < pmtud.search_table[idx + 1]);
        }
    }

    fn pmtud_step(
        pmtud: &mut Pmtud,
        stats: &mut Stats,
        prot: &mut CryptoDxState,
        addr: IpAddr,
        mtu: usize,
        now: Instant,
    ) {
        let stats_before = stats.clone();

        // Fake a packet number, so the builder logic works.
        let mut builder = PacketBuilder::short(Encoder::new(), false, None::<&[u8]>);
        let pn = prot.next_pn();
        builder.pn(pn, 4);
        builder.set_initial_limit(&SendProfile::new_limited(pmtud.plpmtu()), 16, pmtud);
        builder.enable_padding(true);
        pmtud.send_probe(&mut builder, stats);
        builder.pad();
        let encoder = builder.build(prot).unwrap();
        assert_eq!(encoder.len(), pmtud.probe_size());
        assert!(!pmtud.needs_probe());
        assert_eq!(stats_before.pmtud_tx + 1, stats.pmtud_tx);

        let packet = make_sentpacket(pn, now, encoder.len());
        if encoder.len() + Pmtud::header_size(addr) <= mtu {
            pmtud.on_packets_acked(&[packet], stats);
            assert_eq!(stats_before.pmtud_ack + 1, stats.pmtud_ack);
        } else {
            pmtud.on_packets_lost(&[packet], stats, now);
            assert_eq!(stats_before.pmtud_lost + 1, stats.pmtud_lost);
        }
    }

    fn find_pmtu(addr: IpAddr, mtu: usize) {
        fixture_init();
        let now = now();
        let mut pmtud = Pmtud::new(addr);
        let mut stats = Stats::default();
        let mut prot = CryptoDxState::test_default();

        pmtud.start();
        assert!(pmtud.needs_probe());

        while pmtud.needs_probe() {
            pmtud_step(&mut pmtud, &mut stats, &mut prot, addr, mtu, now);
        }
        assert_mtu(&pmtud, mtu);
    }

    #[test]
    fn pmtud_v4_max() {
        find_pmtu(V4, u16::MAX.into());
    }

    #[test]
    fn pmtud_v6_max() {
        find_pmtu(V6, u16::MAX.into());
    }

    #[test]
    fn pmtud_v4_1500() {
        find_pmtu(V4, 1500);
    }

    #[test]
    fn pmtud_v6_1500() {
        find_pmtu(V6, 1500);
    }

    fn find_pmtu_with_reduction(addr: IpAddr, mtu: usize, smaller_mtu: usize) {
        assert!(mtu > smaller_mtu);

        fixture_init();
        let now = now();
        let mut pmtud = Pmtud::new(addr);
        let mut stats = Stats::default();
        let mut prot = CryptoDxState::test_default();

        assert!(smaller_mtu >= pmtud.search_table[0]);
        pmtud.start();
        assert!(pmtud.needs_probe());

        while pmtud.needs_probe() {
            pmtud_step(&mut pmtud, &mut stats, &mut prot, addr, mtu, now);
        }
        assert_mtu(&pmtud, mtu);

        qdebug!("Reducing MTU to {}", smaller_mtu);
        // Drop packets > smaller_mtu until we need a probe again.
        while !pmtud.needs_probe() {
            let pn = prot.next_pn();
            let packet = make_sentpacket(pn, now, pmtud.mtu - pmtud.header_size);
            pmtud.on_packets_lost(&[packet], &mut stats, now);
        }

        // Drive second PMTUD process to completion.
        while pmtud.needs_probe() {
            pmtud_step(&mut pmtud, &mut stats, &mut prot, addr, mtu, now);
        }
        assert_mtu(&pmtud, mtu);
    }

    #[test]
    fn pmtud_v4_max_1300() {
        find_pmtu_with_reduction(V4, u16::MAX.into(), 1300);
    }

    #[test]
    fn pmtud_v6_max_1280() {
        find_pmtu_with_reduction(V6, u16::MAX.into(), 1300);
    }

    #[test]
    fn pmtud_v4_1500_1300() {
        find_pmtu_with_reduction(V4, 1500, 1300);
    }

    #[test]
    fn pmtud_v6_1500_1280() {
        find_pmtu_with_reduction(V6, 1500, 1280);
    }

    fn find_pmtu_with_increase(addr: IpAddr, mtu: usize, larger_mtu: usize) {
        assert!(mtu < larger_mtu);

        fixture_init();
        let now = now();
        let mut pmtud = Pmtud::new(addr);
        let mut stats = Stats::default();
        let mut prot = CryptoDxState::test_default();

        assert!(larger_mtu >= pmtud.search_table[0]);
        pmtud.start();
        assert!(pmtud.needs_probe());

        while pmtud.needs_probe() {
            pmtud_step(&mut pmtud, &mut stats, &mut prot, addr, mtu, now);
        }
        assert_mtu(&pmtud, mtu);

        qdebug!("Increasing MTU to {}", larger_mtu);
        let now = now + PMTU_RAISE_TIMER;
        pmtud.maybe_fire_raise_timer(now);
        while pmtud.needs_probe() {
            pmtud_step(&mut pmtud, &mut stats, &mut prot, addr, larger_mtu, now);
        }
        assert_mtu(&pmtud, larger_mtu);
    }

    #[test]
    fn pmtud_v4_1300_max() {
        find_pmtu_with_increase(V4, 1300, u16::MAX.into());
    }

    #[test]
    fn pmtud_v6_1280_max() {
        find_pmtu_with_increase(V6, 1280, u16::MAX.into());
    }

    #[test]
    fn pmtud_v4_1300_1500() {
        find_pmtu_with_increase(V4, 1300, 1500);
    }

    #[test]
    fn pmtud_v6_1280_1500() {
        find_pmtu_with_increase(V6, 1280, 1500);
    }

    /// Increments the loss counts for the given search table, based on the given packet size.
    fn search_table_inc(pmtud: &Pmtud, loss_counts: &[usize], sz: usize) -> Vec<usize> {
        zip(pmtud.search_table, loss_counts.iter())
            .map(|(&s, &c)| {
                if s >= sz + pmtud.header_size {
                    c + 1
                } else {
                    c
                }
            })
            .collect()
    }

    /// Asserts that the PMTUD process has restarted.
    fn assert_pmtud_restarted(pmtud: &Pmtud) {
        assert_eq!(Probe::Needed, pmtud.probe_state);
        assert_eq!(pmtud.mtu, pmtud.search_table[0]);
        assert_eq!([0; SEARCH_TABLE_LEN], pmtud.loss_counts);
    }

    /// Asserts that the PMTUD process has stopped at the given MTU.
    fn assert_pmtud_stopped(pmtud: &Pmtud, mtu: usize) {
        // assert_eq!(Probe::NotNeeded, pmtud.probe_state);
        assert_eq!(pmtud.mtu, mtu);
        assert_eq!([0; SEARCH_TABLE_LEN], pmtud.loss_counts);
    }

    #[test]
    fn pmtud_on_packets_lost() {
        let now = now();
        let mut pmtud = Pmtud::new(V4);
        let mut stats = Stats::default();

        // No packets lost, nothing should change.
        pmtud.on_packets_lost(&[], &mut stats, now);
        assert_eq!([0; SEARCH_TABLE_LEN], pmtud.loss_counts);

        // A packet of size 100 was lost, which is smaller than all probe sizes.
        // Loss counts should be unchanged.
        pmtud.on_packets_lost(&[make_sentpacket(0, now, 100)], &mut stats, now);
        assert_eq!([0; SEARCH_TABLE_LEN], pmtud.loss_counts);

        // A packet of size 100_000 was lost, which is larger than all probe sizes.
        // Loss counts should be unchanged.
        pmtud.on_packets_lost(&[make_sentpacket(0, now, 100_000)], &mut stats, now);
        assert_eq!([0; SEARCH_TABLE_LEN], pmtud.loss_counts);

        pmtud.loss_counts.fill(0); // Reset the loss counts.

        // A packet of size 1500 was lost, which should increase loss counts >= 1500 by one.
        let plen = 1500 - pmtud.header_size;
        let mut expected_lc = search_table_inc(&pmtud, &pmtud.loss_counts, plen);
        pmtud.on_packets_lost(&[make_sentpacket(0, now, plen)], &mut stats, now);
        assert_eq!(expected_lc, pmtud.loss_counts);

        // A packet of size 2000 was lost, which should increase loss counts >= 2000 by one.
        expected_lc = search_table_inc(&pmtud, &expected_lc, 2000);
        pmtud.on_packets_lost(&[make_sentpacket(0, now, 2000)], &mut stats, now);
        assert_eq!(expected_lc, pmtud.loss_counts);

        // A packet of size 5000 was lost, which should increase loss counts >= 5000 by one. There
        // have now been MAX_PROBES losses of packets >= 5000, so the PMTUD process should have
        // restarted.
        pmtud.on_packets_lost(&[make_sentpacket(0, now, 5000)], &mut stats, now);
        assert_pmtud_restarted(&pmtud);
        expected_lc.fill(0); // Reset the expected loss counts.

        // Two packets of size 4000 were lost, which should increase loss counts >= 4000 by two.
        let expected_lc = search_table_inc(&pmtud, &expected_lc, 4000);
        let expected_lc = search_table_inc(&pmtud, &expected_lc, 4000);
        pmtud.on_packets_lost(
            &[make_sentpacket(0, now, 4000), make_sentpacket(0, now, 4000)],
            &mut stats,
            now,
        );
        assert_eq!(expected_lc, pmtud.loss_counts);

        // A packet of size 2000 was lost, which should increase loss counts >= 2000 by one. There
        // have now been MAX_PROBES losses of packets >= 4000, so the PMTUD process should have
        // stopped.
        pmtud.on_packets_lost(
            &[make_sentpacket(0, now, 2000), make_sentpacket(0, now, 2000)],
            &mut stats,
            now,
        );
        assert_pmtud_stopped(&pmtud, 2047);
    }

    /// Zeros the loss counts for the given search table, below the given packet size.
    fn search_table_zero(pmtud: &Pmtud, loss_counts: &[usize], sz: usize) -> Vec<usize> {
        zip(pmtud.search_table, loss_counts.iter())
            .map(|(&s, &c)| if s <= sz + pmtud.header_size { 0 } else { c })
            .collect()
    }

    #[test]
    fn pmtud_on_packets_lost_and_acked() {
        let now = now();
        let mut pmtud = Pmtud::new(V4);
        let mut stats = Stats::default();

        // A packet of size 100 was ACKed, which is smaller than all probe sizes.
        // Loss counts should be unchanged.
        pmtud.on_packets_acked(&[make_sentpacket(0, now, 100)], &mut stats);
        assert_eq!([0; SEARCH_TABLE_LEN], pmtud.loss_counts);

        // A packet of size 100_000 was ACKed, which is larger than all probe sizes.
        // Loss counts should be unchanged.
        pmtud.on_packets_acked(&[make_sentpacket(0, now, 100_000)], &mut stats);
        assert_eq!([0; SEARCH_TABLE_LEN], pmtud.loss_counts);

        pmtud.loss_counts.fill(0); // Reset the loss counts.

        // No packets ACKed, nothing should change.
        pmtud.on_packets_acked(&[], &mut stats);
        assert_eq!([0; SEARCH_TABLE_LEN], pmtud.loss_counts);

        // One packet of size 4000 was lost, which should increase loss counts >= 4000 by one.
        let expected_lc = search_table_inc(&pmtud, &pmtud.loss_counts, 4000);
        pmtud.on_packets_lost(&[make_sentpacket(0, now, 4000)], &mut stats, now);
        assert_eq!(expected_lc, pmtud.loss_counts);

        // Now a packet of size 5000 is ACKed, which should reset all loss counts <= 5000.
        pmtud.on_packets_acked(&[make_sentpacket(0, now, 5000)], &mut stats);
        let expected_lc = search_table_zero(&pmtud, &pmtud.loss_counts, 5000);
        assert_eq!(expected_lc, pmtud.loss_counts);

        // Now, one more packets of size 4000 was lost, which should increase loss counts >= 4000
        // by one.
        let expected_lc = search_table_inc(&pmtud, &expected_lc, 4000);
        pmtud.on_packets_lost(&[make_sentpacket(0, now, 4000)], &mut stats, now);
        assert_eq!(expected_lc, pmtud.loss_counts);

        // Now a packet of size 8000 is ACKed, which should reset all loss counts <= 8000.
        pmtud.on_packets_acked(&[make_sentpacket(0, now, 8000)], &mut stats);
        let expected_lc = search_table_zero(&pmtud, &pmtud.loss_counts, 8000);
        assert_eq!(expected_lc, pmtud.loss_counts);

        // Now, one more packets of size 9000 was lost, which should increase loss counts >= 9000
        // by one. There have now been MAX_PROBES losses of packets >= 8191, so the PMTUD process
        // should have restarted.
        pmtud.on_packets_lost(&[make_sentpacket(0, now, 9000)], &mut stats, now);
        assert_pmtud_restarted(&pmtud);
    }
}
