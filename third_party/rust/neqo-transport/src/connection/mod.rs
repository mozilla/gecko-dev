// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

// The class implementing a QUIC connection.

use std::{
    cell::RefCell,
    cmp::{max, min},
    fmt::{self, Debug, Display, Formatter, Write as _},
    iter, mem,
    net::{IpAddr, SocketAddr},
    num::NonZeroUsize,
    ops::RangeInclusive,
    rc::{Rc, Weak},
    time::{Duration, Instant},
};

use neqo_common::{
    event::Provider as EventProvider, hex, hex_snip_middle, hrtime, qdebug, qerror, qinfo,
    qlog::NeqoQlog, qtrace, qwarn, Datagram, Decoder, Encoder, IpTos, IpTosEcn, Role,
};
use neqo_crypto::{
    agent::CertificateInfo, Agent, AntiReplay, AuthenticationStatus, Cipher, Client, Group,
    HandshakeState, PrivateKey, PublicKey, ResumptionToken, SecretAgentInfo, SecretAgentPreInfo,
    Server, ZeroRttChecker,
};
use smallvec::SmallVec;
use strum::IntoEnumIterator as _;

use crate::{
    addr_valid::{AddressValidation, NewTokenState},
    cid::{
        ConnectionId, ConnectionIdEntry, ConnectionIdGenerator, ConnectionIdManager,
        ConnectionIdRef, ConnectionIdStore, LOCAL_ACTIVE_CID_LIMIT,
    },
    crypto::{Crypto, CryptoDxState, Epoch},
    ecn,
    events::{ConnectionEvent, ConnectionEvents, OutgoingDatagramOutcome},
    frame::{CloseError, Frame, FrameType},
    packet::{self, DecryptedPacket, PacketBuilder, PacketNumber, PacketType, PublicPacket},
    path::{Path, PathRef, Paths},
    qlog,
    quic_datagrams::{DatagramTracking, QuicDatagrams},
    recovery::{LossRecovery, RecoveryToken, SendProfile, SentPacket},
    recv_stream::RecvStreamStats,
    rtt::{RttEstimate, GRANULARITY, INITIAL_RTT},
    send_stream::SendStream,
    stats::{Stats, StatsCell},
    stream_id::StreamType,
    streams::{SendOrder, Streams},
    tparams::{
        self,
        TransportParameterId::{
            self, AckDelayExponent, ActiveConnectionIdLimit, DisableMigration, GreaseQuicBit,
            InitialSourceConnectionId, MaxAckDelay, MaxDatagramFrameSize, MinAckDelay,
            OriginalDestinationConnectionId, RetrySourceConnectionId, StatelessResetToken,
        },
        TransportParameters, TransportParametersHandler,
    },
    tracking::{AckTracker, PacketNumberSpace, RecvdPackets},
    version::{Version, WireVersion},
    AppError, CloseReason, Error, Res, StreamId,
};

mod idle;
pub mod params;
mod saved;
mod state;
#[cfg(test)]
pub mod test_internal;

use idle::IdleTimeout;
pub use params::ConnectionParameters;
use params::PreferredAddressConfig;
#[cfg(test)]
pub use params::ACK_RATIO_SCALE;
use saved::SavedDatagrams;
use state::StateSignaling;
pub use state::{ClosingFrame, State};

pub use crate::send_stream::{RetransmissionPriority, SendStreamStats, TransmissionPriority};

/// The number of Initial packets that the client will send in response
/// to receiving an undecryptable packet during the early part of the
/// handshake.  This is a hack, but a useful one.
const EXTRA_INITIALS: usize = 4;

#[derive(Debug, PartialEq, Eq, Clone, Copy)]
pub enum ZeroRttState {
    Init,
    Sending,
    AcceptedClient,
    AcceptedServer,
    Rejected,
}

#[derive(Clone, Debug, PartialEq, Eq)]
/// Type returned from `process()` and `process_output()`. Users are required to
/// call these repeatedly until `Callback` or `None` is returned.
pub enum Output {
    /// Connection requires no action.
    None,
    /// Connection requires the datagram be sent.
    Datagram(Datagram),
    /// Connection requires `process_input()` be called when the `Duration`
    /// elapses.
    Callback(Duration),
}

impl Output {
    /// Convert into an `Option<Datagram>`.
    #[must_use]
    pub fn dgram(self) -> Option<Datagram> {
        match self {
            Self::Datagram(dg) => Some(dg),
            _ => None,
        }
    }

    /// Get a reference to the Datagram, if any.
    #[must_use]
    pub const fn as_dgram_ref(&self) -> Option<&Datagram> {
        match self {
            Self::Datagram(dg) => Some(dg),
            _ => None,
        }
    }

    /// Ask how long the caller should wait before calling back.
    #[must_use]
    pub const fn callback(&self) -> Duration {
        match self {
            Self::Callback(t) => *t,
            _ => Duration::new(0, 0),
        }
    }

    #[must_use]
    pub fn or_else<F>(self, f: F) -> Self
    where
        F: FnOnce() -> Self,
    {
        match self {
            x @ (Self::Datagram(_) | Self::Callback(_)) => x,
            Self::None => f(),
        }
    }
}

impl From<Option<Datagram>> for Output {
    fn from(value: Option<Datagram>) -> Self {
        value.map_or(Self::None, Self::Datagram)
    }
}

/// Used by inner functions like `Connection::output`.
enum SendOption {
    /// Yes, please send this datagram.
    Yes(Datagram),
    /// Don't send.  If this was blocked on the pacer (the arg is true).
    No(bool),
}

impl Default for SendOption {
    fn default() -> Self {
        Self::No(false)
    }
}

/// Used by `Connection::preprocess` to determine what to do
/// with an packet before attempting to remove protection.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum PreprocessResult {
    /// End processing and return successfully.
    End,
    /// Stop processing this datagram and move on to the next.
    Next,
    /// Continue and process this packet.
    Continue,
}

/// `AddressValidationInfo` holds information relevant to either
/// responding to address validation (`NewToken`, `Retry`) or generating
/// tokens for address validation (`Server`).
enum AddressValidationInfo {
    None,
    // We are a client and have information from `NEW_TOKEN`.
    NewToken(Vec<u8>),
    // We are a client and have received a `Retry` packet.
    Retry {
        token: Vec<u8>,
        retry_source_cid: ConnectionId,
    },
    // We are a server and can generate tokens.
    Server(Weak<RefCell<AddressValidation>>),
}

impl AddressValidationInfo {
    pub fn token(&self) -> &[u8] {
        match self {
            Self::NewToken(token) | Self::Retry { token, .. } => token,
            _ => &[],
        }
    }

    pub fn generate_new_token(&self, peer_address: SocketAddr, now: Instant) -> Option<Vec<u8>> {
        match self {
            Self::Server(w) => w
                .upgrade()?
                .borrow()
                .generate_new_token(peer_address, now)
                .ok(),
            Self::None => None,
            _ => unreachable!("called a server function on a client"),
        }
    }
}

/// A QUIC Connection
///
/// First, create a new connection using `new_client()` or `new_server()`.
///
/// For the life of the connection, handle activity in the following manner:
/// 1. Perform operations using the `stream_*()` methods.
/// 1. Call `process_input()` when a datagram is received or the timer expires. Obtain information
///    on connection state changes by checking `events()`.
/// 1. Having completed handling current activity, repeatedly call `process_output()` for packets to
///    send, until it returns `Output::Callback` or `Output::None`.
///
/// After the connection is closed (either by calling `close()` or by the
/// remote) continue processing until `state()` returns `Closed`.
pub struct Connection {
    role: Role,
    version: Version,
    state: State,
    tps: Rc<RefCell<TransportParametersHandler>>,
    /// What we are doing with 0-RTT.
    zero_rtt_state: ZeroRttState,
    /// All of the network paths that we are aware of.
    paths: Paths,
    /// This object will generate connection IDs for the connection.
    cid_manager: ConnectionIdManager,
    address_validation: AddressValidationInfo,
    /// The connection IDs that were provided by the peer.
    cids: ConnectionIdStore<[u8; 16]>,

    /// The source connection ID that this endpoint uses for the handshake.
    /// Since we need to communicate this to our peer in tparams, setting this
    /// value is part of constructing the struct.
    local_initial_source_cid: ConnectionId,
    /// The source connection ID from the first packet from the other end.
    /// This is checked against the peer's transport parameters.
    remote_initial_source_cid: Option<ConnectionId>,
    /// The destination connection ID from the first packet from the client.
    /// This is checked by the client against the server's transport parameters.
    original_destination_cid: Option<ConnectionId>,

    /// We sometimes save a datagram against the possibility that keys will later
    /// become available.  This avoids reporting packets as dropped during the handshake
    /// when they are either just reordered or we haven't been able to install keys yet.
    /// In particular, this occurs when asynchronous certificate validation happens.
    saved_datagrams: SavedDatagrams,
    /// Some packets were received, but not tracked.
    received_untracked: bool,

    /// This is responsible for the `QuicDatagrams`' handling:
    /// <https://datatracker.ietf.org/doc/html/draft-ietf-quic-datagram>
    quic_datagrams: QuicDatagrams,

    crypto: Crypto,
    acks: AckTracker,
    idle_timeout: IdleTimeout,
    streams: Streams,
    state_signaling: StateSignaling,
    loss_recovery: LossRecovery,
    events: ConnectionEvents,
    new_token: NewTokenState,
    stats: StatsCell,
    qlog: NeqoQlog,
    /// A session ticket was received without `NEW_TOKEN`,
    /// this is when that turns into an event without `NEW_TOKEN`.
    release_resumption_token_timer: Option<Instant>,
    conn_params: ConnectionParameters,
    hrtime: hrtime::Handle,

    /// For testing purposes it is sometimes necessary to inject frames that wouldn't
    /// otherwise be sent, just to see how a connection handles them.  Inserting them
    /// into packets proper mean that the frames follow the entire processing path.
    #[cfg(test)]
    test_frame_writer: Option<Box<dyn test_internal::FrameWriter>>,
}

impl Debug for Connection {
    fn fmt(&self, f: &mut Formatter) -> fmt::Result {
        write!(
            f,
            "{:?} Connection: {:?} {:?}",
            self.role,
            self.state,
            self.paths.primary()
        )
    }
}

impl Connection {
    /// A long default for timer resolution, so that we don't tax the
    /// system too hard when we don't need to.
    const LOOSE_TIMER_RESOLUTION: Duration = Duration::from_millis(50);

    /// Create a new QUIC connection with Client role.
    /// # Errors
    /// When NSS fails and an agent cannot be created.
    pub fn new_client(
        server_name: impl Into<String>,
        protocols: &[impl AsRef<str>],
        cid_generator: Rc<RefCell<dyn ConnectionIdGenerator>>,
        local_addr: SocketAddr,
        remote_addr: SocketAddr,
        conn_params: ConnectionParameters,
        now: Instant,
    ) -> Res<Self> {
        let dcid = ConnectionId::generate_initial();
        let mut c = Self::new(
            Role::Client,
            Agent::from(Client::new(server_name.into(), conn_params.is_greasing())?),
            cid_generator,
            protocols,
            conn_params,
        )?;
        c.crypto.states_mut().init(
            c.conn_params.get_versions().compatible(),
            Role::Client,
            &dcid,
        )?;
        c.original_destination_cid = Some(dcid);
        let path = Path::temporary(
            local_addr,
            remote_addr,
            &c.conn_params,
            NeqoQlog::default(),
            now,
            &mut c.stats.borrow_mut(),
        );
        c.setup_handshake_path(&Rc::new(RefCell::new(path)), now);
        Ok(c)
    }

    /// Create a new QUIC connection with Server role.
    /// # Errors
    /// When NSS fails and an agent cannot be created.
    pub fn new_server(
        certs: &[impl AsRef<str>],
        protocols: &[impl AsRef<str>],
        cid_generator: Rc<RefCell<dyn ConnectionIdGenerator>>,
        conn_params: ConnectionParameters,
    ) -> Res<Self> {
        Self::new(
            Role::Server,
            Agent::from(Server::new(certs)?),
            cid_generator,
            protocols,
            conn_params,
        )
    }

    fn new<P: AsRef<str>>(
        role: Role,
        agent: Agent,
        cid_generator: Rc<RefCell<dyn ConnectionIdGenerator>>,
        protocols: &[P],
        conn_params: ConnectionParameters,
    ) -> Res<Self> {
        // Setup the local connection ID.
        let local_initial_source_cid = cid_generator
            .borrow_mut()
            .generate_cid()
            .ok_or(Error::ConnectionIdsExhausted)?;
        let mut cid_manager =
            ConnectionIdManager::new(cid_generator, local_initial_source_cid.clone());
        let mut tps = conn_params.create_transport_parameter(role, &mut cid_manager)?;
        tps.local_mut()
            .set_bytes(InitialSourceConnectionId, local_initial_source_cid.to_vec());

        let tphandler = Rc::new(RefCell::new(tps));
        let crypto = Crypto::new(
            conn_params.get_versions().initial(),
            &conn_params,
            agent,
            protocols.iter().map(P::as_ref).map(String::from).collect(),
            Rc::clone(&tphandler),
        )?;

        let stats = StatsCell::default();
        let events = ConnectionEvents::default();
        let quic_datagrams = QuicDatagrams::new(
            conn_params.get_datagram_size(),
            conn_params.get_outgoing_datagram_queue(),
            conn_params.get_incoming_datagram_queue(),
            events.clone(),
        );

        let c = Self {
            role,
            version: conn_params.get_versions().initial(),
            state: State::Init,
            paths: Paths::default(),
            cid_manager,
            tps: Rc::clone(&tphandler),
            zero_rtt_state: ZeroRttState::Init,
            address_validation: AddressValidationInfo::None,
            local_initial_source_cid,
            remote_initial_source_cid: None,
            original_destination_cid: None,
            saved_datagrams: SavedDatagrams::default(),
            received_untracked: false,
            crypto,
            acks: AckTracker::default(),
            idle_timeout: IdleTimeout::new(conn_params.get_idle_timeout()),
            streams: Streams::new(tphandler, role, events.clone()),
            cids: ConnectionIdStore::default(),
            state_signaling: StateSignaling::Idle,
            loss_recovery: LossRecovery::new(stats.clone(), conn_params.get_fast_pto()),
            events,
            new_token: NewTokenState::new(role),
            stats,
            qlog: NeqoQlog::disabled(),
            release_resumption_token_timer: None,
            conn_params,
            hrtime: hrtime::Time::get(Self::LOOSE_TIMER_RESOLUTION),
            quic_datagrams,
            #[cfg(test)]
            test_frame_writer: None,
        };
        c.stats.borrow_mut().init(format!("{c}"));
        Ok(c)
    }

    /// # Errors
    /// When the operation fails.
    pub fn server_enable_0rtt(
        &mut self,
        anti_replay: &AntiReplay,
        zero_rtt_checker: impl ZeroRttChecker + 'static,
    ) -> Res<()> {
        self.crypto
            .server_enable_0rtt(Rc::clone(&self.tps), anti_replay, zero_rtt_checker)
    }

    /// # Errors
    /// When the operation fails.
    pub fn server_enable_ech(
        &mut self,
        config: u8,
        public_name: &str,
        sk: &PrivateKey,
        pk: &PublicKey,
    ) -> Res<()> {
        self.crypto.server_enable_ech(config, public_name, sk, pk)
    }

    /// Get the active ECH configuration, which is empty if ECH is disabled.
    #[must_use]
    pub fn ech_config(&self) -> &[u8] {
        self.crypto.ech_config()
    }

