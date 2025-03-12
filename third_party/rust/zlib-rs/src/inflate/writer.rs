use core::fmt;
use core::mem::MaybeUninit;
use core::ops::Range;

use crate::weak_slice::WeakSliceMut;

pub struct Writer<'a> {
    buf: WeakSliceMut<'a, MaybeUninit<u8>>,
    filled: usize,
}

impl<'a> Writer<'a> {
    /// Creates a new `Writer` from a fully initialized buffer.
    #[inline]
    pub fn new(buf: &'a mut [u8]) -> Writer<'a> {
        unsafe { Self::new_uninit(buf.as_mut_ptr(), buf.len()) }
    }

    /// Creates a new `Writer` from an uninitialized buffer.
    #[inline]
    pub unsafe fn new_uninit(ptr: *mut u8, len: usize) -> Writer<'a> {
        let buf = unsafe { WeakSliceMut::from_raw_parts_mut(ptr as *mut MaybeUninit<u8>, len) };
        Writer { buf, filled: 0 }
    }

    /// Pointer to where the next byte will be written
    #[inline]
    pub fn next_out(&mut self) -> *mut MaybeUninit<u8> {
        self.buf.as_mut_ptr().wrapping_add(self.filled).cast()
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

    /// Returns a shared reference to the filled portion of the buffer.
    #[inline]
    pub fn filled(&self) -> &[u8] {
        // SAFETY: the filled area of the buffer is always initialized, and self.filled is always
        // in-bounds.
        unsafe { core::slice::from_raw_parts(self.buf.as_ptr().cast(), self.filled) }
    }

    /// Returns the number of bytes at the end of the slice that have not yet been filled.
    #[inline]
    pub fn remaining(&self) -> usize {
        self.capacity() - self.filled
    }

    #[inline]
    pub fn is_full(&self) -> bool {
        self.filled == self.buf.len()
    }

    pub fn push(&mut self, byte: u8) {
        self.buf.as_mut_slice()[self.filled] = MaybeUninit::new(byte);

        self.filled += 1;
    }

    /// Appends data to the buffer
    #[inline(always)]
    pub fn extend(&mut self, buf: &[u8]) {
        // using simd here (on x86_64) was not fruitful
        self.buf.as_mut_slice()[self.filled..][..buf.len()].copy_from_slice(slice_to_uninit(buf));

        self.filled += buf.len();
    }

    #[inline(always)]
    pub fn extend_from_window(&mut self, window: &super::window::Window, range: Range<usize>) {
        #[cfg(target_arch = "x86_64")]
        if crate::cpu_features::is_enabled_avx512() {
            return self.extend_from_window_help::<core::arch::x86_64::__m512i>(window, range);
        }

        #[cfg(target_arch = "x86_64")]
        if crate::cpu_features::is_enabled_avx2() {
            return self.extend_from_window_help::<core::arch::x86_64::__m256i>(window, range);
        }

        #[cfg(target_arch = "x86_64")]
        if crate::cpu_features::is_enabled_sse() {
            return self.extend_from_window_help::<core::arch::x86_64::__m128i>(window, range);
        }

        #[cfg(target_arch = "aarch64")]
        if crate::cpu_features::is_enabled_neon() {
            return self.extend_from_window_help::<core::arch::aarch64::uint8x16_t>(window, range);
        }

        #[cfg(target_arch = "wasm32")]
        if crate::cpu_features::is_enabled_simd128() {
            return self.extend_from_window_help::<core::arch::wasm32::v128>(window, range);
        }

        self.extend_from_window_help::<u64>(window, range)
    }

    #[inline(always)]
    fn extend_from_window_help<C: Chunk>(
        &mut self,
        window: &super::window::Window,
        range: Range<usize>,
    ) {
        let len = range.end - range.start;

        if self.remaining() >= len + core::mem::size_of::<C>() {
            // SAFETY: we know that our window has at least a core::mem::size_of::<C>() extra bytes
            // at the end, making it always safe to perform an (unaligned) Chunk read anywhere in
            // the window slice.
            //
            // The calling function checks for CPU features requirements for C.
            unsafe {
                let src = window.as_ptr();
                Self::copy_chunk_unchecked::<C>(
                    src.wrapping_add(range.start).cast(),
                    self.next_out(),
                    len,
                )
            }
        } else {
            let buf = &window.as_slice()[range];
            self.buf.as_mut_slice()[self.filled..][..buf.len()]
                .copy_from_slice(slice_to_uninit(buf));
        }

        self.filled += len;
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

        #[cfg(target_arch = "aarch64")]
        if crate::cpu_features::is_enabled_neon() {
            return self
                .copy_match_help::<core::arch::aarch64::uint8x16_t>(offset_from_end, length);
        }

        #[cfg(target_arch = "wasm32")]
        if crate::cpu_features::is_enabled_simd128() {
            return self.copy_match_help::<core::arch::wasm32::v128>(offset_from_end, length);
        }

        self.copy_match_help::<u64>(offset_from_end, length)
    }

    #[inline(always)]
    fn copy_match_help<C: Chunk>(&mut self, offset_from_end: usize, length: usize) {
        let capacity = self.buf.len();
        let len = Ord::min(self.filled + length + core::mem::size_of::<C>(), capacity);
        let buf = &mut self.buf.as_mut_slice()[..len];

        let current = self.filled;
        self.filled += length;

        // Note also that the referenced string may overlap the current
        // position; for example, if the last 2 bytes decoded have values
        // X and Y, a string reference with <length = 5, distance = 2>
        // adds X,Y,X,Y,X to the output stream.

        if length > offset_from_end {
            match offset_from_end {
                1 => {
                    // this will just repeat this value many times
                    let element = buf[current - 1];
                    buf[current..][..length].fill(element);
                }
                _ => {
                    // there is a SIMD implementation of this logic, which _should_ be faster, but
                    // isn't in measurements on x86_64. It still might be for other architectures,
                    // adds a lot of complexity and unsafe code.
                    for i in 0..length {
                        buf[current + i] = buf[current - offset_from_end + i];
                    }
                }
            }
        } else {
            Self::copy_chunked_within::<C>(buf, capacity, current, offset_from_end, length)
        }
    }

    #[inline(always)]
    fn copy_chunked_within<C: Chunk>(
        buf: &mut [MaybeUninit<u8>],
        capacity: usize,
        current: usize,
        offset_from_end: usize,
        length: usize,
    ) {
        let start = current.checked_sub(offset_from_end).expect("in bounds");

        if current + length + core::mem::size_of::<C>() < capacity {
            let ptr = buf.as_mut_ptr();
            // SAFETY: if statement and checked_sub ensures we stay in bounds.
            unsafe { Self::copy_chunk_unchecked::<C>(ptr.add(start), ptr.add(current), length) }
        } else {
            // a full simd copy does not fit in the output buffer
            buf.copy_within(start..start + length, current);
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
        length: usize,
    ) {
        let end = src.add(length);

        let chunk = C::load_chunk(src);
        C::store_chunk(dst, chunk);

        src = src.add(core::mem::size_of::<C>());
        dst = dst.add(core::mem::size_of::<C>());

        while src < end {
            let chunk = C::load_chunk(src);
            C::store_chunk(dst, chunk);

            src = src.add(core::mem::size_of::<C>());
            dst = dst.add(core::mem::size_of::<C>());
        }
    }
}

impl fmt::Debug for Writer<'_> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("Writer")
            .field("filled", &self.filled)
            .field("capacity", &self.capacity())
            .finish()
    }
}

