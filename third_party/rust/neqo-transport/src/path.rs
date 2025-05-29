// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::{
    cell::RefCell,
    fmt::{self, Display},
    net::SocketAddr,
    rc::Rc,
    time::{Duration, Instant},
};

use neqo_common::{hex, qdebug, qinfo, qlog::NeqoQlog, qtrace, qwarn, Datagram, Encoder, IpTos};
use neqo_crypto::random;

use crate::{
    ackrate::{AckRate, PeerAckDelay},
    cid::{ConnectionId, ConnectionIdRef, ConnectionIdStore, RemoteConnectionIdEntry},
    ecn,
    frame::FrameType,
    packet::{PacketBuilder, PacketType},
    pmtud::Pmtud,
    recovery::{RecoveryToken, SentPacket},
    rtt::{RttEstimate, RttSource},
    sender::PacketSender,
    stats::FrameStats,
    ConnectionParameters, Stats,
};

/// The number of times that a path will be probed before it is considered failed.
///
/// Note that with [`crate::ecn`], a path is probed [`MAX_PATH_PROBES`] with ECN
/// marks and [`MAX_PATH_PROBES`] without.
pub const MAX_PATH_PROBES: usize = 3;
/// The maximum number of paths that `Paths` will track.
const MAX_PATHS: usize = 15;

pub type PathRef = Rc<RefCell<Path>>;

/// A collection for network paths.
/// This holds a collection of paths that have been used for sending or
/// receiving, plus an additional "temporary" path that is held only while
/// processing a packet.
/// This structure limits its storage and will forget about paths if it
/// is exposed to too many paths.
#[derive(Debug, Default)]
pub struct Paths {
    /// All of the paths.  All of these paths will be permanent.
    #[expect(clippy::struct_field_names, reason = "This is the best name.")]
    paths: Vec<PathRef>,
    /// This is the primary path.  This will only be `None` initially, so
    /// care needs to be taken regarding that only during the handshake.
    /// This path will also be in `paths`.
    primary: Option<PathRef>,

    /// The path that we would prefer to migrate to.
    migration_target: Option<PathRef>,

    /// Connection IDs that need to be retired.
    to_retire: Vec<u64>,

    /// `QLog` handler.
    qlog: NeqoQlog,
}

impl Paths {
    /// Find the path for the given addresses.
    /// This might be a temporary path.
    pub fn find_path(
        &self,
        local: SocketAddr,
        remote: SocketAddr,
        conn_params: &ConnectionParameters,
        now: Instant,
        stats: &mut Stats,
    ) -> PathRef {
        self.paths
            .iter()
            .find_map(|p| p.borrow().received_on(local, remote).then(|| Rc::clone(p)))
            .unwrap_or_else(|| {
                let mut p =
                    Path::temporary(local, remote, conn_params, self.qlog.clone(), now, stats);
                if let Some(primary) = self.primary.as_ref() {
                    p.prime_rtt(primary.borrow().rtt());
                }
                Rc::new(RefCell::new(p))
            })
    }

    /// Get a reference to the primary path, if one exists.
    pub fn primary(&self) -> Option<PathRef> {
        self.primary.clone()
    }

    /// Returns true if the path is not permanent.
    pub fn is_temporary(&self, path: &PathRef) -> bool {
        // Ask the path first, which is simpler.
        path.borrow().is_temporary() || !self.paths.iter().any(|p| Rc::ptr_eq(p, path))
    }

    fn retire(to_retire: &mut Vec<u64>, retired: &PathRef) {
        if let Some(cid) = &retired.borrow().remote_cid {
            let seqno = cid.sequence_number();
            if cid.connection_id().is_empty() {
                qdebug!("Connection ID {seqno} is zero-length, not retiring");
            } else {
                to_retire.push(seqno);
            }
        }
    }

