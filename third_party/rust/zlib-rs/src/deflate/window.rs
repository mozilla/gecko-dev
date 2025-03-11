use crate::{allocate::Allocator, weak_slice::WeakSliceMut};

#[derive(Debug)]
pub struct Window<'a> {
    // the full window allocation. This is longer than w_size so that operations don't need to
    // perform bounds checks.
    buf: WeakSliceMut<'a, u8>,

    window_bits: usize,
}

impl<'a> Window<'a> {
    pub fn new_in(alloc: &Allocator<'a>, window_bits: usize) -> Option<Self> {
        let len = 2 * ((1 << window_bits) + Self::padding());
        let ptr = alloc.allocate_zeroed(len)?;
        // SAFETY: freshly allocated buffer
        let buf = unsafe { WeakSliceMut::from_raw_parts_mut(ptr.as_ptr(), len) };

        Some(Self { buf, window_bits })
    }

    pub fn clone_in(&self, alloc: &Allocator<'a>) -> Option<Self> {
        let mut clone = Self::new_in(alloc, self.window_bits)?;

        clone
            .buf
            .as_mut_slice()
            .copy_from_slice(self.buf.as_slice());

        Some(clone)
    }

    /// # Safety
    ///
    /// [`Self`] must not be used after calling this function.
    pub unsafe fn drop_in(&mut self, alloc: &Allocator) {
        if !self.buf.is_empty() {
            let mut buf = core::mem::replace(&mut self.buf, WeakSliceMut::empty());
            unsafe { alloc.deallocate(buf.as_mut_ptr(), buf.len()) };
        }
    }

    pub fn capacity(&self) -> usize {
        2 * (1 << self.window_bits)
    }

    /// Returns a shared reference to the filled portion of the buffer.
    #[inline]
    pub fn filled(&self) -> &[u8] {
        // SAFETY: `self.buf` has been initialized for at least `filled` elements
        unsafe { core::slice::from_raw_parts(self.buf.as_ptr().cast(), self.buf.len()) }
    }

    /// Returns a mutable reference to the filled portion of the buffer.
    #[inline]
    pub fn filled_mut(&mut self) -> &mut [u8] {
        // SAFETY: `self.buf` has been initialized for at least `filled` elements
        unsafe { core::slice::from_raw_parts_mut(self.buf.as_mut_ptr().cast(), self.buf.len()) }
    }

    /// # Safety
    ///
    /// `src` must point to `range.end - range.start` valid (initialized!) bytes
    pub unsafe fn copy_and_initialize(&mut self, range: core::ops::Range<usize>, src: *const u8) {
        let (start, end) = (range.start, range.end);

        let dst = self.buf.as_mut_slice()[range].as_mut_ptr();
        unsafe { core::ptr::copy_nonoverlapping(src, dst, end - start) };
    }

    // padding required so that SIMD operations going out-of-bounds are not a problem
    pub fn padding() -> usize {
        if crate::cpu_features::is_enabled_pclmulqdq() {
            8
        } else {
            0
        }
    }
}