    /// # Errors
    /// When the operation fails.
    pub fn client_enable_ech(&mut self, ech_config_list: impl AsRef<[u8]>) -> Res<()> {
        self.crypto.client_enable_ech(ech_config_list)
    }

    /// Set or clear the qlog for this connection.
    pub fn set_qlog(&mut self, qlog: NeqoQlog) {
        self.loss_recovery.set_qlog(qlog.clone());
        self.paths.set_qlog(qlog.clone());
        self.qlog = qlog;
    }

    /// Get the qlog (if any) for this connection.
    pub fn qlog_mut(&mut self) -> &mut NeqoQlog {
        &mut self.qlog
    }

    /// Get the original destination connection id for this connection. This
    /// will always be present for `Role::Client` but not if `Role::Server` is in
    /// `State::Init`.
    #[must_use]
    pub const fn odcid(&self) -> Option<&ConnectionId> {
        self.original_destination_cid.as_ref()
    }

    /// Set a local transport parameter, possibly overriding a default value.
    /// This only sets transport parameters without dealing with other aspects of
    /// setting the value.
    ///
    /// # Errors
    /// When the transport parameter is invalid.
    /// # Panics
    /// This panics if the transport parameter is known to this crate.
    #[cfg(test)]
    pub fn set_local_tparam(
        &self,
        tp: TransportParameterId,
        value: tparams::TransportParameter,
    ) -> Res<()> {
        if *self.state() == State::Init {
            self.tps.borrow_mut().local_mut().set(tp, value);
            Ok(())
        } else {
            qerror!("Current state: {:?}", self.state());
            qerror!("Cannot set local tparam when not in an initial connection state");
            Err(Error::ConnectionState)
        }
    }

    /// `odcid` is their original choice for our CID, which we get from the Retry token.
    /// `remote_cid` is the value from the Source Connection ID field of an incoming packet: what
    /// the peer wants us to use now. `retry_cid` is what we asked them to use when we sent the
    /// Retry.
    pub(crate) fn set_retry_cids(
        &mut self,
        odcid: &ConnectionId,
        remote_cid: ConnectionId,
        retry_cid: &ConnectionId,
    ) {
        debug_assert_eq!(self.role, Role::Server);
        qtrace!("[{self}] Retry CIDs: odcid={odcid} remote={remote_cid} retry={retry_cid}");
        // We advertise "our" choices in transport parameters.
        self.tps
            .borrow_mut()
            .local_mut()
            .set_bytes(OriginalDestinationConnectionId, odcid.to_vec());
        self.tps
            .borrow_mut()
            .local_mut()
            .set_bytes(RetrySourceConnectionId, retry_cid.to_vec());

        // ...and save their choices for later validation.
        self.remote_initial_source_cid = Some(remote_cid);
    }

    fn retry_sent(&self) -> bool {
        self.tps
            .borrow()
            .local()
            .get_bytes(RetrySourceConnectionId)
            .is_some()
    }

    /// Set ALPN preferences. Strings that appear earlier in the list are given
    /// higher preference.
    /// # Errors
    /// When the operation fails, which is usually due to bad inputs or bad connection state.
    pub fn set_alpn(&mut self, protocols: &[impl AsRef<str>]) -> Res<()> {
        self.crypto.tls_mut().set_alpn(protocols)?;
        Ok(())
    }

    /// Enable a set of ciphers.
    /// # Errors
    /// When the operation fails, which is usually due to bad inputs or bad connection state.
    pub fn set_ciphers(&mut self, ciphers: &[Cipher]) -> Res<()> {
        if self.state != State::Init {
            qerror!("[{self}] Cannot enable ciphers in state {:?}", self.state);
            return Err(Error::ConnectionState);
        }
        self.crypto.tls_mut().set_ciphers(ciphers)?;
        Ok(())
    }

    /// Enable a set of key exchange groups.
    /// # Errors
    /// When the operation fails, which is usually due to bad inputs or bad connection state.
    pub fn set_groups(&mut self, groups: &[Group]) -> Res<()> {
        if self.state != State::Init {
            qerror!("[{self}] Cannot enable groups in state {:?}", self.state);
            return Err(Error::ConnectionState);
        }
        self.crypto.tls_mut().set_groups(groups)?;
        Ok(())
    }

    /// Set the number of additional key shares to send in the client hello.
    /// # Errors
    /// When the operation fails, which is usually due to bad inputs or bad connection state.
    pub fn send_additional_key_shares(&mut self, count: usize) -> Res<()> {
        if self.state != State::Init {
            qerror!("[{self}] Cannot enable groups in state {:?}", self.state);
            return Err(Error::ConnectionState);
        }
        self.crypto.tls_mut().send_additional_key_shares(count)?;
        Ok(())
    }

    fn make_resumption_token(&mut self) -> ResumptionToken {
        debug_assert_eq!(self.role, Role::Client);
        debug_assert!(self.crypto.has_resumption_token());
        // Values less than GRANULARITY are ignored when using the token, so use 0 where needed.
        let rtt = self.paths.primary().map_or_else(
            // If we don't have a path, we don't have an RTT.
            || Duration::from_millis(0),
            |p| {
                let rtt = p.borrow().rtt().estimate();
                if p.borrow().rtt().is_guesstimate() {
                    // When we have no actual RTT sample, do not encode a guestimated RTT larger
                    // than the default initial RTT. (The guess can be very large under lossy
                    // conditions.)
                    if rtt < INITIAL_RTT {
                        rtt
                    } else {
                        Duration::from_millis(0)
                    }
                } else {
                    rtt
                }
            },
        );

        self.crypto
            .create_resumption_token(
                self.new_token.take_token(),
                self.tps
                    .borrow()
                    .remote_handshake()
                    .as_ref()
                    .expect("should have transport parameters"),
                self.version,
                u64::try_from(rtt.as_millis()).unwrap_or(0),
            )
            .expect("caller checked if a resumption token existed")
    }

    fn confirmed(&self) -> bool {
        self.state == State::Confirmed
    }

    /// Get the simplest PTO calculation for all those cases where we need
    /// a value of this approximate order.  Don't use this for loss recovery,
    /// only use it where a more precise value is not important.
    fn pto(&self) -> Duration {
        self.paths.primary().map_or_else(
            || RttEstimate::default().pto(self.confirmed()),
            |p| p.borrow().rtt().pto(self.confirmed()),
        )
    }

    fn create_resumption_token(&mut self, now: Instant) {
        if self.role == Role::Server || self.state < State::Connected {
            return;
        }

        qtrace!(
            "[{self}] Maybe create resumption token: {} {}",
            self.crypto.has_resumption_token(),
            self.new_token.has_token()
        );

        while self.crypto.has_resumption_token() && self.new_token.has_token() {
            let token = self.make_resumption_token();
            self.events.client_resumption_token(token);
        }

        // If we have a resumption ticket check or set a timer.
        if self.crypto.has_resumption_token() {
            let arm = if let Some(expiration_time) = self.release_resumption_token_timer {
                if expiration_time <= now {
                    let token = self.make_resumption_token();
                    self.events.client_resumption_token(token);
                    self.release_resumption_token_timer = None;

                    // This means that we release one session ticket every 3 PTOs
                    // if no NEW_TOKEN frame is received.
                    self.crypto.has_resumption_token()
                } else {
                    false
                }
            } else {
                true
            };

            if arm {
                self.release_resumption_token_timer = Some(now + 3 * self.pto());
            }
        }
    }

    /// The correct way to obtain a resumption token is to wait for the
    /// `ConnectionEvent::ResumptionToken` event. To emit the event we are waiting for a
    /// resumption token and a `NEW_TOKEN` frame to arrive. Some servers don't send `NEW_TOKEN`
    /// frames and in this case, we wait for 3xPTO before emitting an event. This is especially a
    /// problem for short-lived connections, where the connection is closed before any events are
    /// released. This function retrieves the token, without waiting for a `NEW_TOKEN` frame to
    /// arrive.
    ///
    /// # Panics
    ///
    /// If this is called on a server.
    pub fn take_resumption_token(&mut self, now: Instant) -> Option<ResumptionToken> {
        assert_eq!(self.role, Role::Client);

        self.crypto.has_resumption_token().then(|| {
            let token = self.make_resumption_token();
            if self.crypto.has_resumption_token() {
                self.release_resumption_token_timer = Some(now + 3 * self.pto());
            }
            token
        })
    }

    /// Enable resumption, using a token previously provided.
    /// This can only be called once and only on the client.
    /// After calling the function, it should be possible to attempt 0-RTT
    /// if the token supports that.
    ///
    /// This function starts the TLS stack, which means that any configuration change
    /// to that stack needs to occur prior to calling this.
    ///
    /// # Errors
    /// When the operation fails, which is usually due to bad inputs or bad connection state.
    pub fn enable_resumption(&mut self, now: Instant, token: impl AsRef<[u8]>) -> Res<()> {
        if self.state != State::Init {
            qerror!("[{self}] set token in state {:?}", self.state);
            return Err(Error::ConnectionState);
        }
        if self.role == Role::Server {
            return Err(Error::ConnectionState);
        }

        qinfo!(
            "[{self}] resumption token {}",
            hex_snip_middle(token.as_ref())
        );
        let mut dec = Decoder::from(token.as_ref());

        let version = Version::try_from(
            dec.decode_uint::<WireVersion>()
                .ok_or(Error::InvalidResumptionToken)?,
        )?;
        qtrace!("[{self}]   version {version:?}");
        if !self.conn_params.get_versions().all().contains(&version) {
            return Err(Error::DisabledVersion);
        }

        let rtt = Duration::from_millis(dec.decode_varint().ok_or(Error::InvalidResumptionToken)?);
        qtrace!("[{self}]   RTT {rtt:?}");

        let tp_slice = dec.decode_vvec().ok_or(Error::InvalidResumptionToken)?;
        qtrace!("[{self}]   transport parameters {}", hex(tp_slice));
        let mut dec_tp = Decoder::from(tp_slice);
        let tp =
            TransportParameters::decode(&mut dec_tp).map_err(|_| Error::InvalidResumptionToken)?;

        let init_token = dec.decode_vvec().ok_or(Error::InvalidResumptionToken)?;
        qtrace!("[{self}]   Initial token {}", hex(init_token));

        let tok = dec.decode_remainder();
        qtrace!("[{self}]   TLS token {}", hex(tok));

        match self.crypto.tls_mut() {
            Agent::Client(ref mut c) => {
                let res = c.enable_resumption(tok);
                if let Err(e) = res {
                    self.absorb_error::<Error>(now, Err(Error::from(e)));
                    return Ok(());
                }
            }
            Agent::Server(_) => return Err(Error::WrongRole),
        }

        self.version = version;
        self.conn_params.get_versions_mut().set_initial(version);
        self.tps.borrow_mut().set_version(version);
        self.tps.borrow_mut().set_remote_0rtt(Some(tp));
        if !init_token.is_empty() {
            self.address_validation = AddressValidationInfo::NewToken(init_token.to_vec());
        }
        self.paths
            .primary()
            .ok_or(Error::InternalError)?
            .borrow_mut()
            .rtt_mut()
            .set_initial(rtt);
        self.set_initial_limits();
        // Start up TLS, which has the effect of setting up all the necessary
        // state for 0-RTT.  This only stages the CRYPTO frames.
        let res = self.client_start(now);
        self.absorb_error(now, res);
        Ok(())
    }

    pub(crate) fn set_validation(&mut self, validation: &Rc<RefCell<AddressValidation>>) {
        qtrace!("[{self}] Enabling NEW_TOKEN");
        assert_eq!(self.role, Role::Server);
        self.address_validation = AddressValidationInfo::Server(Rc::downgrade(validation));
    }

    /// Send a TLS session ticket AND a `NEW_TOKEN` frame (if possible).
    /// # Errors
    /// When the operation fails, which is usually due to bad inputs or bad connection state.
    pub fn send_ticket(&mut self, now: Instant, extra: &[u8]) -> Res<()> {
        if self.role == Role::Client {
            return Err(Error::WrongRole);
        }

        let tps = &self.tps;
        if let Agent::Server(ref mut s) = self.crypto.tls_mut() {
            let mut enc = Encoder::default();
            enc.encode_vvec_with(|enc_inner| {
                tps.borrow().local().encode(enc_inner);
            });
            enc.encode(extra);
            let records = s.send_ticket(now, enc.as_ref())?;
            qdebug!("[{self}] send session ticket {}", hex(&enc));
            self.crypto.buffer_records(records)?;
        } else {
            unreachable!();
        }

        // If we are able, also send a NEW_TOKEN frame.
        // This should be recording all remote addresses that are valid,
        // but there are just 0 or 1 in the current implementation.
        if let Some(path) = self.paths.primary() {
            if let Some(token) = self
                .address_validation
                .generate_new_token(path.borrow().remote_address(), now)
            {
                self.new_token.send_new_token(token);
            }
            Ok(())
        } else {
            Err(Error::NotConnected)
        }
    }

    #[must_use]
    pub fn tls_info(&self) -> Option<&SecretAgentInfo> {
        self.crypto.tls().info()
    }

    /// # Errors
    /// When there is no information to obtain.
    pub fn tls_preinfo(&self) -> Res<SecretAgentPreInfo> {
        Ok(self.crypto.tls().preinfo()?)
    }

    /// Get the peer's certificate chain and other info.
    #[must_use]
    pub fn peer_certificate(&self) -> Option<CertificateInfo> {
        self.crypto.tls().peer_certificate()
    }

    /// Call by application when the peer cert has been verified.
    ///
    /// This panics if there is no active peer.  It's OK to call this
    /// when authentication isn't needed, that will likely only cause
    /// the connection to fail.  However, if no packets have been
    /// exchanged, it's not OK.
    pub fn authenticated(&mut self, status: AuthenticationStatus, now: Instant) {
        qdebug!("[{self}] Authenticated {status:?}");
        self.crypto.tls_mut().authenticated(status);
        let res = self.handshake(now, self.version, PacketNumberSpace::Handshake, None);
        self.absorb_error(now, res);
        self.process_saved(now);
    }

    /// Get the role of the connection.
    #[must_use]
    pub const fn role(&self) -> Role {
        self.role
    }

    /// Get the state of the connection.
    #[must_use]
    pub const fn state(&self) -> &State {
        &self.state
    }

    /// The QUIC version in use.
    #[must_use]
    pub const fn version(&self) -> Version {
        self.version
    }

    /// Get the 0-RTT state of the connection.
    #[must_use]
    pub const fn zero_rtt_state(&self) -> ZeroRttState {
        self.zero_rtt_state
    }

    /// Get a snapshot of collected statistics.
    #[must_use]
    pub fn stats(&self) -> Stats {
        let mut v = self.stats.borrow().clone();
        if let Some(p) = self.paths.primary() {
            let p = p.borrow();
            v.rtt = p.rtt().estimate();
            v.rttvar = p.rtt().rttvar();
        }
        v
    }

