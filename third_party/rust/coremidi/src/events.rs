use std::fmt::Formatter;
use std::marker::PhantomData;
use std::mem::size_of;
use std::ops::Deref;
use std::slice;

use coremidi_sys::{
    MIDIEventList, MIDIEventListAdd, MIDIEventListInit, MIDIEventPacket, MIDIEventPacketNext,
};

use crate::protocol::Protocol;

pub type Timestamp = u64;

/// A variable-length list of MIDI event packets
/// See [MIDIEventList](https://developer.apple.com/documentation/coremidi/midieventlist)
///
pub struct EventList(MIDIEventList);

impl EventList {
    pub fn protocol(&self) -> Protocol {
        Protocol::from(self.0.protocol)
    }

    /// Check if the packet list is empty.
    ///
    pub fn is_empty(&self) -> bool {
        self.len() == 0
    }

    /// Get the number of packets in the list.
    ///
    pub fn len(&self) -> usize {
        self.0.numPackets as usize
    }

    /// Get an iterator for the packets in the list.
    ///
    pub fn iter(&self) -> EventListIter {
        EventListIter {
            count: self.len(),
            packet_ptr: std::ptr::addr_of!(self.0.packet) as *const MIDIEventPacket,
            _phantom: PhantomData,
        }
    }

    /// For internal usage only.
    /// Requires this instance to actually point to a valid MIDIEventList
    pub(crate) unsafe fn as_ptr(&self) -> *const MIDIEventList {
        self as *const EventList as *const MIDIEventList
    }
}

impl std::fmt::Debug for EventList {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        writeln!(
            f,
            "EventList(protocol={:?}, packets={})",
            self.protocol(),
            self.len()
        )?;
        for packet in self.iter() {
            writeln!(f, "{:?}", packet)?;
        }
        Ok(())
    }
}

pub struct EventListIter<'a> {
    count: usize,
    packet_ptr: *const MIDIEventPacket,
    _phantom: PhantomData<&'a MIDIEventPacket>,
}

impl<'a> Iterator for EventListIter<'a> {
    type Item = &'a EventPacket;

    fn next(&mut self) -> Option<&'a EventPacket> {
        if self.count > 0 {
            let packet = unsafe { &*(self.packet_ptr as *const EventPacket) };
            self.count -= 1;
            self.packet_ptr = unsafe { MIDIEventPacketNext(self.packet_ptr) };
            Some(packet)
        } else {
            None
        }
    }
}

pub struct EventPacket(MIDIEventPacket);

impl EventPacket {
    pub fn timestamp(&self) -> Timestamp {
        self.0.timeStamp as Timestamp
    }

    /// Get the packet data. This method just gives raw MIDI words. You would need another
    /// library to decode them and work with higher level events.
    ///
    pub fn data(&self) -> &[u32] {
        let data_ptr = self.0.words.as_ptr();
        let data_len = self.0.wordCount as usize;
        unsafe { slice::from_raw_parts(data_ptr, data_len) }
    }
}

impl std::fmt::Debug for EventPacket {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        write!(f, "  {:024}:", self.timestamp())?;
        for word in self.data().iter() {
            write!(f, " {:08x}", word)?;
        }
        Ok(())
    }
}

#[derive(Clone)]
pub struct EventBuffer {
    storage: Storage,
    current_packet_offset: usize,
}

impl EventBuffer {
    const PACKET_HEADER_SIZE: usize = 8 +     // MIDIEventPacket::timestamp: MIDITimeStamp/UInt64
                                      4; // MIDIEventPacket::wordCount: UInt32

    /// Create an empty `EventBuffer` for a given [Protocol] without allocating.
    ///
    pub fn new(protocol: Protocol) -> Self {
        Self::with_capacity(Storage::INLINE_SIZE, protocol)
    }

    /// Create an empty `EventBuffer` of a given capacity for a given [Protocol].
    ///
    pub fn with_capacity(capacity: usize, protocol: Protocol) -> Self {
        let mut storage = Storage::with_capacity(capacity);
        let event_list_ptr = unsafe { storage.as_mut_ptr::<MIDIEventList>() };
        let current_packet_ptr = unsafe { MIDIEventListInit(event_list_ptr, protocol.into()) };
        let current_packet_offset = unsafe {
            (current_packet_ptr as *const u8).offset_from(event_list_ptr as *const u8) as usize
        };
        Self {
            storage,
            current_packet_offset,
        }
    }

    /// Get underlying buffer capacity in bytes
    ///
    pub fn capacity(&self) -> usize {
        self.storage.capacity()
    }