    /// Adopt a temporary path as permanent.
    /// The first path that is made permanent is made primary.
    pub fn make_permanent(
        &mut self,
        path: &PathRef,
        local_cid: Option<ConnectionId>,
        remote_cid: RemoteConnectionIdEntry,
        now: Instant,
    ) {
        debug_assert!(self.is_temporary(path));

        // Make sure not to track too many paths.
        // This protects index 0, which contains the primary path.
        if self.paths.len() >= MAX_PATHS {
            debug_assert_eq!(self.paths.len(), MAX_PATHS);
            let removed = self.paths.remove(1);
            Self::retire(&mut self.to_retire, &removed);
            if self
                .migration_target
                .as_ref()
                .is_some_and(|target| Rc::ptr_eq(target, &removed))
            {
                qinfo!(
                    "[{}] The migration target path had to be removed",
                    path.borrow()
                );
                self.migration_target = None;
            }
            debug_assert_eq!(Rc::strong_count(&removed), 1);
        }

        qdebug!("[{}] Make permanent", path.borrow());
        path.borrow_mut().make_permanent(local_cid, remote_cid);
        self.paths.push(Rc::clone(path));
        if self.primary.is_none() {
            assert!(self.select_primary(path, now).is_none());
        }
    }

    /// Select a path as the primary.  Returns the old primary path.
    /// Using the old path is only necessary if this change in path is a reaction
    /// to a migration from a peer, in which case the old path needs to be probed.
    #[must_use]
    fn select_primary(&mut self, path: &PathRef, now: Instant) -> Option<PathRef> {
        qdebug!("[{}] set as primary path", path.borrow());
        let old_path = self.primary.replace(Rc::clone(path)).inspect(|old| {
            old.borrow_mut().set_primary(false, now);
        });

        // Swap the primary path into slot 0, so that it is protected from eviction.
        let idx = self
            .paths
            .iter()
            .enumerate()
            .find_map(|(i, p)| Rc::ptr_eq(p, path).then_some(i))?;
        self.paths.swap(0, idx);

        path.borrow_mut().set_primary(true, now);
        old_path
    }

    /// Migrate to the identified path.  If `force` is true, the path
    /// is forcibly marked as valid and the path is used immediately.
    /// Otherwise, migration will occur after probing succeeds.
    /// The path is always probed and will be abandoned if probing fails.
    /// Returns `true` if the path was migrated.
    pub fn migrate(
        &mut self,
        path: &PathRef,
        force: bool,
        now: Instant,
        stats: &mut Stats,
    ) -> bool {
        debug_assert!(!self.is_temporary(path));
        let baseline = self.primary().map_or_else(
            || ecn::Info::default().baseline(),
            |p| p.borrow().ecn_info.baseline(),
        );
        path.borrow_mut().set_ecn_baseline(baseline);
        if force || path.borrow().is_valid() {
            path.borrow_mut().set_valid(now);
            drop(self.select_primary(path, now));
            self.migration_target = None;
        } else {
            self.migration_target = Some(Rc::clone(path));
        }
        path.borrow_mut().probe(stats);
        self.migration_target.is_none()
    }

