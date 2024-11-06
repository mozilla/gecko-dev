// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

// This file implements a server that can handle multiple connections.

use std::{
    cell::RefCell,
    cmp::min,
    collections::HashSet,
    ops::{Deref, DerefMut},
    path::PathBuf,
    rc::Rc,
    time::Instant,
};

use neqo_common::{
    event::Provider, hex, qdebug, qerror, qinfo, qlog::NeqoQlog, qtrace, qwarn, Datagram, Role,
};
use neqo_crypto::{
    encode_ech_config, AntiReplay, Cipher, PrivateKey, PublicKey, ZeroRttCheckResult,
    ZeroRttChecker,
};

pub use crate::addr_valid::ValidateAddress;
use crate::{
    addr_valid::{AddressValidation, AddressValidationResult},
    cid::{ConnectionId, ConnectionIdGenerator, ConnectionIdRef},
    connection::{Connection, Output, State},
    packet::{PacketBuilder, PacketType, PublicPacket, MIN_INITIAL_PACKET_SIZE},
    ConnectionParameters, Res, Version,
};

/// A `ServerZeroRttChecker` is a simple wrapper around a single checker.
/// It uses `RefCell` so that the wrapped checker can be shared between
/// multiple connections created by the server.
#[derive(Clone, Debug)]
struct ServerZeroRttChecker {
    checker: Rc<RefCell<Box<dyn ZeroRttChecker>>>,
}

impl ServerZeroRttChecker {
    pub fn new(checker: Box<dyn ZeroRttChecker>) -> Self {
        Self {
            checker: Rc::new(RefCell::new(checker)),
        }
    }
}

impl ZeroRttChecker for ServerZeroRttChecker {
    fn check(&self, token: &[u8]) -> ZeroRttCheckResult {
        self.checker.borrow().check(token)
    }
}

/// `InitialDetails` holds important information for processing `Initial` packets.
struct InitialDetails {
    src_cid: ConnectionId,
    dst_cid: ConnectionId,
    token: Vec<u8>,
    version: Version,
}

impl InitialDetails {
    fn new(packet: &PublicPacket) -> Self {
        Self {
            src_cid: ConnectionId::from(packet.scid()),
            dst_cid: ConnectionId::from(packet.dcid()),
            token: packet.token().to_vec(),
            version: packet.version().unwrap(),
        }
    }
}

struct EchConfig {
    config: u8,
    public_name: String,
    sk: PrivateKey,
    pk: PublicKey,
    encoded: Vec<u8>,
}

impl EchConfig {
    fn new(config: u8, public_name: &str, sk: &PrivateKey, pk: &PublicKey) -> Res<Self> {
        let encoded = encode_ech_config(config, public_name, pk)?;
        Ok(Self {
            config,
            public_name: String::from(public_name),
            sk: sk.clone(),
            pk: pk.clone(),
            encoded,
        })
    }
}

pub struct Server {
    /// The names of certificates.
    certs: Vec<String>,
    /// The ALPN values that the server supports.
    protocols: Vec<String>,
    /// The cipher suites that the server supports.
    ciphers: Vec<Cipher>,
    /// Anti-replay configuration for 0-RTT.
    anti_replay: AntiReplay,
    /// A function for determining if 0-RTT can be accepted.
    zero_rtt_checker: ServerZeroRttChecker,
    /// A connection ID generator.
    cid_generator: Rc<RefCell<dyn ConnectionIdGenerator>>,
    /// Connection parameters.
    conn_params: ConnectionParameters,
    /// All connections.
    connections: Vec<Rc<RefCell<Connection>>>,
    /// Address validation logic, which determines whether we send a Retry.
    address_validation: Rc<RefCell<AddressValidation>>,
    /// Directory to create qlog traces in
    qlog_dir: Option<PathBuf>,
    /// Encrypted client hello (ECH) configuration.
    ech_config: Option<EchConfig>,
}

