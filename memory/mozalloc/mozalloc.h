/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=4 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_mozalloc_h
#define mozilla_mozalloc_h

/*
 * https://bugzilla.mozilla.org/show_bug.cgi?id=427099
 */

#if defined(__cplusplus)
#include <new>
// Since libstdc++ 6, including the C headers (e.g. stdlib.h) instead of the
// corresponding C++ header (e.g. cstdlib) can cause confusion in C++ code
// using things defined there. Specifically, with stdlib.h, the use of abs()
// in gfx/graphite2/src/inc/UtfCodec.h somehow ends up picking the wrong abs()
#include <cstdlib>
#else
#include <stdlib.h>
#endif

#if defined(__cplusplus)
#include "mozilla/fallible.h"
#include "mozilla/mozalloc_abort.h"
#include "mozilla/TemplateLib.h"
#endif
#include "mozilla/Attributes.h"
#include "mozilla/Types.h"

MOZ_BEGIN_EXTERN_C

/*
 * We need to use malloc_impl and free_impl in this file when they are
 * defined, because of how mozglue.dll is linked on Windows, where using
 * malloc/free would end up using the symbols from the MSVCRT instead of
 * ours.
 */
#ifndef free_impl
#define free_impl free
#define free_impl_
#endif
#ifndef malloc_impl
#define malloc_impl malloc
#define malloc_impl_
#endif

/*
 * Each declaration below is analogous to a "standard" allocation
 * function, except that the out-of-memory handling is made explicit.
 * The |moz_x| versions will never return a NULL pointer; if memory
 * is exhausted, they abort.  The |moz_| versions may return NULL
 * pointers if memory is exhausted: their return value must be checked.
 *
 * All these allocation functions are *guaranteed* to return a pointer
 * to memory allocated in such a way that that memory can be freed by
 * passing that pointer to |free()|.
 */

MFBT_API void* moz_xmalloc(size_t size) MOZ_ALLOCATOR;

MFBT_API void* moz_xcalloc(size_t nmemb, size_t size) MOZ_ALLOCATOR;

MFBT_API void* moz_xrealloc(void* ptr, size_t size) MOZ_ALLOCATOR;

MFBT_API char* moz_xstrdup(const char* str) MOZ_ALLOCATOR;

#if defined(HAVE_STRNDUP)
MFBT_API char* moz_xstrndup(const char* str, size_t strsize) MOZ_ALLOCATOR;
#endif /* if defined(HAVE_STRNDUP) */

MFBT_API void* moz_xmemdup(const void* ptr, size_t size) MOZ_ALLOCATOR;

MFBT_API void* moz_xmemalign(size_t boundary, size_t size) MOZ_ALLOCATOR;

MFBT_API size_t moz_malloc_usable_size(void* ptr);

MFBT_API size_t moz_malloc_size_of(const void* ptr);

/*
 * Like moz_malloc_size_of(), but works reliably with interior pointers, i.e.
 * pointers into the middle of a live allocation.
 */
MFBT_API size_t moz_malloc_enclosing_size_of(const void* ptr);

MOZ_END_EXTERN_C

#ifdef __cplusplus

/*
 * We implement the default operators new/delete as part of
 * libmozalloc, replacing their definitions in libstdc++.  The
 * operator new* definitions in libmozalloc will never return a NULL
 * pointer.
 *
 * Each operator new immediately below returns a pointer to memory
 * that can be delete'd by any of
 *
 *   (1) the matching infallible operator delete immediately below
 *   (2) the matching system |operator delete(void*, std::nothrow)|
 *   (3) the matching system |operator delete(void*) noexcept(false)|
 *
 * NB: these are declared |noexcept(false)|, though they will never
 * throw that exception.  This declaration is consistent with the rule
 * that |::operator new() noexcept(false)| will never return NULL.
 *
 * NB: mozilla::fallible can be used instead of std::nothrow.
 */

/* NB: This is defined just to silence vacuous warnings about symbol
 * visibility on OS X/gcc. These symbols are force-inline and not
 * exported. */
#if defined(XP_MACOSX)
#define MOZALLOC_EXPORT_NEW MFBT_API
#else
#define MOZALLOC_EXPORT_NEW
#endif

MOZALLOC_EXPORT_NEW
#if defined(__GNUC__) && !defined(__clang__) && defined(__SANITIZE_ADDRESS__)
/* gcc's asan somehow doesn't like always_inline on this function. */
__attribute__((gnu_inline)) inline
#else
MOZ_ALWAYS_INLINE_EVEN_DEBUG
#endif
    void*
    operator new(size_t size) noexcept(false) {
  return moz_xmalloc(size);
}

