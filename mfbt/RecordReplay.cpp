/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RecordReplay.h"

#include "js/GCAnnotations.h"
#include "mozilla/Atomics.h"
#include "mozilla/Casting.h"
#include "mozilla/Utf8.h"

#ifndef XP_WIN
#include <dlfcn.h>
#endif

#include <stdlib.h>

namespace mozilla {
namespace recordreplay {

// clang-format off
#define FOR_EACH_INTERFACE(Macro)                                              \
  Macro(InternalAreThreadEventsPassedThrough, bool, (), ())                    \
  Macro(InternalAreThreadEventsDisallowed, bool, (), ())                       \
  Macro(InternalRecordReplayValue, size_t, (const char* aWhy, size_t aValue),  \
        (aWhy, aValue))                                                        \
  Macro(InternalGeneratePLDHashTableCallbacks, const PLDHashTableOps*,         \
        (const PLDHashTableOps* aOps), (aOps))                                 \
  Macro(InternalUnwrapPLDHashTableCallbacks, const PLDHashTableOps*,           \
        (const PLDHashTableOps* aOps), (aOps))                                 \
  Macro(InternalHasDivergedFromRecording, bool, (), ())                        \
  Macro(InternalThingIndex, size_t, (void* aThing), (aThing))                  \
  Macro(InternalCreateOrderedLock, int, (const char* aName), (aName))          \
  Macro(ExecutionProgressCounter, ProgressCounter*, (), ())                    \
  Macro(NewTimeWarpTarget, ProgressCounter, (), ())                            \
  Macro(ShouldUpdateProgressCounter, bool, (const char* aURL), (aURL))

#define FOR_EACH_INTERFACE_VOID(Macro)                                         \
  Macro(InternalBeginPassThroughThreadEvents, (), ())                          \
  Macro(InternalEndPassThroughThreadEvents, (), ())                            \
  Macro(InternalBeginDisallowThreadEvents, (), ())                             \
  Macro(InternalEndDisallowThreadEvents, (), ())                               \
  Macro(InternalRecordReplayBytes, (const char* aWhy, void* aData, size_t aSize), \
        (aWhy, aData, aSize))                                                  \
  Macro(InternalInvalidateRecording, (const char* aWhy), (aWhy))               \
  Macro(InternalDestroyPLDHashTableCallbacks, (const PLDHashTableOps* aOps),   \
        (aOps))                                                                \
  Macro(InternalMovePLDHashTableContents,                                      \
        (const PLDHashTableOps* aFirstOps, const PLDHashTableOps* aSecondOps), \
        (aFirstOps, aSecondOps))                                               \
  Macro(InternalHoldJSObject, (void* aJSObj), (aJSObj))                        \
  Macro(InternalRecordReplayAssert, (const char* aFormat, va_list aArgs),      \
        (aFormat, aArgs))                                                      \
  Macro(InternalRecordReplayAssertBytes, (const void* aData, size_t aSize),    \
        (aData, aSize))                                                        \
  Macro(InternalPrintLog, (const char* aFormat, va_list aArgs),                \
        (aFormat, aArgs))                                                      \
  Macro(InternalRegisterThing, (void* aThing), (aThing))                       \
  Macro(InternalUnregisterThing, (void* aThing), (aThing))                     \
  Macro(InternalOrderedLock, (int aLock), (aLock))                             \
  Macro(InternalOrderedUnlock, (int aLock), (aLock))                           \
  Macro(InternalAddOrderedPthreadMutex,                                        \
        (const char* aName, pthread_mutex_t* aMutex), (aName, aMutex))         \
  Macro(BeginContentParse,                                                     \
        (const void* aToken, const char* aURL, const char* aContentType),      \
        (aToken, aURL, aContentType))                                          \
  Macro(AddContentParseData8,                                                  \
        (const void* aToken, const mozilla::Utf8Unit* aUtf8Buffer,             \
         size_t aLength),                                                      \
        (aToken, aUtf8Buffer, aLength))                                        \
  Macro(AddContentParseData16,                                                 \
        (const void* aToken, const char16_t* aBuffer, size_t aLength),         \
        (aToken, aBuffer, aLength))                                            \
  Macro(EndContentParse, (const void* aToken), (aToken))                       \
  Macro(AdvanceExecutionProgressCounter, (), ())                               \
  Macro(InternalAssertScriptedCaller, (const char* aWhy), (aWhy))
// clang-format on

#define DECLARE_SYMBOL(aName, aReturnType, aFormals, _) \
  static aReturnType(*gPtr##aName) aFormals;
#define DECLARE_SYMBOL_VOID(aName, aFormals, _) \
  DECLARE_SYMBOL(aName, void, aFormals, _)

FOR_EACH_INTERFACE(DECLARE_SYMBOL)
FOR_EACH_INTERFACE_VOID(DECLARE_SYMBOL_VOID)

#undef DECLARE_SYMBOL
#undef DECLARE_SYMBOL_VOID

static void* LoadSymbol(const char* aName) {
#ifdef XP_WIN
  MOZ_CRASH("LoadSymbol");
#else
  void* rv = dlsym(RTLD_DEFAULT, aName);
  if (!rv) {
    fprintf(stderr, "Record/Replay LoadSymbol failed: %s\n", aName);
    MOZ_CRASH("LoadSymbol");
  }
  return rv;
#endif
}

void Initialize(int* aArgc, char*** aArgv) {
  // Only initialize if the right command line option was specified.
  bool found = false;
  for (int i = 0; i < *aArgc; i++) {
    if (!strcmp((*aArgv)[i], "-recordReplayDispatch")) {
      found = true;
      break;
    }
  }
  if (!found) {
    return;
  }

  void (*initialize)(int*, char***);
  BitwiseCast(LoadSymbol("RecordReplayInterface_Initialize"), &initialize);
  if (!initialize) {
    return;
  }

#define INIT_SYMBOL(aName, _1, _2, _3) \
  BitwiseCast(LoadSymbol("RecordReplayInterface_" #aName), &gPtr##aName);
#define INIT_SYMBOL_VOID(aName, _2, _3) INIT_SYMBOL(aName, void, _2, _3)

  FOR_EACH_INTERFACE(INIT_SYMBOL)
  FOR_EACH_INTERFACE_VOID(INIT_SYMBOL_VOID)

#undef INIT_SYMBOL
#undef INIT_SYMBOL_VOID

  initialize(aArgc, aArgv);
}

// Record/replay API functions can't GC, but we can't use
// JS::AutoSuppressGCAnalysis here due to linking issues.
struct AutoSuppressGCAnalysis {
  AutoSuppressGCAnalysis() {}
  ~AutoSuppressGCAnalysis() {
#ifdef DEBUG
    // Need nontrivial destructor.
    static Atomic<int, SequentiallyConsistent> dummy;
    dummy++;
#endif
  }
} JS_HAZ_GC_SUPPRESSED;

#define DEFINE_WRAPPER(aName, aReturnType, aFormals, aActuals) \
  aReturnType aName aFormals {                                 \
    AutoSuppressGCAnalysis suppress;                           \
    MOZ_ASSERT(IsRecordingOrReplaying() || IsMiddleman());     \
    return gPtr##aName aActuals;                               \
  }

#define DEFINE_WRAPPER_VOID(aName, aFormals, aActuals)     \
  void aName aFormals {                                    \
    AutoSuppressGCAnalysis suppress;                       \
    MOZ_ASSERT(IsRecordingOrReplaying() || IsMiddleman()); \
    gPtr##aName aActuals;                                  \
  }

FOR_EACH_INTERFACE(DEFINE_WRAPPER)
FOR_EACH_INTERFACE_VOID(DEFINE_WRAPPER_VOID)

#undef DEFINE_WRAPPER
#undef DEFINE_WRAPPER_VOID

bool gIsRecordingOrReplaying;
bool gIsRecording;
bool gIsReplaying;

}  // namespace recordreplay
}  // namespace mozilla
