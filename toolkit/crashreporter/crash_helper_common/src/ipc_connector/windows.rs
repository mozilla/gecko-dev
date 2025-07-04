/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

use crate::{
    errors::IPCError,
    messages::{self, Message},
    platform::windows::{create_manual_reset_event, get_last_error, OverlappedOperation},
    ProcessHandle, IO_TIMEOUT,
};

use std::{
    ffi::{c_void, CStr, OsString},
    os::windows::io::{AsRawHandle, FromRawHandle, IntoRawHandle, OwnedHandle, RawHandle},
    ptr::null_mut,
    str::FromStr,
    time::{Duration, Instant},
};
use windows_sys::Win32::{
    Foundation::{
        DuplicateHandle, GetLastError, DUPLICATE_CLOSE_SOURCE, DUPLICATE_SAME_ACCESS,
        ERROR_FILE_NOT_FOUND, ERROR_INVALID_MESSAGE, ERROR_PIPE_BUSY, FALSE, HANDLE,
        INVALID_HANDLE_VALUE, WAIT_TIMEOUT,
    },
    Security::SECURITY_ATTRIBUTES,
    Storage::FileSystem::{
        CreateFileA, FILE_FLAG_OVERLAPPED, FILE_READ_DATA, FILE_SHARE_READ, FILE_SHARE_WRITE,
        FILE_WRITE_ATTRIBUTES, FILE_WRITE_DATA, OPEN_EXISTING,
    },
    System::{
        Pipes::{SetNamedPipeHandleState, WaitNamedPipeA, PIPE_READMODE_MESSAGE},
        Threading::GetCurrentProcess,
    },
};

pub type AncillaryData = HANDLE;

// This must match `kInvalidHandle` in `mfbt/UniquePtrExt.h`
pub const INVALID_ANCILLARY_DATA: AncillaryData = 0;

const HANDLE_SIZE: usize = size_of::<HANDLE>();

// We encode handles at the beginning of every transmitted message. This
// function extracts the handle (if present) and returns it together with
// the rest of the buffer.
fn extract_buffer_and_handle(buffer: Vec<u8>) -> Result<(Vec<u8>, Option<HANDLE>), IPCError> {
    let handle_bytes = &buffer[0..HANDLE_SIZE];
    let data = &buffer[HANDLE_SIZE..];
    let handle_bytes: Result<[u8; HANDLE_SIZE], _> = handle_bytes.try_into();
    let Ok(handle_bytes) = handle_bytes else {
        return Err(IPCError::ParseError);
    };
    let handle = match HANDLE::from_ne_bytes(handle_bytes) {
        INVALID_ANCILLARY_DATA => None,
        handle => Some(handle),
    };

    Ok((data.to_vec(), handle))
}

pub struct IPCConnector {
    handle: OwnedHandle,
    event: OwnedHandle,
    overlapped: Option<OverlappedOperation>,
}

impl IPCConnector {
    pub fn new(handle: OwnedHandle) -> Result<IPCConnector, IPCError> {
        let event = create_manual_reset_event()?;

        Ok(IPCConnector {
            handle,
            event,
            overlapped: None,
        })
    }

    pub fn from_ancillary(ancillary_data: AncillaryData) -> Result<IPCConnector, IPCError> {
        IPCConnector::new(unsafe { OwnedHandle::from_raw_handle(ancillary_data as RawHandle) })
    }

    pub fn as_raw(&self) -> HANDLE {
        self.handle.as_raw_handle() as HANDLE
    }

    pub fn event_raw_handle(&self) -> HANDLE {
        self.event.as_raw_handle() as HANDLE
    }

    pub fn connect(server_addr: &CStr) -> Result<IPCConnector, IPCError> {
        let now = Instant::now();
        let timeout = Duration::from_millis(IO_TIMEOUT.into());
        let mut pipe;
        loop {
            // Connectors must not be inherited
            let security_attributes = SECURITY_ATTRIBUTES {
                nLength: size_of::<SECURITY_ATTRIBUTES>() as u32,
                lpSecurityDescriptor: null_mut(),
                bInheritHandle: FALSE,
            };

            // SAFETY: The `server_addr` pointer is guaranteed to be valid,
            // all other pointer arguments are null.
            pipe = unsafe {
                CreateFileA(
                    server_addr.as_ptr() as *const _,
                    FILE_READ_DATA | FILE_WRITE_DATA | FILE_WRITE_ATTRIBUTES,
                    FILE_SHARE_READ | FILE_SHARE_WRITE,
                    &security_attributes,
                    OPEN_EXISTING,
                    FILE_FLAG_OVERLAPPED,
                    /* hTemplateFile */ 0 as HANDLE,
                )
            };

            if pipe != INVALID_HANDLE_VALUE {
                break;
            }

            let elapsed = now.elapsed();

            if elapsed >= timeout {
                return Err(IPCError::System(WAIT_TIMEOUT)); // TODO: We need a dedicated error
            }

            let error = unsafe { GetLastError() };

            // The pipe might have not been created yet or it might be busy.
            if (error == ERROR_FILE_NOT_FOUND) || (error == ERROR_PIPE_BUSY) {
                // SAFETY: The `server_addr` pointer is guaranteed to be valid.
                let res = unsafe {
                    WaitNamedPipeA(
                        server_addr.as_ptr() as *const _,
                        (timeout - elapsed).as_millis() as u32,
                    )
                };
                let error = unsafe { GetLastError() };

                // If the pipe hasn't been created yet loop over and try again
                if (res == FALSE) && (error != ERROR_FILE_NOT_FOUND) {
                    return Err(IPCError::System(error));
                }
            } else {
                return Err(IPCError::System(error));
            }
        }

        // Change to message-read mode
        let pipe_mode: u32 = PIPE_READMODE_MESSAGE;
        // SAFETY: We pass a pointer to a local variable which guarantees it
        // is valid, we use null for all the other pointer parameters.
        let res = unsafe {
            SetNamedPipeHandleState(
                pipe,
                &pipe_mode,
                /* lpMaxCollectionCount */ null_mut(),
                /* lpCollectDataTimeout */ null_mut(),
            )
        };
        if res == FALSE {
            return Err(IPCError::System(unsafe { GetLastError() }));
        }

        // SAFETY: The raw pipe handle is guaranteed to be open at this point
        let handle = unsafe { OwnedHandle::from_raw_handle(pipe as RawHandle) };
        IPCConnector::new(handle)
    }