    /// Process elapsed time for active paths.
    /// Returns an true if there are viable paths remaining after tidying up.
    ///
    /// TODO(mt) - the paths should own the RTT estimator, so they can find the PTO
    /// for themselves.
    pub fn process_timeout(&mut self, now: Instant, pto: Duration, stats: &mut Stats) -> bool {
        let to_retire = &mut self.to_retire;
        let mut primary_failed = false;
        self.paths.retain(|p| {
            if p.borrow_mut().process_timeout(now, pto, stats) {
                true
            } else {
                qdebug!("[{}] Retiring path", p.borrow());
                if p.borrow().is_primary() {
                    primary_failed = true;
                }
                Self::retire(to_retire, p);
                false
            }
        });

        if primary_failed {
            self.primary = None;
            // Find a valid path to fall back to.
            #[expect(
                clippy::option_if_let_else,
                reason = "The alternative is less readable."
            )]
            if let Some(fallback) = self
                .paths
                .iter()
                .rev() // More recent paths are toward the end.
                .find(|p| p.borrow().is_valid())
            {
                // Need a clone as `fallback` is borrowed from `self`.
                let path = Rc::clone(fallback);
                qinfo!("[{}] Failing over after primary path failed", path.borrow());
                drop(self.select_primary(&path, now));
                true
            } else {
                false
            }
        } else {
            // See if the PMTUD raise timer wants to fire.
            if let Some(path) = self.primary() {
                path.borrow_mut()
                    .pmtud_mut()
                    .maybe_fire_raise_timer(now, stats);
            }
            true
        }
    }

    /// Get when the next call to `process_timeout()` should be scheduled.
    pub fn next_timeout(&self, pto: Duration) -> Option<Instant> {
        self.paths
            .iter()
            .filter_map(|p| p.borrow().next_timeout(pto))
            .min()
    }

    /// Set the identified path to be primary.
    /// This panics if `make_permanent` hasn't been called.
    pub fn handle_migration(
        &mut self,
        path: &PathRef,
        remote: SocketAddr,
        now: Instant,
        stats: &mut Stats,
    ) {
        // The update here needs to match the checks in `Path::received_on`.
        // Here, we update the remote port number to match the source port on the
        // datagram that was received.  This ensures that we send subsequent
        // packets back to the right place.
        path.borrow_mut().update_port(remote.port());

        if path.borrow().is_primary() {
            // Update when the path was last regarded as valid.
            path.borrow_mut().update(now);
            return;
        }

        if let Some(old_path) = self.select_primary(path, now) {
            // Need to probe the old path if the peer migrates.
            old_path.borrow_mut().probe(stats);
            // TODO(mt) - suppress probing if the path was valid within 3PTO.
        }
    }

    /// Select a path to send on.  This will select the first path that has
    /// probes to send, then fall back to the primary path.
    pub fn select_path(&self) -> Option<PathRef> {
        self.paths
            .iter()
            .find_map(|p| p.borrow().has_probe().then(|| Rc::clone(p)))
            .or_else(|| self.primary.clone())
    }

    /// A `PATH_RESPONSE` was received.
    /// Returns `true` if migration occurred.
    #[must_use]
    pub fn path_response(&mut self, response: [u8; 8], now: Instant, stats: &mut Stats) -> bool {
        // TODO(mt) consider recording an RTT measurement here as we don't train
        // RTT for non-primary paths.
        for p in &self.paths {
            if p.borrow_mut().path_response(response, now, stats) {
                // The response was accepted.  If this path is one we intend
                // to migrate to, then migrate.
                if let Some(primary) = self
                    .migration_target
                    .take_if(|target| Rc::ptr_eq(target, p))
                {
                    drop(self.select_primary(&primary, now));
                    return true;
                }
                break;
            }
        }
        false
    }

    /// Retire all of the connection IDs prior to the indicated sequence number.
    /// Keep active paths if possible by pulling new connection IDs from the provided store.
    /// One slightly non-obvious consequence of this is that if migration is being attempted
    /// and the new path cannot obtain a new connection ID, the migration attempt will fail.
    pub fn retire_cids(&mut self, retire_prior: u64, store: &mut ConnectionIdStore<[u8; 16]>) {
        let to_retire = &mut self.to_retire;
        let migration_target = &mut self.migration_target;

        // First, tell the store to release any connection IDs that are too old.
        let mut retired = store.retire_prior_to(retire_prior);
        to_retire.append(&mut retired);

        self.paths.retain(|p| {
            let mut path = p.borrow_mut();
            let Some(current) = path.remote_cid.as_ref() else {
                return true;
            };
            if current.sequence_number() < retire_prior && !current.connection_id().is_empty() {
                to_retire.push(current.sequence_number());
                let new_cid = store.next();
                let has_replacement = new_cid.is_some();
                // There must be a connection ID available for the primary path as we
                // keep that path at the first index.
                debug_assert!(!path.is_primary() || has_replacement);
                path.remote_cid = new_cid;
                if !has_replacement
                    && migration_target
                        .as_ref()
                        .is_some_and(|target| Rc::ptr_eq(target, p))
                {
                    qinfo!(
                        "[{path}] NEW_CONNECTION_ID with Retire Prior To forced migration to fail"
                    );
                    *migration_target = None;
                }
                has_replacement
            } else {
                true
            }
        });
    }

    /// Write out any `RETIRE_CONNECTION_ID` frames that are outstanding.
    pub fn write_frames(
        &mut self,
        builder: &mut PacketBuilder,
        tokens: &mut Vec<RecoveryToken>,
        stats: &mut FrameStats,
    ) {
        while let Some(seqno) = self.to_retire.pop() {
            if builder.remaining() < 1 + Encoder::varint_len(seqno) {
                self.to_retire.push(seqno);
                break;
            }
            builder.encode_varint(FrameType::RetireConnectionId);
            builder.encode_varint(seqno);
            tokens.push(RecoveryToken::RetireConnectionId(seqno));
            stats.retire_connection_id += 1;
        }

        if let Some(path) = self.primary() {
            // Write out any ACK_FREQUENCY frames.
            path.borrow_mut().write_cc_frames(builder, tokens, stats);
        }
    }

    pub fn lost_retire_cid(&mut self, lost: u64) {
        self.to_retire.push(lost);
    }

    pub fn acked_retire_cid(&mut self, acked: u64) {
        self.to_retire.retain(|&seqno| seqno != acked);
    }

    pub fn lost_ack_frequency(&self, lost: &AckRate) {
        if let Some(path) = self.primary() {
            path.borrow_mut().lost_ack_frequency(lost);
        }
    }

    pub fn acked_ack_frequency(&self, acked: &AckRate) {
        if let Some(path) = self.primary() {
            path.borrow_mut().acked_ack_frequency(acked);
        }
    }

    pub fn acked_ecn(&self) {
        if let Some(path) = self.primary() {
            path.borrow_mut().acked_ecn();
        }
    }

    pub fn lost_ecn(&self, pt: PacketType, stats: &mut Stats) {
        if let Some(path) = self.primary() {
            path.borrow_mut().lost_ecn(pt, stats);
        }
    }

    /// Get an estimate of the RTT on the primary path.
    #[cfg(test)]
    pub fn rtt(&self) -> Duration {
        // Rather than have this fail when there is no active path,
        // make a new RTT estimate and interrogate that.
        // That is more expensive, but it should be rare and breaking encapsulation
        // is worse, especially as this is only used in tests.
        self.primary().map_or_else(
            || RttEstimate::default().estimate(),
            |p| p.borrow().rtt().estimate(),
        )
    }

    pub fn set_qlog(&mut self, qlog: NeqoQlog) {
        for p in &mut self.paths {
            p.borrow_mut().set_qlog(qlog.clone());
        }
        self.qlog = qlog;
    }
}

