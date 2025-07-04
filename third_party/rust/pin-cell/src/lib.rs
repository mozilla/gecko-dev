#![no_std]
#![deny(warnings, missing_docs, missing_debug_implementations)]
//! This library defines the `PinCell` type, a pinning variant of the standard
//! library's `RefCell`.
//!
//! It is not safe to "pin project" through a `RefCell` - getting a pinned
//! reference to something inside the `RefCell` when you have a pinned
//! refernece to the `RefCell` - because `RefCell` is too powerful.
//!
//! A `PinCell` is slightly less powerful than `RefCell`: unlike a `RefCell`,
//! one cannot get a mutable reference into a `PinCell`, only a pinned mutable
//! reference (`Pin<&mut T>`). This makes pin projection safe, allowing you
//! to use interior mutability with the knowledge that `T` will never actually
//! be moved out of the `RefCell` that wraps it.

mod pin_mut;
mod pin_ref;

use core::cell::{BorrowError, BorrowMutError, Ref, RefCell, RefMut};
use core::pin::Pin;

pub use crate::pin_mut::PinMut;
pub use crate::pin_ref::PinRef;

/// A mutable memory location with dynamically checked borrow rules
///
/// Unlike `RefCell`, this type only allows *pinned* mutable access to the
/// inner value, enabling a "pin-safe" version of interior mutability.
///
/// See the standard library documentation for more information.
#[derive(Default, Clone, Ord, PartialOrd, Eq, PartialEq, Debug)]
pub struct PinCell<T: ?Sized> {
    inner: RefCell<T>,
}

impl<T> PinCell<T> {
    /// Creates a new `PinCell` containing `value`.
    pub const fn new(value: T) -> PinCell<T> {
        PinCell {
            inner: RefCell::new(value),
        }
    }
}

impl<T: ?Sized> PinCell<T> {
    /// Immutably borrows the wrapped value.
    ///
    /// The borrow lasts until the returned `Ref` exits scope. Multiple
    /// immutable borrows can be taken out at the same time (both pinned and
    /// unpinned).
    pub fn borrow(&self) -> Ref<'_, T> {
        self.inner.borrow()
    }

    /// Immutably borrows the wrapped value, returning an error if the value is
    /// currently mutably borrowed.
    ///
    /// The borrow lasts until the returned `Ref` exits scope. Multiple
    /// immutable borrows can be taken out at the same time (both pinned and
    /// unpinned).
    ///
    /// This is the non-panicking variant of `borrow`.
    pub fn try_borrow(&self) -> Result<Ref<'_, T>, BorrowError> {
        self.inner.try_borrow()
    }

    /// Mutably borrows the wrapped value, preserving its pinnedness.
    ///
    /// The borrow lasts until the returned `PinMut` or all `PinMut`s derived
    /// from it exit scope. The value cannot be borrowed while this borrow is
    /// active.
    pub fn borrow_mut<'a>(self: Pin<&'a Self>) -> PinMut<'a, T> {
        self.try_borrow_mut().expect("already borrowed")
    }

    /// Mutably borrows the wrapped value, preserving its pinnedness,
    /// returning an error if the value is currently borrowed.
    ///
    /// The borrow lasts until the returned `PinMut` or all `PinMut`s derived
    /// from it exit scope. The value cannot be borrowed while this borrow is
    /// active.
    ///
    /// This is the non-panicking variant of `borrow_mut`.
    pub fn try_borrow_mut<'a>(self: Pin<&'a Self>) -> Result<PinMut<'a, T>, BorrowMutError> {
        let ref_mut: RefMut<'a, T> = Pin::get_ref(self).inner.try_borrow_mut()?;

        // this is a pin projection from Pin<&PinCell<T>> to Pin<RefMut<T>>
        // projecting is safe because:
        //
        // - for<T: ?Sized> (PinCell<T>: Unpin) imples (RefMut<T>: Unpin)
        //   holds true
        // - PinCell does not implement Drop
        //
        // see discussion on tracking issue #49150 about pin projection
        // invariants
        let pin_ref_mut: Pin<RefMut<'a, T>> = unsafe { Pin::new_unchecked(ref_mut) };

        Ok(PinMut { inner: pin_ref_mut })
    }

    /// Immutably borrows the wrapped value, preserving its pinnedness.
    ///
    /// The borrow lasts until the returned `PinRef` exits scope. Multiple
    /// immutable borrows can be taken out at the same time (both pinned and
    /// unpinned).
    pub fn borrow_pin<'a>(self: Pin<&'a Self>) -> PinRef<'a, T> {
        self.try_borrow_pin().expect("already mutably borrowed")
    }

    /// Immutably borrows the wrapped value, preserving its pinnedness,
    /// returning an error if the value is currently mutably borrowed.
    ///
    /// The borrow lasts until the returned `PinRef` exits scope. Multiple
    /// immutable borrows can be taken out at the same time (both pinned and
    /// unpinned).
    ///
    /// This is the non-panicking variant of `borrow_pin`.
    pub fn try_borrow_pin<'a>(self: Pin<&'a Self>) -> Result<PinRef<'a, T>, BorrowError> {
        let r: Ref<'a, T> = Pin::get_ref(self).inner.try_borrow()?;

        // this is a pin projection from Pin<&PinCell<T>> to Pin<Ref<T>>
        // projecting is safe because:
        //
        // - for<T: ?Sized> (PinCell<T>: Unpin) imples (Ref<T>: Unpin)
        //   holds true
        // - PinCell does not implement Drop
        //
        // see discussion on tracking issue #49150 about pin projection
        // invariants
        let pin_ref: Pin<Ref<'a, T>> = unsafe { Pin::new_unchecked(r) };

        Ok(PinRef { inner: pin_ref })
    }

    /// Returns a raw pointer to the underlying data in this cell.
    pub fn as_ptr(&self) -> *mut T {
        self.inner.as_ptr()
    }

    /// Returns a mutable reference to the underlying data.
    ///
    /// This call borrows `PinCell` mutably (at compile-time) so there is no
    /// need for dynamic checks.
    ///
    /// However be cautious: this method expects self to be mutable, which is
    /// generally not the case when using a `PinCell`. Take a look at the
    /// `borrow_mut` method instead if self isn't mutable.
    ///
    /// Also, please be aware that this method is only for special
    /// circumstances and is usually not what you want. In case of doubt, use
    /// `borrow_mut` instead.
    pub fn get_mut(&mut self) -> &mut T {
        self.inner.get_mut()
    }
}

impl<T> From<T> for PinCell<T> {
    fn from(value: T) -> PinCell<T> {
        PinCell::new(value)
    }
}

impl<T> From<RefCell<T>> for PinCell<T> {
    fn from(cell: RefCell<T>) -> PinCell<T> {
        PinCell { inner: cell }
    }
}

impl<T> Into<RefCell<T>> for PinCell<T> {
    fn into(self) -> RefCell<T> {
        self.inner
    }
}

// TODO CoerceUnsized
