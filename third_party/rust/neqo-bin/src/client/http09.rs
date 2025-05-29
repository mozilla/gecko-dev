// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![expect(clippy::unwrap_used, reason = "This is example code.")]

//! An [HTTP 0.9](https://www.w3.org/Protocols/HTTP/AsImplemented.html) client implementation.

use std::{
    cell::RefCell,
    collections::{HashMap, VecDeque},
    fmt::Display,
    fs::File,
    io::{BufWriter, Write as _},
    net::SocketAddr,
    path::PathBuf,
    rc::Rc,
    time::Instant,
};

use neqo_common::{event::Provider, qdebug, qinfo, qwarn, Datagram};
use neqo_crypto::{AuthenticationStatus, ResumptionToken};
use neqo_transport::{
    CloseReason, Connection, ConnectionEvent, ConnectionIdGenerator, EmptyConnectionIdGenerator,
    Error, Output, RandomConnectionIdGenerator, State, StreamId, StreamType,
};
use url::Url;

use super::{get_output_file, qlog_new, Args, CloseState, Res};
use crate::STREAM_IO_BUFFER_SIZE;

pub struct Handler<'a> {
    streams: HashMap<StreamId, Option<BufWriter<File>>>,
    url_queue: VecDeque<Url>,
    handled_urls: Vec<Url>,
    all_paths: Vec<PathBuf>,
    args: &'a Args,
    token: Option<ResumptionToken>,
    needs_key_update: bool,
    read_buffer: Vec<u8>,
}

impl Handler<'_> {
    fn reinit(&mut self) {
        for url in self.handled_urls.drain(..) {
            self.url_queue.push_front(url);
        }
        self.streams.clear();
        self.all_paths.clear();
    }
}

impl super::Handler for Handler<'_> {
    type Client = Connection;

    fn handle(&mut self, client: &mut Self::Client) -> Res<bool> {
        while let Some(event) = client.next_event() {
            if self.needs_key_update {
                match client.initiate_key_update() {
                    Ok(()) => {
                        qdebug!("Keys updated");
                        self.needs_key_update = false;
                        self.download_urls(client);
                    }
                    Err(Error::KeyUpdateBlocked) => (),
                    Err(e) => return Err(e.into()),
                }
            }

            match event {
                ConnectionEvent::AuthenticationNeeded => {
                    client.authenticated(AuthenticationStatus::Ok, Instant::now());
                }
                ConnectionEvent::RecvStreamReadable { stream_id } => {
                    self.read(client, stream_id)?;
                }
                ConnectionEvent::SendStreamWritable { stream_id } => {
                    qdebug!("stream {stream_id} writable");
                }
                ConnectionEvent::SendStreamComplete { stream_id } => {
                    qdebug!("stream {stream_id} complete");
                }
                ConnectionEvent::SendStreamCreatable { stream_type } => {
                    qdebug!("stream {stream_type:?} creatable");
                    if stream_type == StreamType::BiDi {
                        self.download_urls(client);
                    }
                }
                ConnectionEvent::StateChange(
                    State::WaitInitial | State::Handshaking | State::Connected,
                ) => {
                    qdebug!("{event:?}");
                    self.download_urls(client);
                }
                ConnectionEvent::ZeroRttRejected => {
                    qdebug!("{event:?}");
                    // All 0-RTT data was rejected. We need to retransmit it.
                    self.reinit();
                    self.download_urls(client);
                }
                ConnectionEvent::ResumptionToken(token) => {
                    self.token = Some(token);
                }
                _ => {
                    qwarn!("Unhandled event {event:?}");
                }
            }
        }

        if !self.streams.is_empty() || !self.url_queue.is_empty() {
            return Ok(false);
        }

        if self.args.resume && self.token.is_none() {
            self.token = client.take_resumption_token(Instant::now());
        }

        Ok(true)
    }

    fn take_token(&mut self) -> Option<ResumptionToken> {
        self.token.take()
    }
}

pub fn create_client(
    args: &Args,
    local_addr: SocketAddr,
    remote_addr: SocketAddr,
    hostname: &str,
    resumption_token: Option<ResumptionToken>,
) -> Res<Connection> {
    let alpn = match args.shared.alpn.as_str() {
        "hq-29" | "hq-30" | "hq-31" | "hq-32" => args.shared.alpn.as_str(),
        _ => "hq-interop",
    };
    let cid_generator: Rc<RefCell<dyn ConnectionIdGenerator>> = if args.cid_len == 0 {
        Rc::new(RefCell::new(EmptyConnectionIdGenerator::default()))
    } else {
        Rc::new(RefCell::new(RandomConnectionIdGenerator::new(
            args.cid_len.into(),
        )))
    };
    let mut client = Connection::new_client(
        hostname,
        &[alpn],
        cid_generator,
        local_addr,
        remote_addr,
        args.shared.quic_parameters.get(alpn),
        Instant::now(),
    )?;

    if let Some(tok) = resumption_token {
        client.enable_resumption(Instant::now(), tok)?;
    }

    let ciphers = args.get_ciphers();
    if !ciphers.is_empty() {
        client.set_ciphers(&ciphers)?;
    }

    client.set_qlog(qlog_new(
        args,
        hostname,
        client.odcid().ok_or(Error::InternalError)?,
    )?);

    Ok(client)
}

