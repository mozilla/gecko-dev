// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

// Buffering data to send until it is acked.

#![allow(
    clippy::module_name_repetitions,
    reason = "<https://github.com/mozilla/neqo/issues/2284#issuecomment-2782711813>"
)]

use std::{
    cell::RefCell,
    cmp::{max, min, Ordering},
    collections::{btree_map::Entry, BTreeMap, VecDeque},
    fmt::{self, Display, Formatter},
    mem,
    num::NonZeroUsize,
    ops::Add,
    rc::Rc,
};

use indexmap::IndexMap;
use neqo_common::{qdebug, qerror, qtrace, Encoder, Role};
use smallvec::SmallVec;

use crate::{
    events::ConnectionEvents,
    fc::SenderFlowControl,
    frame::{Frame, FrameType},
    packet::PacketBuilder,
    recovery::{RecoveryToken, StreamRecoveryToken},
    stats::FrameStats,
    stream_id::StreamId,
    streams::SendOrder,
    tparams::{
        TransportParameterId::{InitialMaxStreamDataBidiRemote, InitialMaxStreamDataUni},
        TransportParameters,
    },
    AppError, Error, Res,
};

/// The maximum stream send buffer size.
///
/// See [`crate::recv_stream::MAX_RECV_WINDOW_SIZE`] for an explanation of this
/// concrete value.
///
/// Keep in sync with [`crate::recv_stream::MAX_RECV_WINDOW_SIZE`].
pub const MAX_SEND_BUFFER_SIZE: usize = 10 * 1024 * 1024;

/// The priority that is assigned to sending data for the stream.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default, PartialOrd, Ord)]
pub enum TransmissionPriority {
    /// This stream is more important than the functioning of the connection.
    /// Don't use this priority unless the stream really is that important.
    /// A stream at this priority can starve out other connection functions,
    /// including flow control, which could be very bad.
    Critical,
    /// The stream is very important.  Stream data will be written ahead of
    /// some of the less critical connection functions, like path validation,
    /// connection ID management, and session tickets.
    Important,
    /// High priority streams are important, but not enough to disrupt
    /// connection operation.  They go ahead of session tickets though.
    High,
    /// The default priority.
    #[default]
    Normal,
    /// Low priority streams get sent last.
    Low,
}

impl Add<RetransmissionPriority> for TransmissionPriority {
    type Output = Self;
    fn add(self, rhs: RetransmissionPriority) -> Self::Output {
        match rhs {
            RetransmissionPriority::Fixed(fixed) => fixed,
            RetransmissionPriority::Same => self,
            RetransmissionPriority::Higher => match self {
                Self::Critical => Self::Critical,
                Self::Important | Self::High => Self::Important,
                Self::Normal => Self::High,
                Self::Low => Self::Normal,
            },
            RetransmissionPriority::MuchHigher => match self {
                Self::Critical | Self::Important => Self::Critical,
                Self::High | Self::Normal => Self::Important,
                Self::Low => Self::High,
            },
        }
    }
}

/// If data is lost, this determines the priority that applies to retransmissions
/// of that data.
#[derive(Debug, Default, Clone, Copy, PartialEq, Eq)]
pub enum RetransmissionPriority {
    /// Prioritize retransmission at a fixed priority.
    /// With this, it is possible to prioritize retransmissions lower than transmissions.
    /// Doing that can create a deadlock with flow control which might cause the connection
    /// to stall unless new data stops arriving fast enough that retransmissions can complete.
    Fixed(TransmissionPriority),
    /// Don't increase priority for retransmission.  This is probably not a good idea
    /// as it could mean starving flow control.
    Same,
    /// Increase the priority of retransmissions (the default).
    /// Retransmissions of `Critical` or `Important` aren't elevated at all.
    #[default]
    Higher,
    /// Increase the priority of retransmissions a lot.
    /// This is useful for streams that are particularly exposed to head-of-line blocking.
    MuchHigher,
}

#[derive(Debug, PartialEq, Eq, Clone, Copy)]
enum RangeState {
    Sent,
    Acked,
}

/// Track ranges in the stream as sent or acked. Acked implies sent. Not in a
/// range implies needing-to-be-sent, either initially or as a retransmission.
#[derive(Debug, Default, PartialEq, Eq)]
pub struct RangeTracker {
    /// The number of bytes that have been acknowledged starting from offset 0.
    acked: u64,
    /// A map that tracks the state of ranges.
    /// Keys are the offset of the start of the range.
    /// Values is a tuple of the range length and its state.
    used: BTreeMap<u64, (u64, RangeState)>,
    /// This is a cache for the output of `first_unmarked_range`, which we check a lot.
    first_unmarked: Option<(u64, Option<u64>)>,
}

impl RangeTracker {
    fn highest_offset(&self) -> u64 {
        self.used
            .last_key_value()
            .map_or(self.acked, |(&k, &(v, _))| k + v)
    }

    const fn acked_from_zero(&self) -> u64 {
        self.acked
    }

    /// Find the first unmarked range. If all are contiguous, this will return
    /// (`highest_offset()`, None).
    fn first_unmarked_range(&mut self) -> (u64, Option<u64>) {
        if let Some(first_unmarked) = self.first_unmarked {
            return first_unmarked;
        }

        let mut prev_end = self.acked;

        for (&cur_off, &(cur_len, _)) in &self.used {
            if prev_end == cur_off {
                prev_end = cur_off + cur_len;
            } else {
                let res = (prev_end, Some(cur_off - prev_end));
                self.first_unmarked = Some(res);
                return res;
            }
        }
        self.first_unmarked = Some((prev_end, None));
        (prev_end, None)
    }

    /// When the range of acknowledged bytes from zero increases, we need to drop any
    /// ranges within that span AND maybe extend it to include any adjacent acknowledged ranges.
    fn coalesce_acked(&mut self) {
        while let Some(e) = self.used.first_entry() {
            match self.acked.cmp(e.key()) {
                Ordering::Greater => {
                    let (off, (len, state)) = e.remove_entry();
                    let overflow = (off + len).saturating_sub(self.acked);
                    if overflow > 0 {
                        if state == RangeState::Acked {
                            self.acked += overflow;
                        } else {
                            self.used.insert(self.acked, (overflow, state));
                        }
                        break;
                    }
                }
                Ordering::Equal => {
                    if e.get().1 == RangeState::Acked {
                        let (len, _) = e.remove();
                        self.acked += len;
                    }
                    break;
                }
                Ordering::Less => break,
            }
        }
    }

    /// Mark a range as acknowledged.  This is simpler than marking a range as sent
    /// because an acknowledged range can never turn back into a sent range, so
    /// this function can just override the entire range.
    ///
    /// The only tricky parts are making sure that we maintain `self.acked`,
    /// which is the first acknowledged range.  And making sure that we don't create
    /// ranges of the same type that are adjacent; these need to be merged.
    #[allow(
        clippy::allow_attributes,
        clippy::missing_panics_doc,
        reason = "OK here."
    )]
    pub fn mark_acked(&mut self, new_off: u64, new_len: usize) {
        let end = new_off + u64::try_from(new_len).expect("usize fits in u64");
        let new_off = max(self.acked, new_off);
        let mut new_len = end.saturating_sub(new_off);
        if new_len == 0 {
            return;
        }

        self.first_unmarked = None;
        if new_off == self.acked {
            self.acked += new_len;
            self.coalesce_acked();
            return;
        }
        let mut new_end = new_off + new_len;

        // Get all existing ranges that start within this new range.
        let mut covered = self
            .used
            .range(new_off..new_end)
            .map(|(&k, _)| k)
            .collect::<SmallVec<[_; 8]>>();

        if let Entry::Occupied(next_entry) = self.used.entry(new_end) {
            // Check if the very next entry is the same type as this.
            if next_entry.get().1 == RangeState::Acked {
                // If is is acked, drop it and extend this new range.
                let (extra_len, _) = next_entry.remove();
                new_len += extra_len;
                new_end += extra_len;
            }
        } else if let Some(last) = covered.pop() {
            // Otherwise, the last of the existing ranges might overhang this one by some.
            let (old_off, (old_len, old_state)) =
                self.used.remove_entry(&last).expect("entry exists"); // can't fail
            let remainder = (old_off + old_len).saturating_sub(new_end);
            if remainder > 0 {
                if old_state == RangeState::Acked {
                    // Just extend the current range.
                    new_len += remainder;
                    new_end += remainder;
                } else {
                    self.used.insert(new_end, (remainder, RangeState::Sent));
                }
            }
        }
        // All covered ranges can just be trashed.
        for k in covered {
            self.used.remove(&k);
        }

        // Now either merge with a preceding acked range
        // or cut a preceding sent range as needed.
        let prev = self.used.range_mut(..new_off).next_back();
        if let Some((prev_off, (prev_len, prev_state))) = prev {
            let prev_end = *prev_off + *prev_len;
            if prev_end >= new_off {
                if *prev_state == RangeState::Sent {
                    *prev_len = new_off - *prev_off;
                    if prev_end > new_end {
                        // There is some extra sent range after the new acked range.
                        self.used
                            .insert(new_end, (prev_end - new_end, RangeState::Sent));
                    }
                } else {
                    *prev_len = max(prev_end, new_end) - *prev_off;
                    return;
                }
            }
        }
        self.used.insert(new_off, (new_len, RangeState::Acked));
    }

    /// Turn a single sent range into a list of subranges that align with existing
    /// acknowledged ranges.
    ///
    /// This is more complicated than adding acked ranges because any acked ranges
    /// need to be kept in place, with sent ranges filling the gaps.
    ///
    /// This means:
    /// ```ignore
    ///   AAA S AAAS AAAAA
    /// +  SSSSSSSSSSSSS
    /// = AAASSSAAASSAAAAA
    /// ```
    ///
    /// But we also have to ensure that:
    /// ```ignore
    ///     SSSS
    /// + SS
    /// = SSSSSS
    /// ```
    /// and
    /// ```ignore
    ///   SSSSS
    /// +     SS
    /// = SSSSSS
    /// ```
    #[allow(
        clippy::allow_attributes,
        clippy::missing_panics_doc,
        reason = "OK here."
    )]
    pub fn mark_sent(&mut self, mut new_off: u64, new_len: usize) {
        let new_end = new_off + u64::try_from(new_len).expect("usize fits in u64");
        new_off = max(self.acked, new_off);
        let mut new_len = new_end.saturating_sub(new_off);
        if new_len == 0 {
            return;
        }

        self.first_unmarked = None;

        // Get all existing ranges that start within this new range.
        let covered = self
            .used
            .range(new_off..(new_off + new_len))
            .map(|(&k, _)| k)
            .collect::<SmallVec<[u64; 8]>>();

        if let Entry::Occupied(next_entry) = self.used.entry(new_end) {
            if next_entry.get().1 == RangeState::Sent {
                // Check if the very next entry is the same type as this, so it can be merged.
                let (extra_len, _) = next_entry.remove();
                new_len += extra_len;
            }
        }

        // Merge with any preceding sent range that might overlap,
        // or cut the head of this if the preceding range is acked.
        let prev = self.used.range(..new_off).next_back();
        if let Some((&prev_off, &(prev_len, prev_state))) = prev {
            if prev_off + prev_len >= new_off {
                let overlap = prev_off + prev_len - new_off;
                new_len = new_len.saturating_sub(overlap);
                if new_len == 0 {
                    // The previous range completely covers this one (no more to do).
                    return;
                }

                if prev_state == RangeState::Acked {
                    // The previous range is acked, so it cuts this one.
                    new_off += overlap;
                } else {
                    // Extend the current range backwards.
                    new_off = prev_off;
                    new_len += prev_len;
                    // The previous range will be updated below.
                    // It might need to be cut because of a covered acked range.
                }
            }
        }

        // Now interleave new sent chunks with any existing acked chunks.
        for old_off in covered {
            let Entry::Occupied(e) = self.used.entry(old_off) else {
                unreachable!();
            };
            let &(old_len, old_state) = e.get();
            if old_state == RangeState::Acked {
                // Now we have to insert a chunk ahead of this acked chunk.
                let chunk_len = old_off - new_off;
                if chunk_len > 0 {
                    self.used.insert(new_off, (chunk_len, RangeState::Sent));
                }
                let included = chunk_len + old_len;
                new_len = new_len.saturating_sub(included);
                if new_len == 0 {
                    return;
                }
                new_off += included;
            } else {
                let overhang = (old_off + old_len).saturating_sub(new_off + new_len);
                new_len += overhang;
                if *e.key() != new_off {
                    // Retain a sent entry at `new_off`.
                    // This avoids the work of removing and re-creating an entry.
                    // The value will be overwritten when the next insert occurs,
                    // either when this loop hits an acked range (above)
                    // or for any remainder (below).
                    e.remove();
                }
            }
        }

        self.used.insert(new_off, (new_len, RangeState::Sent));
    }

    fn unmark_range(&mut self, off: u64, len: usize) {
        if len == 0 {
            qdebug!("unmark 0-length range at {off}");
            return;
        }

        self.first_unmarked = None;
        let len = u64::try_from(len).expect("usize fits in u64");
        let end_off = off + len;

        let mut to_remove = SmallVec::<[_; 8]>::new();
        let mut to_add = None;

        // Walk backwards through possibly affected existing ranges
        for (cur_off, (cur_len, cur_state)) in self.used.range_mut(..off + len).rev() {
            // Maybe fixup range preceding the removed range
            if *cur_off < off {
                // Check for overlap
                if *cur_off + *cur_len > off {
                    if *cur_state == RangeState::Acked {
                        qdebug!(
                            "Attempted to unmark Acked range {cur_off}-{cur_len} with unmark_range {off}-{}",
                            off + len
                        );
                    } else {
                        *cur_len = off - cur_off;
                    }
                }
                break;
            }

            if *cur_state == RangeState::Acked {
                qdebug!(
                    "Attempted to unmark Acked range {cur_off}-{cur_len} with unmark_range {off}-{}",
                    off + len
                );
                continue;
            }

            // Add a new range for old subrange extending beyond
            // to-be-unmarked range
            let cur_end_off = cur_off + *cur_len;
            if cur_end_off > end_off {
                let new_cur_off = off + len;
                let new_cur_len = cur_end_off - end_off;
                assert_eq!(to_add, None);
                to_add = Some((new_cur_off, new_cur_len, *cur_state));
            }

            to_remove.push(*cur_off);
        }

        for remove_off in to_remove {
            self.used.remove(&remove_off);
        }

        if let Some((new_cur_off, new_cur_len, cur_state)) = to_add {
            self.used.insert(new_cur_off, (new_cur_len, cur_state));
        }
    }

    /// Unmark all sent ranges.
    /// # Panics
    /// On 32-bit machines where far too much is sent before calling this.
    /// Note that this should not be called for handshakes, which should never exceed that limit.
    pub fn unmark_sent(&mut self) {
        self.unmark_range(
            0,
            usize::try_from(self.highest_offset()).expect("u64 fits in usize"),
        );
    }
}

