// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Types for optional pointers with niche optimization.
//!
//! The main type is [`CartableOptionPointer`], which is like `Option<Rc>` but
//! with a niche so that the resulting `Yoke` has a niche. The following four
//! types can be stored in the `CartableOptionPointer`:
//!
//! 1. `&T`
//! 2. `Box<T>`
//! 3. `Rc<T>`
//! 4. `Arc<T>`
//!
//! These four types implement the sealed unsafe trait [`CartablePointerLike`].
//! In addition, all except `Box<T>` impl [`CloneableCartablePointerLike`],
//! which allows [`CartableOptionPointer`] to implement `Clone`.

use crate::CloneableCart;
#[cfg(feature = "alloc")]
use alloc::boxed::Box;
#[cfg(feature = "alloc")]
use alloc::rc::Rc;
#[cfg(feature = "alloc")]
use alloc::sync::Arc;
#[cfg(test)]
use core::cell::Cell;
use core::marker::PhantomData;
use core::ptr::NonNull;
use stable_deref_trait::StableDeref;

#[inline]
fn sentinel_for<T>() -> NonNull<T> {
    static SENTINEL: &u8 = &0x1a; // SUB
    unsafe { NonNull::new_unchecked(SENTINEL as *const u8 as *mut T) }
}

#[cfg(test)]
thread_local! {
    static DROP_INVOCATIONS: Cell<usize> = const { Cell::new(0) };
}

mod private {
    pub trait Sealed {}
}

use private::Sealed;

/// An object fully representable by a non-null pointer.
///
/// # Safety
///
/// Implementer safety:
///
/// 1. `into_raw` transfers ownership of the values referenced by StableDeref to the caller,
///    if there is ownership to transfer
/// 2. `drop_raw` returns ownership back to the impl, if there is ownership to transfer
/// 3. `into_raw` must not return the sentinel pointer
///
/// Note: the pointer `NonNull<Self::Raw>` may or may not be aligned and it should never
/// be dereferenced. Rust allows unaligned pointers; see [`std::ptr::read_unaligned`].
pub unsafe trait CartablePointerLike: StableDeref + Sealed {
    /// The raw type used for [`Self::into_raw`] and [`Self::drop_raw`].
    #[doc(hidden)]
    type Raw;

    /// Converts this pointer-like into a pointer.
    #[doc(hidden)]
    fn into_raw(self) -> NonNull<Self::Raw>;

    /// Drops any memory associated with this pointer-like.
    ///
    /// # Safety
    ///
    /// Caller safety:
    ///
    /// 1. The pointer MUST have been returned by this impl's `into_raw`.
    /// 2. The pointer MUST NOT be dangling.
    #[doc(hidden)]
    unsafe fn drop_raw(pointer: NonNull<Self::Raw>);
}

/// An object that implements [`CartablePointerLike`] that also
/// supports cloning without changing the address of referenced data.
///
/// # Safety
///
/// Implementer safety:
///
/// 1. `addref_raw` must create a new owner such that an additional call to
///    `drop_raw` does not create a dangling pointer
/// 2. `addref_raw` must not change the address of any referenced data.
pub unsafe trait CloneableCartablePointerLike: CartablePointerLike {
    /// Clones this pointer-like.
    ///
    /// # Safety
    ///
    /// Caller safety:
    ///
    /// 1. The pointer MUST have been returned by this impl's `into_raw`.
    /// 2. The pointer MUST NOT be dangling.
    #[doc(hidden)]
    unsafe fn addref_raw(pointer: NonNull<Self::Raw>);
}

impl<'a, T> Sealed for &'a T {}

// Safety:
// 1. There is no ownership to transfer
// 2. There is no ownership to transfer
// 3. External clients do not have the sentinel address so it won't be used in this reference.
unsafe impl<'a, T> CartablePointerLike for &'a T {
    type Raw = T;

    #[inline]
    fn into_raw(self) -> NonNull<T> {
        self.into()
    }
    #[inline]
    unsafe fn drop_raw(_pointer: NonNull<T>) {
        // No-op: references are borrowed from elsewhere
    }
}

// Safety:
// 1. There is no ownership
// 2. The impl is a no-op so no addresses are changed.
unsafe impl<'a, T> CloneableCartablePointerLike for &'a T {
    #[inline]
    unsafe fn addref_raw(_pointer: NonNull<T>) {
        // No-op: references are borrowed from elsewhere
    }
}

#[cfg(feature = "alloc")]
impl<T> Sealed for Box<T> {}

// Safety:
// 1. `Box::into_raw` says: "After calling this function, the caller is responsible for the
//    memory previously managed by the Box."
// 2. `Box::from_raw` says: "After calling this function, the raw pointer is owned by the
//    resulting Box."
// 3. The pointer comes from the Box so it won't be the sentinel pointer.
#[cfg(feature = "alloc")]
unsafe impl<T> CartablePointerLike for Box<T> {
    type Raw = T;

