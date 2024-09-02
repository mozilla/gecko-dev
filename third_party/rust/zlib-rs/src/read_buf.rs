// taken from https://docs.rs/tokio/latest/src/tokio/io/read_buf.rs.html#23-27
// based on https://rust-lang.github.io/rfcs/2930-read-buf.html
use core::fmt;
use core::mem::MaybeUninit;

use crate::allocate::Allocator;

/// A wrapper around a byte buffer that is incrementally filled and initialized.
///
/// This type is a sort of "double cursor". It tracks three regions in the
/// buffer: a region at the beginning of the buffer that has been logically
/// filled with data, a region that has been initialized at some point but not
/// yet logically filled, and a region at the end that may be uninitialized.
/// The filled region is guaranteed to be a subset of the initialized region.
///
/// In summary, the contents of the buffer can be visualized as:
///
/// ```not_rust
/// [             capacity              ]
/// [ filled |         unfilled         ]
/// [    initialized    | uninitialized ]
/// ```
///
/// It is undefined behavior to de-initialize any bytes from the uninitialized
/// region, since it is merely unknown whether this region is uninitialized or
/// not, and if part of it turns out to be initialized, it must stay initialized.
pub struct ReadBuf<'a> {
    buf: &'a mut [MaybeUninit<u8>],
    filled: usize,
    initialized: usize,
}

