// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![expect(clippy::unwrap_used, reason = "This is example code.")]

use std::{
    borrow::Cow,
    cell::RefCell,
    collections::HashMap,
    fmt::{self, Display, Formatter},
    rc::Rc,
    slice, str,
    time::Instant,
};

use neqo_common::{event::Provider as _, hex, qdebug, qerror, qinfo, qwarn, Datagram};
use neqo_crypto::{generate_ech_keys, random, AllowZeroRtt, AntiReplay};
use neqo_http3::Error;
use neqo_transport::{
    server::{ConnectionRef, Server, ValidateAddress},
    ConnectionEvent, ConnectionIdGenerator, Output, State, StreamId,
};
use regex::Regex;

use super::{qns_read_response, Args};
use crate::{send_data::SendData, STREAM_IO_BUFFER_SIZE};

#[derive(Default)]
struct HttpStreamState {
    writable: bool,
    data_to_send: Option<SendData>,
}

pub struct HttpServer {
    server: Server,
    write_state: HashMap<StreamId, HttpStreamState>,
    read_state: HashMap<StreamId, Vec<u8>>,
    is_qns_test: bool,
    regex: Regex,
    read_buffer: Vec<u8>,
}

impl HttpServer {
    pub fn new(
        args: &Args,
        anti_replay: AntiReplay,
        cid_manager: Rc<RefCell<dyn ConnectionIdGenerator>>,
    ) -> Result<Self, Error> {
        let mut server = Server::new(
            args.now(),
            slice::from_ref(&args.key),
            slice::from_ref(&args.shared.alpn),
            anti_replay,
            Box::new(AllowZeroRtt {}),
            cid_manager,
            args.shared.quic_parameters.get(&args.shared.alpn),
        )?;

        server.set_ciphers(args.get_ciphers());
        server.set_qlog_dir(args.shared.qlog_dir.clone());
        if args.retry {
            server.set_validation(ValidateAddress::Always);
        }
        if args.ech {
            let (sk, pk) = generate_ech_keys().map_err(|_| Error::Internal)?;
            server
                .enable_ech(random::<1>()[0], "public.example", &sk, &pk)
                .map_err(|_| Error::Internal)?;
            qinfo!("ECHConfigList: {}", hex(server.ech_config()));
        }

        let is_qns_test = args.shared.qns_test.is_some();
        Ok(Self {
            server,
            write_state: HashMap::new(),
            read_state: HashMap::new(),
            is_qns_test,
            regex: if is_qns_test {
                Regex::new(r"GET +/(\S+)(?:\r)?\n").map_err(|_| Error::Internal)?
            } else {
                Regex::new(r"GET +/(\d+)(?:\r)?\n").map_err(|_| Error::Internal)?
            },
            read_buffer: vec![0; STREAM_IO_BUFFER_SIZE],
        })
    }

    fn save_partial(&mut self, stream_id: StreamId, partial: Vec<u8>, conn: &ConnectionRef) {
        if partial.len() < 4096 {
            qdebug!(
                "Saving partial URL: {}",
                String::from_utf8(partial.clone())
                    .unwrap_or_else(|_| format!("<invalid UTF-8: {}>", hex(&partial)))
            );
            self.read_state.insert(stream_id, partial);
        } else {
            qdebug!(
                "Giving up on partial URL {}",
                String::from_utf8(partial.clone())
                    .unwrap_or_else(|_| format!("<invalid UTF-8: {}>", hex(&partial)))
            );
            conn.borrow_mut().stream_stop_sending(stream_id, 0).unwrap();
        }
    }

