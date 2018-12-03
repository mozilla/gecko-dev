/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef js_MemoryMetrics_h
#define js_MemoryMetrics_h

// These declarations are highly likely to change in the future. Depend on them
// at your own risk.

#include "mozilla/MemoryReporting.h"
#include "mozilla/TypeTraits.h"

#include <string.h>

#include "jspubtd.h"

#include "js/AllocPolicy.h"
#include "js/HashTable.h"
#include "js/TracingAPI.h"
#include "js/Utility.h"
#include "js/Vector.h"

class nsISupports;  // Needed for ObjectPrivateVisitor.

namespace JS {

struct TabSizes {
  enum Kind { Objects, Strings, Private, Other };

  TabSizes() : objects(0), strings(0), private_(0), other(0) {}

  void add(Kind kind, size_t n) {
    switch (kind) {
      case Objects:
        objects += n;
        break;
      case Strings:
        strings += n;
        break;
      case Private:
        private_ += n;
        break;
      case Other:
        other += n;
        break;
      default:
        MOZ_CRASH("bad TabSizes kind");
    }
  }

  size_t objects;
  size_t strings;
  size_t private_;
  size_t other;
};

/** These are the measurements used by Servo. */
struct ServoSizes {
  enum Kind {
    GCHeapUsed,
    GCHeapUnused,
    GCHeapAdmin,
    GCHeapDecommitted,
    MallocHeap,
    NonHeap,
    Ignore
  };

  ServoSizes() = default;

  void add(Kind kind, size_t n) {
    switch (kind) {
      case GCHeapUsed:
        gcHeapUsed += n;
        break;
      case GCHeapUnused:
        gcHeapUnused += n;
        break;
      case GCHeapAdmin:
        gcHeapAdmin += n;
        break;
      case GCHeapDecommitted:
        gcHeapDecommitted += n;
        break;
      case MallocHeap:
        mallocHeap += n;
        break;
      case NonHeap:
        nonHeap += n;
        break;
      case Ignore: /* do nothing */
        break;
      default:
        MOZ_CRASH("bad ServoSizes kind");
    }
  }

  size_t gcHeapUsed = 0;
  size_t gcHeapUnused = 0;
  size_t gcHeapAdmin = 0;
  size_t gcHeapDecommitted = 0;
  size_t mallocHeap = 0;
  size_t nonHeap = 0;
};

}  // namespace JS

namespace js {

/**
 * In memory reporting, we have concept of "sundries", line items which are too
 * small to be worth reporting individually.  Under some circumstances, a memory
 * reporter gets tossed into the sundries bucket if it's smaller than
 * MemoryReportingSundriesThreshold() bytes.
 *
 * We need to define this value here, rather than in the code which actually
 * generates the memory reports, because NotableStringInfo uses this value.
 */
JS_FRIEND_API size_t MemoryReportingSundriesThreshold();

/**
 * This hash policy avoids flattening ropes (which perturbs the site being
 * measured and requires a JSContext) at the expense of doing a FULL ROPE COPY
 * on every hash and match! Beware.
 */
struct InefficientNonFlatteningStringHashPolicy {
  typedef JSString* Lookup;
  static HashNumber hash(const Lookup& l);
  static bool match(const JSString* const& k, const Lookup& l);
};

// This file features many classes with numerous size_t fields, and each such
// class has one or more methods that need to operate on all of these fields.
// Writing these individually is error-prone -- it's easy to add a new field
// without updating all the required methods.  So we define a single macro list
// in each class to name the fields (and notable characteristics of them), and
// then use the following macros to transform those lists into the required
// methods.
//
// - The |tabKind| value is used when measuring TabSizes.
//
// - The |servoKind| value is used when measuring ServoSizes and also for
//   the various sizeOfLiveGCThings() methods.
//
// In some classes, one or more of the macro arguments aren't used.  We use '_'
// for those.
//
#define DECL_SIZE(tabKind, servoKind, mSize) size_t mSize;
#define ZERO_SIZE(tabKind, servoKind, mSize) mSize(0),
#define COPY_OTHER_SIZE(tabKind, servoKind, mSize) mSize(other.mSize),
#define ADD_OTHER_SIZE(tabKind, servoKind, mSize) mSize += other.mSize;
#define SUB_OTHER_SIZE(tabKind, servoKind, mSize) \
  MOZ_ASSERT(mSize >= other.mSize);               \
  mSize -= other.mSize;
#define ADD_SIZE_TO_N(tabKind, servoKind, mSize) n += mSize;
#define ADD_SIZE_TO_N_IF_LIVE_GC_THING(tabKind, servoKind, mSize)     \
  /* Avoid self-comparison warnings by comparing enums indirectly. */ \
  n += (mozilla::IsSame<int[ServoSizes::servoKind],                   \
                        int[ServoSizes::GCHeapUsed]>::value)          \
           ? mSize                                                    \
           : 0;
#define ADD_TO_TAB_SIZES(tabKind, servoKind, mSize) \
  sizes->add(JS::TabSizes::tabKind, mSize);
#define ADD_TO_SERVO_SIZES(tabKind, servoKind, mSize) \
  sizes->add(JS::ServoSizes::servoKind, mSize);

}  // namespace js

