// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::{
    cell::RefCell,
    cmp::min,
    mem,
    net::SocketAddr,
    rc::Rc,
    time::{Duration, Instant},
};

use enum_map::EnumMap;
use neqo_common::{event::Provider as _, qdebug, qtrace, Datagram, Decoder, Role};
use neqo_crypto::{random, AllowZeroRtt, AuthenticationStatus, ResumptionToken};
use test_fixture::{fixture_init, new_neqo_qlog, now, DEFAULT_ADDR};

use super::{test_internal, CloseReason, Connection, ConnectionId, Output, State};
use crate::{
    addr_valid::{AddressValidation, ValidateAddress},
    cc::CWND_INITIAL_PKTS,
    cid::ConnectionIdRef,
    events::ConnectionEvent,
    frame::FrameType,
    packet::PacketBuilder,
    pmtud::Pmtud,
    recovery::ACK_ONLY_SIZE_LIMIT,
    stats::{FrameStats, Stats, MAX_PTO_COUNTS},
    tparams::TransportParameterId::*,
    ConnectionIdDecoder, ConnectionIdGenerator, ConnectionParameters, EmptyConnectionIdGenerator,
    Error, StreamId, StreamType, Version, MIN_INITIAL_PACKET_SIZE,
};

// All the tests.
mod ackrate;
mod cc;
mod close;
mod datagram;
mod ecn;
mod handshake;
mod idle;
mod keys;
mod migration;
mod null;
mod priority;
mod recovery;
mod resumption;
mod stream;
mod vn;
mod zerortt;

const DEFAULT_RTT: Duration = Duration::from_millis(100);
const AT_LEAST_PTO: Duration = Duration::from_secs(1);
const DEFAULT_STREAM_DATA: &[u8] = b"message";
/// The number of 1-RTT packets sent in `force_idle` by a client.
const CLIENT_HANDSHAKE_1RTT_PACKETS: usize = 1;

/// WARNING!  In this module, this version of the generator needs to be used.
/// This copies the implementation from
/// `test_fixture::CountingConnectionIdGenerator`, but it uses the different
/// types that are exposed to this module.  See also `default_client`.
///
/// This version doesn't randomize the length; as the congestion control tests
/// count the amount of data sent precisely.
#[derive(Debug, Default)]
pub struct CountingConnectionIdGenerator {
    counter: u32,
}

impl ConnectionIdDecoder for CountingConnectionIdGenerator {
    fn decode_cid<'a>(&self, dec: &mut Decoder<'a>) -> Option<ConnectionIdRef<'a>> {
        let len = usize::from(dec.peek_byte()?);
        dec.decode(len).map(ConnectionIdRef::from)
    }
}

impl ConnectionIdGenerator for CountingConnectionIdGenerator {
    fn generate_cid(&mut self) -> Option<ConnectionId> {
        let mut r = random::<20>();
        r[0] = 8;
        r[1] = u8::try_from(self.counter >> 24).ok()?;
        r[2] = u8::try_from((self.counter >> 16) & 0xff).ok()?;
        r[3] = u8::try_from((self.counter >> 8) & 0xff).ok()?;
        r[4] = u8::try_from(self.counter & 0xff).ok()?;
        self.counter += 1;
        Some(ConnectionId::from(&r[..8]))
    }

    fn as_decoder(&self) -> &dyn ConnectionIdDecoder {
        self
    }
}

// This is fabulous: because test_fixture uses the public API for Connection,
// it gets a different type to the ones that are referenced via super::super::*.
// Thus, this code can't use default_client() and default_server() from
// test_fixture because they produce different - and incompatible - types.
//
// These are a direct copy of those functions.
pub fn new_client(params: ConnectionParameters) -> Connection {
    fixture_init();
    let (log, _contents) = new_neqo_qlog();
    let mut client = Connection::new_client(
        test_fixture::DEFAULT_SERVER_NAME,
        test_fixture::DEFAULT_ALPN,
        Rc::new(RefCell::new(CountingConnectionIdGenerator::default())),
        DEFAULT_ADDR,
        DEFAULT_ADDR,
        params,
        now(),
    )
    .expect("create a default client");
    client.set_qlog(log);
    client
}

pub fn default_client() -> Connection {
    new_client(ConnectionParameters::default())
}