/// The state of a path with respect to address validation.
#[derive(Debug)]
enum ProbeState {
    /// The path was last valid at the indicated time.
    Valid,
    /// The path was previously valid, but a new probe is needed.
    ProbeNeeded { probe_count: usize },
    /// The path hasn't been validated, but a probe has been sent.
    Probing {
        /// The number of probes that have been sent.
        probe_count: usize,
        /// The probe that was last sent.
        data: [u8; 8],
        /// Whether the probe was sent in a datagram padded to the path MTU.
        mtu: bool,
        /// When the probe was sent.
        sent: Instant,
    },
    /// Validation failed the last time it was attempted.
    Failed,
}

impl ProbeState {
    /// Determine whether the current state requires probing.
    const fn probe_needed(&self) -> bool {
        matches!(self, Self::ProbeNeeded { .. })
    }
}

/// A network path.
///
/// Paths are used a little bit strangely by connections:
/// they need to encapsulate all the state for a path (which
/// is normal), but that information is not propagated to the
/// `Paths` instance that holds them.  This is because the packet
/// processing where changes occur can't hold a reference to the
/// `Paths` instance that owns the `Path`.  Any changes to the
/// path are communicated to `Paths` afterwards.
#[derive(Debug)]
pub struct Path {
    /// A local socket address.
    local: SocketAddr,
    /// A remote socket address.
    remote: SocketAddr,
    /// The connection IDs that we use when sending on this path.
    /// This is only needed during the handshake.
    local_cid: Option<ConnectionId>,
    /// The current connection ID that we are using and its details.
    remote_cid: Option<RemoteConnectionIdEntry>,

    /// Whether this is the primary path.
    primary: bool,
    /// Whether the current path is considered valid.
    state: ProbeState,
    /// For a path that is not validated, this is `None`.  For a validated
    /// path, the time that the path was last valid.
    validated: Option<Instant>,
    /// A path challenge was received and `PATH_RESPONSE` has not been sent.
    challenge: Option<[u8; 8]>,

    /// The round trip time estimate for this path.
    rtt: RttEstimate,
    /// A packet sender for the path, which includes congestion control and a pacer.
    sender: PacketSender,

    /// The number of bytes received on this path.
    /// Note that this value might saturate on a long-lived connection,
    /// but we only use it before the path is validated.
    received_bytes: usize,
    /// The number of bytes sent on this path.
    sent_bytes: usize,
    /// The ECN-related state for this path (see RFC9000, Section 13.4 and Appendix A.4)
    ecn_info: ecn::Info,
    /// For logging of events.
    qlog: NeqoQlog,
}