impl<'a> ReadBuf<'a> {
    /// Creates a new `ReadBuf` from a fully initialized buffer.
    #[inline]
    pub fn new(buf: &'a mut [u8]) -> ReadBuf<'a> {
        let initialized = buf.len();
        let buf = unsafe { slice_to_uninit_mut(buf) };
        ReadBuf {
            buf,
            filled: 0,
            initialized,
        }
    }

    /// Pointer to where the next byte will be written
    #[inline]
    pub fn next_out(&mut self) -> *mut MaybeUninit<u8> {
        self.buf[self.filled..].as_mut_ptr()
    }

    /// Pointer to the start of the `ReadBuf`
    #[inline]
    pub fn as_mut_ptr(&mut self) -> *mut MaybeUninit<u8> {
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
        let slice = &self.buf[..self.filled];
        // safety: filled describes how far into the buffer that the
        // user has filled with bytes, so it's been initialized.
        unsafe { slice_assume_init(slice) }
    }

    /// Returns a mutable reference to the entire buffer, without ensuring that it has been fully
    /// initialized.
    ///
    /// The elements between 0 and `self.len()` are filled, and those between 0 and
    /// `self.initialized().len()` are initialized (and so can be converted to a `&mut [u8]`).
    ///
    /// The caller of this method must ensure that these invariants are upheld. For example, if the
    /// caller initializes some of the uninitialized section of the buffer, it must call
    /// [`assume_init`](Self::assume_init) with the number of bytes initialized.
    ///
    /// # Safety
    ///
    /// The caller must not de-initialize portions of the buffer that have already been initialized.
    /// This includes any bytes in the region marked as uninitialized by `ReadBuf`.
    #[inline]
    pub unsafe fn inner_mut(&mut self) -> &mut [MaybeUninit<u8>] {
        self.buf
    }

    /// Returns the number of bytes at the end of the slice that have not yet been filled.
    #[inline]
    pub fn remaining(&self) -> usize {
        self.capacity() - self.filled
    }

    /// Clears the buffer, resetting the filled region to empty.
    ///
    /// The number of initialized bytes is not changed, and the contents of the buffer are not modified.
    #[inline]
    pub fn clear(&mut self) {
        self.filled = 0;
    }

    /// Advances the size of the filled region of the buffer.
    ///
    /// The number of initialized bytes is not changed.
    ///
    /// # Panics
    ///
    /// Panics if the filled region of the buffer would become larger than the initialized region.
    #[inline]
    #[track_caller]
    pub fn advance(&mut self, n: usize) {
        let new = self.filled.checked_add(n).expect("filled overflow");
        self.set_filled(new);
    }

    /// Sets the size of the filled region of the buffer.
    ///
    /// The number of initialized bytes is not changed.
    ///
    /// Note that this can be used to *shrink* the filled region of the buffer in addition to growing it (for
    /// example, by a `AsyncRead` implementation that compresses data in-place).
    ///
    /// # Panics
    ///
    /// Panics if the filled region of the buffer would become larger than the initialized region.
    #[inline]
    #[track_caller]
    pub fn set_filled(&mut self, n: usize) {
        assert!(
            n <= self.initialized,
            "filled must not become larger than initialized"
        );
        self.filled = n;
    }

    /// Asserts that the first `n` unfilled bytes of the buffer are initialized.
    ///
    /// `ReadBuf` assumes that bytes are never de-initialized, so this method does nothing when called with fewer
    /// bytes than are already known to be initialized.
    ///
    /// # Safety
    ///
    /// The caller must ensure that `n` unfilled bytes of the buffer have already been initialized.
    #[inline]
    pub unsafe fn assume_init(&mut self, n: usize) {
        self.initialized = Ord::max(self.initialized, self.filled + n);
    }

    #[track_caller]
    pub fn push(&mut self, byte: u8) {
        assert!(
            self.remaining() >= 1,
            "read_buf is full ({} bytes)",
            self.capacity()
        );

        self.buf[self.filled] = MaybeUninit::new(byte);

        self.initialized = Ord::max(self.initialized, self.filled + 1);
        self.filled += 1;
    }

    /// Appends data to the buffer, advancing the written position and possibly also the initialized position.
    ///
    /// # Panics
    ///
    /// Panics if `self.remaining()` is less than `buf.len()`.
    #[inline(always)]
    #[track_caller]
    pub fn extend(&mut self, buf: &[u8]) {
        assert!(
            self.remaining() >= buf.len(),
            "buf.len() must fit in remaining()"
        );

        // using simd here (on x86_64) was not fruitful
        self.buf[self.filled..][..buf.len()].copy_from_slice(slice_to_uninit(buf));

        let end = self.filled + buf.len();
        self.initialized = Ord::max(self.initialized, end);
        self.filled = end;
    }

    #[inline(always)]
    pub fn copy_match(&mut self, offset_from_end: usize, length: usize) {
        #[cfg(target_arch = "x86_64")]
        if crate::cpu_features::is_enabled_avx512() {
            return self.copy_match_help::<core::arch::x86_64::__m512i>(offset_from_end, length);
        }

        #[cfg(target_arch = "x86_64")]
        if crate::cpu_features::is_enabled_avx2() {
            return self.copy_match_help::<core::arch::x86_64::__m256i>(offset_from_end, length);
        }

        #[cfg(target_arch = "x86_64")]
        if crate::cpu_features::is_enabled_sse() {
            return self.copy_match_help::<core::arch::x86_64::__m128i>(offset_from_end, length);
        }

        self.copy_match_help::<u64>(offset_from_end, length)
    }

    fn copy_match_help<C: Chunk>(&mut self, offset_from_end: usize, length: usize) {
        let current = self.filled;

        let start = current.checked_sub(offset_from_end).expect("in bounds");
        let end = start.checked_add(length).expect("in bounds");

        // Note also that the referenced string may overlap the current
        // position; for example, if the last 2 bytes decoded have values
        // X and Y, a string reference with <length = 5, distance = 2>
        // adds X,Y,X,Y,X to the output stream.

        if end > current {
            if offset_from_end == 1 {
                // this will just repeat this value many times
                let element = self.buf[current - 1];
                self.buf[current..][..length].fill(element);
            } else {
                for i in 0..length {
                    self.buf[current + i] = self.buf[start + i];
                }
            }
        } else {
            Self::copy_chunked_within::<C>(self.buf, current, start, end)
        }

        // safety: we just copied length initialized bytes right beyond self.filled
        unsafe { self.assume_init(length) };

        self.advance(length);
    }

    #[inline(always)]
    fn copy_chunked_within<C: Chunk>(
        buf: &mut [MaybeUninit<u8>],
        current: usize,
        start: usize,
        end: usize,
    ) {
        if (end - start).next_multiple_of(core::mem::size_of::<C>()) <= (buf.len() - current) {
            unsafe {
                Self::copy_chunk_unchecked::<C>(
                    buf.as_ptr().add(start),
                    buf.as_mut_ptr().add(current),
                    buf.as_ptr().add(end),
                )
            }
        } else {
            // a full simd copy does not fit in the output buffer
            buf.copy_within(start..end, current);
        }
    }

    /// # Safety
    ///
    /// `src` must be safe to perform unaligned reads in `core::mem::size_of::<C>()` chunks until
    /// `end` is reached. `dst` must be safe to (unalingned) write that number of chunks.
    #[inline(always)]
    unsafe fn copy_chunk_unchecked<C: Chunk>(
        mut src: *const MaybeUninit<u8>,
        mut dst: *mut MaybeUninit<u8>,
        end: *const MaybeUninit<u8>,
    ) {
        while src < end {
            let chunk = C::load_chunk(src);
            C::store_chunk(dst, chunk);

            src = src.add(core::mem::size_of::<C>());
            dst = dst.add(core::mem::size_of::<C>());
        }
    }

    pub(crate) fn new_in(alloc: &Allocator<'a>, len: usize) -> Option<Self> {
        let buf = alloc.allocate_slice::<u8>(len)?;

        Some(Self {
            buf,
            filled: 0,
            initialized: 0,
        })
    }

    pub(crate) fn clone_in(&self, alloc: &Allocator<'a>) -> Option<Self> {
        let mut clone = Self::new_in(alloc, self.buf.len())?;

        clone.buf.copy_from_slice(self.buf);
        clone.filled = self.filled;
        clone.initialized = self.initialized;

        Some(clone)
    }

    pub(crate) unsafe fn drop_in(&mut self, alloc: &Allocator<'a>) {
        if !self.buf.is_empty() {
            let buf = core::mem::take(&mut self.buf);
            alloc.deallocate(buf.as_mut_ptr(), buf.len());
        }
    }
}

impl fmt::Debug for ReadBuf<'_> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("ReadBuf")
            .field("filled", &self.filled)
            .field("initialized", &self.initialized)
            .field("capacity", &self.capacity())
            .finish()
    }
}

