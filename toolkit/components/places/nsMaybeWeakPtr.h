/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsMaybeWeakPtr_h_
#define nsMaybeWeakPtr_h_

#include "mozilla/Attributes.h"
#include "nsCOMPtr.h"
#include "nsWeakReference.h"
#include "nsTArray.h"
#include "nsCycleCollectionNoteChild.h"

// nsMaybeWeakPtr is a helper object to hold a strong-or-weak reference
// to the template class.  It's pretty minimal, but sufficient.

template<class T>
class nsMaybeWeakPtr
{
public:
  MOZ_IMPLICIT nsMaybeWeakPtr(nsISupports* aRef) : mPtr(aRef) {}
  MOZ_IMPLICIT nsMaybeWeakPtr(const nsCOMPtr<nsIWeakReference>& aRef) : mPtr(aRef) {}
  MOZ_IMPLICIT nsMaybeWeakPtr(const nsCOMPtr<T>& aRef) : mPtr(aRef) {}

  bool operator==(const nsMaybeWeakPtr<T> &other) const {
    return mPtr == other.mPtr;
  }

  nsISupports* GetRawValue() const { return mPtr.get(); }

  const nsCOMPtr<T> GetValue() const;

private:
  nsCOMPtr<nsISupports> mPtr;
};

// nsMaybeWeakPtrArray is an array of MaybeWeakPtr objects, that knows how to
// grab a weak reference to a given object if requested.  It only allows a
// given object to appear in the array once.

template<class T>
class nsMaybeWeakPtrArray : public nsTArray<nsMaybeWeakPtr<T>>
{
  typedef nsTArray<nsMaybeWeakPtr<T>> MaybeWeakArray;

public:
  nsresult AppendWeakElement(T* aElement, bool aOwnsWeak)
  {
    nsCOMPtr<nsISupports> ref;
    if (aOwnsWeak) {
      ref = do_GetWeakReference(aElement);
    } else {
      ref = aElement;
    }

    if (MaybeWeakArray::Contains(ref.get())) {
      return NS_ERROR_INVALID_ARG;
    }
    if (!MaybeWeakArray::AppendElement(ref)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    return NS_OK;
  }

  nsresult RemoveWeakElement(T* aElement)
  {
    if (MaybeWeakArray::RemoveElement(aElement)) {
      return NS_OK;
    }

    // Don't use do_GetWeakReference; it should only be called if we know
    // the object supports weak references.
    nsCOMPtr<nsISupportsWeakReference> supWeakRef = do_QueryInterface(aElement);
    NS_ENSURE_TRUE(supWeakRef, NS_ERROR_INVALID_ARG);

    nsCOMPtr<nsIWeakReference> weakRef;
    nsresult rv = supWeakRef->GetWeakReference(getter_AddRefs(weakRef));
    NS_ENSURE_SUCCESS(rv, rv);

    if (MaybeWeakArray::RemoveElement(weakRef)) {
      return NS_OK;
    }

    return NS_ERROR_INVALID_ARG;
  }
};

template<class T>
const nsCOMPtr<T>
nsMaybeWeakPtr<T>::GetValue() const
{
  if (!mPtr) {
    return nullptr;
  }

  nsresult rv;
  nsCOMPtr<T> ref = do_QueryInterface(mPtr, &rv);
  if (NS_SUCCEEDED(rv)) {
    return ref;
  }

  nsCOMPtr<nsIWeakReference> weakRef = do_QueryInterface(mPtr);
  if (weakRef) {
    ref = do_QueryReferent(weakRef, &rv);
    if (NS_SUCCEEDED(rv)) {
      return ref;
    }
  }

  return nullptr;
}

template <typename T>
inline void
ImplCycleCollectionUnlink(nsMaybeWeakPtrArray<T>& aField)
{
  aField.Clear();
}

template <typename E>
inline void
ImplCycleCollectionTraverse(nsCycleCollectionTraversalCallback& aCallback,
                            nsMaybeWeakPtrArray<E>& aField,
                            const char* aName,
                            uint32_t aFlags = 0)
{
  aFlags |= CycleCollectionEdgeNameArrayFlag;
  size_t length = aField.Length();
  for (size_t i = 0; i < length; ++i) {
    CycleCollectionNoteChild(aCallback, aField[i].GetRawValue(), aName, aFlags);
  }
}

// Call a method on each element in the array, but only if the element is
// non-null.

#define ENUMERATE_WEAKARRAY(array, type, method)                           \
  for (uint32_t array_idx = 0; array_idx < array.Length(); ++array_idx) {  \
    const nsCOMPtr<type> &e = array.ElementAt(array_idx).GetValue();       \
    if (e)                                                                 \
      e->method;                                                           \
  }

#endif
