/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

use num_derive::{FromPrimitive, ToPrimitive};
use num_traits::FromPrimitive;
use std::{
    ffi::{CString, OsString},
    mem::size_of,
};
#[cfg(target_os = "windows")]
use windows_sys::Win32::System::Diagnostics::Debug::{CONTEXT, EXCEPTION_RECORD};

use crate::{
    breakpad::{AncillaryData, BreakpadData, Pid},
    errors::MessageError,
    BreakpadString,
};

#[repr(u8)]
#[derive(Copy, Clone, Debug, FromPrimitive, ToPrimitive, PartialEq)]
pub enum Kind {
    /// Passes the socket used by the breakpad crash generation server as well
    /// as the path where minidumps will be generated. The message type is
    /// followed by a usize integer containing the length of the path, followed
    /// by the actual path as a byte string (with no NUL-terminator). On Linux
    ///  the file descriptor of the Breakpad server socket is passed along in an
    /// SCM_RIGHTS ancillary message. On Windows the Breakpad crash generation
    /// server pipe name is passed after the path.
    Initialize = 1,
    InitializeReply = 2,
    /// Request the transfer of an already generated minidump for the specified
    /// PID back to the client. The message type is followed by a 32-bit
    /// integer containing the PID.
    TransferMinidump = 3,
    TransferMinidumpReply = 4,
    /// Request the generation of a minidump of the sending process. The
    /// sending process' PID will be retrieved from the socket itself.
    GenerateMinidump = 5,
    GenerateMinidumpReply = 6,
    /// Request the generation of a minidump based on data obtained via the
    /// Windows Error Reporting runtime exception module. The reply is empty
    /// and only used to inform the WER module that it's time to shut down the
    /// crashed process. This is only enabled on Windows.
    #[cfg(target_os = "windows")]
    WindowsErrorReporting = 7,
    #[cfg(target_os = "windows")]
    WindowsErrorReportingReply = 8,
}

pub trait Message {
    fn kind() -> Kind
    where
        Self: Sized;
    fn header(&self) -> Vec<u8>;
    fn payload(&self) -> Vec<u8>;
    fn ancillary_payload(&self) -> Option<AncillaryData>;
    fn decode(data: &[u8], ancillary_data: Option<AncillaryData>) -> Result<Self, MessageError>
    where
        Self: Sized;
}

/* Message header, all messages are prefixed with this. The header is sent as
 * a single message over the underlying transport and contains the size of the
 * message payload as well as the type of the message. This allows the receiver
 * to validate and prepare for the reception of the payload. */

pub const HEADER_SIZE: usize = size_of::<Kind>() + size_of::<usize>();

pub struct Header {
    pub kind: Kind,
    pub size: usize,
}

impl Header {
    fn encode(&self) -> Vec<u8> {
        let mut buffer = Vec::with_capacity(HEADER_SIZE);
        buffer.push(self.kind as u8);
        buffer.extend(&self.size.to_ne_bytes());
        debug_assert!(buffer.len() == HEADER_SIZE, "Header size mismatch");
        buffer
    }

    pub fn decode(buffer: &[u8]) -> Result<Header, MessageError> {
        let kind = buffer.first().ok_or(MessageError::Truncated)?;
        let kind = Kind::from_u8(*kind).ok_or(MessageError::InvalidKind)?;
        let size_bytes: [u8; size_of::<usize>()] =
            buffer[size_of::<Kind>()..size_of::<Kind>() + size_of::<usize>()].try_into()?;
        let size = usize::from_ne_bytes(size_bytes);

        Ok(Header { kind, size })
    }
}

/* Initialization message, used to start the crash generator */
pub struct Initialize {
    pub path: OsString,
    pub breakpad_data: BreakpadData,
    pub release_channel: String,
}

impl Initialize {
    pub fn new(path: OsString, breakpad_data: BreakpadData, release_channel: &str) -> Initialize {
        Initialize {
            path,
            breakpad_data,
            release_channel: release_channel.to_owned(),
        }
    }

    fn payload_size(&self) -> usize {
        let path_len = self.path.serialize().len();
        let breakpad_data_len = self.breakpad_data.serialize().len();
        let release_channel_len = self.release_channel.len();
        (size_of::<usize>() * 3) + path_len + breakpad_data_len + release_channel_len
    }
}

impl Message for Initialize {
    fn kind() -> Kind {
        Kind::Initialize
    }

