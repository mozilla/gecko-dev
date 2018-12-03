/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "js/MemoryMetrics.h"

#include "gc/GC.h"
#include "gc/Heap.h"
#include "gc/Nursery.h"
#include "gc/PublicIterators.h"
#include "jit/BaselineJIT.h"
#include "jit/Ion.h"
#include "vm/ArrayObject.h"
#ifdef ENABLE_BIGINT
#include "vm/BigIntType.h"
#endif
#include "vm/HelperThreads.h"
#include "vm/JSObject.h"
#include "vm/JSScript.h"
#include "vm/Realm.h"
#include "vm/Runtime.h"
#include "vm/Shape.h"
#include "vm/StringType.h"
#include "vm/SymbolType.h"
#include "vm/WrapperObject.h"
#include "wasm/WasmInstance.h"
#include "wasm/WasmJS.h"
#include "wasm/WasmModule.h"

using mozilla::MallocSizeOf;
using mozilla::PodCopy;

using namespace js;

using JS::ObjectPrivateVisitor;
using JS::RealmStats;
using JS::RuntimeStats;
using JS::ZoneStats;

namespace js {

JS_FRIEND_API size_t MemoryReportingSundriesThreshold() { return 8 * 1024; }

template <typename CharT>
static uint32_t HashStringChars(JSString* s) {
  uint32_t hash = 0;
  if (s->isLinear()) {
    JS::AutoCheckCannotGC nogc;
    const CharT* chars = s->asLinear().chars<CharT>(nogc);
    hash = mozilla::HashString(chars, s->length());
  } else {
    // Use rope's non-copying hash function.
    if (!s->asRope().hash(&hash)) {
      MOZ_CRASH("oom");
    }
  }

  return hash;
}

/* static */ HashNumber InefficientNonFlatteningStringHashPolicy::hash(
    const Lookup& l) {
  return l->hasLatin1Chars() ? HashStringChars<Latin1Char>(l)
                             : HashStringChars<char16_t>(l);
}

template <typename Char1, typename Char2>
static bool EqualStringsPure(JSString* s1, JSString* s2) {
  if (s1->length() != s2->length()) {
    return false;
  }

  const Char1* c1;
  UniquePtr<Char1[], JS::FreePolicy> ownedChars1;
  JS::AutoCheckCannotGC nogc;
  if (s1->isLinear()) {
    c1 = s1->asLinear().chars<Char1>(nogc);
  } else {
    ownedChars1 = s1->asRope().copyChars<Char1>(/* tcx */ nullptr);
    if (!ownedChars1) {
      MOZ_CRASH("oom");
    }
    c1 = ownedChars1.get();
  }

  const Char2* c2;
  UniquePtr<Char2[], JS::FreePolicy> ownedChars2;
  if (s2->isLinear()) {
    c2 = s2->asLinear().chars<Char2>(nogc);
  } else {
    ownedChars2 = s2->asRope().copyChars<Char2>(/* tcx */ nullptr);
    if (!ownedChars2) {
      MOZ_CRASH("oom");
    }
    c2 = ownedChars2.get();
  }

  return EqualChars(c1, c2, s1->length());
}

/* static */ bool InefficientNonFlatteningStringHashPolicy::match(
    const JSString* const& k, const Lookup& l) {
  // We can't use js::EqualStrings, because that flattens our strings.
  JSString* s1 = const_cast<JSString*>(k);
  if (k->hasLatin1Chars()) {
    return l->hasLatin1Chars() ? EqualStringsPure<Latin1Char, Latin1Char>(s1, l)
                               : EqualStringsPure<Latin1Char, char16_t>(s1, l);
  }

  return l->hasLatin1Chars() ? EqualStringsPure<char16_t, Latin1Char>(s1, l)
                             : EqualStringsPure<char16_t, char16_t>(s1, l);
}

}  // namespace js

