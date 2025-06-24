/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_InvalidationScriptSet_h
#define jit_InvalidationScriptSet_h

#include "gc/Barrier.h"
#include "jit/Invalidation.h"
#include "jit/IonTypes.h"
#include "js/AllocPolicy.h"
#include "js/GCVector.h"
#include "js/SweepingAPI.h"

namespace js::jit {

// A set of Ion scripts that will be invalidated simultaneously.
//
// When using this class, make sure the traceWeak method is called by the GC to
// sweep dead scripts.
class DependentIonScriptSet {
  IonScriptKeyVector ionScripts_;

  // To avoid keeping a lot of stale entries for invalidated IonScripts between
  // GCs, we compact the vector when it grows too large.
  size_t lengthAfterLastCompaction_ = 0;

 public:
  [[nodiscard]] bool addToSet(const IonScriptKey& ionScript);
  void invalidateAndClear(JSContext* cx, const char* reason);

  bool empty() const { return ionScripts_.empty(); }

  bool traceWeak(JSTracer* trc) {
    bool res = ionScripts_.traceWeak(trc);
    lengthAfterLastCompaction_ = ionScripts_.length();
    return res;
  }
};

}  // namespace js::jit

#endif /* jit_InvalidationScriptSet_h */
