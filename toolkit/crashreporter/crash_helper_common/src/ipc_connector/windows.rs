/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

use crate::{
    errors::IPCError,
    messages::{self, Message},
    platform::windows::{create_manual_reset_event, server_name, OverlappedOperation},
    AncillaryData, Pid, IO_TIMEOUT,
};

use std::{
    ffi::{c_void, CStr, OsString},
    os::windows::io::{AsRawHandle, FromRawHandle, OwnedHandle, RawHandle},
    ptr::null_mut,
    str::FromStr,
    time::{Duration, Instant},
};
use windows_sys::Win32::{
    Foundation::{
        GetLastError, ERROR_FILE_NOT_FOUND, ERROR_INVALID_MESSAGE, ERROR_PIPE_BUSY, FALSE, HANDLE,
        INVALID_HANDLE_VALUE, WAIT_TIMEOUT,
    },
    Security::SECURITY_ATTRIBUTES,
    Storage::FileSystem::{
        CreateFileA, FILE_FLAG_OVERLAPPED, FILE_READ_DATA, FILE_SHARE_READ, FILE_SHARE_WRITE,
        FILE_WRITE_ATTRIBUTES, FILE_WRITE_DATA, OPEN_EXISTING,
    },
    System::Pipes::{
        GetNamedPipeClientProcessId, SetNamedPipeHandleState, WaitNamedPipeA, PIPE_READMODE_MESSAGE,
    },
};

pub struct IPCConnector {
    handle: OwnedHandle,
    event: OwnedHandle,
    overlapped: Option<OverlappedOperation>,
    pid: Pid,
}

impl IPCConnector {
    pub fn new(handle: OwnedHandle) -> Result<IPCConnector, IPCError> {
        let event = create_manual_reset_event()?;
        let mut pid: Pid = 0;
        // SAFETY: The `pid` pointer is taken from the stack and thus always valid.
        let res =
            unsafe { GetNamedPipeClientProcessId(handle.as_raw_handle() as HANDLE, &mut pid) };
        if res == FALSE {
            return Err(IPCError::System(unsafe { GetLastError() }));
        }

        Ok(IPCConnector {
            handle,
            event,
            overlapped: None,
            pid,
        })
    }

    pub fn as_raw(&self) -> HANDLE {
        self.handle.as_raw_handle() as HANDLE
    }

    pub fn event_raw_handle(&self) -> HANDLE {
        self.event.as_raw_handle() as HANDLE
    }

    pub fn connect(pid: Pid) -> Result<IPCConnector, IPCError> {
        let server_name = server_name(pid);
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

            // SAFETY: The `server_name` is guaranteed to be valid, all other
            // pointer arguments are null.
            pipe = unsafe {
                CreateFileA(
                    server_name.as_ptr(),
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
                // SAFETY: The `server_name` pointer is guaranteed to be valid.
                let res = unsafe {
                    WaitNamedPipeA(server_name.as_ptr(), (timeout - elapsed).as_millis() as u32)
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

    pub fn send_message(&self, message: &dyn Message) -> Result<(), IPCError> {
        // Send the message header
        self.send(&message.header())?;

        // Send the message payload, ancillary payloads are not used on Windows.
        debug_assert!(
            message.ancillary_payload().is_none(),
            "Windows doesn't transfer ancillary data"
        );
        self.send(&message.payload())?;

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
            messages::HEADER_SIZE,
        )?);
        Ok(())
    }

    pub fn collect_header(&mut self) -> Result<messages::Header, IPCError> {
        // We should never call collect_header() on a connector that wasn't
        // waiting for one, so panic in that scenario.
        let overlapped = self.overlapped.take().unwrap();
        let buffer = overlapped.collect_recv(/* wait */ false)?;
        messages::Header::decode(buffer.as_ref()).map_err(IPCError::BadMessage)
    }

    pub fn send(&self, buff: &[u8]) -> Result<(), IPCError> {
        let overlapped = OverlappedOperation::sched_send(
            self.handle
                .try_clone()
                .map_err(IPCError::CloneHandleFailed)?,
            self.event_raw_handle(),
            buff.to_vec(),
        )?;
        overlapped.complete_send(/* wait */ false)
    }

    pub fn recv(&self, expected_size: usize) -> Result<(Vec<u8>, Option<AncillaryData>), IPCError> {
        let overlapped = OverlappedOperation::sched_recv(
            self.handle
                .try_clone()
                .map_err(IPCError::CloneHandleFailed)?,
            self.event_raw_handle(),
            expected_size,
        )?;
        let buffer = overlapped.collect_recv(/* wait */ true)?;
        Ok((buffer, None))
    }

    pub fn endpoint_pid(&self) -> Pid {
        self.pid
    }
}

// SAFETY: The connector can be transferred across threads in spite of the
// raw pointer contained in the OVERLAPPED structure because it is only
// used internally and never visible externally.
unsafe impl Send for IPCConnector {}