namespace JS {

struct ClassInfo {
#define FOR_EACH_SIZE(MACRO)                                  \
  MACRO(Objects, GCHeapUsed, objectsGCHeap)                   \
  MACRO(Objects, MallocHeap, objectsMallocHeapSlots)          \
  MACRO(Objects, MallocHeap, objectsMallocHeapElementsNormal) \
  MACRO(Objects, MallocHeap, objectsMallocHeapElementsAsmJS)  \
  MACRO(Objects, MallocHeap, objectsMallocHeapMisc)           \
  MACRO(Objects, NonHeap, objectsNonHeapElementsNormal)       \
  MACRO(Objects, NonHeap, objectsNonHeapElementsShared)       \
  MACRO(Objects, NonHeap, objectsNonHeapElementsWasm)         \
  MACRO(Objects, NonHeap, objectsNonHeapCodeWasm)

  ClassInfo() : FOR_EACH_SIZE(ZERO_SIZE) wasmGuardPages(0) {}

  void add(const ClassInfo& other) { FOR_EACH_SIZE(ADD_OTHER_SIZE) }

  void subtract(const ClassInfo& other){FOR_EACH_SIZE(SUB_OTHER_SIZE)}

  size_t sizeOfAllThings() const {
    size_t n = 0;
    FOR_EACH_SIZE(ADD_SIZE_TO_N)
    return n;
  }

  bool isNotable() const {
    static const size_t NotabilityThreshold = 16 * 1024;
    return sizeOfAllThings() >= NotabilityThreshold;
  }

  size_t sizeOfLiveGCThings() const {
    size_t n = 0;
    FOR_EACH_SIZE(ADD_SIZE_TO_N_IF_LIVE_GC_THING)
    return n;
  }

  void addToTabSizes(TabSizes* sizes) const { FOR_EACH_SIZE(ADD_TO_TAB_SIZES) }

  void addToServoSizes(ServoSizes* sizes) const {
      FOR_EACH_SIZE(ADD_TO_SERVO_SIZES)}

  FOR_EACH_SIZE(DECL_SIZE) size_t wasmGuardPages;

#undef FOR_EACH_SIZE
};

struct ShapeInfo {
#define FOR_EACH_SIZE(MACRO)                           \
  MACRO(Other, GCHeapUsed, shapesGCHeapTree)           \
  MACRO(Other, GCHeapUsed, shapesGCHeapDict)           \
  MACRO(Other, GCHeapUsed, shapesGCHeapBase)           \
  MACRO(Other, MallocHeap, shapesMallocHeapTreeTables) \
  MACRO(Other, MallocHeap, shapesMallocHeapDictTables) \
  MACRO(Other, MallocHeap, shapesMallocHeapTreeKids)

  ShapeInfo() : FOR_EACH_SIZE(ZERO_SIZE) dummy() {}

  void add(const ShapeInfo& other) { FOR_EACH_SIZE(ADD_OTHER_SIZE) }

  void subtract(const ShapeInfo& other){FOR_EACH_SIZE(SUB_OTHER_SIZE)}

  size_t sizeOfAllThings() const {
    size_t n = 0;
    FOR_EACH_SIZE(ADD_SIZE_TO_N)
    return n;
  }

  size_t sizeOfLiveGCThings() const {
    size_t n = 0;
    FOR_EACH_SIZE(ADD_SIZE_TO_N_IF_LIVE_GC_THING)
    return n;
  }

  void addToTabSizes(TabSizes* sizes) const { FOR_EACH_SIZE(ADD_TO_TAB_SIZES) }

  void addToServoSizes(ServoSizes* sizes) const {
      FOR_EACH_SIZE(ADD_TO_SERVO_SIZES)}

  FOR_EACH_SIZE(DECL_SIZE) int dummy;  // present just to absorb the trailing
                                       // comma from FOR_EACH_SIZE(ZERO_SIZE)

#undef FOR_EACH_SIZE
};

/**
 * Holds data about a notable class (one whose combined object and shape
 * instances use more than a certain amount of memory) so we can report it
 * individually.
 *
 * The only difference between this class and ClassInfo is that this class
 * holds a copy of the filename.
 */
struct NotableClassInfo : public ClassInfo {
  NotableClassInfo();
  NotableClassInfo(const char* className, const ClassInfo& info);
  NotableClassInfo(NotableClassInfo&& info);
  NotableClassInfo& operator=(NotableClassInfo&& info);

  ~NotableClassInfo() { js_free(className_); }

  char* className_;