/// Buffer to contain queued bytes and track their state.
#[derive(Debug, Default, PartialEq, Eq)]
pub struct TxBuffer {
    send_buf: VecDeque<u8>, // buffer of not-acked bytes
    ranges: RangeTracker,   // ranges in buffer that have been sent or acked
}

impl TxBuffer {
    #[must_use]
    pub fn new() -> Self {
        Self::default()
    }

    /// Attempt to add some or all of the passed-in buffer to the `TxBuffer`.
    pub fn send(&mut self, buf: &[u8]) -> usize {
        let can_buffer = min(MAX_SEND_BUFFER_SIZE - self.buffered(), buf.len());
        if can_buffer > 0 {
            self.send_buf.extend(&buf[..can_buffer]);
            debug_assert!(self.send_buf.len() <= MAX_SEND_BUFFER_SIZE);
        }
        can_buffer
    }

    fn first_unmarked_range(&mut self) -> Option<(u64, Option<u64>)> {
        let (start, maybe_len) = self.ranges.first_unmarked_range();
        let buffered = u64::try_from(self.buffered()).ok()?;
        (start != self.retired() + buffered).then_some((start, maybe_len))
    }

    pub fn is_empty(&mut self) -> bool {
        self.first_unmarked_range().is_none()
    }

    pub fn next_bytes(&mut self) -> Option<(u64, &[u8])> {
        let (start, maybe_len) = self.first_unmarked_range()?;

        // Convert from ranges-relative-to-zero to
        // ranges-relative-to-buffer-start
        let buff_off = usize::try_from(start - self.retired()).ok()?;

        // Deque returns two slices. Create a subslice from whichever
        // one contains the first unmarked data.
        let slc = if buff_off < self.send_buf.as_slices().0.len() {
            &self.send_buf.as_slices().0[buff_off..]
        } else {
            &self.send_buf.as_slices().1[buff_off - self.send_buf.as_slices().0.len()..]
        };

        let len = maybe_len.map_or(slc.len(), |range_len| {
            min(usize::try_from(range_len).unwrap_or(usize::MAX), slc.len())
        });

        debug_assert!(len > 0);
        debug_assert!(len <= slc.len());

        Some((start, &slc[..len]))
    }

    pub fn mark_as_sent(&mut self, offset: u64, len: usize) {
        self.ranges.mark_sent(offset, len);
    }

    #[allow(
        clippy::allow_attributes,
        clippy::missing_panics_doc,
        reason = "OK here."
    )]
    pub fn mark_as_acked(&mut self, offset: u64, len: usize) {
        let prev_retired = self.retired();
        self.ranges.mark_acked(offset, len);

        // Any newly-retired bytes can be dropped from the buffer.
        let new_retirable = self.retired() - prev_retired;
        debug_assert!(new_retirable <= self.buffered() as u64);
        let keep = self.buffered() - usize::try_from(new_retirable).expect("u64 fits in usize");

        // Truncate front
        self.send_buf.rotate_left(self.buffered() - keep);
        self.send_buf.truncate(keep);
    }

    pub fn mark_as_lost(&mut self, offset: u64, len: usize) {
        self.ranges.unmark_range(offset, len);
    }

    /// Forget about anything that was marked as sent.
    pub fn unmark_sent(&mut self) {
        self.ranges.unmark_sent();
    }

    #[must_use]
    pub const fn retired(&self) -> u64 {
        self.ranges.acked_from_zero()
    }

    fn buffered(&self) -> usize {
        self.send_buf.len()
    }

    fn avail(&self) -> usize {
        MAX_SEND_BUFFER_SIZE - self.buffered()
    }

    fn used(&self) -> u64 {
        self.retired() + u64::try_from(self.buffered()).expect("usize fits in u64")
    }
}

/// QUIC sending stream states, based on -transport 3.1.
#[derive(Debug)]
pub enum SendStreamState {
    Ready {
        fc: SenderFlowControl<StreamId>,
        conn_fc: Rc<RefCell<SenderFlowControl<()>>>,
    },
    Send {
        fc: SenderFlowControl<StreamId>,
        conn_fc: Rc<RefCell<SenderFlowControl<()>>>,
        send_buf: TxBuffer,
    },
    // Note: `DataSent` is entered when the stream is closed, not when all data has been
    // sent for the first time.
    DataSent {
        send_buf: TxBuffer,
        fin_sent: bool,
        fin_acked: bool,
    },
    DataRecvd {
        retired: u64,
        written: u64,
    },
    ResetSent {
        err: AppError,
        final_size: u64,
        priority: Option<TransmissionPriority>,
        final_retired: u64,
        final_written: u64,
    },
    ResetRecvd {
        final_retired: u64,
        final_written: u64,
    },
}

impl SendStreamState {
    fn tx_buf_mut(&mut self) -> Option<&mut TxBuffer> {
        match self {
            Self::Send { send_buf, .. } | Self::DataSent { send_buf, .. } => Some(send_buf),
            Self::Ready { .. }
            | Self::DataRecvd { .. }
            | Self::ResetSent { .. }
            | Self::ResetRecvd { .. } => None,
        }
    }

    fn tx_avail(&self) -> usize {
        match self {
            // In Ready, TxBuffer not yet allocated but size is known
            Self::Ready { .. } => MAX_SEND_BUFFER_SIZE,
            Self::Send { send_buf, .. } | Self::DataSent { send_buf, .. } => send_buf.avail(),
            Self::DataRecvd { .. } | Self::ResetSent { .. } | Self::ResetRecvd { .. } => 0,
        }
    }

    fn transition(&mut self, new_state: Self) {
        qtrace!("SendStream state {:?} -> {:?}", self, new_state);
        *self = new_state;
    }
}

// See https://www.w3.org/TR/webtransport/#send-stream-stats.
#[derive(Debug, Clone, Copy)]
pub struct SendStreamStats {
    // The total number of bytes the consumer has successfully written to
    // this stream. This number can only increase.
    pub bytes_written: u64,
    // An indicator of progress on how many of the consumer bytes written to
    // this stream has been sent at least once. This number can only increase,
    // and is always less than or equal to bytes_written.
    pub bytes_sent: u64,
    // An indicator of progress on how many of the consumer bytes written to
    // this stream have been sent and acknowledged as received by the server
    // using QUIC’s ACK mechanism. Only sequential bytes up to,
    // but not including, the first non-acknowledged byte, are counted.
    // This number can only increase and is always less than or equal to
    // bytes_sent.
    pub bytes_acked: u64,
}

impl SendStreamStats {
    #[must_use]
    pub const fn new(bytes_written: u64, bytes_sent: u64, bytes_acked: u64) -> Self {
        Self {
            bytes_written,
            bytes_sent,
            bytes_acked,
        }
    }

    #[must_use]
    pub const fn bytes_written(&self) -> u64 {
        self.bytes_written
    }

    #[must_use]
    pub const fn bytes_sent(&self) -> u64 {
        self.bytes_sent
    }

    #[must_use]
    pub const fn bytes_acked(&self) -> u64 {
        self.bytes_acked
    }
}

/// Implement a QUIC send stream.
#[derive(Debug)]
pub struct SendStream {
    stream_id: StreamId,
    state: SendStreamState,
    conn_events: ConnectionEvents,
    priority: TransmissionPriority,
    retransmission_priority: RetransmissionPriority,
    retransmission_offset: u64,
    sendorder: Option<SendOrder>,
    bytes_sent: u64,
    fair: bool,
    writable_event_low_watermark: NonZeroUsize,
}

impl SendStream {
    pub fn new(
        stream_id: StreamId,
        max_stream_data: u64,
        conn_fc: Rc<RefCell<SenderFlowControl<()>>>,
        conn_events: ConnectionEvents,
    ) -> Self {
        let ss = Self {
            stream_id,
            state: SendStreamState::Ready {
                fc: SenderFlowControl::new(stream_id, max_stream_data),
                conn_fc,
            },
            conn_events,
            priority: TransmissionPriority::default(),
            retransmission_priority: RetransmissionPriority::default(),
            retransmission_offset: 0,
            sendorder: None,
            bytes_sent: 0,
            fair: false,
            writable_event_low_watermark: NonZeroUsize::MIN,
        };
        if ss.avail() > 0 {
            ss.conn_events.send_stream_writable(stream_id);
        }
        ss
    }

    pub fn write_frames(
        &mut self,
        priority: TransmissionPriority,
        builder: &mut PacketBuilder,
        tokens: &mut Vec<RecoveryToken>,
        stats: &mut FrameStats,
    ) {
        qtrace!("write STREAM frames at priority {priority:?}");
        if !self.write_reset_frame(priority, builder, tokens, stats) {
            self.write_blocked_frame(priority, builder, tokens, stats);
            self.write_stream_frame(priority, builder, tokens, stats);
        }
    }

    // return false if the builder is full and the caller should stop iterating
    pub fn write_frames_with_early_return(
        &mut self,
        priority: TransmissionPriority,
        builder: &mut PacketBuilder,
        tokens: &mut Vec<RecoveryToken>,
        stats: &mut FrameStats,
    ) -> bool {
        if !self.write_reset_frame(priority, builder, tokens, stats) {
            self.write_blocked_frame(priority, builder, tokens, stats);
            if builder.is_full() {
                return false;
            }
            self.write_stream_frame(priority, builder, tokens, stats);
            if builder.is_full() {
                return false;
            }
        }
        true
    }

    pub fn set_fairness(&mut self, make_fair: bool) {
        self.fair = make_fair;
    }

    #[must_use]
    pub const fn is_fair(&self) -> bool {
        self.fair
    }

    pub fn set_priority(
        &mut self,
        transmission: TransmissionPriority,
        retransmission: RetransmissionPriority,
    ) {
        self.priority = transmission;
        self.retransmission_priority = retransmission;
    }

    #[must_use]
    pub const fn sendorder(&self) -> Option<SendOrder> {
        self.sendorder
    }

    pub fn set_sendorder(&mut self, sendorder: Option<SendOrder>) {
        self.sendorder = sendorder;
    }

    /// If all data has been buffered or written, how much was sent.
    #[must_use]
    pub fn final_size(&self) -> Option<u64> {
        match &self.state {
            SendStreamState::DataSent { send_buf, .. } => Some(send_buf.used()),
            SendStreamState::ResetSent { final_size, .. } => Some(*final_size),
            _ => None,
        }
    }

    #[must_use]
    pub fn stats(&self) -> SendStreamStats {
        SendStreamStats::new(self.bytes_written(), self.bytes_sent, self.bytes_acked())
    }