    fn stream_readable(&mut self, stream_id: StreamId, conn: &ConnectionRef) {
        if !stream_id.is_client_initiated() || !stream_id.is_bidi() {
            qdebug!("Stream {stream_id} not client-initiated bidi, ignoring");
            return;
        }
        let (sz, fin) = conn
            .borrow_mut()
            .stream_recv(stream_id, &mut self.read_buffer)
            .expect("Read should succeed");

        if sz == 0 {
            if !fin {
                qdebug!("size 0 but !fin");
            }
            return;
        }
        let read_buffer = &self.read_buffer[..sz];

        let buf = self.read_state.remove(&stream_id).map_or(
            Cow::Borrowed(read_buffer),
            |mut existing| {
                existing.extend_from_slice(read_buffer);
                Cow::Owned(existing)
            },
        );

        let Ok(msg) = str::from_utf8(&buf[..]) else {
            self.save_partial(stream_id, buf.to_vec(), conn);
            return;
        };

        let m = self.regex.captures(msg);
        let Some(path) = m.and_then(|m| m.get(1)) else {
            self.save_partial(stream_id, buf.to_vec(), conn);
            return;
        };

        let resp: SendData = {
            let path = path.as_str();
            qdebug!("Path = '{path}'");
            if self.is_qns_test {
                match qns_read_response(path) {
                    Ok(data) => data.into(),
                    Err(e) => {
                        qerror!("Failed to read {path}: {e}");
                        b"404".to_vec().into()
                    }
                }
            } else {
                let count = path.parse().unwrap();
                SendData::zeroes(count)
            }
        };

        if let Some(stream_state) = self.write_state.get_mut(&stream_id) {
            match stream_state.data_to_send {
                None => stream_state.data_to_send = Some(resp),
                Some(_) => {
                    qdebug!("Data already set, doing nothing");
                }
            }
            if stream_state.writable {
                self.stream_writable(stream_id, conn);
            }
        } else {
            self.write_state.insert(
                stream_id,
                HttpStreamState {
                    writable: false,
                    data_to_send: Some(resp),
                },
            );
        }
    }

    fn stream_writable(&mut self, stream_id: StreamId, conn: &ConnectionRef) {
        let Some(stream_state) = self.write_state.get_mut(&stream_id) else {
            qwarn!("Unknown stream {stream_id}, ignoring event");
            return;
        };

        stream_state.writable = true;
        if let Some(resp) = &mut stream_state.data_to_send {
            let done = resp.send(|chunk| conn.borrow_mut().stream_send(stream_id, chunk).unwrap());
            if done {
                conn.borrow_mut().stream_close_send(stream_id).unwrap();
                self.write_state.remove(&stream_id);
            } else {
                stream_state.writable = false;
            }
        }
    }
}

impl super::HttpServer for HttpServer {
    fn process(&mut self, dgram: Option<Datagram<&mut [u8]>>, now: Instant) -> Output {
        self.server.process(dgram, now)
    }

    fn process_events(&mut self, now: Instant) {
        #[expect(
            clippy::mutable_key_type,
            reason = "ActiveConnectionRef::Hash doesn't access any of the interior mutable types"
        )]
        let active_conns = self.server.active_connections();
        #[expect(
            clippy::iter_over_hash_type,
            reason = "OK to loop over active connections in an undefined order."
        )]
        for acr in active_conns {
            loop {
                let Some(event) = acr.borrow_mut().next_event() else {
                    break;
                };
                match event {
                    ConnectionEvent::NewStream { stream_id } => {
                        self.write_state
                            .insert(stream_id, HttpStreamState::default());
                    }
                    ConnectionEvent::RecvStreamReadable { stream_id } => {
                        self.stream_readable(stream_id, &acr);
                    }
                    ConnectionEvent::SendStreamWritable { stream_id } => {
                        self.stream_writable(stream_id, &acr);
                    }
                    ConnectionEvent::StateChange(State::Connected) => {
                        acr.connection()
                            .borrow_mut()
                            .send_ticket(now, b"hi!")
                            .unwrap();
                    }
                    ConnectionEvent::StateChange(_)
                    | ConnectionEvent::SendStreamCreatable { .. }
                    | ConnectionEvent::SendStreamComplete { .. } => (),
                    e => qwarn!("unhandled event {e:?}"),
                }
            }
        }
    }

    fn has_events(&self) -> bool {
        self.server.has_active_connections()
    }
}

impl Display for HttpServer {
    fn fmt(&self, f: &mut Formatter) -> fmt::Result {
        write!(f, "Http 0.9 server ")
    }
}
