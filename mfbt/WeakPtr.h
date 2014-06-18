/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Weak pointer functionality, implemented as a mixin for use with any class. */

/**
 * SupportsWeakPtr lets you have a pointer to an object 'Foo' without affecting
 * its lifetime. It works by creating a single shared reference counted object
 * (WeakReference) that each WeakPtr will access 'Foo' through. This lets 'Foo'
 * clear the pointer in the WeakReference without having to know about all of
 * the WeakPtrs to it and allows the WeakReference to live beyond the lifetime
 * of 'Foo'.
 *
 * PLEASE NOTE: This weak pointer implementation is not thread-safe.
 *
 * Note that when deriving from SupportsWeakPtr you should add
 * MOZ_DECLARE_REFCOUNTED_TYPENAME(ClassName) to the public section of your
 * class, where ClassName is the name of your class.
 *
 * The overhead of WeakPtr is that accesses to 'Foo' becomes an additional
 * dereference, and an additional heap allocated pointer sized object shared
 * between all of the WeakPtrs.
 *
 * Example of usage:
 *
 *   // To have a class C support weak pointers, inherit from SupportsWeakPtr<C>.
 *   class C : public SupportsWeakPtr<C>
 *   {
 *    public:
 *      MOZ_DECLARE_REFCOUNTED_TYPENAME(C)
 *      int num;
 *      void act();
 *   };
 *
 *   C* ptr =  new C();
 *
 *   // Get weak pointers to ptr. The first time asWeakPtr is called
 *   // a reference counted WeakReference object is created that
 *   // can live beyond the lifetime of 'ptr'. The WeakReference
 *   // object will be notified of 'ptr's destruction.
 *   WeakPtr<C> weak = ptr->asWeakPtr();
 *   WeakPtr<C> other = ptr->asWeakPtr();
 *
 *   // Test a weak pointer for validity before using it.
 *   if (weak) {
 *     weak->num = 17;
 *     weak->act();
 *   }
 *
 *   // Destroying the underlying object clears weak pointers to it.
 *   delete ptr;
 *
 *   MOZ_ASSERT(!weak, "Deleting |ptr| clears weak pointers to it.");
 *   MOZ_ASSERT(!other, "Deleting |ptr| clears all weak pointers to it.");
 *
 * WeakPtr is typesafe and may be used with any class. It is not required that
 * the class be reference-counted or allocated in any particular way.
 *
 * The API was loosely inspired by Chromium's weak_ptr.h:
 * http://src.chromium.org/svn/trunk/src/base/memory/weak_ptr.h
 */

#ifndef mozilla_WeakPtr_h
#define mozilla_WeakPtr_h

#include "mozilla/ArrayUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/NullPtr.h"
#include "mozilla/RefPtr.h"
#include "mozilla/TypeTraits.h"

#include <string.h>

namespace mozilla {

template <typename T, class WeakReference> class WeakPtrBase;
template <typename T, class WeakReference> class SupportsWeakPtrBase;

namespace detail {

// This can live beyond the lifetime of the class derived from SupportsWeakPtrBase.
template<class T>
class WeakReference : public ::mozilla::RefCounted<WeakReference<T> >
{
  public:
    explicit WeakReference(T* p) : ptr(p) {}
    T* get() const {
      return ptr;
    }

#ifdef MOZ_REFCOUNTED_LEAK_CHECKING
#ifdef XP_WIN
#define snprintf _snprintf
#endif
    const char* typeName() const {
      static char nameBuffer[1024];
      const char* innerType = ptr->typeName();
      // We could do fancier length checks at runtime, but innerType is
      // controlled by us so we can ensure that this never causes a buffer
      // overflow by this assertion.
      MOZ_ASSERT(strlen(innerType) + sizeof("WeakReference<>") < ArrayLength(nameBuffer),
                 "Exceedingly large type name");
      snprintf(nameBuffer, ArrayLength(nameBuffer), "WeakReference<%s>", innerType);
      // This is usually not OK, but here we are returning a pointer to a static
      // buffer which will immediately be used by the caller.
      return nameBuffer;
    }
    size_t typeSize() const {
      return sizeof(*this);
    }
#undef snprintf
#endif

  private:
    friend class WeakPtrBase<T, WeakReference<T> >;
    friend class SupportsWeakPtrBase<T, WeakReference<T> >;
    void detach() {
      ptr = nullptr;
    }
    T* ptr;
};

} // namespace detail

template <typename T, class WeakReference>
class SupportsWeakPtrBase
{
  public:
    WeakPtrBase<T, WeakReference> asWeakPtr() {
      if (!weakRef)
        weakRef = new WeakReference(static_cast<T*>(this));
      return WeakPtrBase<T, WeakReference>(weakRef);
    }

  protected:
    ~SupportsWeakPtrBase() {
      static_assert(IsBaseOf<SupportsWeakPtrBase<T, WeakReference>, T>::value,
                    "T must derive from SupportsWeakPtrBase<T, WeakReference>");
      if (weakRef)
        weakRef->detach();
    }

  private:
    friend class WeakPtrBase<T, WeakReference>;

    RefPtr<WeakReference> weakRef;
};

template <typename T>
class SupportsWeakPtr : public SupportsWeakPtrBase<T, detail::WeakReference<T> >
{
};

template <typename T, class WeakReference>
class WeakPtrBase
{
  public:
    WeakPtrBase(const WeakPtrBase<T, WeakReference>& o) : ref(o.ref) {}
    // Ensure that ref is dereferenceable in the uninitialized state
    WeakPtrBase() : ref(new WeakReference(nullptr)) {}

    operator T*() const {
      return ref->get();
    }
    T& operator*() const {
      return *ref->get();
    }

    T* operator->() const {
      return ref->get();
    }

    T* get() const {
      return ref->get();
    }

  private:
    friend class SupportsWeakPtrBase<T, WeakReference>;

    explicit WeakPtrBase(const RefPtr<WeakReference> &o) : ref(o) {}

    RefPtr<WeakReference> ref;
};

template <typename T>
class WeakPtr : public WeakPtrBase<T, detail::WeakReference<T> >
{
    typedef WeakPtrBase<T, detail::WeakReference<T> > Base;
  public:
    WeakPtr(const WeakPtr<T>& o) : Base(o) {}
    MOZ_IMPLICIT WeakPtr(const Base& o) : Base(o) {}
    WeakPtr() {}
};

} // namespace mozilla

#endif /* mozilla_WeakPtr_h */