impl Server {
    /// Construct a new server.
    /// * `now` is the time that the server is instantiated.
    /// * `certs` is a list of the certificates that should be configured.
    /// * `protocols` is the preference list of ALPN values.
    /// * `anti_replay` is an anti-replay context.
    /// * `zero_rtt_checker` determines whether 0-RTT should be accepted. This will be passed the
    ///   value of the `extra` argument that was passed to `Connection::send_ticket` to see if it is
    ///   OK.
    /// * `cid_generator` is responsible for generating connection IDs and parsing them; connection
    ///   IDs produced by the manager cannot be zero-length.
    /// # Errors
    /// When address validation state cannot be created.
    pub fn new(
        now: Instant,
        certs: &[impl AsRef<str>],
        protocols: &[impl AsRef<str>],
        anti_replay: AntiReplay,
        zero_rtt_checker: Box<dyn ZeroRttChecker>,
        cid_generator: Rc<RefCell<dyn ConnectionIdGenerator>>,
        conn_params: ConnectionParameters,
    ) -> Res<Self> {
        let validation = AddressValidation::new(now, ValidateAddress::Never)?;
        Ok(Self {
            certs: certs.iter().map(|x| String::from(x.as_ref())).collect(),
            protocols: protocols.iter().map(|x| String::from(x.as_ref())).collect(),
            ciphers: Vec::new(),
            anti_replay,
            zero_rtt_checker: ServerZeroRttChecker::new(zero_rtt_checker),
            cid_generator,
            conn_params,
            connections: Vec::new(),
            address_validation: Rc::new(RefCell::new(validation)),
            qlog_dir: None,
            ech_config: None,
        })
    }

    /// Set or clear directory to create logs of connection events in QLOG format.
    pub fn set_qlog_dir(&mut self, dir: Option<PathBuf>) {
        self.qlog_dir = dir;
    }

    /// Set the policy for address validation.
    pub fn set_validation(&self, v: ValidateAddress) {
        self.address_validation.borrow_mut().set_validation(v);
    }

    /// Set the cipher suites that should be used.  Set an empty value to use
    /// default values.
    pub fn set_ciphers(&mut self, ciphers: impl AsRef<[Cipher]>) {
        self.ciphers = Vec::from(ciphers.as_ref());
    }

    /// # Errors
    /// When the configuration is invalid.
    pub fn enable_ech(
        &mut self,
        config: u8,
        public_name: &str,
        sk: &PrivateKey,
        pk: &PublicKey,
    ) -> Res<()> {
        self.ech_config = Some(EchConfig::new(config, public_name, sk, pk)?);
        Ok(())
    }

    #[must_use]
    pub fn ech_config(&self) -> &[u8] {
        self.ech_config.as_ref().map_or(&[], |cfg| &cfg.encoded)
    }

    fn handle_initial(
        &mut self,
        initial: InitialDetails,
        dgram: Datagram<impl AsRef<[u8]>>,
        now: Instant,
    ) -> Output {
        qdebug!([self], "Handle initial");
        let res = self
            .address_validation
            .borrow()
            .validate(&initial.token, dgram.source(), now);
        match res {
            AddressValidationResult::Invalid => Output::None,
            AddressValidationResult::Pass => self.accept_connection(initial, dgram, None, now),
            AddressValidationResult::ValidRetry(orig_dcid) => {
                self.accept_connection(initial, dgram, Some(orig_dcid), now)
            }
            AddressValidationResult::Validate => {
                qinfo!([self], "Send retry for {:?}", initial.dst_cid);

                let res = self.address_validation.borrow().generate_retry_token(
                    &initial.dst_cid,
                    dgram.source(),
                    now,
                );
                let Ok(token) = res else {
                    qerror!([self], "unable to generate token, dropping packet");
                    return Output::None;
                };
                if let Some(new_dcid) = self.cid_generator.borrow_mut().generate_cid() {
                    let packet = PacketBuilder::retry(
                        initial.version,
                        &initial.src_cid,
                        &new_dcid,
                        &token,
                        &initial.dst_cid,
                    );
                    packet.map_or_else(
                        |_| {
                            qerror!([self], "unable to encode retry, dropping packet");
                            Output::None
                        },
                        |p| {
                            Output::Datagram(Datagram::new(
                                dgram.destination(),
                                dgram.source(),
                                dgram.tos(),
                                p,
                            ))
                        },
                    )
                } else {
                    qerror!([self], "no connection ID for retry, dropping packet");
                    Output::None
                }
            }
        }
    }

