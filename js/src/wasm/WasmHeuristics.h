/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef wasm_WasmHeuristics_h
#define wasm_WasmHeuristics_h

#include <math.h>

#include "js/Prefs.h"
#include "threading/ExclusiveData.h"
#include "vm/MutexIDs.h"

namespace js {
namespace wasm {

// Classes LazyTieringHeuristics and InliningHeuristics allow answering of
// simple questions relating to lazy tiering and inlining, eg, "is this
// function small enough to inline?"  They do not answer questions that involve
// carrying state (eg, remaining inlining budget) across multiple queries.
//
// Note also, they may be queried in parallel without locking, by multiple
// instantiating / compilation threads, and so must be immutable once created.

// For both LazyTieringHeuristics and InliningHeuristics, the default `level_`
// is set to 5 in modules/libpref/init/StaticPrefList.yaml.  The scaling
// factors and tables defined in this file have been set so as to give
// near-optimal performance on Barista-3 and another benchmark; they are
// generally within 2% of the best value that can be found by changing the
// `level_` numbers.  Further performance gains may depend on improving the
// accuracy of estimateIonCompilationCost().
//
// Performance was measured on a mid/high-end Intel CPU (Core i5-1135G7 --
// Tiger Lake) and a low end Intel (Celeron N3050 -- Goldmont).

class LazyTieringHeuristics {
  static constexpr uint32_t MIN_LEVEL = 1;
  static constexpr uint32_t MAX_LEVEL = 9;

  // A scaling table for levels 2 .. 8.  Levels 1 and 9 are special-cased.  In
  // this table, each value differs from its neighbour by a factor of 3, giving
  // a dynamic range in the table of 3 ^ 6 == 729, hence a wide selection of
  // tier-up aggressiveness.
  static constexpr float scale_[7] = {27.0,  9.0,   3.0,
                                      1.0,  // default
                                      0.333, 0.111, 0.037};

 public:
  // 1 = min (almost never, set tiering threshold to max possible, == 2^31-1)
  // 5 = default
  // 9 = max (request tier up at first call, set tiering threshold to zero)
  //
  // Don't use this directly, except for logging etc.
  static uint32_t rawLevel() {
    uint32_t level = JS::Prefs::wasm_lazy_tiering_level();
    // Clamp to range MIN_LEVEL .. MAX_LEVEL.
    level = std::max<uint32_t>(level, MIN_LEVEL);
    level = std::min<uint32_t>(level, MAX_LEVEL);
    return level;
  }

  // Estimate the cost of compiling a function of bytecode size `bodyLength`
  // using Ion, in terms of arbitrary work-units.  The baseline code for the
  // function counts down from the returned value as it runs.  When the value
  // goes negative it requests tier-up.  See "[SMDOC] WebAssembly baseline
  // compiler -- Lazy Tier-Up mechanism" in WasmBaselineCompile.cpp.

  static int32_t estimateIonCompilationCost(uint32_t bodyLength) {
    uint32_t level = rawLevel();
    if (MOZ_LIKELY(MIN_LEVEL < level && level < MAX_LEVEL)) {
      // The estimated cost, in X86_64 insns, for Ion compilation:
      // 30k up-front cost + 4k per bytecode byte.
      //
      // This is derived from measurements of an optimized build of Ion
      // compiling about 99000 functions.  Each estimate is pretty bad, but
      // averaged over a number of functions it's often within 20% of correct.
      // However, this is with no inlining; that causes a much wider variance
      // of costs.  This will need to be revisited at some point.
      float thresholdF = 30000.0 + 4000.0 * float(bodyLength);

      // Rescale to step-down work units, so that the default `level` setting
      // (5) gives pretty good results.
      thresholdF *= 0.25;

      // Rescale again to take into account `level`.
      thresholdF *= scale_[level - (MIN_LEVEL + 1)];

      // Clamp and convert.
      thresholdF = std::max<float>(thresholdF, 10.0);   // at least 10
      thresholdF = std::min<float>(thresholdF, 2.0e9);  // at most 2 billion
      int32_t thresholdI = int32_t(thresholdF);
      MOZ_RELEASE_ASSERT(thresholdI >= 0);
      return thresholdI;
    }
    if (level == MIN_LEVEL) {
      // "almost never tier up"; produce our closest approximation to infinity
      return INT32_MAX;
    }
    if (level == MAX_LEVEL) {
      // request tier up at the first call; return the lowest possible value
      return 0;
    }
    MOZ_CRASH();
  }
};

class InliningHeuristics {
  static constexpr uint32_t MIN_LEVEL = 1;
  static constexpr uint32_t MAX_LEVEL = 9;