fn zero_len_cid_client(local_addr: SocketAddr, remote_addr: SocketAddr) -> Connection {
    Connection::new_client(
        test_fixture::DEFAULT_SERVER_NAME,
        test_fixture::DEFAULT_ALPN,
        Rc::new(RefCell::new(EmptyConnectionIdGenerator::default())),
        local_addr,
        remote_addr,
        ConnectionParameters::default(),
        now(),
    )
    .unwrap()
}

pub fn new_server(params: ConnectionParameters) -> Connection {
    fixture_init();
    let (log, _contents) = new_neqo_qlog();
    let mut c = Connection::new_server(
        test_fixture::DEFAULT_KEYS,
        test_fixture::DEFAULT_ALPN,
        Rc::new(RefCell::new(CountingConnectionIdGenerator::default())),
        params,
    )
    .expect("create a default server");
    c.set_qlog(log);
    c.server_enable_0rtt(&test_fixture::anti_replay(), AllowZeroRtt {})
        .expect("enable 0-RTT");
    c
}
pub fn default_server() -> Connection {
    new_server(ConnectionParameters::default())
}
pub fn resumed_server(client: &Connection) -> Connection {
    new_server(ConnectionParameters::default().versions(client.version(), Version::all()))
}

/// If state is `AuthenticationNeeded` call `authenticated()`. This function will
/// consume all outstanding events on the connection.
pub fn maybe_authenticate(conn: &mut Connection) -> bool {
    let authentication_needed = |e| matches!(e, ConnectionEvent::AuthenticationNeeded);
    if conn.events().any(authentication_needed) {
        conn.authenticated(AuthenticationStatus::Ok, now());
        return true;
    }
    false
}

/// Compute the RTT variance after `n` ACKs or other RTT updates.
pub fn rttvar_after_n_updates(n: usize, rtt: Duration) -> Duration {
    assert!(n > 0);
    let mut rttvar = rtt / 2;
    for _ in 1..n {
        rttvar = rttvar * 3 / 4;
    }
    rttvar
}

/// This inserts a PING frame into packets.
struct PingWriter {}

impl test_internal::FrameWriter for PingWriter {
    fn write_frames(&mut self, builder: &mut PacketBuilder) {
        builder.encode_varint(FrameType::Ping);
    }
}

/// Drive the handshake between the client and server.
fn handshake_with_modifier(
    client: &mut Connection,
    server: &mut Connection,
    now: Instant,
    rtt: Duration,
    modifier: fn(Datagram) -> Option<Datagram>,
) -> Instant {
    let mut a = client;
    let mut b = server;
    let mut now = now;

    let mut input = None;
    let is_done = |c: &mut Connection| {
        matches!(
            c.state(),
            State::Confirmed | State::Closing { .. } | State::Closed(..)
        )
    };

    let mut did_ping = EnumMap::from_array([false, false]);
    while !is_done(a) {
        _ = maybe_authenticate(a);
        // Insert a PING frame into the first application data packet an endpoint sends,
        // in order to force the peer to ACK it. For the server, this is depending on the
        // client's connection state, which is accessible during the tests.
        //
        // We're doing this to prevent packet loss from delaying ACKs, which would cause
        // cwnd to shrink, and also to prevent the delayed ACK timer from being armed after
        // the handshake, which is not something the tests are written to account for.
        let should_ping = !did_ping[a.role()]
            && (a.role() == Role::Client && *a.state() == State::Connected
                || (a.role() == Role::Server && *b.state() == State::Connected));
        if should_ping {
            a.test_frame_writer = Some(Box::new(PingWriter {}));
        }
        let output = a.process(input, now).dgram();
        if should_ping {
            a.test_frame_writer = None;
            did_ping[a.role()] = true;
        }
        input = output.and_then(modifier);
        qtrace!("handshake: t += {:?}", rtt / 2);
        now += rtt / 2;
        mem::swap(&mut a, &mut b);
    }
    if let Some(d) = input {
        a.process_input(d, now);
    }
    now
}

fn handshake(
    client: &mut Connection,
    server: &mut Connection,
    now: Instant,
    rtt: Duration,
) -> Instant {
    handshake_with_modifier(client, server, now, rtt, Some)
}

fn connect_fail(
    client: &mut Connection,
    server: &mut Connection,
    client_error: Error,
    server_error: Error,
) {
    handshake(client, server, now(), Duration::new(0, 0));
    assert_error(client, &CloseReason::Transport(client_error));
    assert_error(server, &CloseReason::Transport(server_error));
}