    #[must_use]
    #[allow(
        clippy::allow_attributes,
        clippy::missing_panics_doc,
        reason = "OK here."
    )]
    pub fn bytes_written(&self) -> u64 {
        match &self.state {
            SendStreamState::Send { send_buf, .. } | SendStreamState::DataSent { send_buf, .. } => {
                send_buf.retired() + u64::try_from(send_buf.buffered()).expect("usize fits in u64")
            }
            SendStreamState::DataRecvd {
                retired, written, ..
            } => *retired + *written,
            SendStreamState::ResetSent {
                final_retired,
                final_written,
                ..
            }
            | SendStreamState::ResetRecvd {
                final_retired,
                final_written,
                ..
            } => *final_retired + *final_written,
            SendStreamState::Ready { .. } => 0,
        }
    }

    #[must_use]
    pub const fn bytes_acked(&self) -> u64 {
        match &self.state {
            SendStreamState::Send { send_buf, .. } | SendStreamState::DataSent { send_buf, .. } => {
                send_buf.retired()
            }
            SendStreamState::DataRecvd { retired, .. } => *retired,
            SendStreamState::ResetSent { final_retired, .. }
            | SendStreamState::ResetRecvd { final_retired, .. } => *final_retired,
            SendStreamState::Ready { .. } => 0,
        }
    }

    /// Return the next range to be sent, if any.
    /// If this is a retransmission, cut off what is sent at the retransmission
    /// offset.
    fn next_bytes(&mut self, retransmission_only: bool) -> Option<(u64, &[u8])> {
        match self.state {
            SendStreamState::Send {
                ref mut send_buf, ..
            } => {
                let (offset, slice) = send_buf.next_bytes()?;
                if retransmission_only {
                    qtrace!(
                        "next_bytes apply retransmission limit at {}",
                        self.retransmission_offset
                    );
                    (self.retransmission_offset > offset).then(|| {
                        let Ok(delta) = usize::try_from(self.retransmission_offset - offset) else {
                            return None;
                        };
                        let len = min(delta, slice.len());
                        Some((offset, &slice[..len]))
                    })?
                } else {
                    Some((offset, slice))
                }
            }
            SendStreamState::DataSent {
                ref mut send_buf,
                fin_sent,
                ..
            } => {
                let used = send_buf.used(); // immutable first
                let bytes = send_buf.next_bytes();
                if bytes.is_some() {
                    bytes
                } else if fin_sent {
                    None
                } else {
                    // Send empty stream frame with fin set
                    Some((used, &[]))
                }
            }
            SendStreamState::Ready { .. }
            | SendStreamState::DataRecvd { .. }
            | SendStreamState::ResetSent { .. }
            | SendStreamState::ResetRecvd { .. } => None,
        }
    }

    /// Calculate how many bytes (length) can fit into available space and whether
    /// the remainder of the space can be filled (or if a length field is needed).
    fn length_and_fill(data_len: usize, space: usize) -> (usize, bool) {
        if data_len >= space {
            // More data than space allows, or an exact fit => fast path.
            qtrace!("SendStream::length_and_fill fill {space}");
            return (space, true);
        }

        // Estimate size of the length field based on the available space,
        // less 1, which is the worst case.
        let length = min(space.saturating_sub(1), data_len);
        let length_len = Encoder::varint_len(u64::try_from(length).expect("usize fits in u64"));
        debug_assert!(length_len <= space); // We don't depend on this being true, but it is true.

        // From here we can always fit `data_len`, but we might as well fill
        // if there is no space for the length field plus another frame.
        let fill = data_len + length_len + PacketBuilder::MINIMUM_FRAME_SIZE > space;
        qtrace!("SendStream::length_and_fill {data_len} fill {fill}");
        (data_len, fill)
    }

    /// Maybe write a `STREAM` frame.
    #[allow(
        clippy::allow_attributes,
        clippy::missing_panics_doc,
        reason = "OK here."
    )]
    pub fn write_stream_frame(
        &mut self,
        priority: TransmissionPriority,
        builder: &mut PacketBuilder,
        tokens: &mut Vec<RecoveryToken>,
        stats: &mut FrameStats,
    ) {
        let retransmission = if priority == self.priority {
            false
        } else if priority == self.priority + self.retransmission_priority {
            true
        } else {
            return;
        };

        let id = self.stream_id;
        let final_size = self.final_size();
        if let Some((offset, data)) = self.next_bytes(retransmission) {
            let overhead = 1 // Frame type
                + Encoder::varint_len(id.as_u64())
                + if offset > 0 {
                    Encoder::varint_len(offset)
                } else {
                    0
                };
            if overhead > builder.remaining() {
                qtrace!("[{self}] write_frame no space for header");
                return;
            }

            let (length, fill) = Self::length_and_fill(data.len(), builder.remaining() - overhead);
            let fin = final_size
                .is_some_and(|fs| fs == offset + u64::try_from(length).expect("usize fits in u64"));
            if length == 0 && !fin {
                qtrace!("[{self}] write_frame no data, no fin");
                return;
            }

            // Write the stream out.
            builder.encode_varint(Frame::stream_type(fin, offset > 0, fill));
            builder.encode_varint(id.as_u64());
            if offset > 0 {
                builder.encode_varint(offset);
            }
            if fill {
                builder.encode(&data[..length]);
                builder.mark_full();
            } else {
                builder.encode_vvec(&data[..length]);
            }
            debug_assert!(builder.len() <= builder.limit());

            self.mark_as_sent(offset, length, fin);
            tokens.push(RecoveryToken::Stream(StreamRecoveryToken::Stream(
                SendStreamRecoveryToken {
                    id,
                    offset,
                    length,
                    fin,
                },
            )));
            stats.stream += 1;
        }
    }

    pub fn reset_acked(&mut self) {
        match self.state {
            SendStreamState::Ready { .. }
            | SendStreamState::Send { .. }
            | SendStreamState::DataSent { .. }
            | SendStreamState::DataRecvd { .. } => {
                qtrace!("[{self}] Reset acked while in {:?} state?", self.state);
            }
            SendStreamState::ResetSent {
                final_retired,
                final_written,
                ..
            } => self.state.transition(SendStreamState::ResetRecvd {
                final_retired,
                final_written,
            }),
            SendStreamState::ResetRecvd { .. } => qtrace!("[{self}] already in ResetRecvd state"),
        }
    }

    pub fn reset_lost(&mut self) {
        match self.state {
            SendStreamState::ResetSent {
                ref mut priority, ..
            } => {
                *priority = Some(self.priority + self.retransmission_priority);
            }
            SendStreamState::ResetRecvd { .. } => (),
            _ => unreachable!(),
        }
    }

    /// Maybe write a `RESET_STREAM` frame.
    pub fn write_reset_frame(
        &mut self,
        p: TransmissionPriority,
        builder: &mut PacketBuilder,
        tokens: &mut Vec<RecoveryToken>,
        stats: &mut FrameStats,
    ) -> bool {
        if let SendStreamState::ResetSent {
            final_size,
            err,
            ref mut priority,
            ..
        } = self.state
        {
            if *priority != Some(p) {
                return false;
            }
            if builder.write_varint_frame(&[
                FrameType::ResetStream.into(),
                self.stream_id.as_u64(),
                err,
                final_size,
            ]) {
                tokens.push(RecoveryToken::Stream(StreamRecoveryToken::ResetStream {
                    stream_id: self.stream_id,
                }));
                stats.reset_stream += 1;
                *priority = None;
                true
            } else {
                false
            }
        } else {
            false
        }
    }

    pub fn blocked_lost(&mut self, limit: u64) {
        if let SendStreamState::Ready { fc, .. } | SendStreamState::Send { fc, .. } =
            &mut self.state
        {
            fc.frame_lost(limit);
        } else {
            qtrace!("[{self}] Ignoring lost STREAM_DATA_BLOCKED({limit})");
        }
    }

    /// Maybe write a `STREAM_DATA_BLOCKED` frame.
    pub fn write_blocked_frame(
        &mut self,
        priority: TransmissionPriority,
        builder: &mut PacketBuilder,
        tokens: &mut Vec<RecoveryToken>,
        stats: &mut FrameStats,
    ) {
        // Send STREAM_DATA_BLOCKED at normal priority always.
        if priority == self.priority {
            if let SendStreamState::Ready { fc, .. } | SendStreamState::Send { fc, .. } =
                &mut self.state
            {
                fc.write_frames(builder, tokens, stats);
            }
        }
    }

    #[allow(
        clippy::allow_attributes,
        clippy::missing_panics_doc,
        reason = "OK here."
    )]
    pub fn mark_as_sent(&mut self, offset: u64, len: usize, fin: bool) {
        self.bytes_sent = max(
            self.bytes_sent,
            offset + u64::try_from(len).expect("usize fits in u64"),
        );

        if let Some(buf) = self.state.tx_buf_mut() {
            buf.mark_as_sent(offset, len);
            self.send_blocked_if_space_needed(0);
        }

        if fin {
            if let SendStreamState::DataSent { fin_sent, .. } = &mut self.state {
                *fin_sent = true;
            }
        }
    }

    #[allow(
        clippy::allow_attributes,
        clippy::missing_panics_doc,
        reason = "OK here."
    )]
    pub fn mark_as_acked(&mut self, offset: u64, len: usize, fin: bool) {
        match self.state {
            SendStreamState::Send {
                ref mut send_buf, ..
            } => {
                let previous_limit = send_buf.avail();
                send_buf.mark_as_acked(offset, len);
                let current_limit = send_buf.avail();
                self.maybe_emit_writable_event(previous_limit, current_limit);
            }
            SendStreamState::DataSent {
                ref mut send_buf,
                ref mut fin_acked,
                ..
            } => {
                send_buf.mark_as_acked(offset, len);
                if fin {
                    *fin_acked = true;
                }
                if *fin_acked && send_buf.buffered() == 0 {
                    self.conn_events.send_stream_complete(self.stream_id);
                    let retired = send_buf.retired();
                    let buffered = u64::try_from(send_buf.buffered()).expect("usize fits in u64");
                    self.state.transition(SendStreamState::DataRecvd {
                        retired,
                        written: buffered,
                    });
                }
            }
            _ => qtrace!("[{self}] mark_as_acked called from state {:?}", self.state),
        }
    }

    #[allow(
        clippy::allow_attributes,
        clippy::missing_panics_doc,
        reason = "OK here."
    )]
    pub fn mark_as_lost(&mut self, offset: u64, len: usize, fin: bool) {
        self.retransmission_offset = max(
            self.retransmission_offset,
            offset + u64::try_from(len).expect("usize fits in u64"),
        );
        qtrace!(
            "[{self}] mark_as_lost retransmission offset={}",
            self.retransmission_offset
        );
        if let Some(buf) = self.state.tx_buf_mut() {
            buf.mark_as_lost(offset, len);
        }

        if fin {
            if let SendStreamState::DataSent {
                fin_sent,
                fin_acked,
                ..
            } = &mut self.state
            {
                *fin_sent = *fin_acked;
            }
        }
    }

    /// Bytes sendable on stream. Constrained by stream credit available,
    /// connection credit available, and space in the tx buffer.
    #[must_use]
    pub fn avail(&self) -> usize {
        if let SendStreamState::Ready { fc, conn_fc } | SendStreamState::Send { fc, conn_fc, .. } =
            &self.state
        {
            min(
                min(fc.available(), conn_fc.borrow().available()),
                self.state.tx_avail(),
            )
        } else {
            0
        }
    }

    /// Set low watermark for [`crate::ConnectionEvent::SendStreamWritable`]
    /// event.
    ///
    /// See [`crate::Connection::stream_set_writable_event_low_watermark`].
    pub fn set_writable_event_low_watermark(&mut self, watermark: NonZeroUsize) {
        self.writable_event_low_watermark = watermark;
    }

    pub fn set_max_stream_data(&mut self, limit: u64) {
        qdebug!("setting max_stream_data to {limit}");
        if let SendStreamState::Ready { fc, .. } | SendStreamState::Send { fc, .. } =
            &mut self.state
        {
            let previous_limit = fc.available();
            if let Some(current_limit) = fc.update(limit) {
                self.maybe_emit_writable_event(previous_limit, current_limit);
            }
        }
    }

    #[must_use]
    pub const fn is_terminal(&self) -> bool {
        matches!(
            self.state,
            SendStreamState::DataRecvd { .. } | SendStreamState::ResetRecvd { .. }
        )
    }

    /// # Errors
    /// When `buf` is empty or when the stream is already closed.
    pub fn send(&mut self, buf: &[u8]) -> Res<usize> {
        self.send_internal(buf, false)
    }

    /// # Errors
    /// When `buf` is empty or when the stream is already closed.
    pub fn send_atomic(&mut self, buf: &[u8]) -> Res<usize> {
        self.send_internal(buf, true)
    }

    fn send_blocked_if_space_needed(&mut self, needed_space: usize) {
        if let SendStreamState::Ready { fc, conn_fc } | SendStreamState::Send { fc, conn_fc, .. } =
            &mut self.state
        {
            if fc.available() <= needed_space {
                fc.blocked();
            }

            if conn_fc.borrow().available() <= needed_space {
                conn_fc.borrow_mut().blocked();
            }
        }
    }

    fn send_internal(&mut self, buf: &[u8], atomic: bool) -> Res<usize> {
        if buf.is_empty() {
            qerror!("[{self}] zero-length send on stream");
            return Err(Error::InvalidInput);
        }

        if let SendStreamState::Ready { fc, conn_fc } = &mut self.state {
            let owned_fc = mem::replace(fc, SenderFlowControl::new(self.stream_id, 0));
            let owned_conn_fc = Rc::clone(conn_fc);
            self.state.transition(SendStreamState::Send {
                fc: owned_fc,
                conn_fc: owned_conn_fc,
                send_buf: TxBuffer::new(),
            });
        }

        if !matches!(self.state, SendStreamState::Send { .. }) {
            return Err(Error::FinalSizeError);
        }

        let buf = if self.avail() == 0 {
            return Ok(0);
        } else if self.avail() < buf.len() {
            if atomic {
                self.send_blocked_if_space_needed(buf.len());
                return Ok(0);
            }

            &buf[..self.avail()]
        } else {
            buf
        };

        match &mut self.state {
            SendStreamState::Ready { .. } => unreachable!(),
            SendStreamState::Send {
                fc,
                conn_fc,
                send_buf,
            } => {
                let sent = send_buf.send(buf);
                fc.consume(sent);
                conn_fc.borrow_mut().consume(sent);
                Ok(sent)
            }
            _ => Err(Error::FinalSizeError),
        }
    }

    pub fn close(&mut self) {
        match &mut self.state {
            SendStreamState::Ready { .. } => {
                self.state.transition(SendStreamState::DataSent {
                    send_buf: TxBuffer::new(),
                    fin_sent: false,
                    fin_acked: false,
                });
            }
            SendStreamState::Send { send_buf, .. } => {
                let owned_buf = mem::replace(send_buf, TxBuffer::new());
                self.state.transition(SendStreamState::DataSent {
                    send_buf: owned_buf,
                    fin_sent: false,
                    fin_acked: false,
                });
            }
            SendStreamState::DataSent { .. } => qtrace!("[{self}] already in DataSent state"),
            SendStreamState::DataRecvd { .. } => qtrace!("[{self}] already in DataRecvd state"),
            SendStreamState::ResetSent { .. } => qtrace!("[{self}] already in ResetSent state"),
            SendStreamState::ResetRecvd { .. } => qtrace!("[{self}] already in ResetRecvd state"),
        }
    }

    #[allow(
        clippy::allow_attributes,
        clippy::missing_panics_doc,
        reason = "OK here."
    )]
    pub fn reset(&mut self, err: AppError) {
        match &self.state {
            SendStreamState::Ready { fc, .. } => {
                let final_size = fc.used();
                self.state.transition(SendStreamState::ResetSent {
                    err,
                    final_size,
                    priority: Some(self.priority),
                    final_retired: 0,
                    final_written: 0,
                });
            }
            SendStreamState::Send { fc, send_buf, .. } => {
                let final_size = fc.used();
                let final_retired = send_buf.retired();
                let buffered = u64::try_from(send_buf.buffered()).expect("usize fits in u64");
                self.state.transition(SendStreamState::ResetSent {
                    err,
                    final_size,
                    priority: Some(self.priority),
                    final_retired,
                    final_written: buffered,
                });
            }
            SendStreamState::DataSent { send_buf, .. } => {
                let final_size = send_buf.used();
                let final_retired = send_buf.retired();
                let buffered = u64::try_from(send_buf.buffered()).expect("usize fits in u64");
                self.state.transition(SendStreamState::ResetSent {
                    err,
                    final_size,
                    priority: Some(self.priority),
                    final_retired,
                    final_written: buffered,
                });
            }
            SendStreamState::DataRecvd { .. } => qtrace!("[{self}] already in DataRecvd state"),
            SendStreamState::ResetSent { .. } => qtrace!("[{self}] already in ResetSent state"),
            SendStreamState::ResetRecvd { .. } => qtrace!("[{self}] already in ResetRecvd state"),
        }
    }

    #[cfg(test)]
    pub(crate) fn state(&mut self) -> &mut SendStreamState {
        &mut self.state
    }

    pub(crate) fn maybe_emit_writable_event(&self, previous_limit: usize, current_limit: usize) {
        let low_watermark = self.writable_event_low_watermark.get();

        // Skip if:
        // - stream was not constrained by limit before,
        // - or stream is still constrained by limit,
        // - or stream is constrained by different limit.
        if low_watermark < previous_limit
            || current_limit < low_watermark
            || self.avail() < low_watermark
        {
            return;
        }

        self.conn_events.send_stream_writable(self.stream_id);
    }
}

