// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

// Functions that handle capturing QLOG traces.

use std::{
    ops::{Deref as _, RangeInclusive},
    time::{Duration, Instant},
};

use neqo_common::{hex, qinfo, qlog::NeqoQlog, Decoder, IpTosEcn};
use qlog::events::{
    connectivity::{ConnectionStarted, ConnectionState, ConnectionStateUpdated},
    quic::{
        AckedRanges, ErrorSpace, MetricsUpdated, PacketDropped, PacketHeader, PacketLost,
        PacketReceived, PacketSent, QuicFrame, StreamType, VersionInformation,
    },
    EventData, RawInfo,
};
use smallvec::SmallVec;

use crate::{
    connection::State,
    frame::{CloseError, Frame},
    packet::{self, metadata::Direction, PacketType, PublicPacket},
    path::PathRef,
    recovery::SentPacket,
    stream_id::StreamType as NeqoStreamType,
    tparams::{
        TransportParameterId::{
            self, AckDelayExponent, ActiveConnectionIdLimit, DisableMigration, InitialMaxData,
            InitialMaxStreamDataBidiLocal, InitialMaxStreamDataBidiRemote, InitialMaxStreamDataUni,
            InitialMaxStreamsBidi, InitialMaxStreamsUni, MaxAckDelay, MaxUdpPayloadSize,
            OriginalDestinationConnectionId, StatelessResetToken,
        },
        TransportParametersHandler,
    },
    version::{Version, VersionConfig, WireVersion},
};

pub fn connection_tparams_set(qlog: &NeqoQlog, tph: &TransportParametersHandler, now: Instant) {
    qlog.add_event_data_with_instant(
        || {
            let remote = tph.remote();
            #[expect(clippy::cast_possible_truncation, reason = "These are OK.")]
            let ev_data =
                EventData::TransportParametersSet(qlog::events::quic::TransportParametersSet {
                    original_destination_connection_id: remote
                        .get_bytes(OriginalDestinationConnectionId)
                        .map(hex),
                    stateless_reset_token: remote.get_bytes(StatelessResetToken).map(hex),
                    disable_active_migration: remote.get_empty(DisableMigration).then_some(true),
                    max_idle_timeout: Some(remote.get_integer(TransportParameterId::IdleTimeout)),
                    max_udp_payload_size: Some(remote.get_integer(MaxUdpPayloadSize) as u32),
                    ack_delay_exponent: Some(remote.get_integer(AckDelayExponent) as u16),
                    max_ack_delay: Some(remote.get_integer(MaxAckDelay) as u16),
                    active_connection_id_limit: Some(
                        remote.get_integer(ActiveConnectionIdLimit) as u32
                    ),
                    initial_max_data: Some(remote.get_integer(InitialMaxData)),
                    initial_max_stream_data_bidi_local: Some(
                        remote.get_integer(InitialMaxStreamDataBidiLocal),
                    ),
                    initial_max_stream_data_bidi_remote: Some(
                        remote.get_integer(InitialMaxStreamDataBidiRemote),
                    ),
                    initial_max_stream_data_uni: Some(remote.get_integer(InitialMaxStreamDataUni)),
                    initial_max_streams_bidi: Some(remote.get_integer(InitialMaxStreamsBidi)),
                    initial_max_streams_uni: Some(remote.get_integer(InitialMaxStreamsUni)),
                    preferred_address: remote.get_preferred_address().and_then(|(paddr, cid)| {
                        Some(qlog::events::quic::PreferredAddress {
                            ip_v4: paddr.ipv4()?.ip().to_string(),
                            ip_v6: paddr.ipv6()?.ip().to_string(),
                            port_v4: paddr.ipv4()?.port(),
                            port_v6: paddr.ipv6()?.port(),
                            connection_id: cid.connection_id().to_string(),
                            stateless_reset_token: hex(cid.reset_token()),
                        })
                    }),
                    ..Default::default()
                });

            Some(ev_data)
        },
        now,
    );
}

pub fn server_connection_started(qlog: &NeqoQlog, path: &PathRef, now: Instant) {
    connection_started(qlog, path, now);
}

pub fn client_connection_started(qlog: &NeqoQlog, path: &PathRef, now: Instant) {
    connection_started(qlog, path, now);
}

fn connection_started(qlog: &NeqoQlog, path: &PathRef, now: Instant) {
    qlog.add_event_data_with_instant(
        || {
            let p = path.deref().borrow();
            let ev_data = EventData::ConnectionStarted(ConnectionStarted {
                ip_version: if p.local_address().ip().is_ipv4() {
                    Some("ipv4".into())
                } else {
                    Some("ipv6".into())
                },
                src_ip: format!("{}", p.local_address().ip()),
                dst_ip: format!("{}", p.remote_address().ip()),
                protocol: Some("QUIC".into()),
                src_port: p.local_address().port().into(),
                dst_port: p.remote_address().port().into(),
                src_cid: p.local_cid().map(ToString::to_string),
                dst_cid: p.remote_cid().map(ToString::to_string),
            });

            Some(ev_data)
        },
        now,
    );
}