    // This function wraps a call to another function and sets the connection state
    // properly if that call fails.
    fn capture_error<T>(
        &mut self,
        path: Option<PathRef>,
        now: Instant,
        frame_type: FrameType,
        res: Res<T>,
    ) -> Res<T> {
        if let Err(v) = &res {
            #[cfg(debug_assertions)]
            let msg = format!("{v:?}");
            #[cfg(not(debug_assertions))]
            let msg = "";
            let error = CloseReason::Transport(v.clone());
            match &self.state {
                State::Closing { error: err, .. }
                | State::Draining { error: err, .. }
                | State::Closed(err) => {
                    qwarn!("[{self}] Closing again after error {err:?}");
                }
                State::Init => {
                    // We have not even sent anything just close the connection without sending any
                    // error. This may happen when client_start fails.
                    self.set_state(State::Closed(error), now);
                }
                State::WaitInitial | State::WaitVersion => {
                    // We don't have any state yet, so don't bother with
                    // the closing state, just send one CONNECTION_CLOSE.
                    if let Some(path) = path.or_else(|| self.paths.primary()) {
                        self.state_signaling
                            .close(path, error.clone(), frame_type, msg);
                    }
                    self.set_state(State::Closed(error), now);
                }
                _ => {
                    if let Some(path) = path.or_else(|| self.paths.primary()) {
                        self.state_signaling
                            .close(path, error.clone(), frame_type, msg);
                        if matches!(v, Error::KeysExhausted) {
                            self.set_state(State::Closed(error), now);
                        } else {
                            self.set_state(
                                State::Closing {
                                    error,
                                    timeout: self.get_closing_period_time(now),
                                },
                                now,
                            );
                        }
                    } else {
                        self.set_state(State::Closed(error), now);
                    }
                }
            }
        }
        res
    }

    /// For use with `process_input()`. Errors there can be ignored, but this
    /// needs to ensure that the state is updated.
    fn absorb_error<T>(&mut self, now: Instant, res: Res<T>) -> Option<T> {
        self.capture_error(None, now, FrameType::Padding, res).ok()
    }

    fn process_timer(&mut self, now: Instant) {
        match &self.state {
            // Only the client runs timers while waiting for Initial packets.
            State::WaitInitial => debug_assert_eq!(self.role, Role::Client),
            // If Closing or Draining, check if it is time to move to Closed.
            State::Closing { error, timeout } | State::Draining { error, timeout } => {
                if *timeout <= now {
                    let st = State::Closed(error.clone());
                    self.set_state(st, now);
                    qinfo!("Closing timer expired");
                    return;
                }
            }
            State::Closed(_) => {
                qdebug!("Timer fired while closed");
                return;
            }
            _ => (),
        }

        let pto = self.pto();
        if self.idle_timeout.expired(now, pto) {
            qinfo!("[{self}] idle timeout expired");
            self.set_state(
                State::Closed(CloseReason::Transport(Error::IdleTimeout)),
                now,
            );
            return;
        }

        if self.state.closing() {
            qtrace!("[{self}] Closing, not processing other timers");
            return;
        }

        self.streams.cleanup_closed_streams();

        let res = self.crypto.states_mut().check_key_update(now);
        self.absorb_error(now, res);

        if let Some(path) = self.paths.primary() {
            let lost = self.loss_recovery.timeout(&path, now);
            self.handle_lost_packets(&lost);
            qlog::packets_lost(&self.qlog, &lost, now);
        }

        if self.release_resumption_token_timer.is_some() {
            self.create_resumption_token(now);
        }

        if !self
            .paths
            .process_timeout(now, pto, &mut self.stats.borrow_mut())
        {
            qinfo!("[{self}] last available path failed");
            self.absorb_error::<Error>(now, Err(Error::NoAvailablePath));
        }
    }

    /// Whether the given [`ConnectionIdRef`] is a valid local [`ConnectionId`].
    #[must_use]
    pub fn is_valid_local_cid(&self, cid: ConnectionIdRef) -> bool {
        self.cid_manager.is_valid(cid)
    }

    /// Process new input datagrams on the connection.
    pub fn process_input(&mut self, d: Datagram<impl AsRef<[u8]> + AsMut<[u8]>>, now: Instant) {
        self.process_multiple_input(iter::once(d), now);
    }

    /// Process new input datagrams on the connection.
    pub fn process_multiple_input(
        &mut self,
        dgrams: impl IntoIterator<Item = Datagram<impl AsRef<[u8]> + AsMut<[u8]>>>,
        now: Instant,
    ) {
        let mut dgrams = dgrams.into_iter().peekable();
        if dgrams.peek().is_none() {
            return;
        }

        for d in dgrams {
            self.input(d, now, now);
        }
        self.process_saved(now);
        self.streams.cleanup_closed_streams();
    }

    /// Get the time that we next need to be called back, relative to `now`.
    fn next_delay(&mut self, now: Instant, paced: bool) -> Duration {
        qtrace!("[{self}] Get callback delay {now:?}");

        // Only one timer matters when closing...
        if let State::Closing { timeout, .. } | State::Draining { timeout, .. } = self.state {
            self.hrtime.update(Self::LOOSE_TIMER_RESOLUTION);
            return timeout.duration_since(now);
        }

        let mut delays = SmallVec::<[_; 7]>::new();
        if let Some(ack_time) = self.acks.ack_time(now) {
            qtrace!("[{self}] Delayed ACK timer {ack_time:?}");
            delays.push(ack_time);
        }

        if let Some(p) = self.paths.primary() {
            let path = p.borrow();
            let rtt = path.rtt();
            let pto = rtt.pto(self.confirmed());

            let idle_time = self.idle_timeout.expiry(now, pto);
            qtrace!("[{self}] Idle timer {idle_time:?}");
            delays.push(idle_time);

            if self.streams.need_keep_alive() {
                if let Some(keep_alive_time) = self.idle_timeout.next_keep_alive(now, pto) {
                    qtrace!("[{self}] Keep alive timer {keep_alive_time:?}");
                    delays.push(keep_alive_time);
                }
            }

            if let Some(lr_time) = self.loss_recovery.next_timeout(&path) {
                qtrace!("[{self}] Loss recovery timer {lr_time:?}");
                delays.push(lr_time);
            }

            if paced {
                if let Some(pace_time) = path.sender().next_paced(rtt.estimate()) {
                    qtrace!("[{self}] Pacing timer {pace_time:?}");
                    delays.push(pace_time);
                }
            }

            if let Some(path_time) = self.paths.next_timeout(pto) {
                qtrace!("[{self}] Path probe timer {path_time:?}");
                delays.push(path_time);
            }
        }

        if let Some(key_update_time) = self.crypto.states().update_time() {
            qtrace!("[{self}] Key update timer {key_update_time:?}");
            delays.push(key_update_time);
        }

        // `release_resumption_token_timer` is not considered here, because
        // it is not important enough to force the application to set a
        // timeout for it  It is expected that other activities will
        // drive it.

        let earliest = delays.into_iter().min().expect("at least one delay");
        // TODO(agrover, mt) - need to analyze and fix #47
        // rather than just clamping to zero here.
        debug_assert!(earliest > now);
        let delay = earliest.saturating_duration_since(now);
        qdebug!("[{self}] delay duration {delay:?}");
        self.hrtime.update(delay / 4);
        delay
    }

    /// Get output packets, as a result of receiving packets, or actions taken
    /// by the application.
    /// Returns datagrams to send, and how long to wait before calling again
    /// even if no incoming packets.
    #[must_use = "Output of the process_output function must be handled"]
    pub fn process_output(&mut self, now: Instant) -> Output {
        qtrace!("[{self}] process_output {:?} {now:?}", self.state);

        match (&self.state, self.role) {
            (State::Init, Role::Client) => {
                let res = self.client_start(now);
                self.absorb_error(now, res);
            }
            (State::Init | State::WaitInitial, Role::Server) => {
                return Output::None;
            }
            _ => {
                self.process_timer(now);
            }
        }

        match self.output(now) {
            SendOption::Yes(dgram) => Output::Datagram(dgram),
            SendOption::No(paced) => match self.state {
                State::Init | State::Closed(_) => Output::None,
                State::Closing { timeout, .. } | State::Draining { timeout, .. } => {
                    Output::Callback(timeout.duration_since(now))
                }
                _ => Output::Callback(self.next_delay(now, paced)),
            },
        }
    }

    /// A test-only output function that uses the provided writer to
    /// pack something extra into the output.
    #[cfg(test)]
    pub fn test_write_frames<W>(&mut self, writer: W, now: Instant) -> Output
    where
        W: test_internal::FrameWriter + 'static,
    {
        self.test_frame_writer = Some(Box::new(writer));
        let res = self.process_output(now);
        self.test_frame_writer = None;
        res
    }

    /// Process input and generate output.
    #[must_use = "Output of the process function must be handled"]
    pub fn process(
        &mut self,
        dgram: Option<Datagram<impl AsRef<[u8]> + AsMut<[u8]>>>,
        now: Instant,
    ) -> Output {
        if let Some(d) = dgram {
            self.input(d, now, now);
            self.process_saved(now);
        }
        let output = self.process_output(now);
        #[cfg(all(feature = "build-fuzzing-corpus", test))]
        if self.test_frame_writer.is_none() {
            if let Some(d) = output.clone().dgram() {
                neqo_common::write_item_to_fuzzing_corpus("packet", &d);
            }
        }
        output
    }

    fn handle_retry(&mut self, packet: &PublicPacket, now: Instant) -> Res<()> {
        qinfo!("[{self}] received Retry");
        if matches!(self.address_validation, AddressValidationInfo::Retry { .. }) {
            self.stats.borrow_mut().pkt_dropped("Extra Retry");
            return Ok(());
        }
        if packet.token().is_empty() {
            self.stats.borrow_mut().pkt_dropped("Retry without a token");
            return Ok(());
        }
        if !packet.is_valid_retry(
            self.original_destination_cid
                .as_ref()
                .ok_or(Error::InvalidRetry)?,
        ) {
            self.stats
                .borrow_mut()
                .pkt_dropped("Retry with bad integrity tag");
            return Ok(());
        }
        // At this point, we should only have the connection ID that we generated.
        // Update to the one that the server prefers.
        let Some(path) = self.paths.primary() else {
            self.stats
                .borrow_mut()
                .pkt_dropped("Retry without an existing path");
            return Ok(());
        };

        path.borrow_mut().set_remote_cid(packet.scid());

        let retry_scid = ConnectionId::from(packet.scid());
        qinfo!(
            "[{self}] Valid Retry received, token={} scid={retry_scid}",
            hex(packet.token())
        );

        let lost_packets = self.loss_recovery.retry(&path, now);
        self.handle_lost_packets(&lost_packets);

        self.crypto.states_mut().init(
            self.conn_params.get_versions().compatible(),
            self.role,
            &retry_scid,
        )?;
        self.address_validation = AddressValidationInfo::Retry {
            token: packet.token().to_vec(),
            retry_source_cid: retry_scid,
        };
        Ok(())
    }

    fn discard_keys(&mut self, space: PacketNumberSpace, now: Instant) {
        if self.crypto.discard(space) {
            qdebug!("[{self}] Drop packet number space {space}");
            if let Some(path) = self.paths.primary() {
                self.loss_recovery.discard(&path, space, now);
            }
            self.acks.drop_space(space);
        }
    }

    fn is_stateless_reset(&self, path: &PathRef, d: &[u8]) -> bool {
        // If the datagram is too small, don't try.
        // If the connection is connected, then the reset token will be invalid.
        if d.len() < 16 || !self.state.connected() {
            return false;
        }
        <&[u8; 16]>::try_from(&d[d.len() - 16..])
            .is_ok_and(|token| path.borrow().is_stateless_reset(token))
    }

    fn check_stateless_reset(
        &mut self,
        path: &PathRef,
        d: &[u8],
        first: bool,
        now: Instant,
    ) -> Res<()> {
        if first && self.is_stateless_reset(path, d) {
            // Failing to process a packet in a datagram might
            // indicate that there is a stateless reset present.
            qdebug!("[{self}] Stateless reset: {}", hex(&d[d.len() - 16..]));
            self.state_signaling.reset();
            self.set_state(
                State::Draining {
                    error: CloseReason::Transport(Error::StatelessReset),
                    timeout: self.get_closing_period_time(now),
                },
                now,
            );
            Err(Error::StatelessReset)
        } else {
            Ok(())
        }
    }

    /// Process any saved datagrams that might be available for processing.
    fn process_saved(&mut self, now: Instant) {
        while let Some(epoch) = self.saved_datagrams.available() {
            qdebug!("[{self}] process saved for epoch {epoch:?}");
            debug_assert!(self
                .crypto
                .states_mut()
                .rx_hp(self.version, epoch)
                .is_some());
            for saved in self.saved_datagrams.take_saved() {
                qtrace!("[{self}] input saved @{:?}: {:?}", saved.t, saved.d);
                self.input(saved.d, saved.t, now);
            }
        }
    }

