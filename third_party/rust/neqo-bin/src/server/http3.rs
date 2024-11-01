// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::{
    cell::RefCell,
    collections::HashMap,
    fmt::{self, Display},
    rc::Rc,
    time::Instant,
};

use neqo_common::{hex, qdebug, qerror, qinfo, Datagram, Header};
use neqo_crypto::{generate_ech_keys, random, AntiReplay};
use neqo_http3::{
    Http3OrWebTransportStream, Http3Parameters, Http3Server, Http3ServerEvent, StreamId,
};
use neqo_transport::{server::ValidateAddress, ConnectionIdGenerator};

use super::{qns_read_response, Args, ResponseData};

pub struct HttpServer {
    server: Http3Server,
    /// Progress writing to each stream.
    remaining_data: HashMap<StreamId, ResponseData>,
    posts: HashMap<Http3OrWebTransportStream, usize>,
    is_qns_test: bool,
}

impl HttpServer {
    pub fn new(
        args: &Args,
        anti_replay: AntiReplay,
        cid_mgr: Rc<RefCell<dyn ConnectionIdGenerator>>,
    ) -> Self {
        let mut server = Http3Server::new(
            args.now(),
            &[args.key.clone()],
            &[args.shared.alpn.clone()],
            anti_replay,
            cid_mgr,
            Http3Parameters::default()
                .connection_parameters(args.shared.quic_parameters.get(&args.shared.alpn))
                .max_table_size_encoder(args.shared.max_table_size_encoder)
                .max_table_size_decoder(args.shared.max_table_size_decoder)
                .max_blocked_streams(args.shared.max_blocked_streams),
            None,
        )
        .expect("We cannot make a server!");

        server.set_ciphers(args.get_ciphers());
        server.set_qlog_dir(args.shared.qlog_dir.clone());
        if args.retry {
            server.set_validation(ValidateAddress::Always);
        }
        if args.ech {
            let (sk, pk) = generate_ech_keys().expect("should create ECH keys");
            server
                .enable_ech(random::<1>()[0], "public.example", &sk, &pk)
                .unwrap();
            let cfg = server.ech_config();
            qinfo!("ECHConfigList: {}", hex(cfg));
        }
        Self {
            server,
            remaining_data: HashMap::new(),
            posts: HashMap::new(),
            is_qns_test: args.shared.qns_test.is_some(),
        }
    }
}

impl Display for HttpServer {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        self.server.fmt(f)
    }
}

impl super::HttpServer for HttpServer {
    fn process(&mut self, dgram: Option<&Datagram>, now: Instant) -> neqo_http3::Output {
        self.server.process(dgram, now)
    }

    fn process_events(&mut self, _now: Instant) {
        while let Some(event) = self.server.next_event() {
            match event {
                Http3ServerEvent::Headers {
                    stream,
                    headers,
                    fin,
                } => {
                    qdebug!("Headers (request={stream} fin={fin}): {headers:?}");

                    if headers
                        .iter()
                        .any(|h| h.name() == ":method" && h.value() == "POST")
                    {
                        self.posts.insert(stream, 0);
                        continue;
                    }

                    let Some(path) = headers.iter().find(|&h| h.name() == ":path") else {
                        stream
                            .cancel_fetch(neqo_http3::Error::HttpRequestIncomplete.code())
                            .unwrap();
                        continue;
                    };

                    let mut response = if self.is_qns_test {
                        match qns_read_response(path.value()) {
                            Ok(data) => ResponseData::from(data),
                            Err(e) => {
                                qerror!("Failed to read {}: {e}", path.value());
                                stream
                                    .send_headers(&[Header::new(":status", "404")])
                                    .unwrap();
                                stream.stream_close_send().unwrap();
                                continue;
                            }
                        }
                    } else if let Ok(count) =
                        path.value().trim_matches(|p| p == '/').parse::<usize>()
                    {
                        ResponseData::zeroes(count)
                    } else {
                        ResponseData::from(path.value())
                    };

                    stream
                        .send_headers(&[
                            Header::new(":status", "200"),
                            Header::new("content-length", response.remaining.to_string()),
                        ])
                        .unwrap();
                    response.send_h3(&stream);
                    if response.done() {
                        stream.stream_close_send().unwrap();
                    } else {
                        self.remaining_data.insert(stream.stream_id(), response);
                    }
                }
                Http3ServerEvent::DataWritable { stream } => {
                    if self.posts.get_mut(&stream).is_none() {
                        if let Some(remaining) = self.remaining_data.get_mut(&stream.stream_id()) {
                            remaining.send_h3(&stream);
                            if remaining.done() {
                                self.remaining_data.remove(&stream.stream_id());
                                stream.stream_close_send().unwrap();
                            }
                        }
                    }
                }

                Http3ServerEvent::Data { stream, data, fin } => {
                    if let Some(received) = self.posts.get_mut(&stream) {
                        *received += data.len();
                    }
                    if fin {
                        if let Some(received) = self.posts.remove(&stream) {
                            let msg = received.to_string().as_bytes().to_vec();
                            stream
                                .send_headers(&[Header::new(":status", "200")])
                                .unwrap();
                            stream.send_data(&msg).unwrap();
                            stream.stream_close_send().unwrap();
                        }
                    }
                }
                _ => {}
            }
        }
    }

    fn has_events(&self) -> bool {
        self.server.has_events()
    }
}