impl Path {
    /// Create a path from addresses and a remote connection ID.
    /// This is used for migration and for new datagrams.
    pub fn temporary(
        local: SocketAddr,
        remote: SocketAddr,
        conn_params: &ConnectionParameters,
        qlog: NeqoQlog,
        now: Instant,
        stats: &mut Stats,
    ) -> Self {
        let iface_mtu = if conn_params.pmtud_iface_mtu_enabled() {
            match mtu::interface_and_mtu(remote.ip()) {
                Ok((name, mtu)) => {
                    qdebug!(
                        "Outbound interface {name} for destination {ip} has MTU {mtu}",
                        ip = remote.ip()
                    );
                    stats.pmtud_iface_mtu = mtu;
                    Some(mtu)
                }
                Err(e) => {
                    qwarn!(
                        "Failed to determine outbound interface for destination {ip}: {e}",
                        ip = remote.ip()
                    );
                    None
                }
            }
        } else {
            None
        };
        let mut sender = PacketSender::new(conn_params, Pmtud::new(remote.ip(), iface_mtu), now);
        sender.set_qlog(qlog.clone());
        Self {
            local,
            remote,
            local_cid: None,
            remote_cid: None,
            primary: false,
            state: ProbeState::ProbeNeeded { probe_count: 0 },
            validated: None,
            challenge: None,
            rtt: RttEstimate::default(),
            sender,
            received_bytes: 0,
            sent_bytes: 0,
            ecn_info: ecn::Info::default(),
            qlog,
        }
    }

    pub fn set_ecn_baseline(&mut self, baseline: ecn::Count) {
        self.ecn_info.set_baseline(baseline);
    }

    /// Return the DSCP/ECN marking to use for outgoing packets on this path.
    pub fn tos(&self, tokens: &mut Vec<RecoveryToken>) -> IpTos {
        self.ecn_info.ecn_mark(tokens).into()
    }

    /// Whether this path is the primary or current path for the connection.
    pub const fn is_primary(&self) -> bool {
        self.primary
    }

    /// Whether this path is a temporary one.
    pub const fn is_temporary(&self) -> bool {
        self.remote_cid.is_none()
    }

    /// By adding a remote connection ID, we make the path permanent
    /// and one that we will later send packets on.
    /// If `local_cid` is `None`, the existing value will be kept.
    pub(crate) fn make_permanent(
        &mut self,
        local_cid: Option<ConnectionId>,
        remote_cid: RemoteConnectionIdEntry,
    ) {
        if self.local_cid.is_none() {
            self.local_cid = local_cid;
        }
        self.remote_cid.replace(remote_cid);
    }

    /// Determine if this path was the one that the provided datagram was received on.
    fn received_on(&self, local: SocketAddr, remote: SocketAddr) -> bool {
        self.local == local && self.remote == remote
    }

    /// Update the remote port number.  Any flexibility we allow in `received_on`
    /// need to be adjusted at this point.
    fn update_port(&mut self, port: u16) {
        self.remote.set_port(port);
    }

    /// Set whether this path is primary.
    pub(crate) fn set_primary(&mut self, primary: bool, now: Instant) {
        qtrace!("[{self}] Make primary {primary}");
        debug_assert!(self.remote_cid.is_some());
        self.primary = primary;
        if !primary {
            self.sender.discard_in_flight(now);
        }
    }

    /// Set the current path as valid.  This updates the time that the path was
    /// last validated and cancels any path validation.
    pub fn set_valid(&mut self, now: Instant) {
        qdebug!("[{self}] Path validated {now:?}");
        self.state = ProbeState::Valid;
        self.validated = Some(now);
    }

    /// Update the last use of this path, if it is valid.
    /// This will keep the path active slightly longer.
    pub fn update(&mut self, now: Instant) {
        if self.validated.is_some() {
            self.validated = Some(now);
        }
    }

    /// Get the PL MTU.
    pub fn plpmtu(&self) -> usize {
        self.pmtud().plpmtu()
    }

    /// Get a reference to the PMTUD state.
    pub fn pmtud(&self) -> &Pmtud {
        self.sender.pmtud()
    }

    /// Get the first local connection ID.
    /// Only do this for the primary path during the handshake.
    pub const fn local_cid(&self) -> Option<&ConnectionId> {
        self.local_cid.as_ref()
    }

    /// Set the remote connection ID based on the peer's choice.
    /// This is only valid during the handshake.
    pub fn set_remote_cid(&mut self, cid: ConnectionIdRef) {
        if let Some(remote_cid) = self.remote_cid.as_mut() {
            remote_cid.update_cid(ConnectionId::from(cid));
        }
    }