    /// In case a datagram arrives that we can only partially process, save any
    /// part that we don't have keys for.
    #[expect(
        clippy::needless_pass_by_value,
        reason = "To consume an owned datagram below."
    )]
    fn save_datagram(
        &mut self,
        epoch: Epoch,
        d: Datagram<impl AsRef<[u8]>>,
        remaining: usize,
        now: Instant,
    ) {
        let d = Datagram::new(
            d.source(),
            d.destination(),
            d.tos(),
            d[d.len() - remaining..].to_vec(),
        );
        self.saved_datagrams.save(epoch, d, now);
        self.stats.borrow_mut().saved_datagrams += 1;
        // We already counted the datagram as received in [`input_path`]. We
        // will do so again when we (re-)process it, so reduce the count now.
        self.stats.borrow_mut().packets_rx -= 1;
    }

    /// Perform version negotiation.
    fn version_negotiation(&mut self, supported: &[WireVersion], now: Instant) -> Res<()> {
        debug_assert_eq!(self.role, Role::Client);

        if let Some(version) = self.conn_params.get_versions().preferred(supported) {
            assert_ne!(self.version, version);

            qinfo!("[{self}] Version negotiation: trying {version:?}");
            let path = self.paths.primary().ok_or(Error::NoAvailablePath)?;
            let local_addr = path.borrow().local_address();
            let remote_addr = path.borrow().remote_address();
            let conn_params = self
                .conn_params
                .clone()
                .versions(version, self.conn_params.get_versions().all().to_vec());
            let mut c = Self::new_client(
                self.crypto.server_name().ok_or(Error::VersionNegotiation)?,
                self.crypto.protocols(),
                self.cid_manager.generator(),
                local_addr,
                remote_addr,
                conn_params,
                now,
            )?;
            c.conn_params
                .get_versions_mut()
                .set_initial(self.conn_params.get_versions().initial());
            mem::swap(self, &mut c);
            qlog::client_version_information_negotiated(
                &self.qlog,
                self.conn_params.get_versions().all(),
                supported,
                version,
                now,
            );
            Ok(())
        } else {
            qinfo!("[{self}] Version negotiation: failed with {supported:?}");
            // This error goes straight to closed.
            self.set_state(
                State::Closed(CloseReason::Transport(Error::VersionNegotiation)),
                now,
            );
            Err(Error::VersionNegotiation)
        }
    }

    /// Perform any processing that we might have to do on packets prior to
    /// attempting to remove protection.
    #[expect(clippy::too_many_lines, reason = "Yeah, it's a work in progress.")]
    fn preprocess_packet(
        &mut self,
        packet: &PublicPacket,
        path: &PathRef,
        dcid: Option<&ConnectionId>,
        now: Instant,
    ) -> Res<PreprocessResult> {
        if dcid.is_some_and(|d| d != &packet.dcid()) {
            self.stats
                .borrow_mut()
                .pkt_dropped("Coalesced packet has different DCID");
            return Ok(PreprocessResult::Next);
        }

        if (packet.packet_type() == PacketType::Initial
            || packet.packet_type() == PacketType::Handshake)
            && self.role == Role::Client
            && !path.borrow().is_primary()
        {
            // If we have received a packet from a different address than we have sent to
            // we should ignore the packet. In such a case a path will be a newly created
            // temporary path, not the primary path.
            return Ok(PreprocessResult::Next);
        }

        match (packet.packet_type(), &self.state, &self.role) {
            (PacketType::Initial, State::Init, Role::Server) => {
                let version = packet.version().ok_or(Error::ProtocolViolation)?;
                if !packet.is_valid_initial()
                    || !self.conn_params.get_versions().all().contains(&version)
                {
                    self.stats.borrow_mut().pkt_dropped("Invalid Initial");
                    return Ok(PreprocessResult::Next);
                }
                qinfo!(
                    "[{self}] Received valid Initial packet with scid {:?} dcid {:?}",
                    packet.scid(),
                    packet.dcid()
                );
                // Record the client's selected CID so that it can be accepted until
                // the client starts using a real connection ID.
                let dcid = ConnectionId::from(packet.dcid());
                self.crypto.states_mut().init_server(version, &dcid)?;
                self.original_destination_cid = Some(dcid);
                self.set_state(State::WaitInitial, now);

                // We need to make sure that we set this transport parameter.
                // This has to happen prior to processing the packet so that
                // the TLS handshake has all it needs.
                if !self.retry_sent() {
                    self.tps
                        .borrow_mut()
                        .local_mut()
                        .set_bytes(OriginalDestinationConnectionId, packet.dcid().to_vec());
                }
            }
            (PacketType::VersionNegotiation, State::WaitInitial, Role::Client) => {
                if let Ok(versions) = packet.supported_versions() {
                    if versions.is_empty()
                        || versions.contains(&self.version().wire_version())
                        || versions.contains(&0)
                        || &packet.scid() != self.odcid().ok_or(Error::InternalError)?
                        || matches!(self.address_validation, AddressValidationInfo::Retry { .. })
                    {
                        // Ignore VersionNegotiation packets that contain the current version.
                        // Or don't have the right connection ID.
                        // Or are received after a Retry.
                        self.stats.borrow_mut().pkt_dropped("Invalid VN");
                    } else {
                        self.version_negotiation(&versions, now)?;
                    }
                } else {
                    self.stats.borrow_mut().pkt_dropped("VN with no versions");
                }
                return Ok(PreprocessResult::End);
            }
            (PacketType::Retry, State::WaitInitial, Role::Client) => {
                self.handle_retry(packet, now)?;
                return Ok(PreprocessResult::Next);
            }
            (PacketType::Handshake | PacketType::Short, State::WaitInitial, Role::Client) => {
                // This packet can't be processed now, but it could be a sign
                // that Initial packets were lost.
                // Resend Initial CRYPTO frames immediately a few times just
                // in case.  As we don't have an RTT estimate yet, this helps
                // when there is a short RTT and losses. Also mark all 0-RTT
                // data as lost.
                if dcid.is_none()
                    && self.cid_manager.is_valid(packet.dcid())
                    && self.stats.borrow().saved_datagrams <= EXTRA_INITIALS
                {
                    self.crypto.resend_unacked(PacketNumberSpace::Initial);
                    self.resend_0rtt(now);
                }
            }
            (PacketType::VersionNegotiation | PacketType::Retry | PacketType::OtherVersion, ..) => {
                self.stats
                    .borrow_mut()
                    .pkt_dropped(format!("{:?}", packet.packet_type()));
                return Ok(PreprocessResult::Next);
            }
            _ => {}
        }

        let res = match self.state {
            State::Init => {
                self.stats
                    .borrow_mut()
                    .pkt_dropped("Received while in Init state");
                PreprocessResult::Next
            }
            State::WaitInitial => PreprocessResult::Continue,
            State::WaitVersion | State::Handshaking | State::Connected | State::Confirmed => {
                if self.cid_manager.is_valid(packet.dcid()) {
                    if self.role == Role::Server && packet.packet_type() == PacketType::Handshake {
                        // Server has received a Handshake packet -> discard Initial keys and states
                        self.discard_keys(PacketNumberSpace::Initial, now);
                    }
                    PreprocessResult::Continue
                } else {
                    self.stats
                        .borrow_mut()
                        .pkt_dropped(format!("Invalid DCID {:?}", packet.dcid()));
                    PreprocessResult::Next
                }
            }
            State::Closing { .. } => {
                // Don't bother processing the packet. Instead ask to get a
                // new close frame.
                self.state_signaling.send_close();
                PreprocessResult::Next
            }
            State::Draining { .. } | State::Closed(..) => {
                // Do nothing.
                self.stats
                    .borrow_mut()
                    .pkt_dropped(format!("State {:?}", self.state));
                PreprocessResult::Next
            }
        };
        Ok(res)
    }

    /// After a Initial, Handshake, `ZeroRtt`, or Short packet is successfully processed.
    #[expect(clippy::too_many_arguments, reason = "Yes, but they're needed.")]
    fn postprocess_packet(
        &mut self,
        path: &PathRef,
        tos: IpTos,
        remote: SocketAddr,
        packet: &PublicPacket,
        packet_number: PacketNumber,
        migrate: bool,
        now: Instant,
    ) {
        let ecn_mark = IpTosEcn::from(tos);
        let mut stats = self.stats.borrow_mut();
        stats.ecn_rx[packet.packet_type()] += ecn_mark;
        if let Some(last_ecn_mark) = stats.ecn_last_mark.filter(|&last_ecn_mark| {
            last_ecn_mark != ecn_mark && stats.ecn_rx_transition[last_ecn_mark][ecn_mark].is_none()
        }) {
            stats.ecn_rx_transition[last_ecn_mark][ecn_mark] =
                Some((packet.packet_type(), packet_number));
        }

        stats.ecn_last_mark = Some(ecn_mark);
        drop(stats);
        let space = PacketNumberSpace::from(packet.packet_type());
        if let Some(space) = self.acks.get_mut(space) {
            *space.ecn_marks() += ecn_mark;
        } else {
            qtrace!("Not tracking ECN for dropped packet number space");
        }

        if self.state == State::WaitInitial {
            self.start_handshake(path, packet, now);
        }

        if matches!(self.state, State::WaitInitial | State::WaitVersion) {
            let new_state = if self.has_version() {
                State::Handshaking
            } else {
                State::WaitVersion
            };
            self.set_state(new_state, now);
            if self.role == Role::Server && self.state == State::Handshaking {
                self.zero_rtt_state =
                    if self.crypto.enable_0rtt(self.version, self.role) == Ok(true) {
                        qdebug!("[{self}] Accepted 0-RTT");
                        ZeroRttState::AcceptedServer
                    } else {
                        ZeroRttState::Rejected
                    };
            }
        }

        if self.state.connected() {
            self.handle_migration(path, remote, migrate, now);
        } else if self.role != Role::Client
            && (packet.packet_type() == PacketType::Handshake
                || (packet.dcid().len() >= 8 && packet.dcid() == self.local_initial_source_cid))
        {
            // We only allow one path during setup, so apply handshake
            // path validation to this path.
            path.borrow_mut().set_valid(now);
        }
    }

    /// Take a datagram as input.  This reports an error if the packet was bad.
    /// This takes two times: when the datagram was received, and the current time.
    fn input(
        &mut self,
        d: Datagram<impl AsRef<[u8]> + AsMut<[u8]>>,
        received: Instant,
        now: Instant,
    ) {
        // First determine the path.
        let path = self.paths.find_path(
            d.destination(),
            d.source(),
            &self.conn_params,
            now,
            &mut self.stats.borrow_mut(),
        );
        path.borrow_mut().add_received(d.len());
        let res = self.input_path(&path, d, received);
        _ = self.capture_error(Some(path), now, FrameType::Padding, res);
    }

    fn input_path(
        &mut self,
        path: &PathRef,
        mut d: Datagram<impl AsRef<[u8]> + AsMut<[u8]>>,
        now: Instant,
    ) -> Res<()> {
        qtrace!("[{self}] {} input {}", path.borrow(), hex(&d));
        let tos = d.tos();
        let remote = d.source();
        let mut slc = d.as_mut();
        let mut dcid = None;
        let pto = path.borrow().rtt().pto(self.confirmed());

        // Handle each packet in the datagram.
        while !slc.is_empty() {
            self.stats.borrow_mut().packets_rx += 1;
            let slc_len = slc.len();
            let (mut packet, remainder) =
                match PublicPacket::decode(slc, self.cid_manager.decoder().as_ref()) {
                    Ok((packet, remainder)) => (packet, remainder),
                    Err(e) => {
                        qinfo!("[{self}] Garbage packet: {e}");
                        self.stats.borrow_mut().pkt_dropped("Garbage packet");
                        break;
                    }
                };
            self.stats.borrow_mut().dscp_rx[tos.into()] += 1;
            match self.preprocess_packet(&packet, path, dcid.as_ref(), now)? {
                PreprocessResult::Continue => (),
                PreprocessResult::Next => break,
                PreprocessResult::End => return Ok(()),
            }

            qtrace!("[{self}] Received unverified packet {packet:?}");

            let packet_len = packet.len();
            match packet.decrypt(self.crypto.states_mut(), now + pto) {
                Ok(payload) => {
                    // OK, we have a valid packet.
                    let pn = payload.pn();
                    self.idle_timeout.on_packet_received(now);
                    self.log_packet(
                        packet::MetaData::new_in(path, tos, packet_len, &payload),
                        now,
                    );

                    #[cfg(feature = "build-fuzzing-corpus")]
                    if payload.packet_type() == PacketType::Initial {
                        let target = if self.role == Role::Client {
                            "server_initial"
                        } else {
                            "client_initial"
                        };
                        neqo_common::write_item_to_fuzzing_corpus(target, &payload[..]);
                    }

                    let space = PacketNumberSpace::from(payload.packet_type());
                    if let Some(space) = self.acks.get_mut(space) {
                        if space.is_duplicate(pn) {
                            qdebug!("Duplicate packet {space}-{}", pn);
                            self.stats.borrow_mut().dups_rx += 1;
                        } else {
                            match self.process_packet(path, &payload, now) {
                                Ok(migrate) => {
                                    self.postprocess_packet(
                                        path, tos, remote, &packet, pn, migrate, now,
                                    );
                                }
                                Err(e) => {
                                    self.ensure_error_path(path, &packet, now);
                                    return Err(e);
                                }
                            }
                        }
                    } else {
                        qdebug!(
                            "[{self}] Received packet {space} for untracked space {}",
                            payload.pn()
                        );
                        return Err(Error::ProtocolViolation);
                    }
                }
                Err(e) => {
                    match e {
                        Error::KeysPending(epoch) => {
                            // This packet can't be decrypted because we don't have the keys yet.
                            // Don't check this packet for a stateless reset, just return.
                            let remaining = slc_len;
                            self.save_datagram(epoch, d, remaining, now);
                            return Ok(());
                        }
                        Error::KeysExhausted => {
                            // Exhausting read keys is fatal.
                            return Err(e);
                        }
                        Error::KeysDiscarded(epoch) => {
                            // This was a valid-appearing Initial packet: maybe probe with
                            // a Handshake packet to keep the handshake moving.
                            self.received_untracked |=
                                self.role == Role::Client && epoch == Epoch::Initial;
                        }
                        _ => (),
                    }
                    // Decryption failure, or not having keys is not fatal.
                    // If the state isn't available, or we can't decrypt the packet, drop
                    // the rest of the datagram on the floor, but don't generate an error.
                    self.check_stateless_reset(path, packet.data(), dcid.is_none(), now)?;
                    self.stats.borrow_mut().pkt_dropped("Decryption failure");
                    qlog::packet_dropped(&self.qlog, &packet, now);
                }
            }
            slc = remainder;
            dcid = Some(ConnectionId::from(packet.dcid()));
        }
        self.check_stateless_reset(path, &d, dcid.is_none(), now)?;
        Ok(())
    }

    /// Process a packet.  Returns true if the packet might initiate migration.
    fn process_packet(
        &mut self,
        path: &PathRef,
        packet: &DecryptedPacket,
        now: Instant,
    ) -> Res<bool> {
        (!packet.is_empty())
            .then_some(())
            .ok_or(Error::ProtocolViolation)?;

        // TODO(ekr@rtfm.com): Have the server blow away the initial
        // crypto state if this fails? Otherwise, we will get a panic
        // on the assert for doesn't exist.
        // OK, we have a valid packet.

        // Get the next packet number we'll send, for ACK verification.
        // TODO: Once PR #2118 lands, this can move to `input_frame`. For now, it needs to be here,
        // because we can drop packet number spaces as we parse through the packet, and if an ACK
        // frame follows a CRYPTO frame that makes us drop a space, we need to know this
        // packet number to verify the ACK against.
        let next_pn = self
            .crypto
            .states()
            .select_tx(self.version, PacketNumberSpace::from(packet.packet_type()))
            .map_or(0, |(_, tx)| tx.next_pn());

        let mut ack_eliciting = false;
        let mut probing = true;
        let mut d = Decoder::from(&packet[..]);
        while d.remaining() > 0 {
            #[cfg(feature = "build-fuzzing-corpus")]
            let pos = d.offset();
            let f = Frame::decode(&mut d)?;
            #[cfg(feature = "build-fuzzing-corpus")]
            neqo_common::write_item_to_fuzzing_corpus("frame", &packet[pos..d.offset()]);
            ack_eliciting |= f.ack_eliciting();
            probing &= f.path_probing();
            let t = f.get_type();
            if let Err(e) = self.input_frame(
                path,
                packet.version(),
                packet.packet_type(),
                f,
                next_pn,
                now,
            ) {
                self.capture_error(Some(Rc::clone(path)), now, t, Err(e))?;
            }
        }

        let largest_received = if let Some(space) = self
            .acks
            .get_mut(PacketNumberSpace::from(packet.packet_type()))
        {
            space.set_received(now, packet.pn(), ack_eliciting)?
        } else {
            qdebug!(
                "[{self}] processed a {:?} packet without tracking it",
                packet.packet_type(),
            );
            // This was a valid packet that caused the same packet number to be
            // discarded.  This happens when the client discards the Initial packet
            // number space after receiving the ServerHello.  Remember this so
            // that we guarantee that we send a Handshake packet.
            self.received_untracked = true;
            // We don't migrate during the handshake, so return false.
            false
        };

        Ok(largest_received && !probing)
    }

    /// During connection setup, the first path needs to be setup.
    /// This uses the connection IDs that were provided during the handshake
    /// to setup that path.
    fn setup_handshake_path(&mut self, path: &PathRef, now: Instant) {
        self.paths.make_permanent(
            path,
            Some(self.local_initial_source_cid.clone()),
            // Ideally we know what the peer wants us to use for the remote CID.
            // But we will use our own guess if necessary.
            ConnectionIdEntry::initial_remote(
                self.remote_initial_source_cid
                    .as_ref()
                    .or(self.original_destination_cid.as_ref())
                    .expect("have either remote_initial_source_cid or original_destination_cid")
                    .clone(),
            ),
            now,
        );
        if self.role == Role::Client {
            path.borrow_mut().set_valid(now);
        }
    }

    /// If the path isn't permanent, assign it a connection ID to make it so.
    fn ensure_permanent(&mut self, path: &PathRef, now: Instant) -> Res<()> {
        if self.paths.is_temporary(path) {
            // If there isn't a connection ID to use for this path, the packet
            // will be processed, but it won't be attributed to a path.  That means
            // no path probes or PATH_RESPONSE.  But it's not fatal.
            if let Some(cid) = self.cids.next() {
                self.paths.make_permanent(path, None, cid, now);
                Ok(())
            } else if let Some(primary) = self.paths.primary() {
                if primary.borrow().remote_cid().is_none_or(|id| id.is_empty()) {
                    self.paths
                        .make_permanent(path, None, ConnectionIdEntry::empty_remote(), now);
                    Ok(())
                } else {
                    qtrace!("[{self}] Unable to make path permanent: {}", path.borrow());
                    Err(Error::InvalidMigration)
                }
            } else {
                qtrace!("[{self}] Unable to make path permanent: {}", path.borrow());
                Err(Error::InvalidMigration)
            }
        } else {
            Ok(())
        }
    }

    /// After an error, a permanent path is needed to send the `CONNECTION_CLOSE`.
    /// This attempts to ensure that this exists.  As the connection is now
    /// temporary, there is no reason to do anything special here.
    fn ensure_error_path(&mut self, path: &PathRef, packet: &PublicPacket, now: Instant) {
        path.borrow_mut().set_valid(now);
        if self.paths.is_temporary(path) {
            // First try to fill in handshake details.
            if packet.packet_type() == PacketType::Initial {
                self.remote_initial_source_cid = Some(ConnectionId::from(packet.scid()));
                self.setup_handshake_path(path, now);
            } else {
                // Otherwise try to get a usable connection ID.
                drop(self.ensure_permanent(path, now));
            }
        }
    }

    fn start_handshake(&mut self, path: &PathRef, packet: &PublicPacket, now: Instant) {
        qtrace!("[{self}] starting handshake");
        debug_assert_eq!(packet.packet_type(), PacketType::Initial);
        self.remote_initial_source_cid = Some(ConnectionId::from(packet.scid()));

        if self.role == Role::Server {
            let Some(original_destination_cid) = self.original_destination_cid.as_ref() else {
                qdebug!("[{self}] No original destination DCID");
                return;
            };
            self.cid_manager.add_odcid(original_destination_cid.clone());
            // Make a path on which to run the handshake.
            self.setup_handshake_path(path, now);
        } else {
            qdebug!("[{self}] Changing to use Server CID={}", packet.scid());
            debug_assert!(path.borrow().is_primary());
            path.borrow_mut().set_remote_cid(packet.scid());
        }
    }

    fn has_version(&self) -> bool {
        if self.role == Role::Server {
            // The server knows the final version if it has remote transport parameters.
            self.tps.borrow().remote_handshake().is_some()
        } else {
            // The client knows the final version if it processed a CRYPTO frame.
            self.stats.borrow().frame_rx.crypto > 0
        }
    }

    /// Migrate to the provided path.
    /// Either local or remote address (but not both) may be provided as `None` to have
    /// the address from the current primary path used.
    /// If `force` is true, then migration is immediate.
    /// Otherwise, migration occurs after the path is probed successfully.
    /// Either way, the path is probed and will be abandoned if the probe fails.
    ///
    /// # Errors
    ///
    /// Fails if this is not a client, not confirmed, the peer disabled connection migration, or
    /// there are not enough connection IDs available to use.
    pub fn migrate(
        &mut self,
        local: Option<SocketAddr>,
        remote: Option<SocketAddr>,
        force: bool,
        now: Instant,
    ) -> Res<()> {
        if self.role != Role::Client {
            return Err(Error::InvalidMigration);
        }
        if !matches!(self.state(), State::Confirmed) {
            return Err(Error::InvalidMigration);
        }
        if self.tps.borrow().remote().get_empty(DisableMigration) {
            return Err(Error::InvalidMigration);
        }

        // Fill in the blanks, using the current primary path.
        if local.is_none() && remote.is_none() {
            // Pointless migration is pointless.
            return Err(Error::InvalidMigration);
        }

        let path = self.paths.primary().ok_or(Error::InvalidMigration)?;
        let local = local.unwrap_or_else(|| path.borrow().local_address());
        let remote = remote.unwrap_or_else(|| path.borrow().remote_address());

        if mem::discriminant(&local.ip()) != mem::discriminant(&remote.ip()) {
            // Can't mix address families.
            return Err(Error::InvalidMigration);
        }
        if local.port() == 0 || remote.ip().is_unspecified() || remote.port() == 0 {
            // All but the local address need to be specified.
            return Err(Error::InvalidMigration);
        }
        if (local.ip().is_loopback() ^ remote.ip().is_loopback()) && !local.ip().is_unspecified() {
            // Block attempts to migrate to a path with loopback on only one end, unless the local
            // address is unspecified.
            return Err(Error::InvalidMigration);
        }

        let path = self.paths.find_path(
            local,
            remote,
            &self.conn_params,
            now,
            &mut self.stats.borrow_mut(),
        );
        self.ensure_permanent(&path, now)?;
        qinfo!(
            "[{self}] Migrate to {} probe {}",
            path.borrow(),
            if force { "now" } else { "after" }
        );
        if self
            .paths
            .migrate(&path, force, now, &mut self.stats.borrow_mut())
        {
            self.loss_recovery.migrate();
        }
        Ok(())
    }

    fn migrate_to_preferred_address(&mut self, now: Instant) -> Res<()> {
        let spa: Option<(tparams::PreferredAddress, ConnectionIdEntry<[u8; 16]>)> = if matches!(
            self.conn_params.get_preferred_address(),
            PreferredAddressConfig::Disabled
        ) {
            qdebug!("[{self}] Preferred address is disabled");
            None
        } else {
            self.tps.borrow_mut().remote().get_preferred_address()
        };
        if let Some((addr, cid)) = spa {
            // The connection ID isn't special, so just save it.
            self.cids.add_remote(cid)?;

            // The preferred address doesn't dictate what the local address is, so this
            // has to use the existing address.  So only pay attention to a preferred
            // address from the same family as is currently in use. More thought will
            // be needed to work out how to get addresses from a different family.
            let prev = self
                .paths
                .primary()
                .ok_or(Error::NoAvailablePath)?
                .borrow()
                .remote_address();
            let remote = match prev.ip() {
                IpAddr::V4(_) => addr.ipv4().map(SocketAddr::V4),
                IpAddr::V6(_) => addr.ipv6().map(SocketAddr::V6),
            };

            if let Some(remote) = remote {
                // Ignore preferred address that move to loopback from non-loopback.
                // `migrate` doesn't enforce this rule.
                if !prev.ip().is_loopback() && remote.ip().is_loopback() {
                    qwarn!("[{self}] Ignoring a move to a loopback address: {remote}");
                    return Ok(());
                }

                if self.migrate(None, Some(remote), false, now).is_err() {
                    qwarn!("[{self}] Ignoring bad preferred address: {remote}");
                }
            } else {
                qwarn!("[{self}] Unable to migrate to a different address family");
            }
        } else {
            qdebug!("[{self}] No preferred address to migrate to");
        }
        Ok(())
    }

    fn handle_migration(
        &mut self,
        path: &PathRef,
        remote: SocketAddr,
        migrate: bool,
        now: Instant,
    ) {
        if !migrate {
            return;
        }
        if self.role == Role::Client {
            return;
        }

        if self.ensure_permanent(path, now).is_ok() {
            self.paths
                .handle_migration(path, remote, now, &mut self.stats.borrow_mut());
        } else {
            qinfo!(
                "[{self}] {} Peer migrated, but no connection ID available",
                path.borrow()
            );
        }
    }

    fn output(&mut self, now: Instant) -> SendOption {
        qtrace!("[{self}] output {now:?}");
        let res = match &self.state {
            State::Init
            | State::WaitInitial
            | State::WaitVersion
            | State::Handshaking
            | State::Connected
            | State::Confirmed => self.paths.select_path().map_or_else(
                || Ok(SendOption::default()),
                |path| {
                    let res = self.output_path(&path, now, None);
                    self.capture_error(Some(path), now, FrameType::Padding, res)
                },
            ),
            State::Closing { .. } | State::Draining { .. } | State::Closed(_) => {
                self.state_signaling.close_frame().map_or_else(
                    || Ok(SendOption::default()),
                    |details| {
                        let path = Rc::clone(details.path());
                        // In some error cases, we will not be able to make a new, permanent path.
                        // For example, if we run out of connection IDs and the error results from
                        // a packet on a new path, we avoid sending (and the privacy risk) rather
                        // than reuse a connection ID.
                        let res = if path.borrow().is_temporary() {
                            qerror!("[{self}] Attempting to close with a temporary path");
                            Err(Error::InternalError)
                        } else {
                            self.output_path(&path, now, Some(&details))
                        };
                        self.capture_error(Some(path), now, FrameType::Padding, res)
                    },
                )
            }
        };
        res.unwrap_or_default()
    }

    fn build_packet_header(
        path: &Path,
        epoch: Epoch,
        encoder: Encoder,
        tx: &CryptoDxState,
        address_validation: &AddressValidationInfo,
        version: Version,
        grease_quic_bit: bool,
    ) -> (PacketType, PacketBuilder) {
        let pt = PacketType::from(epoch);
        let mut builder = if pt == PacketType::Short {
            qdebug!("Building Short dcid {:?}", path.remote_cid());
            PacketBuilder::short(encoder, tx.key_phase(), path.remote_cid())
        } else {
            qdebug!(
                "Building {pt:?} dcid {:?} scid {:?}",
                path.remote_cid(),
                path.local_cid(),
            );
            PacketBuilder::long(encoder, pt, version, path.remote_cid(), path.local_cid())
        };
        if builder.remaining() > 0 {
            builder.scramble(grease_quic_bit);
            if pt == PacketType::Initial {
                builder.initial_token(address_validation.token());
            }
        }

        (pt, builder)
    }

    #[must_use]
    fn add_packet_number(
        builder: &mut PacketBuilder,
        tx: &CryptoDxState,
        largest_acknowledged: Option<PacketNumber>,
    ) -> PacketNumber {
        // Get the packet number and work out how long it is.
        let pn = tx.next_pn();
        let unacked_range = largest_acknowledged.map_or_else(|| pn + 1, |la| (pn - la) << 1);
        // Count how many bytes in this range are non-zero.
        let pn_len = size_of::<PacketNumber>()
            - usize::try_from(unacked_range.leading_zeros() / 8).expect("u32 fits in usize");
        assert!(
            pn_len > 0,
            "pn_len can't be zero as unacked_range should be > 0, pn {pn}, largest_acknowledged {largest_acknowledged:?}, tx {tx}"
        );
        // TODO(mt) also use `4*path CWND/path MTU` to set a minimum length.
        builder.pn(pn, pn_len);
        pn
    }

    fn can_grease_quic_bit(&self) -> bool {
        let tph = self.tps.borrow();
        tph.remote_handshake().as_ref().map_or_else(
            || {
                tph.remote_0rtt()
                    .is_some_and(|r| r.get_empty(GreaseQuicBit))
            },
            |r| r.get_empty(GreaseQuicBit),
        )
    }

    /// Write the frames that are exchanged in the application data space.
    /// The order of calls here determines the relative priority of frames.
    fn write_appdata_frames(
        &mut self,
        builder: &mut PacketBuilder,
        tokens: &mut Vec<RecoveryToken>,
        now: Instant,
    ) {
        let rtt = self.paths.primary().map_or_else(
            || RttEstimate::default().estimate(),
            |p| p.borrow().rtt().estimate(),
        );

        let stats = &mut self.stats.borrow_mut();
        let frame_stats = &mut stats.frame_tx;
        if self.role == Role::Server {
            if let Some(t) = self.state_signaling.write_done(builder) {
                tokens.push(t);
                frame_stats.handshake_done += 1;
            }
        }

        self.streams
            .write_frames(TransmissionPriority::Critical, builder, tokens, frame_stats);
        if builder.is_full() {
            return;
        }

        self.streams
            .write_maintenance_frames(builder, tokens, frame_stats, now, rtt);
        if builder.is_full() {
            return;
        }

        self.streams.write_frames(
            TransmissionPriority::Important,
            builder,
            tokens,
            frame_stats,
        );
        if builder.is_full() {
            return;
        }

        // NEW_CONNECTION_ID, RETIRE_CONNECTION_ID, and ACK_FREQUENCY.
        self.cid_manager.write_frames(builder, tokens, frame_stats);
        if builder.is_full() {
            return;
        }

        self.paths.write_frames(builder, tokens, frame_stats);
        if builder.is_full() {
            return;
        }

        for prio in [TransmissionPriority::High, TransmissionPriority::Normal] {
            self.streams
                .write_frames(prio, builder, tokens, &mut stats.frame_tx);
            if builder.is_full() {
                return;
            }
        }

        // Datagrams are best-effort and unreliable.  Let streams starve them for now.
        self.quic_datagrams.write_frames(builder, tokens, stats);
        if builder.is_full() {
            return;
        }

        // CRYPTO here only includes NewSessionTicket, plus NEW_TOKEN.
        // Both of these are only used for resumption and so can be relatively low priority.
        let frame_stats = &mut stats.frame_tx;
        self.crypto.write_frame(
            PacketNumberSpace::ApplicationData,
            self.conn_params.sni_slicing_enabled(),
            builder,
            tokens,
            frame_stats,
        );
        if builder.is_full() {
            return;
        }

        self.new_token.write_frames(builder, tokens, frame_stats);
        if builder.is_full() {
            return;
        }

        self.streams
            .write_frames(TransmissionPriority::Low, builder, tokens, frame_stats);

        #[cfg(test)]
        if let Some(w) = &mut self.test_frame_writer {
            w.write_frames(builder);
        }
    }

    // Maybe send a probe.  Return true if the packet was ack-eliciting.
    fn maybe_probe(
        &mut self,
        path: &PathRef,
        force_probe: bool,
        builder: &mut PacketBuilder,
        ack_end: usize,
        tokens: &mut Vec<RecoveryToken>,
        now: Instant,
    ) -> bool {
        let untracked = self.received_untracked && !self.state.connected();
        self.received_untracked = false;

        // Anything written after an ACK already elicits acknowledgment.
        // If we need to probe and nothing has been written, send a PING.
        if builder.len() > ack_end {
            return true;
        }

        let pto = path.borrow().rtt().pto(self.confirmed());
        let mut probe = if untracked && builder.packet_empty() || force_probe {
            // If we received an untracked packet and we aren't probing already
            // or the PTO timer fired: probe.
            true
        } else if !builder.packet_empty() {
            // The packet only contains an ACK.  Check whether we want to
            // force an ACK with a PING so we can stop tracking packets.
            self.loss_recovery.should_probe(pto, now)
        } else {
            false
        };

        if self.streams.need_keep_alive() {
            // We need to keep the connection alive, including sending a PING
            // again. If a PING is already scheduled (i.e. `probe` is `true`)
            // piggy back on it. If not, schedule one.
            probe |= self.idle_timeout.send_keep_alive(now, pto, tokens);
        }

        if probe {
            // Nothing ack-eliciting and we need to probe; send PING.
            debug_assert_ne!(builder.remaining(), 0);
            builder.encode_varint(FrameType::Ping);
            let stats = &mut self.stats.borrow_mut().frame_tx;
            stats.ping += 1;
        }
        probe
    }

    /// Write frames to the provided builder.  Returns a list of tokens used for
    /// tracking loss or acknowledgment, whether any frame was ACK eliciting, and
    /// whether the packet was padded.
    fn write_frames(
        &mut self,
        path: &PathRef,
        space: PacketNumberSpace,
        profile: &SendProfile,
        builder: &mut PacketBuilder,
        coalesced: bool, // Whether this packet is coalesced behind another one.
        now: Instant,
    ) -> (Vec<RecoveryToken>, bool, bool) {
        let mut tokens = Vec::new();
        let primary = path.borrow().is_primary();
        let mut ack_eliciting = false;

        if primary {
            let stats = &mut self.stats.borrow_mut().frame_tx;
            self.acks.write_frame(
                space,
                now,
                path.borrow().rtt().estimate(),
                builder,
                &mut tokens,
                stats,
            );
        }
        let ack_end = builder.len();

        // Avoid sending path validation probes until the handshake completes,
        // but send them even when we don't have space.
        let full_mtu = profile.limit() == path.borrow().plpmtu();
        if space == PacketNumberSpace::ApplicationData && self.state.connected() {
            // Path validation probes should only be padded if the full MTU is available.
            // The probing code needs to know so it can track that.
            if path.borrow_mut().write_frames(
                builder,
                &mut self.stats.borrow_mut().frame_tx,
                full_mtu,
                now,
            ) {
                builder.enable_padding(true);
            }
        }

        if profile.ack_only(space) {
            // If we are CC limited we can only send ACKs!
            return (tokens, false, false);
        }

        if primary {
            if space == PacketNumberSpace::ApplicationData {
                if self.state.connected()
                    && path.borrow().pmtud().needs_probe()
                    && !coalesced // Only send PMTUD probes using non-coalesced packets.
                    && full_mtu
                {
                    path.borrow_mut()
                        .pmtud_mut()
                        .send_probe(builder, &mut self.stats.borrow_mut());
                    ack_eliciting = true;
                }
                self.write_appdata_frames(builder, &mut tokens, now);
            } else {
                let stats = &mut self.stats.borrow_mut().frame_tx;
                self.crypto.write_frame(
                    space,
                    self.conn_params.sni_slicing_enabled(),
                    builder,
                    &mut tokens,
                    stats,
                );
            }
        }

        // Maybe send a probe now, either to probe for losses or to keep the connection live.
        let force_probe = profile.should_probe(space);
        ack_eliciting |= self.maybe_probe(path, force_probe, builder, ack_end, &mut tokens, now);
        // If this is not the primary path, this should be ack-eliciting.
        debug_assert!(primary || ack_eliciting);

        // Add padding.  Only pad 1-RTT packets so that we don't prevent coalescing.
        // And avoid padding packets that otherwise only contain ACK because adding PADDING
        // causes those packets to consume congestion window, which is not tracked (yet).
        // And avoid padding if we don't have a full MTU available.
        let stats = &mut self.stats.borrow_mut().frame_tx;
        let padded = if ack_eliciting && full_mtu && builder.pad() {
            stats.padding += 1;
            true
        } else {
            false
        };

        (tokens, ack_eliciting, padded)
    }

    fn write_closing_frames(
        &mut self,
        close: &ClosingFrame,
        builder: &mut PacketBuilder,
        space: PacketNumberSpace,
        now: Instant,
        path: &PathRef,
        tokens: &mut Vec<RecoveryToken>,
    ) {
        if builder.remaining() > ClosingFrame::MIN_LENGTH + RecvdPackets::USEFUL_ACK_LEN {
            // Include an ACK frame with the CONNECTION_CLOSE.
            let limit = builder.limit();
            builder.set_limit(limit - ClosingFrame::MIN_LENGTH);
            self.acks.immediate_ack(space, now);
            self.acks.write_frame(
                space,
                now,
                path.borrow().rtt().estimate(),
                builder,
                tokens,
                &mut self.stats.borrow_mut().frame_tx,
            );
            builder.set_limit(limit);
        }
        // CloseReason::Application is only allowed at 1RTT.
        let sanitized = if space == PacketNumberSpace::ApplicationData {
            None
        } else {
            close.sanitize()
        };
        sanitized.as_ref().unwrap_or(close).write_frame(builder);
        self.stats.borrow_mut().frame_tx.connection_close += 1;
    }

    /// Build a datagram, possibly from multiple packets (for different PN
    /// spaces) and each containing 1+ frames.
    #[expect(clippy::too_many_lines, reason = "Yeah, that's just the way it is.")]
    fn output_path(
        &mut self,
        path: &PathRef,
        now: Instant,
        closing_frame: Option<&ClosingFrame>,
    ) -> Res<SendOption> {
        let mut initial_sent = None;
        let mut packet_tos = None;
        let mut needs_padding = false;
        let grease_quic_bit = self.can_grease_quic_bit();
        let version = self.version();

        // Determine how we are sending packets (PTO, etc..).
        let profile = self.loss_recovery.send_profile(&path.borrow(), now);
        qdebug!("[{self}] output_path send_profile {profile:?}");

        // Frames for different epochs must go in different packets, but then these
        // packets can go in a single datagram
        let mut encoder = Encoder::with_capacity(profile.limit());
        for space in PacketNumberSpace::iter() {
            // Ensure we have tx crypto state for this epoch, or skip it.
            let Some((epoch, tx)) = self.crypto.states_mut().select_tx_mut(self.version, space)
            else {
                continue;
            };

            let header_start = encoder.len();
            let (pt, mut builder) = Self::build_packet_header(
                &path.borrow(),
                epoch,
                encoder,
                tx,
                &self.address_validation,
                version,
                grease_quic_bit,
            );
            let pn = Self::add_packet_number(
                &mut builder,
                tx,
                self.loss_recovery.largest_acknowledged_pn(space),
            );
            // The builder will set the limit to 0 if there isn't enough space for the header.
            if builder.is_full() {
                encoder = builder.abort();
                break;
            }

            // Configure the limits and padding for this packet.
            let aead_expansion = tx.expansion();
            needs_padding |= builder.set_initial_limit(
                &profile,
                aead_expansion,
                self.paths
                    .primary()
                    .ok_or(Error::InternalError)?
                    .borrow()
                    .pmtud(),
            );
            builder.enable_padding(needs_padding);
            if builder.is_full() {
                encoder = builder.abort();
                break;
            }

            // Add frames to the packet.
            let payload_start = builder.len();
            let (mut tokens, mut ack_eliciting, mut padded) = (Vec::new(), false, false);
            if let Some(close) = closing_frame {
                self.write_closing_frames(close, &mut builder, space, now, path, &mut tokens);
            } else {
                (tokens, ack_eliciting, padded) =
                    self.write_frames(path, space, &profile, &mut builder, header_start != 0, now);
            }
            if builder.packet_empty() {
                // Nothing to include in this packet.
                encoder = builder.abort();
                continue;
            }

            // If we don't have a TOS for this UDP datagram yet (i.e. `tos` is `None`), get it,
            // adding a `RecoveryToken::EcnEct0` to `tokens` in case of loss.
            let tos = packet_tos.get_or_insert_with(|| path.borrow().tos(&mut tokens));
            self.log_packet(
                packet::MetaData::new_out(
                    path,
                    pt,
                    pn,
                    builder.len() + aead_expansion,
                    &builder.as_ref()[payload_start..],
                    *tos,
                ),
                now,
            );

            self.stats.borrow_mut().packets_tx += 1;
            let tx = self
                .crypto
                .states_mut()
                .tx_mut(self.version, epoch)
                .ok_or(Error::InternalError)?;
            encoder = builder.build(tx)?;
            self.crypto.states_mut().auto_update()?;

            if ack_eliciting {
                self.idle_timeout.on_packet_sent(now);
            }
            let sent = SentPacket::new(
                pt,
                pn,
                now,
                ack_eliciting,
                tokens,
                encoder.len() - header_start,
            );
            if padded {
                needs_padding = false;
                self.loss_recovery.on_packet_sent(path, sent, now);
            } else if pt == PacketType::Initial && (self.role == Role::Client || ack_eliciting) {
                // Packets containing Initial packets might need padding, and we want to
                // track that padding along with the Initial packet.  So defer tracking.
                initial_sent = Some(sent);
                needs_padding = true;
            } else {
                if pt == PacketType::Handshake && self.role == Role::Client {
                    needs_padding = false;
                }
                self.loss_recovery.on_packet_sent(path, sent, now);
            }

            // Track which packet types are sent with which ECN codepoints. For
            // coalesced packets, this increases the counts for each packet type
            // contained in the coalesced packet. This is per Section 13.4.1 of
            // RFC 9000.
            self.stats.borrow_mut().ecn_tx[pt] += IpTosEcn::from(*tos);
            if space == PacketNumberSpace::Handshake {
                if self.role == Role::Client {
                    // We're sending a Handshake packet, so we can discard Initial keys.
                    self.discard_keys(PacketNumberSpace::Initial, now);
                } else if self.role == Role::Server && self.state == State::Confirmed {
                    // We could discard handshake keys in set_state,
                    // but wait until after sending an ACK.
                    self.discard_keys(PacketNumberSpace::Handshake, now);
                }
            }

            // If the client has more CRYPTO data queued up, do not coalesce if
            // this packet is an Initial. Without this, 0-RTT packets could be
            // coalesced with the first Initial, which some server (e.g., ours)
            // do not support, because they may not save packets they can't
            // decrypt yet.
            if self.role == Role::Client
                && space == PacketNumberSpace::Initial
                && !self.crypto.streams_mut().is_empty(space)
            {
                break;
            }
        }

        if encoder.is_empty() {
            qdebug!("TX blocked, profile={profile:?}");
            Ok(SendOption::No(profile.paced()))
        } else {
            // Perform additional padding for Initial packets as necessary.
            let mut packets: Vec<u8> = encoder.into();
            if let Some(mut initial) = initial_sent.take() {
                if needs_padding && packets.len() < profile.limit() {
                    qdebug!(
                        "[{self}] pad Initial from {} to PLPMTU {}",
                        packets.len(),
                        profile.limit()
                    );
                    initial.track_padding(profile.limit() - packets.len());
                    // These zeros aren't padding frames, they are an invalid all-zero coalesced
                    // packet, which is why we don't increase `frame_tx.padding` count here.
                    packets.resize(profile.limit(), 0);
                }
                self.loss_recovery.on_packet_sent(path, initial, now);
            }
            path.borrow_mut().add_sent(packets.len());
            Ok(SendOption::Yes(path.borrow_mut().datagram(
                packets,
                packet_tos.unwrap_or_default(),
                &mut self.stats.borrow_mut(),
            )))
        }
    }

    /// # Errors
    /// When connection state is not valid.
    pub fn initiate_key_update(&mut self) -> Res<()> {
        if self.state == State::Confirmed {
            let la = self
                .loss_recovery
                .largest_acknowledged_pn(PacketNumberSpace::ApplicationData);
            qinfo!("[{self}] Initiating key update");
            self.crypto.states_mut().initiate_key_update(la)
        } else {
            Err(Error::KeyUpdateBlocked)
        }
    }

    #[cfg(test)]
    #[must_use]
    pub fn get_epochs(&self) -> (Option<usize>, Option<usize>) {
        self.crypto.states().get_epochs()
    }

    fn client_start(&mut self, now: Instant) -> Res<()> {
        qdebug!("[{self}] client_start");
        debug_assert_eq!(self.role, Role::Client);
        if let Some(path) = self.paths.primary() {
            qlog::client_connection_started(&self.qlog, &path, now);
        }
        qlog::client_version_information_initiated(
            &self.qlog,
            self.conn_params.get_versions(),
            now,
        );

        self.handshake(now, self.version, PacketNumberSpace::Initial, None)?;
        self.set_state(State::WaitInitial, now);
        self.zero_rtt_state = if self.crypto.enable_0rtt(self.version, self.role)? {
            qdebug!("[{self}] Enabled 0-RTT");
            ZeroRttState::Sending
        } else {
            ZeroRttState::Init
        };
        Ok(())
    }

    fn get_closing_period_time(&self, now: Instant) -> Instant {
        // Spec says close time should be at least PTO times 3.
        now + (self.pto() * 3)
    }

    /// Close the connection.
    pub fn close(&mut self, now: Instant, app_error: AppError, msg: impl AsRef<str>) {
        let error = CloseReason::Application(app_error);
        let timeout = self.get_closing_period_time(now);
        if let Some(path) = self.paths.primary() {
            self.state_signaling
                .close(path, error.clone(), FrameType::Padding, msg);
            self.set_state(State::Closing { error, timeout }, now);
        } else {
            self.set_state(State::Closed(error), now);
        }
    }

    fn set_initial_limits(&mut self) {
        self.streams.set_initial_limits();
        let peer_timeout = self
            .tps
            .borrow()
            .remote()
            .get_integer(TransportParameterId::IdleTimeout);
        if peer_timeout > 0 {
            self.idle_timeout
                .set_peer_timeout(Duration::from_millis(peer_timeout));
        }

        self.quic_datagrams
            .set_remote_datagram_size(self.tps.borrow().remote().get_integer(MaxDatagramFrameSize));
    }

    #[must_use]
    pub fn is_stream_id_allowed(&self, stream_id: StreamId) -> bool {
        self.streams.is_stream_id_allowed(stream_id)
    }

    /// Process the final set of transport parameters.
    fn process_tps(&mut self, now: Instant) -> Res<()> {
        self.validate_cids()?;
        self.validate_versions()?;
        {
            let tps = self.tps.borrow();
            let remote = tps
                .remote_handshake()
                .ok_or(Error::TransportParameterError)?;

            // If the peer provided a preferred address, then we have to be a client
            // and they have to be using a non-empty connection ID.
            if remote.get_preferred_address().is_some()
                && (self.role == Role::Server
                    || self
                        .remote_initial_source_cid
                        .as_ref()
                        .ok_or(Error::UnknownConnectionId)?
                        .is_empty())
            {
                return Err(Error::TransportParameterError);
            }

            let reset_token = remote.get_bytes(StatelessResetToken).map_or_else(
                || Ok(ConnectionIdEntry::random_srt()),
                |token| <[u8; 16]>::try_from(token).map_err(|_| Error::TransportParameterError),
            )?;
            let path = self.paths.primary().ok_or(Error::NoAvailablePath)?;
            path.borrow_mut().set_reset_token(reset_token);

            let max_ad = Duration::from_millis(remote.get_integer(MaxAckDelay));
            let min_ad = if remote.has_value(MinAckDelay) {
                let min_ad = Duration::from_micros(remote.get_integer(MinAckDelay));
                if min_ad > max_ad {
                    return Err(Error::TransportParameterError);
                }
                Some(min_ad)
            } else {
                None
            };
            path.borrow_mut()
                .set_ack_delay(max_ad, min_ad, self.conn_params.get_ack_ratio());

            let max_active_cids = remote.get_integer(ActiveConnectionIdLimit);
            self.cid_manager.set_limit(max_active_cids);
        }
        self.set_initial_limits();
        qlog::connection_tparams_set(&self.qlog, &self.tps.borrow(), now);
        Ok(())
    }

    fn validate_cids(&self) -> Res<()> {
        let tph = self.tps.borrow();
        let remote_tps = tph
            .remote_handshake()
            .ok_or(Error::TransportParameterError)?;

        let tp = remote_tps.get_bytes(InitialSourceConnectionId);
        if self
            .remote_initial_source_cid
            .as_ref()
            .map(ConnectionId::as_cid_ref)
            != tp.map(ConnectionIdRef::from)
        {
            qwarn!(
                "[{self}] ISCID test failed: self cid {:?} != tp cid {:?}",
                self.remote_initial_source_cid,
                tp.map(hex),
            );
            return Err(Error::ProtocolViolation);
        }

        if self.role == Role::Client {
            let tp = remote_tps.get_bytes(OriginalDestinationConnectionId);
            if self
                .original_destination_cid
                .as_ref()
                .map(ConnectionId::as_cid_ref)
                != tp.map(ConnectionIdRef::from)
            {
                qwarn!(
                    "[{self}] ODCID test failed: self cid {:?} != tp cid {:?}",
                    self.original_destination_cid,
                    tp.map(hex),
                );
                return Err(Error::ProtocolViolation);
            }

            let tp = remote_tps.get_bytes(RetrySourceConnectionId);
            let expected = if let AddressValidationInfo::Retry {
                retry_source_cid, ..
            } = &self.address_validation
            {
                Some(retry_source_cid.as_cid_ref())
            } else {
                None
            };
            if expected != tp.map(ConnectionIdRef::from) {
                qwarn!(
                    "[{self}] RSCID test failed. self cid {expected:?} != tp cid {:?}",
                    tp.map(hex),
                );
                return Err(Error::ProtocolViolation);
            }
        }

        Ok(())
    }

    /// Validate the `version_negotiation` transport parameter from the peer.
    fn validate_versions(&self) -> Res<()> {
        let tph = self.tps.borrow();
        let remote_tps = tph
            .remote_handshake()
            .ok_or(Error::TransportParameterError)?;
        // `current` and `other` are the value from the peer's transport parameters.
        // We're checking that these match our expectations.
        if let Some((current, other)) = remote_tps.get_versions() {
            qtrace!(
                "[{self}] validate_versions: current={:x} chosen={current:x} other={other:x?}",
                self.version.wire_version(),
            );
            if self.role == Role::Server {
                // 1. A server acts on transport parameters, with validation
                // of `current` happening in the transport parameter handler.
                // All we need to do is confirm that the transport parameter
                // was provided.
                Ok(())
            } else if self.version().wire_version() != current {
                qinfo!("[{self}] validate_versions: current version mismatch");
                Err(Error::VersionNegotiation)
            } else if self
                .conn_params
                .get_versions()
                .initial()
                .is_compatible(self.version)
            {
                // 2. The current version is compatible with what we attempted.
                // That's a compatible upgrade and that's OK.
                Ok(())
            } else {
                // 3. The initial version we attempted isn't compatible.  Check that
                // the one we would have chosen is compatible with this one.
                let mut all_versions = other.to_owned();
                all_versions.push(current);
                if self
                    .conn_params
                    .get_versions()
                    .preferred(&all_versions)
                    .ok_or(Error::VersionNegotiation)?
                    .is_compatible(self.version)
                {
                    Ok(())
                } else {
                    qinfo!("[{self}] validate_versions: failed");
                    Err(Error::VersionNegotiation)
                }
            }
        } else if self.version != Version::Version1 && !self.version.is_draft() {
            qinfo!("[{self}] validate_versions: missing extension");
            Err(Error::VersionNegotiation)
        } else {
            Ok(())
        }
    }

    fn confirm_version(&mut self, v: Version) -> Res<()> {
        if self.version != v {
            qdebug!("[{self}] Compatible upgrade {:?} ==> {v:?}", self.version);
        }
        self.crypto.confirm_version(v)?;
        self.version = v;
        Ok(())
    }

    fn compatible_upgrade(&mut self, packet_version: Version) -> Res<()> {
        if !matches!(self.state, State::WaitInitial | State::WaitVersion) {
            return Ok(());
        }

        if self.role == Role::Client {
            self.confirm_version(packet_version)?;
        } else if self.tps.borrow().remote_handshake().is_some() {
            let version = self.tps.borrow().version();
            let dcid = self
                .original_destination_cid
                .as_ref()
                .ok_or(Error::ProtocolViolation)?;
            self.crypto.states_mut().init_server(version, dcid)?;
            self.confirm_version(version)?;
        }
        Ok(())
    }

    fn handshake(
        &mut self,
        now: Instant,
        packet_version: Version,
        space: PacketNumberSpace,
        data: Option<&[u8]>,
    ) -> Res<()> {
        qtrace!("[{self}] Handshake space={space} data={data:0x?}");

        let was_authentication_pending =
            *self.crypto.tls().state() == HandshakeState::AuthenticationPending;
        let try_update = data.is_some();
        match self.crypto.handshake(now, space, data)? {
            HandshakeState::Authenticated(_) | HandshakeState::InProgress => (),
            HandshakeState::AuthenticationPending => {
                if !was_authentication_pending {
                    self.events.authentication_needed();
                }
            }
            HandshakeState::EchFallbackAuthenticationPending(public_name) => self
                .events
                .ech_fallback_authentication_needed(public_name.clone()),
            HandshakeState::Complete(_) => {
                if !self.state.connected() {
                    self.set_connected(now)?;
                }
            }
            _ => {
                qerror!("Crypto state should not be new or failed after successful handshake");
                return Err(Error::CryptoError(neqo_crypto::Error::InternalError));
            }
        }

        // There is a chance that this could be called less often, but getting the
        // conditions right is a little tricky, so call whenever CRYPTO data is used.
        if try_update {
            self.compatible_upgrade(packet_version)?;
            // We have transport parameters, it's go time.
            if self.tps.borrow().remote_handshake().is_some() {
                self.set_initial_limits();
            }
            if self.crypto.install_keys(self.role)? {
                self.saved_datagrams.make_available(Epoch::Handshake);
            }
        }

        Ok(())
    }

    fn set_confirmed(&mut self, now: Instant) -> Res<()> {
        self.set_state(State::Confirmed, now);
        if self.conn_params.pmtud_enabled() {
            self.paths
                .primary()
                .ok_or(Error::InternalError)?
                .borrow_mut()
                .pmtud_mut()
                .start(now, &mut self.stats.borrow_mut());
        }
        Ok(())
    }

    #[expect(clippy::too_many_lines, reason = "Yep, but it's a nice big match.")]
    fn input_frame(
        &mut self,
        path: &PathRef,
        packet_version: Version,
        packet_type: PacketType,
        frame: Frame,
        next_pn: PacketNumber,
        now: Instant,
    ) -> Res<()> {
        if !frame.is_allowed(packet_type) {
            qinfo!("frame not allowed: {frame:?} {packet_type:?}");
            return Err(Error::ProtocolViolation);
        }
        let space = PacketNumberSpace::from(packet_type);
        if frame.is_stream() {
            return self
                .streams
                .input_frame(&frame, &mut self.stats.borrow_mut().frame_rx);
        }
        match frame {
            Frame::Padding(length) => {
                self.stats.borrow_mut().frame_rx.padding += usize::from(length);
            }
            Frame::Ping => {
                // If we get a PING and there are outstanding CRYPTO frames,
                // prepare to resend them.
                self.stats.borrow_mut().frame_rx.ping += 1;
                self.crypto.resend_unacked(space);
                // Send an ACK immediately if we might not otherwise do so.
                self.acks.immediate_ack(space, now);
            }
            Frame::Ack {
                largest_acknowledged,
                ack_delay,
                first_ack_range,
                ack_ranges,
                ecn_count,
            } => {
                // Ensure that the largest acknowledged packet number was actually sent.
                // (If we ever start using non-contiguous packet numbers, we need to check all the
                // packet numbers in the ACKed ranges.)
                if largest_acknowledged >= next_pn {
                    qwarn!("Largest ACKed {largest_acknowledged} was never sent");
                    return Err(Error::AckedUnsentPacket);
                }

                let ranges =
                    Frame::decode_ack_frame(largest_acknowledged, first_ack_range, &ack_ranges)?;
                self.handle_ack(space, ranges, ecn_count, ack_delay, now)?;
            }
            Frame::Crypto { offset, data } => {
                qtrace!(
                    "[{self}] Crypto frame on space={space} offset={offset}, data={:0x?}",
                    &data
                );
                self.stats.borrow_mut().frame_rx.crypto += 1;
                self.crypto
                    .streams_mut()
                    .inbound_frame(space, offset, data)?;
                if self.crypto.streams().data_ready(space) {
                    let mut buf = Vec::new();
                    let read = self.crypto.streams_mut().read_to_end(space, &mut buf);
                    qdebug!("Read {read:?} bytes");
                    self.handshake(now, packet_version, space, Some(&buf))?;
                    self.create_resumption_token(now);
                } else {
                    // If we get a useless CRYPTO frame send outstanding CRYPTO frames and 0-RTT
                    // data again.
                    self.crypto.resend_unacked(space);
                    if space == PacketNumberSpace::Initial {
                        self.crypto.resend_unacked(PacketNumberSpace::Handshake);
                        self.resend_0rtt(now);
                    }
                }
            }
            Frame::NewToken { token } => {
                self.stats.borrow_mut().frame_rx.new_token += 1;
                self.new_token.save_token(token.to_vec());
                self.create_resumption_token(now);
            }
            Frame::NewConnectionId {
                sequence_number,
                connection_id,
                stateless_reset_token,
                retire_prior,
            } => {
                self.stats.borrow_mut().frame_rx.new_connection_id += 1;
                self.cids.add_remote(ConnectionIdEntry::new(
                    sequence_number,
                    ConnectionId::from(connection_id),
                    stateless_reset_token.to_owned(),
                ))?;
                self.paths.retire_cids(retire_prior, &mut self.cids);
                if self.cids.len() >= LOCAL_ACTIVE_CID_LIMIT {
                    qinfo!("[{self}] received too many connection IDs");
                    return Err(Error::ConnectionIdLimitExceeded);
                }
            }
            Frame::RetireConnectionId { sequence_number } => {
                self.stats.borrow_mut().frame_rx.retire_connection_id += 1;
                self.cid_manager.retire(sequence_number);
            }
            Frame::PathChallenge { data } => {
                self.stats.borrow_mut().frame_rx.path_challenge += 1;
                // If we were challenged, try to make the path permanent.
                // Report an error if we don't have enough connection IDs.
                self.ensure_permanent(path, now)?;
                path.borrow_mut().challenged(data);
            }
            Frame::PathResponse { data } => {
                self.stats.borrow_mut().frame_rx.path_response += 1;
                if self
                    .paths
                    .path_response(data, now, &mut self.stats.borrow_mut())
                {
                    // This PATH_RESPONSE enabled migration; tell loss recovery.
                    self.loss_recovery.migrate();
                }
            }
            Frame::ConnectionClose {
                error_code,
                frame_type,
                reason_phrase,
            } => {
                self.stats.borrow_mut().frame_rx.connection_close += 1;
                qinfo!(
                    "[{self}] ConnectionClose received. Error code: {error_code:?} frame type {frame_type:x} reason {reason_phrase}"
                );
                let (detail, frame_type) = if let CloseError::Application(_) = error_code {
                    // Use a transport error here because we want to send
                    // NO_ERROR in this case.
                    (
                        Error::PeerApplicationError(error_code.code()),
                        FrameType::ConnectionCloseApplication,
                    )
                } else {
                    (
                        Error::PeerError(error_code.code()),
                        FrameType::ConnectionCloseTransport,
                    )
                };
                let error = CloseReason::Transport(detail);
                self.state_signaling
                    .drain(Rc::clone(path), error.clone(), frame_type, "");
                self.set_state(
                    State::Draining {
                        error,
                        timeout: self.get_closing_period_time(now),
                    },
                    now,
                );
            }
            Frame::HandshakeDone => {
                self.stats.borrow_mut().frame_rx.handshake_done += 1;
                if self.role == Role::Server || !self.state.connected() {
                    return Err(Error::ProtocolViolation);
                }
                self.set_confirmed(now)?;
                self.discard_keys(PacketNumberSpace::Handshake, now);
                self.migrate_to_preferred_address(now)?;
            }
            Frame::AckFrequency {
                seqno,
                tolerance,
                delay,
                ignore_order,
            } => {
                self.stats.borrow_mut().frame_rx.ack_frequency += 1;
                let delay = Duration::from_micros(delay);
                if delay < GRANULARITY {
                    return Err(Error::ProtocolViolation);
                }
                self.acks
                    .ack_freq(seqno, tolerance - 1, delay, ignore_order);
            }
            Frame::Datagram { data, .. } => {
                self.stats.borrow_mut().frame_rx.datagram += 1;
                self.quic_datagrams
                    .handle_datagram(data, &mut self.stats.borrow_mut())?;
            }
            _ => unreachable!("All other frames are for streams"),
        }

        Ok(())
    }

    /// Given a set of `SentPacket` instances, ensure that the source of the packet
    /// is told that they are lost.  This gives the frame generation code a chance
    /// to retransmit the frame as needed.
    fn handle_lost_packets(&mut self, lost_packets: &[SentPacket]) {
        for lost in lost_packets {
            for token in lost.tokens() {
                qdebug!("[{self}] Lost: {token:?}");
                match token {
                    RecoveryToken::Ack(ack_token) => {
                        // If we lost an ACK frame during the handshake, send another one.
                        if ack_token.space() != PacketNumberSpace::ApplicationData {
                            self.acks.immediate_ack(ack_token.space(), lost.time_sent());
                        }
                    }
                    RecoveryToken::Crypto(ct) => self.crypto.lost(ct),
                    RecoveryToken::HandshakeDone => self.state_signaling.handshake_done(),
                    RecoveryToken::NewToken(seqno) => self.new_token.lost(*seqno),
                    RecoveryToken::NewConnectionId(ncid) => self.cid_manager.lost(ncid),
                    RecoveryToken::RetireConnectionId(seqno) => self.paths.lost_retire_cid(*seqno),
                    RecoveryToken::AckFrequency(rate) => self.paths.lost_ack_frequency(rate),
                    RecoveryToken::KeepAlive => self.idle_timeout.lost_keep_alive(),
                    RecoveryToken::Stream(stream_token) => self.streams.lost(stream_token),
                    RecoveryToken::Datagram(dgram_tracker) => {
                        self.events
                            .datagram_outcome(dgram_tracker, OutgoingDatagramOutcome::Lost);
                        self.stats.borrow_mut().datagram_tx.lost += 1;
                    }
                    RecoveryToken::EcnEct0 => self
                        .paths
                        .lost_ecn(lost.packet_type(), &mut self.stats.borrow_mut()),
                }
            }
        }
    }

    fn decode_ack_delay(&self, v: u64) -> Res<Duration> {
        // If we have remote transport parameters, use them.
        // Otherwise, ack delay should be zero (because it's the handshake).
        self.tps.borrow().remote_handshake().map_or_else(
            || Ok(Duration::default()),
            |r| {
                let exponent = u32::try_from(r.get_integer(AckDelayExponent))?;
                // ACK_DELAY_EXPONENT > 20 is invalid per RFC9000. We already checked that in
                // TransportParameter::decode.
                let corrected = if v.leading_zeros() >= exponent {
                    v << exponent
                } else {
                    u64::MAX
                };
                Ok(Duration::from_micros(corrected))
            },
        )
    }

    fn handle_ack<R>(
        &mut self,
        space: PacketNumberSpace,
        ack_ranges: R,
        ack_ecn: Option<ecn::Count>,
        ack_delay: u64,
        now: Instant,
    ) -> Res<()>
    where
        R: IntoIterator<Item = RangeInclusive<PacketNumber>> + Debug,
        R::IntoIter: ExactSizeIterator,
    {
        qdebug!("[{self}] Rx ACK space={space}, ranges={ack_ranges:?}");

        let Some(path) = self.paths.primary() else {
            return Ok(());
        };
        let (acked_packets, lost_packets) = self.loss_recovery.on_ack_received(
            &path,
            space,
            ack_ranges,
            ack_ecn,
            self.decode_ack_delay(ack_delay)?,
            now,
        );
        let largest_acknowledged = acked_packets.first().map(SentPacket::pn);
        for acked in acked_packets {
            for token in acked.tokens() {
                match token {
                    RecoveryToken::Stream(stream_token) => self.streams.acked(stream_token),
                    RecoveryToken::Ack(at) => self.acks.acked(at),
                    RecoveryToken::Crypto(ct) => self.crypto.acked(ct),
                    RecoveryToken::NewToken(seqno) => self.new_token.acked(*seqno),
                    RecoveryToken::NewConnectionId(entry) => self.cid_manager.acked(entry),
                    RecoveryToken::RetireConnectionId(seqno) => self.paths.acked_retire_cid(*seqno),
                    RecoveryToken::AckFrequency(rate) => self.paths.acked_ack_frequency(rate),
                    RecoveryToken::KeepAlive => self.idle_timeout.ack_keep_alive(),
                    RecoveryToken::Datagram(dgram_tracker) => self
                        .events
                        .datagram_outcome(dgram_tracker, OutgoingDatagramOutcome::Acked),
                    RecoveryToken::EcnEct0 => self.paths.acked_ecn(),
                    // We only worry when these are lost
                    RecoveryToken::HandshakeDone => (),
                }
            }
        }
        self.handle_lost_packets(&lost_packets);
        qlog::packets_lost(&self.qlog, &lost_packets, now);
        let stats = &mut self.stats.borrow_mut().frame_rx;
        stats.ack += 1;
        if let Some(largest_acknowledged) = largest_acknowledged {
            stats.largest_acknowledged = max(stats.largest_acknowledged, largest_acknowledged);
        }
        Ok(())
    }

    /// Tell 0-RTT packets that they were "lost".
    fn resend_0rtt(&mut self, now: Instant) {
        if let Some(path) = self.paths.primary() {
            let dropped = self.loss_recovery.drop_0rtt(&path, now);
            self.handle_lost_packets(&dropped);
        }
    }

    /// When the server rejects 0-RTT we need to drop a bunch of stuff.
    fn client_0rtt_rejected(&mut self, now: Instant) {
        if !matches!(self.zero_rtt_state, ZeroRttState::Sending) {
            return;
        }
        qdebug!("[{self}] 0-RTT rejected");
        self.resend_0rtt(now);
        self.streams.zero_rtt_rejected();
        self.crypto.states_mut().discard_0rtt_keys();
        self.events.client_0rtt_rejected();
    }

    fn set_connected(&mut self, now: Instant) -> Res<()> {
        qdebug!("[{self}] TLS connection complete");
        if self
            .crypto
            .tls()
            .info()
            .map(SecretAgentInfo::alpn)
            .is_none()
        {
            qwarn!("[{self}] No ALPN, closing connection");
            // 120 = no_application_protocol
            return Err(Error::CryptoAlert(120));
        }
        if self.role == Role::Server {
            // Remove the randomized client CID from the list of acceptable CIDs.
            self.cid_manager.remove_odcid();
            // Mark the path as validated, if it isn't already.
            let path = self.paths.primary().ok_or(Error::NoAvailablePath)?;
            path.borrow_mut().set_valid(now);
            // Generate a qlog event that the server connection started.
            qlog::server_connection_started(&self.qlog, &path, now);
        } else {
            self.zero_rtt_state = if self
                .crypto
                .tls()
                .info()
                .ok_or(Error::InternalError)?
                .early_data_accepted()
            {
                ZeroRttState::AcceptedClient
            } else {
                self.client_0rtt_rejected(now);
                ZeroRttState::Rejected
            };
        }

        // Setting application keys has to occur after 0-RTT rejection.
        let pto = self.pto();
        self.crypto
            .install_application_keys(self.version, now + pto)?;
        self.process_tps(now)?;
        self.set_state(State::Connected, now);
        self.create_resumption_token(now);
        self.saved_datagrams.make_available(Epoch::ApplicationData);
        self.stats.borrow_mut().resumed = self
            .crypto
            .tls()
            .info()
            .ok_or(Error::InternalError)?
            .resumed();
        if self.role == Role::Server {
            self.state_signaling.handshake_done();
            self.set_confirmed(now)?;
        }
        qinfo!("[{self}] Connection established");
        Ok(())
    }

    fn set_state(&mut self, state: State, now: Instant) {
        if state > self.state {
            qdebug!("[{self}] State change from {:?} -> {state:?}", self.state);
            self.state = state.clone();
            if self.state.closed() {
                self.streams.clear_streams();
            }
            self.events.connection_state_change(state);
            qlog::connection_state_updated(&self.qlog, &self.state, now);
        } else if mem::discriminant(&state) != mem::discriminant(&self.state) {
            // Only tolerate a regression in state if the new state is closing
            // and the connection is already closed.
            debug_assert!(matches!(
                state,
                State::Closing { .. } | State::Draining { .. }
            ));
            debug_assert!(self.state.closed());
        }
    }

    /// Create a stream.
    /// Returns new stream id
    ///
    /// # Errors
    ///
    /// `ConnectionState` if the connection stat does not allow to create streams.
    /// `StreamLimitError` if we are limited by server's stream concurrence.
    pub fn stream_create(&mut self, st: StreamType) -> Res<StreamId> {
        // Can't make streams while closing, otherwise rely on the stream limits.
        match self.state {
            State::Closing { .. } | State::Draining { .. } | State::Closed { .. } => {
                return Err(Error::ConnectionState);
            }
            State::WaitInitial | State::Handshaking => {
                if self.role == Role::Client && self.zero_rtt_state != ZeroRttState::Sending {
                    return Err(Error::ConnectionState);
                }
            }
            // In all other states, trust that the stream limits are correct.
            _ => (),
        }

        self.streams.stream_create(st)
    }

    /// Set the priority of a stream.
    ///
    /// # Errors
    ///
    /// `InvalidStreamId` the stream does not exist.
    pub fn stream_priority(
        &mut self,
        stream_id: StreamId,
        transmission: TransmissionPriority,
        retransmission: RetransmissionPriority,
    ) -> Res<()> {
        self.streams
            .get_send_stream_mut(stream_id)?
            .set_priority(transmission, retransmission);
        Ok(())
    }

    /// Set the `SendOrder` of a stream.  Re-enqueues to keep the ordering correct
    ///
    /// # Errors
    /// When the stream does not exist.
    pub fn stream_sendorder(
        &mut self,
        stream_id: StreamId,
        sendorder: Option<SendOrder>,
    ) -> Res<()> {
        self.streams.set_sendorder(stream_id, sendorder)
    }

    /// Set the Fairness of a stream
    ///
    /// # Errors
    /// When the stream does not exist.
    pub fn stream_fairness(&mut self, stream_id: StreamId, fairness: bool) -> Res<()> {
        self.streams.set_fairness(stream_id, fairness)
    }

    /// # Errors
    /// When the stream does not exist.
    pub fn send_stream_stats(&self, stream_id: StreamId) -> Res<SendStreamStats> {
        self.streams
            .get_send_stream(stream_id)
            .map(SendStream::stats)
    }

    /// # Errors
    /// When the stream does not exist.
    pub fn recv_stream_stats(&mut self, stream_id: StreamId) -> Res<RecvStreamStats> {
        let stream = self.streams.get_recv_stream_mut(stream_id)?;

        Ok(stream.stats())
    }

    /// Send data on a stream.
    /// Returns how many bytes were successfully sent. Could be less
    /// than total, based on receiver credit space available, etc.
    ///
    /// # Errors
    ///
    /// `InvalidStreamId` the stream does not exist,
    /// `InvalidInput` if length of `data` is zero,
    /// `FinalSizeError` if the stream has already been closed.
    pub fn stream_send(&mut self, stream_id: StreamId, data: &[u8]) -> Res<usize> {
        self.streams.get_send_stream_mut(stream_id)?.send(data)
    }

    /// Send all data or nothing on a stream. May cause `DATA_BLOCKED` or
    /// `STREAM_DATA_BLOCKED` frames to be sent.
    /// Returns true if data was successfully sent, otherwise false.
    ///
    /// # Errors
    ///
    /// `InvalidStreamId` the stream does not exist,
    /// `InvalidInput` if length of `data` is zero,
    /// `FinalSizeError` if the stream has already been closed.
    pub fn stream_send_atomic(&mut self, stream_id: StreamId, data: &[u8]) -> Res<bool> {
        let val = self
            .streams
            .get_send_stream_mut(stream_id)?
            .send_atomic(data);
        if let Ok(val) = val {
            debug_assert!(
                val == 0 || val == data.len(),
                "Unexpected value {val} when trying to send {} bytes atomically",
                data.len()
            );
        }
        val.map(|v| v == data.len())
    }

    /// Bytes that `stream_send()` is guaranteed to accept for sending.
    /// i.e. that will not be blocked by flow credits or send buffer max
    /// capacity.
    /// # Errors
    /// When the stream ID is invalid.
    pub fn stream_avail_send_space(&self, stream_id: StreamId) -> Res<usize> {
        Ok(self.streams.get_send_stream(stream_id)?.avail())
    }

    /// Set low watermark for [`ConnectionEvent::SendStreamWritable`] event.
    ///
    /// Stream emits a [`crate::ConnectionEvent::SendStreamWritable`] event
    /// when:
    /// - the available sendable bytes increased to or above the watermark
    /// - and was previously below the watermark.
    ///
    /// Default value is `1`. In other words
    /// [`crate::ConnectionEvent::SendStreamWritable`] is emitted whenever the
    /// available sendable bytes was previously at `0` and now increased to `1`
    /// or more.
    ///
    /// Use this when your protocol needs at least `watermark` amount of available
    /// sendable bytes to make progress.
    ///
    /// # Errors
    /// When the stream ID is invalid.
    pub fn stream_set_writable_event_low_watermark(
        &mut self,
        stream_id: StreamId,
        watermark: NonZeroUsize,
    ) -> Res<()> {
        self.streams
            .get_send_stream_mut(stream_id)?
            .set_writable_event_low_watermark(watermark);
        Ok(())
    }

    /// Close the stream. Enqueued data will be sent.
    /// # Errors
    /// When the stream ID is invalid.
    pub fn stream_close_send(&mut self, stream_id: StreamId) -> Res<()> {
        self.streams.get_send_stream_mut(stream_id)?.close();
        Ok(())
    }

    /// Abandon transmission of in-flight and future stream data.
    /// # Errors
    /// When the stream ID is invalid.
    pub fn stream_reset_send(&mut self, stream_id: StreamId, err: AppError) -> Res<()> {
        self.streams.get_send_stream_mut(stream_id)?.reset(err);
        Ok(())
    }

    /// Read buffered data from stream. bool says whether read bytes includes
    /// the final data on stream.
    ///
    /// # Errors
    ///
    /// `InvalidStreamId` if the stream does not exist.
    /// `NoMoreData` if data and fin bit were previously read by the application.
    pub fn stream_recv(&mut self, stream_id: StreamId, data: &mut [u8]) -> Res<(usize, bool)> {
        let stream = self.streams.get_recv_stream_mut(stream_id)?;

        let rb = stream.read(data)?;
        Ok(rb)
    }

    /// Application is no longer interested in this stream.
    /// # Errors
    /// When the stream ID is invalid.
    pub fn stream_stop_sending(&mut self, stream_id: StreamId, err: AppError) -> Res<()> {
        let stream = self.streams.get_recv_stream_mut(stream_id)?;

        stream.stop_sending(err);
        Ok(())
    }

    /// Increases `max_stream_data` for a `stream_id`.
    ///
    /// # Errors
    ///
    /// Returns `InvalidStreamId` if a stream does not exist or the receiving
    /// side is closed.
    pub fn set_stream_max_data(&mut self, stream_id: StreamId, max_data: u64) -> Res<()> {
        let stream = self.streams.get_recv_stream_mut(stream_id)?;

        stream.set_stream_max_data(max_data);
        Ok(())
    }

    /// Mark a receive stream as being important enough to keep the connection alive
    /// (if `keep` is `true`) or no longer important (if `keep` is `false`).  If any
    /// stream is marked this way, PING frames will be used to keep the connection
    /// alive, even when there is no activity.
    ///
    /// # Errors
    ///
    /// Returns `InvalidStreamId` if a stream does not exist or the receiving
    /// side is closed.
    pub fn stream_keep_alive(&mut self, stream_id: StreamId, keep: bool) -> Res<()> {
        self.streams.keep_alive(stream_id, keep)
    }

    #[must_use]
    pub const fn remote_datagram_size(&self) -> u64 {
        self.quic_datagrams.remote_datagram_size()
    }

    /// Returns the current max size of a datagram that can fit into a packet.
    /// The value will change over time depending on the encoded size of the
    /// packet number, ack frames, etc.
    ///
    /// # Errors
    /// The function returns `NotAvailable` if datagrams are not enabled.
    /// # Panics
    /// Basically never, because that unwrap won't fail.
    pub fn max_datagram_size(&self) -> Res<u64> {
        let max_dgram_size = self.quic_datagrams.remote_datagram_size();
        if max_dgram_size == 0 {
            return Err(Error::NotAvailable);
        }
        let version = self.version();
        let Some((epoch, tx)) = self
            .crypto
            .states()
            .select_tx(self.version, PacketNumberSpace::ApplicationData)
        else {
            return Err(Error::NotAvailable);
        };
        let path = self.paths.primary().ok_or(Error::NotAvailable)?;
        let mtu = path.borrow().plpmtu();
        let encoder = Encoder::default();

        let (_, mut builder) = Self::build_packet_header(
            &path.borrow(),
            epoch,
            encoder,
            tx,
            &self.address_validation,
            version,
            false,
        );
        _ = Self::add_packet_number(
            &mut builder,
            tx,
            self.loss_recovery
                .largest_acknowledged_pn(PacketNumberSpace::ApplicationData),
        );

        let data_len_possible =
            u64::try_from(mtu.saturating_sub(tx.expansion() + builder.len() + 1))?;
        Ok(min(data_len_possible, max_dgram_size))
    }

    /// Queue a datagram for sending.
    ///
    /// # Errors
    ///
    /// The function returns `TooMuchData` if the supply buffer is bigger than
    /// the allowed remote datagram size. The function does not check if the
    /// datagram can fit into a packet (i.e. MTU limit). This is checked during
    /// creation of an actual packet and the datagram will be dropped if it does
    /// not fit into the packet. The app is encourage to use `max_datagram_size`
    /// to check the estimated max datagram size and to use smaller datagrams.
    /// `max_datagram_size` is just a current estimate and will change over
    /// time depending on the encoded size of the packet number, ack frames, etc.
    pub fn send_datagram(&mut self, buf: Vec<u8>, id: impl Into<DatagramTracking>) -> Res<()> {
        self.quic_datagrams
            .add_datagram(buf, id.into(), &mut self.stats.borrow_mut())
    }

    /// Return the PLMTU of the primary path.
    ///
    /// # Panics
    ///
    /// The function panics if there is no primary path. (Should be fine for test usage.)
    #[cfg(test)]
    #[must_use]
    pub fn plpmtu(&self) -> usize {
        self.paths.primary().unwrap().borrow().plpmtu()
    }

    fn log_packet(&self, meta: packet::MetaData, now: Instant) {
        if log::log_enabled!(log::Level::Debug) {
            let mut s = String::new();
            let mut d = Decoder::from(meta.payload());
            while d.remaining() > 0 {
                let Ok(f) = Frame::decode(&mut d) else {
                    s.push_str(" [broken]...");
                    break;
                };
                let x = f.dump();
                if !x.is_empty() {
                    _ = write!(&mut s, "\n  {} {}", meta.direction(), &x);
                }
            }
            qdebug!("[{self}] {meta}{s}");
        }

        qlog::packet_io(&self.qlog, meta, now);
    }
}

impl EventProvider for Connection {
    type Event = ConnectionEvent;

    /// Return true if there are outstanding events.
    fn has_events(&self) -> bool {
        self.events.has_events()
    }

    /// Get events that indicate state changes on the connection. This method
    /// correctly handles cases where handling one event can obsolete
    /// previously-queued events, or cause new events to be generated.
    fn next_event(&mut self) -> Option<Self::Event> {
        self.events.next_event()
    }
}

impl Display for Connection {
    fn fmt(&self, f: &mut Formatter) -> fmt::Result {
        write!(f, "{:?} ", self.role)?;
        if let Some(cid) = self.odcid() {
            Display::fmt(&cid, f)
        } else {
            write!(f, "...")
        }
    }
}

#[cfg(test)]
mod tests;
