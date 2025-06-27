/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

use crate::{
    errors::IPCError,
    platform::windows::{create_manual_reset_event, server_name, OverlappedOperation},
    IPCConnector, Pid,
};

use std::{
    ffi::{c_void, CStr, OsString},
    os::windows::io::{AsRawHandle, FromRawHandle, OwnedHandle, RawHandle},
    ptr::null_mut,
    str::FromStr,
};
use windows_sys::Win32::{
    Foundation::{GetLastError, HANDLE, INVALID_HANDLE_VALUE, TRUE},
    Security::SECURITY_ATTRIBUTES,
    Storage::FileSystem::{
        FILE_FLAG_FIRST_PIPE_INSTANCE, FILE_FLAG_OVERLAPPED, PIPE_ACCESS_DUPLEX,
    },
    System::Pipes::{
        CreateNamedPipeA, PIPE_READMODE_MESSAGE, PIPE_TYPE_MESSAGE, PIPE_UNLIMITED_INSTANCES,
        PIPE_WAIT,
    },
};

pub struct IPCListener {
    server_name: String,
    handle: OwnedHandle,
    overlapped: Option<OverlappedOperation>,
    event: OwnedHandle,
}

impl IPCListener {
    pub fn new(pid: Pid) -> Result<IPCListener, IPCError> {
        let server_name = server_name(pid);
        let pipe = create_named_pipe(&server_name, /* first_instance */ true)?;
        let event = create_manual_reset_event()?;

        Ok(IPCListener {
            server_name,
            handle: pipe,
            overlapped: None,
            event,
        })
    }

    pub fn event_raw_handle(&self) -> HANDLE {
        self.event.as_raw_handle() as HANDLE
    }

    pub fn listen(&mut self) -> Result<(), IPCError> {
        self.overlapped = Some(OverlappedOperation::listen(
            self.handle
                .try_clone()
                .map_err(IPCError::CloneHandleFailed)?,
            self.event_raw_handle(),
        )?);
        Ok(())
    }

    pub fn accept(&mut self) -> Result<IPCConnector, IPCError> {
        // We should never call accept() on a listener that wasn't
        // already waiting, so panic in that scenario.
        let overlapped = self.overlapped.take().unwrap();
        overlapped.accept(self.handle.as_raw_handle() as HANDLE)?;
        let new_pipe = create_named_pipe(&self.server_name, /* first_instance */ false)?;
        let connected_pipe = std::mem::replace(&mut self.handle, new_pipe);

        // Once we've accepted a new connection and replaced the listener's
        // pipe we need to listen again before we return, so that we're ready
        // for the next iteration.
        self.listen()?;

        IPCConnector::new(connected_pipe)
    }

    /// Serialize this listener into a string that can be passed on the
    /// command-line to a child process. This only works for newly
    /// created listeners because they are explicitly created as inheritable.
    pub fn serialize(&self) -> OsString {
        let raw_handle = self.handle.as_raw_handle() as usize;
        OsString::from_str(raw_handle.to_string().as_ref()).unwrap()
    }

    /// Deserialize a listener from an argument passed on the command-line.
    /// The resulting listener is ready to accept new connections.
    pub fn deserialize(string: &CStr, pid: Pid) -> Result<IPCListener, IPCError> {
        let server_name = server_name(pid);
        let string = string.to_str().map_err(|_e| IPCError::ParseError)?;
        let handle = usize::from_str(string).map_err(|_e| IPCError::ParseError)?;
        let handle = handle as *mut c_void;
        // SAFETY: This is a handle we passed in ourselves.
        let handle = unsafe { OwnedHandle::from_raw_handle(handle) };
        let event = create_manual_reset_event()?;

        let mut listener = IPCListener {
            server_name,
            handle,
            overlapped: None,
            event,
        };

        // Since we've inherited this handler we need to start a new
        // asynchronous operation to listen for incoming connections.
        listener.listen()?;

        Ok(listener)
    }
}

// SAFETY: The listener can be transferred across threads in spite of the
// raw pointer contained in the OVERLAPPED structure because it is only
// used internally and never visible externally.
unsafe impl Send for IPCListener {}

fn create_named_pipe(server_name: &str, first_instance: bool) -> Result<OwnedHandle, IPCError> {
    const PIPE_BUFFER_SIZE: u32 = 4096;

    let open_mode = PIPE_ACCESS_DUPLEX
        | FILE_FLAG_OVERLAPPED
        | if first_instance {
            FILE_FLAG_FIRST_PIPE_INSTANCE
        } else {
            0
        };

    let security_attributes = SECURITY_ATTRIBUTES {
        nLength: size_of::<SECURITY_ATTRIBUTES>() as u32,
        lpSecurityDescriptor: null_mut(),
        bInheritHandle: TRUE,
    };

    // SAFETY: We pass a pointer to the server name which we guarantee to be
    // valid, and null for all the other pointer arguments.
    let pipe = unsafe {
        CreateNamedPipeA(
            server_name.as_ptr(),
            open_mode,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            PIPE_UNLIMITED_INSTANCES,
            PIPE_BUFFER_SIZE,
            PIPE_BUFFER_SIZE,
            0, // nDefaultTimeout, default is 50ms
            &security_attributes,
        )
    };

    if pipe == INVALID_HANDLE_VALUE {
        return Err(IPCError::System(unsafe { GetLastError() }));
    }

    // SAFETY: We just verified that the handle is valid.
    Ok(unsafe { OwnedHandle::from_raw_handle(pipe as RawHandle) })
}
