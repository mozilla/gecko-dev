// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::{cmp::max, time::Duration};

use neqo_common::MAX_VARINT;

pub use crate::recovery::FAST_PTO_SCALE;
use crate::{
    connection::{ConnectionIdManager, Role, LOCAL_ACTIVE_CID_LIMIT},
    recv_stream::INITIAL_RECV_WINDOW_SIZE,
    rtt::GRANULARITY,
    stream_id::StreamType,
    tparams::{
        PreferredAddress, TransportParameter,
        TransportParameterId::{
            self, ActiveConnectionIdLimit, DisableMigration, GreaseQuicBit, InitialMaxData,
            InitialMaxStreamDataBidiLocal, InitialMaxStreamDataBidiRemote, InitialMaxStreamDataUni,
            InitialMaxStreamsBidi, InitialMaxStreamsUni, MaxAckDelay, MaxDatagramFrameSize,
            MinAckDelay,
        },
        TransportParametersHandler,
    },
    tracking::DEFAULT_LOCAL_ACK_DELAY,
    version::{Version, VersionConfig},
    CongestionControlAlgorithm, Res,
};

const LOCAL_MAX_DATA: u64 = MAX_VARINT;
const LOCAL_STREAM_LIMIT_BIDI: u64 = 16;
const LOCAL_STREAM_LIMIT_UNI: u64 = 16;
/// See `ConnectionParameters.ack_ratio` for a discussion of this value.
pub const ACK_RATIO_SCALE: u8 = 10;
/// By default, aim to have the peer acknowledge 4 times per round trip time.
/// See `ConnectionParameters.ack_ratio` for more.
pub const DEFAULT_ACK_RATIO: u8 = 4 * ACK_RATIO_SCALE;
/// The local value for the idle timeout period.
const DEFAULT_IDLE_TIMEOUT: Duration = Duration::from_secs(30);
const MAX_QUEUED_DATAGRAMS_DEFAULT: usize = 10;

/// What to do with preferred addresses.
#[derive(Debug, Clone)]
pub enum PreferredAddressConfig {
    /// Disabled, whether for client or server.
    Disabled,
    /// Enabled at a client, disabled at a server.
    Default,
    /// Enabled at both client and server.
    Address(PreferredAddress),
}

/// `ConnectionParameters` use for setting initial value for QUIC parameters.
/// This collects configuration like initial limits, protocol version, and
/// congestion control algorithm.
#[expect(clippy::struct_excessive_bools, reason = "We need that many, sorry.")]
#[derive(Debug, Clone)]
pub struct ConnectionParameters {
    versions: VersionConfig,
    cc_algorithm: CongestionControlAlgorithm,
    /// Initial connection-level flow control limit.
    max_data: u64,
    /// Initial flow control limit for receiving data on bidirectional streams that the peer
    /// creates.
    max_stream_data_bidi_remote: u64,
    /// Initial flow control limit for receiving data on bidirectional streams that this endpoint
    /// creates.
    max_stream_data_bidi_local: u64,
    /// Initial flow control limit for receiving data on unidirectional streams that the peer
    /// creates.
    max_stream_data_uni: u64,
    /// Initial limit on bidirectional streams that the peer creates.
    max_streams_bidi: u64,
    /// Initial limit on unidirectional streams that this endpoint creates.
    max_streams_uni: u64,
    /// The ACK ratio determines how many acknowledgements we will request as a
    /// fraction of both the current congestion window (expressed in packets) and
    /// as a fraction of the current round trip time.  This value is scaled by
    /// `ACK_RATIO_SCALE`; that is, if the goal is to have at least five
    /// acknowledgments every round trip, set the value to `5 * ACK_RATIO_SCALE`.
    /// Values less than `ACK_RATIO_SCALE` are clamped to `ACK_RATIO_SCALE`.
    ack_ratio: u8,
    /// The duration of the idle timeout for the connection.
    idle_timeout: Duration,
    preferred_address: PreferredAddressConfig,
    datagram_size: u64,
    outgoing_datagram_queue: usize,
    incoming_datagram_queue: usize,
    fast_pto: u8,
    grease: bool,
    disable_migration: bool,
    pacing: bool,
    /// Whether the connection performs PLPMTUD.
    pmtud: bool,
    /// Whether PMTUD should take the local interface MTU into account.
    pmtud_iface_mtu: bool,
    /// Whether the connection should use SNI slicing.
    sni_slicing: bool,
    /// Whether to enable mlkem768nistp256-sha256.
    mlkem: bool,
}