impl Display for SendStream {
    fn fmt(&self, f: &mut Formatter) -> fmt::Result {
        write!(f, "SendStream {}", self.stream_id)
    }
}

#[derive(Debug, Default)]
pub struct OrderGroup {
    // This vector is sorted by StreamId
    vec: Vec<StreamId>,

    // Since we need to remember where we were, we'll store the iterator next
    // position in the object.  This means there can only be a single iterator active
    // at a time!
    next: usize,
    // This is used when an iterator is created to set the start/stop point for the
    // iteration.  The iterator must iterate from this entry to the end, and then
    // wrap and iterate from 0 until before the initial value of next.
    // This value may need to be updated after insertion and removal; in theory we should
    // track the target entry across modifications, but in practice it should be good
    // enough to simply leave it alone unless it points past the end of the
    // Vec, and re-initialize to 0 in that case.
}

pub struct OrderGroupIter<'a> {
    group: &'a mut OrderGroup,
    // We store the next position in the OrderGroup.
    // Otherwise we'd need an explicit "done iterating" call to be made, or implement Drop to
    // copy the value back.
    // This is where next was when we iterated for the first time; when we get back to that we
    // stop.
    started_at: Option<usize>,
}

impl OrderGroup {
    pub fn iter(&mut self) -> OrderGroupIter {
        // Ids may have been deleted since we last iterated
        if self.next >= self.vec.len() {
            self.next = 0;
        }
        OrderGroupIter {
            started_at: None,
            group: self,
        }
    }

    #[must_use]
    pub const fn stream_ids(&self) -> &Vec<StreamId> {
        &self.vec
    }

    pub fn clear(&mut self) {
        self.vec.clear();
    }

    pub fn push(&mut self, stream_id: StreamId) {
        self.vec.push(stream_id);
    }

    #[cfg(test)]
    pub fn truncate(&mut self, position: usize) {
        self.vec.truncate(position);
    }

    fn update_next(&mut self) -> usize {
        let next = self.next;
        self.next = (self.next + 1) % self.vec.len();
        next
    }

    /// # Panics
    /// If the stream ID is already present.
    pub fn insert(&mut self, stream_id: StreamId) {
        let Err(pos) = self.vec.binary_search(&stream_id) else {
            // element already in vector @ `pos`
            panic!("Duplicate stream_id {stream_id}");
        };
        self.vec.insert(pos, stream_id);
    }

    /// # Panics
    /// If the stream ID is not present.
    pub fn remove(&mut self, stream_id: StreamId) {
        let Ok(pos) = self.vec.binary_search(&stream_id) else {
            // element already in vector @ `pos`
            panic!("Missing stream_id {stream_id}");
        };
        self.vec.remove(pos);
    }
}

impl Iterator for OrderGroupIter<'_> {
    type Item = StreamId;
    fn next(&mut self) -> Option<Self::Item> {
        // Stop when we would return the started_at element on the next
        // call.  Note that this must take into account wrapping.
        if self.started_at == Some(self.group.next) || self.group.vec.is_empty() {
            return None;
        }
        self.started_at = self.started_at.or(Some(self.group.next));
        let orig = self.group.update_next();
        Some(self.group.vec[orig])
    }
}

#[derive(Debug, Default)]
pub struct SendStreams {
    map: IndexMap<StreamId, SendStream>,

    // What we really want is a Priority Queue that we can do arbitrary
    // removes from (so we can reprioritize). BinaryHeap doesn't work,
    // because there's no remove().  BTreeMap doesn't work, since you can't
    // duplicate keys.  PriorityQueue does have what we need, except for an
    // ordered iterator that doesn't consume the queue.  So we roll our own.

    // Added complication: We want to have Fairness for streams of the same
    // 'group' (for WebTransport), but for H3 (and other non-WT streams) we
    // tend to get better pageload performance by prioritizing by creation order.
    //
    // Two options are to walk the 'map' first, ignoring WebTransport
    // streams, then process the unordered and ordered WebTransport
    // streams.  The second is to have a sorted Vec for unfair streams (and
    // use a normal iterator for that), and then chain the iterators for
    // the unordered and ordered WebTranport streams.  The first works very
    // well for H3, and for WebTransport nodes are visited twice on every
    // processing loop.  The second adds insertion and removal costs, but
    // avoids a CPU penalty for WebTransport streams.  For now we'll do #1.
    //
    // So we use a sorted Vec<> for the regular streams (that's usually all of
    // them), and then a BTreeMap of an entry for each SendOrder value, and
    // for each of those entries a Vec of the stream_ids at that
    // sendorder.  In most cases (such as stream-per-frame), there will be
    // a single stream at a given sendorder.

    // These both store stream_ids, which need to be looked up in 'map'.
    // This avoids the complexity of trying to hold references to the
    // Streams which are owned by the IndexMap.
    sendordered: BTreeMap<SendOrder, OrderGroup>,
    regular: OrderGroup, // streams with no SendOrder set, sorted in stream_id order
}

impl SendStreams {
    #[allow(
        clippy::allow_attributes,
        clippy::missing_errors_doc,
        reason = "OK here."
    )]
    pub fn get(&self, id: StreamId) -> Res<&SendStream> {
        self.map.get(&id).ok_or(Error::InvalidStreamId)
    }

    #[allow(
        clippy::allow_attributes,
        clippy::missing_errors_doc,
        reason = "OK here."
    )]
    pub fn get_mut(&mut self, id: StreamId) -> Res<&mut SendStream> {
        self.map.get_mut(&id).ok_or(Error::InvalidStreamId)
    }

    #[must_use]
    pub fn exists(&self, id: StreamId) -> bool {
        self.map.contains_key(&id)
    }

    pub fn insert(&mut self, id: StreamId, stream: SendStream) {
        self.map.insert(id, stream);
    }

    fn group_mut(&mut self, sendorder: Option<SendOrder>) -> &mut OrderGroup {
        if let Some(order) = sendorder {
            self.sendordered.entry(order).or_default()
        } else {
            &mut self.regular
        }
    }

    #[allow(
        clippy::allow_attributes,
        clippy::missing_errors_doc,
        reason = "OK here."
    )]
    pub fn set_sendorder(&mut self, stream_id: StreamId, sendorder: Option<SendOrder>) -> Res<()> {
        self.set_fairness(stream_id, true)?;
        if let Some(stream) = self.map.get_mut(&stream_id) {
            // don't grab stream here; causes borrow errors
            let old_sendorder = stream.sendorder();
            if old_sendorder != sendorder {
                // we have to remove it from the list it was in, and reinsert it with the new
                // sendorder key
                let mut group = self.group_mut(old_sendorder);
                group.remove(stream_id);
                self.get_mut(stream_id)?.set_sendorder(sendorder);
                group = self.group_mut(sendorder);
                group.insert(stream_id);
                qtrace!(
                    "ordering of stream_ids: {:?}",
                    self.sendordered.values().collect::<Vec::<_>>()
                );
            }
            Ok(())
        } else {
            Err(Error::InvalidStreamId)
        }
    }

    #[allow(
        clippy::allow_attributes,
        clippy::missing_errors_doc,
        reason = "OK here."
    )]
    pub fn set_fairness(&mut self, stream_id: StreamId, make_fair: bool) -> Res<()> {
        let stream: &mut SendStream = self.map.get_mut(&stream_id).ok_or(Error::InvalidStreamId)?;
        let was_fair = stream.fair;
        stream.set_fairness(make_fair);
        if !was_fair && make_fair {
            // Move to the regular OrderGroup.

            // We know sendorder can't have been set, since
            // set_sendorder() will call this routine if it's not
            // already set as fair.

            // This normally is only called when a new stream is created.  If
            // so, because of how we allocate StreamIds, it should always have
            // the largest value.  This means we can just append it to the
            // regular vector.  However, if we were ever to change this
            // invariant, things would break subtly.

            // To be safe we can try to insert at the end and if not
            // fall back to binary-search insertion
            if matches!(self.regular.stream_ids().last(), Some(last) if stream_id > *last) {
                self.regular.push(stream_id);
            } else {
                self.regular.insert(stream_id);
            }
        } else if was_fair && !make_fair {
            // remove from the OrderGroup
            let group = if let Some(sendorder) = stream.sendorder {
                self.sendordered
                    .get_mut(&sendorder)
                    .ok_or(Error::InternalError)?
            } else {
                &mut self.regular
            };
            group.remove(stream_id);
        }
        Ok(())
    }

    pub fn acked(&mut self, token: &SendStreamRecoveryToken) {
        if let Some(ss) = self.map.get_mut(&token.id) {
            ss.mark_as_acked(token.offset, token.length, token.fin);
        }
    }

    pub fn reset_acked(&mut self, id: StreamId) {
        if let Some(ss) = self.map.get_mut(&id) {
            ss.reset_acked();
        }
    }

    pub fn lost(&mut self, token: &SendStreamRecoveryToken) {
        if let Some(ss) = self.map.get_mut(&token.id) {
            ss.mark_as_lost(token.offset, token.length, token.fin);
        }
    }

    pub fn reset_lost(&mut self, stream_id: StreamId) {
        if let Some(ss) = self.map.get_mut(&stream_id) {
            ss.reset_lost();
        }
    }

    pub fn blocked_lost(&mut self, stream_id: StreamId, limit: u64) {
        if let Some(ss) = self.map.get_mut(&stream_id) {
            ss.blocked_lost(limit);
        }
    }

    pub fn clear(&mut self) {
        self.map.clear();
        self.sendordered.clear();
        self.regular.clear();
    }

    pub fn remove_terminal(&mut self) {
        self.map.retain(|stream_id, stream| {
            if stream.is_terminal() {
                if stream.is_fair() {
                    match stream.sendorder() {
                        None => self.regular.remove(*stream_id),
                        Some(sendorder) => {
                            if let Some(group) = self.sendordered.get_mut(&sendorder) {
                                group.remove(*stream_id);
                            }
                        }
                    }
                }
                // if unfair, we're done
                return false;
            }
            true
        });
    }

    pub(crate) fn write_frames(
        &mut self,
        priority: TransmissionPriority,
        builder: &mut PacketBuilder,
        tokens: &mut Vec<RecoveryToken>,
        stats: &mut FrameStats,
    ) {
        qtrace!("write STREAM frames at priority {priority:?}");
        // WebTransport data (which is Normal) may have a SendOrder
        // priority attached.  The spec states (6.3 write-chunk 6.1):

        // First, we send any streams without Fairness defined, with
        // ordering defined by StreamId.  (Http3 streams used for
        // e.g. pageload benefit from being processed in order of creation
        // so the far side can start acting on a datum/request sooner. All
        // WebTransport streams MUST have fairness set.)  Then we send
        // streams with fairness set (including all WebTransport streams)
        // as follows:

        // If stream.[[SendOrder]] is null then this sending MUST NOT
        // starve except for flow control reasons or error.  If
        // stream.[[SendOrder]] is not null then this sending MUST starve
        // until all bytes queued for sending on WebTransportSendStreams
        // with a non-null and higher [[SendOrder]], that are neither
        // errored nor blocked by flow control, have been sent.

        // So data without SendOrder goes first.   Then the highest priority
        // SendOrdered streams.
        //
        // Fairness is implemented by a round-robining or "statefully
        // iterating" within a single sendorder/unordered vector.  We do
        // this by recording where we stopped in the previous pass, and
        // starting there the next pass.  If we store an index into the
        // vec, this means we can't use a chained iterator, since we want
        // to retain our place-in-the-vector.  If we rotate the vector,
        // that would let us use the chained iterator, but would require
        // more expensive searches for insertion and removal (since the
        // sorted order would be lost).

        // Iterate the map, but only those without fairness, then iterate
        // OrderGroups, then iterate each group
        qtrace!("processing streams...  unfair:");
        for stream in self.map.values_mut() {
            if !stream.is_fair() {
                qtrace!("   {stream}");
                if !stream.write_frames_with_early_return(priority, builder, tokens, stats) {
                    break;
                }
            }
        }
        qtrace!("fair streams:");
        let stream_ids = self.regular.iter().chain(
            self.sendordered
                .values_mut()
                .rev()
                .flat_map(|group| group.iter()),
        );
        for stream_id in stream_ids {
            if let Some(stream) = self.map.get_mut(&stream_id) {
                if let Some(order) = stream.sendorder() {
                    qtrace!("   {stream_id} ({order})");
                } else {
                    qtrace!("   None");
                }
                if !stream.write_frames_with_early_return(priority, builder, tokens, stats) {
                    break;
                }
            }
        }
    }

    #[allow(
        clippy::allow_attributes,
        clippy::missing_panics_doc,
        reason = "OK here."
    )]
    pub fn update_initial_limit(&mut self, remote: &TransportParameters) {
        for (id, ss) in &mut self.map {
            let limit = if id.is_bidi() {
                assert!(!id.is_remote_initiated(Role::Client));
                remote.get_integer(InitialMaxStreamDataBidiRemote)
            } else {
                remote.get_integer(InitialMaxStreamDataUni)
            };
            ss.set_max_stream_data(limit);
        }
    }
}