    fn create_qlog_trace(&self, odcid: ConnectionIdRef<'_>) -> NeqoQlog {
        self.qlog_dir
            .as_ref()
            .map_or_else(NeqoQlog::disabled, |qlog_dir| {
                NeqoQlog::enabled_with_file(
                    qlog_dir.clone(),
                    Role::Server,
                    Some("Neqo server qlog".to_string()),
                    Some("Neqo server qlog".to_string()),
                    odcid,
                )
                .unwrap_or_else(|e| {
                    qerror!("failed to create NeqoQlog: {}", e);
                    NeqoQlog::disabled()
                })
            })
    }

    fn setup_connection(
        &self,
        c: &mut Connection,
        initial: InitialDetails,
        orig_dcid: Option<ConnectionId>,
    ) {
        let zcheck = self.zero_rtt_checker.clone();
        if c.server_enable_0rtt(&self.anti_replay, zcheck).is_err() {
            qwarn!([self], "Unable to enable 0-RTT");
        }
        if let Some(odcid) = &orig_dcid {
            // There was a retry, so set the connection IDs for.
            c.set_retry_cids(odcid, initial.src_cid, &initial.dst_cid);
        }
        c.set_validation(&self.address_validation);
        c.set_qlog(self.create_qlog_trace(orig_dcid.unwrap_or(initial.dst_cid).as_cid_ref()));
        if let Some(cfg) = &self.ech_config {
            if c.server_enable_ech(cfg.config, &cfg.public_name, &cfg.sk, &cfg.pk)
                .is_err()
            {
                qwarn!([self], "Unable to enable ECH");
            }
        }
    }

    fn accept_connection(
        &mut self,
        initial: InitialDetails,
        dgram: Datagram<impl AsRef<[u8]>>,
        orig_dcid: Option<ConnectionId>,
        now: Instant,
    ) -> Output {
        qinfo!(
            [self],
            "Accept connection {:?}",
            orig_dcid.as_ref().unwrap_or(&initial.dst_cid)
        );
        // The internal connection ID manager that we use is not used directly.
        // Instead, wrap it so that we can save connection IDs.

        let mut params = self.conn_params.clone();
        params.get_versions_mut().set_initial(initial.version);
        let sconn = Connection::new_server(
            &self.certs,
            &self.protocols,
            Rc::clone(&self.cid_generator),
            params,
        );

        match sconn {
            Ok(mut c) => {
                self.setup_connection(&mut c, initial, orig_dcid);
                let out = c.process(Some(dgram), now);
                self.connections.push(Rc::new(RefCell::new(c)));
                out
            }
            Err(e) => {
                qwarn!([self], "Unable to create connection");
                if e == crate::Error::VersionNegotiation {
                    crate::qlog::server_version_information_failed(
                        &self.create_qlog_trace(orig_dcid.unwrap_or(initial.dst_cid).as_cid_ref()),
                        self.conn_params.get_versions().all(),
                        initial.version.wire_version(),
                    );
                }
                Output::None
            }
        }
    }

    fn process_input(&mut self, dgram: Datagram<impl AsRef<[u8]>>, now: Instant) -> Output {
        qtrace!("Process datagram: {}", hex(&dgram[..]));

        // This is only looking at the first packet header in the datagram.
        // All packets in the datagram are routed to the same connection.
        let res = PublicPacket::decode(&dgram[..], self.cid_generator.borrow().as_decoder());
        let Ok((packet, _remainder)) = res else {
            qtrace!([self], "Discarding {:?}", dgram);
            return Output::None;
        };

        // Finding an existing connection. Should be the most common case.
        if let Some(c) = self
            .connections
            .iter_mut()
            .find(|c| c.borrow().is_valid_local_cid(packet.dcid()))
        {
            return c.borrow_mut().process(Some(dgram), now);
        }

        if packet.packet_type() == PacketType::Short {
            // TODO send a stateless reset here.
            qtrace!([self], "Short header packet for an unknown connection");
            return Output::None;
        }

        if packet.packet_type() == PacketType::OtherVersion
            || (packet.packet_type() == PacketType::Initial
                && !self
                    .conn_params
                    .get_versions()
                    .all()
                    .contains(&packet.version().unwrap()))
        {
            if dgram.len() < MIN_INITIAL_PACKET_SIZE {
                qdebug!([self], "Unsupported version: too short");
                return Output::None;
            }

            qdebug!([self], "Unsupported version: {:x}", packet.wire_version());
            let vn = PacketBuilder::version_negotiation(
                &packet.scid()[..],
                &packet.dcid()[..],
                packet.wire_version(),
                self.conn_params.get_versions().all(),
            );

            crate::qlog::server_version_information_failed(
                &self.create_qlog_trace(packet.dcid()),
                self.conn_params.get_versions().all(),
                packet.wire_version(),
            );

            return Output::Datagram(Datagram::new(
                dgram.destination(),
                dgram.source(),
                dgram.tos(),
                vn,
            ));
        }

        match packet.packet_type() {
            PacketType::Initial => {
                if dgram.len() < MIN_INITIAL_PACKET_SIZE {
                    qdebug!([self], "Drop initial: too short");
                    return Output::None;
                }
                // Copy values from `packet` because they are currently still borrowing from
                // `dgram`.
                let initial = InitialDetails::new(&packet);
                self.handle_initial(initial, dgram, now)
            }
            PacketType::ZeroRtt => {
                let dcid = ConnectionId::from(packet.dcid());
                qdebug!([self], "Dropping 0-RTT for unknown connection {}", dcid);
                Output::None
            }
            PacketType::OtherVersion => unreachable!(),
            _ => {
                qtrace!([self], "Not an initial packet");
                Output::None
            }
        }
    }