 private:
  NotableClassInfo(const NotableClassInfo& info) = delete;
};

/** Data for tracking JIT-code memory usage. */
struct CodeSizes {
#define FOR_EACH_SIZE(MACRO)  \
  MACRO(_, NonHeap, ion)      \
  MACRO(_, NonHeap, baseline) \
  MACRO(_, NonHeap, regexp)   \
  MACRO(_, NonHeap, other)    \
  MACRO(_, NonHeap, unused)

  CodeSizes() : FOR_EACH_SIZE(ZERO_SIZE) dummy() {}

  void addToServoSizes(ServoSizes* sizes) const {
      FOR_EACH_SIZE(ADD_TO_SERVO_SIZES)}

  FOR_EACH_SIZE(DECL_SIZE) int dummy;  // present just to absorb the trailing
                                       // comma from FOR_EACH_SIZE(ZERO_SIZE)

#undef FOR_EACH_SIZE
};

/** Data for tracking GC memory usage. */
struct GCSizes {
  // |nurseryDecommitted| is marked as NonHeap rather than GCHeapDecommitted
  // because we don't consider the nursery to be part of the GC heap.
#define FOR_EACH_SIZE(MACRO)                   \
  MACRO(_, MallocHeap, marker)                 \
  MACRO(_, NonHeap, nurseryCommitted)          \
  MACRO(_, MallocHeap, nurseryMallocedBuffers) \
  MACRO(_, MallocHeap, storeBufferVals)        \
  MACRO(_, MallocHeap, storeBufferCells)       \
  MACRO(_, MallocHeap, storeBufferSlots)       \
  MACRO(_, MallocHeap, storeBufferWholeCells)  \
  MACRO(_, MallocHeap, storeBufferGenerics)

  GCSizes() : FOR_EACH_SIZE(ZERO_SIZE) dummy() {}

  void addToServoSizes(ServoSizes* sizes) const {
      FOR_EACH_SIZE(ADD_TO_SERVO_SIZES)}

  FOR_EACH_SIZE(DECL_SIZE) int dummy;  // present just to absorb the trailing
                                       // comma from FOR_EACH_SIZE(ZERO_SIZE)

#undef FOR_EACH_SIZE
};

/**
 * This class holds information about the memory taken up by identical copies of
 * a particular string.  Multiple JSStrings may have their sizes aggregated
 * together into one StringInfo object.  Note that two strings with identical
 * chars will not be aggregated together if one is a short string and the other
 * is not.
 */
struct StringInfo {
#define FOR_EACH_SIZE(MACRO)                   \
  MACRO(Strings, GCHeapUsed, gcHeapLatin1)     \
  MACRO(Strings, GCHeapUsed, gcHeapTwoByte)    \
  MACRO(Strings, MallocHeap, mallocHeapLatin1) \
  MACRO(Strings, MallocHeap, mallocHeapTwoByte)

  StringInfo() : FOR_EACH_SIZE(ZERO_SIZE) numCopies(0) {}

  void add(const StringInfo& other) {
    FOR_EACH_SIZE(ADD_OTHER_SIZE);
    numCopies++;
  }

  void subtract(const StringInfo& other) {
    FOR_EACH_SIZE(SUB_OTHER_SIZE);
    numCopies--;
  }

  bool isNotable() const {
    static const size_t NotabilityThreshold = 16 * 1024;
    size_t n = 0;
    FOR_EACH_SIZE(ADD_SIZE_TO_N)
    return n >= NotabilityThreshold;
  }

  size_t sizeOfLiveGCThings() const {
    size_t n = 0;
    FOR_EACH_SIZE(ADD_SIZE_TO_N_IF_LIVE_GC_THING)
    return n;
  }

  void addToTabSizes(TabSizes* sizes) const { FOR_EACH_SIZE(ADD_TO_TAB_SIZES) }

  void addToServoSizes(ServoSizes* sizes) const {
      FOR_EACH_SIZE(ADD_TO_SERVO_SIZES)}

  FOR_EACH_SIZE(DECL_SIZE)
      uint32_t numCopies;  // How many copies of the string have we seen?

#undef FOR_EACH_SIZE
};

/**
 * Holds data about a notable string (one which, counting all duplicates, uses
 * more than a certain amount of memory) so we can report it individually.
 *
 * The only difference between this class and StringInfo is that
 * NotableStringInfo holds a copy of some or all of the string's chars.
 */
struct NotableStringInfo : public StringInfo {
  static const size_t MAX_SAVED_CHARS = 1024;

  NotableStringInfo();
  NotableStringInfo(JSString* str, const StringInfo& info);
  NotableStringInfo(NotableStringInfo&& info);
  NotableStringInfo& operator=(NotableStringInfo&& info);

  ~NotableStringInfo() { js_free(buffer); }

  char* buffer;
  size_t length;

 private:
  NotableStringInfo(const NotableStringInfo& info) = delete;
};

/**
 * This class holds information about the memory taken up by script sources
 * from a particular file.
 */
struct ScriptSourceInfo {
#define FOR_EACH_SIZE(MACRO) MACRO(_, MallocHeap, misc)