#[expect(clippy::similar_names, reason = "new and now are similar.")]
pub fn connection_state_updated(qlog: &NeqoQlog, new: &State, now: Instant) {
    qlog.add_event_data_with_instant(
        || {
            let ev_data = EventData::ConnectionStateUpdated(ConnectionStateUpdated {
                old: None,
                new: match new {
                    State::Init | State::WaitInitial => ConnectionState::Attempted,
                    State::WaitVersion | State::Handshaking => ConnectionState::HandshakeStarted,
                    State::Connected => ConnectionState::HandshakeCompleted,
                    State::Confirmed => ConnectionState::HandshakeConfirmed,
                    State::Closing { .. } => ConnectionState::Closing,
                    State::Draining { .. } => ConnectionState::Draining,
                    State::Closed { .. } => ConnectionState::Closed,
                },
            });

            Some(ev_data)
        },
        now,
    );
}

pub fn client_version_information_initiated(
    qlog: &NeqoQlog,
    version_config: &VersionConfig,
    now: Instant,
) {
    qlog.add_event_data_with_instant(
        || {
            Some(EventData::VersionInformation(VersionInformation {
                client_versions: Some(
                    version_config
                        .all()
                        .iter()
                        .map(|v| format!("{:02x}", v.wire_version()))
                        .collect(),
                ),
                chosen_version: Some(format!("{:02x}", version_config.initial().wire_version())),
                ..Default::default()
            }))
        },
        now,
    );
}

pub fn client_version_information_negotiated(
    qlog: &NeqoQlog,
    client: &[Version],
    server: &[WireVersion],
    chosen: Version,
    now: Instant,
) {
    qlog.add_event_data_with_instant(
        || {
            Some(EventData::VersionInformation(VersionInformation {
                client_versions: Some(
                    client
                        .iter()
                        .map(|v| format!("{:02x}", v.wire_version()))
                        .collect(),
                ),
                server_versions: Some(server.iter().map(|v| format!("{v:02x}")).collect()),
                chosen_version: Some(format!("{:02x}", chosen.wire_version())),
            }))
        },
        now,
    );
}

pub fn server_version_information_failed(
    qlog: &NeqoQlog,
    server: &[Version],
    client: WireVersion,
    now: Instant,
) {
    qlog.add_event_data_with_instant(
        || {
            Some(EventData::VersionInformation(VersionInformation {
                client_versions: Some(vec![format!("{client:02x}")]),
                server_versions: Some(
                    server
                        .iter()
                        .map(|v| format!("{:02x}", v.wire_version()))
                        .collect(),
                ),
                ..Default::default()
            }))
        },
        now,
    );
}

pub fn packet_io(qlog: &NeqoQlog, meta: packet::MetaData, now: Instant) {
    qlog.add_event_data_with_instant(
        || {
            let mut d = Decoder::from(meta.payload());
            let raw = RawInfo {
                length: Some(meta.length() as u64),
                payload_length: None,
                data: None,
            };

            let mut frames = SmallVec::new();
            while d.remaining() > 0 {
                if let Ok(f) = Frame::decode(&mut d) {
                    frames.push(QuicFrame::from(f));
                } else {
                    qinfo!("qlog: invalid frame");
                    break;
                }
            }

            match meta.direction() {
                Direction::Tx => Some(EventData::PacketSent(PacketSent {
                    header: meta.into(),
                    frames: Some(frames),
                    raw: Some(raw),
                    ..Default::default()
                })),
                Direction::Rx => Some(EventData::PacketReceived(PacketReceived {
                    header: meta.into(),
                    frames: Some(frames.to_vec()),
                    raw: Some(raw),
                    ..Default::default()
                })),
            }
        },
        now,
    );
}

pub fn packet_dropped(qlog: &NeqoQlog, public_packet: &PublicPacket, now: Instant) {
    qlog.add_event_data_with_instant(
        || {
            let header =
                PacketHeader::with_type(public_packet.packet_type().into(), None, None, None, None);
            let raw = RawInfo {
                length: Some(public_packet.len() as u64),
                ..Default::default()
            };

            let ev_data = EventData::PacketDropped(PacketDropped {
                header: Some(header),
                raw: Some(raw),
                ..Default::default()
            });

            Some(ev_data)
        },
        now,
    );
}