namespace JS {

NotableStringInfo::NotableStringInfo() : StringInfo(), buffer(0), length(0) {}

template <typename CharT>
static void StoreStringChars(char* buffer, size_t bufferSize, JSString* str) {
  const CharT* chars;
  UniquePtr<CharT[], JS::FreePolicy> ownedChars;
  JS::AutoCheckCannotGC nogc;
  if (str->isLinear()) {
    chars = str->asLinear().chars<CharT>(nogc);
  } else {
    ownedChars = str->asRope().copyChars<CharT>(/* tcx */ nullptr);
    if (!ownedChars) {
      MOZ_CRASH("oom");
    }
    chars = ownedChars.get();
  }

  // We might truncate |str| even if it's much shorter than 1024 chars, if
  // |str| contains unicode chars.  Since this is just for a memory reporter,
  // we don't care.
  PutEscapedString(buffer, bufferSize, chars, str->length(), /* quote */ 0);
}

NotableStringInfo::NotableStringInfo(JSString* str, const StringInfo& info)
    : StringInfo(info), length(str->length()) {
  size_t bufferSize = Min(str->length() + 1, size_t(MAX_SAVED_CHARS));
  buffer = js_pod_malloc<char>(bufferSize);
  if (!buffer) {
    MOZ_CRASH("oom");
  }

  if (str->hasLatin1Chars()) {
    StoreStringChars<Latin1Char>(buffer, bufferSize, str);
  } else {
    StoreStringChars<char16_t>(buffer, bufferSize, str);
  }
}

NotableStringInfo::NotableStringInfo(NotableStringInfo&& info)
    : StringInfo(std::move(info)), length(info.length) {
  buffer = info.buffer;
  info.buffer = nullptr;
}

NotableStringInfo& NotableStringInfo::operator=(NotableStringInfo&& info) {
  MOZ_ASSERT(this != &info, "self-move assignment is prohibited");
  this->~NotableStringInfo();
  new (this) NotableStringInfo(std::move(info));
  return *this;
}

NotableClassInfo::NotableClassInfo() : ClassInfo(), className_(nullptr) {}

NotableClassInfo::NotableClassInfo(const char* className, const ClassInfo& info)
    : ClassInfo(info) {
  size_t bytes = strlen(className) + 1;
  className_ = js_pod_malloc<char>(bytes);
  if (!className_) {
    MOZ_CRASH("oom");
  }
  PodCopy(className_, className, bytes);
}

NotableClassInfo::NotableClassInfo(NotableClassInfo&& info)
    : ClassInfo(std::move(info)) {
  className_ = info.className_;
  info.className_ = nullptr;
}

NotableClassInfo& NotableClassInfo::operator=(NotableClassInfo&& info) {
  MOZ_ASSERT(this != &info, "self-move assignment is prohibited");
  this->~NotableClassInfo();
  new (this) NotableClassInfo(std::move(info));
  return *this;
}

NotableScriptSourceInfo::NotableScriptSourceInfo()
    : ScriptSourceInfo(), filename_(nullptr) {}

NotableScriptSourceInfo::NotableScriptSourceInfo(const char* filename,
                                                 const ScriptSourceInfo& info)
    : ScriptSourceInfo(info) {
  size_t bytes = strlen(filename) + 1;
  filename_ = js_pod_malloc<char>(bytes);
  if (!filename_) {
    MOZ_CRASH("oom");
  }
  PodCopy(filename_, filename, bytes);
}

NotableScriptSourceInfo::NotableScriptSourceInfo(NotableScriptSourceInfo&& info)
    : ScriptSourceInfo(std::move(info)) {
  filename_ = info.filename_;
  info.filename_ = nullptr;
}

NotableScriptSourceInfo& NotableScriptSourceInfo::operator=(
    NotableScriptSourceInfo&& info) {
  MOZ_ASSERT(this != &info, "self-move assignment is prohibited");
  this->~NotableScriptSourceInfo();
  new (this) NotableScriptSourceInfo(std::move(info));
  return *this;
}

}  // namespace JS

typedef HashSet<ScriptSource*, DefaultHasher<ScriptSource*>, SystemAllocPolicy>
    SourceSet;

struct StatsClosure {
  RuntimeStats* rtStats;
  ObjectPrivateVisitor* opv;
  SourceSet seenSources;
  wasm::Metadata::SeenSet wasmSeenMetadata;
  wasm::ShareableBytes::SeenSet wasmSeenBytes;
  wasm::Code::SeenSet wasmSeenCode;
  wasm::Table::SeenSet wasmSeenTables;
  bool anonymize;

  StatsClosure(RuntimeStats* rt, ObjectPrivateVisitor* v, bool anon)
      : rtStats(rt), opv(v), anonymize(anon) {}
};