#[allow(
    clippy::allow_attributes,
    clippy::into_iter_without_iter,
    reason = "OK here."
)]
impl<'a> IntoIterator for &'a mut SendStreams {
    type Item = (&'a StreamId, &'a mut SendStream);
    type IntoIter = indexmap::map::IterMut<'a, StreamId, SendStream>;

    fn into_iter(self) -> indexmap::map::IterMut<'a, StreamId, SendStream> {
        self.map.iter_mut()
    }
}

#[derive(Debug, Clone)]
pub struct SendStreamRecoveryToken {
    id: StreamId,
    offset: u64,
    length: usize,
    fin: bool,
}

#[cfg(test)]
mod tests {
    use std::{cell::RefCell, collections::VecDeque, num::NonZeroUsize, rc::Rc};

    use neqo_common::{event::Provider as _, hex_with_len, qtrace, Encoder, MAX_VARINT};

    use super::SendStreamRecoveryToken;
    use crate::{
        connection::{RetransmissionPriority, TransmissionPriority},
        events::ConnectionEvent,
        fc::SenderFlowControl,
        packet::PacketBuilder,
        recovery::{RecoveryToken, StreamRecoveryToken},
        send_stream::{
            RangeState, RangeTracker, SendStream, SendStreamState, SendStreams, TxBuffer,
            MAX_SEND_BUFFER_SIZE,
        },
        stats::FrameStats,
        ConnectionEvents, StreamId, INITIAL_RECV_WINDOW_SIZE,
    };

    fn connection_fc(limit: u64) -> Rc<RefCell<SenderFlowControl<()>>> {
        Rc::new(RefCell::new(SenderFlowControl::new((), limit)))
    }

    #[test]
    fn mark_acked_from_zero() {
        let mut rt = RangeTracker::default();

        // ranges can go from nothing->Sent if queued for retrans and then
        // acks arrive
        rt.mark_acked(5, 5);
        assert_eq!(rt.highest_offset(), 10);
        assert_eq!(rt.acked_from_zero(), 0);
        rt.mark_acked(10, 4);
        assert_eq!(rt.highest_offset(), 14);
        assert_eq!(rt.acked_from_zero(), 0);

        rt.mark_sent(0, 5);
        assert_eq!(rt.highest_offset(), 14);
        assert_eq!(rt.acked_from_zero(), 0);
        rt.mark_acked(0, 5);
        assert_eq!(rt.highest_offset(), 14);
        assert_eq!(rt.acked_from_zero(), 14);

        rt.mark_acked(12, 20);
        assert_eq!(rt.highest_offset(), 32);
        assert_eq!(rt.acked_from_zero(), 32);

        // ack the lot
        rt.mark_acked(0, 400);
        assert_eq!(rt.highest_offset(), 400);
        assert_eq!(rt.acked_from_zero(), 400);

        // acked trumps sent
        rt.mark_sent(0, 200);
        assert_eq!(rt.highest_offset(), 400);
        assert_eq!(rt.acked_from_zero(), 400);
    }

    /// Check that `marked_acked` correctly handles all paths.
    /// ```ignore
    ///   SSS  SSSAAASSS
    /// +    AAAAAAAAA
    /// = SSSAAAAAAAAASS
    /// ```
    #[test]
    fn mark_acked_1() {
        let mut rt = RangeTracker::default();
        rt.mark_sent(0, 3);
        rt.mark_sent(6, 3);
        rt.mark_acked(9, 3);
        rt.mark_sent(12, 3);

        rt.mark_acked(3, 10);

        let mut canon = RangeTracker::default();
        canon.used.insert(0, (3, RangeState::Sent));
        canon.used.insert(3, (10, RangeState::Acked));
        canon.used.insert(13, (2, RangeState::Sent));
        assert_eq!(rt, canon);
    }

    /// Check that `marked_acked` correctly handles all paths.
    /// ```ignore
    ///   SSS  SSS   AAA
    /// +   AAAAAAAAA
    /// = SSAAAAAAAAAAAA
    /// ```
    #[test]
    fn mark_acked_2() {
        let mut rt = RangeTracker::default();
        rt.mark_sent(0, 3);
        rt.mark_sent(6, 3);
        rt.mark_acked(12, 3);

        rt.mark_acked(2, 10);

        let mut canon = RangeTracker::default();
        canon.used.insert(0, (2, RangeState::Sent));
        canon.used.insert(2, (13, RangeState::Acked));
        assert_eq!(rt, canon);
    }

    /// Check that `marked_acked` correctly handles all paths.
    /// ```ignore
    ///    AASSS  AAAA
    /// + AAAAAAAAA
    /// = AAAAAAAAAAAA
    /// ```
    #[test]
    fn mark_acked_3() {
        let mut rt = RangeTracker::default();
        rt.mark_acked(1, 2);
        rt.mark_sent(3, 3);
        rt.mark_acked(8, 4);

        rt.mark_acked(0, 9);

        let canon = RangeTracker {
            acked: 12,
            ..RangeTracker::default()
        };
        assert_eq!(rt, canon);
    }

    /// Check that `marked_acked` correctly handles all paths.
    /// ```ignore
    ///      SSS
    /// + AAAA
    /// = AAAASS
    /// ```
    #[test]
    fn mark_acked_4() {
        let mut rt = RangeTracker::default();
        rt.mark_sent(3, 3);

        rt.mark_acked(0, 4);

        let mut canon = RangeTracker {
            acked: 4,
            ..Default::default()
        };
        canon.used.insert(4, (2, RangeState::Sent));
        assert_eq!(rt, canon);
    }

    /// Check that `marked_acked` correctly handles all paths.
    /// ```ignore
    ///   AAAAAASSS
    /// +    AAA
    /// = AAAAAASSS
    /// ```
    #[test]
    fn mark_acked_5() {
        let mut rt = RangeTracker::default();
        rt.mark_acked(0, 6);
        rt.mark_sent(6, 3);

        rt.mark_acked(3, 3);

        let mut canon = RangeTracker {
            acked: 6,
            ..RangeTracker::default()
        };
        canon.used.insert(6, (3, RangeState::Sent));
        assert_eq!(rt, canon);
    }

    /// Check that `marked_acked` correctly handles all paths.
    /// ```ignore
    ///      AAA  AAA  AAA
    /// +       AAAAAAA
    /// =    AAAAAAAAAAAAA
    /// ```
    #[test]
    fn mark_acked_6() {
        let mut rt = RangeTracker::default();
        rt.mark_acked(3, 3);
        rt.mark_acked(8, 3);
        rt.mark_acked(13, 3);

        rt.mark_acked(6, 7);

        let mut canon = RangeTracker::default();
        canon.used.insert(3, (13, RangeState::Acked));
        assert_eq!(rt, canon);
    }

    /// Check that `marked_acked` correctly handles all paths.
    /// ```ignore
    ///      AAA  AAA
    /// +       AAA
    /// =    AAAAAAAA
    /// ```
    #[test]
    fn mark_acked_7() {
        let mut rt = RangeTracker::default();
        rt.mark_acked(3, 3);
        rt.mark_acked(8, 3);

        rt.mark_acked(6, 3);

        let mut canon = RangeTracker::default();
        canon.used.insert(3, (8, RangeState::Acked));
        assert_eq!(rt, canon);
    }

    /// Check that `marked_acked` correctly handles all paths.
    /// ```ignore
    ///   SSSSSSSS
    /// +   AAAA
    /// = SSAAAASS
    /// ```
    #[test]
    fn mark_acked_8() {
        let mut rt = RangeTracker::default();
        rt.mark_sent(0, 8);

        rt.mark_acked(2, 4);

        let mut canon = RangeTracker::default();
        canon.used.insert(0, (2, RangeState::Sent));
        canon.used.insert(2, (4, RangeState::Acked));
        canon.used.insert(6, (2, RangeState::Sent));
        assert_eq!(rt, canon);
    }

    /// Check that `marked_acked` correctly handles all paths.
    /// ```ignore
    ///        SSS
    /// + AAA
    /// = AAA  SSS
    /// ```
    #[test]
    fn mark_acked_9() {
        let mut rt = RangeTracker::default();
        rt.mark_sent(5, 3);

        rt.mark_acked(0, 3);

        let mut canon = RangeTracker {
            acked: 3,
            ..Default::default()
        };
        canon.used.insert(5, (3, RangeState::Sent));
        assert_eq!(rt, canon);
    }

    /// Check that `marked_sent` correctly handles all paths.
    /// ```ignore
    ///   AAA   AAA   SSS
    /// + SSSSSSSSSSSS
    /// = AAASSSAAASSSSSS
    /// ```
    #[test]
    fn mark_sent_1() {
        let mut rt = RangeTracker::default();
        rt.mark_acked(0, 3);
        rt.mark_acked(6, 3);
        rt.mark_sent(12, 3);

        rt.mark_sent(0, 12);

        let mut canon = RangeTracker {
            acked: 3,
            ..RangeTracker::default()
        };
        canon.used.insert(3, (3, RangeState::Sent));
        canon.used.insert(6, (3, RangeState::Acked));
        canon.used.insert(9, (6, RangeState::Sent));
        assert_eq!(rt, canon);
    }

    /// Check that `marked_sent` correctly handles all paths.
    /// ```ignore
    ///   AAASS AAA S SSSS
    /// + SSSSSSSSSSSSS
    /// = AAASSSAAASSSSSSS
    /// ```
    #[test]
    fn mark_sent_2() {
        let mut rt = RangeTracker::default();
        rt.mark_acked(0, 3);
        rt.mark_sent(3, 2);
        rt.mark_acked(6, 3);
        rt.mark_sent(10, 1);
        rt.mark_sent(12, 4);

        rt.mark_sent(0, 13);

        let mut canon = RangeTracker {
            acked: 3,
            ..RangeTracker::default()
        };
        canon.used.insert(3, (3, RangeState::Sent));
        canon.used.insert(6, (3, RangeState::Acked));
        canon.used.insert(9, (7, RangeState::Sent));
        assert_eq!(rt, canon);
    }

