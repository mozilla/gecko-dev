// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::time::Duration;

use neqo_common::{event::Provider as _, Decoder, Encoder, IpTosDscp};
use test_fixture::{assertions, datagram, now};

use super::{
    super::{CloseReason, ConnectionEvent, Output, State, ZeroRttState},
    connect, connect_fail, default_client, default_server, exchange_ticket, new_client, new_server,
    send_something,
};
use crate::{
    packet::PACKET_BIT_LONG,
    tparams::{TransportParameter, TransportParameterId::*},
    ConnectionParameters, Error, Stats, Version, MIN_INITIAL_PACKET_SIZE,
};

// The expected PTO duration after the first Initial is sent.
const INITIAL_PTO: Duration = Duration::from_millis(300);

/// # Panics
///
/// When the count of received packets doesn't match the count of received packets with the
/// (default) DSCP.
pub fn assert_dscp(stats: &Stats) {
    assert_eq!(stats.dscp_rx[IpTosDscp::Cs0], stats.packets_rx);
}

#[test]
fn unknown_version() {
    let mut client = default_client();
    // Start the handshake.
    drop(client.process_output(now()).dgram());

    let mut unknown_version_packet = vec![0x80, 0x1a, 0x1a, 0x1a, 0x1a];
    unknown_version_packet.resize(MIN_INITIAL_PACKET_SIZE, 0x0);
    drop(client.process(Some(datagram(unknown_version_packet)), now()));
    assert_eq!(1, client.stats().dropped_rx);
    assert_dscp(&client.stats());
}

#[test]
fn server_receive_unknown_first_packet() {
    let mut server = default_server();

    let mut unknown_version_packet = vec![0x80, 0x1a, 0x1a, 0x1a, 0x1a];
    unknown_version_packet.resize(MIN_INITIAL_PACKET_SIZE, 0x0);

    assert_eq!(
        server.process(Some(datagram(unknown_version_packet)), now()),
        Output::None
    );

    assert_eq!(1, server.stats().dropped_rx);
    assert_dscp(&server.stats());
}

fn create_vn(initial_pkt: &[u8], versions: &[u32]) -> Vec<u8> {
    let mut dec = Decoder::from(&initial_pkt[5..]); // Skip past version.
    let dst_cid = dec.decode_vec(1).expect("client DCID");
    let src_cid = dec.decode_vec(1).expect("client SCID");

    let mut encoder = Encoder::default();
    encoder.encode_byte(PACKET_BIT_LONG);
    encoder.encode(&[0; 4]); // Zero version == VN.
    encoder.encode_vec(1, src_cid);
    encoder.encode_vec(1, dst_cid);

    for v in versions {
        encoder.encode_uint(4, *v);
    }
    encoder.into()
}

#[test]
fn version_negotiation_current_version() {
    let mut client = default_client();
    // Start the handshake.
    let initial_pkt = client
        .process_output(now())
        .dgram()
        .expect("a datagram")
        .to_vec();
    _ = client.process_output(now()).dgram().expect("a datagram");

    let vn = create_vn(
        &initial_pkt,
        &[0x1a1a_1a1a, Version::default().wire_version()],
    );

    let dgram = datagram(vn);
    let mut delay = client.process(Some(dgram), now()).callback();
    delay += client.process_output(now() + delay).callback(); // TODO: Why is there first a 5ms pacing delay before the PTO?
    assert_eq!(delay, INITIAL_PTO);
    assert_eq!(*client.state(), State::WaitInitial);
    assert_eq!(1, client.stats().dropped_rx);
    assert_dscp(&client.stats());
}

#[test]
fn version_negotiation_version0() {
    let mut client = default_client();
    // Start the handshake.
    let initial_pkt = client
        .process_output(now())
        .dgram()
        .expect("a datagram")
        .to_vec();
    _ = client.process_output(now()).dgram().expect("a datagram");

    let vn = create_vn(&initial_pkt, &[0, 0x1a1a_1a1a]);

    let dgram = datagram(vn);
    let mut delay = client.process(Some(dgram), now()).callback();
    delay += client.process_output(now() + delay).callback(); // TODO: Why is there first a 5ms pacing delay before the PTO?
    assert_eq!(delay, INITIAL_PTO);
    assert_eq!(*client.state(), State::WaitInitial);
    assert_eq!(1, client.stats().dropped_rx);
    assert_dscp(&client.stats());
}

