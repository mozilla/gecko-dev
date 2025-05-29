// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::{
    cell::RefCell,
    net::{IpAddr, Ipv6Addr, SocketAddr},
    rc::Rc,
    time::Duration,
};

use neqo_common::{event::Provider as _, qdebug, Datagram};
use neqo_crypto::{
    constants::TLS_CHACHA20_POLY1305_SHA256, generate_ech_keys, AuthenticationStatus,
};
#[cfg(not(feature = "disable-encryption"))]
use test_fixture::datagram;
use test_fixture::{
    assertions, assertions::assert_coalesced_0rtt, fixture_init, now, split_datagram, DEFAULT_ADDR,
};

use super::{
    super::{Connection, Output, State},
    assert_error, connect, connect_force_idle, connect_with_rtt, default_client, default_server,
    get_tokens, handshake, maybe_authenticate, resumed_server, send_something, zero_len_cid_client,
    CountingConnectionIdGenerator, AT_LEAST_PTO, DEFAULT_RTT, DEFAULT_STREAM_DATA,
};
use crate::{
    connection::{
        tests::{exchange_ticket, new_client, new_server},
        AddressValidation,
    },
    events::ConnectionEvent,
    server::ValidateAddress,
    stats::FrameStats,
    tparams::{TransportParameter, TransportParameterId::*},
    tracking::DEFAULT_LOCAL_ACK_DELAY,
    CloseReason, ConnectionParameters, Error, Pmtud, StreamType, Version,
};

const ECH_CONFIG_ID: u8 = 7;
const ECH_PUBLIC_NAME: &str = "public.example";

fn full_handshake(pmtud: bool) {
    qdebug!("---- client: generate CH");
    let mut client = new_client(ConnectionParameters::default().pmtud(pmtud));
    let out = client.process_output(now());
    let out2 = client.process_output(now());
    assert!(out.as_dgram_ref().is_some() && out2.as_dgram_ref().is_some());
    assert_eq!(out.as_dgram_ref().unwrap().len(), client.plpmtu());
    assert_eq!(out2.as_dgram_ref().unwrap().len(), client.plpmtu());

    qdebug!("---- server: CH -> SH, EE, CERT, CV, FIN");
    let mut server = new_server(ConnectionParameters::default().pmtud(pmtud));
    server.process_input(out.dgram().unwrap(), now());
    let out = server.process(out2.dgram(), now());
    assert!(out.as_dgram_ref().is_some());
    assert_eq!(out.as_dgram_ref().unwrap().len(), server.plpmtu());

    qdebug!("---- client: cert verification");
    let out = client.process(out.dgram(), now());
    assert!(out.as_dgram_ref().is_some());

    let out = server.process(out.dgram(), now());
    let out = client.process(out.dgram(), now());

    let out = server.process(out.dgram(), now());
    assert!(out.as_dgram_ref().is_none());

    assert!(maybe_authenticate(&mut client));

    qdebug!("---- client: SH..FIN -> FIN");
    let out = client.process(out.dgram(), now());
    assert!(out.as_dgram_ref().is_some());
    assert_eq!(*client.state(), State::Connected);

    qdebug!("---- server: FIN -> ACKS");
    let out = server.process(out.dgram(), now());
    assert!(out.as_dgram_ref().is_some());
    assert_eq!(*server.state(), State::Confirmed);

    qdebug!("---- client: ACKS -> 0");
    let out = client.process(out.dgram(), now());
    if pmtud {
        // PMTUD causes a PING probe to be sent here
        let pkt = out.dgram().unwrap();
        assert!(pkt.len() > client.plpmtu());
    } else {
        assert!(out.as_dgram_ref().is_none());
    }
    assert_eq!(*client.state(), State::Confirmed);
}

#[test]
fn handshake_no_pmtud() {
    full_handshake(false);
}

#[test]
fn handshake_pmtud() {
    full_handshake(true);
}

#[test]
fn handshake_failed_authentication() {
    qdebug!("---- client: generate CH");
    let mut client = default_client();
    let out = client.process_output(now());
    let out2 = client.process_output(now());
    assert!(out.as_dgram_ref().is_some() && out2.as_dgram_ref().is_some());

    qdebug!("---- server: CH -> SH, EE, CERT, CV, FIN");
    let mut server = default_server();
    server.process_input(out.dgram().unwrap(), now());
    let out = server.process(out2.dgram(), now());
    assert!(out.as_dgram_ref().is_some());

    qdebug!("---- client: cert verification");
    let out = client.process(out.dgram(), now());
    assert!(out.as_dgram_ref().is_some());

    let out = server.process(out.dgram(), now());
    assert!(out.as_dgram_ref().is_some());
    let out = client.process(out.dgram(), now());
    assert!(out.as_dgram_ref().is_some());
    let out = server.process(out.dgram(), now());
    assert!(out.as_dgram_ref().is_none());

    let authentication_needed = |e| matches!(e, ConnectionEvent::AuthenticationNeeded);
    assert!(client.events().any(authentication_needed));
    qdebug!("---- client: Alert(certificate_revoked)");
    client.authenticated(AuthenticationStatus::CertRevoked, now());

    qdebug!("---- client: -> Alert(certificate_revoked)");
    let out = client.process_output(now());
    assert!(out.as_dgram_ref().is_some());

    qdebug!("---- server: Alert(certificate_revoked)");
    let out = server.process(out.dgram(), now());
    assert!(out.as_dgram_ref().is_some());
    assert_error(&client, &CloseReason::Transport(Error::CryptoAlert(44)));
    assert_error(&server, &CloseReason::Transport(Error::PeerError(300)));
}

#[test]
fn no_alpn() {
    fixture_init();
    let mut client = Connection::new_client(
        "example.com",
        &["bad-alpn"],
        Rc::new(RefCell::new(CountingConnectionIdGenerator::default())),
        DEFAULT_ADDR,
        DEFAULT_ADDR,
        ConnectionParameters::default(),
        now(),
    )
    .unwrap();
    let mut server = default_server();

    handshake(&mut client, &mut server, now(), Duration::new(0, 0));
    // TODO (mt): errors are immediate, which means that we never send CONNECTION_CLOSE
    // and the client never sees the server's rejection of its handshake.
    // assert_error(&client, CloseReason::Transport(Error::CryptoAlert(120)));
    assert_error(&server, &CloseReason::Transport(Error::CryptoAlert(120)));
}

