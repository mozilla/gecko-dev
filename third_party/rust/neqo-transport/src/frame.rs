// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

// Directly relating to QUIC frames.

use std::ops::RangeInclusive;

use neqo_common::{qtrace, Decoder, Encoder, MAX_VARINT};
use strum::FromRepr;

use crate::{
    cid::MAX_CONNECTION_ID_LEN,
    ecn,
    packet::PacketType,
    stream_id::{StreamId, StreamType},
    AppError, CloseReason, Error, Res, TransportError,
};

#[repr(u64)]
#[derive(Debug, Clone, Copy, PartialEq, Eq, FromRepr)]
pub enum FrameType {
    Padding = 0x0,
    Ping = 0x1,
    Ack = 0x2,
    AckEcn = 0x3,
    ResetStream = 0x4,
    StopSending = 0x5,
    Crypto = 0x6,
    NewToken = 0x7,
    Stream = 0x08, // + 0b000
    StreamWithFin = 0x08 + 0b001,
    StreamWithLen = 0x08 + 0b010,
    StreamWithLenFin = 0x08 + 0b011,
    StreamWithOff = 0x08 + 0b100,
    StreamWithOffFin = 0x08 + 0b101,
    StreamWithOffLen = 0x08 + 0b110,
    StreamWithOffLenFin = 0x08 + 0b111,
    MaxData = 0x10,
    MaxStreamData = 0x11,
    MaxStreamsBiDi = 0x12,
    MaxStreamsUniDi = 0x13,
    DataBlocked = 0x14,
    StreamDataBlocked = 0x15,
    StreamsBlockedBiDi = 0x16,
    StreamsBlockedUniDi = 0x17,
    NewConnectionId = 0x18,
    RetireConnectionId = 0x19,
    PathChallenge = 0x1a,
    PathResponse = 0x1b,
    ConnectionCloseTransport = 0x1c,
    ConnectionCloseApplication = 0x1d,
    HandshakeDone = 0x1e,
    // draft-ietf-quic-ack-delay
    AckFrequency = 0xaf,
    // draft-ietf-quic-datagram
    Datagram = 0x30,
    DatagramWithLen = 0x31,
}

impl From<FrameType> for u64 {
    fn from(val: FrameType) -> Self {
        val as Self
    }
}

impl From<FrameType> for u8 {
    fn from(val: FrameType) -> Self {
        val as Self
    }
}

impl TryFrom<u64> for FrameType {
    type Error = Error;

    fn try_from(value: u64) -> Result<Self, Self::Error> {
        Self::from_repr(value).ok_or(Error::UnknownFrameType)
    }
}

impl FrameType {
    const fn is_stream_with_length(self) -> bool {
        matches!(
            self,
            Self::StreamWithLen
                | Self::StreamWithLenFin
                | Self::StreamWithOffLen
                | Self::StreamWithOffLenFin
        )
    }

    const fn is_stream_with_offset(self) -> bool {
        matches!(
            self,
            Self::StreamWithOff
                | Self::StreamWithOffFin
                | Self::StreamWithOffLen
                | Self::StreamWithOffLenFin
        )
    }
    const fn is_stream_with_fin(self) -> bool {
        matches!(
            self,
            Self::StreamWithFin
                | Self::StreamWithLenFin
                | Self::StreamWithOffFin
                | Self::StreamWithOffLenFin
        )
    }
}

impl TryFrom<FrameType> for StreamType {
    type Error = Error;

    fn try_from(value: FrameType) -> Result<Self, Self::Error> {
        match value {
            FrameType::MaxStreamsBiDi | FrameType::StreamsBlockedBiDi => Ok(Self::BiDi),
            FrameType::MaxStreamsUniDi | FrameType::StreamsBlockedUniDi => Ok(Self::UniDi),
            _ => Err(Error::FrameEncodingError),
        }
    }
}

#[derive(PartialEq, Eq, Debug, PartialOrd, Ord, Clone, Copy)]
pub enum CloseError {
    Transport(TransportError),
    Application(AppError),
}

impl CloseError {
    #[must_use]
    pub const fn code(&self) -> u64 {
        match self {
            Self::Transport(c) | Self::Application(c) => *c,
        }
    }
}