impl TryFrom<&State> for CloseState {
    type Error = CloseReason;

    fn try_from(value: &State) -> Result<Self, Self::Error> {
        let (state, error) = match value {
            State::Closing { error, .. } | State::Draining { error, .. } => (Self::Closing, error),
            State::Closed(error) => (Self::Closed, error),
            _ => return Ok(Self::NotClosing),
        };

        if error.is_error() {
            Err(error.clone())
        } else {
            Ok(state)
        }
    }
}

impl super::Client for Connection {
    fn process_output(&mut self, now: Instant) -> Output {
        self.process_output(now)
    }

    fn process_multiple_input<'a>(
        &mut self,
        dgrams: impl IntoIterator<Item = Datagram<&'a mut [u8]>>,
        now: Instant,
    ) {
        self.process_multiple_input(dgrams, now);
    }

    fn close<S>(&mut self, now: Instant, app_error: neqo_transport::AppError, msg: S)
    where
        S: AsRef<str> + Display,
    {
        if !self.state().closed() {
            self.close(now, app_error, msg);
        }
    }

    fn is_closed(&self) -> Result<CloseState, CloseReason> {
        self.state().try_into()
    }

    fn stats(&self) -> neqo_transport::Stats {
        self.stats()
    }

    fn has_events(&self) -> bool {
        Provider::has_events(self)
    }
}

impl<'b> Handler<'b> {
    pub fn new(url_queue: VecDeque<Url>, args: &'b Args) -> Self {
        Self {
            streams: HashMap::new(),
            url_queue,
            handled_urls: Vec::new(),
            all_paths: Vec::new(),
            args,
            token: None,
            needs_key_update: args.key_update,
            read_buffer: vec![0; STREAM_IO_BUFFER_SIZE],
        }
    }

    fn download_urls(&mut self, client: &mut Connection) {
        loop {
            if self.url_queue.is_empty() {
                break;
            }
            if self.streams.len() >= self.args.concurrency {
                break;
            }
            if !self.download_next(client) {
                break;
            }
        }
    }

    fn download_next(&mut self, client: &mut Connection) -> bool {
        if self.needs_key_update {
            qdebug!("Deferring requests until after first key update");
            return false;
        }
        let url = self
            .url_queue
            .pop_front()
            .expect("download_next called with empty queue");
        match client.stream_create(StreamType::BiDi) {
            Ok(client_stream_id) => {
                qinfo!("Created stream {client_stream_id} for {url}");
                let req = format!("GET {}\r\n", url.path());
                _ = client
                    .stream_send(client_stream_id, req.as_bytes())
                    .unwrap();
                client.stream_close_send(client_stream_id).unwrap();
                let out_file =
                    get_output_file(&url, self.args.output_dir.as_ref(), &mut self.all_paths);
                self.streams.insert(client_stream_id, out_file);
                self.handled_urls.push(url);
                true
            }
            Err(e @ (Error::StreamLimitError | Error::ConnectionState)) => {
                qwarn!("Cannot create stream {e:?}");
                self.url_queue.push_front(url);
                false
            }
            Err(e) => {
                panic!("Error creating stream {e:?}");
            }
        }
    }

    /// Read and maybe print received data from a stream.
    // Returns bool: was fin received?
    fn read_from_stream(
        client: &mut Connection,
        stream_id: StreamId,
        read_buffer: &mut [u8],
        output_read_data: bool,
        maybe_out_file: &mut Option<BufWriter<File>>,
    ) -> Res<bool> {
        loop {
            let (sz, fin) = client.stream_recv(stream_id, read_buffer)?;
            if sz == 0 {
                return Ok(fin);
            }
            let read_buffer = &read_buffer[0..sz];

            if let Some(out_file) = maybe_out_file {
                out_file.write_all(read_buffer)?;
            } else if !output_read_data {
                qdebug!("READ[{stream_id}]: {} bytes", read_buffer.len());
            } else {
                qdebug!(
                    "READ[{stream_id}]: {}",
                    std::str::from_utf8(read_buffer).map_err(|_| Error::InvalidInput)?
                );
            }
            if fin {
                return Ok(true);
            }
        }
    }

    fn read(&mut self, client: &mut Connection, stream_id: StreamId) -> Res<()> {
        match self.streams.get_mut(&stream_id) {
            None => {
                qwarn!("Data on unexpected stream: {stream_id}");
                return Ok(());
            }
            Some(maybe_out_file) => {
                let fin_recvd = Self::read_from_stream(
                    client,
                    stream_id,
                    &mut self.read_buffer,
                    self.args.output_read_data,
                    maybe_out_file,
                )?;

                if fin_recvd {
                    if let Some(mut out_file) = maybe_out_file.take() {
                        out_file.flush()?;
                    } else {
                        qinfo!("<FIN[{stream_id}]>");
                    }
                    self.streams.remove(&stream_id);
                    self.download_urls(client);
                }
            }
        }
        Ok(())
    }
}