    #[inline]
    fn into_raw(self) -> NonNull<T> {
        // Safety: `Box::into_raw` says: "The pointer will be properly aligned and non-null."
        unsafe { NonNull::new_unchecked(Box::into_raw(self)) }
    }
    #[inline]
    unsafe fn drop_raw(pointer: NonNull<T>) {
        let _box = Box::from_raw(pointer.as_ptr());

        // Boxes are always dropped
        #[cfg(test)]
        DROP_INVOCATIONS.with(|x| x.set(x.get() + 1))
    }
}

#[cfg(feature = "alloc")]
impl<T> Sealed for Rc<T> {}

// Safety:
// 1. `Rc::into_raw` says: "Consumes the Rc, returning the wrapped pointer. To avoid a memory
//    leak the pointer must be converted back to an Rc using Rc::from_raw."
// 2. See 1.
// 3. The pointer comes from the Rc so it won't be the sentinel pointer.
#[cfg(feature = "alloc")]
unsafe impl<T> CartablePointerLike for Rc<T> {
    type Raw = T;

    #[inline]
    fn into_raw(self) -> NonNull<T> {
        // Safety: Rcs must contain data (and not be null)
        unsafe { NonNull::new_unchecked(Rc::into_raw(self) as *mut T) }
    }
    #[inline]
    unsafe fn drop_raw(pointer: NonNull<T>) {
        let _rc = Rc::from_raw(pointer.as_ptr());

        // Rc is dropped if refcount is 1
        #[cfg(test)]
        if Rc::strong_count(&_rc) == 1 {
            DROP_INVOCATIONS.with(|x| x.set(x.get() + 1))
        }
    }
}

// Safety:
// 1. The impl increases the refcount such that `Drop` will decrease it.
// 2. The impl increases refcount without changing the address of data.
#[cfg(feature = "alloc")]
unsafe impl<T> CloneableCartablePointerLike for Rc<T> {
    #[inline]
    unsafe fn addref_raw(pointer: NonNull<T>) {
        // Safety: The caller safety of this function says that:
        // 1. The pointer was obtained through Rc::into_raw
        // 2. The associated Rc instance is valid
        // Further, this impl is not defined for anything but the global allocator.
        Rc::increment_strong_count(pointer.as_ptr());
    }
}

#[cfg(feature = "alloc")]
impl<T> Sealed for Arc<T> {}

// Safety:
// 1. `Rc::into_raw` says: "Consumes the Arc, returning the wrapped pointer. To avoid a memory
//    leak the pointer must be converted back to an Arc using Arc::from_raw."
// 2. See 1.
// 3. The pointer comes from the Arc so it won't be the sentinel pointer.
#[cfg(feature = "alloc")]
unsafe impl<T> CartablePointerLike for Arc<T> {
    type Raw = T;

    #[inline]
    fn into_raw(self) -> NonNull<T> {
        // Safety: Arcs must contain data (and not be null)
        unsafe { NonNull::new_unchecked(Arc::into_raw(self) as *mut T) }
    }
    #[inline]
    unsafe fn drop_raw(pointer: NonNull<T>) {
        let _arc = Arc::from_raw(pointer.as_ptr());

        // Arc is dropped if refcount is 1
        #[cfg(test)]
        if Arc::strong_count(&_arc) == 1 {
            DROP_INVOCATIONS.with(|x| x.set(x.get() + 1))
        }
    }
}

// Safety:
// 1. The impl increases the refcount such that `Drop` will decrease it.
// 2. The impl increases refcount without changing the address of data.
#[cfg(feature = "alloc")]
unsafe impl<T> CloneableCartablePointerLike for Arc<T> {
    #[inline]
    unsafe fn addref_raw(pointer: NonNull<T>) {
        // Safety: The caller safety of this function says that:
        // 1. The pointer was obtained through Arc::into_raw
        // 2. The associated Arc instance is valid
        // Further, this impl is not defined for anything but the global allocator.
        Arc::increment_strong_count(pointer.as_ptr());
    }
}

/// A type with similar semantics as `Option<C<T>>` but with a niche.
///
/// This type cannot be publicly constructed. To use this in a `Yoke`, see
/// [`Yoke::convert_cart_into_option_pointer`].
///
/// [`Yoke::convert_cart_into_option_pointer`]: crate::Yoke::convert_cart_into_option_pointer
#[derive(Debug)]
pub struct CartableOptionPointer<C>
where
    C: CartablePointerLike,
{
    /// The inner pointer.
    ///
    /// # Invariants
    ///
    /// 1. Must be either `SENTINEL_PTR` or created from `CartablePointerLike::into_raw`
    /// 2. If non-sentinel, must _always_ be for a valid SelectedRc
    inner: NonNull<C::Raw>,
    _cartable: PhantomData<C>,
}