MOZALLOC_EXPORT_NEW MOZ_ALWAYS_INLINE_EVEN_DEBUG void* operator new(
    size_t size, const std::nothrow_t&) noexcept(true) {
  return malloc_impl(size);
}

MOZALLOC_EXPORT_NEW MOZ_ALWAYS_INLINE_EVEN_DEBUG void* operator new[](
    size_t size) noexcept(false) {
  return moz_xmalloc(size);
}

MOZALLOC_EXPORT_NEW MOZ_ALWAYS_INLINE_EVEN_DEBUG void* operator new[](
    size_t size, const std::nothrow_t&) noexcept(true) {
  return malloc_impl(size);
}

MOZALLOC_EXPORT_NEW MOZ_ALWAYS_INLINE_EVEN_DEBUG void operator delete(
    void* ptr) noexcept(true) {
  return free_impl(ptr);
}

MOZALLOC_EXPORT_NEW MOZ_ALWAYS_INLINE_EVEN_DEBUG void operator delete(
    void* ptr, const std::nothrow_t&)noexcept(true) {
  return free_impl(ptr);
}

MOZALLOC_EXPORT_NEW MOZ_ALWAYS_INLINE_EVEN_DEBUG void operator delete[](
    void* ptr) noexcept(true) {
  return free_impl(ptr);
}

MOZALLOC_EXPORT_NEW MOZ_ALWAYS_INLINE_EVEN_DEBUG void operator delete[](
    void* ptr, const std::nothrow_t&) noexcept(true) {
  return free_impl(ptr);
}

#if defined(XP_WIN)
// We provide the global sized delete overloads unconditionally because the
// MSVC runtime headers do, despite compiling with /Zc:sizedDealloc-
MOZALLOC_EXPORT_NEW MOZ_ALWAYS_INLINE_EVEN_DEBUG void operator delete(
    void* ptr, size_t /*size*/) noexcept(true) {
  return free_impl(ptr);
}

MOZALLOC_EXPORT_NEW MOZ_ALWAYS_INLINE_EVEN_DEBUG void operator delete[](
    void* ptr, size_t /*size*/) noexcept(true) {
  return free_impl(ptr);
}
#endif

/*
 * This policy is identical to MallocAllocPolicy, except it uses
 * moz_xmalloc/moz_xcalloc/moz_xrealloc instead of
 * malloc/calloc/realloc.
 */
class InfallibleAllocPolicy {
 public:
  template <typename T>
  T* maybe_pod_malloc(size_t aNumElems) {
    return pod_malloc<T>(aNumElems);
  }

  template <typename T>
  T* maybe_pod_calloc(size_t aNumElems) {
    return pod_calloc<T>(aNumElems);
  }

  template <typename T>
  T* maybe_pod_realloc(T* aPtr, size_t aOldSize, size_t aNewSize) {
    return pod_realloc<T>(aPtr, aOldSize, aNewSize);
  }

  template <typename T>
  T* pod_malloc(size_t aNumElems) {
    if (aNumElems & mozilla::tl::MulOverflowMask<sizeof(T)>::value) {
      reportAllocOverflow();
    }
    return static_cast<T*>(moz_xmalloc(aNumElems * sizeof(T)));
  }

  template <typename T>
  T* pod_calloc(size_t aNumElems) {
    return static_cast<T*>(moz_xcalloc(aNumElems, sizeof(T)));
  }

  template <typename T>
  T* pod_realloc(T* aPtr, size_t aOldSize, size_t aNewSize) {
    if (aNewSize & mozilla::tl::MulOverflowMask<sizeof(T)>::value) {
      reportAllocOverflow();
    }
    return static_cast<T*>(moz_xrealloc(aPtr, aNewSize * sizeof(T)));
  }

  template <typename T>
  void free_(T* aPtr, size_t aNumElems = 0) {
    free_impl(aPtr);
  }

  void reportAllocOverflow() const { mozalloc_abort("alloc overflow"); }

  bool checkSimulatedOOM() const { return true; }
};

#endif /* ifdef __cplusplus */

#ifdef malloc_impl_
#undef malloc_impl_
#undef malloc_impl
#endif
#ifdef free_impl_
#undef free_impl_
#undef free_impl
#endif

#endif /* ifndef mozilla_mozalloc_h */
