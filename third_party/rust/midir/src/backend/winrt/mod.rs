use std::sync::{Arc, Mutex};
use std::convert::TryInto;

use ::errors::*;
use ::Ignore;

use windows::core::{Interface, HSTRING};

use windows::{
    Foundation::{TypedEventHandler, EventRegistrationToken, IClosable, IMemoryBufferReference},
    Devices::Midi::*,
    Devices::Enumeration::DeviceInformation,
    Storage::Streams::{Buffer, DataWriter},
    Win32::System::WinRT::IMemoryBufferByteAccess,
};

// see also https://github.com/microsoft/windows-rs/blob/master/examples/buffer/src/main.rs
fn buffer_get_bytes(buffer: &IMemoryBufferReference) -> ::windows::core::Result<&[u8]> {
    let interop: IMemoryBufferByteAccess = buffer.cast::<IMemoryBufferByteAccess>()?;
    let mut bufptr = std::ptr::null_mut();
    let mut capacity: u32 = 0;
    unsafe { // TODO: somehow make sure that the buffer is not invalidated while we're reading from it ...
        interop.GetBuffer(&mut bufptr, &mut capacity)?;
        if capacity == 0 {
            bufptr = 1 as *mut u8; // null pointer is not allowed
        }
        Ok(std::slice::from_raw_parts(bufptr, capacity as usize))
    }
}

#[derive(Clone, PartialEq)]
pub struct MidiInputPort {
    id: HSTRING
}

pub struct MidiInput {
    selector: HSTRING,
    ignore_flags: Ignore
}

impl MidiInput {
    pub fn new(_client_name: &str) -> Result<Self, InitError> {
        let device_selector = MidiInPort::GetDeviceSelector().map_err(|_| InitError)?;
        Ok(MidiInput { selector: device_selector, ignore_flags: Ignore::None })
    }

    pub fn ignore(&mut self, flags: Ignore) {
        self.ignore_flags = flags;
    }

    pub(crate) fn ports_internal(&self) -> Vec<::common::MidiInputPort> {
        let device_collection = DeviceInformation::FindAllAsyncAqsFilter(&self.selector).unwrap().get().expect("FindAllAsyncAqsFilter failed");
        let count = device_collection.Size().expect("Size failed") as usize;
        let mut result = Vec::with_capacity(count as usize);
        for device_info in device_collection.into_iter() {
            let device_id = device_info.Id().expect("Id failed");
            result.push(::common::MidiInputPort {
                imp: MidiInputPort { id: device_id }
            });
        }
        result
    }
    
    pub fn port_count(&self) -> usize {
        let device_collection = DeviceInformation::FindAllAsyncAqsFilter(&self.selector).unwrap().get().expect("FindAllAsyncAqsFilter failed");
        device_collection.Size().expect("Size failed") as usize
    }
    
    pub fn port_name(&self, port: &MidiInputPort) -> Result<String, PortInfoError> {
        let device_info_async = DeviceInformation::CreateFromIdAsync(&port.id).map_err(|_| PortInfoError::InvalidPort)?;
        let device_info = device_info_async.get().map_err(|_| PortInfoError::InvalidPort)?;
        let device_name = device_info.Name().map_err(|_| PortInfoError::CannotRetrievePortName)?;
        Ok(device_name.to_string())
    }

    fn handle_input<T>(args: &MidiMessageReceivedEventArgs, handler_data: &mut HandlerData<T>) {
        let ignore = handler_data.ignore_flags;
        let data = &mut handler_data.user_data.as_mut().unwrap();
        let message = args.Message().expect("Message failed");
        let timestamp = message.Timestamp().expect("Timestamp failed").Duration as u64 / 10;
        let buffer = message.RawData().expect("RawData failed");
        let membuffer = Buffer::CreateMemoryBufferOverIBuffer(&buffer).expect("CreateMemoryBufferOverIBuffer failed");
        let memory_reference: IMemoryBufferReference = membuffer.CreateReference().expect("CreateReference failed");
        let message_bytes = buffer_get_bytes(&memory_reference).expect("buffer_get_bytes failed");

        // The first byte in the message is the status
        let status = message_bytes[0];

        if !(status == 0xF0 && ignore.contains(Ignore::Sysex) ||
             status == 0xF1 && ignore.contains(Ignore::Time) ||
             status == 0xF8 && ignore.contains(Ignore::Time) ||
             status == 0xFE && ignore.contains(Ignore::ActiveSense))
        {
            (handler_data.callback)(timestamp, message_bytes, data);
        }
    }

    pub fn connect<F, T: Send + 'static>(
        self, port: &MidiInputPort, _port_name: &str, callback: F, data: T
    ) -> Result<MidiInputConnection<T>, ConnectError<MidiInput>>
        where F: FnMut(u64, &[u8], &mut T) + Send + 'static {
        
        let in_port = match MidiInPort::FromIdAsync(&port.id) {
            Ok(port_async) => match port_async.get() {
                Ok(port) => port,
                _ => return Err(ConnectError::new(ConnectErrorKind::InvalidPort, self))
            }
            Err(_) => return Err(ConnectError::new(ConnectErrorKind::InvalidPort, self))
        };
        
        let handler_data = Arc::new(Mutex::new(HandlerData {
            ignore_flags: self.ignore_flags,
            callback: Box::new(callback),
            user_data: Some(data)
        }));
        let handler_data2 = handler_data.clone();

        type Handler = TypedEventHandler<MidiInPort, MidiMessageReceivedEventArgs>;
        let handler = Handler::new(move |_sender, args: &Option<MidiMessageReceivedEventArgs>| {
            MidiInput::handle_input(args.as_ref().expect("MidiMessageReceivedEventArgs were null"), &mut *handler_data2.lock().unwrap());
            Ok(())
        });
        let event_token = in_port.MessageReceived(&handler).expect("MessageReceived failed");

        Ok(MidiInputConnection { port: RtMidiInPort(in_port), event_token: event_token, handler_data: handler_data })
    }
}