#[test]
#[expect(clippy::cognitive_complexity, reason = "OK in a test.")]
fn dup_server_flight1() {
    qdebug!("---- client: generate CH");
    let mut client = default_client();
    let out = client.process_output(now());
    let out2 = client.process_output(now());
    assert!(out.as_dgram_ref().is_some() && out2.as_dgram_ref().is_some());
    assert_eq!(out.as_dgram_ref().unwrap().len(), client.plpmtu());
    assert_eq!(out2.as_dgram_ref().unwrap().len(), client.plpmtu());
    qdebug!(
        "Output={:0x?} {:0x?}",
        out.as_dgram_ref(),
        out2.as_dgram_ref()
    );

    qdebug!("---- server: CH -> SH, EE, CERT, CV, FIN");
    let mut server = default_server();
    server.process_input(out.dgram().unwrap(), now());
    let out_to_rep = server.process(out2.dgram(), now());
    assert!(out_to_rep.as_dgram_ref().is_some());
    qdebug!("Output={:0x?}", out_to_rep.as_dgram_ref());

    qdebug!("---- client: cert verification");
    let out = client.process(Some(out_to_rep.as_dgram_ref().cloned().unwrap()), now());
    let out_to_rep2 = server.process(out.dgram(), now());
    let out = client.process(out_to_rep2.clone().dgram(), now());

    assert!(out.as_dgram_ref().is_some());
    qdebug!("Output={:0x?}", out.as_dgram_ref());

    let out = server.process(out.dgram(), now());
    assert!(out.as_dgram_ref().is_none());

    assert!(maybe_authenticate(&mut client));

    qdebug!("---- client: SH..FIN -> FIN");
    let out = client.process_output(now());
    assert!(out.as_dgram_ref().is_some());
    qdebug!("Output={:0x?}", out.as_dgram_ref());

    assert_eq!(4, client.stats().packets_rx);
    assert_eq!(0, client.stats().dups_rx);
    assert_eq!(1, client.stats().dropped_rx);

    qdebug!("---- Dup, ignored");
    client.process_input(out_to_rep.dgram().unwrap(), now());
    let out = client.process(out_to_rep2.dgram(), now());
    assert!(out.as_dgram_ref().is_none());
    qdebug!("Output={:0x?}", out.as_dgram_ref());

    // Four packets total received, 1 of them is a dup and one has been dropped because Initial keys
    // are dropped.  Add 2 counts of the padding that the server adds to Initial packets.
    assert_eq!(8, client.stats().packets_rx);
    assert_eq!(1, client.stats().dups_rx);
    assert_eq!(4, client.stats().dropped_rx);
}

// Test that we split crypto data if they cannot fit into one packet.
// To test this we will use a long server certificate.
#[test]
fn crypto_frame_split() {
    // This test has its own logic for generating large CRYPTO frames, so turn off MLKEM.
    let mut client = new_client(ConnectionParameters::default().mlkem(false));

    let mut server = Connection::new_server(
        test_fixture::LONG_CERT_KEYS,
        test_fixture::DEFAULT_ALPN,
        Rc::new(RefCell::new(CountingConnectionIdGenerator::default())),
        ConnectionParameters::default(),
    )
    .expect("create a server");

    let client1 = client.process_output(now());
    assert!(client1.as_dgram_ref().is_some());

    // The entire server flight doesn't fit in a single packet because the
    // certificate is large, therefore the server will produce 2 packets.
    let server1 = server.process(client1.dgram(), now());
    assert!(server1.as_dgram_ref().is_some());
    let server2 = server.process_output(now());
    assert!(server2.as_dgram_ref().is_some());

    let client2 = client.process(server1.dgram(), now());
    // This is an ack.
    assert!(client2.as_dgram_ref().is_some());
    // The client might have the certificate now, so we can't guarantee that
    // this will work.
    let auth1 = maybe_authenticate(&mut client);
    assert_eq!(*client.state(), State::Handshaking);

    // let server process the ack for the first packet.
    let server3 = server.process(client2.dgram(), now());
    assert!(server3.as_dgram_ref().is_none());

    // Consume the second packet from the server.
    let client3 = client.process(server2.dgram(), now());

    // Check authentication.
    let auth2 = maybe_authenticate(&mut client);
    assert!(auth1 ^ auth2);
    // Now client has all data to finish handshake.
    assert_eq!(*client.state(), State::Connected);

    let client4 = client.process(server3.dgram(), now());
    // One of these will contain data depending on whether Authentication was completed
    // after the first or second server packet.
    assert!(client3.as_dgram_ref().is_some() ^ client4.as_dgram_ref().is_some());

    drop(server.process(client3.dgram(), now()));
    drop(server.process(client4.dgram(), now()));

    assert_eq!(*client.state(), State::Connected);
    assert_eq!(*server.state(), State::Confirmed);
}

/// Run a single ChaCha20-Poly1305 test and get a PTO.
#[test]
fn chacha20poly1305() {
    let mut server = default_server();
    let mut client = zero_len_cid_client(DEFAULT_ADDR, DEFAULT_ADDR);
    client.set_ciphers(&[TLS_CHACHA20_POLY1305_SHA256]).unwrap();
    connect_force_idle(&mut client, &mut server);
}

/// Test that a server can send 0.5 RTT application data.
#[test]
fn send_05rtt() {
    let mut client = default_client();
    let mut server = default_server();

    let c1 = client.process_output(now()).dgram();
    let c2 = client.process_output(now()).dgram();
    assert!(c1.is_some() && c2.is_some());
    server.process_input(c1.unwrap(), now());
    let s1 = server.process(c2, now()).dgram().unwrap();
    assert_eq!(s1.len(), server.plpmtu());

    // The server should accept writes at this point.
    let s2 = send_something(&mut server, now());

    // Complete the handshake at the client.
    client.process_input(s1, now());

    // The client should receive the 0.5-RTT data now.
    client.process_input(s2, now());
    maybe_authenticate(&mut client);
    assert_eq!(*client.state(), State::Connected);
    let mut buf = vec![0; DEFAULT_STREAM_DATA.len() + 1];
    let stream_id = client
        .events()
        .find_map(|e| {
            if let ConnectionEvent::RecvStreamReadable { stream_id } = e {
                Some(stream_id)
            } else {
                None
            }
        })
        .unwrap();
    let (l, ended) = client.stream_recv(stream_id, &mut buf).unwrap();
    assert_eq!(&buf[..l], DEFAULT_STREAM_DATA);
    assert!(ended);
}

/// Test that a client buffers 0.5-RTT data when it arrives early.
#[test]
fn reorder_05rtt() {
    // This test makes too many assumptions about single-packet PTOs for multi-packet MLKEM flights
    let mut client = new_client(ConnectionParameters::default().mlkem(false));
    let mut server = default_server();

    let c1 = client.process_output(now()).dgram();
    assert!(c1.is_some());
    let s1 = server.process(c1, now()).dgram().unwrap();

    // The server should accept writes at this point.
    let s2 = send_something(&mut server, now());

    // We can't use the standard facility to complete the handshake, so
    // drive it as aggressively as possible.
    assert_eq!(client.stats().saved_datagrams, 0);
    assert_eq!(client.stats().packets_rx, 0);
    client.process_input(s2, now());
    assert_eq!(client.stats().saved_datagrams, 1);
    assert_eq!(client.stats().packets_rx, 0);

    // After processing the first packet, the client should go back and
    // process the 0.5-RTT packet data, which should make data available.
    client.process_input(s1, now());
    // We can't use `maybe_authenticate` here as that consumes events.
    client.authenticated(AuthenticationStatus::Ok, now());
    assert_eq!(*client.state(), State::Connected);

    let mut buf = vec![0; DEFAULT_STREAM_DATA.len() + 1];
    let stream_id = client
        .events()
        .find_map(|e| {
            if let ConnectionEvent::RecvStreamReadable { stream_id } = e {
                Some(stream_id)
            } else {
                None
            }
        })
        .unwrap();
    let (l, ended) = client.stream_recv(stream_id, &mut buf).unwrap();
    assert_eq!(&buf[..l], DEFAULT_STREAM_DATA);
    assert!(ended);
}