    /// Access the remote connection ID.
    pub fn remote_cid(&self) -> Option<&ConnectionId> {
        self.remote_cid
            .as_ref()
            .map(super::cid::ConnectionIdEntry::connection_id)
    }

    /// Set the stateless reset token for the connection ID that is currently in use.
    pub fn set_reset_token(&mut self, token: [u8; 16]) {
        if let Some(remote_cid) = self.remote_cid.as_mut() {
            remote_cid.set_stateless_reset_token(token);
        }
    }

    /// Determine if the provided token is a stateless reset token.
    pub fn is_stateless_reset(&self, token: &[u8; 16]) -> bool {
        self.remote_cid
            .as_ref()
            .is_some_and(|rcid| rcid.is_stateless_reset(token))
    }

    /// Make a datagram.
    pub fn datagram<V: Into<Vec<u8>>>(
        &mut self,
        payload: V,
        tos: IpTos,
        stats: &mut Stats,
    ) -> Datagram {
        // Make sure to use the TOS value from before calling ecn::Info::on_packet_sent, which may
        // update the ECN state and can hence change it - this packet should still be sent
        // with the current value.
        self.ecn_info.on_packet_sent(stats);
        Datagram::new(self.local, self.remote, tos, payload.into())
    }

    /// Get local address as `SocketAddr`
    pub const fn local_address(&self) -> SocketAddr {
        self.local
    }

    /// Get remote address as `SocketAddr`
    pub const fn remote_address(&self) -> SocketAddr {
        self.remote
    }

    /// Whether the path has been validated.
    pub const fn is_valid(&self) -> bool {
        self.validated.is_some()
    }

    /// Handle a `PATH_RESPONSE` frame. Returns true if the response was accepted.
    pub fn path_response(&mut self, response: [u8; 8], now: Instant, stats: &mut Stats) -> bool {
        if let ProbeState::Probing { data, mtu, .. } = &mut self.state {
            if response == *data {
                let need_full_probe = !*mtu;
                self.set_valid(now);
                if need_full_probe {
                    qdebug!("[{self}] Sub-MTU probe successful, reset probe count");
                    self.probe(stats);
                }
                true
            } else {
                false
            }
        } else {
            false
        }
    }

    /// The path has been challenged.  This generates a response.
    /// This only generates a single response at a time.
    pub fn challenged(&mut self, challenge: [u8; 8]) {
        self.challenge = Some(challenge.to_owned());
    }

    /// At the next opportunity, send a probe.
    /// If the probe count has been exhausted already, marks the path as failed.
    fn probe(&mut self, stats: &mut Stats) {
        let probe_count = match &self.state {
            ProbeState::Probing { probe_count, .. } => *probe_count + 1,
            ProbeState::ProbeNeeded { probe_count, .. } => *probe_count,
            _ => 0,
        };
        self.state = if probe_count >= MAX_PATH_PROBES {
            if self.ecn_info.is_marking() {
                // The path validation failure may be due to ECN blackholing, try again without ECN.
                qinfo!("[{self}] Possible ECN blackhole, disabling ECN and re-probing path");
                self.ecn_info
                    .disable_ecn(stats, ecn::ValidationError::BlackHole);
                ProbeState::ProbeNeeded { probe_count: 0 }
            } else {
                qinfo!("[{self}] Probing failed");
                ProbeState::Failed
            }
        } else {
            qdebug!("[{self}] Initiating probe");
            ProbeState::ProbeNeeded { probe_count }
        };
    }

    /// Returns true if this path have any probing frames to send.
    pub const fn has_probe(&self) -> bool {
        self.challenge.is_some() || self.state.probe_needed()
    }

    pub fn write_frames(
        &mut self,
        builder: &mut PacketBuilder,
        stats: &mut FrameStats,
        mtu: bool, // Whether the packet we're writing into will be a full MTU.
        now: Instant,
    ) -> bool {
        if builder.remaining() < 9 {
            return false;
        }
        // Send PATH_RESPONSE.
        let resp_sent = if let Some(challenge) = self.challenge.take() {
            qtrace!("[{self}] Responding to path challenge {}", hex(challenge));
            builder.encode_varint(FrameType::PathResponse);
            builder.encode(&challenge[..]);

            // These frames are not retransmitted in the usual fashion.
            stats.path_response += 1;

            if builder.remaining() < 9 {
                return true;
            }
            true
        } else {
            false
        };

        // Send PATH_CHALLENGE.
        if let ProbeState::ProbeNeeded { probe_count } = self.state {
            qtrace!("[{self}] Initiating path challenge {probe_count}");
            let data = random::<8>();
            builder.encode_varint(FrameType::PathChallenge);
            builder.encode(&data);

            // As above, no recovery token.
            stats.path_challenge += 1;

            self.state = ProbeState::Probing {
                probe_count,
                data,
                mtu,
                sent: now,
            };
            true
        } else {
            resp_sent
        }
    }

