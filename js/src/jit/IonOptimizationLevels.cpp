/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/IonOptimizationLevels.h"

#include "jsanalyze.h"
#include "jsscript.h"

using namespace js;
using namespace js::jit;

namespace js {
namespace jit {

OptimizationInfos js_IonOptimizations;

void
OptimizationInfo::initNormalOptimizationInfo()
{
    level_ = Optimization_Normal;

    eaa_ = true;
    edgeCaseAnalysis_ = true;
    eliminateRedundantChecks_ = true;
    inlineInterpreted_ = true;
    inlineNative_ = true;
    gvn_ = true;
    gvnKind_ = GVN_Optimistic;
    licm_ = true;
    uce_ = true;
    rangeAnalysis_ = true;
    registerAllocator_ = RegisterAllocator_LSRA;

    inlineMaxTotalBytecodeLength_ = 1000;
    inliningMaxCallerBytecodeLength_ = 10000;
    maxInlineDepth_ = 3;
    smallFunctionMaxInlineDepth_ = 10;
    usesBeforeCompile_ = 1000;
    usesBeforeInliningFactor_ = 0.125;
}

void
OptimizationInfo::initAsmjsOptimizationInfo()
{
    // The AsmJS optimization level
    // Disables some passes that don't work well with asmjs.

    // Take normal option values for not specified values.
    initNormalOptimizationInfo();

    level_ = Optimization_AsmJS;
    edgeCaseAnalysis_ = false;
    eliminateRedundantChecks_ = false;
}

uint32_t
OptimizationInfo::usesBeforeCompile(JSScript *script, jsbytecode *pc) const
{
    JS_ASSERT(pc == nullptr || pc == script->code() || JSOp(*pc) == JSOP_LOOPENTRY);

    if (pc == script->code())
        pc = nullptr;

    uint32_t minUses = usesBeforeCompile_;
    if (js_JitOptions.forceDefaultIonUsesBeforeCompile)
        minUses = js_JitOptions.forcedDefaultIonUsesBeforeCompile;

    // If the script is too large to compile on the main thread, we can still
    // compile it off thread. In these cases, increase the use count threshold
    // to improve the compilation's type information and hopefully avoid later
    // recompilation.

    if (script->length() > MAX_MAIN_THREAD_SCRIPT_SIZE)
        minUses = minUses * (script->length() / (double) MAX_MAIN_THREAD_SCRIPT_SIZE);

    uint32_t numLocalsAndArgs = analyze::TotalSlots(script);
    if (numLocalsAndArgs > MAX_MAIN_THREAD_LOCALS_AND_ARGS)
        minUses = minUses * (numLocalsAndArgs / (double) MAX_MAIN_THREAD_LOCALS_AND_ARGS);

    if (!pc || js_JitOptions.eagerCompilation)
        return minUses;

    // It's more efficient to enter outer loops, rather than inner loops, via OSR.
    // To accomplish this, we use a slightly higher threshold for inner loops.
    // Note that the loop depth is always > 0 so we will prefer non-OSR over OSR.
    uint32_t loopDepth = LoopEntryDepthHint(pc);
    JS_ASSERT(loopDepth > 0);
    return minUses + loopDepth * 100;
}

OptimizationInfos::OptimizationInfos()
{
    infos_[Optimization_Normal - 1].initNormalOptimizationInfo();
    infos_[Optimization_AsmJS - 1].initAsmjsOptimizationInfo();

#ifdef DEBUG
    OptimizationLevel level = firstLevel();
    while (!isLastLevel(level)) {
        OptimizationLevel next = nextLevel(level);
        JS_ASSERT(level < next);
        level = next;
    }
#endif
}

OptimizationLevel
OptimizationInfos::nextLevel(OptimizationLevel level) const
{
    JS_ASSERT(!isLastLevel(level));
    switch (level) {
      case Optimization_DontCompile:
        return Optimization_Normal;
      default:
        MOZ_ASSUME_UNREACHABLE("Unknown optimization level.");
    }
}

OptimizationLevel
OptimizationInfos::firstLevel() const
{
    return nextLevel(Optimization_DontCompile);
}

bool
OptimizationInfos::isLastLevel(OptimizationLevel level) const
{
    return level == Optimization_Normal;
}

OptimizationLevel
OptimizationInfos::levelForScript(JSScript *script, jsbytecode *pc) const
{
    OptimizationLevel prev = Optimization_DontCompile;

    while (!isLastLevel(prev)) {
        OptimizationLevel level = nextLevel(prev);
        const OptimizationInfo *info = get(level);
        if (script->getUseCount() < info->usesBeforeCompile(script, pc))
            return prev;

        prev = level;
    }

    return prev;
}

} // namespace jit
} // namespace js