    /// Add a new packet containing the provided timestamp and data.
    /// It consumes the instance and returns it modified with the new packet.
    ///
    /// See [EventBuffer::push] for further details.
    ///
    /// Example:
    ///
    /// ```
    /// use coremidi::{Protocol, Timestamp, EventBuffer};
    ///
    /// let buffer = EventBuffer::new(Protocol::Midi20)
    ///     .with_packet(0, &[0x40903c00, 0xffff0000]); // Note On for Middle C
    ///
    /// assert_eq!(buffer.len(), 1);
    /// assert_eq!(
    ///     buffer.iter()
    ///         .map(|packet| (packet.timestamp(), packet.data().to_vec()))
    ///         .collect::<Vec<(Timestamp, Vec<u32>)>>(),
    ///     vec![(0, vec![0x40903c00, 0xffff0000])],
    /// )
    /// ```
    pub fn with_packet(mut self, timestamp: Timestamp, data: &[u32]) -> Self {
        self.push(timestamp, data);
        self
    }

    /// Add a new event containing the provided timestamp and data.
    ///
    /// According to the official documentation for CoreMIDI, the timestamp represents
    /// the time at which the events are to be played, where zero means "now".
    /// The timestamp applies to the first MIDI word in the packet.
    ///
    /// An event must not have a timestamp that is smaller than that of a previous event
    /// in the same `EventBuffer`
    ///
    /// Example:
    ///
    /// ```
    /// use coremidi::{EventBuffer, Protocol, Timestamp};
    ///
    /// let mut buffer = EventBuffer::new(Protocol::Midi20);
    /// buffer.push(0, &[0x40903c00, 0xffff0000]); // Note On for Middle C
    ///
    /// assert_eq!(buffer.len(), 1);
    /// assert_eq!(
    ///     buffer.iter()
    ///         .map(|packet| (packet.timestamp(), packet.data().to_vec()))
    ///         .collect::<Vec<(Timestamp, Vec<u32>)>>(),
    ///     vec![(0, vec![0x40903c00, 0xffff0000])],
    /// )
    /// ```
    pub fn push(&mut self, timestamp: Timestamp, data: &[u32]) -> &mut Self {
        self.ensure_capacity(data.len());

        let packet_list_ptr = unsafe { self.storage.as_mut_ptr::<MIDIEventList>() };
        let current_packet_ptr = unsafe {
            self.storage.as_ptr::<u8>().add(self.current_packet_offset) as *mut MIDIEventPacket
        };
        let current_packet_ptr = unsafe {
            MIDIEventListAdd(
                packet_list_ptr,
                self.storage.capacity() as u64,
                current_packet_ptr,
                timestamp,
                data.len() as u64,
                data.as_ptr(),
            )
        };

        self.current_packet_offset = unsafe {
            (current_packet_ptr as *const u8).offset_from(packet_list_ptr as *const u8) as usize
        };

        self
    }

    /// Clears the buffer, removing all packets.
    /// Note that this method has no effect on the allocated capacity of the buffer.
    pub fn clear(&mut self) {
        let event_list_ptr = unsafe { self.storage.as_mut_ptr::<MIDIEventList>() };
        let protocol = unsafe { (*event_list_ptr).protocol };
        let current_packet_ptr = unsafe { MIDIEventListInit(event_list_ptr, protocol) };
        self.current_packet_offset = unsafe {
            (current_packet_ptr as *const u8).offset_from(event_list_ptr as *const u8) as usize
        };
    }

    fn ensure_capacity(&mut self, data_len: usize) {
        let next_capacity =
            self.aligned_bytes_len() + Self::PACKET_HEADER_SIZE + data_len * size_of::<u32>();

        unsafe {
            // We ensure capacity for the worst case as if there was no merge with the current packet
            self.storage.ensure_capacity(next_capacity);
        }
    }

    #[inline]
    fn aligned_bytes_len(&self) -> usize {
        let storage_start_ptr = unsafe { self.storage.as_ptr::<u8>() };
        if self.as_ref().is_empty() {
            self.current_packet_offset
        } else {
            let current_packet = unsafe {
                &*(self.storage.as_ptr::<u8>().add(self.current_packet_offset)
                    as *const MIDIEventPacket)
            };
            let current_packet_data_ptr = current_packet.words.as_ptr() as *const u8;
            let data_offset = current_packet_data_ptr as *const u8 as usize
                - storage_start_ptr as *const u8 as usize;
            let data_len = current_packet.wordCount as usize * size_of::<u32>();
            (data_offset + data_len + 3) & !3
        }
    }
}

impl AsRef<EventList> for EventBuffer {
    #[inline]
    fn as_ref(&self) -> &EventList {
        unsafe { &*self.storage.as_ptr::<EventList>() }
    }
}