static void DecommittedArenasChunkCallback(JSRuntime* rt, void* data,
                                           gc::Chunk* chunk) {
  // This case is common and fast to check.  Do it first.
  if (chunk->decommittedArenas.isAllClear()) {
    return;
  }

  size_t n = 0;
  for (size_t i = 0; i < gc::ArenasPerChunk; i++) {
    if (chunk->decommittedArenas.get(i)) {
      n += gc::ArenaSize;
    }
  }
  MOZ_ASSERT(n > 0);
  *static_cast<size_t*>(data) += n;
}

static void StatsZoneCallback(JSRuntime* rt, void* data, Zone* zone) {
  // Append a new RealmStats to the vector.
  RuntimeStats* rtStats = static_cast<StatsClosure*>(data)->rtStats;

  // CollectRuntimeStats reserves enough space.
  MOZ_ALWAYS_TRUE(rtStats->zoneStatsVector.growBy(1));
  ZoneStats& zStats = rtStats->zoneStatsVector.back();
  if (!zStats.initStrings()) {
    MOZ_CRASH("oom");
  }
  rtStats->initExtraZoneStats(zone, &zStats);
  rtStats->currZoneStats = &zStats;

  zone->addSizeOfIncludingThis(
      rtStats->mallocSizeOf_, &zStats.typePool, &zStats.regexpZone,
      &zStats.jitZone, &zStats.baselineStubsOptimized, &zStats.cachedCFG,
      &zStats.uniqueIdMap, &zStats.shapeTables,
      &rtStats->runtime.atomsMarkBitmaps, &zStats.compartmentObjects,
      &zStats.crossCompartmentWrappersTables, &zStats.compartmentsPrivateData);
}

static void StatsRealmCallback(JSContext* cx, void* data,
                               Handle<Realm*> realm) {
  // Append a new RealmStats to the vector.
  RuntimeStats* rtStats = static_cast<StatsClosure*>(data)->rtStats;

  // CollectRuntimeStats reserves enough space.
  MOZ_ALWAYS_TRUE(rtStats->realmStatsVector.growBy(1));
  RealmStats& realmStats = rtStats->realmStatsVector.back();
  if (!realmStats.initClasses()) {
    MOZ_CRASH("oom");
  }
  rtStats->initExtraRealmStats(realm, &realmStats);

  realm->setRealmStats(&realmStats);

  // Measure the realm object itself, and things hanging off it.
  realm->addSizeOfIncludingThis(
      rtStats->mallocSizeOf_, &realmStats.typeInferenceAllocationSiteTables,
      &realmStats.typeInferenceArrayTypeTables,
      &realmStats.typeInferenceObjectTypeTables, &realmStats.realmObject,
      &realmStats.realmTables, &realmStats.innerViewsTable,
      &realmStats.lazyArrayBuffersTable, &realmStats.objectMetadataTable,
      &realmStats.savedStacksSet, &realmStats.varNamesSet,
      &realmStats.nonSyntacticLexicalScopesTable, &realmStats.jitRealm,
      &realmStats.scriptCountsMap);
}

static void StatsArenaCallback(JSRuntime* rt, void* data, gc::Arena* arena,
                               JS::TraceKind traceKind, size_t thingSize) {
  RuntimeStats* rtStats = static_cast<StatsClosure*>(data)->rtStats;

  // The admin space includes (a) the header fields and (b) the padding
  // between the end of the header fields and the first GC thing.
  size_t allocationSpace = gc::Arena::thingsSpan(arena->getAllocKind());
  rtStats->currZoneStats->gcHeapArenaAdmin += gc::ArenaSize - allocationSpace;

  // We don't call the callback on unused things.  So we compute the
  // unused space like this:  arenaUnused = maxArenaUnused - arenaUsed.
  // We do this by setting arenaUnused to maxArenaUnused here, and then
  // subtracting thingSize for every used cell, in StatsCellCallback().
  rtStats->currZoneStats->unusedGCThings.addToKind(traceKind, allocationSpace);
}

// FineGrained is used for normal memory reporting.  CoarseGrained is used by
// AddSizeOfTab(), which aggregates all the measurements into a handful of
// high-level numbers, which means that fine-grained reporting would be a waste
// of effort.
enum Granularity { FineGrained, CoarseGrained };

static void AddClassInfo(Granularity granularity, RealmStats& realmStats,
                         const char* className, JS::ClassInfo& info) {
  if (granularity == FineGrained) {
    if (!className) {
      className = "<no class name>";
    }
    RealmStats::ClassesHashMap::AddPtr p =
        realmStats.allClasses->lookupForAdd(className);
    if (!p) {
      bool ok = realmStats.allClasses->add(p, className, info);
      // Ignore failure -- we just won't record the
      // object/shape/base-shape as notable.
      (void)ok;
    } else {
      p->value().add(info);
    }
  }
}

