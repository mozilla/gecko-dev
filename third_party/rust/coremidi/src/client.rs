use block::RcBlock;
use core_foundation::{
    base::{OSStatus, TCFType},
    string::CFString,
};
use std::cell::RefCell;
use std::{mem::MaybeUninit, ops::Deref, os::raw::c_void, ptr};

use coremidi_sys::{
    MIDIClientCreate, MIDIClientCreateWithBlock, MIDIClientDispose, MIDIDestinationCreateWithBlock,
    MIDIDestinationCreateWithProtocol, MIDIEventList, MIDIInputPortCreateWithBlock,
    MIDIInputPortCreateWithProtocol, MIDINotification, MIDINotifyBlock, MIDIOutputPortCreate,
    MIDIPacketList, MIDIReadBlock, MIDIReceiveBlock, MIDISourceCreate,
};

use crate::ports::InputPortWithContext;
use crate::{
    endpoints::{destinations::VirtualDestination, sources::VirtualSource},
    notifications::Notification,
    object::Object,
    packets::PacketList,
    ports::{InputPort, OutputPort},
    result_from_status, EventList, Protocol,
};

pub enum NotifyCallback {
    ByReference(RefCell<Box<dyn FnMut(&Notification) + Send + 'static>>),
    ByOwnership(RefCell<Box<dyn FnMut(Notification) + Send + 'static>>),
}

impl NotifyCallback {
    pub fn by_reference<F>(callback: F) -> Self
    where
        F: FnMut(&Notification) + Send + 'static,
    {
        Self::ByReference(RefCell::new(Box::new(callback)))
    }

    pub fn by_ownership<F>(callback: F) -> Self
    where
        F: FnMut(Notification) + Send + 'static,
    {
        Self::ByOwnership(RefCell::new(Box::new(callback)))
    }
}

impl<F> From<F> for NotifyCallback
where
    F: FnMut(&Notification) + Send + 'static,
{
    fn from(callback: F) -> Self {
        Self::by_reference(callback)
    }
}

/// A [MIDI client](https://developer.apple.com/documentation/coremidi/midiclientref).
///
/// An object maintaining per-client state.
///
/// A simple example to create a Client:
///
/// ```rust,no_run
/// let client = coremidi::Client::new("example-client").unwrap();
/// ```
#[derive(Debug)]
pub struct Client {
    object: Object,
}

impl Client {
    /// Creates a new CoreMIDI client with support for notifications.
    /// See [MIDIClientCreateWithBlock](https://developer.apple.com/documentation/coremidi/1495330-midiclientcreatewithblock).
    ///
    /// The notification callback will be called on the [run loop](https://developer.apple.com/library/archive/documentation/Cocoa/Conceptual/Multithreading/RunLoopManagement/RunLoopManagement.html)
    /// that was current when this associated function is called.
    ///
    /// It follows that this particular run loop needs to be running in order to
    /// actually receive notifications. The run loop can be started after the
    /// client has been created if need be. Please see the examples to know how.
    ///
    pub fn new_with_notifications<F>(name: &str, callback: F) -> Result<Client, OSStatus>
    where
        F: Into<NotifyCallback>,
    {
        let client_name = CFString::new(name);
        let mut client_ref = MaybeUninit::uninit();
        let notify_block = Self::notify_block(callback.into());
        let status = unsafe {
            MIDIClientCreateWithBlock(
                client_name.as_concrete_TypeRef(),
                client_ref.as_mut_ptr(),
                notify_block.deref() as *const _ as MIDINotifyBlock,
            )
        };
        result_from_status(status, || {
            let client_ref = unsafe { client_ref.assume_init() };
            Client {
                object: Object(client_ref),
            }
        })
    }

    /// Creates a new CoreMIDI client.
    /// See [MIDIClientCreate](https://developer.apple.com/documentation/coremidi/1495360-midiclientcreate).
    ///
    pub fn new(name: &str) -> Result<Client, OSStatus> {
        let client_name = CFString::new(name);
        let mut client_ref = MaybeUninit::uninit();
        let status = unsafe {
            MIDIClientCreate(
                client_name.as_concrete_TypeRef(),
                None,
                ptr::null_mut(),
                client_ref.as_mut_ptr(),
            )
        };
        result_from_status(status, || {
            let client_ref = unsafe { client_ref.assume_init() };
            Client {
                object: Object(client_ref),
            }
        })
    }

    /// Creates an output port through which the client may send outgoing MIDI messages to any MIDI destination.
    /// See [MIDIOutputPortCreate](https://developer.apple.com/documentation/coremidi/1495166-midioutputportcreate).
    ///
    pub fn output_port(&self, name: &str) -> Result<OutputPort, OSStatus> {
        let port_name = CFString::new(name);
        let mut port_ref = MaybeUninit::uninit();
        let status = unsafe {
            MIDIOutputPortCreate(
                self.object.0,
                port_name.as_concrete_TypeRef(),
                port_ref.as_mut_ptr(),
            )
        };
        result_from_status(status, || {
            let port_ref = unsafe { port_ref.assume_init() };
            OutputPort::new(port_ref)
        })
    }

    /// Creates an input port through which the client may receive incoming MIDI 1.0 messages from any MIDI source.
    /// See [MIDIInputPortCreate](https://developer.apple.com/documentation/coremidi/1495225-midiinputportcreate).
    ///
    /// **MIDIInputPortCreate is deprecated in CoreMIDI. Please consider using `input_port_with_protocol` instead**
    ///
    pub fn input_port<F>(&self, name: &str, callback: F) -> Result<InputPort, OSStatus>
    where
        F: FnMut(&PacketList) + Send + 'static,
    {
        let port_name = CFString::new(name);
        let mut port_ref = MaybeUninit::uninit();
        let read_block = Self::read_block(callback);
        let status = unsafe {
            MIDIInputPortCreateWithBlock(
                self.object.0,
                port_name.as_concrete_TypeRef(),
                port_ref.as_mut_ptr(),
                read_block.deref() as *const _ as MIDIReadBlock,
            )
        };
        result_from_status(status, || {
            let port_ref = unsafe { port_ref.assume_init() };
            InputPort::new(port_ref)
        })
    }

    /// Creates an input port through which the client may receive incoming MIDI messages from any MIDI source.
    /// It allows to choose which MIDI [Protocol] to use.
    /// See [MIDIInputPortCreateWithProtocol](https://developer.apple.com/documentation/coremidi/3566488-midiinputportcreatewithprotocol).
    ///
    pub fn input_port_with_protocol<T, F>(
        &self,
        name: &str,
        protocol: Protocol,
        callback: F,
    ) -> Result<InputPortWithContext<T>, OSStatus>
    where
        F: FnMut(&EventList, &mut T) + Send + 'static,
    {
        let port_name = CFString::new(name);
        let mut port_ref = MaybeUninit::uninit();
        let receive_block = Self::receive_block::<T, _>(callback);
        let status = unsafe {
            MIDIInputPortCreateWithProtocol(
                self.object.0,
                port_name.as_concrete_TypeRef(),
                protocol.into(),
                port_ref.as_mut_ptr(),
                receive_block.deref() as *const _ as MIDIReceiveBlock,
            )
        };
        result_from_status(status, || {
            let port_ref = unsafe { port_ref.assume_init() };
            InputPortWithContext::<T>::new(port_ref)
        })
    }

    /// Creates a virtual source in the client.
    /// See [MIDISourceCreate](https://developer.apple.com/documentation/coremidi/1495212-midisourcecreate).
    ///
    pub fn virtual_source(&self, name: &str) -> Result<VirtualSource, OSStatus> {
        let virtual_source_name = CFString::new(name);
        let mut virtual_source = MaybeUninit::uninit();
        let status = unsafe {
            MIDISourceCreate(
                self.object.0,
                virtual_source_name.as_concrete_TypeRef(),
                virtual_source.as_mut_ptr(),
            )
        };
        result_from_status(status, || {
            let endpoint_ref = unsafe { virtual_source.assume_init() };
            VirtualSource::new(endpoint_ref)
        })
    }

    /// Creates a virtual destination in the client.
    /// See [MIDIDestinationCreate](https://developer.apple.com/documentation/coremidi/1495347-mididestinationcreate).
    ///
    /// **MIDIDestinationCreate is deprecated in CoreMIDI. Please consider using `virtual_destination_with_protocol` instead**
    ///
    pub fn virtual_destination<F>(
        &self,
        name: &str,
        callback: F,
    ) -> Result<VirtualDestination, OSStatus>
    where
        F: FnMut(&PacketList) + Send + 'static,
    {
        let virtual_destination_name = CFString::new(name);
        let mut virtual_destination = MaybeUninit::uninit();
        let read_block = Self::read_block(callback);
        let status = unsafe {
            MIDIDestinationCreateWithBlock(
                self.object.0,
                virtual_destination_name.as_concrete_TypeRef(),
                virtual_destination.as_mut_ptr(),
                read_block.deref() as *const _ as MIDIReadBlock,
            )
        };
        result_from_status(status, || {
            let endpoint_ref = unsafe { virtual_destination.assume_init() };
            VirtualDestination::new(endpoint_ref)
        })
    }

    /// Creates a virtual destination in the client.
    /// It allows to choose which MIDI [Protocol] to use.
    /// See [MIDIDestinationCreate](https://developer.apple.com/documentation/coremidi/1495347-mididestinationcreate).
    ///
    pub fn virtual_destination_with_protocol<F>(
        &self,
        name: &str,
        protocol: Protocol,
        mut callback: F,
    ) -> Result<VirtualDestination, OSStatus>
    where
        F: FnMut(&EventList) + Send + 'static,
    {
        let virtual_destination_name = CFString::new(name);
        let mut virtual_destination = MaybeUninit::uninit();
        let receive_block =
            Self::receive_block::<(), _>(move |event_list, _| (callback)(event_list));
        let status = unsafe {
            MIDIDestinationCreateWithProtocol(
                self.object.0,
                virtual_destination_name.as_concrete_TypeRef(),
                protocol.into(),
                virtual_destination.as_mut_ptr(),
                receive_block.deref() as *const _ as MIDIReceiveBlock,
            )
        };
        result_from_status(status, || {
            let endpoint_ref = unsafe { virtual_destination.assume_init() };
            VirtualDestination::new(endpoint_ref)
        })
    }

    fn notify_block(callback: NotifyCallback) -> RcBlock<(*const MIDINotification,), ()> {
        let notify_block = block::ConcreteBlock::new(move |message: *const MIDINotification| {
            let message = unsafe { &*message };
            if let Ok(notification) = Notification::try_from(message) {
                match &callback {
                    NotifyCallback::ByReference(f) => (f.borrow_mut())(&notification),
                    NotifyCallback::ByOwnership(f) => (f.borrow_mut())(notification),
                }
            }
        });
        notify_block.copy()
    }

    fn read_block<F>(callback: F) -> RcBlock<(*const MIDIPacketList, *mut c_void), ()>
    where
        F: FnMut(&PacketList) + Send + 'static,
    {
        let callback = RefCell::new(callback);
        let read_block = block::ConcreteBlock::new(
            move |pktlist: *const MIDIPacketList, _src_conn_ref_con: *mut c_void| {
                let packet_list = unsafe { &*(pktlist as *const PacketList) };
                (callback.borrow_mut())(packet_list);
            },
        );
        read_block.copy()
    }

    fn receive_block<T, F>(callback: F) -> RcBlock<(*const MIDIEventList, *mut c_void), ()>
    where
        F: FnMut(&EventList, &mut T) + Send + 'static,
    {
        let callback = RefCell::new(callback);
        let receive_block = block::ConcreteBlock::new(
            move |evtlist: *const MIDIEventList, src_conn_ref_con: *mut c_void| {
                let event_list = unsafe { &*(evtlist as *const EventList) };
                let context = unsafe { &mut *(src_conn_ref_con as *mut T) };
                (callback.borrow_mut())(event_list, context);
            },
        );
        receive_block.copy()
    }
}

impl Deref for Client {
    type Target = Object;

    fn deref(&self) -> &Object {
        &self.object
    }
}

impl Drop for Client {
    fn drop(&mut self) {
        unsafe { MIDIClientDispose(self.object.0) };
    }
}