impl Default for ConnectionParameters {
    fn default() -> Self {
        Self {
            versions: VersionConfig::default(),
            cc_algorithm: CongestionControlAlgorithm::Cubic,
            max_data: LOCAL_MAX_DATA,
            max_stream_data_bidi_remote: u64::try_from(INITIAL_RECV_WINDOW_SIZE)
                .expect("usize fits in u64"),
            max_stream_data_bidi_local: u64::try_from(INITIAL_RECV_WINDOW_SIZE)
                .expect("usize fits in u64"),
            max_stream_data_uni: u64::try_from(INITIAL_RECV_WINDOW_SIZE)
                .expect("usize fits in u64"),
            max_streams_bidi: LOCAL_STREAM_LIMIT_BIDI,
            max_streams_uni: LOCAL_STREAM_LIMIT_UNI,
            ack_ratio: DEFAULT_ACK_RATIO,
            idle_timeout: DEFAULT_IDLE_TIMEOUT,
            preferred_address: PreferredAddressConfig::Default,
            datagram_size: 0,
            outgoing_datagram_queue: MAX_QUEUED_DATAGRAMS_DEFAULT,
            incoming_datagram_queue: MAX_QUEUED_DATAGRAMS_DEFAULT,
            fast_pto: FAST_PTO_SCALE,
            grease: true,
            disable_migration: false,
            pacing: true,
            pmtud: false,
            pmtud_iface_mtu: true,
            sni_slicing: true,
            mlkem: true,
        }
    }
}

impl ConnectionParameters {
    #[must_use]
    pub const fn get_versions(&self) -> &VersionConfig {
        &self.versions
    }

    pub(crate) fn get_versions_mut(&mut self) -> &mut VersionConfig {
        &mut self.versions
    }

    /// Describe the initial version that should be attempted and all the
    /// versions that should be enabled.  This list should contain the initial
    /// version and be in order of preference, with more preferred versions
    /// before less preferred.
    #[must_use]
    pub fn versions(mut self, initial: Version, all: Vec<Version>) -> Self {
        self.versions = VersionConfig::new(initial, all);
        self
    }

    #[must_use]
    pub const fn get_cc_algorithm(&self) -> CongestionControlAlgorithm {
        self.cc_algorithm
    }

    #[must_use]
    pub const fn cc_algorithm(mut self, v: CongestionControlAlgorithm) -> Self {
        self.cc_algorithm = v;
        self
    }

    #[must_use]
    pub const fn get_max_data(&self) -> u64 {
        self.max_data
    }

    #[must_use]
    pub const fn max_data(mut self, v: u64) -> Self {
        self.max_data = v;
        self
    }

    #[must_use]
    pub const fn get_max_streams(&self, stream_type: StreamType) -> u64 {
        match stream_type {
            StreamType::BiDi => self.max_streams_bidi,
            StreamType::UniDi => self.max_streams_uni,
        }
    }

    /// # Panics
    ///
    /// If v > 2^60 (the maximum allowed by the protocol).
    #[must_use]
    pub fn max_streams(mut self, stream_type: StreamType, v: u64) -> Self {
        assert!(v <= (1 << 60), "max_streams is too large");
        match stream_type {
            StreamType::BiDi => {
                self.max_streams_bidi = v;
            }
            StreamType::UniDi => {
                self.max_streams_uni = v;
            }
        }
        self
    }

    /// Get the maximum stream data that we will accept on different types of streams.
    ///
    /// # Panics
    ///
    /// If `StreamType::UniDi` and `false` are passed as that is not a valid combination.
    #[must_use]
    pub fn get_max_stream_data(&self, stream_type: StreamType, remote: bool) -> u64 {
        match (stream_type, remote) {
            (StreamType::BiDi, false) => self.max_stream_data_bidi_local,
            (StreamType::BiDi, true) => self.max_stream_data_bidi_remote,
            (StreamType::UniDi, false) => {
                panic!("Can't get receive limit on a stream that can only be sent")
            }
            (StreamType::UniDi, true) => self.max_stream_data_uni,
        }
    }