#[test]
fn version_negotiation_only_reserved() {
    let mut client = default_client();
    // Start the handshake.
    let initial_pkt = client
        .process_output(now())
        .dgram()
        .expect("a datagram")
        .to_vec();

    let vn = create_vn(&initial_pkt, &[0x1a1a_1a1a, 0x2a2a_2a2a]);

    let dgram = datagram(vn);
    assert_eq!(client.process(Some(dgram), now()), Output::None);
    match client.state() {
        State::Closed(err) => {
            assert_eq!(*err, CloseReason::Transport(Error::VersionNegotiation));
        }
        _ => panic!("Invalid client state"),
    }
    assert_dscp(&client.stats());
}

#[test]
fn version_negotiation_corrupted() {
    let mut client = default_client();
    // Start the handshake.
    let initial_pkt = client
        .process_output(now())
        .dgram()
        .expect("a datagram")
        .to_vec();
    _ = client.process_output(now()).dgram().expect("a datagram");

    let vn = create_vn(&initial_pkt, &[0x1a1a_1a1a, 0x2a2a_2a2a]);

    let dgram = datagram(vn[..vn.len() - 1].to_vec());
    let mut delay = client.process(Some(dgram), now()).callback();
    delay += client.process_output(now() + delay).callback(); // TODO: Why is there first a 5ms pacing delay before the PTO?
    assert_eq!(delay, INITIAL_PTO);
    assert_eq!(*client.state(), State::WaitInitial);
    assert_eq!(1, client.stats().dropped_rx);
    assert_dscp(&client.stats());
}

#[test]
fn version_negotiation_empty() {
    let mut client = default_client();
    // Start the handshake.
    let initial_pkt = client
        .process_output(now())
        .dgram()
        .expect("a datagram")
        .to_vec();
    _ = client.process_output(now()).dgram().expect("a datagram");

    let vn = create_vn(&initial_pkt, &[]);

    let dgram = datagram(vn);
    let mut delay = client.process(Some(dgram), now()).callback();
    delay += client.process_output(now() + delay).callback(); // TODO: Why is there first a 5ms pacing delay before the PTO?
    assert_eq!(delay, INITIAL_PTO);
    assert_eq!(*client.state(), State::WaitInitial);
    assert_eq!(1, client.stats().dropped_rx);
    assert_dscp(&client.stats());
}

#[test]
fn version_negotiation_not_supported() {
    let mut client = default_client();
    // Start the handshake.
    let initial_pkt = client
        .process_output(now())
        .dgram()
        .expect("a datagram")
        .to_vec();

    let vn = create_vn(&initial_pkt, &[0x1a1a_1a1a, 0x2a2a_2a2a, 0xff00_0001]);
    let dgram = datagram(vn);
    assert_eq!(client.process(Some(dgram), now()), Output::None);
    match client.state() {
        State::Closed(err) => {
            assert_eq!(*err, CloseReason::Transport(Error::VersionNegotiation));
        }
        _ => panic!("Invalid client state"),
    }
    assert_dscp(&client.stats());
}

#[test]
fn version_negotiation_bad_cid() {
    let mut client = default_client();
    // Start the handshake.
    let mut initial_pkt = client
        .process_output(now())
        .dgram()
        .expect("a datagram")
        .to_vec();
    _ = client.process_output(now()).dgram().expect("a datagram");

    initial_pkt[6] ^= 0xc4;
    let vn = create_vn(&initial_pkt, &[0x1a1a_1a1a, 0x2a2a_2a2a, 0xff00_0001]);

    let dgram = datagram(vn);
    let mut delay = client.process(Some(dgram), now()).callback();
    delay += client.process_output(now() + delay).callback(); // TODO: Why is there first a 5ms pacing delay before the PTO?
    assert_eq!(delay, INITIAL_PTO);
    assert_eq!(*client.state(), State::WaitInitial);
    assert_eq!(1, client.stats().dropped_rx);
    assert_dscp(&client.stats());
}

#[test]
fn compatible_upgrade() {
    let mut client = default_client();
    let mut server = default_server();

    connect(&mut client, &mut server);
    assert_eq!(client.version(), Version::Version2);
    assert_eq!(server.version(), Version::Version2);
    assert_dscp(&client.stats());
    assert_dscp(&server.stats());
}