fn slice_to_uninit(slice: &[u8]) -> &[MaybeUninit<u8>] {
    unsafe { &*(slice as *const [u8] as *const [MaybeUninit<u8>]) }
}

trait Chunk {
    /// # Safety
    ///
    /// Must be valid to read a `Self::Chunk` value from `from` with an unaligned read.
    ///
    /// Implementations may have CPU feature specific requirements depending on the type.
    unsafe fn load_chunk(from: *const MaybeUninit<u8>) -> Self;

    /// # Safety
    ///
    /// Must be valid to write a `Self::Chunk` value to `out` with an unaligned write.
    ///
    /// Implementations may have CPU feature specific requirements depending on the type.
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

#[cfg(target_arch = "aarch64")]
impl Chunk for core::arch::aarch64::uint8x16_t {
    #[inline(always)]
    unsafe fn load_chunk(from: *const MaybeUninit<u8>) -> Self {
        core::arch::aarch64::vld1q_u8(from.cast())
    }

    #[inline(always)]
    unsafe fn store_chunk(out: *mut MaybeUninit<u8>, chunk: Self) {
        core::arch::aarch64::vst1q_u8(out.cast(), chunk)
    }
}

#[cfg(target_arch = "wasm32")]
impl Chunk for core::arch::wasm32::v128 {
    #[inline(always)]
    unsafe fn load_chunk(from: *const MaybeUninit<u8>) -> Self {
        core::arch::wasm32::v128_load(from.cast())
    }