fn connect_with_rtt_and_modifier(
    client: &mut Connection,
    server: &mut Connection,
    now: Instant,
    rtt: Duration,
    modifier: fn(Datagram) -> Option<Datagram>,
) -> Instant {
    fn check_rtt(stats: &Stats, rtt: Duration) {
        assert_eq!(stats.rtt, rtt);
        // Validate that rttvar has been computed correctly based on the number of RTT updates.
        let n = stats.frame_rx.ack + usize::from(stats.rtt_init_guess);
        assert_eq!(stats.rttvar, rttvar_after_n_updates(n, rtt));
    }
    let now = handshake_with_modifier(client, server, now, rtt, modifier);
    assert_eq!(*client.state(), State::Confirmed);
    assert_eq!(*server.state(), State::Confirmed);

    check_rtt(&client.stats(), rtt);
    check_rtt(&server.stats(), rtt);
    now
}

fn connect_with_rtt(
    client: &mut Connection,
    server: &mut Connection,
    now: Instant,
    rtt: Duration,
) -> Instant {
    connect_with_rtt_and_modifier(client, server, now, rtt, Some)
}

fn connect(client: &mut Connection, server: &mut Connection) {
    connect_with_rtt(client, server, now(), Duration::new(0, 0));
}

fn assert_error(c: &Connection, expected: &CloseReason) {
    match c.state() {
        State::Closing { error, .. } | State::Draining { error, .. } | State::Closed(error) => {
            assert_eq!(*error, *expected, "{c} error mismatch");
        }
        _ => panic!("bad state {:?}", c.state()),
    }
}

fn exchange_ticket(
    client: &mut Connection,
    server: &mut Connection,
    now: Instant,
) -> ResumptionToken {
    let validation = AddressValidation::new(now, ValidateAddress::NoToken).unwrap();
    let validation = Rc::new(RefCell::new(validation));
    server.set_validation(&validation);
    server.send_ticket(now, &[]).expect("can send ticket");
    let ticket = server.process_output(now).dgram();
    assert!(ticket.is_some());
    client.process_input(ticket.unwrap(), now);
    assert_eq!(*client.state(), State::Confirmed);
    get_tokens(client).pop().expect("should have token")
}

/// The `handshake` method inserts PING frames into the first application data packets,
/// which forces each peer to ACK them. As a side effect, that causes both sides of the
/// connection to be idle aftwerwards. This method simply verifies that this is the case.
fn assert_idle(client: &mut Connection, server: &mut Connection, rtt: Duration, now: Instant) {
    let idle_timeout = min(
        client.conn_params.get_idle_timeout(),
        server.conn_params.get_idle_timeout(),
    );
    // Client started its idle period half an RTT before now.
    assert_eq!(
        client.process_output(now),
        Output::Callback(idle_timeout - rtt / 2)
    );
    assert_eq!(server.process_output(now), Output::Callback(idle_timeout));
}

/// Connect with an RTT and then force both peers to be idle.
fn connect_rtt_idle_with_modifier(
    client: &mut Connection,
    server: &mut Connection,
    rtt: Duration,
    modifier: fn(Datagram) -> Option<Datagram>,
) -> Instant {
    let now = connect_with_rtt_and_modifier(client, server, now(), rtt, modifier);
    assert_idle(client, server, rtt, now);
    // Drain events from both as well.
    _ = client.events().count();
    _ = server.events().count();
    qtrace!("----- connected and idle with RTT {rtt:?}");
    now
}

fn connect_rtt_idle(client: &mut Connection, server: &mut Connection, rtt: Duration) -> Instant {
    connect_rtt_idle_with_modifier(client, server, rtt, Some)
}

fn connect_force_idle_with_modifier(
    client: &mut Connection,
    server: &mut Connection,
    modifier: fn(Datagram) -> Option<Datagram>,
) {
    connect_rtt_idle_with_modifier(client, server, Duration::new(0, 0), modifier);
}

fn connect_force_idle(client: &mut Connection, server: &mut Connection) {
    connect_force_idle_with_modifier(client, server, Some);
}

fn fill_stream(c: &mut Connection, stream: StreamId) {
    const BLOCK_SIZE: usize = 4_096;
    loop {
        let bytes_sent = c.stream_send(stream, &[0x42; BLOCK_SIZE]).unwrap();
        qtrace!("fill_cwnd wrote {bytes_sent} bytes");
        if bytes_sent < BLOCK_SIZE {
            break;
        }
    }
}

