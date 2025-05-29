// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::fmt::{Debug, Write as _};

use neqo_common::{Decoder, Encoder};
use neqo_crypto::random;
use neqo_transport::StreamId;

use crate::{frames::reader::FrameDecoder, settings::HSettings, Error, Priority, PushId, Res};

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct HFrameType(pub u64);

pub const H3_FRAME_TYPE_DATA: HFrameType = HFrameType(0x0);
pub const H3_FRAME_TYPE_HEADERS: HFrameType = HFrameType(0x1);
pub const H3_FRAME_TYPE_CANCEL_PUSH: HFrameType = HFrameType(0x3);
pub const H3_FRAME_TYPE_SETTINGS: HFrameType = HFrameType(0x4);
pub const H3_FRAME_TYPE_PUSH_PROMISE: HFrameType = HFrameType(0x5);
pub const H3_FRAME_TYPE_GOAWAY: HFrameType = HFrameType(0x7);
pub const H3_FRAME_TYPE_MAX_PUSH_ID: HFrameType = HFrameType(0xd);
pub const H3_FRAME_TYPE_PRIORITY_UPDATE_REQUEST: HFrameType = HFrameType(0xf0700);
pub const H3_FRAME_TYPE_PRIORITY_UPDATE_PUSH: HFrameType = HFrameType(0xf0701);

pub const H3_RESERVED_FRAME_TYPES: &[HFrameType] = &[
    HFrameType(0x2),
    HFrameType(0x6),
    HFrameType(0x8),
    HFrameType(0x9),
];

impl From<HFrameType> for u64 {
    fn from(t: HFrameType) -> Self {
        t.0
    }
}

// data for DATA frame is not read into HFrame::Data.
#[derive(PartialEq, Eq, Debug)]
pub enum HFrame {
    Data {
        len: u64, // length of the data
    },
    Headers {
        header_block: Vec<u8>,
    },
    CancelPush {
        push_id: PushId,
    },
    Settings {
        settings: HSettings,
    },
    PushPromise {
        push_id: PushId,
        header_block: Vec<u8>,
    },
    Goaway {
        stream_id: StreamId,
    },
    MaxPushId {
        push_id: PushId,
    },
    Grease,
    PriorityUpdateRequest {
        element_id: u64,
        priority: Priority,
    },
    PriorityUpdatePush {
        element_id: u64,
        priority: Priority,
    },
}

impl HFrame {
    fn get_type(&self) -> HFrameType {
        match self {
            Self::Data { .. } => H3_FRAME_TYPE_DATA,
            Self::Headers { .. } => H3_FRAME_TYPE_HEADERS,
            Self::CancelPush { .. } => H3_FRAME_TYPE_CANCEL_PUSH,
            Self::Settings { .. } => H3_FRAME_TYPE_SETTINGS,
            Self::PushPromise { .. } => H3_FRAME_TYPE_PUSH_PROMISE,
            Self::Goaway { .. } => H3_FRAME_TYPE_GOAWAY,
            Self::MaxPushId { .. } => H3_FRAME_TYPE_MAX_PUSH_ID,
            Self::PriorityUpdateRequest { .. } => H3_FRAME_TYPE_PRIORITY_UPDATE_REQUEST,
            Self::PriorityUpdatePush { .. } => H3_FRAME_TYPE_PRIORITY_UPDATE_PUSH,
            Self::Grease => {
                let r = u64::from_ne_bytes(random::<8>());
                // Zero out the top 7 bits: 2 for being a varint; 5 to account for the *0x1f.
                HFrameType((r >> 7) * 0x1f + 0x21)
            }
        }
    }