    /// Write `ACK_FREQUENCY` frames.
    pub fn write_cc_frames(
        &mut self,
        builder: &mut PacketBuilder,
        tokens: &mut Vec<RecoveryToken>,
        stats: &mut FrameStats,
    ) {
        self.rtt.write_frames(builder, tokens, stats);
    }

    pub fn lost_ack_frequency(&mut self, lost: &AckRate) {
        self.rtt.frame_lost(lost);
    }

    pub fn acked_ecn(&mut self) {
        self.ecn_info.acked_ecn();
    }

    pub fn lost_ecn(&mut self, pt: PacketType, stats: &mut Stats) {
        self.ecn_info.lost_ecn(pt, stats);
    }

    pub fn acked_ack_frequency(&mut self, acked: &AckRate) {
        self.rtt.frame_acked(acked);
    }

    /// Process a timer for this path.
    /// This returns true if the path is viable and can be kept alive.
    pub fn process_timeout(&mut self, now: Instant, pto: Duration, stats: &mut Stats) -> bool {
        if let ProbeState::Probing { sent, .. } = &self.state {
            if now >= *sent + pto {
                self.probe(stats);
            }
        }
        if matches!(self.state, ProbeState::Failed) {
            // Retire failed paths immediately.
            false
        } else if self.primary {
            // Keep valid primary paths otherwise.
            true
        } else if matches!(self.state, ProbeState::Valid) {
            // Retire validated, non-primary paths.
            // Allow more than `2 * MAX_PATH_PROBES` times the PTO so that an old
            // path remains around until after a previous path fails.
            let count = u32::try_from(2 * MAX_PATH_PROBES + 1).expect("result fits in u32");
            self.validated
                .is_some_and(|validated| validated + (pto * count) > now)
        } else {
            // Keep paths that are being actively probed.
            true
        }
    }

    /// Return the next time that this path needs servicing.
    /// This only considers retransmissions of probes, not cleanup of the path.
    /// If there is no other activity, then there is no real need to schedule a
    /// timer to cleanup old paths.
    pub fn next_timeout(&self, pto: Duration) -> Option<Instant> {
        if let ProbeState::Probing { sent, .. } = &self.state {
            Some(*sent + pto)
        } else {
            None
        }
    }

    /// Get the RTT estimator for this path.
    pub const fn rtt(&self) -> &RttEstimate {
        &self.rtt
    }

    /// Mutably borrow the RTT estimator for this path.
    pub fn rtt_mut(&mut self) -> &mut RttEstimate {
        &mut self.rtt
    }

    /// Mutably borrow the PMTUD discoverer for this path.
    pub fn pmtud_mut(&mut self) -> &mut Pmtud {
        self.sender.pmtud_mut()
    }

    /// Read-only access to the owned sender.
    pub const fn sender(&self) -> &PacketSender {
        &self.sender
    }

    /// Pass on RTT configuration: the maximum acknowledgment delay of the peer,
    /// and maybe the minimum delay.
    pub fn set_ack_delay(
        &mut self,
        max_ack_delay: Duration,
        min_ack_delay: Option<Duration>,
        ack_ratio: u8,
    ) {
        let ack_delay = min_ack_delay.map_or_else(
            || PeerAckDelay::fixed(max_ack_delay),
            |m| {
                PeerAckDelay::flexible(
                    max_ack_delay,
                    m,
                    ack_ratio,
                    self.sender.cwnd(),
                    self.plpmtu(),
                    self.rtt.estimate(),
                )
            },
        );
        self.rtt.set_ack_delay(ack_delay);
    }

    /// Initialize the RTT for the path based on an existing estimate.
    pub fn prime_rtt(&mut self, rtt: &RttEstimate) {
        self.rtt.prime_rtt(rtt);
    }

    /// Record received bytes for the path.
    pub fn add_received(&mut self, count: usize) {
        self.received_bytes = self.received_bytes.saturating_add(count);
    }

