// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! An HTTP 3 client implementation.

use std::{
    cell::RefCell,
    collections::{HashMap, VecDeque},
    fmt::Display,
    fs::File,
    io::{BufWriter, Write},
    net::SocketAddr,
    path::PathBuf,
    rc::Rc,
    time::Instant,
};

use neqo_common::{event::Provider, hex, qdebug, qinfo, qwarn, Datagram, Header};
use neqo_crypto::{AuthenticationStatus, ResumptionToken};
use neqo_http3::{Error, Http3Client, Http3ClientEvent, Http3Parameters, Http3State, Priority};
use neqo_transport::{
    AppError, CloseReason, Connection, EmptyConnectionIdGenerator, Error as TransportError, Output,
    RandomConnectionIdGenerator, StreamId,
};
use url::Url;

use super::{get_output_file, qlog_new, Args, CloseState, Res};
use crate::{send_data::SendData, STREAM_IO_BUFFER_SIZE};

pub struct Handler<'a> {
    #[allow(clippy::struct_field_names)]
    url_handler: UrlHandler<'a>,
    token: Option<ResumptionToken>,
    output_read_data: bool,
    read_buffer: Vec<u8>,
}

impl<'a> Handler<'a> {
    pub(crate) fn new(url_queue: VecDeque<Url>, args: &'a Args) -> Self {
        let url_handler = UrlHandler {
            url_queue,
            handled_urls: Vec::new(),
            stream_handlers: HashMap::new(),
            all_paths: Vec::new(),
            args,
        };

        Self {
            url_handler,
            token: None,
            output_read_data: args.output_read_data,
            read_buffer: vec![0; STREAM_IO_BUFFER_SIZE],
        }
    }
}

pub fn create_client(
    args: &Args,
    local_addr: SocketAddr,
    remote_addr: SocketAddr,
    hostname: &str,
    resumption_token: Option<ResumptionToken>,
) -> Res<Http3Client> {
    let cid_generator: Rc<RefCell<dyn neqo_transport::ConnectionIdGenerator>> = if args.cid_len == 0
    {
        Rc::new(RefCell::new(EmptyConnectionIdGenerator::default()))
    } else {
        Rc::new(RefCell::new(RandomConnectionIdGenerator::new(
            args.cid_len.into(),
        )))
    };
    let mut transport = Connection::new_client(
        hostname,
        &[&args.shared.alpn],
        cid_generator,
        local_addr,
        remote_addr,
        args.shared.quic_parameters.get(args.shared.alpn.as_str()),
        Instant::now(),
    )?;
    let ciphers = args.get_ciphers();
    if !ciphers.is_empty() {
        transport.set_ciphers(&ciphers)?;
    }
    let mut client = Http3Client::new_with_conn(
        transport,
        Http3Parameters::default()
            .max_table_size_encoder(args.shared.max_table_size_encoder)
            .max_table_size_decoder(args.shared.max_table_size_decoder)
            .max_blocked_streams(args.shared.max_blocked_streams)
            .max_concurrent_push_streams(args.max_concurrent_push_streams),
    );

    let qlog = qlog_new(args, hostname, client.connection_id())?;
    client.set_qlog(qlog);
    if let Some(ech) = &args.ech {
        client.enable_ech(ech).expect("enable ECH");
    }
    if let Some(token) = resumption_token {
        client
            .enable_resumption(Instant::now(), token)
            .expect("enable resumption");
    }

    Ok(client)
}

impl TryFrom<Http3State> for CloseState {
    type Error = CloseReason;

    fn try_from(value: Http3State) -> Result<Self, Self::Error> {
        let (state, error) = match value {
            Http3State::Closing(error) => (Self::Closing, error),
            Http3State::Closed(error) => (Self::Closed, error),
            _ => return Ok(Self::NotClosing),
        };

        if error.is_error() {
            Err(error)
        } else {
            Ok(state)
        }
    }
}

impl super::Client for Http3Client {
    fn is_closed(&self) -> Result<CloseState, CloseReason> {
        self.state().try_into()
    }

    fn process_output(&mut self, now: Instant) -> Output {
        self.process_output(now)
    }

    fn process_multiple_input<'a>(
        &mut self,
        dgrams: impl IntoIterator<Item = Datagram<&'a [u8]>>,
        now: Instant,
    ) {
        self.process_multiple_input(dgrams, now);
    }

    fn close<S>(&mut self, now: Instant, app_error: AppError, msg: S)
    where
        S: AsRef<str> + Display,
    {
        self.close(now, app_error, msg);
    }

    fn stats(&self) -> neqo_transport::Stats {
        self.transport_stats()
    }

    fn has_events(&self) -> bool {
        neqo_common::event::Provider::has_events(self)
    }
}