/// This fills the congestion window from a single source.
/// As the pacer will interfere with this, this moves time forward
/// as `Output::Callback` is received.  Because it is hard to tell
/// from the return value whether a timeout is an ACK delay, PTO, or
/// pacing, this looks at the congestion window to tell when to stop.
/// Returns a list of datagrams and the new time.
fn fill_cwnd(c: &mut Connection, stream: StreamId, mut now: Instant) -> (Vec<Datagram>, Instant) {
    qtrace!("fill_cwnd starting cwnd: {}", cwnd_avail(c));
    fill_stream(c, stream);

    let mut total_dgrams = Vec::new();
    loop {
        let pkt = c.process_output(now);
        qtrace!(
            "fill_cwnd cwnd remaining={}, output: {pkt:?}",
            cwnd_avail(c)
        );
        match pkt {
            Output::Datagram(dgram) => {
                total_dgrams.push(dgram);
            }
            Output::Callback(t) => {
                if cwnd_avail(c) < ACK_ONLY_SIZE_LIMIT {
                    break;
                }
                now += t;
            }
            Output::None => panic!(),
        }
    }

    qtrace!(
        "fill_cwnd sent {} bytes",
        total_dgrams.iter().map(Datagram::len).sum::<usize>()
    );
    (total_dgrams, now)
}

/// This function is like the combination of `fill_cwnd` and `ack_bytes`.
/// However, it acknowledges everything inline and preserves an RTT of `DEFAULT_RTT`.
fn increase_cwnd(
    sender: &mut Connection,
    receiver: &mut Connection,
    stream: StreamId,
    mut now: Instant,
) -> Instant {
    fill_stream(sender, stream);
    loop {
        let pkt = sender.process_output(now);
        match pkt {
            Output::Datagram(dgram) => {
                receiver.process_input(dgram, now + DEFAULT_RTT / 2);
            }
            Output::Callback(t) => {
                if t < DEFAULT_RTT {
                    now += t;
                } else {
                    break; // We're on PTO now.
                }
            }
            Output::None => panic!(),
        }
    }

    // Now acknowledge all those packets at once.
    now += DEFAULT_RTT / 2;
    let ack = receiver.process_output(now).dgram();
    now += DEFAULT_RTT / 2;
    sender.process_input(ack.unwrap(), now);
    now
}

/// Receive multiple packets and generate an ack-only packet.
///
/// # Panics
///
/// The caller is responsible for ensuring that `dest` has received
/// enough data that it wants to generate an ACK.  This panics if
/// no ACK frame is generated.
fn ack_bytes<D>(dest: &mut Connection, stream: StreamId, in_dgrams: D, now: Instant) -> Datagram
where
    D: IntoIterator<Item = Datagram>,
    D::IntoIter: ExactSizeIterator,
{
    let mut srv_buf = [0; 4_096];

    let in_dgrams = in_dgrams.into_iter();
    qdebug!("[{dest}] ack_bytes {} datagrams", in_dgrams.len());
    for dgram in in_dgrams {
        dest.process_input(dgram, now);
    }

    loop {
        let (bytes_read, _fin) = dest.stream_recv(stream, &mut srv_buf).unwrap();
        qtrace!("[{dest}] ack_bytes read {bytes_read} bytes");
        if bytes_read == 0 {
            break;
        }
    }

    dest.process_output(now).dgram().unwrap()
}

// Get the current congestion window for the connection.
fn cwnd(c: &Connection) -> usize {
    c.paths.primary().unwrap().borrow().sender().cwnd()
}

fn cwnd_avail(c: &Connection) -> usize {
    c.paths.primary().unwrap().borrow().sender().cwnd_avail()
}

fn cwnd_min(c: &Connection) -> usize {
    c.paths.primary().unwrap().borrow().sender().cwnd_min()
}

