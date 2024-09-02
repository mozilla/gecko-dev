use core::ffi::c_int;
use core::{
    alloc::Layout,
    ffi::{c_uint, c_void},
    marker::PhantomData,
    mem::MaybeUninit,
};

#[cfg(feature = "rust-allocator")]
use alloc::alloc::GlobalAlloc;

#[allow(non_camel_case_types)]
type size_t = usize;

/// # Safety
///
/// This function is safe, but must have this type signature to be used elsewhere in the library
#[cfg(unix)]
unsafe extern "C" fn zalloc_c(opaque: *mut c_void, items: c_uint, size: c_uint) -> *mut c_void {
    let _ = opaque;

    extern "C" {
        fn posix_memalign(memptr: *mut *mut c_void, align: size_t, size: size_t) -> c_int;
    }

    let mut ptr = core::ptr::null_mut();
    match posix_memalign(&mut ptr, 64, items as size_t * size as size_t) {
        0 => ptr,
        _ => core::ptr::null_mut(),
    }
}

/// # Safety
///
/// This function is safe, but must have this type signature to be used elsewhere in the library
#[cfg(not(unix))]
unsafe extern "C" fn zalloc_c(opaque: *mut c_void, items: c_uint, size: c_uint) -> *mut c_void {
    let _ = opaque;

    extern "C" {
        fn malloc(size: size_t) -> *mut c_void;
    }

    malloc(items as size_t * size as size_t)
}

/// # Safety
///
/// The `ptr` must be allocated with the allocator that is used internally by `zcfree`
unsafe extern "C" fn zfree_c(opaque: *mut c_void, ptr: *mut c_void) {
    let _ = opaque;

    extern "C" {
        fn free(p: *mut c_void);
    }

    unsafe { free(ptr) }
}

/// # Safety
///
/// This function is safe to call.
#[cfg(feature = "rust-allocator")]
unsafe extern "C" fn zalloc_rust(_opaque: *mut c_void, count: c_uint, size: c_uint) -> *mut c_void {
    let align = 64;
    let size = count as usize * size as usize;

    // internally, we want to align allocations to 64 bytes (in part for SIMD reasons)
    let layout = Layout::from_size_align(size, align).unwrap();

    let ptr = std::alloc::System.alloc(layout);

    ptr as *mut c_void
}

/// # Safety
///
/// - `ptr` must be allocated with the rust `alloc::System` allocator
/// - `opaque` is a `&usize` that represents the size of the allocation
#[cfg(feature = "rust-allocator")]
unsafe extern "C" fn zfree_rust(opaque: *mut c_void, ptr: *mut c_void) {
    if ptr.is_null() {
        return;
    }

    // we can't really do much else. Deallocating with an invalid layout is UB.
    debug_assert!(!opaque.is_null());
    if opaque.is_null() {
        return;
    }

    let size = *(opaque as *mut usize);
    let align = 64;

    let layout = Layout::from_size_align(size, align);
    let layout = layout.unwrap();

    std::alloc::System.dealloc(ptr.cast(), layout);
}

#[derive(Clone, Copy)]
#[repr(C)]
pub struct Allocator<'a> {
    pub zalloc: crate::c_api::alloc_func,
    pub zfree: crate::c_api::free_func,
    pub opaque: crate::c_api::voidpf,
    pub _marker: PhantomData<&'a ()>,
}

impl Allocator<'static> {
    #[cfg(feature = "rust-allocator")]
    pub const RUST: Self = Self {
        zalloc: zalloc_rust,
        zfree: zfree_rust,
        opaque: core::ptr::null_mut(),
        _marker: PhantomData,
    };

    #[cfg(feature = "c-allocator")]
    pub const C: Self = Self {
        zalloc: zalloc_c,
        zfree: zfree_c,
        opaque: core::ptr::null_mut(),
        _marker: PhantomData,
    };
}

