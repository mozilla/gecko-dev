/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file has the logic which the middleman process uses to communicate with
// the parent process and with the replayed process.

#include "ParentInternal.h"

#include "base/task.h"
#include "ipc/Channel.h"
#include "js/Proxy.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/ContentProcessMessageManager.h"
#include "mozilla/dom/WorkerRunnable.h"
#include "ChildInternal.h"
#include "InfallibleVector.h"
#include "JSControl.h"
#include "Monitor.h"
#include "ProcessRecordReplay.h"
#include "ProcessRedirect.h"
#include "rrIConnection.h"

#include "mozilla/ClearOnShutdown.h"
#include "nsImportModule.h"

#include <algorithm>

using std::min;

namespace mozilla {
namespace recordreplay {

const char* parent::CurrentFirefoxVersion() {
  return "74.0a1";
}

namespace parent {

// Used in parent and middleman processes.
static TimeStamp gStartupTime;

///////////////////////////////////////////////////////////////////////////////
// UI Process State
///////////////////////////////////////////////////////////////////////////////

const char* gSaveAllRecordingsDirectory = nullptr;

void InitializeUIProcess(int aArgc, char** aArgv) {
  for (int i = 0; i < aArgc; i++) {
    if (!strcmp(aArgv[i], "--save-recordings") && i + 1 < aArgc) {
      gSaveAllRecordingsDirectory = strdup(aArgv[i + 1]);
    }
  }
}

const char* SaveAllRecordingsDirectory() {
  MOZ_RELEASE_ASSERT(XRE_IsParentProcess());
  return gSaveAllRecordingsDirectory;
}

static StaticRefPtr<rrIConnection> gConnection;

static bool StatusCallback(JSContext* aCx, unsigned aArgc, JS::Value* aVp);

static const JSFunctionSpec gCallbacks[] = {
  JS_FN("updateStatus", StatusCallback, 1, 0),
  JS_FS_END
};

static nsString gCloudReplayStatus;

static bool gUIStateInitialized;

void EnsureUIStateInitialized() {
  if (gUIStateInitialized) {
    return;
  }
  gUIStateInitialized = true;
  MOZ_RELEASE_ASSERT(!gConnection);

  gStartupTime = TimeStamp::Now();

  nsCOMPtr<rrIConnection> connection =
    do_ImportModule("resource://devtools/server/actors/replay/connection.js");
  gConnection = connection.forget();
  ClearOnShutdown(&gConnection);

  AutoSafeJSContext cx;
  JSAutoRealm ar(cx, xpc::PrivilegedJunkScope());

  JS::RootedObject callbacks(cx, JS_NewObject(cx, nullptr));
  MOZ_RELEASE_ASSERT(callbacks);

  if (!JS_DefineFunctions(cx, callbacks, gCallbacks)) {
    MOZ_CRASH("EnsureUIStateInitialized");
  }

  JS::RootedValue callbacksValue(cx, JS::ObjectValue(*callbacks));
  if (NS_FAILED(gConnection->Initialize(callbacksValue))) {
    MOZ_CRASH("EnsureUIStateInitialized");
  }

  gCloudReplayStatus.AssignLiteral("cloudConnecting.label");
}

void GetCloudReplayStatus(nsAString& aResult) {
  aResult = gCloudReplayStatus;
}

static JS::PersistentRootedObject* gStatusCallback;

void SetCloudReplayStatusCallback(JS::HandleValue aCallback) {
  AutoSafeJSContext cx;

  if (!gStatusCallback) {
    gStatusCallback = new JS::PersistentRootedObject(cx);
  }

  *gStatusCallback = aCallback.isObject() ? &aCallback.toObject() : nullptr;
}

static bool StatusCallback(JSContext* aCx, unsigned aArgc, JS::Value* aVp) {
  JS::CallArgs args = CallArgsFromVp(aArgc, aVp);

  if (!args.get(0).isString()) {
    JS_ReportErrorASCII(aCx, "Expected string");
    return false;
  }

  nsAutoCString status;
  js::ConvertJSStringToCString(aCx, args.get(0).toString(), status);
  gCloudReplayStatus = NS_ConvertUTF8toUTF16(status);

  if (gStatusCallback && *gStatusCallback) {
    JSAutoRealm ar(aCx, *gStatusCallback);

    JS::AutoValueArray<3> newArgs(aCx);
    newArgs[0].set(args.get(0));
    newArgs[1].set(args.get(1));
    newArgs[2].set(args.get(2));

    JS_WrapValue(aCx, newArgs[0]);
    JS_WrapValue(aCx, newArgs[1]);
    JS_WrapValue(aCx, newArgs[2]);

    JS::RootedObject thisv(aCx);
    JS::RootedValue fval(aCx, JS::ObjectValue(**gStatusCallback));
    JS::RootedValue rv(aCx);
    if (!JS_CallFunctionValue(aCx, thisv, fval, newArgs, &rv)) {
      return false;
    }
  }

  args.rval().setUndefined();
  return true;
}

double ElapsedTime() {
  return (TimeStamp::Now() - gStartupTime).ToSeconds();
}

void ContentParentDestroyed(int32_t aPid) {
  MOZ_RELEASE_ASSERT(gUIStateInitialized);

  AutoSafeJSContext cx;
  JSAutoRealm ar(cx, xpc::PrivilegedJunkScope());

  if (NS_FAILED(gConnection->RecordingDestroyed(aPid))) {
    MOZ_CRASH("ContentParentDestroyed");
  }
}

///////////////////////////////////////////////////////////////////////////////
// Child Processes
///////////////////////////////////////////////////////////////////////////////

// The single recording child process, or null.
ChildProcessInfo* gRecordingChild;

void Shutdown() {
  delete gRecordingChild;
  _exit(0);
}

///////////////////////////////////////////////////////////////////////////////
// Initialization
///////////////////////////////////////////////////////////////////////////////

// Message loop processed on the main thread.
static MessageLoop* gMainThreadMessageLoop;

MessageLoop* MainThreadMessageLoop() { return gMainThreadMessageLoop; }

static base::ProcessId gParentPid;

base::ProcessId ParentProcessId() { return gParentPid; }

Monitor* gMonitor;

void InitializeMiddleman(int aArgc, char* aArgv[], base::ProcessId aParentPid,
                         const base::SharedMemoryHandle& aPrefsHandle,
                         const ipc::FileDescriptor& aPrefMapHandle) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  gStartupTime = TimeStamp::Now();

  CrashReporter::AnnotateCrashReport(CrashReporter::Annotation::RecordReplay,
                                     true);

  gParentPid = aParentPid;

  // Construct the message that will be sent to each child when starting up.
  IntroductionMessage* msg = IntroductionMessage::New(aParentPid, aArgc, aArgv);

  ChildProcessInfo::SetIntroductionMessage(msg);

  MOZ_RELEASE_ASSERT(gProcessKind == ProcessKind::MiddlemanRecording);

  InitializeGraphicsMemory();

  gMonitor = new Monitor();

  gMainThreadMessageLoop = MessageLoop::current();

  RecordingProcessData data(aPrefsHandle, aPrefMapHandle);
  gRecordingChild = new ChildProcessInfo(0, Some(data), 0);

  InitializeForwarding();
}

}  // namespace parent
}  // namespace recordreplay
}  // namespace mozilla