template <Granularity granularity>
static void CollectScriptSourceStats(StatsClosure* closure, ScriptSource* ss) {
  RuntimeStats* rtStats = closure->rtStats;

  SourceSet::AddPtr entry = closure->seenSources.lookupForAdd(ss);
  if (entry) {
    return;
  }

  bool ok = closure->seenSources.add(entry, ss);
  (void)ok;  // Not much to be done on failure.

  JS::ScriptSourceInfo info;  // This zeroes all the sizes.
  ss->addSizeOfIncludingThis(rtStats->mallocSizeOf_, &info);

  rtStats->runtime.scriptSourceInfo.add(info);

  if (granularity == FineGrained) {
    const char* filename = ss->filename();
    if (!filename) {
      filename = "<no filename>";
    }

    JS::RuntimeSizes::ScriptSourcesHashMap::AddPtr p =
        rtStats->runtime.allScriptSources->lookupForAdd(filename);
    if (!p) {
      bool ok = rtStats->runtime.allScriptSources->add(p, filename, info);
      // Ignore failure -- we just won't record the script source as notable.
      (void)ok;
    } else {
      p->value().add(info);
    }
  }
}

// The various kinds of hashing are expensive, and the results are unused when
// doing coarse-grained measurements. Skipping them more than doubles the
// profile speed for complex pages such as gmail.com.
template <Granularity granularity>
static void StatsCellCallback(JSRuntime* rt, void* data, void* thing,
                              JS::TraceKind traceKind, size_t thingSize) {
  StatsClosure* closure = static_cast<StatsClosure*>(data);
  RuntimeStats* rtStats = closure->rtStats;
  ZoneStats* zStats = rtStats->currZoneStats;
  switch (traceKind) {
    case JS::TraceKind::Object: {
      JSObject* obj = static_cast<JSObject*>(thing);
      RealmStats& realmStats = obj->maybeCCWRealm()->realmStats();
      JS::ClassInfo info;  // This zeroes all the sizes.
      info.objectsGCHeap += thingSize;

      obj->addSizeOfExcludingThis(rtStats->mallocSizeOf_, &info);

      // These classes require special handling due to shared resources which
      // we must be careful not to report twice.
      if (obj->is<WasmModuleObject>()) {
        const wasm::Module& module = obj->as<WasmModuleObject>().module();
        if (ScriptSource* ss = module.metadata().maybeScriptSource()) {
          CollectScriptSourceStats<granularity>(closure, ss);
        }
        module.addSizeOfMisc(rtStats->mallocSizeOf_, &closure->wasmSeenMetadata,
                             &closure->wasmSeenBytes, &closure->wasmSeenCode,
                             &info.objectsNonHeapCodeWasm,
                             &info.objectsMallocHeapMisc);
      } else if (obj->is<WasmInstanceObject>()) {
        wasm::Instance& instance = obj->as<WasmInstanceObject>().instance();
        if (ScriptSource* ss = instance.metadata().maybeScriptSource()) {
          CollectScriptSourceStats<granularity>(closure, ss);
        }
        instance.addSizeOfMisc(
            rtStats->mallocSizeOf_, &closure->wasmSeenMetadata,
            &closure->wasmSeenBytes, &closure->wasmSeenCode,
            &closure->wasmSeenTables, &info.objectsNonHeapCodeWasm,
            &info.objectsMallocHeapMisc);
      }

      realmStats.classInfo.add(info);

      const Class* clasp = obj->getClass();
      const char* className = clasp->name;
      AddClassInfo(granularity, realmStats, className, info);

      if (ObjectPrivateVisitor* opv = closure->opv) {
        nsISupports* iface;
        if (opv->getISupports_(obj, &iface) && iface) {
          realmStats.objectsPrivate += opv->sizeOfIncludingThis(iface);
        }
      }
      break;
    }

    case JS::TraceKind::Script: {
      JSScript* script = static_cast<JSScript*>(thing);
      RealmStats& realmStats = script->realm()->realmStats();
      realmStats.scriptsGCHeap += thingSize;
      realmStats.scriptsMallocHeapData +=
          script->sizeOfData(rtStats->mallocSizeOf_);
      realmStats.typeInferenceTypeScripts +=
          script->sizeOfTypeScript(rtStats->mallocSizeOf_);
      jit::AddSizeOfBaselineData(script, rtStats->mallocSizeOf_,
                                 &realmStats.baselineData,
                                 &realmStats.baselineStubsFallback);
      realmStats.ionData += jit::SizeOfIonData(script, rtStats->mallocSizeOf_);
      CollectScriptSourceStats<granularity>(closure, script->scriptSource());
      break;
    }

    case JS::TraceKind::String: {
      JSString* str = static_cast<JSString*>(thing);
      size_t size = thingSize;
      if (!str->isTenured()) {
        size += Nursery::stringHeaderSize();
      }

      JS::StringInfo info;
      if (str->hasLatin1Chars()) {
        info.gcHeapLatin1 = size;
        info.mallocHeapLatin1 =
            str->sizeOfExcludingThis(rtStats->mallocSizeOf_);
      } else {
        info.gcHeapTwoByte = size;
        info.mallocHeapTwoByte =
            str->sizeOfExcludingThis(rtStats->mallocSizeOf_);
      }
      info.numCopies = 1;

      zStats->stringInfo.add(info);

      // The primary use case for anonymization is automated crash submission
      // (to help detect OOM crashes). In that case, we don't want to pay the
      // memory cost required to do notable string detection.
      if (granularity == FineGrained && !closure->anonymize) {
        ZoneStats::StringsHashMap::AddPtr p =
            zStats->allStrings->lookupForAdd(str);
        if (!p) {
          bool ok = zStats->allStrings->add(p, str, info);
          // Ignore failure -- we just won't record the string as notable.
          (void)ok;
        } else {
          p->value().add(info);
        }
      }
      break;
    }

    case JS::TraceKind::Symbol:
      zStats->symbolsGCHeap += thingSize;
      break;

#ifdef ENABLE_BIGINT
    case JS::TraceKind::BigInt: {
      JS::BigInt* bi = static_cast<BigInt*>(thing);
      zStats->bigIntsGCHeap += thingSize;
      zStats->bigIntsMallocHeap +=
          bi->sizeOfExcludingThis(rtStats->mallocSizeOf_);
      break;
    }
#endif

    case JS::TraceKind::BaseShape: {
      JS::ShapeInfo info;  // This zeroes all the sizes.
      info.shapesGCHeapBase += thingSize;
      // No malloc-heap measurements.

      zStats->shapeInfo.add(info);
      break;
    }

    case JS::TraceKind::JitCode: {
      zStats->jitCodesGCHeap += thingSize;
      // The code for a script is counted in ExecutableAllocator::sizeOfCode().
      break;
    }

    case JS::TraceKind::LazyScript: {
      LazyScript* lazy = static_cast<LazyScript*>(thing);
      zStats->lazyScriptsGCHeap += thingSize;
      zStats->lazyScriptsMallocHeap +=
          lazy->sizeOfExcludingThis(rtStats->mallocSizeOf_);
      break;
    }

    case JS::TraceKind::Shape: {
      Shape* shape = static_cast<Shape*>(thing);

      JS::ShapeInfo info;  // This zeroes all the sizes.
      if (shape->inDictionary()) {
        info.shapesGCHeapDict += thingSize;
      } else {
        info.shapesGCHeapTree += thingSize;
      }
      shape->addSizeOfExcludingThis(rtStats->mallocSizeOf_, &info);
      zStats->shapeInfo.add(info);
      break;
    }

    case JS::TraceKind::ObjectGroup: {
      ObjectGroup* group = static_cast<ObjectGroup*>(thing);
      zStats->objectGroupsGCHeap += thingSize;
      zStats->objectGroupsMallocHeap +=
          group->sizeOfExcludingThis(rtStats->mallocSizeOf_);
      break;
    }

    case JS::TraceKind::Scope: {
      Scope* scope = static_cast<Scope*>(thing);
      zStats->scopesGCHeap += thingSize;
      zStats->scopesMallocHeap +=
          scope->sizeOfExcludingThis(rtStats->mallocSizeOf_);
      break;
    }

    case JS::TraceKind::RegExpShared: {
      auto regexp = static_cast<RegExpShared*>(thing);
      zStats->regExpSharedsGCHeap += thingSize;
      zStats->regExpSharedsMallocHeap +=
          regexp->sizeOfExcludingThis(rtStats->mallocSizeOf_);
      break;
    }

    default:
      MOZ_CRASH("invalid traceKind in StatsCellCallback");
  }

  // Yes, this is a subtraction:  see StatsArenaCallback() for details.
  zStats->unusedGCThings.addToKind(traceKind, -thingSize);
}