    fn header(&self) -> Vec<u8> {
        Header {
            kind: Self::kind(),
            size: self.payload_size(),
        }
        .encode()
    }

    fn payload(&self) -> Vec<u8> {
        let mut payload = Vec::with_capacity(self.payload_size());
        let path = self.path.serialize();
        let breakpad_data = self.breakpad_data.serialize();
        payload.extend(path.len().to_ne_bytes());
        payload.extend(breakpad_data.len().to_ne_bytes());
        payload.extend(self.release_channel.len().to_ne_bytes());
        payload.extend(self.path.serialize());
        payload.extend(self.breakpad_data.serialize());
        payload.extend(self.release_channel.as_bytes());
        payload
    }

    fn ancillary_payload(&self) -> Option<AncillaryData> {
        self.breakpad_data.ancillary()
    }

    fn decode(
        data: &[u8],
        ancillary_data: Option<AncillaryData>,
    ) -> Result<Initialize, MessageError> {
        let path_len_bytes: [u8; size_of::<usize>()] = data[0..size_of::<usize>()].try_into()?;
        let path_len = usize::from_ne_bytes(path_len_bytes);
        let offset = size_of::<usize>();

        let breakpad_data_len_bytes: [u8; size_of::<usize>()] =
            data[offset..offset + size_of::<usize>()].try_into()?;
        let breakpad_data_len = usize::from_ne_bytes(breakpad_data_len_bytes);
        let offset = offset + size_of::<usize>();

        let release_channel_len_bytes: [u8; size_of::<usize>()] =
            data[offset..offset + size_of::<usize>()].try_into()?;
        let release_channel_len = usize::from_ne_bytes(release_channel_len_bytes);
        let offset = offset + size_of::<usize>();

        // TODO: This needs better error handling.
        let path = <OsString as BreakpadString>::deserialize(&data[offset..offset + path_len])
            .map_err(|_| MessageError::InvalidData)?;
        let offset = offset + path_len;
        let breakpad_data = BreakpadData::from_slice_with_ancillary(
            &data[offset..offset + breakpad_data_len],
            ancillary_data,
        )
        .map_err(|_| MessageError::InvalidData)?;
        let offset = offset + breakpad_data_len;
        let release_channel =
            String::from_utf8_lossy(&data[offset..offset + release_channel_len]).into_owned();

        Ok(Initialize {
            path,
            breakpad_data,
            release_channel,
        })
    }
}

/* Initialize reply, received from the server after having sent an Initialize message.
 * Confirms that the server is ready to operate. */

pub struct InitializeReply {}

impl Default for InitializeReply {
    fn default() -> Self {
        Self::new()
    }
}

impl InitializeReply {
    pub fn new() -> InitializeReply {
        InitializeReply {}
    }

    fn payload_size(&self) -> usize {
        0
    }
}

impl Message for InitializeReply {
    fn kind() -> Kind {
        Kind::InitializeReply
    }

    fn header(&self) -> Vec<u8> {
        Header {
            kind: Self::kind(),
            size: self.payload_size(),
        }
        .encode()
    }

    fn payload(&self) -> Vec<u8> {
        Vec::<u8>::new()
    }

    fn ancillary_payload(&self) -> Option<AncillaryData> {
        None
    }

    fn decode(
        data: &[u8],
        ancillary_data: Option<AncillaryData>,
    ) -> Result<InitializeReply, MessageError> {
        if ancillary_data.is_some() || !data.is_empty() {
            return Err(MessageError::InvalidData);
        }

        Ok(InitializeReply::new())
    }
}

/* Transfer minidump message, used to request the minidump which has been
 * generated for the specified pid. */

pub struct TransferMinidump {
    pub pid: Pid,
}

impl TransferMinidump {
    pub fn new(pid: Pid) -> TransferMinidump {
        TransferMinidump { pid }
    }
}

impl Message for TransferMinidump {
    fn kind() -> Kind {
        Kind::TransferMinidump
    }

    fn header(&self) -> Vec<u8> {
        Header {
            kind: Self::kind(),
            size: size_of::<Pid>(),
        }
        .encode()
    }

    fn payload(&self) -> Vec<u8> {
        self.pid.to_ne_bytes().to_vec()
    }

    fn ancillary_payload(&self) -> Option<AncillaryData> {
        None
    }