    /// Set the maximum stream data that we will accept on different types of streams.
    ///
    /// # Panics
    ///
    /// If `StreamType::UniDi` and `false` are passed as that is not a valid combination
    /// or if v >= 62 (the maximum allowed by the protocol).
    #[must_use]
    pub fn max_stream_data(mut self, stream_type: StreamType, remote: bool, v: u64) -> Self {
        assert!(v < (1 << 62), "max stream data is too large");
        match (stream_type, remote) {
            (StreamType::BiDi, false) => {
                self.max_stream_data_bidi_local = v;
            }
            (StreamType::BiDi, true) => {
                self.max_stream_data_bidi_remote = v;
            }
            (StreamType::UniDi, false) => {
                panic!("Can't set receive limit on a stream that can only be sent")
            }
            (StreamType::UniDi, true) => {
                self.max_stream_data_uni = v;
            }
        }
        self
    }

    /// Set a preferred address (which only has an effect for a server).
    #[must_use]
    pub const fn preferred_address(mut self, preferred: PreferredAddress) -> Self {
        self.preferred_address = PreferredAddressConfig::Address(preferred);
        self
    }

    /// Disable the use of preferred addresses.
    #[must_use]
    pub const fn disable_preferred_address(mut self) -> Self {
        self.preferred_address = PreferredAddressConfig::Disabled;
        self
    }

    #[must_use]
    pub const fn get_preferred_address(&self) -> &PreferredAddressConfig {
        &self.preferred_address
    }

    #[must_use]
    pub const fn ack_ratio(mut self, ack_ratio: u8) -> Self {
        self.ack_ratio = ack_ratio;
        self
    }

    #[must_use]
    pub const fn get_ack_ratio(&self) -> u8 {
        self.ack_ratio
    }

    /// # Panics
    ///
    /// If `timeout` is 2^62 milliseconds or more.
    #[must_use]
    pub fn idle_timeout(mut self, timeout: Duration) -> Self {
        assert!(timeout.as_millis() < (1 << 62), "idle timeout is too long");
        self.idle_timeout = timeout;
        self
    }

    #[must_use]
    pub const fn get_idle_timeout(&self) -> Duration {
        self.idle_timeout
    }

    #[must_use]
    pub const fn get_datagram_size(&self) -> u64 {
        self.datagram_size
    }

    #[must_use]
    pub const fn datagram_size(mut self, v: u64) -> Self {
        self.datagram_size = v;
        self
    }

    #[must_use]
    pub const fn get_outgoing_datagram_queue(&self) -> usize {
        self.outgoing_datagram_queue
    }

    #[must_use]
    pub fn outgoing_datagram_queue(mut self, v: usize) -> Self {
        // The max queue length must be at least 1.
        self.outgoing_datagram_queue = max(v, 1);
        self
    }

    #[must_use]
    pub const fn get_incoming_datagram_queue(&self) -> usize {
        self.incoming_datagram_queue
    }

    #[must_use]
    pub fn incoming_datagram_queue(mut self, v: usize) -> Self {
        // The max queue length must be at least 1.
        self.incoming_datagram_queue = max(v, 1);
        self
    }

    #[must_use]
    pub const fn get_fast_pto(&self) -> u8 {
        self.fast_pto
    }

    /// Scale the PTO timer.  A value of `FAST_PTO_SCALE` follows the spec, a smaller
    /// value does not, but produces more probes with the intent of ensuring lower
    /// latency in the event of tail loss. A value of `FAST_PTO_SCALE/4` is quite
    /// aggressive. Smaller values (other than zero) are not rejected, but could be
    /// very wasteful. Values greater than `FAST_PTO_SCALE` delay probes and could
    /// reduce performance. It should not be possible to increase the PTO timer by
    /// too much based on the range of valid values, but a maximum value of 255 will
    /// result in very poor performance.
    /// Scaling PTO this way does not affect when persistent congestion is declared,
    /// but may change how many retransmissions are sent before declaring persistent
    /// congestion.
    ///
    /// # Panics
    ///
    /// A value of 0 is invalid and will cause a panic.
    #[must_use]
    pub fn fast_pto(mut self, scale: u8) -> Self {
        assert_ne!(scale, 0);
        self.fast_pto = scale;
        self
    }

