#![deny(unsafe_op_in_unsafe_fn)]
#![allow(clippy::needless_doctest_main, clippy::partialeq_ne_impl)]

#[cfg(feature = "alloc")]
pub use self::slice::SliceExt;

pub mod alloc;

#[cfg(feature = "alloc")]
pub(crate) mod boxed;

#[cfg(feature = "alloc")]
mod raw_vec;

#[cfg(feature = "alloc")]
pub mod vec;

#[cfg(feature = "alloc")]
mod macros;

#[cfg(feature = "alloc")]
mod slice;

#[cfg(feature = "alloc")]
#[track_caller]
#[inline(always)]
#[cfg(debug_assertions)]
unsafe fn assume(v: bool) {
    if !v {
        core::unreachable!()
    }
}

#[cfg(feature = "alloc")]
#[track_caller]
#[inline(always)]
#[cfg(not(debug_assertions))]
unsafe fn assume(v: bool) {
    if !v {
        unsafe {
            core::hint::unreachable_unchecked();
        }
    }
}

#[cfg(feature = "alloc")]
#[inline(always)]
fn addr<T>(x: *const T) -> usize {
    #[allow(clippy::useless_transmute, clippy::transmutes_expressible_as_ptr_casts)]
    unsafe {
        core::mem::transmute(x)
    }
}

#[cfg(feature = "alloc")]
#[inline(always)]
fn invalid_mut<T>(addr: usize) -> *mut T {
    #[allow(clippy::useless_transmute, clippy::transmutes_expressible_as_ptr_casts)]
    unsafe {
        core::mem::transmute(addr)
    }
}