#[test]
fn reorder_05rtt_with_0rtt() {
    const RTT: Duration = Duration::from_millis(100);

    let mut client = default_client();
    let mut server = default_server();
    let validation = AddressValidation::new(now(), ValidateAddress::NoToken).unwrap();
    let validation = Rc::new(RefCell::new(validation));
    server.set_validation(&validation);
    let mut now = connect_with_rtt(&mut client, &mut server, now(), RTT);

    // Include RTT in sending the ticket or the ticket age reported by the
    // client is wrong, which causes the server to reject 0-RTT.
    now += RTT / 2;
    server.send_ticket(now, &[]).unwrap();
    let ticket = server.process_output(now).dgram().unwrap();
    now += RTT / 2;
    client.process_input(ticket, now);

    let token = get_tokens(&mut client).pop().unwrap();
    // This test makes too many assumptions about what's in the packets to work with multi-packet
    // MLKEM flights.
    let mut client = new_client(ConnectionParameters::default().mlkem(false));
    client.enable_resumption(now, token).unwrap();
    let mut server = resumed_server(&client);

    // Send ClientHello and some 0-RTT.
    let c1 = send_something(&mut client, now);
    assert_coalesced_0rtt(&c1[..]);
    // Drop the 0-RTT from the coalesced datagram, so that the server
    // acknowledges the next 0-RTT packet.
    let (c1, _) = split_datagram(&c1);
    let c2 = send_something(&mut client, now);

    // Handle the first packet and send 0.5-RTT in response.  Drop the response.
    now += RTT / 2;
    drop(server.process(Some(c1), now).dgram().unwrap());
    // The gap in 0-RTT will result in this 0.5 RTT containing an ACK.
    server.process_input(c2, now);
    let s2 = send_something(&mut server, now);

    // Save the 0.5 RTT.
    now += RTT / 2;
    client.process_input(s2, now);
    assert_eq!(client.stats().saved_datagrams, 1);

    // Now PTO at the client and cause the server to re-send handshake packets.
    now += AT_LEAST_PTO;
    let c3 = client.process_output(now).dgram();
    assert_coalesced_0rtt(c3.as_ref().unwrap());

    now += RTT / 2;
    let s3 = server.process(c3, now).dgram().unwrap();

    // The client should be able to process the 0.5 RTT now.
    // This should contain an ACK, so we are processing an ACK from the past.
    now += RTT / 2;
    client.process_input(s3, now);
    maybe_authenticate(&mut client);
    let c4 = client.process_output(now).dgram();
    assert_eq!(*client.state(), State::Connected);
    assert_eq!(client.paths.rtt(), RTT);

    now += RTT / 2;
    server.process_input(c4.unwrap(), now);
    assert_eq!(*server.state(), State::Confirmed);
    // Don't check server RTT as it will be massively inflated by a
    // poor initial estimate received when the server dropped the
    // Initial packet number space.
}

/// Test that a server that coalesces 0.5 RTT with handshake packets
/// doesn't cause the client to drop application data.
#[test]
fn coalesce_05rtt() {
    const RTT: Duration = Duration::from_millis(100);
    let mut client = default_client();
    let mut server = default_server();
    let mut now = now();

    // The first exchange doesn't offer a chance for the server to send.
    // So drop the server flight and wait for the PTO.
    let c1 = client.process_output(now).dgram();
    let c11 = client.process_output(now).dgram();
    assert!(c1.is_some() && c11.is_some());
    now += RTT / 2;
    server.process_input(c1.unwrap(), now);
    let s1 = server.process(c11, now).dgram();
    assert!(s1.is_some());

    // Drop the server flight.  Then send some data.
    let stream_id = server.stream_create(StreamType::UniDi).unwrap();
    assert!(server.stream_send(stream_id, DEFAULT_STREAM_DATA).is_ok());
    assert!(server.stream_close_send(stream_id).is_ok());

    // Now after a PTO the client can send another packet.
    // The server should then send its entire flight again,
    // including the application data, which it sends in a 1-RTT packet.
    now += AT_LEAST_PTO;
    let c2 = client.process_output(now).dgram();
    let c21 = client.process_output(now).dgram();
    assert!(c2.is_some() && c21.is_some());
    now += RTT / 2;
    server.process_input(c21.unwrap(), now);
    let s2 = server.process(c2, now).dgram();

    let dgram = client.process(s2, now).dgram();
    let s2 = server.process(dgram, now).dgram();

    // Even though there is a 1-RTT packet at the end of the datagram, the
    // flight should be padded to full size.
    assert_eq!(s2.as_ref().unwrap().len(), server.plpmtu());

    // The client should process the datagram.  It can't process the 1-RTT
    // packet until authentication completes though.  So it saves it.
    now += RTT / 2;
    assert_eq!(client.stats().dropped_rx, 0);
    drop(client.process(s2, now).dgram());
    // This packet will contain an ACK, but we can ignore it.
    assert_eq!(client.stats().dropped_rx, 0);
    assert_eq!(client.stats().packets_rx, 3);
    assert_eq!(client.stats().saved_datagrams, 1);

    // After (successful) authentication, the packet is processed.
    maybe_authenticate(&mut client);
    let c3 = client.process_output(now).dgram();
    assert!(c3.is_some());
    assert_eq!(client.stats().dropped_rx, 0); // No Initial padding.
    assert_eq!(client.stats().packets_rx, 4);
    assert_eq!(client.stats().saved_datagrams, 1);
    assert!(client.stats().frame_rx.padding > 0); // Padding uses frames.

    // Allow the handshake to complete.
    now += RTT / 2;
    let s3 = server.process(c3, now).dgram();
    assert!(s3.is_some());
    assert_eq!(*server.state(), State::Confirmed);
    now += RTT / 2;
    drop(client.process(s3, now).dgram());
    assert_eq!(*client.state(), State::Confirmed);

    assert_eq!(client.stats().dropped_rx, 0); // No dropped packets.
}

