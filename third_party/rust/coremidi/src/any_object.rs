use coremidi_sys::{MIDIObjectRef, MIDIObjectType};

use crate::{Destination, Device, Entity, Object, Source};

#[derive(Debug, PartialEq)]
pub enum AnyObject {
    Other(Object),
    Device(Device),
    Entity(Entity),
    Source(Source),
    Destination(Destination),
    ExternalDevice(Device),
    ExternalEntity(Entity),
    ExternalSource(Source),
    ExternalDestination(Destination),
}

impl AnyObject {
    pub(crate) fn create(object_type: MIDIObjectType, object_ref: MIDIObjectRef) -> Option<Self> {
        match object_type {
            coremidi_sys::kMIDIObjectType_Other => Some(Self::Other(Object(object_ref))),
            coremidi_sys::kMIDIObjectType_Device => Some(Self::Device(Device::new(object_ref))),
            coremidi_sys::kMIDIObjectType_Entity => Some(Self::Entity(Entity::new(object_ref))),
            coremidi_sys::kMIDIObjectType_Source => Some(Self::Source(Source::new(object_ref))),
            coremidi_sys::kMIDIObjectType_Destination => {
                Some(Self::Destination(Destination::new(object_ref)))
            }
            coremidi_sys::kMIDIObjectType_ExternalDevice => {
                Some(Self::ExternalDevice(Device::new(object_ref)))
            }
            coremidi_sys::kMIDIObjectType_ExternalEntity => {
                Some(Self::ExternalEntity(Entity::new(object_ref)))
            }
            coremidi_sys::kMIDIObjectType_ExternalSource => {
                Some(Self::ExternalSource(Source::new(object_ref)))
            }
            coremidi_sys::kMIDIObjectType_ExternalDestination => {
                Some(Self::ExternalDestination(Destination::new(object_ref)))
            }
            _ => None,
        }
    }
}

impl AsRef<Object> for AnyObject {
    fn as_ref(&self) -> &Object {
        match self {
            Self::Other(object) => object,
            Self::Device(device) => &device.object,
            Self::Entity(entity) => &entity.object,
            Self::Source(source) => &source.object,
            Self::Destination(destination) => &destination.object,
            Self::ExternalDevice(device) => &device.object,
            Self::ExternalEntity(entity) => &entity.object,
            Self::ExternalSource(source) => &source.object,
            Self::ExternalDestination(destination) => &destination.object,
        }
    }
}

#[cfg(test)]
mod tests {
    use crate::any_object::AnyObject;
    use crate::{Destination, Device, Entity, Object, Source};

    #[test]
    fn any_object_create() {
        assert_eq!(
            AnyObject::create(coremidi_sys::kMIDIObjectType_Other, 1),
            Some(AnyObject::Other(Object(1)))
        );
        assert_eq!(
            AnyObject::create(coremidi_sys::kMIDIObjectType_Device, 1),
            Some(AnyObject::Device(Device::new(1)))
        );
        assert_eq!(
            AnyObject::create(coremidi_sys::kMIDIObjectType_Entity, 1),
            Some(AnyObject::Entity(Entity::new(1)))
        );
        assert_eq!(
            AnyObject::create(coremidi_sys::kMIDIObjectType_Source, 1),
            Some(AnyObject::Source(Source::new(1)))
        );
        assert_eq!(
            AnyObject::create(coremidi_sys::kMIDIObjectType_Destination, 1),
            Some(AnyObject::Destination(Destination::new(1)))
        );
        assert_eq!(
            AnyObject::create(coremidi_sys::kMIDIObjectType_ExternalDevice, 1),
            Some(AnyObject::ExternalDevice(Device::new(1)))
        );
        assert_eq!(
            AnyObject::create(coremidi_sys::kMIDIObjectType_ExternalEntity, 1),
            Some(AnyObject::ExternalEntity(Entity::new(1)))
        );
        assert_eq!(
            AnyObject::create(coremidi_sys::kMIDIObjectType_ExternalSource, 1),
            Some(AnyObject::ExternalSource(Source::new(1)))
        );
        assert_eq!(
            AnyObject::create(coremidi_sys::kMIDIObjectType_ExternalDestination, 1),
            Some(AnyObject::ExternalDestination(Destination::new(1)))
        );
    }

    #[test]
    fn any_object_as_ref() {
        let expected_object = Object(1);
        assert_eq!(AnyObject::Other(Object(1)).as_ref(), &expected_object);
        assert_eq!(AnyObject::Device(Device::new(1)).as_ref(), &expected_object);
        assert_eq!(AnyObject::Entity(Entity::new(1)).as_ref(), &expected_object);
        assert_eq!(AnyObject::Source(Source::new(1)).as_ref(), &expected_object);
        assert_eq!(
            AnyObject::Destination(Destination::new(1)).as_ref(),
            &expected_object
        );
        assert_eq!(
            AnyObject::ExternalDevice(Device::new(1)).as_ref(),
            &expected_object
        );
        assert_eq!(
            AnyObject::ExternalEntity(Entity::new(1)).as_ref(),
            &expected_object
        );
        assert_eq!(
            AnyObject::ExternalSource(Source::new(1)).as_ref(),
            &expected_object
        );
        assert_eq!(
            AnyObject::ExternalDestination(Destination::new(1)).as_ref(),
            &expected_object
        );
    }

    #[test]
    fn any_object_from_error() {
        assert_eq!(AnyObject::create(0xffff_i32, 1), None);
    }
}
