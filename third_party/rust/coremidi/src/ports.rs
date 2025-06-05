use core_foundation::base::OSStatus;
use std::collections::HashMap;
use std::ffi::c_void;
use std::ops::Deref;
use std::ptr;

use coremidi_sys::{
    MIDIObjectRef, MIDIPortConnectSource, MIDIPortDisconnectSource, MIDIPortDispose, MIDIPortRef,
    MIDISend, MIDISendEventList,
};

use crate::endpoints::destinations::Destination;
use crate::endpoints::sources::Source;
use crate::object::Object;
use crate::packets::PacketList;
use crate::{EventBuffer, EventList, PacketBuffer};

pub enum Packets<'a> {
    BorrowedPacketList(&'a PacketList),
    BorrowedEventList(&'a EventList),
    OwnedEventBuffer(EventBuffer),
}

impl<'a> From<&'a PacketList> for Packets<'a> {
    fn from(packet_list: &'a PacketList) -> Self {
        Self::BorrowedPacketList(packet_list)
    }
}

impl<'a> From<&'a PacketBuffer> for Packets<'a> {
    fn from(packet_buffer: &'a PacketBuffer) -> Self {
        Self::BorrowedPacketList(&*packet_buffer)
    }
}

impl<'a> From<&'a EventList> for Packets<'a> {
    fn from(event_list: &'a EventList) -> Self {
        Self::BorrowedEventList(event_list)
    }
}

impl<'a> From<&'a EventBuffer> for Packets<'a> {
    fn from(event_buffer: &'a EventBuffer) -> Self {
        Self::BorrowedEventList(&*event_buffer)
    }
}

impl<'a> From<EventBuffer> for Packets<'a> {
    fn from(event_buffer: EventBuffer) -> Self {
        Self::OwnedEventBuffer(event_buffer)
    }
}

/// A MIDI connection port owned by a client.
/// See [MIDIPortRef](https://developer.apple.com/documentation/coremidi/midiportref).
///
/// Ports can't be instantiated directly, but through a client.
///
#[derive(Debug)]
pub struct Port {
    pub(crate) object: Object,
}

impl Port {
    pub(crate) fn new(port_ref: MIDIPortRef) -> Self {
        Self {
            object: Object(port_ref),
        }
    }
}

impl Deref for Port {
    type Target = Object;

    fn deref(&self) -> &Object {
        &self.object
    }
}

impl Drop for Port {
    fn drop(&mut self) {
        unsafe { MIDIPortDispose(self.object.0) };
    }
}

/// An output [MIDI port](https://developer.apple.com/documentation/coremidi/midiportref) owned by a client.
///
/// A simple example to create an output port and send a MIDI event:
///
/// ```rust,no_run
/// use coremidi::{Client, Destination, EventBuffer, Protocol};
/// let client = Client::new("example-client").unwrap();
/// let output_port = client.output_port("example-port").unwrap();
/// let destination = Destination::from_index(0).unwrap();
/// let events = EventBuffer::new(Protocol::Midi10).with_packet(0, &[0x2090407f]);
/// output_port.send(&destination, &events).unwrap();
/// ```
#[derive(Debug)]
pub struct OutputPort {
    pub(crate) port: Port,
}

impl OutputPort {
    pub(crate) fn new(port_ref: MIDIPortRef) -> Self {
        Self {
            port: Port::new(port_ref),
        }
    }

    /// Send a list of packets to a destination.
    /// See [MIDISendEventList](https://developer.apple.com/documentation/coremidi/3566494-midisendeventlist)
    /// See [MIDISend](https://developer.apple.com/documentation/coremidi/1495289-midisend).
    ///
    pub fn send<'a, P>(&self, destination: &Destination, packets: P) -> Result<(), OSStatus>
    where
        P: Into<Packets<'a>>,
    {
        let status = match packets.into() {
            Packets::BorrowedPacketList(packet_list) => unsafe {
                MIDISend(
                    self.port.object.0,
                    destination.endpoint.object.0,
                    packet_list.as_ptr(),
                )
            },
            Packets::BorrowedEventList(event_list) => unsafe {
                MIDISendEventList(
                    self.port.object.0,
                    destination.endpoint.object.0,
                    event_list.as_ptr(),
                )
            },
            Packets::OwnedEventBuffer(event_buffer) => unsafe {
                MIDISendEventList(
                    self.port.object.0,
                    destination.endpoint.object.0,
                    event_buffer.as_ptr(),
                )
            },
        };
        if status == 0 {
            Ok(())
        } else {
            Err(status)
        }
    }
}

impl Deref for OutputPort {
    type Target = Port;

    fn deref(&self) -> &Port {
        &self.port
    }
}

#[derive(Debug)]
pub struct InputPort {
    pub(crate) port: Port,
}

impl InputPort {
    pub(crate) fn new(port_ref: MIDIPortRef) -> Self {
        Self {
            port: Port::new(port_ref),
        }
    }

    pub fn connect_source(&self, source: &Source) -> Result<(), OSStatus> {
        let status =
            unsafe { MIDIPortConnectSource(self.object.0, source.object.0, ptr::null_mut()) };
        if status == 0 {
            Ok(())
        } else {
            Err(status)
        }
    }

    pub fn disconnect_source(&self, source: &Source) -> Result<(), OSStatus> {
        let status = unsafe { MIDIPortDisconnectSource(self.object.0, source.object.0) };
        if status == 0 {
            Ok(())
        } else {
            Err(status)
        }
    }
}

impl Deref for InputPort {
    type Target = Port;

    fn deref(&self) -> &Port {
        &self.port
    }
}

/// An input [MIDI port](https://developer.apple.com/documentation/coremidi/midiportref) owned by a client.
///
/// A simple example to create an input port:
///
/// ```rust,no_run
/// use coremidi::{Client, Protocol, Source};
/// let client = Client::new("example-client").unwrap();
/// let mut input_port = client.input_port_with_protocol("example-port", Protocol::Midi10, |event_list, context: &mut u32| println!("{:08x}: {:?}", context, event_list)).unwrap();
/// let source = Source::from_index(0).unwrap();
/// let context = source.unique_id().unwrap_or(0);
/// input_port.connect_source(&source, context);
/// ```
#[derive(Debug)]
pub struct InputPortWithContext<T> {
    pub(crate) port: Port,
    pub(crate) contexts: HashMap<MIDIObjectRef, Box<T>>,
}

impl<T> InputPortWithContext<T> {
    pub(crate) fn new(port_ref: MIDIPortRef) -> Self {
        Self {
            port: Port::new(port_ref),
            contexts: HashMap::new(),
        }
    }

    pub fn connect_source(&mut self, source: &Source, context: T) -> Result<(), OSStatus> {
        let mut context = Box::new(context);
        let context_ptr = context.as_mut() as *mut T;
        let status = unsafe {
            MIDIPortConnectSource(self.object.0, source.object.0, context_ptr as *mut c_void)
        };
        if status == 0 {
            self.contexts.insert(source.object.0, context);
            Ok(())
        } else {
            Err(status)
        }
    }

    pub fn disconnect_source(&mut self, source: &Source) -> Result<(), OSStatus> {
        let status = unsafe { MIDIPortDisconnectSource(self.object.0, source.object.0) };
        if status == 0 {
            self.contexts.remove(&source.object.0);
            Ok(())
        } else {
            Err(status)
        }
    }
}

impl<T> Deref for InputPortWithContext<T> {
    type Target = Port;

    fn deref(&self) -> &Port {
        &self.port
    }
}
