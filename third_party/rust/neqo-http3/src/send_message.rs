// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::{
    cell::RefCell,
    cmp::min,
    fmt::{self, Debug, Display, Formatter},
    num::NonZeroUsize,
    rc::Rc,
};

use neqo_common::{qdebug, qtrace, Encoder, Header, MessageType};
use neqo_qpack::encoder::QPackEncoder;
use neqo_transport::{Connection, StreamId};

use crate::{
    frames::HFrame,
    headers_checks::{headers_valid, is_interim, trailers_valid},
    BufferedStream, CloseType, Error, Http3StreamInfo, Http3StreamType, HttpSendStream, Res,
    SendStream, SendStreamEvents, Stream,
};

const MIN_DATA_FRAME_SIZE: usize = 3; // Minimal DATA frame size: 2 (header) + 1 (payload)
const MAX_DATA_HEADER_SIZE_2: usize = (1 << 6) - 1; // Maximal amount of data with DATA frame header size 2
const MAX_DATA_HEADER_SIZE_2_LIMIT: usize = MAX_DATA_HEADER_SIZE_2 + 3; // 63 + 3 (size of the next buffer data frame header)
const MAX_DATA_HEADER_SIZE_3: usize = (1 << 14) - 1; // Maximal amount of data with DATA frame header size 3
const MAX_DATA_HEADER_SIZE_3_LIMIT: usize = MAX_DATA_HEADER_SIZE_3 + 5; // 16383 + 5 (size of the next buffer data frame header)
const MAX_DATA_HEADER_SIZE_5: usize = (1 << 30) - 1; // Maximal amount of data with DATA frame header size 3
const MAX_DATA_HEADER_SIZE_5_LIMIT: usize = MAX_DATA_HEADER_SIZE_5 + 9; // 1073741823 + 9 (size of the next buffer data frame header)

/// A HTTP message, request and response, consists of headers, optional data and an optional
/// trailer header block. This state machine does not reflect what was already sent to the
/// transport layer but only reflect what has been supplied to the `SendMessage`. It is
/// represented by the following states:
///   `WaitingForHeaders` - the headers have not been supplied yet. In this state only a
///                         request/response header can be added. When headers are supplied
///                         the state changes to `WaitingForData`. A response may contain
///                         multiple messages only if all but the last one are informational(1xx)
///                         responses. The informational responses can only contain headers,
///                         therefore after an informational response is received the state
///                         machine states in `WaitingForHeaders` state.
///   `WaitingForData` - in this state, data and trailers can be supplied. This state means that
///                      a request or response header is already supplied.
///   `TrailersSet` - trailers have been supplied. At this stage no more data or headers can be
///                   supply only a fin.
///   `Done` - in this state no more data or headers can be added. This state is entered when the
///            message is closed.
#[derive(Debug, PartialEq)]
enum MessageState {
    WaitingForHeaders,
    WaitingForData,
    TrailersSet,
    Done,
}

impl MessageState {
    fn new_headers(&mut self, headers: &[Header], message_type: MessageType) -> Res<()> {
        match &self {
            Self::WaitingForHeaders => {
                // This is only a debug assertion because we expect that application will
                // do the right thing here and performing the check costs.
                debug_assert!(headers_valid(headers, message_type).is_ok());
                match message_type {
                    MessageType::Request => {
                        *self = Self::WaitingForData;
                    }
                    MessageType::Response => {
                        if !is_interim(headers)? {
                            *self = Self::WaitingForData;
                        }
                    }
                }
                Ok(())
            }
            Self::WaitingForData => {
                trailers_valid(headers)?;
                *self = Self::TrailersSet;
                Ok(())
            }
            Self::TrailersSet | Self::Done => Err(Error::InvalidInput),
        }
    }

    fn new_data(&self) -> Res<()> {
        if &Self::WaitingForData == self {
            Ok(())
        } else {
            Err(Error::InvalidInput)
        }
    }