pub fn packets_lost(qlog: &NeqoQlog, pkts: &[SentPacket], now: Instant) {
    qlog.add_event_with_stream(|stream| {
        for pkt in pkts {
            let header =
                PacketHeader::with_type(pkt.packet_type().into(), Some(pkt.pn()), None, None, None);

            let ev_data = EventData::PacketLost(PacketLost {
                header: Some(header),
                ..Default::default()
            });

            stream.add_event_data_with_instant(ev_data, now)?;
        }
        Ok(())
    });
}

#[expect(dead_code, reason = "TODO: Construct all variants.")]
pub enum QlogMetric {
    MinRtt(Duration),
    SmoothedRtt(Duration),
    LatestRtt(Duration),
    RttVariance(Duration),
    MaxAckDelay(u64),
    PtoCount(usize),
    CongestionWindow(usize),
    BytesInFlight(usize),
    SsThresh(usize),
    PacketsInFlight(u64),
    InRecovery(bool),
    PacingRate(u64),
}

pub fn metrics_updated(qlog: &NeqoQlog, updated_metrics: &[QlogMetric], now: Instant) {
    debug_assert!(!updated_metrics.is_empty());

    qlog.add_event_data_with_instant(
        || {
            let mut min_rtt: Option<f32> = None;
            let mut smoothed_rtt: Option<f32> = None;
            let mut latest_rtt: Option<f32> = None;
            let mut rtt_variance: Option<f32> = None;
            let mut pto_count: Option<u16> = None;
            let mut congestion_window: Option<u64> = None;
            let mut bytes_in_flight: Option<u64> = None;
            let mut ssthresh: Option<u64> = None;
            let mut packets_in_flight: Option<u64> = None;
            let mut pacing_rate: Option<u64> = None;

            for metric in updated_metrics {
                match metric {
                    QlogMetric::MinRtt(v) => min_rtt = Some(v.as_secs_f32() * 1000.0),
                    QlogMetric::SmoothedRtt(v) => smoothed_rtt = Some(v.as_secs_f32() * 1000.0),
                    QlogMetric::LatestRtt(v) => latest_rtt = Some(v.as_secs_f32() * 1000.0),
                    QlogMetric::RttVariance(v) => rtt_variance = Some(v.as_secs_f32() * 1000.0),
                    QlogMetric::PtoCount(v) => {
                        pto_count = Some(u16::try_from(*v).expect("fits in u16"));
                    }
                    QlogMetric::CongestionWindow(v) => {
                        congestion_window = Some(u64::try_from(*v).expect("fits in u64"));
                    }
                    QlogMetric::BytesInFlight(v) => {
                        bytes_in_flight = Some(u64::try_from(*v).expect("fits in u64"));
                    }
                    QlogMetric::SsThresh(v) => {
                        ssthresh = Some(u64::try_from(*v).expect("fits in u64"));
                    }
                    QlogMetric::PacketsInFlight(v) => packets_in_flight = Some(*v),
                    QlogMetric::PacingRate(v) => pacing_rate = Some(*v),
                    _ => (),
                }
            }

            let ev_data = EventData::MetricsUpdated(MetricsUpdated {
                min_rtt,
                smoothed_rtt,
                latest_rtt,
                rtt_variance,
                pto_count,
                congestion_window,
                bytes_in_flight,
                ssthresh,
                packets_in_flight,
                pacing_rate,
            });

            Some(ev_data)
        },
        now,
    );
}

// Helper functions