#[test]
fn reorder_handshake() {
    const RTT: Duration = Duration::from_millis(100);
    let mut client = default_client();
    let mut server = default_server();
    let mut now = now();

    let c1 = client.process_output(now).dgram();
    let c2 = client.process_output(now).dgram();
    assert!(c1.is_some() && c2.is_some());

    now += RTT / 2;
    server.process_input(c1.unwrap(), now);
    let s1 = server.process(c2, now).dgram();
    assert!(s1.is_some());
    now += RTT / 2;
    let dgram = client.process(s1, now).dgram();
    assert!(dgram.is_some());
    now += RTT / 2;
    let s1 = server.process(dgram, now).dgram();
    assert!(s1.is_some());

    // Drop the Initial packet from this.
    let (_, s_hs) = split_datagram(&s1.unwrap());
    assert!(s_hs.is_some());

    // Pass just the handshake packet in and the client can't handle it yet.
    // It can only send another Initial packet.
    now += RTT + RTT / 2; // With multi-packet MLKEM flights, client needs more time here.
    let dgram = client.process(s_hs, now).dgram();
    assertions::assert_initial(dgram.as_ref().unwrap(), false);
    assert_eq!(client.stats().saved_datagrams, 1);
    assert_eq!(client.stats().packets_rx, 1);

    // Get the server to try again.
    // Though we currently allow the server to arm its PTO timer, use
    // a second client Initial packet to cause it to send again.
    now += AT_LEAST_PTO;
    let c2 = client.process_output(now).dgram();
    now += RTT / 2;
    let s2 = server.process(c2, now).dgram();
    assert!(s2.is_some());

    let (s_init, s_hs) = split_datagram(&s2.unwrap());
    assert!(s_hs.is_some());

    // Processing the Handshake packet first should save it.
    now += RTT / 2;
    client.process_input(s_hs.unwrap(), now);
    assert_eq!(client.stats().saved_datagrams, 2);
    assert_eq!(client.stats().packets_rx, 1);

    client.process_input(s_init, now);
    // Each saved packet should now be "received" again.
    assert_eq!(client.stats().packets_rx, 6);
    maybe_authenticate(&mut client);
    let c3 = client.process_output(now).dgram();
    assert!(c3.is_some());

    // Note that though packets were saved and processed very late,
    // they don't cause the RTT to change.
    now += RTT / 2;
    let s3 = server.process(c3, now).dgram();
    assert_eq!(*server.state(), State::Confirmed);
    // Don't check server RTT estimate as it will be inflated due to
    // it making a guess based on retransmissions when it dropped
    // the Initial packet number space.

    now += RTT / 2;
    client.process_input(s3.unwrap(), now);
    assert_eq!(*client.state(), State::Confirmed);
    assert_eq!(client.paths.rtt(), RTT);
}

#[test]
fn reorder_1rtt() {
    const RTT: Duration = Duration::from_millis(100);
    const PACKETS: usize = 4; // Many, but not enough to overflow cwnd.
    let mut client = default_client();
    let mut server = default_server();
    let mut now = now();

    let c1 = client.process_output(now).dgram();
    let c2 = client.process_output(now).dgram();
    assert!(c1.is_some() && c2.is_some());

    now += RTT / 2;
    server.process_input(c1.unwrap(), now);
    let s1 = server.process(c2, now).dgram();
    assert!(s1.is_some());

    now += RTT / 2;
    let dgram = client.process(s1, now).dgram();

    now += RTT / 2;
    let dgram = server.process(dgram, now).dgram();

    now += RTT / 2;
    client.process_input(dgram.unwrap(), now);
    maybe_authenticate(&mut client);
    let c2 = client.process_output(now).dgram();
    assert!(c2.is_some());

    // Now get a bunch of packets from the client.
    // Give them to the server before giving it `c2`.
    for _ in 0..PACKETS {
        let d = send_something(&mut client, now);
        server.process_input(d, now + RTT / 2);
    }
    // The server has now received those packets, and saved them.
    // The six extra received are Initial + the junk we use for padding.
    assert_eq!(server.stats().packets_rx, PACKETS + 2);
    assert_eq!(server.stats().saved_datagrams, PACKETS);
    assert_eq!(server.stats().dropped_rx, 3);

    now += RTT / 2;
    let s2 = server.process(c2, now).dgram();
    // The server has now received those packets, and saved them.
    // The two additional are an Initial w/ACK, a Handshake w/ACK and a 1-RTT (w/
    // NEW_CONNECTION_ID).
    assert_eq!(server.stats().packets_rx, PACKETS * 2 + 5);
    assert_eq!(server.stats().saved_datagrams, PACKETS);
    assert_eq!(server.stats().dropped_rx, 3);
    assert_eq!(*server.state(), State::Confirmed);
    assert_eq!(server.paths.rtt(), RTT);

    now += RTT / 2;
    client.process_input(s2.unwrap(), now);
    assert_eq!(client.paths.rtt(), RTT);

    // All the stream data that was sent should now be available.
    let streams = server
        .events()
        .filter_map(|e| {
            if let ConnectionEvent::RecvStreamReadable { stream_id } = e {
                Some(stream_id)
            } else {
                None
            }
        })
        .collect::<Vec<_>>();
    assert_eq!(streams.len(), PACKETS);
    for stream_id in streams {
        let mut buf = vec![0; DEFAULT_STREAM_DATA.len() + 1];
        let (recvd, fin) = server.stream_recv(stream_id, &mut buf).unwrap();
        assert_eq!(recvd, DEFAULT_STREAM_DATA.len());
        assert!(fin);
    }
}

#[cfg(not(feature = "disable-encryption"))]
#[test]
fn corrupted_initial() {
    let mut client = default_client();
    let mut server = default_server();
    let d = client.process_output(now()).dgram().unwrap();
    let mut corrupted = Vec::from(&d[..]);
    // Find the last non-zero value and corrupt that.
    let (idx, _) = corrupted
        .iter()
        .enumerate()
        .rev()
        .find(|(_, &v)| v != 0)
        .unwrap();
    corrupted[idx] ^= 0x76;
    let dgram = Datagram::new(d.source(), d.destination(), d.tos(), corrupted);
    server.process_input(dgram, now());
    // The server should have received two packets,
    // the first should be dropped, the second saved.
    assert_eq!(server.stats().packets_rx, 2);
    assert_eq!(server.stats().dropped_rx, 2);
    assert_eq!(server.stats().saved_datagrams, 0);
}

#[test]
// Absent path PTU discovery, max v6 packet size should be PATH_MTU_V6.
fn verify_pkt_honors_mtu() {
    let mut client = default_client();
    let mut server = default_server();
    connect_force_idle(&mut client, &mut server);

    let now = now();

    let res = client.process_output(now);
    let idle_timeout = ConnectionParameters::default().get_idle_timeout();
    assert_eq!(res, Output::Callback(idle_timeout));

    // Try to send a large stream and verify first packet is correctly sized
    let stream_id = client.stream_create(StreamType::UniDi).unwrap();
    assert_eq!(client.stream_send(stream_id, &[0xbb; 2000]).unwrap(), 2000);
    let pkt0 = client.process_output(now);
    assert!(matches!(pkt0, Output::Datagram(_)));
    assert_eq!(pkt0.as_dgram_ref().unwrap().len(), client.plpmtu());
}