    fn fin(&mut self) -> Res<()> {
        match &self {
            Self::WaitingForHeaders | Self::Done => Err(Error::InvalidInput),
            Self::WaitingForData | Self::TrailersSet => {
                *self = Self::Done;
                Ok(())
            }
        }
    }

    fn done(&self) -> bool {
        &Self::Done == self
    }
}

#[derive(Debug)]
pub struct SendMessage {
    state: MessageState,
    message_type: MessageType,
    stream_type: Http3StreamType,
    stream: BufferedStream,
    encoder: Rc<RefCell<QPackEncoder>>,
    conn_events: Box<dyn SendStreamEvents>,
}

impl SendMessage {
    pub fn new(
        message_type: MessageType,
        stream_type: Http3StreamType,
        stream_id: StreamId,
        encoder: Rc<RefCell<QPackEncoder>>,
        conn_events: Box<dyn SendStreamEvents>,
    ) -> Self {
        qdebug!("Create a request stream_id={stream_id}");
        Self {
            state: MessageState::WaitingForHeaders,
            message_type,
            stream_type,
            stream: BufferedStream::new(stream_id),
            encoder,
            conn_events,
        }
    }

    /// # Errors
    ///
    /// `ClosedCriticalStream` if the encoder stream is closed.
    /// `InternalError` if an unexpected error occurred.
    fn encode(
        encoder: &mut QPackEncoder,
        headers: &[Header],
        conn: &mut Connection,
        stream_id: StreamId,
    ) -> Vec<u8> {
        qdebug!("Encoding headers");
        let header_block = encoder.encode_header_block(conn, headers, stream_id);
        let hframe = HFrame::Headers {
            header_block: header_block.to_vec(),
        };
        let mut d = Encoder::default();
        hframe.encode(&mut d);
        d.into()
    }

    fn stream_id(&self) -> StreamId {
        Option::<StreamId>::from(&self.stream).expect("stream has ID")
    }

    fn get_stream_info(&self) -> Http3StreamInfo {
        Http3StreamInfo::new(self.stream_id(), Http3StreamType::Http)
    }
}

impl Stream for SendMessage {
    fn stream_type(&self) -> Http3StreamType {
        self.stream_type
    }
}
impl SendStream for SendMessage {
    fn send_data(&mut self, conn: &mut Connection, buf: &[u8]) -> Res<usize> {
        qtrace!("[{self}] send_body: len={}", buf.len());

        self.state.new_data()?;

        self.stream.send_buffer(conn)?;
        if self.stream.has_buffered_data() {
            return Ok(0);
        }
        let available = conn
            .stream_avail_send_space(self.stream_id())
            .map_err(|e| Error::map_stream_send_errors(&e.into()))?;
        if available < MIN_DATA_FRAME_SIZE {
            // Setting this once, instead of every time the available send space
            // is exhausted, would suffice. That said, function call should be
            // cheap, thus not worth optimizing.
            conn.stream_set_writable_event_low_watermark(
                self.stream_id(),
                NonZeroUsize::new(MIN_DATA_FRAME_SIZE).ok_or(Error::Internal)?,
            )?;
            return Ok(0);
        }
        let to_send = if available <= MAX_DATA_HEADER_SIZE_2_LIMIT {
            // 63 + 3
            min(min(buf.len(), available - 2), MAX_DATA_HEADER_SIZE_2)
        } else if available <= MAX_DATA_HEADER_SIZE_3_LIMIT {
            // 16383 + 5
            min(min(buf.len(), available - 3), MAX_DATA_HEADER_SIZE_3)
        } else if available <= MAX_DATA_HEADER_SIZE_5 {
            // 1073741823 + 9
            min(min(buf.len(), available - 5), MAX_DATA_HEADER_SIZE_5_LIMIT)
        } else {
            min(buf.len(), available - 9)
        };

        qdebug!("[{self}] send_request_body: available={available} to_send={to_send}");

        let data_frame = HFrame::Data {
            len: to_send as u64,
        };
        let mut enc = Encoder::default();
        data_frame.encode(&mut enc);
        let sent_fh = self
            .stream
            .send_atomic(conn, enc.as_ref())
            .map_err(|e| Error::map_stream_send_errors(&e))?;
        debug_assert!(sent_fh);

        let sent = self
            .stream
            .send_atomic(conn, &buf[..to_send])
            .map_err(|e| Error::map_stream_send_errors(&e))?;
        debug_assert!(sent);
        Ok(to_send)
    }