  ScriptSourceInfo() : FOR_EACH_SIZE(ZERO_SIZE) numScripts(0) {}

  void add(const ScriptSourceInfo& other) {
    FOR_EACH_SIZE(ADD_OTHER_SIZE)
    numScripts++;
  }

  void subtract(const ScriptSourceInfo& other) {
    FOR_EACH_SIZE(SUB_OTHER_SIZE)
    numScripts--;
  }

  void addToServoSizes(ServoSizes* sizes) const {
    FOR_EACH_SIZE(ADD_TO_SERVO_SIZES)
  }

  bool isNotable() const {
    static const size_t NotabilityThreshold = 16 * 1024;
    size_t n = 0;
    FOR_EACH_SIZE(ADD_SIZE_TO_N)
    return n >= NotabilityThreshold;
  }

  FOR_EACH_SIZE(DECL_SIZE)
  uint32_t numScripts;  // How many ScriptSources come from this file? (It
                        // can be more than one in XML files that have
                        // multiple scripts in CDATA sections.)
#undef FOR_EACH_SIZE
};

/**
 * Holds data about a notable script source file (one whose combined
 * script sources use more than a certain amount of memory) so we can report it
 * individually.
 *
 * The only difference between this class and ScriptSourceInfo is that this
 * class holds a copy of the filename.
 */
struct NotableScriptSourceInfo : public ScriptSourceInfo {
  NotableScriptSourceInfo();
  NotableScriptSourceInfo(const char* filename, const ScriptSourceInfo& info);
  NotableScriptSourceInfo(NotableScriptSourceInfo&& info);
  NotableScriptSourceInfo& operator=(NotableScriptSourceInfo&& info);

  ~NotableScriptSourceInfo() { js_free(filename_); }

  char* filename_;

 private:
  NotableScriptSourceInfo(const NotableScriptSourceInfo& info) = delete;
};

struct HelperThreadStats {
#define FOR_EACH_SIZE(MACRO)       \
  MACRO(_, MallocHeap, stateData)  \
  MACRO(_, MallocHeap, parseTask)  \
  MACRO(_, MallocHeap, ionBuilder) \
  MACRO(_, MallocHeap, wasmCompile)

  explicit HelperThreadStats()
      : FOR_EACH_SIZE(ZERO_SIZE) idleThreadCount(0), activeThreadCount(0) {}

  FOR_EACH_SIZE(DECL_SIZE)

  unsigned idleThreadCount;
  unsigned activeThreadCount;

#undef FOR_EACH_SIZE
};

/**
 * Measurements that not associated with any individual runtime.
 */
struct GlobalStats {
#define FOR_EACH_SIZE(MACRO) MACRO(_, MallocHeap, tracelogger)

  explicit GlobalStats(mozilla::MallocSizeOf mallocSizeOf)
      : FOR_EACH_SIZE(ZERO_SIZE) mallocSizeOf_(mallocSizeOf) {}

  FOR_EACH_SIZE(DECL_SIZE)

  HelperThreadStats helperThread;

  mozilla::MallocSizeOf mallocSizeOf_;

#undef FOR_EACH_SIZE
};

/**
 * These measurements relate directly to the JSRuntime, and not to zones,
 * compartments, and realms within it.
 */
struct RuntimeSizes {
#define FOR_EACH_SIZE(MACRO)                        \
  MACRO(_, MallocHeap, object)                      \
  MACRO(_, MallocHeap, atomsTable)                  \
  MACRO(_, MallocHeap, atomsMarkBitmaps)            \
  MACRO(_, MallocHeap, contexts)                    \
  MACRO(_, MallocHeap, temporary)                   \
  MACRO(_, MallocHeap, interpreterStack)            \
  MACRO(_, MallocHeap, sharedImmutableStringsCache) \
  MACRO(_, MallocHeap, sharedIntlData)              \
  MACRO(_, MallocHeap, uncompressedSourceCache)     \
  MACRO(_, MallocHeap, scriptData)                  \
  MACRO(_, MallocHeap, tracelogger)                 \
  MACRO(_, MallocHeap, wasmRuntime)                 \
  MACRO(_, MallocHeap, jitLazyLink)

  RuntimeSizes()
      : FOR_EACH_SIZE(ZERO_SIZE) scriptSourceInfo(),
        code(),
        gc(),
        notableScriptSources() {
    allScriptSources = js_new<ScriptSourcesHashMap>();
    if (!allScriptSources) {
      MOZ_CRASH("oom");
    }
  }

  ~RuntimeSizes() {
    // |allScriptSources| is usually deleted and set to nullptr before this
    // destructor runs. But there are failure cases due to OOMs that may
    // prevent that, so it doesn't hurt to try again here.
    js_delete(allScriptSources);
  }