    #[must_use]
    pub const fn is_greasing(&self) -> bool {
        self.grease
    }

    #[must_use]
    pub const fn grease(mut self, grease: bool) -> Self {
        self.grease = grease;
        self
    }

    #[must_use]
    pub const fn disable_migration(mut self, disable_migration: bool) -> Self {
        self.disable_migration = disable_migration;
        self
    }

    #[must_use]
    pub const fn pacing_enabled(&self) -> bool {
        self.pacing
    }

    #[must_use]
    pub const fn pacing(mut self, pacing: bool) -> Self {
        self.pacing = pacing;
        self
    }

    #[must_use]
    pub const fn pmtud_enabled(&self) -> bool {
        self.pmtud
    }

    #[must_use]
    pub const fn pmtud(mut self, pmtud: bool) -> Self {
        self.pmtud = pmtud;
        self
    }

    #[must_use]
    pub const fn pmtud_iface_mtu_enabled(&self) -> bool {
        self.pmtud_iface_mtu
    }

    #[must_use]
    pub const fn pmtud_iface_mtu(mut self, pmtud_iface_mtu: bool) -> Self {
        self.pmtud_iface_mtu = pmtud_iface_mtu;
        self
    }

    #[must_use]
    pub const fn sni_slicing_enabled(&self) -> bool {
        self.sni_slicing
    }

    #[must_use]
    pub const fn sni_slicing(mut self, sni_slicing: bool) -> Self {
        self.sni_slicing = sni_slicing;
        self
    }

    #[must_use]
    pub const fn mlkem_enabled(&self) -> bool {
        self.mlkem
    }

    #[must_use]
    pub const fn mlkem(mut self, mlkem: bool) -> Self {
        self.mlkem = mlkem;
        self
    }

    /// # Errors
    /// When a connection ID cannot be obtained.
    /// # Panics
    /// Only when this code includes a transport parameter that is invalid.
    pub fn create_transport_parameter(
        &self,
        role: Role,
        cid_manager: &mut ConnectionIdManager,
    ) -> Res<TransportParametersHandler> {
        let mut tps = TransportParametersHandler::new(role, self.versions.clone());
        // default parameters
        tps.local_mut().set_integer(
            ActiveConnectionIdLimit,
            u64::try_from(LOCAL_ACTIVE_CID_LIMIT)?,
        );
        if self.disable_migration {
            tps.local_mut().set_empty(DisableMigration);
        }
        if self.grease {
            tps.local_mut().set_empty(GreaseQuicBit);
        }
        tps.local_mut().set_integer(
            MaxAckDelay,
            u64::try_from(DEFAULT_LOCAL_ACK_DELAY.as_millis())?,
        );
        tps.local_mut()
            .set_integer(MinAckDelay, u64::try_from(GRANULARITY.as_micros())?);

        // set configurable parameters
        tps.local_mut().set_integer(InitialMaxData, self.max_data);
        tps.local_mut().set_integer(
            InitialMaxStreamDataBidiLocal,
            self.max_stream_data_bidi_local,
        );
        tps.local_mut().set_integer(
            InitialMaxStreamDataBidiRemote,
            self.max_stream_data_bidi_remote,
        );
        tps.local_mut()
            .set_integer(InitialMaxStreamDataUni, self.max_stream_data_uni);
        tps.local_mut()
            .set_integer(InitialMaxStreamsBidi, self.max_streams_bidi);
        tps.local_mut()
            .set_integer(InitialMaxStreamsUni, self.max_streams_uni);
        tps.local_mut().set_integer(
            TransportParameterId::IdleTimeout,
            u64::try_from(self.idle_timeout.as_millis()).unwrap_or(0),
        );
        if let PreferredAddressConfig::Address(preferred) = &self.preferred_address {
            if role == Role::Server {
                let (cid, srt) = cid_manager.preferred_address_cid()?;
                tps.local_mut().set(
                    TransportParameterId::PreferredAddress,
                    TransportParameter::PreferredAddress {
                        v4: preferred.ipv4(),
                        v6: preferred.ipv6(),
                        cid,
                        srt,
                    },
                );
            }
        }
        tps.local_mut()
            .set_integer(MaxDatagramFrameSize, self.datagram_size);
        Ok(tps)
    }
}