    /// Serialize this connector into a string that can be passed on the
    /// command-line to a child process. This only works for newly
    /// created connectors because they are explicitly created as inheritable.
    pub fn serialize(&self) -> OsString {
        let raw_handle = self.handle.as_raw_handle() as usize;
        OsString::from_str(raw_handle.to_string().as_ref()).unwrap()
    }

    /// Deserialize a connector from an argument passed on the command-line.
    pub fn deserialize(string: &CStr) -> Result<IPCConnector, IPCError> {
        let string = string.to_str().map_err(|_e| IPCError::ParseError)?;
        let handle = usize::from_str(string).map_err(|_e| IPCError::ParseError)?;
        let handle = handle as *mut c_void;
        // SAFETY: This is a handle we passed in ourselves.
        let handle = unsafe { OwnedHandle::from_raw_handle(handle) };
        IPCConnector::new(handle)
    }

    pub fn into_ancillary(
        self,
        dst_process: &Option<ProcessHandle>,
    ) -> Result<AncillaryData, IPCError> {
        let mut dst_handle: HANDLE = INVALID_ANCILLARY_DATA;

        if let Some(dst_process) = dst_process.as_ref() {
            let res = unsafe {
                DuplicateHandle(
                    GetCurrentProcess(),
                    self.handle.into_raw_handle() as HANDLE,
                    dst_process.as_raw_handle() as HANDLE,
                    &mut dst_handle,
                    /* dwDesiredAccess */ 0,
                    /* bInheritHandle */ FALSE,
                    DUPLICATE_CLOSE_SOURCE | DUPLICATE_SAME_ACCESS,
                )
            };

            if res > 0 {
                Ok(dst_handle)
            } else {
                Err(IPCError::System(get_last_error()))
            }
        } else {
            Ok(self.handle.into_raw_handle() as HANDLE)
        }
    }

    pub fn send_message(&self, message: &dyn Message) -> Result<(), IPCError> {
        // Send the message header
        self.send(&message.header(), None)?;

        // Send the message payload
        self.send(&message.payload(), message.ancillary_payload())?;

        Ok(())
    }

    pub fn recv_reply<T>(&self) -> Result<T, IPCError>
    where
        T: Message,
    {
        let header = self.recv_header()?;

        if header.kind != T::kind() {
            return Err(IPCError::ReceptionFailure(ERROR_INVALID_MESSAGE));
        }

        let (data, _) = self.recv(header.size)?;
        T::decode(&data, None).map_err(IPCError::from)
    }

    fn recv_header(&self) -> Result<messages::Header, IPCError> {
        let (header, _) = self.recv(messages::HEADER_SIZE)?;
        messages::Header::decode(&header).map_err(IPCError::BadMessage)
    }

    pub fn sched_recv_header(&mut self) -> Result<(), IPCError> {
        if self.overlapped.is_some() {
            // We're already waiting for a header.
            return Ok(());
        }

        self.overlapped = Some(OverlappedOperation::sched_recv(
            self.handle
                .try_clone()
                .map_err(IPCError::CloneHandleFailed)?,
            self.event_raw_handle(),
            HANDLE_SIZE + messages::HEADER_SIZE,
        )?);
        Ok(())
    }

    pub fn collect_header(&mut self) -> Result<messages::Header, IPCError> {
        // We should never call collect_header() on a connector that wasn't
        // waiting for one, so panic in that scenario.
        let overlapped = self.overlapped.take().unwrap();
        let buffer = overlapped.collect_recv(/* wait */ false)?;
        let (data, _) = extract_buffer_and_handle(buffer)?;
        messages::Header::decode(data.as_ref()).map_err(IPCError::BadMessage)
    }

    pub fn send(&self, buff: &[u8], handle: Option<AncillaryData>) -> Result<(), IPCError> {
        let mut buffer = Vec::<u8>::with_capacity(HANDLE_SIZE + buff.len());
        buffer.extend(handle.unwrap_or(INVALID_ANCILLARY_DATA).to_ne_bytes());
        buffer.extend(buff);

        let overlapped =
            OverlappedOperation::sched_send(self.handle
                .try_clone()
                .map_err(IPCError::CloneHandleFailed)?, self.event_raw_handle(), buffer)?;
        overlapped.complete_send(/* wait */ true)
    }

    pub fn recv(&self, expected_size: usize) -> Result<(Vec<u8>, Option<AncillaryData>), IPCError> {
        let overlapped = OverlappedOperation::sched_recv(
            self.handle
                .try_clone()
                .map_err(IPCError::CloneHandleFailed)?,
            self.event_raw_handle(),
            HANDLE_SIZE + expected_size,
        )?;
        let buffer = overlapped.collect_recv(/* wait */ true)?;
        extract_buffer_and_handle(buffer)
    }
}

// SAFETY: The connector can be transferred across threads in spite of the raw
// pointer contained in the OVERLAPPED structure because it is only used
// internally and never visible externally.
unsafe impl Send for IPCConnector {}