    pub fn encode(&self, enc: &mut Encoder) {
        enc.encode_varint(self.get_type());

        match self {
            Self::Data { len } => {
                // DATA frame only encode the length here.
                enc.encode_varint(*len);
            }
            Self::Headers { header_block } => {
                enc.encode_vvec(header_block);
            }
            Self::CancelPush { push_id } => {
                enc.encode_vvec_with(|enc_inner| {
                    enc_inner.encode_varint(*push_id);
                });
            }
            Self::Settings { settings } => {
                settings.encode_frame_contents(enc);
            }
            Self::PushPromise {
                push_id,
                header_block,
            } => {
                enc.encode_varint(
                    (header_block.len() + (Encoder::varint_len(u64::from(*push_id)))) as u64,
                );
                enc.encode_varint(*push_id);
                enc.encode(header_block);
            }
            Self::Goaway { stream_id } => {
                enc.encode_vvec_with(|enc_inner| {
                    enc_inner.encode_varint(stream_id.as_u64());
                });
            }
            Self::MaxPushId { push_id } => {
                enc.encode_vvec_with(|enc_inner| {
                    enc_inner.encode_varint(*push_id);
                });
            }
            Self::Grease => {
                // Encode some number of random bytes.
                let r = random::<8>();
                enc.encode_vvec(&r[1..usize::from(1 + (r[0] & 0x7))]);
            }
            Self::PriorityUpdateRequest {
                element_id,
                priority,
            }
            | Self::PriorityUpdatePush {
                element_id,
                priority,
            } => {
                enc.encode_vvec_with(|enc_inner| {
                    enc_inner.encode_varint(*element_id);
                    write!(enc_inner, "{priority}").expect("write OK");
                });
            }
        }
    }
}

impl FrameDecoder<Self> for HFrame {
    fn frame_type_allowed(frame_type: HFrameType) -> Res<()> {
        if H3_RESERVED_FRAME_TYPES.contains(&frame_type) {
            return Err(Error::HttpFrameUnexpected);
        }
        Ok(())
    }

    fn decode(frame_type: HFrameType, frame_len: u64, data: Option<&[u8]>) -> Res<Option<Self>> {
        if frame_type == H3_FRAME_TYPE_DATA {
            Ok(Some(Self::Data { len: frame_len }))
        } else if let Some(payload) = data {
            let mut dec = Decoder::from(payload);
            Ok(match frame_type {
                H3_FRAME_TYPE_DATA => unreachable!("DATA frame has been handled already"),
                H3_FRAME_TYPE_HEADERS => Some(Self::Headers {
                    header_block: dec.decode_remainder().to_vec(),
                }),
                H3_FRAME_TYPE_CANCEL_PUSH => Some(Self::CancelPush {
                    push_id: dec.decode_varint().ok_or(Error::HttpFrame)?.into(),
                }),
                H3_FRAME_TYPE_SETTINGS => {
                    let mut settings = HSettings::default();
                    settings.decode_frame_contents(&mut dec).map_err(|e| {
                        if e == Error::HttpSettings {
                            e
                        } else {
                            Error::HttpFrame
                        }
                    })?;
                    Some(Self::Settings { settings })
                }
                H3_FRAME_TYPE_PUSH_PROMISE => Some(Self::PushPromise {
                    push_id: dec.decode_varint().ok_or(Error::HttpFrame)?.into(),
                    header_block: dec.decode_remainder().to_vec(),
                }),
                H3_FRAME_TYPE_GOAWAY => Some(Self::Goaway {
                    stream_id: StreamId::new(dec.decode_varint().ok_or(Error::HttpFrame)?),
                }),
                H3_FRAME_TYPE_MAX_PUSH_ID => Some(Self::MaxPushId {
                    push_id: dec.decode_varint().ok_or(Error::HttpFrame)?.into(),
                }),
                H3_FRAME_TYPE_PRIORITY_UPDATE_REQUEST | H3_FRAME_TYPE_PRIORITY_UPDATE_PUSH => {
                    let element_id = dec.decode_varint().ok_or(Error::HttpFrame)?;
                    let priority = dec.decode_remainder();
                    let priority = Priority::from_bytes(priority)?;
                    if frame_type == H3_FRAME_TYPE_PRIORITY_UPDATE_REQUEST {
                        Some(Self::PriorityUpdateRequest {
                            element_id,
                            priority,
                        })
                    } else {
                        Some(Self::PriorityUpdatePush {
                            element_id,
                            priority,
                        })
                    }
                }
                _ => None,
            })
        } else {
            Ok(None)
        }
    }

    fn is_known_type(frame_type: HFrameType) -> bool {
        matches!(
            frame_type,
            H3_FRAME_TYPE_DATA
                | H3_FRAME_TYPE_HEADERS
                | H3_FRAME_TYPE_CANCEL_PUSH
                | H3_FRAME_TYPE_SETTINGS
                | H3_FRAME_TYPE_PUSH_PROMISE
                | H3_FRAME_TYPE_GOAWAY
                | H3_FRAME_TYPE_MAX_PUSH_ID
                | H3_FRAME_TYPE_PRIORITY_UPDATE_REQUEST
                | H3_FRAME_TYPE_PRIORITY_UPDATE_PUSH
        )
    }
}