  void addToServoSizes(ServoSizes* sizes) const {
    FOR_EACH_SIZE(ADD_TO_SERVO_SIZES)
    scriptSourceInfo.addToServoSizes(sizes);
    code.addToServoSizes(sizes);
    gc.addToServoSizes(sizes);
  }

  // The script source measurements in |scriptSourceInfo| are initially for
  // all script sources.  At the end, if the measurement granularity is
  // FineGrained, we subtract the measurements of the notable script sources
  // and move them into |notableScriptSources|.
  FOR_EACH_SIZE(DECL_SIZE)
  ScriptSourceInfo scriptSourceInfo;
  CodeSizes code;
  GCSizes gc;

  typedef js::HashMap<const char*, ScriptSourceInfo, mozilla::CStringHasher,
                      js::SystemAllocPolicy>
      ScriptSourcesHashMap;

  // |allScriptSources| is only used transiently.  During the reporting phase
  // it is filled with info about every script source in the runtime.  It's
  // then used to fill in |notableScriptSources| (which actually gets
  // reported), and immediately discarded afterwards.
  ScriptSourcesHashMap* allScriptSources;
  js::Vector<NotableScriptSourceInfo, 0, js::SystemAllocPolicy>
      notableScriptSources;

#undef FOR_EACH_SIZE
};

struct UnusedGCThingSizes {
#define FOR_EACH_SIZE(MACRO)                      \
  MACRO(Other, GCHeapUnused, object)              \
  MACRO(Other, GCHeapUnused, script)              \
  MACRO(Other, GCHeapUnused, lazyScript)          \
  MACRO(Other, GCHeapUnused, shape)               \
  MACRO(Other, GCHeapUnused, baseShape)           \
  MACRO(Other, GCHeapUnused, objectGroup)         \
  MACRO(Other, GCHeapUnused, string)              \
  MACRO(Other, GCHeapUnused, symbol)              \
  IF_BIGINT(MACRO(Other, GCHeapUnused, bigInt), ) \
  MACRO(Other, GCHeapUnused, jitcode)             \
  MACRO(Other, GCHeapUnused, scope)               \
  MACRO(Other, GCHeapUnused, regExpShared)

  UnusedGCThingSizes() : FOR_EACH_SIZE(ZERO_SIZE) dummy() {}

  UnusedGCThingSizes(UnusedGCThingSizes&& other)
      : FOR_EACH_SIZE(COPY_OTHER_SIZE) dummy() {}

  void addToKind(JS::TraceKind kind, intptr_t n) {
    switch (kind) {
      case JS::TraceKind::Object:
        object += n;
        break;
      case JS::TraceKind::String:
        string += n;
        break;
      case JS::TraceKind::Symbol:
        symbol += n;
        break;
#ifdef ENABLE_BIGINT
      case JS::TraceKind::BigInt:
        bigInt += n;
        break;
#endif
      case JS::TraceKind::Script:
        script += n;
        break;
      case JS::TraceKind::Shape:
        shape += n;
        break;
      case JS::TraceKind::BaseShape:
        baseShape += n;
        break;
      case JS::TraceKind::JitCode:
        jitcode += n;
        break;
      case JS::TraceKind::LazyScript:
        lazyScript += n;
        break;
      case JS::TraceKind::ObjectGroup:
        objectGroup += n;
        break;
      case JS::TraceKind::Scope:
        scope += n;
        break;
      case JS::TraceKind::RegExpShared:
        regExpShared += n;
        break;
      default:
        MOZ_CRASH("Bad trace kind for UnusedGCThingSizes");
    }
  }

  void addSizes(const UnusedGCThingSizes& other){FOR_EACH_SIZE(ADD_OTHER_SIZE)}

  size_t totalSize() const {
    size_t n = 0;
    FOR_EACH_SIZE(ADD_SIZE_TO_N)
    return n;
  }

  void addToTabSizes(JS::TabSizes* sizes) const {
    FOR_EACH_SIZE(ADD_TO_TAB_SIZES)
  }

  void addToServoSizes(JS::ServoSizes* sizes) const {
      FOR_EACH_SIZE(ADD_TO_SERVO_SIZES)}