    /// Iterate through the pending connections looking for any that might want
    /// to send a datagram.  Stop at the first one that does.
    fn process_next_output(&mut self, now: Instant) -> Output {
        let mut callback = None;

        for connection in &mut self.connections {
            match connection.borrow_mut().process_output(now) {
                Output::None => {}
                d @ Output::Datagram(_) => return d,
                Output::Callback(next) => match callback {
                    Some(previous) => callback = Some(min(previous, next)),
                    None => callback = Some(next),
                },
            }
        }

        callback.map_or(Output::None, Output::Callback)
    }

    /// Short-hand for [`Server::process`] without an input datagram.
    #[must_use]
    pub fn process_output(&mut self, now: Instant) -> Output {
        self.process(None::<Datagram>, now)
    }

    #[must_use]
    pub fn process(&mut self, dgram: Option<Datagram<impl AsRef<[u8]>>>, now: Instant) -> Output {
        let out = dgram
            .map_or(Output::None, |d| self.process_input(d, now))
            .or_else(|| self.process_next_output(now));

        // Clean-up closed connections.
        self.connections
            .retain(|c| !matches!(c.borrow().state(), State::Closed(_)));

        out
    }

    /// This lists the connections that have received new events
    /// as a result of calling `process()`.
    // `ActiveConnectionRef` `Hash` implementation doesn’t access any of the interior mutable types.
    #[allow(clippy::mutable_key_type)]
    #[must_use]
    pub fn active_connections(&self) -> HashSet<ConnectionRef> {
        self.connections
            .iter()
            .filter(|c| c.borrow().has_events())
            .map(|c| ConnectionRef { c: Rc::clone(c) })
            .collect()
    }

    /// Whether any connections have received new events as a result of calling
    /// `process()`.
    #[must_use]
    pub fn has_active_connections(&self) -> bool {
        self.connections.iter().any(|c| c.borrow().has_events())
    }
}

#[derive(Clone, Debug)]
pub struct ConnectionRef {
    c: Rc<RefCell<Connection>>,
}

impl ConnectionRef {
    #[must_use]
    pub fn borrow(&self) -> impl Deref<Target = Connection> + '_ {
        self.c.borrow()
    }

    #[must_use]
    pub fn borrow_mut(&self) -> impl DerefMut<Target = Connection> + '_ {
        self.c.borrow_mut()
    }

    #[must_use]
    pub fn connection(&self) -> Rc<RefCell<Connection>> {
        Rc::clone(&self.c)
    }
}

impl std::hash::Hash for ConnectionRef {
    fn hash<H: std::hash::Hasher>(&self, state: &mut H) {
        let ptr: *const _ = self.c.as_ref();
        ptr.hash(state);
    }
}

impl PartialEq for ConnectionRef {
    fn eq(&self, other: &Self) -> bool {
        Rc::ptr_eq(&self.c, &other.c)
    }
}

impl Eq for ConnectionRef {}

impl ::std::fmt::Display for Server {
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        write!(f, "Server")
    }
}