 public:
  // 1 = no inlining allowed
  // 2 = min (minimal inlining)
  // 5 = default
  // 9 = max (very aggressive inlining)
  //
  // Don't use these directly, except for logging etc.
  static uint32_t rawLevel() {
    uint32_t level = JS::Prefs::wasm_inlining_level();
    // Clamp to range MIN_LEVEL .. MAX_LEVEL.
    level = std::max<uint32_t>(level, MIN_LEVEL);
    level = std::min<uint32_t>(level, MAX_LEVEL);
    return level;
  }
  static bool rawDirectAllowed() { return JS::Prefs::wasm_direct_inlining(); }
  static bool rawCallRefAllowed() {
    return JS::Prefs::wasm_call_ref_inlining();
  }
  // For a call_ref site, returns the percentage of total calls made by that
  // site, that any single target has to make in order to be considered as a
  // candidate for speculative inlining.
  static uint32_t rawCallRefPercent() {
    uint32_t percent = JS::Prefs::wasm_call_ref_inlining_percent();
    // Clamp to range 10 .. 100 (%).
    percent = std::max<uint32_t>(10, percent);
    percent = std::min<uint32_t>(100, percent);
    return percent;
  }

  // Given a call of kind `callKind` to a function of bytecode size
  // `bodyLength` at `inliningDepth`, decide whether the it is allowable to
  // inline the call.  Note that `inliningDepth` starts at zero, not one.  In
  // other words, a value of zero means the query relates to a function which
  // (if approved) would be inlined into the top-level function currently being
  // compiled.
  enum class CallKind { Direct, CallRef };
  static bool isSmallEnoughToInline(CallKind callKind, uint32_t inliningDepth,
                                    uint32_t bodyLength) {
    // If this fails, something's seriously wrong; bail out.
    MOZ_RELEASE_ASSERT(inliningDepth <= 10);  // because 10 > (400 / 50)
    // Check whether calls of this kind are currently allowed
    if ((callKind == CallKind::Direct && !rawDirectAllowed()) ||
        (callKind == CallKind::CallRef && !rawCallRefAllowed())) {
      return false;
    }
    // Check the size is allowable.  This depends on how deep we are in the
    // stack and on the setting of level_.  We allow inlining of functions of
    // size up to the `baseSize[]` value at depth zero, but reduce the
    // allowable size by 50 for each further level of inlining, so that only
    // smaller and smaller functions are allowed as we inline deeper.
    //
    // At some point `allowedSize` goes negative and thereby disallows all
    // further inlining.  Note that the `baseSize` entry for
    // `level_ == MIN_LEVEL (== 1)` is set so as to disallow inlining even at
    // depth zero.  Hence `level_ == MIN_LEVEL` disallows all inlining.
    static constexpr int32_t baseSize[9] = {0,   50,  100, 150,
                                            200,  // default
                                            250, 300, 350, 400};
    uint32_t level = rawLevel();
    MOZ_RELEASE_ASSERT(level >= MIN_LEVEL && level <= MAX_LEVEL);
    int32_t allowedSize = baseSize[level - MIN_LEVEL];
    allowedSize -= int32_t(50 * inliningDepth);
    return allowedSize > 0 && bodyLength <= uint32_t(allowedSize);
  }
};

}  // namespace wasm
}  // namespace js

#endif /* wasm_WasmHeuristics_h */