fn slice_to_uninit(slice: &[u8]) -> &[MaybeUninit<u8>] {
    unsafe { &*(slice as *const [u8] as *const [MaybeUninit<u8>]) }
}

unsafe fn slice_to_uninit_mut(slice: &mut [u8]) -> &mut [MaybeUninit<u8>] {
    &mut *(slice as *mut [u8] as *mut [MaybeUninit<u8>])
}

// TODO: This could use `MaybeUninit::slice_assume_init` when it is stable.
unsafe fn slice_assume_init(slice: &[MaybeUninit<u8>]) -> &[u8] {
    &*(slice as *const [MaybeUninit<u8>] as *const [u8])
}

trait Chunk {
    /// Safety: must be valid to read a `Self::Chunk` value from `from` with an unaligned read.
    unsafe fn load_chunk(from: *const MaybeUninit<u8>) -> Self;

    /// Safety: must be valid to write a `Self::Chunk` value to `out` with an unaligned write.
    unsafe fn store_chunk(out: *mut MaybeUninit<u8>, chunk: Self);
}

impl Chunk for u64 {
    unsafe fn load_chunk(from: *const MaybeUninit<u8>) -> Self {
        u64::to_le(core::ptr::read_unaligned(from.cast()))
    }

    unsafe fn store_chunk(out: *mut MaybeUninit<u8>, chunk: Self) {
        core::ptr::copy_nonoverlapping(
            chunk.to_le_bytes().as_ptr().cast(),
            out,
            core::mem::size_of::<Self>(),
        )
    }
}

#[cfg(target_arch = "x86_64")]
impl Chunk for core::arch::x86_64::__m128i {
    #[inline(always)]
    unsafe fn load_chunk(from: *const MaybeUninit<u8>) -> Self {
        core::arch::x86_64::_mm_loadu_si128(from.cast())
    }

    #[inline(always)]
    unsafe fn store_chunk(out: *mut MaybeUninit<u8>, chunk: Self) {
        core::arch::x86_64::_mm_storeu_si128(out as *mut Self, chunk);
    }
}

#[cfg(target_arch = "x86_64")]
impl Chunk for core::arch::x86_64::__m256i {
    #[inline(always)]
    unsafe fn load_chunk(from: *const MaybeUninit<u8>) -> Self {
        core::arch::x86_64::_mm256_loadu_si256(from.cast())
    }

    #[inline(always)]
    unsafe fn store_chunk(out: *mut MaybeUninit<u8>, chunk: Self) {
        core::arch::x86_64::_mm256_storeu_si256(out as *mut Self, chunk);
    }
}

#[cfg(target_arch = "x86_64")]
impl Chunk for core::arch::x86_64::__m512i {
    #[inline(always)]
    unsafe fn load_chunk(from: *const MaybeUninit<u8>) -> Self {
        // TODO AVX-512 is effectively unstable.
        // We cross our fingers that LLVM optimizes this into a vmovdqu32
        //
        // https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm512_loadu_si512&expand=3420&ig_expand=4110
        core::ptr::read_unaligned(from.cast())
    }

    #[inline(always)]
    unsafe fn store_chunk(out: *mut MaybeUninit<u8>, chunk: Self) {
        // TODO AVX-512 is effectively unstable.
        // We cross our fingers that LLVM optimizes this into a vmovdqu32
        //
        // https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm512_storeu_si512&expand=3420&ig_expand=4110,6550
        core::ptr::write_unaligned(out.cast(), chunk)
    }
}