fn induce_persistent_congestion(
    client: &mut Connection,
    server: &mut Connection,
    stream: StreamId,
    mut now: Instant,
) -> Instant {
    // Note: wait some arbitrary time that should be longer than pto
    // timer. This is rather brittle.
    qtrace!("[{client}] induce_persistent_congestion");
    now += AT_LEAST_PTO;

    let mut pto_counts = [0; MAX_PTO_COUNTS];
    assert_eq!(client.stats.borrow().pto_counts, pto_counts);

    qtrace!("[{client}] first PTO");
    let (c_tx_dgrams, next_now) = fill_cwnd(client, stream, now);
    now = next_now;
    assert_eq!(c_tx_dgrams.len(), 2); // Two PTO packets

    pto_counts[0] = 1;
    assert_eq!(client.stats.borrow().pto_counts, pto_counts);

    qtrace!("[{client}] second PTO");
    now += AT_LEAST_PTO * 2;
    let (c_tx_dgrams, next_now) = fill_cwnd(client, stream, now);
    now = next_now;
    assert_eq!(c_tx_dgrams.len(), 2); // Two PTO packets

    pto_counts[0] = 0;
    pto_counts[1] = 1;
    assert_eq!(client.stats.borrow().pto_counts, pto_counts);

    qtrace!("[{client}] third PTO");
    now += AT_LEAST_PTO * 4;
    let (c_tx_dgrams, next_now) = fill_cwnd(client, stream, now);
    now = next_now;
    assert_eq!(c_tx_dgrams.len(), 2); // Two PTO packets

    pto_counts[1] = 0;
    pto_counts[2] = 1;
    assert_eq!(client.stats.borrow().pto_counts, pto_counts);

    // An ACK for the third PTO causes persistent congestion.
    let s_ack = ack_bytes(server, stream, c_tx_dgrams, now);
    client.process_input(s_ack, now);
    assert_eq!(cwnd(client), cwnd_min(client));
    now
}

/// This magic number is the size of the client's CWND after the handshake completes.
/// This is the same as the initial congestion window, because during the handshake
/// the cc is app limited and cwnd is not increased.
///
/// As we change how we build packets, or even as NSS changes,
/// this number might be different.  The tests that depend on this
/// value could fail as a result of variations, so it's OK to just
/// change this value, but it is good to first understand where the
/// change came from.
const POST_HANDSHAKE_CWND: usize = Pmtud::default_plpmtu(DEFAULT_ADDR.ip()) * CWND_INITIAL_PKTS;

/// Determine the number of packets required to fill the CWND.
const fn cwnd_packets(data: usize, mtu: usize) -> usize {
    // Add one if the last chunk is >= ACK_ONLY_SIZE_LIMIT.
    (data + mtu - ACK_ONLY_SIZE_LIMIT) / mtu
}

/// Determine the size of the last packet.
/// The minimal size of a packet is `ACK_ONLY_SIZE_LIMIT`.
const fn last_packet(cwnd: usize, mtu: usize) -> usize {
    if (cwnd % mtu) > ACK_ONLY_SIZE_LIMIT {
        cwnd % mtu
    } else {
        mtu
    }
}

/// Assert that the set of packets fill the CWND.
fn assert_full_cwnd(packets: &[Datagram], cwnd: usize, mtu: usize) {
    assert_eq!(packets.len(), cwnd_packets(cwnd, mtu));
    let (last, rest) = packets.split_last().unwrap();
    assert!(rest.iter().all(|d| d.len() == mtu));
    assert_eq!(last.len(), last_packet(cwnd, mtu));
}

/// Send something on a stream from `sender` to `receiver`, maybe allowing for pacing.
/// Takes a modifier function that can be used to modify the datagram before it is sent.
/// Return the resulting datagram and the new time.
#[must_use]
fn send_something_paced_with_modifier(
    sender: &mut Connection,
    mut now: Instant,
    allow_pacing: bool,
    modifier: fn(Datagram) -> Option<Datagram>,
) -> (Datagram, Instant) {
    let stream_id = sender.stream_create(StreamType::UniDi).unwrap();
    assert!(sender.stream_send(stream_id, DEFAULT_STREAM_DATA).is_ok());
    assert!(sender.stream_close_send(stream_id).is_ok());
    qdebug!("[{sender}] send_something on {stream_id}");
    let dgram = match sender.process_output(now) {
        Output::Callback(t) => {
            assert!(allow_pacing, "send_something: unexpected delay");
            now += t;
            sender
                .process_output(now)
                .dgram()
                .expect("send_something: should have something to send")
        }
        Output::Datagram(d) => d,
        Output::None => panic!("send_something: got Output::None"),
    };
    (modifier(dgram).unwrap(), now)
}