impl<'a> Allocator<'a> {
    pub fn allocate_layout(&self, layout: Layout) -> *mut c_void {
        // Special case for the Rust `alloc` backed allocator
        #[cfg(feature = "rust-allocator")]
        if self.zalloc == Allocator::RUST.zalloc {
            let ptr = unsafe { (Allocator::RUST.zalloc)(self.opaque, layout.size() as _, 1) };

            debug_assert_eq!(ptr as usize % layout.align(), 0);

            return ptr;
        }

        // General case for c-style allocation

        // We cannot rely on the allocator giving properly aligned allocations and have to fix that ourselves.
        //
        // The general approach is to allocate a bit more than the layout needs, so that we can
        // give the application a properly aligned address and also store the real allocation
        // pointer in the allocation so that `free` can free the real allocation pointer.
        //
        //
        // Example: The layout represents `(u32, u32)`, with an alignment of 4 bytes and a
        // total size of 8 bytes.
        //
        // Assume that the allocator will give us address `0x07`. We need that to be a multiple
        // of the alignment, so that shifts the starting position to `0x08`. Then we also need
        // to store the pointer to the start of the allocation so that `free` can free that
        // pointer, bumping to `0x10`. The `0x10` pointer is then the pointer that the application
        // deals with. When free'ing, the original allocation pointer can be read from `0x10 - size_of::<*const c_void>()`.
        //
        // Of course there does need to be enough space in the allocation such that when we
        // shift the start forwards, the end is still within the allocation. Hence we allocate
        // `extra_space` bytes: enough for a full alignment plus a pointer.

        // we need at least
        //
        // - `align` extra space so that no matter what pointer we get from zalloc, we can shift the start of the
        //      allocation by at most `align - 1` so that `ptr as usize % align == 0
        // - `size_of::<*mut _>` extra space so that after aligning to `align`,
        //      there is `size_of::<*mut _>` space to store the pointer to the allocation.
        //      This pointer is then retrieved in `free`
        let extra_space = core::mem::size_of::<*mut c_void>() + layout.align();

        // Safety: we assume allocating works correctly in the safety assumptions on
        // `DeflateStream` and `InflateStream`.
        let ptr = unsafe { (self.zalloc)(self.opaque, (layout.size() + extra_space) as _, 1) };

        if ptr.is_null() {
            return ptr;
        }

        // Calculate return pointer address with space enough to store original pointer
        let align_diff = (ptr as usize).next_multiple_of(layout.align()) - (ptr as usize);

        // Safety: offset is smaller than 64, and we allocated 64 extra bytes in the allocation
        let mut return_ptr = unsafe { ptr.cast::<u8>().add(align_diff) };

        // if there is not enough space to store a pointer we need to make more
        if align_diff < core::mem::size_of::<*mut c_void>() {
            // # Safety
            //
            // - `return_ptr` is well-aligned, therefore `return_ptr + align` is also well-aligned
            // - we reserve `size_of::<*mut _> + align` extra space in the allocation, so
            //      `ptr + align_diff + align` is still valid for (at least) `layout.size` bytes
            let offset = Ord::max(core::mem::size_of::<*mut c_void>(), layout.align());
            return_ptr = unsafe { return_ptr.add(offset) };
        }

        // Store the original pointer for free()
        //
        // Safety: `align >= size_of::<*mut _>`, so there is now space for a pointer before `return_ptr`
        // in the allocation
        unsafe {
            let original_ptr = return_ptr.sub(core::mem::size_of::<*mut c_void>());
            core::ptr::write_unaligned(original_ptr.cast::<*mut c_void>(), ptr);
        };

        // Return properly aligned pointer in allocation
        let ptr = return_ptr.cast::<c_void>();

        debug_assert_eq!(ptr as usize % layout.align(), 0);

        ptr
    }

    pub fn allocate<T>(&self) -> Option<&'a mut MaybeUninit<T>> {
        let ptr = self.allocate_layout(Layout::new::<T>());

