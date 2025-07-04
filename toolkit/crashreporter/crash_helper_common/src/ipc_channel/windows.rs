/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

use std::{ffi::CString, hash::RandomState, process};

use windows_sys::Win32::Foundation::ERROR_ACCESS_DENIED;

use crate::{
    errors::IPCError,
    platform::windows::{get_last_error, server_addr},
    IPCConnector, IPCListener, Pid,
};

pub struct IPCChannel {
    listener: IPCListener,
    client_endpoint: IPCConnector,
    server_endpoint: IPCConnector,
}

impl IPCChannel {
    /// Create a new IPCChannel, this includes a listening endpoint that
    /// will use the current process PID as part of its address and two
    /// connected endpoints.
    pub fn new() -> Result<IPCChannel, IPCError> {
        let pid = process::id() as Pid;
        let mut listener = IPCListener::new(server_addr(pid))?;
        listener.listen()?;
        let client_endpoint = IPCConnector::connect(listener.address())?;
        let server_endpoint = listener.accept()?;

        Ok(IPCChannel {
            listener,
            client_endpoint,
            server_endpoint,
        })
    }

    /// Deconstruct the IPC channel, returning the listening endpoint,
    /// the connected server-side endpoint and the connected client-side
    /// endpoint.
    pub fn deconstruct(self) -> (IPCListener, IPCConnector, IPCConnector) {
        (self.listener, self.server_endpoint, self.client_endpoint)
    }
}

pub struct IPCClientChannel {
    client_endpoint: IPCConnector,
    server_endpoint: IPCConnector,
}

impl IPCClientChannel {
    /// Create a new IPC channel for use between one of the browser's child
    /// processes and the crash helper.
    pub fn new() -> Result<IPCClientChannel, IPCError> {
        let mut listener = Self::create_listener()?;
        listener.listen()?;
        let client_endpoint = IPCConnector::connect(listener.address())?;
        let server_endpoint = listener.accept()?;

        Ok(IPCClientChannel {
            client_endpoint,
            server_endpoint,
        })
    }

    fn create_listener() -> Result<IPCListener, IPCError> {
        const ATTEMPTS: u32 = 5;

        // We pick the listener name at random, as unlikely as it may be there
        // could be clashes so try a few times before giving up.
        for _i in 0..ATTEMPTS {
            use std::hash::{BuildHasher, Hasher};
            let random_id = RandomState::new().build_hasher().finish();

            let pipe_name = CString::new(format!(
                "\\\\.\\pipe\\gecko-crash-helper-child-pipe.{random_id:}"
            ))
            .unwrap();
            match IPCListener::new(pipe_name) {
                Ok(listener) => return Ok(listener),
                Err(_error @ IPCError::System(ERROR_ACCESS_DENIED)) => {} // Try again
                Err(error) => return Err(error),
            }
        }

        // If we got to this point return whatever was the last error we
        // encountered along the way.
        Err(IPCError::System(get_last_error()))
    }

    /// Deconstruct the IPC channel, returning the listening endpoint,
    /// the connected server-side endpoint and the connected client-side
    /// endpoint.
    pub fn deconstruct(self) -> (IPCConnector, IPCConnector) {
        (self.server_endpoint, self.client_endpoint)
    }
}
