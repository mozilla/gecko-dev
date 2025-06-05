use coremidi_sys::MIDIObjectRef;
use std::ops::Deref;

use crate::object::Object;

/// A [MIDI object](https://developer.apple.com/documentation/coremidi/midideviceref).
///
/// A MIDI device or external device, containing entities.
///
#[derive(Debug, PartialEq)]
pub struct Device {
    pub(crate) object: Object,
}

impl Device {
    pub(crate) fn new(object_ref: MIDIObjectRef) -> Self {
        Self {
            object: Object(object_ref),
        }
    }
}

impl Clone for Device {
    fn clone(&self) -> Self {
        Self::new(self.object.0)
    }
}

impl AsRef<Object> for Device {
    fn as_ref(&self) -> &Object {
        &self.object
    }
}

impl Deref for Device {
    type Target = Object;

    fn deref(&self) -> &Object {
        &self.object
    }
}