bool ZoneStats::initStrings() {
  isTotals = false;
  allStrings = js_new<StringsHashMap>();
  if (!allStrings) {
    js_delete(allStrings);
    allStrings = nullptr;
    return false;
  }
  return true;
}

bool RealmStats::initClasses() {
  isTotals = false;
  allClasses = js_new<ClassesHashMap>();
  if (!allClasses) {
    js_delete(allClasses);
    allClasses = nullptr;
    return false;
  }
  return true;
}

static bool FindNotableStrings(ZoneStats& zStats) {
  using namespace JS;

  // We should only run FindNotableStrings once per ZoneStats object.
  MOZ_ASSERT(zStats.notableStrings.empty());

  for (ZoneStats::StringsHashMap::Range r = zStats.allStrings->all();
       !r.empty(); r.popFront()) {
    JSString* str = r.front().key();
    StringInfo& info = r.front().value();

    if (!info.isNotable()) {
      continue;
    }

    if (!zStats.notableStrings.growBy(1)) {
      return false;
    }

    zStats.notableStrings.back() = NotableStringInfo(str, info);

    // We're moving this string from a non-notable to a notable bucket, so
    // subtract it out of the non-notable tallies.
    zStats.stringInfo.subtract(info);
  }
  // Delete |allStrings| now, rather than waiting for zStats's destruction,
  // to reduce peak memory consumption during reporting.
  js_delete(zStats.allStrings);
  zStats.allStrings = nullptr;
  return true;
}