impl Deref for EventBuffer {
    type Target = EventList;

    #[inline]
    fn deref(&self) -> &EventList {
        self.as_ref()
    }
}

#[derive(Clone)]
pub(crate) enum Storage {
    /// Inline stores the data directly on the stack, if it is small enough.
    /// NOTE: using u32 ensures correct alignment (required on ARM)
    Inline([u32; Storage::INLINE_SIZE / 4]),
    /// External is used whenever the size of the data exceeds INLINE_PACKET_BUFFER_SIZE.
    /// This means that the size of the contained vector is always greater than INLINE_PACKET_BUFFER_SIZE.
    External(Vec<u32>),
}

impl Storage {
    pub(crate) const INLINE_SIZE: usize = 8 // MIDIEventList header
        + 12 // MIDIEventPacket header
        + 4 * 4; // 4 words

    #[inline]
    #[allow(clippy::uninit_vec)]
    pub(crate) fn with_capacity(capacity: usize) -> Self {
        if capacity <= Self::INLINE_SIZE {
            Self::Inline([0; Self::INLINE_SIZE / 4])
        } else {
            let u32_len = ((capacity - 1) / 4) + 1;
            let mut buffer = Vec::with_capacity(u32_len);
            unsafe {
                buffer.set_len(u32_len);
            }
            Storage::External(buffer)
        }
    }

    #[inline]
    pub(crate) fn capacity(&self) -> usize {
        match *self {
            Storage::Inline(ref inline) => inline.len() * 4,
            Storage::External(ref vec) => vec.len() * 4,
        }
    }

    #[inline]
    pub(crate) fn get_slice<T>(&self) -> &[T] {
        unsafe {
            match *self {
                Storage::Inline(ref inline) => slice::from_raw_parts(
                    inline.as_ptr() as *const T,
                    inline.len() * size_of::<u32>() / size_of::<T>(),
                ),
                Storage::External(ref vec) => {
                    slice::from_raw_parts(vec.as_ptr() as *const T, vec.len() * 4 / size_of::<T>())
                }
            }
        }
    }

    /// Call this only with larger length values (won't make the buffer smaller)
    #[allow(clippy::uninit_vec)]
    pub(crate) unsafe fn ensure_capacity(&mut self, capacity: usize) {
        if capacity < Self::INLINE_SIZE || capacity < self.get_slice::<u8>().len() {
            return;
        }

        let vec_capacity = ((capacity - 1) / 4) + 1;
        let vec: Option<Vec<u32>> = match *self {
            Storage::Inline(ref inline) => {
                let mut v = Vec::with_capacity(vec_capacity);
                v.extend_from_slice(inline);
                v.set_len(vec_capacity);
                Some(v)
            }
            Storage::External(ref mut vec) => {
                let current_len = vec.len();
                vec.reserve(vec_capacity - current_len);
                vec.set_len(vec_capacity);
                None
            }
        };

        // to prevent borrow-check errors, this must come after the `match`
        if let Some(v) = vec {
            *self = Storage::External(v);
        }
    }

    #[inline]
    pub(crate) unsafe fn as_ptr<T>(&self) -> *const T {
        match *self {
            Storage::Inline(ref inline) => inline.as_ptr() as *const T,
            Storage::External(ref vec) => vec.as_ptr() as *const T,
        }
    }

    #[inline]
    pub(crate) unsafe fn as_mut_ptr<T>(&mut self) -> *mut T {
        self.as_ptr::<T>() as *mut T
    }
}