impl From<CloseReason> for CloseError {
    fn from(err: CloseReason) -> Self {
        match err {
            CloseReason::Transport(c) => Self::Transport(c.code()),
            CloseReason::Application(c) => Self::Application(c),
        }
    }
}

impl From<std::array::TryFromSliceError> for Error {
    fn from(_err: std::array::TryFromSliceError) -> Self {
        Self::FrameEncodingError
    }
}

#[derive(PartialEq, Eq, Debug, Default, Clone)]
pub struct AckRange {
    gap: u64,
    range: u64,
}

#[derive(PartialEq, Eq, Debug, Clone)]
pub enum Frame<'a> {
    Padding(u16),
    Ping,
    Ack {
        largest_acknowledged: u64,
        ack_delay: u64,
        first_ack_range: u64,
        ack_ranges: Vec<AckRange>,
        ecn_count: Option<ecn::Count>,
    },
    ResetStream {
        stream_id: StreamId,
        application_error_code: AppError,
        final_size: u64,
    },
    StopSending {
        stream_id: StreamId,
        application_error_code: AppError,
    },
    Crypto {
        offset: u64,
        data: &'a [u8],
    },
    NewToken {
        token: &'a [u8],
    },
    Stream {
        stream_id: StreamId,
        offset: u64,
        data: &'a [u8],
        fin: bool,
        fill: bool,
    },
    MaxData {
        maximum_data: u64,
    },
    MaxStreamData {
        stream_id: StreamId,
        maximum_stream_data: u64,
    },
    MaxStreams {
        stream_type: StreamType,
        maximum_streams: u64,
    },
    DataBlocked {
        data_limit: u64,
    },
    StreamDataBlocked {
        stream_id: StreamId,
        stream_data_limit: u64,
    },
    StreamsBlocked {
        stream_type: StreamType,
        stream_limit: u64,
    },
    NewConnectionId {
        sequence_number: u64,
        retire_prior: u64,
        connection_id: &'a [u8],
        stateless_reset_token: &'a [u8; 16],
    },
    RetireConnectionId {
        sequence_number: u64,
    },
    PathChallenge {
        data: [u8; 8],
    },
    PathResponse {
        data: [u8; 8],
    },
    ConnectionClose {
        error_code: CloseError,
        frame_type: u64,
        // Not a reference as we use this to hold the value.
        // This is not used in optimized builds anyway.
        reason_phrase: String,
    },
    HandshakeDone,
    AckFrequency {
        /// The current ACK frequency sequence number.
        seqno: u64,
        /// The number of contiguous packets that can be received without
        /// acknowledging immediately.
        tolerance: u64,
        /// The time to delay after receiving the first packet that is
        /// not immediately acknowledged.
        delay: u64,
        /// Ignore reordering when deciding to immediately acknowledge.
        ignore_order: bool,
    },
    Datagram {
        data: &'a [u8],
        fill: bool,
    },
}

impl<'a> Frame<'a> {
    #[must_use]
    pub const fn get_type(&self) -> FrameType {
        match self {
            Self::Padding { .. } => FrameType::Padding,
            Self::Ping => FrameType::Ping,
            Self::Ack { .. } => FrameType::Ack,
            Self::ResetStream { .. } => FrameType::ResetStream,
            Self::StopSending { .. } => FrameType::StopSending,
            Self::Crypto { .. } => FrameType::Crypto,
            Self::NewToken { .. } => FrameType::NewToken,
            Self::Stream {
                fin, offset, fill, ..
            } => Self::stream_type(*fin, *offset > 0, *fill),
            Self::MaxData { .. } => FrameType::MaxData,
            Self::MaxStreamData { .. } => FrameType::MaxStreamData,
            Self::MaxStreams { stream_type, .. } => match stream_type {
                StreamType::BiDi => FrameType::MaxStreamsBiDi,
                StreamType::UniDi => FrameType::MaxStreamsUniDi,
            },
            Self::DataBlocked { .. } => FrameType::DataBlocked,
            Self::StreamDataBlocked { .. } => FrameType::StreamDataBlocked,
            Self::StreamsBlocked { stream_type, .. } => match stream_type {
                StreamType::BiDi => FrameType::StreamsBlockedBiDi,
                StreamType::UniDi => FrameType::StreamsBlockedUniDi,
            },
            Self::NewConnectionId { .. } => FrameType::NewConnectionId,
            Self::RetireConnectionId { .. } => FrameType::RetireConnectionId,
            Self::PathChallenge { .. } => FrameType::PathChallenge,
            Self::PathResponse { .. } => FrameType::PathResponse,
            Self::ConnectionClose { error_code, .. } => match error_code {
                CloseError::Transport(_) => FrameType::ConnectionCloseTransport,
                CloseError::Application(_) => FrameType::ConnectionCloseApplication,
            },
            Self::HandshakeDone => FrameType::HandshakeDone,
            Self::AckFrequency { .. } => FrameType::AckFrequency,
            Self::Datagram { fill, .. } => match fill {
                false => FrameType::Datagram,
                true => FrameType::DatagramWithLen,
            },
        }
    }

