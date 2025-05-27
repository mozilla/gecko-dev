//! Implement Fallible Box
use super::TryClone;
use crate::TryReserveError;
use alloc::boxed::Box;
use core::borrow::Borrow;
use core::mem::ManuallyDrop;
use core::ops::Deref;

/// trait to implement Fallible Box
pub trait FallibleBox<T> {
    /// try creating a new box, returning a Result<Box<T>,
    /// TryReserveError> if allocation failed
    fn try_new(t: T) -> Result<Self, TryReserveError>
    where
        Self: Sized;
}
/// TryBox is a thin wrapper around alloc::boxed::Box to provide support for
/// fallible allocation.
///
/// See the crate documentation for more.
pub struct TryBox<T> {
    inner: Box<T>,
}

impl<T> TryBox<T> {
    #[inline]
    pub fn try_new(t: T) -> Result<Self, TryReserveError> {
        Ok(Self {
            inner: <Box<T> as FallibleBox<T>>::try_new(t)?,
        })
    }

    #[inline(always)]
    pub fn into_raw(b: TryBox<T>) -> *mut T {
        Box::into_raw(b.inner)
    }

    /// # Safety
    ///
    /// See std::boxed::from_raw
    #[inline(always)]
    pub unsafe fn from_raw(raw: *mut T) -> Self {
        Self {
            inner: Box::from_raw(raw),
        }
    }
}

impl<T: TryClone> TryClone for TryBox<T> {
    fn try_clone(&self) -> Result<Self, TryReserveError> {
        let clone: T = (*self.inner).try_clone()?;
        Self::try_new(clone)
    }
}

impl<T> Deref for TryBox<T> {
    type Target = T;

    #[inline(always)]
    fn deref(&self) -> &T {
        self.inner.deref()
    }
}

impl<T> FallibleBox<T> for Box<T> {
    fn try_new(t: T) -> Result<Self, TryReserveError> {
        let mut vec = alloc::vec::Vec::new();
        vec.try_reserve_exact(1)?;
        vec.push(t);
        // try_reserve_exact doesn't promise the exact size, but into_boxed_slice does.
        // in practice the size is going to be okay anyway, so it won't realloc.
        let ptr: *mut T = ManuallyDrop::new(vec.into_boxed_slice()).as_mut_ptr();
        Ok(unsafe { Box::from_raw(ptr) })
    }
}

impl<T: TryClone> TryClone for Box<T> {
    #[inline]
    fn try_clone(&self) -> Result<Self, TryReserveError> {
        <Self as FallibleBox<T>>::try_new(Borrow::<T>::borrow(self).try_clone()?)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn boxed() {
        let mut v = <Box<_> as FallibleBox<_>>::try_new(5).unwrap();
        assert_eq!(*v, 5);
        *v = 3;
        assert_eq!(*v, 3);
    }
    // #[test]
    // fn big_alloc() {
    //     let layout = Layout::from_size_align(1_000_000_000_000, 8).unwrap();
    //     let ptr = unsafe { alloc::alloc::alloc(layout) };
    //     assert!(ptr.is_null());
    // }

    #[test]
    fn trybox_zst() {
        let b = <Box<_> as FallibleBox<_>>::try_new(()).expect("ok");
        assert_eq!(b, Box::new(()));
    }

    struct NonCopyType;

    #[test]
    fn trybox_deref() {
        let try_box: TryBox<NonCopyType> = TryBox::try_new(NonCopyType {}).unwrap();
        let _derefed: &NonCopyType = try_box.deref();
    }

    #[test]
    fn trybox_as_deref() {
        let try_box_option: Option<TryBox<NonCopyType>> =
            Some(TryBox::try_new(NonCopyType).unwrap());
        let _ref_option: Option<&NonCopyType> = try_box_option.as_deref();
    }
}