#[test]
fn extra_initial_hs() {
    let mut client = default_client();
    let mut server = default_server();
    let mut now = now();

    let c_init = client.process_output(now).dgram();
    let c_init2 = client.process_output(now).dgram();
    assert!(c_init.is_some() && c_init2.is_some());
    now += DEFAULT_RTT / 2;
    server.process_input(c_init.unwrap(), now);
    let s_init = server.process(c_init2, now).dgram();
    assert!(s_init.is_some());
    now += DEFAULT_RTT / 2;

    let dgram = client.process(s_init, now).dgram();
    let s_init = server.process(dgram, now).dgram();

    // Drop the Initial packet, keep only the Handshake.
    let (_, undecryptable) = split_datagram(&s_init.unwrap());
    assert!(undecryptable.is_some());

    // Feed the same undecryptable packet into the client a few times.
    // Do that EXTRA_INITIALS times and each time the client will emit
    // another Initial packet.
    for _ in 0..=super::super::EXTRA_INITIALS {
        let c_init = match client.process(undecryptable.clone(), now) {
            Output::None => todo!(),
            Output::Datagram(c_init) => Some(c_init),
            Output::Callback(duration) => {
                now += duration;
                client.process_output(now).dgram()
            }
        };
        assertions::assert_initial(c_init.as_ref().unwrap(), false);
        now += DEFAULT_RTT / 10;
    }

    // After EXTRA_INITIALS, the client stops sending Initial packets.
    let nothing = client.process(undecryptable, now).dgram();
    assert!(nothing.is_none());

    // Until PTO, where another Initial can be used to complete the handshake.
    now += client.process_output(now).callback();
    let c_init = client.process_output(now).dgram();
    assertions::assert_initial(c_init.as_ref().unwrap(), false);
    now += DEFAULT_RTT / 2;
    let s_init = server.process(c_init, now).dgram();
    now += DEFAULT_RTT / 2;
    client.process_input(s_init.unwrap(), now);
    maybe_authenticate(&mut client);
    let c_fin = client.process_output(now).dgram();
    assert_eq!(*client.state(), State::Connected);
    now += DEFAULT_RTT / 2;
    server.process_input(c_fin.unwrap(), now);
    assert_eq!(*server.state(), State::Confirmed);
}

#[test]
fn extra_initial_invalid_cid() {
    let mut client = default_client();
    let mut server = default_server();
    let mut now = now();

    let c_init = client.process_output(now).dgram();
    let c_init2 = client.process_output(now).dgram();
    assert!(c_init.is_some() && c_init2.is_some());
    now += DEFAULT_RTT / 2;
    server.process_input(c_init.unwrap(), now);
    let s_init = server.process(c_init2, now).dgram();
    assert!(s_init.is_some());
    now += DEFAULT_RTT / 2;

    let dgram = client.process(s_init, now).dgram();
    let s_init = server.process(dgram, now).dgram();

    // If the client receives a packet that contains the wrong connection
    // ID, it won't send another Initial.
    let (_, hs) = split_datagram(&s_init.unwrap());
    let hs = hs.unwrap();
    let mut copy = hs.to_vec();
    assert_ne!(copy[5], 0); // The DCID should be non-zero length.
    copy[6] ^= 0xc4;
    let dgram_copy = Datagram::new(hs.destination(), hs.source(), hs.tos(), copy);
    let nothing = client.process(Some(dgram_copy), now).dgram();
    assert!(nothing.is_none());
}

#[test]
fn connect_one_version() {
    fn connect_v(version: Version) {
        fixture_init();
        let mut client = Connection::new_client(
            test_fixture::DEFAULT_SERVER_NAME,
            test_fixture::DEFAULT_ALPN,
            Rc::new(RefCell::new(CountingConnectionIdGenerator::default())),
            DEFAULT_ADDR,
            DEFAULT_ADDR,
            ConnectionParameters::default().versions(version, vec![version]),
            now(),
        )
        .unwrap();
        let mut server = Connection::new_server(
            test_fixture::DEFAULT_KEYS,
            test_fixture::DEFAULT_ALPN,
            Rc::new(RefCell::new(CountingConnectionIdGenerator::default())),
            ConnectionParameters::default().versions(version, vec![version]),
        )
        .unwrap();
        connect_force_idle(&mut client, &mut server);
        assert_eq!(client.version(), version);
        assert_eq!(server.version(), version);
    }

    for v in Version::all() {
        println!("Connecting with {v:?}");
        connect_v(v);
    }
}

#[test]
fn anti_amplification() {
    // This test has its own logic for generating large CRYPTO frames, so turn off MLKEM.
    let mut client = new_client(ConnectionParameters::default().mlkem(false));
    let mut server = default_server();
    let mut now = now();

    // With a gigantic transport parameter, the server is unable to complete
    // the handshake within the amplification limit.
    let very_big = TransportParameter::Bytes(vec![0; Pmtud::default_plpmtu(DEFAULT_ADDR.ip()) * 3]);
    server
        .set_local_tparam(TestTransportParameter, very_big)
        .unwrap();

    let c_init = client.process_output(now).dgram();
    now += DEFAULT_RTT / 2;
    let s_init1 = server.process(c_init, now).dgram().unwrap();
    assert_eq!(s_init1.len(), server.plpmtu());
    let s_init2 = server.process_output(now).dgram().unwrap();
    assert_eq!(s_init2.len(), server.plpmtu());
    let s_init3 = server.process_output(now).dgram().unwrap();
    assert_eq!(s_init3.len(), server.plpmtu());
    let cb = server.process_output(now).callback();

    // We are blocked by the amplification limit now.
    assert_eq!(cb, server.conn_params.get_idle_timeout());

    now += DEFAULT_RTT / 2;
    client.process_input(s_init1, now);
    client.process_input(s_init2, now);
    let ack_count = client.stats().frame_tx.ack;
    let frame_count = client.stats().frame_tx.all();
    let ack = client.process(Some(s_init3), now).dgram().unwrap();
    assert!(!maybe_authenticate(&mut client)); // No need yet.

    // The client sends a padded datagram, with just ACKs for Initial and Handshake.
    assert_eq!(client.stats().frame_tx.ack, ack_count + 2);
    assert_eq!(client.stats().frame_tx.all(), frame_count + 2);
    assert_ne!(ack.len(), client.plpmtu()); // Not padded (it includes Handshake).

    now += DEFAULT_RTT / 2;
    let remainder = server.process(Some(ack), now).dgram();

    now += DEFAULT_RTT / 2;
    client.process_input(remainder.unwrap(), now);
    assert!(maybe_authenticate(&mut client)); // OK, we have all of it.
    let fin = client.process_output(now).dgram();
    assert_eq!(*client.state(), State::Connected);

    now += DEFAULT_RTT / 2;
    server.process_input(fin.unwrap(), now);
    assert_eq!(*server.state(), State::Confirmed);
}

