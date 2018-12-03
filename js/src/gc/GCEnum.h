/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * GC-internal enum definitions.
 */

#ifndef gc_GCEnum_h
#define gc_GCEnum_h

#include <stdint.h>

namespace js {
namespace gc {

// Mark colors to pass to markIfUnmarked.
enum class MarkColor : uint32_t { Black = 0, Gray };

// The phases of an incremental GC.
#define GCSTATES(D) \
  D(NotActive)      \
  D(MarkRoots)      \
  D(Mark)           \
  D(Sweep)          \
  D(Finalize)       \
  D(Compact)        \
  D(Decommit)
enum class State {
#define MAKE_STATE(name) name,
  GCSTATES(MAKE_STATE)
#undef MAKE_STATE
};

// Reasons we reset an ongoing incremental GC or perform a non-incremental GC.
#define GC_ABORT_REASONS(D)     \
  D(None, 0)                    \
  D(NonIncrementalRequested, 1) \
  D(AbortRequested, 2)          \
  D(Unused1, 3)                 \
  D(IncrementalDisabled, 4)     \
  D(ModeChange, 5)              \
  D(MallocBytesTrigger, 6)      \
  D(GCBytesTrigger, 7)          \
  D(ZoneChange, 8)              \
  D(CompartmentRevived, 9)      \
  D(GrayRootBufferingFailed, 10)
enum class AbortReason {
#define MAKE_REASON(name, num) name = num,
  GC_ABORT_REASONS(MAKE_REASON)
#undef MAKE_REASON
};

#define JS_FOR_EACH_ZEAL_MODE(D)       \
  D(RootsChange, 1)                    \
  D(Alloc, 2)                          \
  D(VerifierPre, 4)                    \
  D(GenerationalGC, 7)                 \
  D(YieldBeforeMarking, 8)             \
  D(YieldBeforeSweeping, 9)            \
  D(IncrementalMultipleSlices, 10)     \
  D(IncrementalMarkingValidator, 11)   \
  D(ElementsBarrier, 12)               \
  D(CheckHashTablesOnMinorGC, 13)      \
  D(Compact, 14)                       \
  D(CheckHeapAfterGC, 15)              \
  D(CheckNursery, 16)                  \
  D(YieldBeforeSweepingAtoms, 17)      \
  D(CheckGrayMarking, 18)              \
  D(YieldBeforeSweepingCaches, 19)     \
  D(YieldBeforeSweepingTypes, 20)      \
  D(YieldBeforeSweepingObjects, 21)    \
  D(YieldBeforeSweepingNonObjects, 22) \
  D(YieldBeforeSweepingShapeTrees, 23)

enum class ZealMode {
#define ZEAL_MODE(name, value) name = value,
  JS_FOR_EACH_ZEAL_MODE(ZEAL_MODE)
#undef ZEAL_MODE
      Count,
  Limit = Count - 1
};

} /* namespace gc */
} /* namespace js */

#endif /* gc_GCEnum_h */
