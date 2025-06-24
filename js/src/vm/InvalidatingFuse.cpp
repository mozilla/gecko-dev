/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/InvalidatingFuse.h"

#include "gc/PublicIterators.h"
#include "jit/Invalidation.h"
#include "jit/JitSpewer.h"
#include "vm/JSContext.h"
#include "vm/JSScript.h"
#include "vm/Logging.h"

#include "gc/StableCellHasher-inl.h"
#include "vm/JSScript-inl.h"

js::FuseDependentIonScriptSet::FuseDependentIonScriptSet(JSContext* cx,
                                                         InvalidatingFuse* fuse)
    : associatedFuse(fuse), ionScripts(cx->runtime()) {}

bool js::InvalidatingRuntimeFuse::addFuseDependency(
    JSContext* cx, const jit::IonScriptKey& ionScript) {
  MOZ_ASSERT(ionScript.script()->zone() == cx->zone());

  auto* scriptSet =
      cx->zone()->fuseDependencies.getOrCreateDependentScriptSet(cx, this);
  if (!scriptSet) {
    return false;
  }

  return scriptSet->addScriptForFuse(this, ionScript);
}

void js::InvalidatingRuntimeFuse::popFuse(JSContext* cx) {
  // Pop the fuse in the base class
  GuardFuse::popFuse(cx);
  JS_LOG(fuseInvalidation, Verbose, "Invalidating fuse popping: %s", name());
  // do invalidation.
  for (AllZonesIter z(cx->runtime()); !z.done(); z.next()) {
    // There's one dependent script set per fuse; just iterate over them all to
    // find the one we need (see comment on JS::Zone::fuseDependencies for
    // reasoning).
    for (auto& fd : z.get()->fuseDependencies) {
      fd.invalidateForFuse(cx, this);
    }
  }
}

void js::FuseDependentIonScriptSet::invalidateForFuse(JSContext* cx,
                                                      InvalidatingFuse* fuse) {
  if (associatedFuse != fuse) {
    return;
  }
  ionScripts.get().invalidateAndClear(cx, "fuse");
}

void js::jit::DependentIonScriptSet::invalidateAndClear(JSContext* cx,
                                                        const char* reason) {
  for (const auto& ionScriptKey : ionScripts_) {
    IonScript* ionScript = ionScriptKey.maybeIonScriptToInvalidate();
    if (ionScript) {
      JSScript* script = ionScriptKey.script();
      JitSpew(jit::JitSpew_IonInvalidate, "Invalidating ion script %p for %s",
              ionScript, reason);
      JS_LOG(fuseInvalidation, Debug,
             "Invalidating ion script %s:%d for reason %s", script->filename(),
             script->lineno(), reason);
    }
  }
  js::jit::Invalidate(cx, ionScripts_);
  ionScripts_.clearAndFree();
}

bool js::FuseDependentIonScriptSet::addScriptForFuse(
    InvalidatingFuse* fuse, const jit::IonScriptKey& ionScript) {
  MOZ_ASSERT(fuse == associatedFuse);
  return ionScripts.get().addToSet(ionScript);
}

js::FuseDependentIonScriptSet*
js::DependentIonScriptGroup::getOrCreateDependentScriptSet(
    JSContext* cx, js::InvalidatingFuse* fuse) {
  for (auto& dss : dependencies) {
    if (dss.associatedFuse == fuse) {
      return &dss;
    }
  }

  if (!dependencies.emplaceBack(cx, fuse)) {
    return nullptr;
  }

  auto& dss = dependencies.back();
  MOZ_ASSERT(dss.associatedFuse == fuse);
  return &dss;
}

bool js::jit::DependentIonScriptSet::addToSet(const IonScriptKey& ionScript) {
  MOZ_ASSERT(lengthAfterLastCompaction_ <= ionScripts_.length());

  // If `ionScript` is already in the vector, it must be the last entry.
  if (!ionScripts_.empty() && ionScripts_.back() == ionScript) {
    return true;
  }

  // Assert `ionScript` is not in the vector. Limit this to the last 8 entries
  // to not slow down debug builds too much.
#ifdef DEBUG
  size_t numToCheck = std::min<size_t>(ionScripts_.length(), 8);
  for (size_t i = 0; i < numToCheck; i++) {
    MOZ_ASSERT(ionScripts_[ionScripts_.length() - 1 - i] != ionScript);
  }
#endif

  // Compact the vector if its size doubled since the last compaction. This
  // isn't required for correctness but it avoids keeping a lot of stale
  // entries around between GCs.
  if (ionScripts_.length() / 2 > lengthAfterLastCompaction_) {
    ionScripts_.eraseIf([](const IonScriptKey& ionScript) {
      return ionScript.maybeIonScriptToInvalidate() == nullptr;
    });
    lengthAfterLastCompaction_ = ionScripts_.length();
  }

  return ionScripts_.append(ionScript);
}
