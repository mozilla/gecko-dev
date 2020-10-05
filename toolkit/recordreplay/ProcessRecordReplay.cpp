/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ProcessRecordReplay.h"

#include "JSControl.h"
#include "mozilla/BasicEvents.h"
#include "mozilla/dom/BrowserChild.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/Compression.h"
#include "mozilla/CycleCollectedJSContext.h"
#include "mozilla/Maybe.h"
#include "mozilla/Sprintf.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/VsyncDispatcher.h"
#include "nsAppRunner.h"
#include "pratom.h"
#include "nsPrintfCString.h"

#include <dlfcn.h>
#include <fcntl.h>
#include <unistd.h>

#include <mach/exc.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <mach/ndr.h>
#include <sys/time.h>

namespace mozilla {
namespace recordreplay {

MOZ_NEVER_INLINE void BusyWait() {
  static volatile int value = 1;
  while (value) {
  }
}

///////////////////////////////////////////////////////////////////////////////
// Basic interface
///////////////////////////////////////////////////////////////////////////////

static bool gAutomatedTesting;

struct JSFilter {
  std::string mFilename;
  unsigned mStartLine;
  unsigned mEndLine;
};

static void ParseJSFilters(const char* aEnv, InfallibleVector<JSFilter>& aFilters);
static bool FilterMatches(const InfallibleVector<JSFilter>& aFilters,
                          const char* aFilename, unsigned aLine);

// Whether to assert on execution progress changes.
static InfallibleVector<JSFilter> gExecutionAsserts;

// Whether to assert on JS values.
static InfallibleVector<JSFilter> gJSAsserts;

static void (*gAttach)(const char* dispatch, const char* buildId);
static void (*gRecordCommandLineArguments)(int*, char***);
static uintptr_t (*gRecordReplayValue)(const char* why, uintptr_t value);
static void (*gRecordReplayBytes)(const char* why, void* buf, size_t size);
static void (*gPrintVA)(const char* format, va_list args);
static void (*gRegisterPointer)(void* ptr);
static void (*gUnregisterPointer)(void* ptr);
static int (*gPointerId)(void* ptr);
static void (*gAssert)(const char* format, va_list);
static void (*gAssertBytes)(const char* why, const void*, size_t);
static void (*gFinishRecording)();
static uint64_t* (*gProgressCounter)();
static void (*gBeginPassThroughEvents)();
static void (*gEndPassThroughEvents)();
static bool (*gAreEventsPassedThrough)();
static void (*gBeginDisallowEvents)();
static void (*gEndDisallowEvents)();
static bool (*gAreEventsDisallowed)();
static bool (*gHasDivergedFromRecording)();
static void (*gRecordReplayNewCheckpoint)();
static bool (*gRecordReplayIsReplaying)();
static int (*gCreateOrderedLock)(const char* aName);
static void (*gOrderedLock)(int aLock);
static void (*gOrderedUnlock)(int aLock);
static void (*gAddOrderedPthreadMutex)(const char* aName, pthread_mutex_t* aMutex);

static void* gDriverHandle;

void LoadSymbolInternal(const char* name, void** psym) {
  *psym = dlsym(gDriverHandle, name);
  if (!*psym) {
    fprintf(stderr, "Could not find %s in Record Replay driver, crashing.\n", name);
    MOZ_CRASH();
  }
}

extern "C" {

MOZ_EXPORT void RecordReplayInterface_Initialize(int* aArgc, char*** aArgv) {
  // Parse command line options for the process kind and recording file.
  Maybe<char*> dispatchAddress;
  int argc = *aArgc;
  char** argv = *aArgv;
  for (int i = 0; i < argc; i++) {
    if (!strcmp(argv[i], "-recordReplayDispatch")) {
      MOZ_RELEASE_ASSERT(dispatchAddress.isNothing() && i + 1 < argc);
      dispatchAddress.emplace(argv[i + 1]);
    }
  }
  MOZ_RELEASE_ASSERT(dispatchAddress.isSome());

  const char* driver = getenv("RECORD_REPLAY_DRIVER");
  if (!driver) {
    fprintf(stderr, "RECORD_REPLAY_DRIVER not set, crashing...\n");
    MOZ_CRASH("RECORD_REPLAY_DRIVER not set");
  }

  // Don't create a stylo thread pool when recording or replaying.
  putenv((char*)"STYLO_THREADS=1");

  for (size_t i = 0; i < 60; i++) {
    gDriverHandle = dlopen(driver, RTLD_LAZY);
    if (gDriverHandle) {
      break;
    }
    fprintf(stderr, "Loading driver at %s failed, waiting...\n", driver);
    sleep(1);
  }
  if (!gDriverHandle) {
    fprintf(stderr, "Loading driver at %s failed, crashing.\n", driver);
    MOZ_CRASH("RECORD_REPLAY_DRIVER loading failed");
  }

  LoadSymbol("RecordReplayAttach", gAttach);
  LoadSymbol("RecordReplayRecordCommandLineArguments",
             gRecordCommandLineArguments);
  LoadSymbol("RecordReplayValue", gRecordReplayValue);
  LoadSymbol("RecordReplayBytes", gRecordReplayBytes);
  LoadSymbol("RecordReplayPrint", gPrintVA);
  LoadSymbol("RecordReplayFinishRecording", gFinishRecording);
  LoadSymbol("RecordReplayRegisterPointer", gRegisterPointer);
  LoadSymbol("RecordReplayUnregisterPointer", gUnregisterPointer);
  LoadSymbol("RecordReplayPointerId", gPointerId);
  LoadSymbol("RecordReplayAssert", gAssert);
  LoadSymbol("RecordReplayAssertBytes", gAssertBytes);
  LoadSymbol("RecordReplayProgressCounter", gProgressCounter);
  LoadSymbol("RecordReplayBeginPassThroughEvents", gBeginPassThroughEvents);
  LoadSymbol("RecordReplayEndPassThroughEvents", gEndPassThroughEvents);
  LoadSymbol("RecordReplayAreEventsPassedThrough", gAreEventsPassedThrough);
  LoadSymbol("RecordReplayBeginDisallowEvents", gBeginDisallowEvents);
  LoadSymbol("RecordReplayEndDisallowEvents", gEndDisallowEvents);
  LoadSymbol("RecordReplayAreEventsDisallowed", gAreEventsDisallowed);
  LoadSymbol("RecordReplayHasDivergedFromRecording", gHasDivergedFromRecording);
  LoadSymbol("RecordReplayNewCheckpoint", gRecordReplayNewCheckpoint);
  LoadSymbol("RecordReplayIsReplaying", gRecordReplayIsReplaying);
  LoadSymbol("RecordReplayCreateOrderedLock", gCreateOrderedLock);
  LoadSymbol("RecordReplayOrderedLock", gOrderedLock);
  LoadSymbol("RecordReplayOrderedUnlock", gOrderedUnlock);
  LoadSymbol("RecordReplayAddOrderedPthreadMutex", gAddOrderedPthreadMutex);

  js::InitializeJS();
  InitializeGraphics();

  char buildId[128];
  snprintf(buildId, sizeof(buildId), "macOS-gecko-%s", PlatformBuildID());
  gAttach(*dispatchAddress, buildId);

  gIsRecordingOrReplaying = true;
  gIsRecording = !gRecordReplayIsReplaying();
  gIsReplaying = gRecordReplayIsReplaying();

  gAutomatedTesting = TestEnv("RECORD_REPLAY_TEST_SCRIPT");
  ParseJSFilters("RECORD_REPLAY_RECORD_EXECUTION_ASSERTS", gExecutionAsserts);
  ParseJSFilters("RECORD_REPLAY_RECORD_JS_ASSERTS", gJSAsserts);

  gRecordCommandLineArguments(aArgc, aArgv);
}

MOZ_EXPORT size_t
RecordReplayInterface_InternalRecordReplayValue(const char* aWhy, size_t aValue) {
  return gRecordReplayValue(aWhy, aValue);
}

MOZ_EXPORT void RecordReplayInterface_InternalRecordReplayBytes(const char* aWhy,
                                                                void* aData,
                                                                size_t aSize) {
  gRecordReplayBytes(aWhy, aData, aSize);
}

MOZ_EXPORT void RecordReplayInterface_InternalInvalidateRecording(
    const char* aWhy) {
  PrintLog("Recording invalidated %s, crashing...", aWhy);
  MOZ_CRASH();
}

MOZ_EXPORT void RecordReplayInterface_InternalRecordReplayAssert(
    const char* aFormat, va_list aArgs) {
  gAssert(aFormat, aArgs);
}

MOZ_EXPORT void RecordReplayInterface_InternalRecordReplayAssertBytes(
    const void* aData, size_t aSize) {
  gAssertBytes("Bytes", aData, aSize);
}

MOZ_EXPORT void RecordReplayAssertFromC(const char* aText) {
  RecordReplayAssert(aText);
}

MOZ_EXPORT void RecordReplayInterface_InternalRegisterThing(void* aThing) {
  gRegisterPointer(aThing);
}

MOZ_EXPORT void RecordReplayInterface_InternalUnregisterThing(void* aThing) {
  gUnregisterPointer(aThing);
}

MOZ_EXPORT size_t RecordReplayInterface_InternalThingIndex(void* aThing) {
  return gPointerId(aThing);
}

MOZ_EXPORT void RecordReplayInterface_InternalHoldJSObject(void* aJSObj) {
  if (aJSObj) {
    JSContext* cx = dom::danger::GetJSContext();
    JS::PersistentRootedObject* root = new JS::PersistentRootedObject(cx);
    *root = static_cast<JSObject*>(aJSObj);
  }
}

MOZ_EXPORT bool RecordReplayInterface_InternalInAutomatedTest() {
  return gAutomatedTesting;
}

MOZ_EXPORT void RecordReplayInterface_InternalAssertScriptedCaller(const char* aWhy) {
  JS::AutoFilename filename;
  unsigned lineno;
  unsigned column;
  JSContext* cx = nullptr;
  if (NS_IsMainThread() && CycleCollectedJSContext::Get()) {
    cx = dom::danger::GetJSContext();
  }
  if (cx && JS::DescribeScriptedCaller(cx, &filename, &lineno, &column)) {
    RecordReplayAssert("%s %s:%u:%u", aWhy, filename.get(), lineno, column);
  } else {
    RecordReplayAssert("%s NoScriptedCaller", aWhy);
  }
}

MOZ_EXPORT void RecordReplayInterface_ExecutionProgressHook(const char* aFilename, unsigned aLineno,
                                                            unsigned aColumn) {
  if (FilterMatches(gExecutionAsserts, aFilename, aLineno)) {
    RecordReplayAssert("ExecutionProgress %s:%u:%u", aFilename, aLineno, aColumn);
  }
}

MOZ_EXPORT bool RecordReplayInterface_ShouldEmitRecordReplayAssert(const char* aFilename,
                                                                   unsigned aLineno,
                                                                   unsigned aColumn) {
  return FilterMatches(gJSAsserts, aFilename, aLineno);
}

MOZ_EXPORT void RecordReplayInterface_InternalPrintLog(const char* aFormat,
                                                       va_list aArgs) {
  gPrintVA(aFormat, aArgs);
}

MOZ_EXPORT ProgressCounter* RecordReplayInterface_ExecutionProgressCounter() {
  return gProgressCounter();
}

MOZ_EXPORT void RecordReplayInterface_AdvanceExecutionProgressCounter() {
  ++*gProgressCounter();
}

MOZ_EXPORT void RecordReplayInterface_InternalBeginPassThroughThreadEvents() {
  gBeginPassThroughEvents();
}

MOZ_EXPORT void RecordReplayInterface_InternalEndPassThroughThreadEvents() {
  gEndPassThroughEvents();
}

MOZ_EXPORT bool RecordReplayInterface_InternalAreThreadEventsPassedThrough() {
  return gAreEventsPassedThrough();
}

MOZ_EXPORT void RecordReplayInterface_InternalBeginDisallowThreadEvents() {
  gBeginDisallowEvents();
}

MOZ_EXPORT void RecordReplayInterface_InternalEndDisallowThreadEvents() {
  gEndDisallowEvents();
}

MOZ_EXPORT bool RecordReplayInterface_InternalAreThreadEventsDisallowed() {
  return gAreEventsDisallowed();
}

MOZ_EXPORT bool RecordReplayInterface_InternalHasDivergedFromRecording() {
  return gHasDivergedFromRecording();
}

MOZ_EXPORT int RecordReplayInterface_InternalCreateOrderedLock(const char* aName) {
  return gCreateOrderedLock(aName);
}

MOZ_EXPORT void RecordReplayInterface_InternalOrderedLock(int aLock) {
  gOrderedLock(aLock);
}

MOZ_EXPORT void RecordReplayInterface_InternalOrderedUnlock(int aLock) {
  gOrderedUnlock(aLock);
}

MOZ_EXPORT void RecordReplayInterface_InternalAddOrderedPthreadMutex(const char* aName,
                                                                     pthread_mutex_t* aMutex) {
  gAddOrderedPthreadMutex(aName, aMutex);
}

}  // extern "C"

static void ParseJSFilters(const char* aEnv, InfallibleVector<JSFilter>& aFilters) {
  const char* value = getenv(aEnv);
  if (!value) {
    return;
  }

  while (true) {
    JSFilter filter;

    const char* end = strchr(value, '@');
    if (!end) {
      break;
    }

    filter.mFilename = std::string(value, end - value);
    value = end + 1;

    end = strchr(value, '@');
    if (!end) {
      break;
    }

    filter.mStartLine = atoi(value);
    value = end + 1;

    filter.mEndLine = atoi(value);

    PrintLog("ParseJSFilter %s %s %u %u", aEnv,
             filter.mFilename.c_str(), filter.mStartLine, filter.mEndLine);
    aFilters.append(filter);

    end = strchr(value, '@');
    if (!end) {
      break;
    }

    value = end + 1;
  }
}

static bool FilterMatches(const InfallibleVector<JSFilter>& aFilters,
                          const char* aFilename, unsigned aLine) {
  for (const JSFilter& filter : aFilters) {
    if (strstr(aFilename, filter.mFilename.c_str()) &&
        aLine >= filter.mStartLine &&
        aLine <= filter.mEndLine) {
      return true;
    }
  }
  return false;
}

const char* CurrentFirefoxVersion() {
  return "74.0a1";
}

void OnWidgetEvent(dom::BrowserChild* aChild, const WidgetMouseEvent& aEvent) {
}

static bool gHasCheckpoint = false;

void CreateCheckpoint() {
  js::EnsureModuleInitialized();
  gRecordReplayNewCheckpoint();
}

void MaybeCreateCheckpoint() {
  // This is called at the top of the event loop, and the process might not be
  // fully initialized. CreateCheckpoint() is only called after the process has
  // been fully initialized, and we don't want any checkpoints before then.
  if (gHasCheckpoint) {
    gRecordReplayNewCheckpoint();
  }
}

void FinishRecording() {
  js::SendRecordingFinished();

  gFinishRecording();

  // RecordReplayFinishRecording() does not return until the recording has been
  // fully uploaded. The ContentParent will not kill this process after
  // finishing the recording, so we have to it ourselves.
  PrintLog("Recording finished, exiting.");
  exit(0);
}

}  // namespace recordreplay
}  // namespace mozilla
