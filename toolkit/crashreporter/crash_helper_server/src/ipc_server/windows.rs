/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::convert::TryInto;

use crash_helper_common::{errors::IPCError, IPCEvent};
use windows_sys::Win32::{
    Foundation::{ERROR_BROKEN_PIPE, ERROR_INVALID_PARAMETER, FALSE, HANDLE, WAIT_OBJECT_0},
    System::{
        SystemServices::MAXIMUM_WAIT_OBJECTS,
        Threading::{WaitForMultipleObjects, INFINITE},
    },
};

use super::IPCServer;

impl IPCServer {
    pub fn wait_for_events(&mut self) -> Result<Vec<IPCEvent>, IPCError> {
        for connection in self.connections.iter_mut() {
            // TODO: We might get a broken pipe error here which would cause us to
            // fail instead of just dropping the disconnected connection.
            connection.connector.sched_recv_header()?;
        }

        let native_events = self.collect_events();

        if native_events.len() > MAXIMUM_WAIT_OBJECTS as usize {
            return Err(IPCError::WaitingFailure(Some(ERROR_INVALID_PARAMETER)));
        }

        // SAFETY: This is less than MAXIMUM_WAIT_OBJECTS
        let native_events_len: u32 = unsafe { native_events.len().try_into().unwrap_unchecked() };

        let res = unsafe {
            WaitForMultipleObjects(
                native_events_len,
                native_events.as_ptr(),
                FALSE, // bWaitAll
                INFINITE,
            )
        };

        if res >= (WAIT_OBJECT_0 + native_events_len) {
            return Err(IPCError::WaitingFailure(None));
        }

        let index = (res - WAIT_OBJECT_0) as usize;

        let mut events = Vec::<IPCEvent>::new();
        if index == 0 {
            if let Ok(connector) = self.listener.accept() {
                events.push(IPCEvent::Connect(connector));
            }
        } else {
            let index = index - 1;
            // SAFETY: The index is guaranteed to be within the bounds of the connections array.
            let connection = unsafe { self.connections.get_unchecked_mut(index) };
            let header = connection.connector.collect_header();

            match header {
                Ok(header) => {
                    events.push(IPCEvent::Header(index, header));
                }
                Err(error) => match error {
                    IPCError::System(_code @ ERROR_BROKEN_PIPE) => {
                        events.push(IPCEvent::Disconnect(index));
                    }
                    _ => return Err(error),
                },
            }
        }

        Ok(events)
    }

    fn collect_events(&self) -> Vec<HANDLE> {
        let mut events = Vec::with_capacity(1 + self.connections.len());

        events.push(self.listener.event_raw_handle());
        for connection in self.connections.iter() {
            events.push(connection.connector.event_raw_handle());
        }

        events
    }
}
