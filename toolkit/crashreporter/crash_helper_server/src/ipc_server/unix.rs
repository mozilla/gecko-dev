/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

use crash_helper_common::{errors::IPCError, ignore_eintr, IPCEvent};
use nix::{
    errno::Errno,
    poll::{poll, PollFd, PollFlags, PollTimeout},
};

use super::IPCServer;

impl IPCServer {
    pub fn wait_for_events(&mut self) -> Result<Vec<IPCEvent>, IPCError> {
        let mut pollfds = Vec::with_capacity(1 + self.connections.len());
        pollfds.push(PollFd::new(self.listener.as_raw_ref(), PollFlags::POLLIN));
        pollfds.extend(
            self.connections.iter().map(|connection| {
                PollFd::new(connection.connector.as_raw_ref(), PollFlags::POLLIN)
            }),
        );

        let mut events = Vec::<IPCEvent>::new();
        let mut num_events =
            ignore_eintr!(poll(&mut pollfds, PollTimeout::NONE)).map_err(IPCError::System)?;

        for (index, pollfd) in pollfds.iter().enumerate() {
            // revents() returns None only if the kernel sends back data
            // that nix does not understand, we can safely assume this
            // never happens in practice hence the unwrap().
            let revents = pollfd.revents().unwrap();

            if revents.contains(PollFlags::POLLHUP) {
                if index > 0 {
                    events.push(IPCEvent::Disconnect(index - 1));
                    // If a process was disconnected then skip all further
                    // processing of the socket. This wouldn't matter normally,
                    // but on macOS calling recvmsg() on a hung-up socket seems
                    // to trigger a kernel panic, one we've already encountered
                    // in the past. Doing things this way avoids the panic
                    // while having no real downsides.
                    continue;
                } else {
                    // This should never happen, unless the listener socket was
                    // not set up properly or a failure happened during setup.
                    return Err(IPCError::System(Errno::EFAULT));
                }
            }

            if revents.contains(PollFlags::POLLIN) {
                if index == 0 {
                    if let Ok(connector) = self.listener.accept() {
                        events.push(IPCEvent::Connect(connector));
                    }
                } else {
                    // SAFETY: The index is guaranteed to be >0 and within
                    // the bounds of the connections array.
                    let connection = unsafe { self.connections.get_unchecked(index - 1) };
                    let header = connection.connector.recv_header();
                    if let Ok(header) = header {
                        // Note that if we encounter a failure we don't propagate
                        // it, when the socket gets disconnected we'll get a
                        // POLLHUP event anyway so deal with disconnections there
                        // instead of here.
                        events.push(IPCEvent::Header(index - 1, header));
                    }
                }
            }

            if !revents.is_empty() {
                num_events -= 1;

                if num_events == 0 {
                    break;
                }
            }
        }

        Ok(events)
    }
}