#[expect(clippy::too_many_lines, reason = "Yeah, but it's a nice match.")]
#[expect(
    clippy::cast_precision_loss,
    clippy::cast_possible_truncation,
    reason = "We need to truncate here."
)]
impl From<Frame<'_>> for QuicFrame {
    fn from(frame: Frame) -> Self {
        match frame {
            Frame::Padding(len) => Self::Padding {
                length: None,
                payload_length: u32::from(len),
            },
            Frame::Ping => Self::Ping {
                length: None,
                payload_length: None,
            },
            Frame::Ack {
                largest_acknowledged,
                ack_delay,
                first_ack_range,
                ack_ranges,
                ecn_count,
            } => {
                let ranges =
                    Frame::decode_ack_frame(largest_acknowledged, first_ack_range, &ack_ranges)
                        .ok();

                let acked_ranges = ranges.map(|all| {
                    AckedRanges::Double(
                        all.into_iter()
                            .map(RangeInclusive::into_inner)
                            .collect::<Vec<_>>(),
                    )
                });

                Self::Ack {
                    ack_delay: Some(ack_delay as f32 / 1000.0),
                    acked_ranges,
                    ect1: ecn_count.map(|c| c[IpTosEcn::Ect1]),
                    ect0: ecn_count.map(|c| c[IpTosEcn::Ect0]),
                    ce: ecn_count.map(|c| c[IpTosEcn::Ce]),
                    length: None,
                    payload_length: None,
                }
            }
            Frame::ResetStream {
                stream_id,
                application_error_code,
                final_size,
            } => Self::ResetStream {
                stream_id: stream_id.as_u64(),
                error_code: application_error_code,
                final_size,
                length: None,
                payload_length: None,
            },
            Frame::StopSending {
                stream_id,
                application_error_code,
            } => Self::StopSending {
                stream_id: stream_id.as_u64(),
                error_code: application_error_code,
                length: None,
                payload_length: None,
            },
            Frame::Crypto { offset, data } => Self::Crypto {
                offset,
                length: data.len() as u64,
            },
            Frame::NewToken { token } => Self::NewToken {
                token: qlog::Token {
                    ty: Some(qlog::TokenType::Retry),
                    details: None,
                    raw: Some(RawInfo {
                        data: Some(hex(token)),
                        length: Some(token.len() as u64),
                        payload_length: None,
                    }),
                },
            },
            Frame::Stream {
                fin,
                stream_id,
                offset,
                data,
                ..
            } => Self::Stream {
                stream_id: stream_id.as_u64(),
                offset,
                length: data.len() as u64,
                fin: Some(fin),
                raw: None,
            },
            Frame::MaxData { maximum_data } => Self::MaxData {
                maximum: maximum_data,
            },
            Frame::MaxStreamData {
                stream_id,
                maximum_stream_data,
            } => Self::MaxStreamData {
                stream_id: stream_id.as_u64(),
                maximum: maximum_stream_data,
            },
            Frame::MaxStreams {
                stream_type,
                maximum_streams,
            } => Self::MaxStreams {
                stream_type: match stream_type {
                    NeqoStreamType::BiDi => StreamType::Bidirectional,
                    NeqoStreamType::UniDi => StreamType::Unidirectional,
                },
                maximum: maximum_streams,
            },
            Frame::DataBlocked { data_limit } => Self::DataBlocked { limit: data_limit },
            Frame::StreamDataBlocked {
                stream_id,
                stream_data_limit,
            } => Self::StreamDataBlocked {
                stream_id: stream_id.as_u64(),
                limit: stream_data_limit,
            },
            Frame::StreamsBlocked {
                stream_type,
                stream_limit,
            } => Self::StreamsBlocked {
                stream_type: match stream_type {
                    NeqoStreamType::BiDi => StreamType::Bidirectional,
                    NeqoStreamType::UniDi => StreamType::Unidirectional,
                },
                limit: stream_limit,
            },
            Frame::NewConnectionId {
                sequence_number,
                retire_prior,
                connection_id,
                stateless_reset_token,
            } => Self::NewConnectionId {
                sequence_number: sequence_number as u32,
                retire_prior_to: retire_prior as u32,
                connection_id_length: Some(connection_id.len() as u8),
                connection_id: hex(connection_id),
                stateless_reset_token: Some(hex(stateless_reset_token)),
            },
            Frame::RetireConnectionId { sequence_number } => Self::RetireConnectionId {
                sequence_number: sequence_number as u32,
            },
            Frame::PathChallenge { data } => Self::PathChallenge {
                data: Some(hex(data)),
            },
            Frame::PathResponse { data } => Self::PathResponse {
                data: Some(hex(data)),
            },
            Frame::ConnectionClose {
                error_code,
                frame_type,
                reason_phrase,
            } => Self::ConnectionClose {
                error_space: match error_code {
                    CloseError::Transport(_) => Some(ErrorSpace::TransportError),
                    CloseError::Application(_) => Some(ErrorSpace::ApplicationError),
                },
                error_code: Some(error_code.code()),
                error_code_value: Some(0),
                reason: Some(reason_phrase),
                trigger_frame_type: Some(frame_type),
            },
            Frame::HandshakeDone => Self::HandshakeDone,
            Frame::AckFrequency { .. } => Self::Unknown {
                frame_type_value: None,
                raw_frame_type: frame.get_type().into(),
                raw: None,
            },
            Frame::Datagram { data, .. } => Self::Datagram {
                length: data.len() as u64,
                raw: None,
            },
        }
    }
}

impl From<PacketType> for qlog::events::quic::PacketType {
    fn from(value: PacketType) -> Self {
        match value {
            PacketType::Initial => Self::Initial,
            PacketType::Handshake => Self::Handshake,
            PacketType::ZeroRtt => Self::ZeroRtt,
            PacketType::Short => Self::OneRtt,
            PacketType::Retry => Self::Retry,
            PacketType::VersionNegotiation => Self::VersionNegotiation,
            PacketType::OtherVersion => Self::Unknown,
        }
    }
}