impl Handler<'_> {
    fn reinit(&mut self) {
        for url in self.url_handler.handled_urls.drain(..) {
            self.url_handler.url_queue.push_front(url);
        }
        self.url_handler.stream_handlers.clear();
        self.url_handler.all_paths.clear();
    }
}

impl super::Handler for Handler<'_> {
    type Client = Http3Client;

    fn handle(&mut self, client: &mut Http3Client) -> Res<bool> {
        while let Some(event) = client.next_event() {
            match event {
                Http3ClientEvent::AuthenticationNeeded => {
                    client.authenticated(AuthenticationStatus::Ok, Instant::now());
                }
                Http3ClientEvent::HeaderReady {
                    stream_id,
                    headers,
                    fin,
                    ..
                } => {
                    if let Some(handler) = self.url_handler.stream_handler(stream_id) {
                        handler.process_header_ready(stream_id, fin, headers);
                    } else {
                        qwarn!("Data on unexpected stream: {stream_id}");
                    }
                    if fin {
                        self.url_handler.on_stream_fin(client, stream_id);
                    }
                }
                Http3ClientEvent::DataReadable { stream_id } => {
                    let mut stream_done = false;
                    match self.url_handler.stream_handler(stream_id) {
                        None => {
                            qwarn!("Data on unexpected stream: {stream_id}");
                        }
                        Some(handler) => loop {
                            let (sz, fin) = client
                                .read_data(Instant::now(), stream_id, &mut self.read_buffer)
                                .expect("Read should succeed");

                            handler.process_data_readable(
                                stream_id,
                                fin,
                                &self.read_buffer[..sz],
                                self.output_read_data,
                            )?;

                            if fin {
                                stream_done = true;
                                break;
                            }

                            if sz == 0 {
                                break;
                            }
                        },
                    }

                    if stream_done {
                        self.url_handler.on_stream_fin(client, stream_id);
                    }
                }
                Http3ClientEvent::DataWritable { stream_id } => {
                    match self.url_handler.stream_handler(stream_id) {
                        None => {
                            qwarn!("Data on unexpected stream: {stream_id}");
                        }
                        Some(handler) => {
                            handler.process_data_writable(client, stream_id);
                        }
                    }
                }
                Http3ClientEvent::StateChange(Http3State::Connected)
                | Http3ClientEvent::RequestsCreatable => {
                    qinfo!("{event:?}");
                    self.url_handler.process_urls(client);
                }
                Http3ClientEvent::ZeroRttRejected => {
                    qinfo!("{event:?}");
                    // All 0-RTT data was rejected. We need to retransmit it.
                    self.reinit();
                    self.url_handler.process_urls(client);
                }
                Http3ClientEvent::ResumptionToken(t) => self.token = Some(t),
                _ => {
                    qwarn!("Unhandled event {event:?}");
                }
            }
        }

        Ok(self.url_handler.done())
    }

    fn take_token(&mut self) -> Option<ResumptionToken> {
        self.token.take()
    }
}

trait StreamHandler {
    fn process_header_ready(&mut self, stream_id: StreamId, fin: bool, headers: Vec<Header>);
    fn process_data_readable(
        &mut self,
        stream_id: StreamId,
        fin: bool,
        data: &[u8],
        output_read_data: bool,
    ) -> Res<bool>;
    fn process_data_writable(&mut self, client: &mut Http3Client, stream_id: StreamId);
}

struct DownloadStreamHandler {
    out_file: Option<BufWriter<File>>,
}

impl StreamHandler for DownloadStreamHandler {
    fn process_header_ready(&mut self, stream_id: StreamId, fin: bool, headers: Vec<Header>) {
        if self.out_file.is_none() {
            qdebug!("READ HEADERS[{stream_id}]: fin={fin} {headers:?}");
        }
    }

    fn process_data_readable(
        &mut self,
        stream_id: StreamId,
        fin: bool,
        data: &[u8],
        output_read_data: bool,
    ) -> Res<bool> {
        if let Some(out_file) = &mut self.out_file {
            if !data.is_empty() {
                out_file.write_all(data)?;
            }
            return Ok(true);
        } else if !output_read_data {
            qdebug!("READ[{stream_id}]: {} bytes", data.len());
        } else if let Ok(txt) = std::str::from_utf8(data) {
            qdebug!("READ[{stream_id}]: {txt}");
        } else {
            qdebug!("READ[{}]: 0x{}", stream_id, hex(data));
        }

        if fin {
            if let Some(mut out_file) = self.out_file.take() {
                out_file.flush()?;
            } else {
                qdebug!("<FIN[{stream_id}]>");
            }
        }

        Ok(true)
    }

