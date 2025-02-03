/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_WinOLELock_h__
#define mozilla_widget_WinOLELock_h__

#include <type_traits>
#include <minwindef.h>
#include <winbase.h>
#include "mozilla/Assertions.h"

namespace details {
// implementation of `is_complete_type_v` borrowed from Raymond Chen:
//  https://devblogs.microsoft.com/oldnewthing/20190711-00/?p=102682
template <typename, typename = void>
constexpr bool is_complete_type_v = false;
template <typename T>
constexpr bool is_complete_type_v<T, std::void_t<decltype(sizeof(T))>> = true;

// The Windows SDK typically declares handle types to be of type "pointer to
// incomplete struct type"; this is broadly a good choice, but implies that we
// need to take additional steps to avoid declaring operator* or operator-> for
// smart pointers thereto.
template <typename T>
constexpr bool is_dereferenceable_v =
    std::is_pointer_v<T> && is_complete_type_v<std::remove_pointer_t<T>>;

template <typename T>
constexpr bool is_arrowable_v =
    std::is_pointer_v<T> && is_complete_type_v<std::remove_pointer_t<T>> &&
    std::is_class_v<std::remove_pointer_t<T>>;

// SFINAE mixins for ScopedOLELock, below.
template <typename Derived, typename T, bool = is_dereferenceable_v<T>,
          bool = is_arrowable_v<T>>
struct OLELockMixin {};

template <typename D, typename T>
struct OLELockMixin<D, T, true, false> {
  auto operator*() const -> std::remove_pointer_t<T>& {
    T ptr = static_cast<D const*>(this)->get();
    MOZ_ASSERT(ptr);
    return *ptr;
  }
};

template <typename D, typename T>
struct OLELockMixin<D, T, true, true> : OLELockMixin<D, T, true, false> {
  auto operator->() const -> T {
    T ptr = static_cast<D const*>(this)->get();
    MOZ_ASSERT(ptr);
    return ptr;
  }
};
}  // namespace details

// RAII scoped-handle object for ::GlobalLock()ed data -- which, in practice,
// means data associated with either the clipboard or with drag-and-drop.
//
// T must be of pointer, handle, or array type.
template <class T>
class ScopedOLELock;

// Handle/pointer implementation of ScopedHGLock.
template <class T>
class ScopedOLELock : public details::OLELockMixin<ScopedOLELock<T>, T> {
 public:
  explicit ScopedOLELock(HGLOBAL glob) : mGlobal(glob) {
    static_assert(std::is_pointer_v<T>);
    mData = static_cast<T>(::GlobalLock(mGlobal));
  }
  ~ScopedOLELock() { ::GlobalUnlock(mGlobal); }

  auto operator=(ScopedOLELock const&) = delete;
  ScopedOLELock(ScopedOLELock const&) = delete;

  explicit operator bool() const { return bool(mData); }

  auto get() const -> T { return mData; }

  // operator* and operator-> are also SFINAE'd in, when usable.
  // TODO(C++20): inline them here.

 private:
  HGLOBAL mGlobal;
  T mData;
};

template <class U>
class ScopedOLELock<U[]> {
 public:
  explicit ScopedOLELock(HGLOBAL glob) : mGlobal(glob) {
    static_assert(details::is_complete_type_v<U>);
    mData = static_cast<U*>(::GlobalLock(mGlobal));

    auto const [quot, rem] = std::lldiv(GlobalSize(mGlobal), sizeof(U));
    mExtent = quot;
    NS_WARNING_ASSERTION(
        rem == 0,
        "size of alleged array is not a multiple of the array element size");
  }
  ~ScopedOLELock() { ::GlobalUnlock(mGlobal); }

  auto operator=(ScopedOLELock const&) = delete;
  ScopedOLELock(ScopedOLELock const&) = delete;

  explicit operator bool() const { return bool(mData); }

  auto get() const -> U* { return mData; }

  // STL-style, for range-based iteration and so forth
  auto begin() const -> U* { return mData; }
  auto end() const -> U* { return mData + mExtent; }
  auto size() const -> size_t { return mExtent; }

  auto operator[](size_t index) const {
    MOZ_ASSERT(index < mExtent);
    return mData[index];
  }

 private:
  HGLOBAL mGlobal;
  U* mData;
  size_t mExtent;
};

// RAII scoped-handle object for _locally-created_ ::GlobalLock()ed data.
//
// T must be of POD type. A specialication for runtime-bounded array types is
// provided below.
template <typename T>
class ScopedOLEMemory {
  static_assert(alignof(T) <= 8,
                "GlobalAlloc only aligns to 8-byte boundaries");

  // This could be weakened if needed, but not eliminated: the destructor must
  // be trivial.
  static_assert(std::is_pod_v<T>, "type must be POD");

 public:
  explicit ScopedOLEMemory()
      : mHandle(::GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, sizeof(T))) {}
  ~ScopedOLEMemory() {
    // documented as `_Frees_ptr_opt_`, so we don't need to check for NULL
    ::GlobalFree(mHandle);
  }

  ScopedOLELock<T*> lock() const { return ScopedOLELock<T*>(mHandle); }

  explicit operator bool() const { return bool(mHandle); }

  HGLOBAL forget() {
    HGLOBAL r{mHandle};
    mHandle = nullptr;
    return r;
  }

 private:
  HGLOBAL mHandle;
};

template <typename U>
class ScopedOLEMemory<U[]> {
  static_assert(alignof(U) <= 8,
                "GlobalAlloc only aligns to 8-byte boundaries");
  // This could be weakened if needed, but not eliminated: the destructor must
  // be trivial.
  static_assert(std::is_pod_v<U>, "type must be POD");

 public:
  explicit ScopedOLEMemory(size_t n)
      : mHandle(::GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, sizeof(U) * n)),
        mExtent(n) {}
  ~ScopedOLEMemory() { ::GlobalFree(mHandle); }

  ScopedOLELock<U[]> lock() const { return ScopedOLELock<U[]>(mHandle); }

  explicit operator bool() const { return bool(mHandle); }

  HGLOBAL forget() {
    HGLOBAL r{mHandle};
    mHandle = nullptr;
    return r;
  }

 private:
  HGLOBAL mHandle;
  size_t mExtent;
};

#endif  // mozilla_widget_WinOLELock_h__