impl<C> CartableOptionPointer<C>
where
    C: CartablePointerLike,
{
    /// Creates a new instance corresponding to a `None` value.
    #[inline]
    pub(crate) fn none() -> Self {
        Self {
            inner: sentinel_for::<C::Raw>(),
            _cartable: PhantomData,
        }
    }

    /// Creates a new instance corresponding to a `Some` value.
    #[inline]
    pub(crate) fn from_cartable(cartable: C) -> Self {
        Self {
            inner: cartable.into_raw(),
            _cartable: PhantomData,
        }
    }

    /// Returns whether this instance is `None`. From the return value:
    ///
    /// - If `true`, the instance is `None`
    /// - If `false`, the instance is a valid `SelectedRc`
    #[inline]
    pub fn is_none(&self) -> bool {
        self.inner == sentinel_for::<C::Raw>()
    }
}

impl<C> Drop for CartableOptionPointer<C>
where
    C: CartablePointerLike,
{
    #[inline]
    fn drop(&mut self) {
        let ptr = self.inner;
        if ptr != sentinel_for::<C::Raw>() {
            // By the invariants, `ptr` is a valid raw value since it's
            // either that or sentinel, and we just checked for sentinel.
            // We will replace it with the sentinel and then drop `ptr`.
            self.inner = sentinel_for::<C::Raw>();
            // Safety: by the invariants, `ptr` is a valid raw value.
            unsafe { C::drop_raw(ptr) }
        }
    }
}

impl<C> Clone for CartableOptionPointer<C>
where
    C: CloneableCartablePointerLike,
{
    #[inline]
    fn clone(&self) -> Self {
        let ptr = self.inner;
        if ptr != sentinel_for::<C::Raw>() {
            // By the invariants, `ptr` is a valid raw value since it's
            // either that or sentinel, and we just checked for sentinel.
            // Safety: by the invariants, `ptr` is a valid raw value.
            unsafe { C::addref_raw(ptr) }
        }
        Self {
            inner: self.inner,
            _cartable: PhantomData,
        }
    }
}

// Safety: logically an Option<C>. Has same bounds as Option<C>.
// The `StableDeref` parts of `C` continue to be `StableDeref`.
unsafe impl<C> CloneableCart for CartableOptionPointer<C> where
    C: CloneableCartablePointerLike + CloneableCart
{
}

// Safety: logically an Option<C>. Has same bounds as Option<C>
unsafe impl<C> Send for CartableOptionPointer<C> where C: Sync + CartablePointerLike {}

// Safety: logically an Option<C>. Has same bounds as Option<C>
unsafe impl<C> Sync for CartableOptionPointer<C> where C: Send + CartablePointerLike {}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::Yoke;
    use core::mem::size_of;

    const SAMPLE_BYTES: &[u8] = b"abCDEfg";
    const W: usize = size_of::<usize>();

    #[test]
    fn test_sizes() {
        assert_eq!(W * 4, size_of::<Yoke<[usize; 3], &&[u8]>>());
        assert_eq!(W * 4, size_of::<Yoke<[usize; 3], Option<&&[u8]>>>());
        assert_eq!(
            W * 4,
            size_of::<Yoke<[usize; 3], CartableOptionPointer<&&[u8]>>>()
        );

        assert_eq!(W * 4, size_of::<Option<Yoke<[usize; 3], &&[u8]>>>());
        assert_eq!(W * 5, size_of::<Option<Yoke<[usize; 3], Option<&&[u8]>>>>());
        assert_eq!(
            W * 4,
            size_of::<Option<Yoke<[usize; 3], CartableOptionPointer<&&[u8]>>>>()
        );
    }

    #[test]
    fn test_new_sentinel() {
        let start = DROP_INVOCATIONS.with(Cell::get);
        {
            let _ = CartableOptionPointer::<Rc<&[u8]>>::none();
        }
        assert_eq!(start, DROP_INVOCATIONS.with(Cell::get));
        {
            let _ = CartableOptionPointer::<Rc<&[u8]>>::none();
        }
        assert_eq!(start, DROP_INVOCATIONS.with(Cell::get));
    }

    #[test]
    fn test_new_rc() {
        let start = DROP_INVOCATIONS.with(Cell::get);
        {
            let _ = CartableOptionPointer::<Rc<&[u8]>>::from_cartable(SAMPLE_BYTES.into());
        }
        assert_eq!(start + 1, DROP_INVOCATIONS.with(Cell::get));
    }

    #[test]
    fn test_rc_clone() {
        let start = DROP_INVOCATIONS.with(Cell::get);
        {
            let x = CartableOptionPointer::<Rc<&[u8]>>::from_cartable(SAMPLE_BYTES.into());
            assert_eq!(start, DROP_INVOCATIONS.with(Cell::get));
            {
                let _ = x.clone();
            }
            assert_eq!(start, DROP_INVOCATIONS.with(Cell::get));
            {
                let _ = x.clone();
                let _ = x.clone();
                let _ = x.clone();
            }
            assert_eq!(start, DROP_INVOCATIONS.with(Cell::get));
        }
        assert_eq!(start + 1, DROP_INVOCATIONS.with(Cell::get));
    }
}