  FOR_EACH_SIZE(DECL_SIZE) int dummy;  // present just to absorb the trailing
                                       // comma from FOR_EACH_SIZE(ZERO_SIZE)

#undef FOR_EACH_SIZE
};

struct ZoneStats {
#define FOR_EACH_SIZE(MACRO)                               \
  MACRO(Other, GCHeapUsed, symbolsGCHeap)                  \
  IF_BIGINT(MACRO(Other, GCHeapUsed, bigIntsGCHeap), )     \
  IF_BIGINT(MACRO(Other, MallocHeap, bigIntsMallocHeap), ) \
  MACRO(Other, GCHeapAdmin, gcHeapArenaAdmin)              \
  MACRO(Other, GCHeapUsed, lazyScriptsGCHeap)              \
  MACRO(Other, MallocHeap, lazyScriptsMallocHeap)          \
  MACRO(Other, GCHeapUsed, jitCodesGCHeap)                 \
  MACRO(Other, GCHeapUsed, objectGroupsGCHeap)             \
  MACRO(Other, MallocHeap, objectGroupsMallocHeap)         \
  MACRO(Other, GCHeapUsed, scopesGCHeap)                   \
  MACRO(Other, MallocHeap, scopesMallocHeap)               \
  MACRO(Other, GCHeapUsed, regExpSharedsGCHeap)            \
  MACRO(Other, MallocHeap, regExpSharedsMallocHeap)        \
  MACRO(Other, MallocHeap, typePool)                       \
  MACRO(Other, MallocHeap, regexpZone)                     \
  MACRO(Other, MallocHeap, jitZone)                        \
  MACRO(Other, MallocHeap, baselineStubsOptimized)         \
  MACRO(Other, MallocHeap, cachedCFG)                      \
  MACRO(Other, MallocHeap, uniqueIdMap)                    \
  MACRO(Other, MallocHeap, shapeTables)                    \
  MACRO(Other, MallocHeap, compartmentObjects)             \
  MACRO(Other, MallocHeap, crossCompartmentWrappersTables) \
  MACRO(Other, MallocHeap, compartmentsPrivateData)

  ZoneStats()
      : FOR_EACH_SIZE(ZERO_SIZE) unusedGCThings(),
        stringInfo(),
        shapeInfo(),
        extra(),
        allStrings(nullptr),
        notableStrings(),
        isTotals(true) {}

  ZoneStats(ZoneStats&& other)
      : FOR_EACH_SIZE(COPY_OTHER_SIZE)
            unusedGCThings(std::move(other.unusedGCThings)),
        stringInfo(std::move(other.stringInfo)),
        shapeInfo(std::move(other.shapeInfo)),
        extra(other.extra),
        allStrings(other.allStrings),
        notableStrings(std::move(other.notableStrings)),
        isTotals(other.isTotals) {
    other.allStrings = nullptr;
    MOZ_ASSERT(!other.isTotals);
  }

  ~ZoneStats() {
    // |allStrings| is usually deleted and set to nullptr before this
    // destructor runs. But there are failure cases due to OOMs that may
    // prevent that, so it doesn't hurt to try again here.
    js_delete(allStrings);
  }

  bool initStrings();

  void addSizes(const ZoneStats& other) {
    MOZ_ASSERT(isTotals);
    FOR_EACH_SIZE(ADD_OTHER_SIZE)
    unusedGCThings.addSizes(other.unusedGCThings);
    stringInfo.add(other.stringInfo);
    shapeInfo.add(other.shapeInfo);
  }

  size_t sizeOfLiveGCThings() const {
    MOZ_ASSERT(isTotals);
    size_t n = 0;
    FOR_EACH_SIZE(ADD_SIZE_TO_N_IF_LIVE_GC_THING)
    n += stringInfo.sizeOfLiveGCThings();
    n += shapeInfo.sizeOfLiveGCThings();
    return n;
  }

  void addToTabSizes(JS::TabSizes* sizes) const {
    MOZ_ASSERT(isTotals);
    FOR_EACH_SIZE(ADD_TO_TAB_SIZES)
    unusedGCThings.addToTabSizes(sizes);
    stringInfo.addToTabSizes(sizes);
    shapeInfo.addToTabSizes(sizes);
  }

  void addToServoSizes(JS::ServoSizes* sizes) const {
    MOZ_ASSERT(isTotals);
    FOR_EACH_SIZE(ADD_TO_SERVO_SIZES)
    unusedGCThings.addToServoSizes(sizes);
    stringInfo.addToServoSizes(sizes);
    shapeInfo.addToServoSizes(sizes);
  }

  // These string measurements are initially for all strings.  At the end,
  // if the measurement granularity is FineGrained, we subtract the
  // measurements of the notable script sources and move them into
  // |notableStrings|.
  FOR_EACH_SIZE(DECL_SIZE)
  UnusedGCThingSizes unusedGCThings;
  StringInfo stringInfo;
  ShapeInfo shapeInfo;
  void* extra;  // This field can be used by embedders.

  typedef js::HashMap<JSString*, StringInfo,
                      js::InefficientNonFlatteningStringHashPolicy,
                      js::SystemAllocPolicy>
      StringsHashMap;