    #[must_use]
    pub const fn is_stream(&self) -> bool {
        matches!(
            self,
            Self::ResetStream { .. }
                | Self::StopSending { .. }
                | Self::Stream { .. }
                | Self::MaxData { .. }
                | Self::MaxStreamData { .. }
                | Self::MaxStreams { .. }
                | Self::DataBlocked { .. }
                | Self::StreamDataBlocked { .. }
                | Self::StreamsBlocked { .. }
        )
    }

    #[must_use]
    pub const fn stream_type(fin: bool, nonzero_offset: bool, fill: bool) -> FrameType {
        match (nonzero_offset, fill, fin) {
            (false, true, false) => FrameType::Stream,
            (false, true, true) => FrameType::StreamWithFin,
            (false, false, false) => FrameType::StreamWithLen,
            (false, false, true) => FrameType::StreamWithLenFin,
            (true, true, false) => FrameType::StreamWithOff,
            (true, true, true) => FrameType::StreamWithOffFin,
            (true, false, false) => FrameType::StreamWithOffLen,
            (true, false, true) => FrameType::StreamWithOffLenFin,
        }
    }

    /// If the frame causes a recipient to generate an ACK within its
    /// advertised maximum acknowledgement delay.
    #[must_use]
    pub const fn ack_eliciting(&self) -> bool {
        !matches!(
            self,
            Self::Ack { .. } | Self::Padding { .. } | Self::ConnectionClose { .. }
        )
    }

    /// If the frame can be sent in a path probe
    /// without initiating migration to that path.
    #[must_use]
    pub const fn path_probing(&self) -> bool {
        matches!(
            self,
            Self::Padding { .. }
                | Self::NewConnectionId { .. }
                | Self::PathChallenge { .. }
                | Self::PathResponse { .. }
        )
    }

    /// Converts `AckRanges` as encoded in a ACK frame (see -transport
    /// 19.3.1) into ranges of acked packets (end, start), inclusive of
    /// start and end values.
    ///
    /// # Errors
    ///
    /// Returns an error if the ranges are invalid.
    pub fn decode_ack_frame(
        largest_acked: u64,
        first_ack_range: u64,
        ack_ranges: &[AckRange],
    ) -> Res<Vec<RangeInclusive<u64>>> {
        let mut acked_ranges = Vec::with_capacity(ack_ranges.len() + 1);

        if largest_acked < first_ack_range {
            return Err(Error::FrameEncodingError);
        }
        acked_ranges.push((largest_acked - first_ack_range)..=largest_acked);
        if !ack_ranges.is_empty() && largest_acked < first_ack_range + 1 {
            return Err(Error::FrameEncodingError);
        }
        let mut cur = if ack_ranges.is_empty() {
            0
        } else {
            largest_acked - first_ack_range - 1
        };
        for r in ack_ranges {
            if cur < r.gap + 1 {
                return Err(Error::FrameEncodingError);
            }
            cur = cur - r.gap - 1;

            if cur < r.range {
                return Err(Error::FrameEncodingError);
            }
            acked_ranges.push((cur - r.range)..=cur);

            if cur > r.range + 1 {
                cur -= r.range + 1;
            } else {
                cur -= r.range;
            }
        }

        Ok(acked_ranges)
    }