    fn done(&self) -> bool {
        !self.stream.has_buffered_data() && self.state.done()
    }

    fn stream_writable(&self) {
        if !self.stream.has_buffered_data() && !self.state.done() {
            // DataWritable is just a signal for an application to try to write more data,
            // if writing fails it is fine. Therefore we do not need to properly check
            // whether more credits are available on the transport layer.
            self.conn_events.data_writable(self.get_stream_info());
        }
    }

    /// # Errors
    ///
    /// `InternalError` if an unexpected error occurred.
    /// `InvalidStreamId` if the stream does not exist,
    /// `AlreadyClosed` if the stream has already been closed.
    /// `TransportStreamDoesNotExist` if the transport stream does not exist (this may happen if
    /// `process_output` has not been called when needed, and HTTP3 layer has not picked up the
    /// info that the stream has been closed.)
    fn send(&mut self, conn: &mut Connection) -> Res<()> {
        let sent = Error::map_error(self.stream.send_buffer(conn), Error::HttpInternal(5))?;

        qtrace!("[{self}] {sent} bytes sent");
        if !self.stream.has_buffered_data() {
            if self.state.done() {
                Error::map_error(
                    conn.stream_close_send(self.stream_id()),
                    Error::HttpInternal(6),
                )?;
                qtrace!("[{self}] done sending request");
            } else {
                // DataWritable is just a signal for an application to try to write more data,
                // if writing fails it is fine. Therefore we do not need to properly check
                // whether more credits are available on the transport layer.
                self.conn_events.data_writable(self.get_stream_info());
            }
        }
        Ok(())
    }

    // SendMessage owns headers and sends them. It may also own data for the server side.
    // This method returns if they're still being sent. Request body (if any) is sent by
    // http client afterwards using `send_request_body` after receiving DataWritable event.
    fn has_data_to_send(&self) -> bool {
        self.stream.has_buffered_data()
    }

    fn close(&mut self, conn: &mut Connection) -> Res<()> {
        self.state.fin()?;
        if !self.stream.has_buffered_data() {
            conn.stream_close_send(self.stream_id())?;
        }

        self.conn_events
            .send_closed(self.get_stream_info(), CloseType::Done);
        Ok(())
    }

    fn handle_stop_sending(&mut self, close_type: CloseType) {
        if !self.state.done() {
            self.conn_events
                .send_closed(self.get_stream_info(), close_type);
        }
    }

    fn http_stream(&mut self) -> Option<&mut dyn HttpSendStream> {
        Some(self)
    }

    fn send_data_atomic(&mut self, conn: &mut Connection, buf: &[u8]) -> Res<()> {
        let data_frame = HFrame::Data {
            len: buf.len() as u64,
        };
        let mut enc = Encoder::default();
        data_frame.encode(&mut enc);
        self.stream.buffer(enc.as_ref());
        self.stream.buffer(buf);
        _ = self.stream.send_buffer(conn)?;
        Ok(())
    }
}

impl HttpSendStream for SendMessage {
    fn send_headers(&mut self, headers: &[Header], conn: &mut Connection) -> Res<()> {
        self.state.new_headers(headers, self.message_type)?;
        let buf = Self::encode(
            &mut self.encoder.borrow_mut(),
            headers,
            conn,
            self.stream_id(),
        );
        self.stream.buffer(&buf);
        Ok(())
    }

    fn set_new_listener(&mut self, conn_events: Box<dyn SendStreamEvents>) {
        self.stream_type = Http3StreamType::ExtendedConnect;
        self.conn_events = conn_events;
    }
}

impl Display for SendMessage {
    fn fmt(&self, f: &mut Formatter) -> fmt::Result {
        write!(f, "SendMesage {}", self.stream_id())
    }
}