        if ptr.is_null() {
            None
        } else {
            Some(unsafe { &mut *(ptr as *mut MaybeUninit<T>) })
        }
    }

    pub fn allocate_slice<T>(&self, len: usize) -> Option<&'a mut [MaybeUninit<T>]> {
        let ptr = self.allocate_layout(Layout::array::<T>(len).ok()?);

        if ptr.is_null() {
            None
        } else {
            Some(unsafe { core::slice::from_raw_parts_mut(ptr.cast(), len) })
        }
    }

    /// # Panics
    ///
    /// - when `len` is 0
    ///
    /// # Safety
    ///
    /// - `ptr` must be allocated with this allocator
    /// - `len` must be the number of `T`s that are in this allocation
    #[allow(unused)] // Rust needs `len` for deallocation
    pub unsafe fn deallocate<T>(&self, ptr: *mut T, len: usize) {
        if !ptr.is_null() {
            // Special case for the Rust `alloc` backed allocator
            #[cfg(feature = "rust-allocator")]
            if self.zfree == Allocator::RUST.zfree {
                assert_ne!(len, 0, "invalid size for {:?}", ptr);
                let mut size = core::mem::size_of::<T>() * len;
                return (Allocator::RUST.zfree)(&mut size as *mut usize as *mut c_void, ptr.cast());
            }

            // General case for c-style allocation
            let original_ptr = (ptr as *mut u8).sub(core::mem::size_of::<*const c_void>());
            let free_ptr = core::ptr::read_unaligned(original_ptr as *mut *mut c_void);

            (self.zfree)(self.opaque, free_ptr)
        }
    }
}

#[cfg(test)]
mod tests {
    use core::sync::atomic::{AtomicPtr, Ordering};
    use std::sync::Mutex;

    use super::*;

    static PTR: AtomicPtr<c_void> = AtomicPtr::new(core::ptr::null_mut());
    static MUTEX: Mutex<()> = Mutex::new(());

    unsafe extern "C" fn unaligned_alloc(
        _opaque: *mut c_void,
        _items: c_uint,
        _size: c_uint,
    ) -> *mut c_void {
        PTR.load(Ordering::Relaxed)
    }

    unsafe extern "C" fn unaligned_free(_opaque: *mut c_void, ptr: *mut c_void) {
        let expected = PTR.load(Ordering::Relaxed);
        assert_eq!(expected, ptr)
    }

    fn unaligned_allocator_help<T>() {
        let mut buf = [0u8; 1024];

        // we don't want anyone else messing with the PTR static
        let _guard = MUTEX.lock().unwrap();

        for i in 0..64 {
            let ptr = unsafe { buf.as_mut_ptr().add(i).cast() };
            PTR.store(ptr, Ordering::Relaxed);

            let allocator = Allocator {
                zalloc: unaligned_alloc,
                zfree: unaligned_free,
                opaque: core::ptr::null_mut(),
                _marker: PhantomData,
            };

            let ptr = allocator.allocate::<T>().unwrap();
            assert_eq!(ptr.as_ptr() as usize % core::mem::align_of::<T>(), 0);
            unsafe { allocator.deallocate(ptr, 1) }

            let ptr = allocator.allocate_slice::<T>(10).unwrap();
            assert_eq!(ptr.as_ptr() as usize % core::mem::align_of::<T>(), 0);
            unsafe { allocator.deallocate(ptr.as_mut_ptr(), 10) }
        }
    }

    #[test]
    fn unaligned_allocator_0() {
        unaligned_allocator_help::<()>()
    }

    #[test]
    fn unaligned_allocator_1() {
        unaligned_allocator_help::<u8>()
    }

    #[test]
    fn unaligned_allocator_2() {
        unaligned_allocator_help::<u16>()
    }
    #[test]
    fn unaligned_allocator_4() {
        unaligned_allocator_help::<u32>()
    }
    #[test]
    fn unaligned_allocator_8() {
        unaligned_allocator_help::<u64>()
    }
    #[test]
    fn unaligned_allocator_16() {
        unaligned_allocator_help::<u128>()
    }

    #[test]
    fn unaligned_allocator_32() {
        #[repr(C, align(32))]
        struct Align32(u8);

        unaligned_allocator_help::<Align32>()
    }

    #[test]
    fn unaligned_allocator_64() {
        #[repr(C, align(64))]
        struct Align64(u8);

        unaligned_allocator_help::<Align64>()
    }
}