static bool FindNotableClasses(RealmStats& realmStats) {
  using namespace JS;

  // We should only run FindNotableClasses once per ZoneStats object.
  MOZ_ASSERT(realmStats.notableClasses.empty());

  for (RealmStats::ClassesHashMap::Range r = realmStats.allClasses->all();
       !r.empty(); r.popFront()) {
    const char* className = r.front().key();
    ClassInfo& info = r.front().value();

    // If this class isn't notable, or if we can't grow the notableStrings
    // vector, skip this string.
    if (!info.isNotable()) {
      continue;
    }

    if (!realmStats.notableClasses.growBy(1)) {
      return false;
    }

    realmStats.notableClasses.back() = NotableClassInfo(className, info);

    // We're moving this class from a non-notable to a notable bucket, so
    // subtract it out of the non-notable tallies.
    realmStats.classInfo.subtract(info);
  }
  // Delete |allClasses| now, rather than waiting for zStats's destruction,
  // to reduce peak memory consumption during reporting.
  js_delete(realmStats.allClasses);
  realmStats.allClasses = nullptr;
  return true;
}

static bool FindNotableScriptSources(JS::RuntimeSizes& runtime) {
  using namespace JS;

  // We should only run FindNotableScriptSources once per RuntimeSizes.
  MOZ_ASSERT(runtime.notableScriptSources.empty());

  for (RuntimeSizes::ScriptSourcesHashMap::Range r =
           runtime.allScriptSources->all();
       !r.empty(); r.popFront()) {
    const char* filename = r.front().key();
    ScriptSourceInfo& info = r.front().value();

    if (!info.isNotable()) {
      continue;
    }

    if (!runtime.notableScriptSources.growBy(1)) {
      return false;
    }

    runtime.notableScriptSources.back() =
        NotableScriptSourceInfo(filename, info);

    // We're moving this script source from a non-notable to a notable
    // bucket, so subtract its sizes from the non-notable tallies.
    runtime.scriptSourceInfo.subtract(info);
  }
  // Delete |allScriptSources| now, rather than waiting for zStats's
  // destruction, to reduce peak memory consumption during reporting.
  js_delete(runtime.allScriptSources);
  runtime.allScriptSources = nullptr;
  return true;
}