    /// Record sent bytes for the path.
    pub fn add_sent(&mut self, count: usize) {
        self.sent_bytes = self.sent_bytes.saturating_add(count);
    }

    /// Record a packet as having been sent on this path.
    pub fn packet_sent(&mut self, sent: &mut SentPacket, now: Instant) {
        if !self.is_primary() {
            sent.clear_primary_path();
        }
        self.sender.on_packet_sent(sent, self.rtt.estimate(), now);
    }

    /// Discard a packet that previously might have been in-flight.
    pub fn discard_packet(&mut self, sent: &SentPacket, now: Instant, stats: &mut Stats) {
        if self.rtt.first_sample_time().is_none() {
            // When discarding a packet there might not be a good RTT estimate.
            // But discards only occur after receiving something, so that means
            // that there is some RTT information, which is better than nothing.
            // Two cases: 1. at the client when handling a Retry and
            // 2. at the server when disposing the Initial packet number space.
            qinfo!(
                "[{self}] discarding a packet without an RTT estimate; guessing RTT={:?}",
                now - sent.time_sent()
            );
            stats.rtt_init_guess = true;
            self.rtt.update(
                &self.qlog,
                now - sent.time_sent(),
                Duration::new(0, 0),
                RttSource::Guesstimate,
                now,
            );
        }

        self.sender.discard(sent, now);
    }

    /// Record packets as acknowledged with the sender.
    pub fn on_packets_acked(
        &mut self,
        acked_pkts: &[SentPacket],
        ack_ecn: Option<ecn::Count>,
        now: Instant,
        stats: &mut Stats,
    ) {
        debug_assert!(self.is_primary());

        let ecn_ce_received = self.ecn_info.on_packets_acked(acked_pkts, ack_ecn, stats);
        if ecn_ce_received {
            let cwnd_reduced = self
                .sender
                .on_ecn_ce_received(acked_pkts.first().expect("must be there"), now);
            if cwnd_reduced {
                self.rtt.update_ack_delay(self.sender.cwnd(), self.plpmtu());
            }
        }

        self.sender
            .on_packets_acked(acked_pkts, &self.rtt, now, stats);
    }

    /// Record packets as lost with the sender.
    pub fn on_packets_lost(
        &mut self,
        prev_largest_acked_sent: Option<Instant>,
        confirmed: bool,
        lost_packets: &[SentPacket],
        stats: &mut Stats,
        now: Instant,
    ) {
        debug_assert!(self.is_primary());
        let cwnd_reduced = self.sender.on_packets_lost(
            self.rtt.first_sample_time(),
            prev_largest_acked_sent,
            self.rtt.pto(confirmed), // Important: the base PTO, not adjusted.
            lost_packets,
            stats,
            now,
        );
        if cwnd_reduced {
            self.rtt.update_ack_delay(self.sender.cwnd(), self.plpmtu());
        }
    }

    /// Determine whether we should be setting a PTO for this path. This is true when either the
    /// path is valid or when there is enough remaining in the amplification limit to fit a
    /// full-sized path (i.e., the path MTU).
    pub fn pto_possible(&self) -> bool {
        // See the implementation of `amplification_limit` for details.
        self.amplification_limit() >= self.plpmtu()
    }

    /// Get the number of bytes that can be written to this path.
    pub fn amplification_limit(&self) -> usize {
        if matches!(self.state, ProbeState::Failed) {
            0
        } else if self.is_valid() {
            usize::MAX
        } else {
            self.received_bytes
                .checked_mul(3)
                .map_or(usize::MAX, |limit| {
                    let budget = if limit == 0 {
                        // If we have received absolutely nothing thus far, then this endpoint
                        // is the one initiating communication on this path.  Allow enough space for
                        // probing.
                        self.plpmtu() * 5
                    } else {
                        limit
                    };
                    budget.saturating_sub(self.sent_bytes)
                })
        }
    }

    /// Update the `NeqoQLog` instance.
    pub fn set_qlog(&mut self, qlog: NeqoQlog) {
        self.sender.set_qlog(qlog);
    }
}

impl Display for Path {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        if self.is_primary() {
            write!(f, "pri-")?; // primary
        }
        if !self.is_valid() {
            write!(f, "unv-")?; // unvalidated
        }
        write!(f, "path")?;
        if let Some(entry) = self.remote_cid.as_ref() {
            write!(f, ":{}", entry.connection_id())?;
        }
        write!(f, " {}->{}", self.local, self.remote)?;
        Ok(())
    }
}