  // |allStrings| is only used transiently.  During the zone traversal it is
  // filled with info about every string in the zone.  It's then used to fill
  // in |notableStrings| (which actually gets reported), and immediately
  // discarded afterwards.
  StringsHashMap* allStrings;
  js::Vector<NotableStringInfo, 0, js::SystemAllocPolicy> notableStrings;
  bool isTotals;

#undef FOR_EACH_SIZE
};

struct RealmStats {
  // We assume that |objectsPrivate| is on the malloc heap, but it's not
  // actually guaranteed. But for Servo, at least, it's a moot point because
  // it doesn't provide an ObjectPrivateVisitor so the value will always be
  // zero.
#define FOR_EACH_SIZE(MACRO)                                  \
  MACRO(Private, MallocHeap, objectsPrivate)                  \
  MACRO(Other, GCHeapUsed, scriptsGCHeap)                     \
  MACRO(Other, MallocHeap, scriptsMallocHeapData)             \
  MACRO(Other, MallocHeap, baselineData)                      \
  MACRO(Other, MallocHeap, baselineStubsFallback)             \
  MACRO(Other, MallocHeap, ionData)                           \
  MACRO(Other, MallocHeap, typeInferenceTypeScripts)          \
  MACRO(Other, MallocHeap, typeInferenceAllocationSiteTables) \
  MACRO(Other, MallocHeap, typeInferenceArrayTypeTables)      \
  MACRO(Other, MallocHeap, typeInferenceObjectTypeTables)     \
  MACRO(Other, MallocHeap, realmObject)                       \
  MACRO(Other, MallocHeap, realmTables)                       \
  MACRO(Other, MallocHeap, innerViewsTable)                   \
  MACRO(Other, MallocHeap, lazyArrayBuffersTable)             \
  MACRO(Other, MallocHeap, objectMetadataTable)               \
  MACRO(Other, MallocHeap, savedStacksSet)                    \
  MACRO(Other, MallocHeap, varNamesSet)                       \
  MACRO(Other, MallocHeap, nonSyntacticLexicalScopesTable)    \
  MACRO(Other, MallocHeap, jitRealm)                          \
  MACRO(Other, MallocHeap, scriptCountsMap)

  RealmStats()
      : FOR_EACH_SIZE(ZERO_SIZE) classInfo(),
        extra(),
        allClasses(nullptr),
        notableClasses(),
        isTotals(true) {}

  RealmStats(RealmStats&& other)
      : FOR_EACH_SIZE(COPY_OTHER_SIZE) classInfo(std::move(other.classInfo)),
        extra(other.extra),
        allClasses(other.allClasses),
        notableClasses(std::move(other.notableClasses)),
        isTotals(other.isTotals) {
    other.allClasses = nullptr;
    MOZ_ASSERT(!other.isTotals);
  }

  RealmStats(const RealmStats&) = delete;  // disallow copying

  ~RealmStats() {
    // |allClasses| is usually deleted and set to nullptr before this
    // destructor runs. But there are failure cases due to OOMs that may
    // prevent that, so it doesn't hurt to try again here.
    js_delete(allClasses);
  }

  bool initClasses();

  void addSizes(const RealmStats& other) {
    MOZ_ASSERT(isTotals);
    FOR_EACH_SIZE(ADD_OTHER_SIZE)
    classInfo.add(other.classInfo);
  }

  size_t sizeOfLiveGCThings() const {
    MOZ_ASSERT(isTotals);
    size_t n = 0;
    FOR_EACH_SIZE(ADD_SIZE_TO_N_IF_LIVE_GC_THING)
    n += classInfo.sizeOfLiveGCThings();
    return n;
  }

  void addToTabSizes(TabSizes* sizes) const {
    MOZ_ASSERT(isTotals);
    FOR_EACH_SIZE(ADD_TO_TAB_SIZES);
    classInfo.addToTabSizes(sizes);
  }

  void addToServoSizes(ServoSizes* sizes) const {
    MOZ_ASSERT(isTotals);
    FOR_EACH_SIZE(ADD_TO_SERVO_SIZES);
    classInfo.addToServoSizes(sizes);
  }

  // The class measurements in |classInfo| are initially for all classes.  At
  // the end, if the measurement granularity is FineGrained, we subtract the
  // measurements of the notable classes and move them into |notableClasses|.
  FOR_EACH_SIZE(DECL_SIZE)
  ClassInfo classInfo;
  void* extra;  // This field can be used by embedders.

  typedef js::HashMap<const char*, ClassInfo, mozilla::CStringHasher,
                      js::SystemAllocPolicy>
      ClassesHashMap;

  // These are similar to |allStrings| and |notableStrings| in ZoneStats.
  ClassesHashMap* allClasses;
  js::Vector<NotableClassInfo, 0, js::SystemAllocPolicy> notableClasses;
  bool isTotals;

#undef FOR_EACH_SIZE
};

typedef js::Vector<RealmStats, 0, js::SystemAllocPolicy> RealmStatsVector;
typedef js::Vector<ZoneStats, 0, js::SystemAllocPolicy> ZoneStatsVector;

struct RuntimeStats {
  // |gcHeapChunkTotal| is ignored because it's the sum of all the other
  // values. |gcHeapGCThings| is ignored because it's the sum of some of the
  // values from the zones and compartments. Both of those values are not
  // reported directly, but are just present for sanity-checking other
  // values.
#define FOR_EACH_SIZE(MACRO)                           \
  MACRO(_, Ignore, gcHeapChunkTotal)                   \
  MACRO(_, GCHeapDecommitted, gcHeapDecommittedArenas) \
  MACRO(_, GCHeapUnused, gcHeapUnusedChunks)           \
  MACRO(_, GCHeapUnused, gcHeapUnusedArenas)           \
  MACRO(_, GCHeapAdmin, gcHeapChunkAdmin)              \
  MACRO(_, Ignore, gcHeapGCThings)