static bool CollectRuntimeStatsHelper(JSContext* cx, RuntimeStats* rtStats,
                                      ObjectPrivateVisitor* opv, bool anonymize,
                                      IterateCellCallback statsCellCallback) {
  JSRuntime* rt = cx->runtime();
  if (!rtStats->realmStatsVector.reserve(rt->numRealms)) {
    return false;
  }

  size_t totalZones = rt->gc.zones().length() + 1;  // + 1 for the atoms zone.
  if (!rtStats->zoneStatsVector.reserve(totalZones)) {
    return false;
  }

  rtStats->gcHeapChunkTotal =
      size_t(JS_GetGCParameter(cx, JSGC_TOTAL_CHUNKS)) * gc::ChunkSize;

  rtStats->gcHeapUnusedChunks =
      size_t(JS_GetGCParameter(cx, JSGC_UNUSED_CHUNKS)) * gc::ChunkSize;

  IterateChunks(cx, &rtStats->gcHeapDecommittedArenas,
                DecommittedArenasChunkCallback);

  // Take the per-compartment measurements.
  StatsClosure closure(rtStats, opv, anonymize);
  IterateHeapUnbarriered(cx, &closure, StatsZoneCallback, StatsRealmCallback,
                         StatsArenaCallback, statsCellCallback);

  // Take the "explicit/js/runtime/" measurements.
  rt->addSizeOfIncludingThis(rtStats->mallocSizeOf_, &rtStats->runtime);

  if (!FindNotableScriptSources(rtStats->runtime)) {
    return false;
  }

  JS::ZoneStatsVector& zs = rtStats->zoneStatsVector;
  ZoneStats& zTotals = rtStats->zTotals;

  // We don't look for notable strings for zTotals. So we first sum all the
  // zones' measurements to get the totals. Then we find the notable strings
  // within each zone.
  for (size_t i = 0; i < zs.length(); i++) {
    zTotals.addSizes(zs[i]);
  }

  for (size_t i = 0; i < zs.length(); i++) {
    if (!FindNotableStrings(zs[i])) {
      return false;
    }
  }

  MOZ_ASSERT(!zTotals.allStrings);

  JS::RealmStatsVector& realmStats = rtStats->realmStatsVector;
  RealmStats& realmTotals = rtStats->realmTotals;

  // As with the zones, we sum all realms first, and then get the
  // notable classes within each zone.
  for (size_t i = 0; i < realmStats.length(); i++) {
    realmTotals.addSizes(realmStats[i]);
  }

  for (size_t i = 0; i < realmStats.length(); i++) {
    if (!FindNotableClasses(realmStats[i])) {
      return false;
    }
  }

  MOZ_ASSERT(!realmTotals.allClasses);

  rtStats->gcHeapGCThings = rtStats->zTotals.sizeOfLiveGCThings() +
                            rtStats->realmTotals.sizeOfLiveGCThings();

#ifdef DEBUG
  // Check that the in-arena measurements look ok.
  size_t totalArenaSize = rtStats->zTotals.gcHeapArenaAdmin +
                          rtStats->zTotals.unusedGCThings.totalSize() +
                          rtStats->gcHeapGCThings;
  MOZ_ASSERT(totalArenaSize % gc::ArenaSize == 0);
#endif

  for (RealmsIter realm(rt); !realm.done(); realm.next()) {
    realm->nullRealmStats();
  }

  size_t numDirtyChunks =
      (rtStats->gcHeapChunkTotal - rtStats->gcHeapUnusedChunks) / gc::ChunkSize;
  size_t perChunkAdmin =
      sizeof(gc::Chunk) - (sizeof(gc::Arena) * gc::ArenasPerChunk);
  rtStats->gcHeapChunkAdmin = numDirtyChunks * perChunkAdmin;

  // |gcHeapUnusedArenas| is the only thing left.  Compute it in terms of
  // all the others.  See the comment in RuntimeStats for explanation.
  rtStats->gcHeapUnusedArenas =
      rtStats->gcHeapChunkTotal - rtStats->gcHeapDecommittedArenas -
      rtStats->gcHeapUnusedChunks -
      rtStats->zTotals.unusedGCThings.totalSize() - rtStats->gcHeapChunkAdmin -
      rtStats->zTotals.gcHeapArenaAdmin - rtStats->gcHeapGCThings;
  return true;
}

JS_PUBLIC_API bool JS::CollectGlobalStats(GlobalStats* gStats) {
  AutoLockHelperThreadState lock;

  // HelperThreadState holds data that is not part of a Runtime. This does
  // not include data is is currently being processed by a HelperThread.
  HelperThreadState().addSizeOfIncludingThis(gStats, lock);

#ifdef JS_TRACE_LOGGING
  // Global data used by TraceLogger
  gStats->tracelogger += SizeOfTraceLogState(gStats->mallocSizeOf_);
  gStats->tracelogger += SizeOfTraceLogGraphState(gStats->mallocSizeOf_);
#endif

  return true;
}