    /// Check that `marked_sent` correctly handles all paths.
    /// ```ignore
    ///   AAA  AAA
    /// +   SSSS
    /// = AAASSAAA
    /// ```
    #[test]
    fn mark_sent_3() {
        let mut rt = RangeTracker::default();
        rt.mark_acked(0, 3);
        rt.mark_acked(5, 3);

        rt.mark_sent(2, 4);

        let mut canon = RangeTracker {
            acked: 3,
            ..RangeTracker::default()
        };
        canon.used.insert(3, (2, RangeState::Sent));
        canon.used.insert(5, (3, RangeState::Acked));
        assert_eq!(rt, canon);
    }

    /// Check that `marked_sent` correctly handles all paths.
    /// ```ignore
    ///   SSS  AAA  SS
    /// +   SSSSSSSS
    /// = SSSSSAAASSSS
    /// ```
    #[test]
    fn mark_sent_4() {
        let mut rt = RangeTracker::default();
        rt.mark_sent(0, 3);
        rt.mark_acked(5, 3);
        rt.mark_sent(10, 2);

        rt.mark_sent(2, 8);

        let mut canon = RangeTracker::default();
        canon.used.insert(0, (5, RangeState::Sent));
        canon.used.insert(5, (3, RangeState::Acked));
        canon.used.insert(8, (4, RangeState::Sent));
        assert_eq!(rt, canon);
    }

    /// Check that `marked_sent` correctly handles all paths.
    /// ```ignore
    ///     AAA
    /// +   SSSSSS
    /// =   AAASSS
    /// ```
    #[test]
    fn mark_sent_5() {
        let mut rt = RangeTracker::default();
        rt.mark_acked(3, 3);

        rt.mark_sent(3, 6);

        let mut canon = RangeTracker::default();
        canon.used.insert(3, (3, RangeState::Acked));
        canon.used.insert(6, (3, RangeState::Sent));
        assert_eq!(rt, canon);
    }

    /// Check that `marked_sent` correctly handles all paths.
    /// ```ignore
    ///   SSSSS
    /// +  SSS
    /// = SSSSS
    /// ```
    #[test]
    fn mark_sent_6() {
        let mut rt = RangeTracker::default();
        rt.mark_sent(0, 5);

        rt.mark_sent(1, 3);

        let mut canon = RangeTracker::default();
        canon.used.insert(0, (5, RangeState::Sent));
        assert_eq!(rt, canon);
    }

    #[test]
    fn unmark_sent_start() {
        let mut rt = RangeTracker::default();

        rt.mark_sent(0, 5);
        assert_eq!(rt.highest_offset(), 5);
        assert_eq!(rt.acked_from_zero(), 0);

        rt.unmark_sent();
        assert_eq!(rt.highest_offset(), 0);
        assert_eq!(rt.acked_from_zero(), 0);
        assert_eq!(rt.first_unmarked_range(), (0, None));
    }

    #[test]
    fn unmark_sent_middle() {
        let mut rt = RangeTracker::default();

        rt.mark_acked(0, 5);
        assert_eq!(rt.highest_offset(), 5);
        assert_eq!(rt.acked_from_zero(), 5);
        rt.mark_sent(5, 5);
        assert_eq!(rt.highest_offset(), 10);
        assert_eq!(rt.acked_from_zero(), 5);
        rt.mark_acked(10, 5);
        assert_eq!(rt.highest_offset(), 15);
        assert_eq!(rt.acked_from_zero(), 5);
        assert_eq!(rt.first_unmarked_range(), (15, None));

        rt.unmark_sent();
        assert_eq!(rt.highest_offset(), 15);
        assert_eq!(rt.acked_from_zero(), 5);
        assert_eq!(rt.first_unmarked_range(), (5, Some(5)));
    }

    #[test]
    fn unmark_sent_end() {
        let mut rt = RangeTracker::default();

        rt.mark_acked(0, 5);
        assert_eq!(rt.highest_offset(), 5);
        assert_eq!(rt.acked_from_zero(), 5);
        rt.mark_sent(5, 5);
        assert_eq!(rt.highest_offset(), 10);
        assert_eq!(rt.acked_from_zero(), 5);
        assert_eq!(rt.first_unmarked_range(), (10, None));

        rt.unmark_sent();
        assert_eq!(rt.highest_offset(), 5);
        assert_eq!(rt.acked_from_zero(), 5);
        assert_eq!(rt.first_unmarked_range(), (5, None));
    }

    #[test]
    fn truncate_front() {
        let mut v = VecDeque::new();
        v.push_back(5);
        v.push_back(6);
        v.push_back(7);
        v.push_front(4usize);

        v.rotate_left(1);
        v.truncate(3);
        assert_eq!(*v.front().unwrap(), 5);
        assert_eq!(*v.back().unwrap(), 7);
    }

    #[test]
    fn unmark_range() {
        let mut rt = RangeTracker::default();

        rt.mark_acked(5, 5);
        rt.mark_sent(10, 5);

        // Should unmark sent but not acked range
        rt.unmark_range(7, 6);

        let res = rt.first_unmarked_range();
        assert_eq!(res, (0, Some(5)));
        assert_eq!(
            rt.used.iter().next().unwrap(),
            (&5, &(5, RangeState::Acked))
        );
        assert_eq!(
            rt.used.iter().nth(1).unwrap(),
            (&13, &(2, RangeState::Sent))
        );
        assert!(rt.used.iter().nth(2).is_none());
        rt.mark_sent(0, 5);

        let res = rt.first_unmarked_range();
        assert_eq!(res, (10, Some(3)));
        rt.mark_sent(10, 3);

        let res = rt.first_unmarked_range();
        assert_eq!(res, (15, None));
    }

    #[test]
    fn tx_buffer_next_bytes_1() {
        let mut txb = TxBuffer::new();

        // Fill the buffer
        let big_buf = vec![1; INITIAL_RECV_WINDOW_SIZE];
        assert_eq!(txb.send(&big_buf), INITIAL_RECV_WINDOW_SIZE);
        assert!(matches!(txb.next_bytes(),
                         Some((0, x)) if x.len() == INITIAL_RECV_WINDOW_SIZE
                         && x.iter().all(|ch| *ch == 1)));

        // Mark almost all as sent. Get what's left
        let one_byte_from_end = INITIAL_RECV_WINDOW_SIZE as u64 - 1;
        txb.mark_as_sent(0, usize::try_from(one_byte_from_end).unwrap());
        assert!(matches!(txb.next_bytes(),
                         Some((start, x)) if x.len() == 1
                         && start == one_byte_from_end
                         && x.iter().all(|ch| *ch == 1)));

        // Mark all as sent. Get nothing
        txb.mark_as_sent(0, INITIAL_RECV_WINDOW_SIZE);
        assert!(txb.next_bytes().is_none());

        // Mark as lost. Get it again
        txb.mark_as_lost(one_byte_from_end, 1);
        assert!(matches!(txb.next_bytes(),
                         Some((start, x)) if x.len() == 1
                         && start == one_byte_from_end
                         && x.iter().all(|ch| *ch == 1)));

        // Mark a larger range lost, including beyond what's in the buffer even.
        // Get a little more
        let five_bytes_from_end = INITIAL_RECV_WINDOW_SIZE as u64 - 5;
        txb.mark_as_lost(five_bytes_from_end, 100);
        assert!(matches!(txb.next_bytes(),
                         Some((start, x)) if x.len() == 5
                         && start == five_bytes_from_end
                         && x.iter().all(|ch| *ch == 1)));

        // Contig acked range at start means it can be removed from buffer
        // Impl of vecdeque should now result in a split buffer when more data
        // is sent
        txb.mark_as_acked(0, usize::try_from(five_bytes_from_end).unwrap());
        assert_eq!(txb.send(&[2; 30]), 30);
        // Just get 5 even though there is more
        assert!(matches!(txb.next_bytes(),
                         Some((start, x)) if x.len() == 5
                         && start == five_bytes_from_end
                         && x.iter().all(|ch| *ch == 1)));
        assert_eq!(txb.retired(), five_bytes_from_end);
        assert_eq!(txb.buffered(), 35);

        // Marking that bit as sent should let the last contig bit be returned
        // when called again
        txb.mark_as_sent(five_bytes_from_end, 5);
        assert!(matches!(txb.next_bytes(),
                         Some((start, x)) if x.len() == 30
                         && start == INITIAL_RECV_WINDOW_SIZE as u64
                         && x.iter().all(|ch| *ch == 2)));
    }

    #[test]
    fn tx_buffer_next_bytes_2() {
        let mut txb = TxBuffer::new();

        // Fill the buffer
        let big_buf = vec![1; INITIAL_RECV_WINDOW_SIZE];
        assert_eq!(txb.send(&big_buf), INITIAL_RECV_WINDOW_SIZE);
        assert!(matches!(txb.next_bytes(),
                         Some((0, x)) if x.len()==INITIAL_RECV_WINDOW_SIZE
                         && x.iter().all(|ch| *ch == 1)));

        // As above
        let forty_bytes_from_end = INITIAL_RECV_WINDOW_SIZE as u64 - 40;

        txb.mark_as_acked(0, usize::try_from(forty_bytes_from_end).unwrap());
        assert!(matches!(txb.next_bytes(),
                 Some((start, x)) if x.len() == 40
                 && start == forty_bytes_from_end
        ));

        // Valid new data placed in split locations
        assert_eq!(txb.send(&[2; 100]), 100);

        // Mark a little more as sent
        txb.mark_as_sent(forty_bytes_from_end, 10);
        let thirty_bytes_from_end = forty_bytes_from_end + 10;
        assert!(matches!(txb.next_bytes(),
                         Some((start, x)) if x.len() == 30
                         && start == thirty_bytes_from_end
                         && x.iter().all(|ch| *ch == 1)));

        // Mark a range 'A' in second slice as sent. Should still return the same
        let range_a_start = INITIAL_RECV_WINDOW_SIZE as u64 + 30;
        let range_a_end = range_a_start + 10;
        txb.mark_as_sent(range_a_start, 10);
        assert!(matches!(txb.next_bytes(),
                         Some((start, x)) if x.len() == 30
                         && start == thirty_bytes_from_end
                         && x.iter().all(|ch| *ch == 1)));

        // Ack entire first slice and into second slice
        let ten_bytes_past_end = INITIAL_RECV_WINDOW_SIZE as u64 + 10;
        txb.mark_as_acked(0, usize::try_from(ten_bytes_past_end).unwrap());

        // Get up to marked range A
        assert!(matches!(txb.next_bytes(),
                         Some((start, x)) if x.len() == 20
                         && start == ten_bytes_past_end
                         && x.iter().all(|ch| *ch == 2)));

        txb.mark_as_sent(ten_bytes_past_end, 20);

        // Get bit after earlier marked range A
        assert!(matches!(txb.next_bytes(),
                         Some((start, x)) if x.len() == 60
                         && start == range_a_end
                         && x.iter().all(|ch| *ch == 2)));

        // No more bytes.
        txb.mark_as_sent(range_a_end, 60);
        assert!(txb.next_bytes().is_none());
    }

    #[test]
    fn stream_tx() {
        let conn_fc = connection_fc(4096);
        let conn_events = ConnectionEvents::default();

        let mut s = SendStream::new(4.into(), 1024, Rc::clone(&conn_fc), conn_events);

        let res = s.send(&[4; 100]).unwrap();
        assert_eq!(res, 100);
        s.mark_as_sent(0, 50, false);
        if let SendStreamState::Send { fc, .. } = s.state() {
            assert_eq!(fc.used(), 100);
        } else {
            panic!("unexpected stream state");
        }

        // Should hit stream flow control limit before filling up send buffer
        let big_buf = vec![4; INITIAL_RECV_WINDOW_SIZE + 100];
        let res = s.send(&big_buf[..INITIAL_RECV_WINDOW_SIZE]).unwrap();
        assert_eq!(res, 1024 - 100);

        // should do nothing, max stream data already 1024
        s.set_max_stream_data(1024);
        let res = s.send(&big_buf[..INITIAL_RECV_WINDOW_SIZE]).unwrap();
        assert_eq!(res, 0);

        // should now hit the conn flow control (4096)
        s.set_max_stream_data(1_048_576);
        let res = s.send(&big_buf[..INITIAL_RECV_WINDOW_SIZE]).unwrap();
        assert_eq!(res, 3072);

        // should now hit the tx buffer size
        conn_fc.borrow_mut().update(INITIAL_RECV_WINDOW_SIZE as u64);
        let res = s.send(&big_buf).unwrap();
        assert_eq!(res, INITIAL_RECV_WINDOW_SIZE - 4096);

        // TODO(agrover@mozilla.com): test ooo acks somehow
        s.mark_as_acked(0, 40, false);
    }