#[cfg(not(feature = "disable-encryption"))]
#[test]
fn garbage_initial() {
    let mut client = default_client();
    let mut server = default_server();

    let dgram = client.process_output(now()).dgram().unwrap();
    let (initial, rest) = split_datagram(&dgram);
    let mut corrupted = Vec::from(&initial[..initial.len() - 1]);
    corrupted.push(initial[initial.len() - 1] ^ 0xb7);
    corrupted.extend_from_slice(rest.as_ref().map_or(&[], |r| &r[..]));
    let garbage = datagram(corrupted);
    assert_eq!(Output::None, server.process(Some(garbage), now()));
}

#[test]
fn drop_initial_packet_from_wrong_address() {
    let mut client = default_client();
    let out = client.process_output(now());
    let out2 = client.process_output(now());
    assert!(out.as_dgram_ref().is_some() && out2.as_dgram_ref().is_some());

    let mut server = default_server();
    server.process_input(out.dgram().unwrap(), now());
    let out = server.process(out2.dgram(), now());
    assert!(out.as_dgram_ref().is_some());

    let p = out.dgram().unwrap();
    let dgram = Datagram::new(
        SocketAddr::new(IpAddr::V6(Ipv6Addr::new(0xfe80, 0, 0, 0, 0, 0, 0, 2)), 443),
        p.destination(),
        p.tos(),
        &p[..],
    );

    let out = client.process(Some(dgram), now());
    assert!(out.as_dgram_ref().is_none());
}

#[test]
fn drop_handshake_packet_from_wrong_address() {
    let mut client = default_client();
    let out = client.process_output(now());
    let out2 = client.process_output(now());
    assert!(out.as_dgram_ref().is_some() && out2.as_dgram_ref().is_some());

    let mut server = default_server();
    server.process_input(out.dgram().unwrap(), now());
    let out = server.process(out2.dgram(), now());
    assert!(out.as_dgram_ref().is_some());

    let out = client.process(out.dgram(), now());
    let out = server.process(out.dgram(), now());

    let (s_in, s_hs) = split_datagram(&out.dgram().unwrap());

    // Pass the initial packet.
    drop(client.process(Some(s_in), now()).dgram());

    let p = s_hs.unwrap();
    let dgram = Datagram::new(
        SocketAddr::new(IpAddr::V6(Ipv6Addr::new(0xfe80, 0, 0, 0, 0, 0, 0, 2)), 443),
        p.destination(),
        p.tos(),
        &p[..],
    );

    let out = client.process(Some(dgram), now());
    assert!(out.as_dgram_ref().is_none());
}

#[test]
fn ech() {
    let mut server = default_server();
    let (sk, pk) = generate_ech_keys().unwrap();
    server
        .server_enable_ech(ECH_CONFIG_ID, ECH_PUBLIC_NAME, &sk, &pk)
        .unwrap();

    let mut client = default_client();
    client.client_enable_ech(server.ech_config()).unwrap();

    connect(&mut client, &mut server);

    assert!(client.tls_info().unwrap().ech_accepted());
    assert!(server.tls_info().unwrap().ech_accepted());
    assert!(client.tls_preinfo().unwrap().ech_accepted().unwrap());
    assert!(server.tls_preinfo().unwrap().ech_accepted().unwrap());
}

fn damaged_ech_config(config: &[u8]) -> Vec<u8> {
    let mut cfg = Vec::from(config);
    // Ensure that the version and config_id is correct.
    assert_eq!(cfg[2], 0xfe);
    assert_eq!(cfg[3], 0x0d);
    assert_eq!(cfg[6], ECH_CONFIG_ID);
    // Change the config_id so that the server doesn't recognize it.
    cfg[6] ^= 0x94;
    cfg
}

#[test]
fn ech_retry() {
    fixture_init();
    let mut server = default_server();
    let (sk, pk) = generate_ech_keys().unwrap();
    server
        .server_enable_ech(ECH_CONFIG_ID, ECH_PUBLIC_NAME, &sk, &pk)
        .unwrap();

    let mut client = default_client();
    client
        .client_enable_ech(damaged_ech_config(server.ech_config()))
        .unwrap();

    let dgram = client.process_output(now()).dgram();
    let dgram2 = client.process_output(now()).dgram();
    server.process_input(dgram.unwrap(), now());
    let dgram = server.process(dgram2, now()).dgram();
    let dgram = client.process(dgram, now()).dgram();
    let dgram = server.process(dgram, now()).dgram();
    client.process_input(dgram.unwrap(), now());
    let auth_event = ConnectionEvent::EchFallbackAuthenticationNeeded {
        public_name: String::from(ECH_PUBLIC_NAME),
    };
    assert!(client.events().any(|e| e == auth_event));
    client.authenticated(AuthenticationStatus::Ok, now());
    assert!(client.state().error().is_some());

    // Tell the server about the error.
    let dgram = client.process_output(now()).dgram();
    server.process_input(dgram.unwrap(), now());
    assert_eq!(
        server.state().error(),
        Some(&CloseReason::Transport(Error::PeerError(0x100 + 121)))
    );

    let Some(CloseReason::Transport(Error::EchRetry(updated_config))) = client.state().error()
    else {
        panic!(
            "Client state should be failed with EchRetry, is {:?}",
            client.state()
        );
    };

    let mut server = default_server();
    server
        .server_enable_ech(ECH_CONFIG_ID, ECH_PUBLIC_NAME, &sk, &pk)
        .unwrap();
    let mut client = default_client();
    client.client_enable_ech(updated_config).unwrap();

    connect(&mut client, &mut server);

    assert!(client.tls_info().unwrap().ech_accepted());
    assert!(server.tls_info().unwrap().ech_accepted());
    assert!(client.tls_preinfo().unwrap().ech_accepted().unwrap());
    assert!(server.tls_preinfo().unwrap().ech_accepted().unwrap());
}