impl std::fmt::Debug for Storage {
    fn fmt(&self, f: &mut Formatter<'_>) -> std::fmt::Result {
        for b in self.get_slice::<u8>() {
            write!(f, " {:02x}", *b)?;
        }
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use crate::events::{Storage, Timestamp};
    use crate::protocol::Protocol;
    use crate::{EventBuffer, EventList};
    use coremidi_sys::{
        kMIDIProtocol_2_0, ByteCount, MIDIEventList, MIDIEventListAdd, MIDIEventListInit,
        MIDIProtocolID,
    };

    #[test]
    fn event_list_accessors() {
        const BUFFER_SIZE: usize = 256;
        let buffer = [0u8; BUFFER_SIZE];
        let event_list_ptr = buffer.as_ptr() as *const MIDIEventList as *mut MIDIEventList;
        let event_packet_ptr =
            unsafe { MIDIEventListInit(event_list_ptr, kMIDIProtocol_2_0 as MIDIProtocolID) };
        let event_packet_ptr = unsafe {
            MIDIEventListAdd(
                event_list_ptr,
                BUFFER_SIZE as ByteCount,
                event_packet_ptr,
                10,
                2,
                [1, 2].as_ptr(),
            )
        };
        let _ = unsafe {
            MIDIEventListAdd(
                event_list_ptr,
                BUFFER_SIZE as ByteCount,
                event_packet_ptr,
                20,
                3,
                [3, 4, 5].as_ptr(),
            )
        };
        let event_list = unsafe { &*(event_list_ptr as *const EventList) };

        assert_eq!(event_list.protocol(), Protocol::Midi20);
        assert!(!event_list.is_empty());
        assert_eq!(event_list.len(), 2);

        assert_eq!(
            event_list
                .iter()
                .map(|packet| (packet.timestamp(), packet.data().to_vec()))
                .collect::<Vec<(Timestamp, Vec<u32>)>>(),
            vec![(10, vec![1, 2]), (20, vec![3, 4, 5]),]
        );
    }

    #[test]
    fn event_buffer_new() {
        let event_buffer = EventBuffer::new(Protocol::Midi20);

        assert_eq!(event_buffer.capacity(), Storage::INLINE_SIZE);
        assert_eq!(event_buffer.protocol(), Protocol::Midi20);
        assert_eq!(event_buffer.len(), 0);
    }

    #[test]
    fn event_buffer_with_capacity_inline() {
        let event_buffer = EventBuffer::with_capacity(2, Protocol::Midi20);

        assert_eq!(event_buffer.capacity(), Storage::INLINE_SIZE);
        assert_eq!(event_buffer.protocol(), Protocol::Midi20);
        assert_eq!(event_buffer.len(), 0);
    }

    #[test]
    fn event_buffer_with_capacity_external() {
        let event_buffer = EventBuffer::with_capacity(Storage::INLINE_SIZE * 2, Protocol::Midi20);

        assert_eq!(event_buffer.capacity(), Storage::INLINE_SIZE * 2);
    }

    #[test]
    fn event_buffer_with_packet() {
        let event_buffer = EventBuffer::new(Protocol::Midi20)
            .with_packet(10, &[1, 2])
            .with_packet(20, &[3, 4, 5]);

        assert_eq!(event_buffer.len(), 2);
        assert_eq!(
            event_buffer
                .iter()
                .map(|packet| (packet.timestamp(), packet.data().to_vec()))
                .collect::<Vec<(Timestamp, Vec<u32>)>>(),
            vec![(10, vec![1, 2]), (20, vec![3, 4, 5]),]
        );
    }

    #[test]
    fn event_buffer_push_within_capacity() {
        let mut event_buffer = EventBuffer::new(Protocol::Midi20);
        event_buffer.push(10, &[1, 2]).push(20, &[3, 4, 5]);

        assert_eq!(event_buffer.len(), 2);
        assert_eq!(
            event_buffer
                .iter()
                .map(|packet| (packet.timestamp(), packet.data().to_vec()))
                .collect::<Vec<(Timestamp, Vec<u32>)>>(),
            vec![(10, vec![1, 2]), (20, vec![3, 4, 5]),]
        );
    }

    #[test]
    fn event_buffer_push_over_capacity() {
        let mut event_buffer = EventBuffer::new(Protocol::Midi20);
        event_buffer
            .push(10, &[1, 2])
            .push(20, &[3, 4, 5, 6, 7, 8, 9, 10]);

        assert_eq!(event_buffer.len(), 2);
        assert_eq!(
            event_buffer
                .iter()
                .map(|packet| (packet.timestamp(), packet.data().to_vec()))
                .collect::<Vec<(Timestamp, Vec<u32>)>>(),
            vec![(10, vec![1, 2]), (20, vec![3, 4, 5, 6, 7, 8, 9, 10])]
        );
    }

    #[test]
    fn event_buffer_clear() {
        let mut event_buffer = EventBuffer::new(Protocol::Midi20).with_packet(10, &[1, 2]);

        assert_eq!(event_buffer.len(), 1);
        assert_eq!(
            event_buffer
                .iter()
                .map(|packet| (packet.timestamp(), packet.data().to_vec()))
                .collect::<Vec<(Timestamp, Vec<u32>)>>(),
            vec![(10, vec![1, 2])]
        );

        event_buffer.clear();

        assert_eq!(event_buffer.len(), 0);
        assert_eq!(event_buffer.capacity(), Storage::INLINE_SIZE);
        assert_eq!(
            event_buffer
                .iter()
                .map(|packet| (packet.timestamp(), packet.data().to_vec()))
                .collect::<Vec<(Timestamp, Vec<u32>)>>(),
            vec![]
        );
    }
}