JS_PUBLIC_API bool JS::CollectRuntimeStats(JSContext* cx, RuntimeStats* rtStats,
                                           ObjectPrivateVisitor* opv,
                                           bool anonymize) {
  return CollectRuntimeStatsHelper(cx, rtStats, opv, anonymize,
                                   StatsCellCallback<FineGrained>);
}

JS_PUBLIC_API size_t JS::SystemRealmCount(JSContext* cx) {
  size_t n = 0;
  for (RealmsIter realm(cx->runtime()); !realm.done(); realm.next()) {
    if (realm->isSystem()) {
      ++n;
    }
  }
  return n;
}

JS_PUBLIC_API size_t JS::UserRealmCount(JSContext* cx) {
  size_t n = 0;
  for (RealmsIter realm(cx->runtime()); !realm.done(); realm.next()) {
    if (!realm->isSystem()) {
      ++n;
    }
  }
  return n;
}

JS_PUBLIC_API size_t JS::PeakSizeOfTemporary(const JSContext* cx) {
  return cx->tempLifoAlloc().peakSizeOfExcludingThis();
}

namespace JS {

class SimpleJSRuntimeStats : public JS::RuntimeStats {
 public:
  explicit SimpleJSRuntimeStats(MallocSizeOf mallocSizeOf)
      : JS::RuntimeStats(mallocSizeOf) {}

  virtual void initExtraZoneStats(JS::Zone* zone,
                                  JS::ZoneStats* zStats) override {}

  virtual void initExtraRealmStats(Handle<Realm*> realm,
                                   JS::RealmStats* realmStats) override {}
};

JS_PUBLIC_API bool AddSizeOfTab(JSContext* cx, HandleObject obj,
                                MallocSizeOf mallocSizeOf,
                                ObjectPrivateVisitor* opv, TabSizes* sizes) {
  SimpleJSRuntimeStats rtStats(mallocSizeOf);

  JS::Zone* zone = GetObjectZone(obj);

  if (!rtStats.realmStatsVector.reserve(zone->compartments().length())) {
    return false;
  }

  if (!rtStats.zoneStatsVector.reserve(1)) {
    return false;
  }

  // Take the per-compartment measurements. No need to anonymize because
  // these measurements will be aggregated.
  StatsClosure closure(&rtStats, opv, /* anonymize = */ false);
  IterateHeapUnbarrieredForZone(cx, zone, &closure, StatsZoneCallback,
                                StatsRealmCallback, StatsArenaCallback,
                                StatsCellCallback<CoarseGrained>);

  MOZ_ASSERT(rtStats.zoneStatsVector.length() == 1);
  rtStats.zTotals.addSizes(rtStats.zoneStatsVector[0]);

  for (size_t i = 0; i < rtStats.realmStatsVector.length(); i++) {
    rtStats.realmTotals.addSizes(rtStats.realmStatsVector[i]);
  }

  for (RealmsInZoneIter realm(zone); !realm.done(); realm.next()) {
    realm->nullRealmStats();
  }

  rtStats.zTotals.addToTabSizes(sizes);
  rtStats.realmTotals.addToTabSizes(sizes);

  return true;
}

JS_PUBLIC_API bool AddServoSizeOf(JSContext* cx, MallocSizeOf mallocSizeOf,
                                  ObjectPrivateVisitor* opv,
                                  ServoSizes* sizes) {
  SimpleJSRuntimeStats rtStats(mallocSizeOf);

  // No need to anonymize because the results will be aggregated.
  if (!CollectRuntimeStatsHelper(cx, &rtStats, opv, /* anonymize = */ false,
                                 StatsCellCallback<CoarseGrained>))
    return false;

#ifdef DEBUG
  size_t gcHeapTotalOriginal = sizes->gcHeapUsed + sizes->gcHeapUnused +
                               sizes->gcHeapAdmin + sizes->gcHeapDecommitted;
#endif

  rtStats.addToServoSizes(sizes);
  rtStats.zTotals.addToServoSizes(sizes);
  rtStats.realmTotals.addToServoSizes(sizes);

#ifdef DEBUG
  size_t gcHeapTotal = sizes->gcHeapUsed + sizes->gcHeapUnused +
                       sizes->gcHeapAdmin + sizes->gcHeapDecommitted;
  MOZ_ASSERT(rtStats.gcHeapChunkTotal == gcHeapTotal - gcHeapTotalOriginal);
#endif

  return true;
}

}  // namespace JS