/// When the first packet from the client is gigantic, the server might generate acknowledgment
/// packets in version 1.  Both client and server need to handle that gracefully.
#[test]
fn compatible_upgrade_large_initial() {
    let params = ConnectionParameters::default().versions(
        Version::Version1,
        vec![Version::Version2, Version::Version1],
    );
    let mut client = new_client(params.clone());
    client
        .set_local_tparam(
            TestTransportParameter,
            TransportParameter::Bytes(vec![0; 2048]),
        )
        .unwrap();
    let mut server = new_server(params);

    // Client Initial should take 2 packets.
    // Each should elicit a Version 1 ACK from the server.
    let dgram = client.process_output(now()).dgram();
    let dgram2 = client.process_output(now()).dgram();
    assert!(dgram.is_some() && dgram2.is_some());
    server.process_input(dgram.unwrap(), now());
    let dgram = server.process(dgram2, now()).dgram();
    assert!(dgram.is_some());
    // The following uses the Version from *outside* this crate.
    assertions::assert_version(dgram.as_ref().unwrap(), Version::Version1.wire_version());
    client.process_input(dgram.unwrap(), now());

    connect(&mut client, &mut server);
    assert_eq!(client.version(), Version::Version2);
    assert_eq!(server.version(), Version::Version2);
    // Only handshake padding is "dropped".
    assert_eq!(client.stats().dropped_rx, 1);
    assert!(matches!(server.stats().dropped_rx, 2 | 3));
    assert_dscp(&client.stats());
    assert!(
        server.stats().dscp_rx[IpTosDscp::Cs0] == server.stats().packets_rx
            || server.stats().dscp_rx[IpTosDscp::Cs0] == server.stats().packets_rx + 1
            || server.stats().dscp_rx[IpTosDscp::Cs0] == server.stats().packets_rx - 1,
        "dscp_rx[IpTosDscp::Cs0] {} != packets_rx {} (possibly +/- 1)",
        server.stats().dscp_rx[IpTosDscp::Cs0],
        server.stats().packets_rx
    );
}

/// A server that supports versions 1 and 2 might prefer version 1 and that's OK.
/// This one starts with version 1 and stays there.
#[test]
fn compatible_no_upgrade() {
    let mut client = new_client(ConnectionParameters::default().versions(
        Version::Version1,
        vec![Version::Version2, Version::Version1],
    ));
    let mut server = new_server(ConnectionParameters::default().versions(
        Version::Version1,
        vec![Version::Version1, Version::Version2],
    ));

    connect(&mut client, &mut server);
    assert_eq!(client.version(), Version::Version1);
    assert_eq!(server.version(), Version::Version1);
}

/// A server that supports versions 1 and 2 might prefer version 1 and that's OK.
/// This one starts with version 2 and downgrades to version 1.
#[test]
fn compatible_downgrade() {
    let mut client = new_client(ConnectionParameters::default().versions(
        Version::Version2,
        vec![Version::Version2, Version::Version1],
    ));
    let mut server = new_server(ConnectionParameters::default().versions(
        Version::Version2,
        vec![Version::Version1, Version::Version2],
    ));

    connect(&mut client, &mut server);
    assert_eq!(client.version(), Version::Version1);
    assert_eq!(server.version(), Version::Version1);
}

/// Inject a Version Negotiation packet, which the client detects when it validates the
/// server `version_negotiation` transport parameter.
#[test]
fn version_negotiation_downgrade() {
    const DOWNGRADE: Version = Version::Draft29;

    let mut client = default_client();
    // The server sets the current version in the transport parameter and
    // protects Initial packets with the version in its configuration.
    // When a server `Connection` is created by a `Server`, the configuration is set
    // to match the version of the packet it first receives.  This replicates that.
    let mut server =
        new_server(ConnectionParameters::default().versions(DOWNGRADE, Version::all()));

    // Start the handshake and spoof a VN packet.
    let initial = client.process_output(now()).dgram().unwrap();
    let vn = create_vn(&initial, &[DOWNGRADE.wire_version()]);
    let dgram = datagram(vn);
    client.process_input(dgram, now());

    connect_fail(
        &mut client,
        &mut server,
        Error::VersionNegotiation,
        Error::PeerError(Error::VersionNegotiation.code()),
    );
}

/// A server connection needs to be configured with the version that the client attempts.
/// Otherwise, it will object to the client transport parameters and not do anything.
#[test]
fn invalid_server_version() {
    let mut client =
        new_client(ConnectionParameters::default().versions(Version::Version1, Version::all()));
    let mut server =
        new_server(ConnectionParameters::default().versions(Version::Version2, Version::all()));

    let dgram = client.process_output(now()).dgram();
    let dgram2 = client.process_output(now()).dgram();
    server.process_input(dgram.unwrap(), now());
    server.process_input(dgram2.unwrap(), now());

    // Three packets received (one is zero padding).
    assert_eq!(server.stats().packets_rx, 3);
    // One dropped (the zero padding).
    assert_eq!(server.stats().dropped_rx, 1);
    assert_eq!(server.stats().saved_datagrams, 0);
    // The server effectively hasn't reacted here.
    match server.state() {
        State::Closed(err) => {
            assert_eq!(*err, CloseReason::Transport(Error::CryptoAlert(47)));
        }
        _ => panic!("invalid server state"),
    }
}