    #[inline(always)]
    unsafe fn store_chunk(out: *mut MaybeUninit<u8>, chunk: Self) {
        core::arch::wasm32::v128_store(out as *mut Self, chunk)
    }
}

#[cfg(test)]
mod test {
    use super::*;

    const N: usize = 128;
    const M: usize = 64;

    fn test_array() -> [MaybeUninit<u8>; N] {
        core::array::from_fn(|i| MaybeUninit::new(if i < M { i as u8 } else { 0xAAu8 }))
    }

    fn test_copy_match(offset_from_end: usize, length: usize) {
        let mut buf = test_array();
        let mut writer = Writer {
            buf: unsafe { WeakSliceMut::from_raw_parts_mut(buf.as_mut_ptr(), buf.len()) },
            filled: M,
        };
        writer.copy_match(offset_from_end, length);
        assert_eq!(writer.filled, M + length);

        let mut naive = test_array();
        for i in 0..length {
            naive[M + i] = naive[M - offset_from_end + i];
        }

        let buf = unsafe { core::mem::transmute::<[MaybeUninit<u8>; 128], [u8; N]>(buf) };
        let naive = unsafe { core::mem::transmute::<[MaybeUninit<u8>; 128], [u8; N]>(naive) };
        assert_eq!(
            buf[M..][..length],
            naive[M..][..length],
            "{} {}",
            offset_from_end,
            length
        );
    }

    #[test]
    fn copy_chunk_unchecked() {
        let offset_from_end = 17;
        let length = 17;

        #[cfg(target_arch = "x86_64")]
        use core::arch::x86_64::{__m128i, __m256i, __m512i};

        #[cfg(target_arch = "aarch64")]
        use core::arch::aarch64::uint8x16_t;

        #[cfg(target_arch = "wasm32")]
        use core::arch::wasm32::v128;

        macro_rules! helper {
            ($func:expr) => {
                let mut buf = test_array();
                let mut writer = Writer {
                    buf: unsafe { WeakSliceMut::from_raw_parts_mut(buf.as_mut_ptr(), buf.len()) },
                    filled: M,
                };

                $func(&mut writer, offset_from_end, length);
            };
        }

        #[cfg(target_arch = "x86_64")]
        if crate::cpu_features::is_enabled_avx512() {
            helper!(Writer::copy_match_help::<__m512i>);
        }

        #[cfg(target_arch = "x86_64")]
        if crate::cpu_features::is_enabled_avx2() {
            helper!(Writer::copy_match_help::<__m256i>);
        }

        #[cfg(target_arch = "x86_64")]
        if crate::cpu_features::is_enabled_sse() {
            helper!(Writer::copy_match_help::<__m128i>);
        }

        #[cfg(target_arch = "aarch64")]
        if crate::cpu_features::is_enabled_neon() {
            helper!(Writer::copy_match_help::<uint8x16_t>);
        }

        #[cfg(target_arch = "wasm32")]
        if crate::cpu_features::is_enabled_simd128() {
            helper!(Writer::copy_match_help::<v128>);
        }

        helper!(Writer::copy_match_help::<u64>);
    }

    #[test]
    fn copy_match() {
        for offset_from_end in 1..=64 {
            for length in 0..=64 {
                test_copy_match(offset_from_end, length)
            }
        }
    }

    #[test]
    fn copy_match_insufficient_space_for_simd() {
        let mut buf = [1, 2, 3, 0xAA, 0xAA].map(MaybeUninit::new);
        let mut writer = Writer {
            buf: unsafe { WeakSliceMut::from_raw_parts_mut(buf.as_mut_ptr(), buf.len()) },
            filled: 3,
        };

        writer.copy_match(3, 2);

        assert_eq!(buf.map(|e| unsafe { e.assume_init() }), [1, 2, 3, 1, 2]);
    }
}
