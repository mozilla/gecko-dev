// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::fmt::{self, Display, Formatter};

use neqo_transport::{Connection, StreamId};

use crate::{qlog, Res};

#[derive(Debug, PartialEq, Eq, Default)]
pub enum BufferedStream {
    #[default]
    Uninitialized,
    Initialized {
        stream_id: StreamId,
        buf: Vec<u8>,
    },
}

impl Display for BufferedStream {
    fn fmt(&self, f: &mut Formatter) -> fmt::Result {
        write!(f, "BufferedStream {:?}", Option::<StreamId>::from(self))
    }
}

impl BufferedStream {
    #[must_use]
    pub const fn new(stream_id: StreamId) -> Self {
        Self::Initialized {
            stream_id,
            buf: Vec::new(),
        }
    }

    /// # Panics
    ///
    /// If the `BufferedStream` is initialized more than once, it will panic.
    pub fn init(&mut self, stream_id: StreamId) {
        debug_assert!(&Self::Uninitialized == self);
        *self = Self::Initialized {
            stream_id,
            buf: Vec::new(),
        };
    }

    /// # Panics
    ///
    /// This function cannot be called before the `BufferedStream` is initialized.
    pub fn buffer(&mut self, to_buf: &[u8]) {
        if let Self::Initialized { buf, .. } = self {
            buf.extend_from_slice(to_buf);
        } else {
            debug_assert!(false, "Do not buffer date before the stream is initialized");
        }
    }

    /// # Errors
    ///
    /// Returns `neqo_transport` errors.
    pub fn send_buffer(&mut self, conn: &mut Connection) -> Res<usize> {
        let Self::Initialized { stream_id, buf } = self else {
            return Ok(0);
        };
        if buf.is_empty() {
            return Ok(0);
        }
        let sent = conn.stream_send(*stream_id, &buf[..])?;
        if sent == 0 {
            return Ok(0);
        } else if sent == buf.len() {
            buf.clear();
        } else {
            let b = buf.split_off(sent);
            *buf = b;
        }
        qlog::h3_data_moved_down(conn.qlog_mut(), *stream_id, sent);
        Ok(sent)
    }

    /// # Errors
    ///
    /// Returns `neqo_transport` errors.
    pub fn send_atomic(&mut self, conn: &mut Connection, to_send: &[u8]) -> Res<bool> {
        // First try to send anything that is in the buffer.
        self.send_buffer(conn)?;
        let Self::Initialized { stream_id, buf } = self else {
            return Ok(false);
        };
        if !buf.is_empty() {
            return Ok(false);
        }
        let res = conn.stream_send_atomic(*stream_id, to_send)?;
        if res {
            qlog::h3_data_moved_down(conn.qlog_mut(), *stream_id, to_send.len());
        }
        Ok(res)
    }

    #[must_use]
    pub fn has_buffered_data(&self) -> bool {
        if let Self::Initialized { buf, .. } = self {
            !buf.is_empty()
        } else {
            false
        }
    }
}

impl From<&BufferedStream> for Option<StreamId> {
    fn from(stream: &BufferedStream) -> Self {
        if let BufferedStream::Initialized { stream_id, .. } = stream {
            Some(*stream_id)
        } else {
            None
        }
    }
}