    #[test]
    fn tx_buffer_acks() {
        let mut tx = TxBuffer::new();
        assert_eq!(tx.send(&[4; 100]), 100);
        let res = tx.next_bytes().unwrap();
        assert_eq!(res.0, 0);
        assert_eq!(res.1.len(), 100);
        tx.mark_as_sent(0, 100);
        let res = tx.next_bytes();
        assert_eq!(res, None);

        tx.mark_as_acked(0, 100);
        let res = tx.next_bytes();
        assert_eq!(res, None);
    }

    #[test]
    fn send_stream_writable_event_gen() {
        let conn_fc = connection_fc(2);
        let mut conn_events = ConnectionEvents::default();

        let mut s = SendStream::new(4.into(), 0, Rc::clone(&conn_fc), conn_events.clone());

        // Stream is initially blocked (conn:2, stream:0)
        // and will not accept data.
        assert_eq!(s.send(b"hi").unwrap(), 0);

        // increasing to (conn:2, stream:2) will allow 2 bytes, and also
        // generate a SendStreamWritable event.
        s.set_max_stream_data(2);
        let evts = conn_events.events().collect::<Vec<_>>();
        assert_eq!(evts.len(), 1);
        assert!(matches!(
            evts[0],
            ConnectionEvent::SendStreamWritable { .. }
        ));
        assert_eq!(s.send(b"hello").unwrap(), 2);

        // increasing to (conn:2, stream:4) will not generate an event or allow
        // sending anything.
        s.set_max_stream_data(4);
        assert_eq!(conn_events.events().count(), 0);
        assert_eq!(s.send(b"hello").unwrap(), 0);

        // Increasing conn max (conn:4, stream:4) will unblock but not emit
        // event b/c that happens in Connection::emit_frame() (tested in
        // connection.rs)
        assert!(conn_fc.borrow_mut().update(4).is_some());
        assert_eq!(conn_events.events().count(), 0);
        assert_eq!(s.avail(), 2);
        assert_eq!(s.send(b"hello").unwrap(), 2);

        // No event because still blocked by conn
        s.set_max_stream_data(1_000_000_000);
        assert_eq!(conn_events.events().count(), 0);

        // No event because happens in emit_frame()
        conn_fc.borrow_mut().update(1_000_000_000);
        assert_eq!(conn_events.events().count(), 0);

        let big_buf = vec![b'a'; INITIAL_RECV_WINDOW_SIZE];
        assert_eq!(s.send(&big_buf).unwrap(), INITIAL_RECV_WINDOW_SIZE);
    }

    #[test]
    fn send_stream_writable_event_gen_with_watermark() {
        let conn_fc = connection_fc(0);
        let mut conn_events = ConnectionEvents::default();

        let mut s = SendStream::new(4.into(), 0, Rc::clone(&conn_fc), conn_events.clone());
        // Set watermark at 3.
        s.set_writable_event_low_watermark(NonZeroUsize::new(3).unwrap());

        // Stream is initially blocked (conn:0, stream:0, watermark: 3) and will
        // not accept data.
        assert_eq!(s.avail(), 0);
        assert_eq!(s.send(b"hi!").unwrap(), 0);

        // Increasing the connection limit (conn:10, stream:0, watermark: 3) will not generate
        // event or allow sending anything. Stream is constrained by stream limit.
        assert!(conn_fc.borrow_mut().update(10).is_some());
        assert_eq!(s.avail(), 0);
        assert_eq!(conn_events.events().count(), 0);

        // Increasing the connection limit further (conn:11, stream:0, watermark: 3) will not
        // generate event or allow sending anything. Stream wasn't constrained by connection
        // limit before.
        assert!(conn_fc.borrow_mut().update(11).is_some());
        assert_eq!(s.avail(), 0);
        assert_eq!(conn_events.events().count(), 0);

        // Increasing to (conn:11, stream:2, watermark: 3) will allow 2 bytes
        // but not generate a SendStreamWritable event as it is still below the
        // configured watermark.
        s.set_max_stream_data(2);
        assert_eq!(conn_events.events().count(), 0);
        assert_eq!(s.avail(), 2);

        // Increasing to (conn:11, stream:3, watermark: 3) will generate an
        // event as available sendable bytes are >= watermark.
        s.set_max_stream_data(3);
        let evts = conn_events.events().collect::<Vec<_>>();
        assert_eq!(evts.len(), 1);
        assert!(matches!(
            evts[0],
            ConnectionEvent::SendStreamWritable { .. }
        ));

        assert_eq!(s.send(b"hi!").unwrap(), 3);
    }

    #[test]
    fn send_stream_writable_event_new_stream() {
        let conn_fc = connection_fc(2);
        let mut conn_events = ConnectionEvents::default();

        let _s = SendStream::new(4.into(), 100, conn_fc, conn_events.clone());

        // Creating a new stream with conn and stream credits should result in
        // an event.
        let evts = conn_events.events().collect::<Vec<_>>();
        assert_eq!(evts.len(), 1);
        assert!(matches!(
            evts[0],
            ConnectionEvent::SendStreamWritable { .. }
        ));
    }

    const fn as_stream_token(t: &RecoveryToken) -> &SendStreamRecoveryToken {
        if let RecoveryToken::Stream(StreamRecoveryToken::Stream(rt)) = &t {
            rt
        } else {
            panic!();
        }
    }

    #[test]
    // Verify lost frames handle fin properly
    fn send_stream_get_frame_data() {
        let conn_fc = connection_fc(100);
        let conn_events = ConnectionEvents::default();

        let mut s = SendStream::new(0.into(), 100, conn_fc, conn_events);
        s.send(&[0; 10]).unwrap();
        s.close();

        let mut ss = SendStreams::default();
        ss.insert(StreamId::from(0), s);

        let mut tokens = Vec::new();
        let mut builder = PacketBuilder::short(Encoder::new(), false, None::<&[u8]>);

        // Write a small frame: no fin.
        let written = builder.len();
        builder.set_limit(written + 6);
        ss.write_frames(
            TransmissionPriority::default(),
            &mut builder,
            &mut tokens,
            &mut FrameStats::default(),
        );
        assert_eq!(builder.len(), written + 6);
        assert_eq!(tokens.len(), 1);
        let f1_token = tokens.remove(0);
        assert!(!as_stream_token(&f1_token).fin);

        // Write the rest: fin.
        let written = builder.len();
        builder.set_limit(written + 200);
        ss.write_frames(
            TransmissionPriority::default(),
            &mut builder,
            &mut tokens,
            &mut FrameStats::default(),
        );
        assert_eq!(builder.len(), written + 10);
        assert_eq!(tokens.len(), 1);
        let f2_token = tokens.remove(0);
        assert!(as_stream_token(&f2_token).fin);

        // Should be no more data to frame.
        let written = builder.len();
        ss.write_frames(
            TransmissionPriority::default(),
            &mut builder,
            &mut tokens,
            &mut FrameStats::default(),
        );
        assert_eq!(builder.len(), written);
        assert!(tokens.is_empty());

        // Mark frame 1 as lost
        ss.lost(as_stream_token(&f1_token));

        // Next frame should not set fin even though stream has fin but frame
        // does not include end of stream
        let written = builder.len();
        ss.write_frames(
            TransmissionPriority::default() + RetransmissionPriority::default(),
            &mut builder,
            &mut tokens,
            &mut FrameStats::default(),
        );
        assert_eq!(builder.len(), written + 7); // Needs a length this time.
        assert_eq!(tokens.len(), 1);
        let f4_token = tokens.remove(0);
        assert!(!as_stream_token(&f4_token).fin);

        // Mark frame 2 as lost
        ss.lost(as_stream_token(&f2_token));

        // Next frame should set fin because it includes end of stream
        let written = builder.len();
        ss.write_frames(
            TransmissionPriority::default() + RetransmissionPriority::default(),
            &mut builder,
            &mut tokens,
            &mut FrameStats::default(),
        );
        assert_eq!(builder.len(), written + 10);
        assert_eq!(tokens.len(), 1);
        let f5_token = tokens.remove(0);
        assert!(as_stream_token(&f5_token).fin);
    }

    #[test]
    // Verify lost frames handle fin properly with zero length fin
    fn send_stream_get_frame_zerolength_fin() {
        let conn_fc = connection_fc(100);
        let conn_events = ConnectionEvents::default();

        let mut s = SendStream::new(0.into(), 100, conn_fc, conn_events);
        s.send(&[0; 10]).unwrap();

        let mut ss = SendStreams::default();
        ss.insert(StreamId::from(0), s);

        let mut tokens = Vec::new();
        let mut builder = PacketBuilder::short(Encoder::new(), false, None::<&[u8]>);
        ss.write_frames(
            TransmissionPriority::default(),
            &mut builder,
            &mut tokens,
            &mut FrameStats::default(),
        );
        let f1_token = tokens.remove(0);
        assert_eq!(as_stream_token(&f1_token).offset, 0);
        assert_eq!(as_stream_token(&f1_token).length, 10);
        assert!(!as_stream_token(&f1_token).fin);

        // Should be no more data to frame
        ss.write_frames(
            TransmissionPriority::default(),
            &mut builder,
            &mut tokens,
            &mut FrameStats::default(),
        );
        assert!(tokens.is_empty());

        ss.get_mut(StreamId::from(0)).unwrap().close();

        ss.write_frames(
            TransmissionPriority::default(),
            &mut builder,
            &mut tokens,
            &mut FrameStats::default(),
        );
        let f2_token = tokens.remove(0);
        assert_eq!(as_stream_token(&f2_token).offset, 10);
        assert_eq!(as_stream_token(&f2_token).length, 0);
        assert!(as_stream_token(&f2_token).fin);

        // Mark frame 2 as lost
        ss.lost(as_stream_token(&f2_token));

        // Next frame should set fin
        ss.write_frames(
            TransmissionPriority::default(),
            &mut builder,
            &mut tokens,
            &mut FrameStats::default(),
        );
        let f3_token = tokens.remove(0);
        assert_eq!(as_stream_token(&f3_token).offset, 10);
        assert_eq!(as_stream_token(&f3_token).length, 0);
        assert!(as_stream_token(&f3_token).fin);

        // Mark frame 1 as lost
        ss.lost(as_stream_token(&f1_token));

        // Next frame should set fin and include all data
        ss.write_frames(
            TransmissionPriority::default(),
            &mut builder,
            &mut tokens,
            &mut FrameStats::default(),
        );
        let f4_token = tokens.remove(0);
        assert_eq!(as_stream_token(&f4_token).offset, 0);
        assert_eq!(as_stream_token(&f4_token).length, 10);
        assert!(as_stream_token(&f4_token).fin);
    }

    #[test]
    fn data_blocked() {
        let conn_fc = connection_fc(5);
        let conn_events = ConnectionEvents::default();

        let stream_id = StreamId::from(4);
        let mut s = SendStream::new(stream_id, 2, Rc::clone(&conn_fc), conn_events);

        // Only two bytes can be sent due to the stream limit.
        assert_eq!(s.send(b"abc").unwrap(), 2);
        assert_eq!(s.next_bytes(false), Some((0, &b"ab"[..])));

        // This doesn't report blocking yet.
        let mut builder = PacketBuilder::short(Encoder::new(), false, None::<&[u8]>);
        let mut tokens = Vec::new();
        let mut stats = FrameStats::default();
        s.write_blocked_frame(
            TransmissionPriority::default(),
            &mut builder,
            &mut tokens,
            &mut stats,
        );
        assert_eq!(stats.stream_data_blocked, 0);

        // Blocking is reported after sending the last available credit.
        s.mark_as_sent(0, 2, false);
        s.write_blocked_frame(
            TransmissionPriority::default(),
            &mut builder,
            &mut tokens,
            &mut stats,
        );
        assert_eq!(stats.stream_data_blocked, 1);

        // Now increase the stream limit and test the connection limit.
        s.set_max_stream_data(10);

        assert_eq!(s.send(b"abcd").unwrap(), 3);
        assert_eq!(s.next_bytes(false), Some((2, &b"abc"[..])));
        // DATA_BLOCKED is not sent yet.
        conn_fc
            .borrow_mut()
            .write_frames(&mut builder, &mut tokens, &mut stats);
        assert_eq!(stats.data_blocked, 0);

        // DATA_BLOCKED is queued once bytes using all credit are sent.
        s.mark_as_sent(2, 3, false);
        conn_fc
            .borrow_mut()
            .write_frames(&mut builder, &mut tokens, &mut stats);
        assert_eq!(stats.data_blocked, 1);
    }

    #[test]
    fn max_send_buffer_size() {
        // Huge FC limit. Thus buffer size limited only.
        const FC_LIMIT: u64 = 1024 * 1024 * 1024;
        let s = SendStream::new(
            StreamId::from(4),
            FC_LIMIT,
            connection_fc(FC_LIMIT),
            ConnectionEvents::default(),
        );
        assert_eq!(s.avail(), MAX_SEND_BUFFER_SIZE);
    }