    #[must_use]
    pub fn dump(&self) -> String {
        match self {
            Self::Crypto { offset, data } => {
                format!("Crypto {{ offset: {offset}, len: {} }}", data.len())
            }
            Self::Stream {
                stream_id,
                offset,
                fill,
                data,
                fin,
            } => format!(
                "Stream {{ stream_id: {}, offset: {offset}, len: {}{}, fin: {fin} }}",
                stream_id.as_u64(),
                if *fill { ">>" } else { "" },
                data.len(),
            ),
            Self::Padding(length) => format!("Padding {{ len: {length} }}"),
            Self::Datagram { data, .. } => format!("Datagram {{ len: {} }}", data.len()),
            _ => format!("{self:?}"),
        }
    }

    #[must_use]
    pub fn is_allowed(&self, pt: PacketType) -> bool {
        match self {
            Self::Padding { .. } | Self::Ping => true,
            Self::Crypto { .. }
            | Self::Ack { .. }
            | Self::ConnectionClose {
                error_code: CloseError::Transport(_),
                ..
            } => pt != PacketType::ZeroRtt,
            Self::NewToken { .. } | Self::ConnectionClose { .. } => pt == PacketType::Short,
            _ => pt == PacketType::ZeroRtt || pt == PacketType::Short,
        }
    }