    fn decode(
        data: &[u8],
        ancillary_data: Option<AncillaryData>,
    ) -> Result<TransferMinidump, MessageError> {
        debug_assert!(
            ancillary_data.is_none(),
            "TransferMinidump messages cannot carry ancillary data"
        );
        let bytes: [u8; size_of::<Pid>()] = data[0..size_of::<Pid>()].try_into()?;
        let pid = Pid::from_ne_bytes(bytes);

        Ok(TransferMinidump { pid })
    }
}

/* Transfer minidump reply, received from the server after having sent a
 * TransferMinidump message. */

pub struct TransferMinidumpReply {
    pub path: OsString,
    pub error: Option<CString>,
}

impl TransferMinidumpReply {
    pub fn new(path: OsString, error: Option<CString>) -> TransferMinidumpReply {
        TransferMinidumpReply { path, error }
    }

    fn payload_size(&self) -> usize {
        let path_len = self.path.serialize().len();
        // TODO: We should use checked arithmetic here
        (size_of::<usize>() * 2)
            + path_len
            + self
                .error
                .as_ref()
                .map_or(0, |error| error.as_bytes().len())
    }
}

impl Message for TransferMinidumpReply {
    fn kind() -> Kind {
        Kind::TransferMinidumpReply
    }

    fn header(&self) -> Vec<u8> {
        Header {
            kind: Self::kind(),
            size: self.payload_size(),
        }
        .encode()
    }

    fn payload(&self) -> Vec<u8> {
        let path_bytes = self.path.serialize();
        let mut buffer = Vec::with_capacity(self.payload_size());
        buffer.extend(path_bytes.len().to_ne_bytes());
        buffer.extend(
            (self
                .error
                .as_ref()
                .map_or(0, |error| error.as_bytes().len()))
            .to_ne_bytes(),
        );
        buffer.extend(path_bytes);
        buffer.extend(
            self.error
                .as_ref()
                .map_or(Vec::new(), |error| Vec::from(error.as_bytes())),
        );
        buffer
    }

    fn ancillary_payload(&self) -> Option<AncillaryData> {
        None
    }

    fn decode(
        data: &[u8],
        ancillary_data: Option<AncillaryData>,
    ) -> Result<TransferMinidumpReply, MessageError> {
        debug_assert!(
            ancillary_data.is_none(),
            "TransferMinidumpReply messages cannot carry ancillary data"
        );
        let path_len_bytes: [u8; size_of::<usize>()] = data[0..size_of::<usize>()].try_into()?;
        let path_len = usize::from_ne_bytes(path_len_bytes);
        let offset = size_of::<usize>();

        let error_len_bytes: [u8; size_of::<usize>()] =
            data[offset..offset + size_of::<usize>()].try_into()?;
        let error_len = usize::from_ne_bytes(error_len_bytes);
        let offset = offset + size_of::<usize>();

        let path = <OsString as BreakpadString>::deserialize(&data[offset..offset + path_len])
            .map_err(|_| MessageError::InvalidData)?;
        let offset = offset + path_len;

        let error = if error_len > 0 {
            Some(CString::new(&data[offset..offset + error_len])?)
        } else {
            None
        };

        Ok(TransferMinidumpReply::new(path, error))
    }
}

/* Generate a minidump based on information captured by the Windows Error Reporting runtime exception module. */

#[cfg(target_os = "windows")]
pub struct WindowsErrorReportingMinidump {
    pub pid: Pid,
    pub tid: Pid, // TODO: This should be a different type
    pub exception_records: Vec<EXCEPTION_RECORD>,
    pub context: CONTEXT,
}

#[cfg(target_os = "windows")]
impl WindowsErrorReportingMinidump {
    pub fn new(
        pid: Pid,
        tid: Pid,
        exception_records: Vec<EXCEPTION_RECORD>,
        context: CONTEXT,
    ) -> WindowsErrorReportingMinidump {
        WindowsErrorReportingMinidump {
            pid,
            tid,
            exception_records,
            context,
        }
    }

    fn payload_size(&self) -> usize {
        (size_of::<Pid>() * 2)
            + size_of::<usize>()
            + (size_of::<EXCEPTION_RECORD>() * self.exception_records.len())
            + size_of::<CONTEXT>()
    }
}

#[cfg(target_os = "windows")]
impl Message for WindowsErrorReportingMinidump {
    fn kind() -> Kind {
        Kind::WindowsErrorReporting
    }

    fn header(&self) -> Vec<u8> {
        Header {
            kind: Self::kind(),
            size: self.payload_size(),
        }
        .encode()
    }

