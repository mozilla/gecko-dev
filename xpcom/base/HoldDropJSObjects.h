/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_HoldDropJSObjects_h
#define mozilla_HoldDropJSObjects_h

#include <type_traits>
#include "nsCycleCollectionNoteChild.h"

class nsISupports;
class nsScriptObjectTracer;
class nsCycleCollectionParticipant;

namespace JS {
class Zone;
}

// Only HoldJSObjects and DropJSObjects should be called directly.

namespace mozilla {

enum class ShouldClearJSRefs : bool { Clear = true, AlreadyCleared = false };

namespace cyclecollector {

void HoldJSObjectsImpl(void* aHolder, nsScriptObjectTracer* aTracer,
                       JS::Zone* aZone = nullptr);
void HoldJSObjectsImpl(nsISupports* aHolder);
void DropJSObjectsImpl(void* aHolder,
                       ShouldClearJSRefs aClearRefs = ShouldClearJSRefs::Clear);
void DropJSObjectsImpl(nsISupports* aHolder, ShouldClearJSRefs aClearRefs);

}  // namespace cyclecollector

template <class T, bool isISupports = std::is_base_of<nsISupports, T>::value,
          typename P = typename T::NS_CYCLE_COLLECTION_INNERCLASS>
struct HoldDropJSObjectsHelper {
  static void Hold(T* aHolder) {
    cyclecollector::HoldJSObjectsImpl(aHolder,
                                      NS_CYCLE_COLLECTION_PARTICIPANT(T));
  }
  static void Drop(T* aHolder, ShouldClearJSRefs aClearRefs) {
    cyclecollector::DropJSObjectsImpl(aHolder, aClearRefs);
  }
};

template <class T>
struct HoldDropJSObjectsHelper<T, true> {
  static void Hold(T* aHolder) {
    cyclecollector::HoldJSObjectsImpl(ToSupports(aHolder));
  }
  static void Drop(T* aHolder, ShouldClearJSRefs aClearRefs) {
    cyclecollector::DropJSObjectsImpl(ToSupports(aHolder), aClearRefs);
  }
};

/**
  Classes that hold strong references to JS GC things such as `JSObjects` and
  `JS::Values` (e.g. `JS::Heap<JSObject*> mFoo;`) must use these, generally by
  calling `HoldJSObjects(this)` and `DropJSObjects(this)` in the ctor and dtor
  respectively.

  For classes that are wrapper cached and hold no other strong references to JS
  GC things, there's no need to call these; it will be taken care of
  automatically by nsWrapperCache.

  When called with one argument `DropJSObjects` will clear any references to JS
  GC things. This is important because otherwise these would become dangling
  references and accessing them after calling `DropJSObjects` could result in
  use-after-free.

  An optional second argument to `DropJSObjects` can be passed to skip clearing
  these references in cases where this is guaranteed to have already happened.
  This may help in places where performance is a concern. The default is to
  clear these references and is the right thing to do in most circumstances.
**/
template <class T>
void HoldJSObjects(T* aHolder) {
  static_assert(!std::is_base_of<nsCycleCollectionParticipant, T>::value,
                "Don't call this on the CC participant but on the object that "
                "it's for (in an Unlink implementation it's usually stored in "
                "a variable named 'tmp').");
  HoldDropJSObjectsHelper<T>::Hold(aHolder);
}

template <class T>
void DropJSObjects(T* aHolder,
                   ShouldClearJSRefs aClearRefs = ShouldClearJSRefs::Clear) {
  static_assert(!std::is_base_of<nsCycleCollectionParticipant, T>::value,
                "Don't call this on the CC participant but on the object that "
                "it's for (in an Unlink implementation it's usually stored in "
                "a variable named 'tmp').");
  HoldDropJSObjectsHelper<T>::Drop(aHolder, aClearRefs);
}

}  // namespace mozilla

#endif  // mozilla_HoldDropJSObjects_h