  explicit RuntimeStats(mozilla::MallocSizeOf mallocSizeOf)
      : FOR_EACH_SIZE(ZERO_SIZE) runtime(),
        realmTotals(),
        zTotals(),
        realmStatsVector(),
        zoneStatsVector(),
        currZoneStats(nullptr),
        mallocSizeOf_(mallocSizeOf) {}

  // Here's a useful breakdown of the GC heap.
  //
  // - rtStats.gcHeapChunkTotal
  //   - decommitted bytes
  //     - rtStats.gcHeapDecommittedArenas
  //         (decommitted arenas in non-empty chunks)
  //   - unused bytes
  //     - rtStats.gcHeapUnusedChunks (empty chunks)
  //     - rtStats.gcHeapUnusedArenas (empty arenas within non-empty chunks)
  //     - rtStats.zTotals.unusedGCThings.totalSize()
  //         (empty GC thing slots within non-empty arenas)
  //   - used bytes
  //     - rtStats.gcHeapChunkAdmin
  //     - rtStats.zTotals.gcHeapArenaAdmin
  //     - rtStats.gcHeapGCThings (in-use GC things)
  //       == (rtStats.zTotals.sizeOfLiveGCThings() +
  //           rtStats.cTotals.sizeOfLiveGCThings())
  //
  // It's possible that some arenas in empty chunks may be decommitted, but
  // we don't count those under rtStats.gcHeapDecommittedArenas because (a)
  // it's rare, and (b) this means that rtStats.gcHeapUnusedChunks is a
  // multiple of the chunk size, which is good.

  void addToServoSizes(ServoSizes* sizes) const {
    FOR_EACH_SIZE(ADD_TO_SERVO_SIZES)
    runtime.addToServoSizes(sizes);
  }

  FOR_EACH_SIZE(DECL_SIZE)

  RuntimeSizes runtime;

  RealmStats realmTotals;  // The sum of this runtime's realms' measurements.
  ZoneStats zTotals;       // The sum of this runtime's zones' measurements.

  RealmStatsVector realmStatsVector;
  ZoneStatsVector zoneStatsVector;

  ZoneStats* currZoneStats;

  mozilla::MallocSizeOf mallocSizeOf_;

  virtual void initExtraRealmStats(JS::Handle<JS::Realm*> realm,
                                   RealmStats* rstats) = 0;
  virtual void initExtraZoneStats(JS::Zone* zone, ZoneStats* zstats) = 0;

#undef FOR_EACH_SIZE
};

class ObjectPrivateVisitor {
 public:
  // Within CollectRuntimeStats, this method is called for each JS object
  // that has an nsISupports pointer.
  virtual size_t sizeOfIncludingThis(nsISupports* aSupports) = 0;

  // A callback that gets a JSObject's nsISupports pointer, if it has one.
  // Note: this function does *not* addref |iface|.
  typedef bool (*GetISupportsFun)(JSObject* obj, nsISupports** iface);
  GetISupportsFun getISupports_;

  explicit ObjectPrivateVisitor(GetISupportsFun getISupports)
      : getISupports_(getISupports) {}
};

extern JS_PUBLIC_API bool CollectGlobalStats(GlobalStats* gStats);

extern JS_PUBLIC_API bool CollectRuntimeStats(JSContext* cx,
                                              RuntimeStats* rtStats,
                                              ObjectPrivateVisitor* opv,
                                              bool anonymize);

extern JS_PUBLIC_API size_t SystemRealmCount(JSContext* cx);

extern JS_PUBLIC_API size_t UserRealmCount(JSContext* cx);

extern JS_PUBLIC_API size_t PeakSizeOfTemporary(const JSContext* cx);

extern JS_PUBLIC_API bool AddSizeOfTab(JSContext* cx, JS::HandleObject obj,
                                       mozilla::MallocSizeOf mallocSizeOf,
                                       ObjectPrivateVisitor* opv,
                                       TabSizes* sizes);

extern JS_PUBLIC_API bool AddServoSizeOf(JSContext* cx,
                                         mozilla::MallocSizeOf mallocSizeOf,
                                         ObjectPrivateVisitor* opv,
                                         ServoSizes* sizes);

}  // namespace JS

#undef DECL_SIZE
#undef ZERO_SIZE
#undef COPY_OTHER_SIZE
#undef ADD_OTHER_SIZE
#undef SUB_OTHER_SIZE
#undef ADD_SIZE_TO_N
#undef ADD_SIZE_TO_N_IF_LIVE_GC_THING
#undef ADD_TO_TAB_SIZES

#endif /* js_MemoryMetrics_h */