    fn payload(&self) -> Vec<u8> {
        let mut buffer = Vec::<u8>::with_capacity(self.payload_size());
        buffer.extend(self.pid.to_ne_bytes());
        buffer.extend(self.tid.to_ne_bytes());
        buffer.extend(self.exception_records.len().to_ne_bytes());
        for exception_record in self.exception_records.iter() {
            let bytes: [u8; size_of::<EXCEPTION_RECORD>()] =
                unsafe { std::mem::transmute(*exception_record) };
            buffer.extend(bytes);
        }
        let bytes: [u8; size_of::<CONTEXT>()] = unsafe { std::mem::transmute(self.context) };
        buffer.extend(bytes);
        buffer
    }

    fn ancillary_payload(&self) -> Option<AncillaryData> {
        None
    }

    fn decode(
        data: &[u8],
        ancillary_data: Option<AncillaryData>,
    ) -> Result<WindowsErrorReportingMinidump, MessageError> {
        debug_assert!(
            ancillary_data.is_none(),
            "WindowsErrorReportingMinidump messages cannot carry ancillary data"
        );
        let bytes: [u8; size_of::<Pid>()] = data[0..size_of::<Pid>()].try_into()?;
        let pid = Pid::from_ne_bytes(bytes);
        let offset = size_of::<Pid>();

        let bytes: [u8; size_of::<Pid>()] = data[offset..(offset + size_of::<Pid>())].try_into()?;
        let tid = Pid::from_ne_bytes(bytes);
        let offset = offset + size_of::<Pid>();

        let bytes: [u8; size_of::<usize>()] =
            data[offset..(offset + size_of::<usize>())].try_into()?;
        let exception_records_n = usize::from_ne_bytes(bytes);
        let offset = offset + size_of::<usize>();

        let mut exception_records = Vec::<EXCEPTION_RECORD>::with_capacity(exception_records_n);
        for i in 0..exception_records_n {
            let element_offset = offset + (i * size_of::<EXCEPTION_RECORD>());
            let bytes: [u8; size_of::<EXCEPTION_RECORD>()] = data
                [element_offset..(element_offset + size_of::<EXCEPTION_RECORD>())]
                .try_into()?;
            let exception_record = unsafe {
                std::mem::transmute::<[u8; size_of::<EXCEPTION_RECORD>()], EXCEPTION_RECORD>(bytes)
            };
            exception_records.push(exception_record);
        }

        let bytes: [u8; size_of::<CONTEXT>()] =
            data[offset..(offset + size_of::<CONTEXT>())].try_into()?;
        let context = unsafe { std::mem::transmute::<[u8; size_of::<CONTEXT>()], CONTEXT>(bytes) };

        Ok(WindowsErrorReportingMinidump {
            pid,
            tid,
            exception_records,
            context,
        })
    }
}

/* Windows Error Reporting minidump reply, received from the server after
 * having sent a WindowsErrorReportingMinidumpReply. Informs the client that
 * it can tear down the crashed process. */

#[cfg(target_os = "windows")]
pub struct WindowsErrorReportingMinidumpReply {}

#[cfg(target_os = "windows")]
impl Default for WindowsErrorReportingMinidumpReply {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(target_os = "windows")]
impl WindowsErrorReportingMinidumpReply {
    pub fn new() -> WindowsErrorReportingMinidumpReply {
        WindowsErrorReportingMinidumpReply {}
    }

    fn payload_size(&self) -> usize {
        0
    }
}

#[cfg(target_os = "windows")]
impl Message for WindowsErrorReportingMinidumpReply {
    fn kind() -> Kind {
        Kind::WindowsErrorReportingReply
    }

    fn header(&self) -> Vec<u8> {
        Header {
            kind: Self::kind(),
            size: self.payload_size(),
        }
        .encode()
    }

    fn payload(&self) -> Vec<u8> {
        Vec::<u8>::new()
    }

    fn ancillary_payload(&self) -> Option<AncillaryData> {
        None
    }

    fn decode(
        data: &[u8],
        ancillary_data: Option<AncillaryData>,
    ) -> Result<WindowsErrorReportingMinidumpReply, MessageError> {
        if ancillary_data.is_some() || !data.is_empty() {
            return Err(MessageError::InvalidData);
        }

        Ok(WindowsErrorReportingMinidumpReply::new())
    }
}