#[test]
fn ech_retry_fallback_rejected() {
    fixture_init();
    let mut server = default_server();
    let (sk, pk) = generate_ech_keys().unwrap();
    server
        .server_enable_ech(ECH_CONFIG_ID, ECH_PUBLIC_NAME, &sk, &pk)
        .unwrap();

    let mut client = default_client();
    client
        .client_enable_ech(damaged_ech_config(server.ech_config()))
        .unwrap();

    let dgram = client.process_output(now()).dgram();
    let dgram2 = client.process_output(now()).dgram();
    server.process_input(dgram.unwrap(), now());
    let dgram = server.process(dgram2, now()).dgram();
    let dgram = client.process(dgram, now()).dgram();
    let dgram = server.process(dgram, now()).dgram();
    client.process_input(dgram.unwrap(), now());
    let auth_event = ConnectionEvent::EchFallbackAuthenticationNeeded {
        public_name: String::from(ECH_PUBLIC_NAME),
    };
    assert!(client.events().any(|e| e == auth_event));
    client.authenticated(AuthenticationStatus::PolicyRejection, now());
    assert!(client.state().error().is_some());

    if let Some(CloseReason::Transport(Error::EchRetry(_))) = client.state().error() {
        panic!("Client should not get EchRetry error");
    }

    // Pass the error on.
    let dgram = client.process_output(now()).dgram();
    server.process_input(dgram.unwrap(), now());
    assert_eq!(
        server.state().error(),
        Some(&CloseReason::Transport(Error::PeerError(298)))
    ); // A bad_certificate alert.
}

#[test]
fn bad_min_ack_delay() {
    const EXPECTED_ERROR: CloseReason = CloseReason::Transport(Error::TransportParameterError);
    let mut server = default_server();
    let max_ad = u64::try_from(DEFAULT_LOCAL_ACK_DELAY.as_micros()).unwrap();
    server
        .set_local_tparam(MinAckDelay, TransportParameter::Integer(max_ad + 1))
        .unwrap();
    let mut client = default_client();

    let dgram = client.process_output(now()).dgram();
    let dgram2 = client.process_output(now()).dgram();
    server.process_input(dgram.unwrap(), now());
    let dgram = server.process(dgram2, now()).dgram();
    let dgram = client.process(dgram, now()).dgram();
    let dgram = server.process(dgram, now()).dgram();
    client.process_input(dgram.unwrap(), now());
    client.authenticated(AuthenticationStatus::Ok, now());
    assert_eq!(client.state().error(), Some(&EXPECTED_ERROR));
    let dgram = client.process_output(now()).dgram();

    server.process_input(dgram.unwrap(), now());
    assert_eq!(
        server.state().error(),
        Some(&CloseReason::Transport(Error::PeerError(
            Error::TransportParameterError.code()
        )))
    );
}

/// Ensure that the client probes correctly if it only receives Initial packets
/// from the server.
#[test]
fn only_server_initial() {
    let mut server = default_server();
    let mut client = default_client();
    let mut now = now();

    let client_dgram = client.process_output(now).dgram();
    let client_dgram2 = client.process_output(now).dgram();

    // Now fetch two flights of messages from the server.
    server.process_input(client_dgram.unwrap(), now);
    let dgram = server.process(client_dgram2, now).dgram();
    let dgram = client.process(dgram, now).dgram();
    let server_dgram1 = server.process(dgram, now).dgram();
    let server_dgram2 = server.process_output(now + AT_LEAST_PTO).dgram();

    // Only pass on the Initial from the first.  We should get a Handshake in return.
    let (initial, handshake) = split_datagram(&server_dgram1.unwrap());
    assert!(handshake.is_some());

    // The client sends an Initial ACK.
    assert_eq!(client.stats().frame_tx.ack, 1);
    let probe = client.process(Some(initial), now).dgram();
    assertions::assert_initial(&probe.unwrap(), false);
    assert_eq!(client.stats().dropped_rx, 0);
    assert_eq!(client.stats().frame_tx.ack, 2);

    let (initial, handshake) = split_datagram(&server_dgram2.unwrap());
    assert!(handshake.is_some());

    // The same happens after a PTO.
    now += AT_LEAST_PTO;
    assert_eq!(client.stats().frame_tx.ack, 2);
    let discarded = client.stats().dropped_rx;
    let probe = client.process(Some(initial), now).dgram();
    assertions::assert_initial(&probe.unwrap(), false);
    assert_eq!(client.stats().frame_tx.ack, 3);
    assert_eq!(client.stats().dropped_rx, discarded);

    // Pass the Handshake packet and complete the handshake.
    client.process_input(handshake.unwrap(), now);
    maybe_authenticate(&mut client);
    let dgram = client.process_output(now).dgram();
    let dgram = server.process(dgram, now).dgram();
    client.process_input(dgram.unwrap(), now);

    assert_eq!(*client.state(), State::Confirmed);
    assert_eq!(*server.state(), State::Confirmed);
}

// Collect a few spare Initial packets as the handshake is exchanged.
// Later, replay those packets to see if they result in additional probes; they should not.
#[test]
fn no_extra_probes_after_confirmed() {
    let mut server = default_server();
    let mut client = default_client();
    let mut now = now();

    // First, collect a client Initial.
    let spare_initial = client.process_output(now).dgram();
    let spare_initial2 = client.process_output(now).dgram();
    assert!(spare_initial.is_some() && spare_initial2.is_some());

    // Collect ANOTHER client Initial.
    now += AT_LEAST_PTO;
    let dgram1 = client.process_output(now).dgram();
    _ = client.process_output(now).dgram();
    let (replay_initial, _) = split_datagram(dgram1.as_ref().unwrap());

    // Finally, run the handshake.
    now += AT_LEAST_PTO * 2;
    let dgram = client.process_output(now).dgram();
    let dgram2 = client.process_output(now).dgram();
    server.process_input(dgram.unwrap(), now);
    let dgram = server.process(dgram2, now).dgram();

    // The server should have dropped the Initial keys now, so passing in the Initial
    // should elicit a retransmit rather than having it completely ignored.
    let spare_handshake = server.process(Some(replay_initial), now).dgram();
    assert!(spare_handshake.is_some());

    let dgram = client.process(dgram, now).dgram();
    let dgram = server.process(dgram, now).dgram();
    client.process_input(dgram.unwrap(), now);
    maybe_authenticate(&mut client);
    let dgram = client.process_output(now).dgram();
    let dgram = server.process(dgram, now).dgram();
    client.process_input(dgram.unwrap(), now);

    assert_eq!(*client.state(), State::Confirmed);
    assert_eq!(*server.state(), State::Confirmed);

    let probe = server.process(spare_initial, now).dgram();
    assert!(probe.is_none());
    let probe = client.process(spare_handshake, now).dgram();
    assert!(probe.is_none());
}

#[test]
fn implicit_rtt_server() {
    const RTT: Duration = Duration::from_secs(2);
    let mut server = default_server();
    let mut client = default_client();
    let mut now = now();

    let dgram = client.process_output(now).dgram();
    let dgram2 = client.process_output(now).dgram();
    now += RTT / 2;
    server.process_input(dgram.unwrap(), now);
    let dgram = server.process(dgram2, now).dgram();
    now += RTT / 2;
    let dgram = client.process(dgram, now).dgram();
    now += RTT / 2;
    let dgram = server.process(dgram, now).dgram();
    now += RTT / 2;
    let dgram = client.process(dgram, now).dgram();
    let (initial, handshake) = split_datagram(dgram.as_ref().unwrap());
    assertions::assert_initial(&initial, false);
    assertions::assert_handshake(handshake.as_ref().unwrap());
    now += RTT / 2;
    server.process_input(dgram.unwrap(), now);

    // The server doesn't receive any acknowledgments, but it can infer
    // an RTT estimate from having discarded the Initial packet number space.
    assert_eq!(server.stats().rtt, RTT);
}

