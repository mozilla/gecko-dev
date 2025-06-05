use coremidi_sys::MIDIObjectRef;
use std::ops::Deref;

use crate::object::Object;

/// A [MIDI object](https://developer.apple.com/documentation/coremidi/midientityref).
///
/// An entity that a device owns and that contains endpoints.
///
#[derive(Debug, PartialEq)]
pub struct Entity {
    pub(crate) object: Object,
}

impl Entity {
    pub(crate) fn new(object_ref: MIDIObjectRef) -> Self {
        Self {
            object: Object(object_ref),
        }
    }
}

impl Clone for Entity {
    fn clone(&self) -> Self {
        Self::new(self.object.0)
    }
}

impl AsRef<Object> for Entity {
    fn as_ref(&self) -> &Object {
        &self.object
    }
}

impl Deref for Entity {
    type Target = Object;

    fn deref(&self) -> &Object {
        &self.object
    }
}