    /// # Errors
    ///
    /// Returns an error if the frame cannot be decoded.
    #[expect(
        clippy::too_many_lines,
        reason = "Yeah, but it's a nice match statement."
    )]
    pub fn decode(dec: &mut Decoder<'a>) -> Res<Self> {
        /// Maximum ACK Range Count in ACK Frame
        ///
        /// Given a max UDP datagram size of 64k bytes and a minimum ACK Range size of 2
        /// bytes (2 QUIC varints), a single datagram can at most contain 32k ACK
        /// Ranges.
        ///
        /// Note that the maximum (jumbogram) Ethernet MTU of 9216 or on the
        /// Internet the regular Ethernet MTU of 1518 are more realistically to
        /// be the limiting factor. Though for simplicity the higher limit is chosen.
        const MAX_ACK_RANGE_COUNT: u64 = 32 * 1024;

        fn d<T>(v: Option<T>) -> Res<T> {
            v.ok_or(Error::NoMoreData)
        }
        fn dv(dec: &mut Decoder) -> Res<u64> {
            d(dec.decode_varint())
        }

        fn decode_ack<'a>(dec: &mut Decoder<'a>, ecn: bool) -> Res<Frame<'a>> {
            let la = dv(dec)?;
            let ad = dv(dec)?;
            let nr = dv(dec).and_then(|nr| {
                if nr < MAX_ACK_RANGE_COUNT {
                    Ok(nr)
                } else {
                    Err(Error::TooMuchData)
                }
            })?;
            let fa = dv(dec)?;
            let mut arr: Vec<AckRange> = Vec::with_capacity(usize::try_from(nr)?);
            for _ in 0..nr {
                let ar = AckRange {
                    gap: dv(dec)?,
                    range: dv(dec)?,
                };
                arr.push(ar);
            }

            // Now check for the values for ACK_ECN.
            let ecn_count = ecn
                .then(|| -> Res<ecn::Count> {
                    Ok(ecn::Count::new(0, dv(dec)?, dv(dec)?, dv(dec)?))
                })
                .transpose()?;

            Ok(Frame::Ack {
                largest_acknowledged: la,
                ack_delay: ad,
                first_ack_range: fa,
                ack_ranges: arr,
                ecn_count,
            })
        }

        // Check for minimal encoding of frame type.
        let pos = dec.offset();
        let t = dv(dec)?;
        // RFC 9000, Section 12.4:
        //
        // The Frame Type field uses a variable-length integer encoding [...],
        // with one exception. To ensure simple and efficient implementations of
        // frame parsing, a frame type MUST use the shortest possible encoding.
        if Encoder::varint_len(t) != dec.offset() - pos {
            return Err(Error::ProtocolViolation);
        }

        let t = t.try_into()?;
        match t {
            FrameType::Padding => {
                let mut length: u16 = 1;
                while let Some(b) = dec.peek_byte() {
                    if b != u8::from(FrameType::Padding) {
                        break;
                    }
                    length += 1;
                    dec.skip(1);
                }
                Ok(Self::Padding(length))
            }
            FrameType::Ping => Ok(Self::Ping),
            FrameType::ResetStream => Ok(Self::ResetStream {
                stream_id: StreamId::from(dv(dec)?),
                application_error_code: dv(dec)?,
                final_size: match dec.decode_varint() {
                    Some(v) => v,
                    _ => return Err(Error::NoMoreData),
                },
            }),
            FrameType::Ack => decode_ack(dec, false),
            FrameType::AckEcn => decode_ack(dec, true),
            FrameType::StopSending => Ok(Self::StopSending {
                stream_id: StreamId::from(dv(dec)?),
                application_error_code: dv(dec)?,
            }),
            FrameType::Crypto => {
                let offset = dv(dec)?;
                let data = d(dec.decode_vvec())?;
                if offset + u64::try_from(data.len())? > MAX_VARINT {
                    return Err(Error::FrameEncodingError);
                }
                Ok(Self::Crypto { offset, data })
            }
            FrameType::NewToken => {
                let token = d(dec.decode_vvec())?;
                if token.is_empty() {
                    return Err(Error::FrameEncodingError);
                }
                Ok(Self::NewToken { token })
            }
            FrameType::Stream
            | FrameType::StreamWithFin
            | FrameType::StreamWithLen
            | FrameType::StreamWithLenFin
            | FrameType::StreamWithOff
            | FrameType::StreamWithOffFin
            | FrameType::StreamWithOffLen
            | FrameType::StreamWithOffLenFin => {
                let s = dv(dec)?;
                let o = if t.is_stream_with_offset() {
                    dv(dec)?
                } else {
                    0
                };
                let fill = !t.is_stream_with_length();
                let data = if fill {
                    qtrace!("STREAM frame, extends to the end of the packet");
                    dec.decode_remainder()
                } else {
                    qtrace!("STREAM frame, with length");
                    d(dec.decode_vvec())?
                };
                if o + u64::try_from(data.len())? > MAX_VARINT {
                    return Err(Error::FrameEncodingError);
                }
                Ok(Self::Stream {
                    fin: t.is_stream_with_fin(),
                    stream_id: StreamId::from(s),
                    offset: o,
                    data,
                    fill,
                })
            }
            FrameType::MaxData => Ok(Self::MaxData {
                maximum_data: dv(dec)?,
            }),
            FrameType::MaxStreamData => Ok(Self::MaxStreamData {
                stream_id: StreamId::from(dv(dec)?),
                maximum_stream_data: dv(dec)?,
            }),
            FrameType::MaxStreamsBiDi | FrameType::MaxStreamsUniDi => {
                let m = dv(dec)?;
                if m > (1 << 60) {
                    return Err(Error::StreamLimitError);
                }
                Ok(Self::MaxStreams {
                    stream_type: t.try_into()?,
                    maximum_streams: m,
                })
            }
            FrameType::DataBlocked => Ok(Self::DataBlocked {
                data_limit: dv(dec)?,
            }),
            FrameType::StreamDataBlocked => Ok(Self::StreamDataBlocked {
                stream_id: dv(dec)?.into(),
                stream_data_limit: dv(dec)?,
            }),
            FrameType::StreamsBlockedBiDi | FrameType::StreamsBlockedUniDi => {
                Ok(Self::StreamsBlocked {
                    stream_type: t.try_into()?,
                    stream_limit: dv(dec)?,
                })
            }
            FrameType::NewConnectionId => {
                let sequence_number = dv(dec)?;
                let retire_prior = dv(dec)?;
                let connection_id = d(dec.decode_vec(1))?;
                if connection_id.len() > MAX_CONNECTION_ID_LEN {
                    return Err(Error::FrameEncodingError);
                }
                let srt = d(dec.decode(16))?;
                let stateless_reset_token = <&[_; 16]>::try_from(srt)?;

                Ok(Self::NewConnectionId {
                    sequence_number,
                    retire_prior,
                    connection_id,
                    stateless_reset_token,
                })
            }
            FrameType::RetireConnectionId => Ok(Self::RetireConnectionId {
                sequence_number: dv(dec)?,
            }),
            FrameType::PathChallenge => {
                let data = d(dec.decode(8))?;
                let mut datav: [u8; 8] = [0; 8];
                datav.copy_from_slice(data);
                Ok(Self::PathChallenge { data: datav })
            }
            FrameType::PathResponse => {
                let data = d(dec.decode(8))?;
                let mut datav: [u8; 8] = [0; 8];
                datav.copy_from_slice(data);
                Ok(Self::PathResponse { data: datav })
            }
            FrameType::ConnectionCloseTransport | FrameType::ConnectionCloseApplication => {
                let (error_code, frame_type) = if t == FrameType::ConnectionCloseTransport {
                    (CloseError::Transport(dv(dec)?), dv(dec)?)
                } else {
                    (CloseError::Application(dv(dec)?), 0)
                };
                // We can tolerate this copy for now.
                let reason_phrase = String::from_utf8_lossy(d(dec.decode_vvec())?).to_string();
                Ok(Self::ConnectionClose {
                    error_code,
                    frame_type,
                    reason_phrase,
                })
            }
            FrameType::HandshakeDone => Ok(Self::HandshakeDone),
            FrameType::AckFrequency => {
                let seqno = dv(dec)?;
                let tolerance = dv(dec)?;
                if tolerance == 0 {
                    return Err(Error::FrameEncodingError);
                }
                let delay = dv(dec)?;
                let ignore_order = match d(dec.decode_uint::<u8>())? {
                    0 => false,
                    1 => true,
                    _ => return Err(Error::FrameEncodingError),
                };
                Ok(Self::AckFrequency {
                    seqno,
                    tolerance,
                    delay,
                    ignore_order,
                })
            }
            FrameType::Datagram | FrameType::DatagramWithLen => {
                let fill = t == FrameType::Datagram;
                let data = if fill {
                    qtrace!("DATAGRAM frame, extends to the end of the packet");
                    dec.decode_remainder()
                } else {
                    qtrace!("DATAGRAM frame, with length");
                    d(dec.decode_vvec())?
                };
                Ok(Self::Datagram { data, fill })
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use neqo_common::{Decoder, Encoder};

    use crate::{
        cid::MAX_CONNECTION_ID_LEN,
        ecn::Count,
        frame::{AckRange, Frame, FrameType},
        CloseError, Error, StreamId, StreamType,
    };

    fn just_dec(f: &Frame, s: &str) {
        let encoded = Encoder::from_hex(s);
        let decoded = Frame::decode(&mut encoded.as_decoder()).expect("Failed to decode frame");
        assert_eq!(*f, decoded);
    }

    #[test]
    fn padding() {
        let f = Frame::Padding(1);
        just_dec(&f, "00");
        let f = Frame::Padding(2);
        just_dec(&f, "0000");
    }

    #[test]
    fn ping() {
        let f = Frame::Ping;
        just_dec(&f, "01");
    }

    #[test]
    fn ack() {
        let ar = vec![AckRange { gap: 1, range: 2 }, AckRange { gap: 3, range: 4 }];

        let f = Frame::Ack {
            largest_acknowledged: 0x1234,
            ack_delay: 0x1235,
            first_ack_range: 0x1236,
            ack_ranges: ar.clone(),
            ecn_count: None,
        };

        just_dec(&f, "025234523502523601020304");

        // Try to parse ACK_ECN without ECN values
        let enc = Encoder::from_hex("035234523502523601020304");
        let mut dec = enc.as_decoder();
        assert_eq!(Frame::decode(&mut dec).unwrap_err(), Error::NoMoreData);

        // Try to parse ACK_ECN with ECN values
        let ecn_count = Some(Count::new(0, 1, 2, 3));
        let fe = Frame::Ack {
            largest_acknowledged: 0x1234,
            ack_delay: 0x1235,
            first_ack_range: 0x1236,
            ack_ranges: ar,
            ecn_count,
        };
        let enc = Encoder::from_hex("035234523502523601020304010203");
        let mut dec = enc.as_decoder();
        assert_eq!(Frame::decode(&mut dec).unwrap(), fe);
    }

    #[test]
    fn reset_stream() {
        let f = Frame::ResetStream {
            stream_id: StreamId::from(0x1234),
            application_error_code: 0x77,
            final_size: 0x3456,
        };

        just_dec(&f, "04523440777456");
    }

    #[test]
    fn stop_sending() {
        let f = Frame::StopSending {
            stream_id: StreamId::from(63),
            application_error_code: 0x77,
        };

        just_dec(&f, "053F4077");
    }

    #[test]
    fn crypto() {
        let f = Frame::Crypto {
            offset: 1,
            data: &[1, 2, 3],
        };

        just_dec(&f, "060103010203");
    }

    #[test]
    fn new_token() {
        let f = Frame::NewToken {
            token: &[0x12, 0x34, 0x56],
        };

        just_dec(&f, "0703123456");
    }

    #[test]
    fn empty_new_token() {
        let mut dec = Decoder::from(&[0x07, 0x00][..]);
        assert_eq!(
            Frame::decode(&mut dec).unwrap_err(),
            Error::FrameEncodingError
        );
    }

    #[test]
    fn stream() {
        // First, just set the length bit.
        let f = Frame::Stream {
            fin: false,
            stream_id: StreamId::from(5),
            offset: 0,
            data: &[1, 2, 3],
            fill: false,
        };

        just_dec(&f, "0a0503010203");

        // Now with offset != 0 and FIN
        let f = Frame::Stream {
            fin: true,
            stream_id: StreamId::from(5),
            offset: 1,
            data: &[1, 2, 3],
            fill: false,
        };
        just_dec(&f, "0f050103010203");

        // Now to fill the packet.
        let f = Frame::Stream {
            fin: true,
            stream_id: StreamId::from(5),
            offset: 0,
            data: &[1, 2, 3],
            fill: true,
        };
        just_dec(&f, "0905010203");
    }

    #[test]
    fn max_data() {
        let f = Frame::MaxData {
            maximum_data: 0x1234,
        };

        just_dec(&f, "105234");
    }

    #[test]
    fn max_stream_data() {
        let f = Frame::MaxStreamData {
            stream_id: StreamId::from(5),
            maximum_stream_data: 0x1234,
        };

        just_dec(&f, "11055234");
    }

    #[test]
    fn max_streams() {
        let mut f = Frame::MaxStreams {
            stream_type: StreamType::BiDi,
            maximum_streams: 0x1234,
        };

        just_dec(&f, "125234");

        f = Frame::MaxStreams {
            stream_type: StreamType::UniDi,
            maximum_streams: 0x1234,
        };

        just_dec(&f, "135234");
    }

    #[test]
    fn data_blocked() {
        let f = Frame::DataBlocked { data_limit: 0x1234 };

        just_dec(&f, "145234");
    }

    #[test]
    fn stream_data_blocked() {
        let f = Frame::StreamDataBlocked {
            stream_id: StreamId::from(5),
            stream_data_limit: 0x1234,
        };

        just_dec(&f, "15055234");
    }

    #[test]
    fn streams_blocked() {
        let mut f = Frame::StreamsBlocked {
            stream_type: StreamType::BiDi,
            stream_limit: 0x1234,
        };

        just_dec(&f, "165234");

        f = Frame::StreamsBlocked {
            stream_type: StreamType::UniDi,
            stream_limit: 0x1234,
        };

        just_dec(&f, "175234");
    }

    #[test]
    fn new_connection_id() {
        let f = Frame::NewConnectionId {
            sequence_number: 0x1234,
            retire_prior: 0,
            connection_id: &[0x01, 0x02],
            stateless_reset_token: &[9; 16],
        };

        just_dec(&f, "1852340002010209090909090909090909090909090909");
    }

    #[test]
    fn too_large_new_connection_id() {
        let mut enc = Encoder::from_hex("18523400"); // up to the CID
        enc.encode_vvec(&[0x0c; MAX_CONNECTION_ID_LEN + 10]);
        enc.encode(&[0x11; 16][..]);
        assert_eq!(
            Frame::decode(&mut enc.as_decoder()).unwrap_err(),
            Error::FrameEncodingError
        );
    }

    #[test]
    fn retire_connection_id() {
        let f = Frame::RetireConnectionId {
            sequence_number: 0x1234,
        };

        just_dec(&f, "195234");
    }

    #[test]
    fn path_challenge() {
        let f = Frame::PathChallenge { data: [9; 8] };

        just_dec(&f, "1a0909090909090909");
    }

    #[test]
    fn path_response() {
        let f = Frame::PathResponse { data: [9; 8] };

        just_dec(&f, "1b0909090909090909");
    }

    #[test]
    fn connection_close_transport() {
        let f = Frame::ConnectionClose {
            error_code: CloseError::Transport(0x5678),
            frame_type: 0x1234,
            reason_phrase: String::from("\x01\x02\x03"),
        };

        just_dec(&f, "1c80005678523403010203");
    }

    #[test]
    fn connection_close_application() {
        let f = Frame::ConnectionClose {
            error_code: CloseError::Application(0x5678),
            frame_type: 0,
            reason_phrase: String::from("\x01\x02\x03"),
        };

        just_dec(&f, "1d8000567803010203");
    }

    #[test]
    fn compare() {
        let f1 = Frame::Padding(1);
        let f2 = Frame::Padding(1);
        let f3 = Frame::Crypto {
            offset: 0,
            data: &[1, 2, 3],
        };
        let f4 = Frame::Crypto {
            offset: 0,
            data: &[1, 2, 3],
        };
        let f5 = Frame::Crypto {
            offset: 1,
            data: &[1, 2, 3],
        };
        let f6 = Frame::Crypto {
            offset: 0,
            data: &[1, 2, 4],
        };

        assert_eq!(f1, f2);
        assert_ne!(f1, f3);
        assert_eq!(f3, f4);
        assert_ne!(f3, f5);
        assert_ne!(f3, f6);
    }

    #[test]
    fn decode_ack_frame() {
        let res = Frame::decode_ack_frame(7, 2, &[AckRange { gap: 0, range: 3 }]);
        assert!(res.is_ok());
        assert_eq!(res.unwrap(), vec![5..=7, 0..=3]);
    }

    #[test]
    fn ack_frequency() {
        let f = Frame::AckFrequency {
            seqno: 10,
            tolerance: 5,
            delay: 2000,
            ignore_order: true,
        };
        just_dec(&f, "40af0a0547d001");
    }

    #[test]
    fn ack_frequency_ignore_error_error() {
        let enc = Encoder::from_hex("40af0a0547d003"); // ignore_order of 3
        assert_eq!(
            Frame::decode(&mut enc.as_decoder()).unwrap_err(),
            Error::FrameEncodingError
        );
    }

    /// Hopefully this test is eventually redundant.
    #[test]
    fn ack_frequency_zero_packets() {
        let enc = Encoder::from_hex("40af0a000101"); // packets of 0
        assert_eq!(
            Frame::decode(&mut enc.as_decoder()).unwrap_err(),
            Error::FrameEncodingError
        );
    }

    #[test]
    fn datagram() {
        // Without the length bit.
        let f = Frame::Datagram {
            data: &[1, 2, 3],
            fill: true,
        };

        just_dec(&f, "30010203");

        // With the length bit.
        let f = Frame::Datagram {
            data: &[1, 2, 3],
            fill: false,
        };
        just_dec(&f, "3103010203");
    }

    #[test]
    fn frame_decode_enforces_bound_on_ack_range() {
        let mut e = Encoder::new();

        e.encode_varint(FrameType::Ack);
        e.encode_varint(0u64); // largest acknowledged
        e.encode_varint(0u64); // ACK delay
        e.encode_varint(u32::MAX); // ACK range count = huge, but maybe available for allocation

        assert_eq!(Err(Error::TooMuchData), Frame::decode(&mut e.as_decoder()));
    }

    #[test]
    #[should_panic(expected = "Failed to decode frame")]
    fn invalid_frame_type_len() {
        let f = Frame::Datagram {
            data: &[1, 2, 3],
            fill: true,
        };

        just_dec(&f, "4030010203");
    }
}
