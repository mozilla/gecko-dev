use crate::allocate::Allocator;
use core::mem::MaybeUninit;

#[derive(Debug)]
pub struct Window<'a> {
    // the full window allocation. This is longer than w_size so that operations don't need to
    // perform bounds checks.
    buf: &'a mut [MaybeUninit<u8>],

    // number of initialized bytes
    filled: usize,

    window_bits: usize,

    high_water: usize,
}

impl<'a> Window<'a> {
    pub fn new_in(alloc: &Allocator<'a>, window_bits: usize) -> Option<Self> {
        let buf = alloc.allocate_slice::<u8>(2 * ((1 << window_bits) + Self::padding()))?;

        Some(Self {
            buf,
            filled: 0,
            window_bits,
            high_water: 0,
        })
    }

    pub fn clone_in(&self, alloc: &Allocator<'a>) -> Option<Self> {
        let mut clone = Self::new_in(alloc, self.window_bits)?;

        clone.buf.copy_from_slice(self.buf);
        clone.filled = self.filled;
        clone.high_water = self.high_water;

        Some(clone)
    }

    pub unsafe fn drop_in(&mut self, alloc: &Allocator) {
        if !self.buf.is_empty() {
            let buf = core::mem::take(&mut self.buf);
            alloc.deallocate(buf.as_mut_ptr(), buf.len());
        }
    }

    pub fn capacity(&self) -> usize {
        2 * (1 << self.window_bits)
    }

    /// Returns a shared reference to the filled portion of the buffer.
    #[inline]
    pub fn filled(&self) -> &[u8] {
        // safety: `self.buf` has been initialized for at least `filled` elements
        unsafe { core::slice::from_raw_parts(self.buf.as_ptr().cast(), self.filled) }
    }

    /// Returns a mutable reference to the filled portion of the buffer.
    #[inline]
    pub fn filled_mut(&mut self) -> &mut [u8] {
        // safety: `self.buf` has been initialized for at least `filled` elements
        unsafe { core::slice::from_raw_parts_mut(self.buf.as_mut_ptr().cast(), self.filled) }
    }

    /// # Safety
    ///
    /// `src` must point to `range.end - range.start` valid (initialized!) bytes
    pub unsafe fn copy_and_initialize(&mut self, range: core::ops::Range<usize>, src: *const u8) {
        let (start, end) = (range.start, range.end);

        let dst = self.buf[range].as_mut_ptr() as *mut u8;
        core::ptr::copy_nonoverlapping(src, dst, end - start);

        if start >= self.filled {
            self.filled = Ord::max(self.filled, end);
        }

        self.high_water = Ord::max(self.high_water, self.filled);
    }

    // this library has many functions that operated in a chunked fashion on memory. For
    // performance, we want to minimize bounds checks. Therefore we reserve initialize some extra
    // memory at the end of the window so that chunked operations can use the whole buffer. If they
    // go slightly over `self.capacity` that's okay, we account for that here by making sure the
    // memory there is initialized!
    pub fn initialize_out_of_bounds(&mut self) {
        const WIN_INIT: usize = crate::deflate::STD_MAX_MATCH;

        // If the WIN_INIT bytes after the end of the current data have never been
        // written, then zero those bytes in order to avoid memory check reports of
        // the use of uninitialized (or uninitialised as Julian writes) bytes by
        // the longest match routines.  Update the high water mark for the next
        // time through here.  WIN_INIT is set to STD_MAX_MATCH since the longest match
        // routines allow scanning to strstart + STD_MAX_MATCH, ignoring lookahead.
        if self.high_water < self.capacity() {
            let curr = self.filled().len();

            if self.high_water < curr {
                // Previous high water mark below current data -- zero WIN_INIT
                // bytes or up to end of window, whichever is less.
                let init = Ord::min(self.capacity() - curr, WIN_INIT);

                self.buf[curr..][..init].fill(MaybeUninit::new(0));

                self.high_water = curr + init;

                self.filled += init;
            } else if self.high_water < curr + WIN_INIT {
                // High water mark at or above current data, but below current data
                // plus WIN_INIT -- zero out to current data plus WIN_INIT, or up
                // to end of window, whichever is less.
                let init = Ord::min(
                    curr + WIN_INIT - self.high_water,
                    self.capacity() - self.high_water,
                );

                self.buf[self.high_water..][..init].fill(MaybeUninit::new(0));

                self.high_water += init;
                self.filled += init;
            }
        }
    }

    pub fn initialize_at_least(&mut self, at_least: usize) {
        let end = at_least.clamp(self.high_water, self.buf.len());
        self.buf[self.high_water..end].fill(MaybeUninit::new(0));

        self.high_water = end;
        self.filled = end;
    }

    // padding required so that SIMD operations going out-of-bounds are not a problem
    pub fn padding() -> usize {
        #[cfg(feature = "std")]
        #[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
        if std::is_x86_feature_detected!("pclmulqdq")
            && std::is_x86_feature_detected!("sse2")
            && std::is_x86_feature_detected!("sse4.1")
        {
            return 8;
        }

        0
    }
}
