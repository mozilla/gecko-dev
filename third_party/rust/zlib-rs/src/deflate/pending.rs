use core::marker::PhantomData;

use crate::allocate::Allocator;

pub struct Pending<'a> {
    buf: *mut u8,
    out: *mut u8,
    pub(crate) pending: usize,
    end: *mut u8,
    _marker: PhantomData<&'a mut [u8]>,
}

impl<'a> Pending<'a> {
    pub fn reset_keep(&mut self) {
        // keep the buffer as it is
        self.pending = 0;
    }

    pub fn pending(&self) -> &[u8] {
        unsafe { core::slice::from_raw_parts(self.out, self.pending) }
    }

    pub(crate) fn remaining(&self) -> usize {
        self.end as usize - self.out as usize
    }

    pub(crate) fn capacity(&self) -> usize {
        self.end as usize - self.buf as usize
    }

    #[inline(always)]
    #[track_caller]
    pub fn advance(&mut self, n: usize) {
        assert!(n <= self.remaining(), "advancing past the end");
        debug_assert!(self.pending >= n);

        self.out = self.out.wrapping_add(n);
        self.pending -= n;

        if self.pending == 0 {
            self.out = self.buf;
        }
    }

    #[inline(always)]
    #[track_caller]
    pub fn rewind(&mut self, n: usize) {
        assert!(n <= self.pending, "rewinding past then start");

        self.pending -= n;
    }

    #[inline(always)]
    #[track_caller]
    pub fn extend(&mut self, buf: &[u8]) {
        assert!(
            self.remaining() >= buf.len(),
            "buf.len() must fit in remaining()"
        );

        unsafe {
            core::ptr::copy_nonoverlapping(buf.as_ptr(), self.out.add(self.pending), buf.len());
        }

        self.pending += buf.len();
    }

    pub(crate) fn new_in(alloc: &Allocator<'a>, len: usize) -> Option<Self> {
        let range = alloc.allocate_slice::<u8>(len)?.as_mut_ptr_range();

        Some(Self {
            buf: range.start as *mut u8,
            out: range.start as *mut u8,
            end: range.end as *mut u8,
            pending: 0,
            _marker: PhantomData,
        })
    }

    pub(crate) fn clone_in(&self, alloc: &Allocator<'a>) -> Option<Self> {
        let len = self.end as usize - self.buf as usize;
        let mut clone = Self::new_in(alloc, len)?;

        unsafe { core::ptr::copy_nonoverlapping(self.buf, clone.buf, len) };
        clone.out = unsafe { clone.buf.add(self.out as usize - self.buf as usize) };
        clone.pending = self.pending;

        Some(clone)
    }

    pub(crate) unsafe fn drop_in(&self, alloc: &Allocator) {
        let len = self.end as usize - self.buf as usize;
        alloc.deallocate(self.buf, len);
    }
}