fn send_something_paced(
    sender: &mut Connection,
    now: Instant,
    allow_pacing: bool,
) -> (Datagram, Instant) {
    send_something_paced_with_modifier(sender, now, allow_pacing, Some)
}

fn send_something_with_modifier(
    sender: &mut Connection,
    now: Instant,
    modifier: fn(Datagram) -> Option<Datagram>,
) -> Datagram {
    send_something_paced_with_modifier(sender, now, false, modifier).0
}

/// Send something on a stream from `sender` to `receiver`.
/// Return the resulting datagram.
fn send_something(sender: &mut Connection, now: Instant) -> Datagram {
    send_something_with_modifier(sender, now, Some)
}

/// Send something, but add a little something extra into the output.
fn send_with_extra<W>(sender: &mut Connection, writer: W, now: Instant) -> Datagram
where
    W: test_internal::FrameWriter + 'static,
{
    sender.test_frame_writer = Some(Box::new(writer));
    let res = send_something_with_modifier(sender, now, Some);
    sender.test_frame_writer = None;
    res
}

/// Send something on a stream from `sender` through a modifier to `receiver`.
/// Return any ACK that might result.
fn send_with_modifier_and_receive(
    sender: &mut Connection,
    receiver: &mut Connection,
    now: Instant,
    modifier: fn(Datagram) -> Option<Datagram>,
) -> Option<Datagram> {
    let dgram = send_something_with_modifier(sender, now, modifier);
    receiver.process(Some(dgram), now).dgram()
}

/// Send something on a stream from `sender` to `receiver`.
/// Return any ACK that might result.
fn send_and_receive(
    sender: &mut Connection,
    receiver: &mut Connection,
    now: Instant,
) -> Option<Datagram> {
    send_with_modifier_and_receive(sender, receiver, now, Some)
}

fn get_tokens(client: &mut Connection) -> Vec<ResumptionToken> {
    client
        .events()
        .filter_map(|e| {
            if let ConnectionEvent::ResumptionToken(token) = e {
                Some(token)
            } else {
                None
            }
        })
        .collect()
}

fn assert_default_stats(stats: &Stats) {
    assert_eq!(stats.packets_rx, 0);
    assert_eq!(stats.packets_tx, 0);
    let dflt_frames = FrameStats::default();
    assert_eq!(stats.frame_rx, dflt_frames);
    assert_eq!(stats.frame_tx, dflt_frames);
}

fn assert_path_challenge_min_len(c: &Connection, d: &Datagram, now: Instant) {
    let path = c.paths.find_path(
        d.source(),
        d.destination(),
        &c.conn_params,
        now,
        &mut c.stats.borrow_mut(),
    );
    if path.borrow().amplification_limit() < path.borrow().plpmtu() {
        // If the amplification limit is less than the PLPMTU, then the path
        // challenge will not have been padded.
        return;
    }
    assert!(
        d.len() >= MIN_INITIAL_PACKET_SIZE,
        "{} < {MIN_INITIAL_PACKET_SIZE}",
        d.len()
    );
}

#[test]
fn create_client() {
    let client = default_client();
    assert_eq!(client.role(), Role::Client);
    assert!(matches!(client.state(), State::Init));
    let stats = client.stats();
    assert_default_stats(&stats);
    assert_eq!(stats.rtt, crate::rtt::INITIAL_RTT);
    assert_eq!(stats.rttvar, crate::rtt::INITIAL_RTT / 2);
}

#[test]
fn create_server() {
    let server = default_server();
    assert_eq!(server.role(), Role::Server);
    assert!(matches!(server.state(), State::Init));
    let stats = server.stats();
    assert_default_stats(&stats);
    // Server won't have a default path, so no RTT.
    assert_eq!(stats.rtt, Duration::from_secs(0));
}

#[test]
fn tp_grease() {
    for enable in [true, false] {
        let client = new_client(ConnectionParameters::default().grease(enable));
        let grease = client.tps.borrow_mut().local().get_empty(GreaseQuicBit);
        assert_eq!(enable, grease);
    }
}

#[test]
fn tp_disable_migration() {
    for disable in [true, false] {
        let client = new_client(ConnectionParameters::default().disable_migration(disable));
        let disable_migration = client.tps.borrow_mut().local().get_empty(DisableMigration);
        assert_eq!(disable, disable_migration);
    }
}
