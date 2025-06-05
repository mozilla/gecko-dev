use std::fmt::Formatter;

use coremidi_sys::MIDIProtocolID;

/// The [MIDI Protocol](https://developer.apple.com/documentation/coremidi/midiprotocolid) to use for messages
///
#[derive(Clone, Copy, PartialEq)]
pub enum Protocol {
    /// MIDI 1.0
    Midi10,

    /// MIDI 2.0
    Midi20,

    /// Reserved for future protocols not known by this crate yet
    /// Please don't use it, unless really needed.
    Unknown(MIDIProtocolID),
}

impl From<MIDIProtocolID> for Protocol {
    fn from(protocol_id: MIDIProtocolID) -> Self {
        match protocol_id as ::std::os::raw::c_uint {
            coremidi_sys::kMIDIProtocol_1_0 => Protocol::Midi10,
            coremidi_sys::kMIDIProtocol_2_0 => Protocol::Midi20,
            _ => Protocol::Unknown(protocol_id),
        }
    }
}

impl From<Protocol> for MIDIProtocolID {
    fn from(protocol: Protocol) -> Self {
        match protocol {
            Protocol::Midi10 => coremidi_sys::kMIDIProtocol_1_0 as MIDIProtocolID,
            Protocol::Midi20 => coremidi_sys::kMIDIProtocol_2_0 as MIDIProtocolID,
            Protocol::Unknown(protocol_id) => protocol_id,
        }
    }
}

impl std::fmt::Debug for Protocol {
    fn fmt(&self, f: &mut Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Midi10 => write!(f, "MIDI 1.0"),
            Self::Midi20 => write!(f, "MIDI 2.0"),
            Self::Unknown(protocol_id) => write!(f, "Unknown({})", protocol_id),
        }
    }
}