#[test]
fn emit_authentication_needed_once() {
    let mut client = default_client();

    let mut server = Connection::new_server(
        test_fixture::LONG_CERT_KEYS,
        test_fixture::DEFAULT_ALPN,
        Rc::new(RefCell::new(CountingConnectionIdGenerator::default())),
        // TODO: Why is this needed to avoind the 5ms pacing delay?
        ConnectionParameters::default().pacing(false),
    )
    .expect("create a server");

    let client1 = client.process_output(now());
    let client2 = client.process_output(now());
    assert!(client1.as_dgram_ref().is_some() && client2.as_dgram_ref().is_some());

    // The entire server flight doesn't fit in a single packet because the
    // certificate is large, therefore the server will produce 2 packets.
    _ = server.process(client1.dgram(), now());
    let server1 = server.process(client2.dgram(), now());
    assert!(server1.as_dgram_ref().is_some());
    let server2 = server.process_output(now());
    assert!(server2.as_dgram_ref().is_some());
    let server3 = server.process_output(now());
    assert!(server3.as_dgram_ref().is_some());

    let authentication_needed_count = |client: &mut Connection| {
        client
            .events()
            .filter(|e| matches!(e, ConnectionEvent::AuthenticationNeeded))
            .count()
    };

    // Upon receiving the first two packet, the client has the server certificate,
    // but not yet all required handshake data. It moves to
    // `HandshakeState::AuthenticationPending` and emits a
    // `ConnectionEvent::AuthenticationNeeded` event.
    //
    // Note that this is a tiny bit fragile in that it depends on having a certificate
    // that is within a fairly narrow range of sizes.  It has to fit in a single
    // packet, but be large enough that the CertificateVerify message does not
    // also fit in the same packet.  Our default test setup achieves this, but
    // changes to the setup might invalidate this test.
    _ = client.process(server1.dgram(), now());
    _ = client.process(server2.dgram(), now());
    assert_eq!(1, authentication_needed_count(&mut client));
    assert!(client.peer_certificate().is_some());

    // The `AuthenticationNeeded` event is still pending a call to
    // `Connection::authenticated`. On receiving the second packet from the
    // server, the client must not emit a another
    // `ConnectionEvent::AuthenticationNeeded`.
    _ = client.process(server3.dgram(), now());
    assert_eq!(0, authentication_needed_count(&mut client));
}

#[test]
fn client_initial_retransmits_identical() {
    let mut now = now();
    // TODO: With pacing on, why does the delay callback return by 5ms and then PTO after 295ms?
    let mut client = new_client(ConnectionParameters::default().pacing(false));

    // Force the client to retransmit its Initial flight a number of times and make sure the
    // retranmissions are identical to the original. Also, verify the PTO durations.
    for i in 1..=5 {
        let ci = client.process_output(now).dgram().unwrap();
        assert_eq!(ci.len(), client.plpmtu());
        let ci2 = client.process_output(now).dgram().unwrap();
        assert_eq!(ci2.len(), client.plpmtu());
        assert_eq!(
            client.stats().frame_tx,
            FrameStats {
                crypto: 3 * i,
                ..Default::default()
            }
        );
        let pto = client.process_output(now).callback();
        assert_eq!(pto, DEFAULT_RTT * 3 * (1 << (i - 1)));
        now += pto;
    }
}

#[test]
fn server_initial_retransmits_identical() {
    let mut now = now();
    let mut client = default_client();
    let mut ci = client.process_output(now).dgram();
    let mut ci2 = client.process_output(now).dgram();

    // Force the server to retransmit its Initial flight a number of times and make sure the
    // retranmissions are identical to the original. Also, verify the PTO durations.
    let mut server = new_server(ConnectionParameters::default().pacing(false));
    let mut total_ptos = Duration::from_secs(0);
    for i in 1..=3 {
        _ = server.process(ci.take(), now);
        _ = server.process(ci2.take(), now);
        if i == 1 {
            // On the first iteration, the server will want to send its entire flight.
            // During later ones, we will have hit a PTO and can hence only send two packets.
            _ = server.process(ci2.take(), now);
        }
        assert_eq!(
            server.stats().frame_tx,
            FrameStats {
                crypto: i * 3,
                ack: i * 2 - i.saturating_sub(1),
                largest_acknowledged: (i - i.saturating_sub(1)) as u64,
                ..Default::default()
            }
        );

        let pto = server.process_output(now).callback();
        now += pto;
        total_ptos += pto;
    }

    // Server is amplification-limited now.
    let pto = server.process_output(now).callback();
    assert_eq!(pto, server.conn_params.get_idle_timeout() - total_ptos);
}

#[test]
fn grease_quic_bit_transport_parameter() {
    fn get_remote_tp(conn: &Connection) -> bool {
        conn.tps
            .borrow()
            .remote_handshake()
            .as_ref()
            .unwrap()
            .get_empty(GreaseQuicBit)
    }

    for client_grease in [true, false] {
        for server_grease in [true, false] {
            let mut client = new_client(ConnectionParameters::default().grease(client_grease));
            let mut server = new_server(ConnectionParameters::default().grease(server_grease));

            connect(&mut client, &mut server);

            assert_eq!(client_grease, get_remote_tp(&server));
            assert_eq!(server_grease, get_remote_tp(&client));
        }
    }
}

#[test]
fn zero_rtt_with_ech() {
    let mut server = default_server();
    let (sk, pk) = generate_ech_keys().unwrap();
    server
        .server_enable_ech(ECH_CONFIG_ID, ECH_PUBLIC_NAME, &sk, &pk)
        .unwrap();

    let mut client = default_client();
    client.client_enable_ech(server.ech_config()).unwrap();

    connect(&mut client, &mut server);

    assert!(client.tls_info().unwrap().ech_accepted());
    assert!(server.tls_info().unwrap().ech_accepted());

    let token = exchange_ticket(&mut client, &mut server, now());
    let mut client = default_client();
    client.client_enable_ech(server.ech_config()).unwrap();
    client
        .enable_resumption(now(), token)
        .expect("should set token");

    let mut server = resumed_server(&client);
    server
        .server_enable_ech(ECH_CONFIG_ID, ECH_PUBLIC_NAME, &sk, &pk)
        .unwrap();

    connect(&mut client, &mut server);
    assert!(client.tls_info().unwrap().ech_accepted());
    assert!(server.tls_info().unwrap().ech_accepted());
    assert!(client.tls_info().unwrap().early_data_accepted());
    assert!(server.tls_info().unwrap().early_data_accepted());
}