#[test]
fn invalid_current_version_client() {
    const OTHER_VERSION: Version = Version::Draft29;

    let mut client = default_client();
    let mut server = default_server();

    assert_ne!(OTHER_VERSION, client.version());
    client
        .set_local_tparam(
            VersionInformation,
            TransportParameter::Versions {
                current: OTHER_VERSION.wire_version(),
                other: Version::all()
                    .iter()
                    .copied()
                    .map(Version::wire_version)
                    .collect(),
            },
        )
        .unwrap();

    connect_fail(
        &mut client,
        &mut server,
        Error::PeerError(Error::CryptoAlert(47).code()),
        Error::CryptoAlert(47),
    );
}

/// To test this, we need to disable compatible upgrade so that the server doesn't update
/// its transport parameters.  Then, we can overwrite its transport parameters without
/// them being overwritten.  Otherwise, it would be hard to find a window during which
/// the transport parameter can be modified.
#[test]
fn invalid_current_version_server() {
    const OTHER_VERSION: Version = Version::Draft29;

    let mut client = default_client();
    let mut server = new_server(
        ConnectionParameters::default().versions(Version::default(), vec![Version::default()]),
    );

    assert!(!Version::default().is_compatible(OTHER_VERSION));
    server
        .set_local_tparam(
            VersionInformation,
            TransportParameter::Versions {
                current: OTHER_VERSION.wire_version(),
                other: vec![OTHER_VERSION.wire_version()],
            },
        )
        .unwrap();

    connect_fail(
        &mut client,
        &mut server,
        Error::CryptoAlert(47),
        Error::PeerError(Error::CryptoAlert(47).code()),
    );
}

#[test]
fn no_compatible_version() {
    const OTHER_VERSION: Version = Version::Draft29;

    let mut client = default_client();
    let mut server = default_server();

    assert_ne!(OTHER_VERSION, client.version());
    client
        .set_local_tparam(
            VersionInformation,
            TransportParameter::Versions {
                current: Version::default().wire_version(),
                other: vec![OTHER_VERSION.wire_version()],
            },
        )
        .unwrap();

    connect_fail(
        &mut client,
        &mut server,
        Error::PeerError(Error::CryptoAlert(47).code()),
        Error::CryptoAlert(47),
    );
}

/// When a compatible upgrade chooses a different version, 0-RTT is rejected.
#[test]
fn compatible_upgrade_0rtt_rejected() {
    // This is the baseline configuration where v1 is attempted and v2 preferred.
    let prefer_v2 = ConnectionParameters::default().versions(
        Version::Version1,
        vec![Version::Version2, Version::Version1],
    );
    let mut client = new_client(prefer_v2.clone());
    // The server will start with this so that the client resumes with v1.
    let just_v1 =
        ConnectionParameters::default().versions(Version::Version1, vec![Version::Version1]);
    let mut server = new_server(just_v1);

    connect(&mut client, &mut server);
    assert_eq!(client.version(), Version::Version1);
    let token = exchange_ticket(&mut client, &mut server, now());

    // Now upgrade the server to the preferred configuration.
    let mut client = new_client(prefer_v2.clone());
    let mut server = new_server(prefer_v2);
    client.enable_resumption(now(), token).unwrap();

    // Create a packet with 0-RTT from the client.
    let initial = send_something(&mut client, now());
    let initial2 = send_something(&mut client, now());
    assertions::assert_version(&initial, Version::Version1.wire_version());
    assertions::assert_coalesced_0rtt(&initial2);
    server.process_input(initial, now());
    server.process_input(initial2, now());
    assert!(!server
        .events()
        .any(|e| matches!(e, ConnectionEvent::NewStream { .. })));

    // Finalize the connection.  Don't use connect() because it uses
    // maybe_authenticate() too liberally and that eats the events we want to check.
    let dgram = server.process_output(now()).dgram(); // ServerHello flight
    let dgram = client.process(dgram, now()).dgram();
    let dgram = server.process(dgram, now()).dgram();
    let dgram = client.process(dgram, now()).dgram(); // Client Finished (note: no authentication)
    let dgram = server.process(dgram, now()).dgram(); // HANDSHAKE_DONE
    client.process_input(dgram.unwrap(), now());

    assert!(matches!(client.state(), State::Confirmed));
    assert!(matches!(server.state(), State::Confirmed));

    assert!(client.events().any(|e| {
        println!(" client event: {e:?}");
        matches!(e, ConnectionEvent::ZeroRttRejected)
    }));
    assert_eq!(client.zero_rtt_state(), ZeroRttState::Rejected);
}
