// taken from https://docs.rs/tokio/latest/src/tokio/io/read_buf.rs.html#23-27
// based on https://rust-lang.github.io/rfcs/2930-read-buf.html
use core::fmt;

use crate::allocate::Allocator;
use crate::weak_slice::WeakSliceMut;

pub struct ReadBuf<'a> {
    buf: WeakSliceMut<'a, u8>,
    filled: usize,
}

impl<'a> ReadBuf<'a> {
    /// Pointer to the start of the `ReadBuf`
    #[inline]
    pub fn as_mut_ptr(&mut self) -> *mut u8 {
        self.buf.as_mut_ptr()
    }

    /// Returns the total capacity of the buffer.
    #[inline]
    pub fn capacity(&self) -> usize {
        self.buf.len()
    }

    /// Returns the length of the filled part of the buffer
    #[inline]
    pub fn len(&self) -> usize {
        self.filled
    }

    /// Returns true if there are no bytes in this ReadBuf
    #[inline]
    pub fn is_empty(&self) -> bool {
        self.filled == 0
    }

    /// Returns a shared reference to the filled portion of the buffer.
    #[inline]
    pub fn filled(&self) -> &[u8] {
        &self.buf.as_slice()[..self.filled]
    }

    /// Clears the buffer, resetting the filled region to empty.
    ///
    /// The number of initialized bytes is not changed, and the contents of the buffer are not modified.
    #[inline]
    pub fn clear(&mut self) {
        self.buf.as_mut_slice().fill(0);
        self.filled = 0;
    }

    #[inline(always)]
    pub fn push_lit(&mut self, byte: u8) {
        // NOTE: we rely on the buffer being zeroed here!
        self.buf.as_mut_slice()[self.filled + 2] = byte;

        self.filled += 3;
    }

    #[inline(always)]
    pub fn push_dist(&mut self, dist: u16, len: u8) {
        let buf = &mut self.buf.as_mut_slice()[self.filled..][..3];
        let [dist1, dist2] = dist.to_le_bytes();

        buf[0] = dist1;
        buf[1] = dist2;
        buf[2] = len;

        self.filled += 3;
    }

    pub(crate) fn new_in(alloc: &Allocator<'a>, len: usize) -> Option<Self> {
        let ptr = alloc.allocate_zeroed(len);

        if ptr.is_null() {
            return None;
        }

        // safety: all elements are now initialized
        let buf = unsafe { WeakSliceMut::from_raw_parts_mut(ptr, len) };

        Some(Self { buf, filled: 0 })
    }

    pub(crate) fn clone_in(&self, alloc: &Allocator<'a>) -> Option<Self> {
        let mut clone = Self::new_in(alloc, self.buf.len())?;

        clone
            .buf
            .as_mut_slice()
            .copy_from_slice(self.buf.as_slice());
        clone.filled = self.filled;

        Some(clone)
    }

    pub(crate) unsafe fn drop_in(&mut self, alloc: &Allocator<'a>) {
        if !self.buf.is_empty() {
            let mut buf = core::mem::replace(&mut self.buf, WeakSliceMut::empty());
            alloc.deallocate(buf.as_mut_ptr(), buf.len());
        }
    }
}

impl fmt::Debug for ReadBuf<'_> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("ReadBuf")
            .field("filled", &self.filled)
            .field("capacity", &self.capacity())
            .finish()
    }
}