struct RtMidiInPort(MidiInPort);
unsafe impl Send for RtMidiInPort {}

pub struct MidiInputConnection<T> {
    port: RtMidiInPort,
    event_token: EventRegistrationToken,
    // TODO: get rid of Arc & Mutex?
    //       synchronization is required because the borrow checker does not
    //       know that the callback we're in here is never called concurrently
    //       (always in sequence)
    handler_data: Arc<Mutex<HandlerData<T>>>
}


impl<T> MidiInputConnection<T> {
    pub fn close(self) -> (MidiInput, T) {
        let _ = self.port.0.RemoveMessageReceived(self.event_token);
        let closable: IClosable = self.port.0.try_into().unwrap();
        let _ = closable.Close();
        let device_selector = MidiInPort::GetDeviceSelector().expect("GetDeviceSelector failed"); // probably won't ever fail here, because it worked previously
        let mut handler_data_locked = self.handler_data.lock().unwrap();
        (MidiInput {
            selector: device_selector,
            ignore_flags: handler_data_locked.ignore_flags
        }, handler_data_locked.user_data.take().unwrap())
    }
}

/// This is all the data that is stored on the heap as long as a connection
/// is opened and passed to the callback handler.
///
/// It is important that `user_data` is the last field to not influence
/// offsets after monomorphization.
struct HandlerData<T> {
    ignore_flags: Ignore,
    callback: Box<dyn FnMut(u64, &[u8], &mut T) + Send>,
    user_data: Option<T>
}

#[derive(Clone, PartialEq)]
pub struct MidiOutputPort {
    id: HSTRING
}

pub struct MidiOutput {
    selector: HSTRING
}

impl MidiOutput {
    pub fn new(_client_name: &str) -> Result<Self, InitError> {
        let device_selector = MidiOutPort::GetDeviceSelector().map_err(|_| InitError)?;
        Ok(MidiOutput { selector: device_selector })
    }

    pub(crate) fn ports_internal(&self) -> Vec<::common::MidiOutputPort> {
        let device_collection = DeviceInformation::FindAllAsyncAqsFilter(&self.selector).unwrap().get().expect("FindAllAsyncAqsFilter failed");
        let count = device_collection.Size().expect("Size failed") as usize;
        let mut result = Vec::with_capacity(count as usize);
        for device_info in device_collection.into_iter() {
            let device_id = device_info.Id().expect("Id failed");
            result.push(::common::MidiOutputPort {
                imp: MidiOutputPort { id: device_id }
            });
        }
        result
    }
    
    pub fn port_count(&self) -> usize {
        let device_collection = DeviceInformation::FindAllAsyncAqsFilter(&self.selector).unwrap().get().expect("FindAllAsyncAqsFilter failed");
        device_collection.Size().expect("Size failed") as usize
    }
    
    pub fn port_name(&self, port: &MidiOutputPort) -> Result<String, PortInfoError> {
        let device_info_async = DeviceInformation::CreateFromIdAsync(&port.id).map_err(|_| PortInfoError::InvalidPort)?;
        let device_info = device_info_async.get().map_err(|_| PortInfoError::InvalidPort)?;
        let device_name = device_info.Name().map_err(|_| PortInfoError::CannotRetrievePortName)?;
        Ok(device_name.to_string())
    }
    
    pub fn connect(self, port: &MidiOutputPort, _port_name: &str) -> Result<MidiOutputConnection, ConnectError<MidiOutput>> {        
        let out_port = match MidiOutPort::FromIdAsync(&port.id) {
            Ok(port_async) => match port_async.get() {
                Ok(port) => port,
                _ => return Err(ConnectError::new(ConnectErrorKind::InvalidPort, self))
            }
            Err(_) => return Err(ConnectError::new(ConnectErrorKind::InvalidPort, self))
        };
        Ok(MidiOutputConnection { port: out_port })
    }
}

pub struct MidiOutputConnection {
    port: IMidiOutPort
}

unsafe impl Send for MidiOutputConnection {}

impl MidiOutputConnection {
    pub fn close(self) -> MidiOutput {
        let closable: IClosable = self.port.try_into().unwrap();
        let _ = closable.Close();
        let device_selector = MidiOutPort::GetDeviceSelector().expect("GetDeviceSelector failed"); // probably won't ever fail here, because it worked previously
        MidiOutput { selector: device_selector }
    }
    
    pub fn send(&mut self, message: &[u8]) -> Result<(), SendError> {
        let data_writer = DataWriter::new().unwrap();
        data_writer.WriteBytes(message).map_err(|_| SendError::Other("WriteBytes failed"))?;
        let buffer = data_writer.DetachBuffer().map_err(|_| SendError::Other("DetachBuffer failed"))?;
        self.port.SendBuffer(&buffer).map_err(|_| SendError::Other("SendBuffer failed"))?;
        Ok(())
    }
}