    fn process_data_writable(&mut self, _client: &mut Http3Client, _stream_id: StreamId) {}
}

struct UploadStreamHandler {
    data: SendData,
    start: Instant,
}

impl StreamHandler for UploadStreamHandler {
    fn process_header_ready(&mut self, stream_id: StreamId, fin: bool, headers: Vec<Header>) {
        qdebug!("READ HEADERS[{stream_id}]: fin={fin} {headers:?}");
    }

    fn process_data_readable(
        &mut self,
        stream_id: StreamId,
        _fin: bool,
        data: &[u8],
        _output_read_data: bool,
    ) -> Res<bool> {
        if let Ok(txt) = std::str::from_utf8(data) {
            let trimmed_txt = txt.trim_end_matches(char::from(0));
            let parsed: usize = trimmed_txt.parse().unwrap();
            if parsed == self.data.len() {
                let upload_time = Instant::now().duration_since(self.start);
                qinfo!("Stream ID: {stream_id:?}, Upload time: {upload_time:?}");
            }
        } else {
            panic!("Unexpected data [{}]: 0x{}", stream_id, hex(data));
        }
        Ok(true)
    }

    fn process_data_writable(&mut self, client: &mut Http3Client, stream_id: StreamId) {
        let done = self
            .data
            .send(|chunk| client.send_data(stream_id, chunk).unwrap());
        if done {
            client.stream_close_send(stream_id).unwrap();
        }
    }
}

struct UrlHandler<'a> {
    url_queue: VecDeque<Url>,
    handled_urls: Vec<Url>,
    stream_handlers: HashMap<StreamId, Box<dyn StreamHandler>>,
    all_paths: Vec<PathBuf>,
    args: &'a Args,
}

impl UrlHandler<'_> {
    fn stream_handler(&mut self, stream_id: StreamId) -> Option<&mut Box<dyn StreamHandler>> {
        self.stream_handlers.get_mut(&stream_id)
    }

    fn process_urls(&mut self, client: &mut Http3Client) {
        loop {
            if self.url_queue.is_empty() {
                break;
            }
            if self.stream_handlers.len() >= self.args.concurrency {
                break;
            }
            if !self.next_url(client) {
                break;
            }
        }
    }

    fn next_url(&mut self, client: &mut Http3Client) -> bool {
        let url = self
            .url_queue
            .pop_front()
            .expect("download_next called with empty queue");
        match client.fetch(
            Instant::now(),
            &self.args.method,
            &url,
            &to_headers(&self.args.header),
            Priority::default(),
        ) {
            Ok(client_stream_id) => {
                qdebug!("Successfully created stream id {client_stream_id} for {url}");

                let handler: Box<dyn StreamHandler> = match self.args.method.as_str() {
                    "GET" => {
                        let out_file = get_output_file(
                            &url,
                            self.args.output_dir.as_ref(),
                            &mut self.all_paths,
                        );
                        client.stream_close_send(client_stream_id).unwrap();
                        Box::new(DownloadStreamHandler { out_file })
                    }
                    "POST" => Box::new(UploadStreamHandler {
                        data: SendData::zeroes(self.args.upload_size),
                        start: Instant::now(),
                    }),
                    _ => unimplemented!(),
                };

                self.stream_handlers.insert(client_stream_id, handler);
                self.handled_urls.push(url);
                true
            }
            Err(
                Error::TransportError(TransportError::StreamLimitError)
                | Error::StreamLimitError
                | Error::Unavailable,
            ) => {
                self.url_queue.push_front(url);
                false
            }
            Err(e) => {
                panic!("Can't create stream {e}");
            }
        }
    }

    fn done(&self) -> bool {
        self.stream_handlers.is_empty() && self.url_queue.is_empty()
    }

    fn on_stream_fin(&mut self, client: &mut Http3Client, stream_id: StreamId) {
        self.stream_handlers.remove(&stream_id);
        self.process_urls(client);
    }
}

fn to_headers(values: &[impl AsRef<str>]) -> Vec<Header> {
    values
        .iter()
        .scan(None, |state, value| {
            if let Some(name) = state.take() {
                *state = None;
                Some(Header::new(name, value.as_ref()))
            } else {
                *state = Some(value.as_ref().to_string());
                None
            }
        })
        .collect()
}
