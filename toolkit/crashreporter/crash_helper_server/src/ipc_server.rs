/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

use anyhow::Result;
use crash_helper_common::{errors::IPCError, messages, IPCConnector, IPCEvent, IPCListener};

use crate::crash_generation::{CrashGenerator, MessageResult};

#[cfg(any(target_os = "android", target_os = "linux", target_os = "macos"))]
mod unix;
#[cfg(target_os = "windows")]
mod windows;

#[derive(PartialEq)]
pub enum IPCServerState {
    Running,
    ClientDisconnected,
}

#[derive(PartialEq)]
enum IPCEndpoint {
    Parent, // A connection to the parent process
    #[allow(dead_code)]
    Child, // A connection to the child process
    External, // A connection to an external process
}

struct IPCConnection {
    connector: IPCConnector,
    endpoint: IPCEndpoint,
}

pub(crate) struct IPCServer {
    listener: IPCListener,
    connections: Vec<IPCConnection>,
}

impl IPCServer {
    pub(crate) fn new(listener: IPCListener, connector: IPCConnector) -> IPCServer {
        IPCServer {
            listener,
            connections: vec![IPCConnection {
                connector,
                endpoint: IPCEndpoint::Parent,
            }],
        }
    }

    pub(crate) fn run(
        &mut self,
        generator: &mut CrashGenerator,
    ) -> Result<IPCServerState, IPCError> {
        let events = self.wait_for_events()?;

        // We reverse the order of events, so that we start processing them
        // from the highest indexes toward the lowest. If we did the opposite
        // removed connections would invalidate the successive indexes.
        for event in events.into_iter().rev() {
            match event {
                IPCEvent::Connect(connector) => {
                    self.connections.push(IPCConnection {
                        connector,
                        endpoint: IPCEndpoint::External,
                    });
                }
                IPCEvent::Header(index, header) => {
                    let res = self.handle_message(index, &header, generator);
                    if let Err(error) = res {
                        log::error!(
                            "Error {error} while handling a message of {:?} kind",
                            header.kind
                        );
                    }
                }
                IPCEvent::Disconnect(index) => {
                    let connection = self.connections.remove(index);
                    if connection.endpoint == IPCEndpoint::Parent {
                        // The main process disconnected, leave
                        return Ok(IPCServerState::ClientDisconnected);
                    }
                }
            }
        }

        Ok(IPCServerState::Running)
    }

    fn handle_message(
        &mut self,
        index: usize,
        header: &messages::Header,
        generator: &mut CrashGenerator,
    ) -> Result<()> {
        let connection = self
            .connections
            .get_mut(index)
            .expect("Invalid connector index");
        let connector = &mut connection.connector;
        let (data, ancillary_data) = connector.recv(header.size)?;

        let reply = match connection.endpoint {
            IPCEndpoint::Parent => generator.parent_message(header.kind, &data, ancillary_data),
            IPCEndpoint::Child => generator.child_message(header.kind, &data, ancillary_data),
            IPCEndpoint::External => generator.external_message(header.kind, &data, ancillary_data),
        }?;

        match reply {
            MessageResult::Reply(reply) => connector.send_message(reply.as_ref())?,
            MessageResult::Connection(connector) => {
                self.connections.push(IPCConnection {
                    connector,
                    endpoint: IPCEndpoint::Child,
                });
            }

            MessageResult::None => {}
        }

        Ok(())
    }
}