    #[test]
    fn data_blocked_atomic() {
        let conn_fc = connection_fc(5);
        let conn_events = ConnectionEvents::default();

        let stream_id = StreamId::from(4);
        let mut s = SendStream::new(stream_id, 2, Rc::clone(&conn_fc), conn_events);

        // Stream is initially blocked (conn:5, stream:2)
        // and will not accept atomic write of 3 bytes.
        assert_eq!(s.send_atomic(b"abc").unwrap(), 0);

        // Assert that STREAM_DATA_BLOCKED is sent.
        let mut builder = PacketBuilder::short(Encoder::new(), false, None::<&[u8]>);
        let mut tokens = Vec::new();
        let mut stats = FrameStats::default();
        s.write_blocked_frame(
            TransmissionPriority::default(),
            &mut builder,
            &mut tokens,
            &mut stats,
        );
        assert_eq!(stats.stream_data_blocked, 1);

        // Assert that a non-atomic write works.
        assert_eq!(s.send(b"abc").unwrap(), 2);
        assert_eq!(s.next_bytes(false), Some((0, &b"ab"[..])));
        s.mark_as_sent(0, 2, false);

        // Set limits to (conn:5, stream:10).
        s.set_max_stream_data(10);

        // An atomic write of 4 bytes exceeds the remaining limit of 3.
        assert_eq!(s.send_atomic(b"abcd").unwrap(), 0);

        // Assert that DATA_BLOCKED is sent.
        conn_fc
            .borrow_mut()
            .write_frames(&mut builder, &mut tokens, &mut stats);
        assert_eq!(stats.data_blocked, 1);

        // Check that a non-atomic write works.
        assert_eq!(s.send(b"abcd").unwrap(), 3);
        assert_eq!(s.next_bytes(false), Some((2, &b"abc"[..])));
        s.mark_as_sent(2, 3, false);

        // Increase limits to (conn:15, stream:15).
        s.set_max_stream_data(15);
        conn_fc.borrow_mut().update(15);

        // Check that atomic writing right up to the limit works.
        assert_eq!(s.send_atomic(b"abcdefghij").unwrap(), 10);
    }

    #[test]
    fn ack_fin_first() {
        const MESSAGE: &[u8] = b"hello";
        let len_u64 = u64::try_from(MESSAGE.len()).unwrap();

        let conn_fc = connection_fc(len_u64);
        let conn_events = ConnectionEvents::default();

        let mut s = SendStream::new(StreamId::new(100), 0, conn_fc, conn_events);
        s.set_max_stream_data(len_u64);

        // Send all the data, then the fin.
        _ = s.send(MESSAGE).unwrap();
        s.mark_as_sent(0, MESSAGE.len(), false);
        s.close();
        s.mark_as_sent(len_u64, 0, true);

        // Ack the fin, then the data.
        s.mark_as_acked(len_u64, 0, true);
        s.mark_as_acked(0, MESSAGE.len(), false);
        assert!(s.is_terminal());
    }

    #[test]
    fn ack_then_lose_fin() {
        const MESSAGE: &[u8] = b"hello";
        let len_u64 = u64::try_from(MESSAGE.len()).unwrap();

        let conn_fc = connection_fc(len_u64);
        let conn_events = ConnectionEvents::default();

        let id = StreamId::new(100);
        let mut s = SendStream::new(id, 0, conn_fc, conn_events);
        s.set_max_stream_data(len_u64);

        // Send all the data, then the fin.
        _ = s.send(MESSAGE).unwrap();
        s.mark_as_sent(0, MESSAGE.len(), false);
        s.close();
        s.mark_as_sent(len_u64, 0, true);

        // Ack the fin, then mark it lost.
        s.mark_as_acked(len_u64, 0, true);
        s.mark_as_lost(len_u64, 0, true);

        // No frame should be sent here.
        let mut builder = PacketBuilder::short(Encoder::new(), false, None::<&[u8]>);
        let mut tokens = Vec::new();
        let mut stats = FrameStats::default();
        s.write_stream_frame(
            TransmissionPriority::default(),
            &mut builder,
            &mut tokens,
            &mut stats,
        );
        assert_eq!(stats.stream, 0);
    }

    /// Create a `SendStream` and force it into a state where it believes that
    /// `offset` bytes have already been sent and acknowledged.
    fn stream_with_sent(stream: u64, offset: usize) -> SendStream {
        let conn_fc = connection_fc(MAX_VARINT);
        let mut s = SendStream::new(
            StreamId::from(stream),
            MAX_VARINT,
            conn_fc,
            ConnectionEvents::default(),
        );

        let mut send_buf = TxBuffer::new();
        send_buf.ranges.mark_acked(0, offset);
        let mut fc = SenderFlowControl::new(StreamId::from(stream), MAX_VARINT);
        fc.consume(offset);
        let conn_fc = Rc::new(RefCell::new(SenderFlowControl::new((), MAX_VARINT)));
        s.state = SendStreamState::Send {
            fc,
            conn_fc,
            send_buf,
        };
        s
    }

    fn frame_sent_sid(stream: u64, offset: usize, len: usize, fin: bool, space: usize) -> bool {
        const BUF: &[u8] = &[0x42; 128];

        qtrace!("frame_sent stream={stream} offset={offset} len={len} fin={fin}, space={space}");

        let mut s = stream_with_sent(stream, offset);

        // Now write out the proscribed data and maybe close.
        if len > 0 {
            s.send(&BUF[..len]).unwrap();
        }
        if fin {
            s.close();
        }

        let mut builder = PacketBuilder::short(Encoder::new(), false, None::<&[u8]>);
        let header_len = builder.len();
        builder.set_limit(header_len + space);

        let mut tokens = Vec::new();
        let mut stats = FrameStats::default();
        s.write_stream_frame(
            TransmissionPriority::default(),
            &mut builder,
            &mut tokens,
            &mut stats,
        );
        qtrace!(
            "STREAM frame: {}",
            hex_with_len(&builder.as_ref()[header_len..])
        );
        stats.stream > 0
    }

    fn frame_sent(offset: usize, len: usize, fin: bool, space: usize) -> bool {
        frame_sent_sid(0, offset, len, fin, space)
    }

    #[test]
    fn stream_frame_empty() {
        // Stream frames with empty data and no fin never work.
        assert!(!frame_sent(10, 0, false, 2));
        assert!(!frame_sent(10, 0, false, 3));
        assert!(!frame_sent(10, 0, false, 4));
        assert!(!frame_sent(10, 0, false, 5));
        assert!(!frame_sent(10, 0, false, 100));

        // Empty data with fin is only a problem if there is no space.
        assert!(!frame_sent(0, 0, true, 1));
        assert!(frame_sent(0, 0, true, 2));
        assert!(!frame_sent(10, 0, true, 2));
        assert!(frame_sent(10, 0, true, 3));
        assert!(frame_sent(10, 0, true, 4));
        assert!(frame_sent(10, 0, true, 5));
        assert!(frame_sent(10, 0, true, 100));
    }

    #[test]
    fn stream_frame_minimum() {
        // Add minimum data
        assert!(!frame_sent(10, 1, false, 3));
        assert!(!frame_sent(10, 1, true, 3));
        assert!(frame_sent(10, 1, false, 4));
        assert!(frame_sent(10, 1, true, 4));
        assert!(frame_sent(10, 1, false, 5));
        assert!(frame_sent(10, 1, true, 5));
        assert!(frame_sent(10, 1, false, 100));
        assert!(frame_sent(10, 1, true, 100));
    }

    #[test]
    fn stream_frame_more() {
        // Try more data
        assert!(!frame_sent(10, 100, false, 3));
        assert!(!frame_sent(10, 100, true, 3));
        assert!(frame_sent(10, 100, false, 4));
        assert!(frame_sent(10, 100, true, 4));
        assert!(frame_sent(10, 100, false, 5));
        assert!(frame_sent(10, 100, true, 5));
        assert!(frame_sent(10, 100, false, 100));
        assert!(frame_sent(10, 100, true, 100));

        assert!(frame_sent(10, 100, false, 1000));
        assert!(frame_sent(10, 100, true, 1000));
    }

    #[test]
    fn stream_frame_big_id() {
        // A value that encodes to the largest varint.
        const BIG: u64 = 1 << 30;
        const BIGSZ: usize = 1 << 30;

        assert!(!frame_sent_sid(BIG, BIGSZ, 0, false, 16));
        assert!(!frame_sent_sid(BIG, BIGSZ, 0, true, 16));
        assert!(!frame_sent_sid(BIG, BIGSZ, 0, false, 17));
        assert!(frame_sent_sid(BIG, BIGSZ, 0, true, 17));
        assert!(!frame_sent_sid(BIG, BIGSZ, 0, false, 18));
        assert!(frame_sent_sid(BIG, BIGSZ, 0, true, 18));

        assert!(!frame_sent_sid(BIG, BIGSZ, 1, false, 17));
        assert!(!frame_sent_sid(BIG, BIGSZ, 1, true, 17));
        assert!(frame_sent_sid(BIG, BIGSZ, 1, false, 18));
        assert!(frame_sent_sid(BIG, BIGSZ, 1, true, 18));
        assert!(frame_sent_sid(BIG, BIGSZ, 1, false, 19));
        assert!(frame_sent_sid(BIG, BIGSZ, 1, true, 19));
        assert!(frame_sent_sid(BIG, BIGSZ, 1, false, 100));
        assert!(frame_sent_sid(BIG, BIGSZ, 1, true, 100));
    }

    fn stream_frame_at_boundary(data: &[u8]) {
        fn send_with_extra_capacity(data: &[u8], extra: usize, expect_full: bool) -> Vec<u8> {
            qtrace!("send_with_extra_capacity {} + {extra}", data.len());
            let mut s = stream_with_sent(0, 0);
            s.send(data).unwrap();
            s.close();

            let mut builder = PacketBuilder::short(Encoder::new(), false, None::<&[u8]>);
            let header_len = builder.len();
            // Add 2 for the frame type and stream ID, then add the extra.
            builder.set_limit(header_len + data.len() + 2 + extra);
            let mut tokens = Vec::new();
            let mut stats = FrameStats::default();
            s.write_stream_frame(
                TransmissionPriority::default(),
                &mut builder,
                &mut tokens,
                &mut stats,
            );
            assert_eq!(stats.stream, 1);
            assert_eq!(builder.is_full(), expect_full);
            Vec::from(Encoder::from(builder)).split_off(header_len)
        }

        // The minimum amount of extra space for getting another frame in.
        let mut enc = Encoder::new();
        enc.encode_varint(u64::try_from(data.len()).unwrap());
        let len_buf = Vec::from(enc);
        let minimum_extra = len_buf.len() + PacketBuilder::MINIMUM_FRAME_SIZE;

        // For anything short of the minimum extra, the frame should fill the packet.
        for i in 0..minimum_extra {
            let frame = send_with_extra_capacity(data, i, true);
            let (header, body) = frame.split_at(2);
            assert_eq!(header, &[0b1001, 0]);
            assert_eq!(body, data);
        }

        // Once there is space for another packet AND a length field,
        // then a length will be added.
        let frame = send_with_extra_capacity(data, minimum_extra, false);
        let (header, rest) = frame.split_at(2);
        assert_eq!(header, &[0b1011, 0]);
        let (len, body) = rest.split_at(len_buf.len());
        assert_eq!(len, &len_buf);
        assert_eq!(body, data);
    }

    /// 16383/16384 is an odd boundary in STREAM frame construction.
    /// That is the boundary where a length goes from 2 bytes to 4 bytes.
    /// Test that we correctly add a length field to the frame; and test
    /// that if we don't, then we don't allow other frames to be added.
    #[test]
    fn stream_frame_16384() {
        stream_frame_at_boundary(&[4; 16383]);
        stream_frame_at_boundary(&[4; 16384]);
    }

    /// 63/64 is the other odd boundary.
    #[test]
    fn stream_frame_64() {
        stream_frame_at_boundary(&[2; 63]);
        stream_frame_at_boundary(&[2; 64]);
    }

    fn check_stats(
        stream: &SendStream,
        expected_written: u64,
        expected_sent: u64,
        expected_acked: u64,
    ) {
        let stream_stats = stream.stats();
        assert_eq!(stream_stats.bytes_written(), expected_written);
        assert_eq!(stream_stats.bytes_sent(), expected_sent);
        assert_eq!(stream_stats.bytes_acked(), expected_acked);
    }

    #[test]
    fn send_stream_stats() {
        const MESSAGE: &[u8] = b"hello";
        let len_u64 = u64::try_from(MESSAGE.len()).unwrap();

        let conn_fc = connection_fc(len_u64);
        let conn_events = ConnectionEvents::default();

        let id = StreamId::new(100);
        let mut s = SendStream::new(id, 0, conn_fc, conn_events);
        s.set_max_stream_data(len_u64);

        // Initial stats should be all 0.
        check_stats(&s, 0, 0, 0);
        // Adter sending the data, bytes_written should be increased.
        _ = s.send(MESSAGE).unwrap();
        check_stats(&s, len_u64, 0, 0);

        // Adter calling mark_as_sent, bytes_sent should be increased.
        s.mark_as_sent(0, MESSAGE.len(), false);
        check_stats(&s, len_u64, len_u64, 0);

        s.close();
        s.mark_as_sent(len_u64, 0, true);

        // In the end, check bytes_acked.
        s.mark_as_acked(0, MESSAGE.len(), false);
        check_stats(&s, len_u64, len_u64, len_u64);

        s.mark_as_acked(len_u64, 0, true);
        assert!(s.is_terminal());
    }
}
