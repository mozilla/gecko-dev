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
#include "mozilla/dom/ContentProcessMessageManager.h"
#include "ChildInternal.h"
#include "InfallibleVector.h"
#include "JSControl.h"
#include "Monitor.h"
#include "ProcessRecordReplay.h"
#include "ProcessRedirect.h"
#include "rrIConnection.h"

#include <algorithm>

using std::min;

namespace mozilla {
namespace recordreplay {

const char* parent::CurrentFirefoxVersion() {
  return "74.0a1";
}

namespace parent {

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

static void ReadFileSync(const nsCString& aFile,
                         StaticInfallibleVector<char>& aContents) {
  FileHandle fd = DirectOpenFile(aFile.BeginReading(), false);

  char buf[4096];
  while (true) {
    size_t n = DirectRead(fd, buf, sizeof(buf));
    if (!n) {
      break;
    }
    aContents.append(buf, n);
  }

  DirectCloseFile(fd);
}

static StaticRefPtr<rrIConnection> gConnection;

static StaticInfallibleVector<char> gControlJS;
static StaticInfallibleVector<char> gReplayJS;

static bool StatusCallback(JSContext* aCx, unsigned aArgc, JS::Value* aVp);
static bool LoadedCallback(JSContext* aCx, unsigned aArgc, JS::Value* aVp);
static bool MessageCallback(JSContext* aCx, unsigned aArgc, JS::Value* aVp);

static nsString gCloudReplayStatus;

void EnsureUIStateInitialized() {
  if (!UseCloudForReplayingProcesses()) {
    if (!gControlJS.empty()) {
      return;
    }

    const char* path = getenv("WEBREPLAY_OFFLINE");
    if (!path) {
      gCloudReplayStatus.AssignLiteral("cloudNotSet.label");
      return;
    }

    ReadFileSync(nsPrintfCString("%s/control.js", path), gControlJS);
    ReadFileSync(nsPrintfCString("%s/replay.js", path), gReplayJS);
    return;
  }

  if (gConnection) {
    return;
  }

  nsAutoString cloudServer;
  Preferences::GetString("devtools.recordreplay.cloudServer", cloudServer);
  MOZ_RELEASE_ASSERT(cloudServer.Length() != 0);

  nsCOMPtr<rrIConnection> connection =
    do_ImportModule("resource://devtools/server/actors/replay/connection.js");
  gConnection = connection.forget();
  ClearOnShutdown(&gConnection);

  AutoSafeJSContext cx;
  JSAutoRealm ar(cx, xpc::PrivilegedJunkScope());
  JSFunction* fun;

  fun = JS_NewFunction(cx, StatusCallback, 1, 0, "StatusCallback");
  MOZ_RELEASE_ASSERT(fun);
  JS::RootedValue statusCallback(cx, JS::ObjectValue(*(JSObject*)fun));

  fun = JS_NewFunction(cx, LoadedCallback, 2, 0, "LoadedCallback");
  MOZ_RELEASE_ASSERT(fun);
  JS::RootedValue loadedCallback(cx, JS::ObjectValue(*(JSObject*)fun));

  fun = JS_NewFunction(cx, MessageCallback, 2, 0, "MessageCallback");
  MOZ_RELEASE_ASSERT(fun);
  JS::RootedValue messageCallback(cx, JS::ObjectValue(*(JSObject*)fun));

  if (NS_FAILED(gConnection->Initialize(cloudServer, statusCallback,
                                        loadedCallback, messageCallback))) {
    MOZ_CRASH("CreateReplayingCloudProcess");
  }

  gCloudReplayStatus.AssignLiteral("cloudConnecting.label");
}

void GetWebReplayJS(nsAutoCString& aControlJS, nsAutoCString& aReplayJS) {
  MOZ_RELEASE_ASSERT(gControlJS.length() && gReplayJS.length());

  aControlJS.SetLength(gControlJS.length());
  memcpy(aControlJS.BeginWriting(), gControlJS.begin(), gControlJS.length());

  aReplayJS.SetLength(gReplayJS.length());
  memcpy(aReplayJS.BeginWriting(), gReplayJS.begin(), gReplayJS.length());
}

void GetCloudReplayStatus(nsAString& aResult) {
  aResult = gCloudReplayStatus;
}

static PersistentRootedObject* gStatusCallback;

void SetCloudReplayStatusCallback(JS::HandleValue aCallback) {
  AutoSafeJSContext cx;

  if (!gStatusCallback) {
    gStatusCallback = new PersistentRootedObject(cx);
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
    JS::RootedValue arg(aCx, args.get(0));
    JS_WrapValue(aCx, &arg);
    JS::RootedObject thisv(aCx);
    JS::RootedValue fval(aCx, ObjectValue(**gStatusCallback));
    JS::RootedValue rv(aCx);
    if (!JS_CallFunctionValue(aCx, thisv, fval, JS::HandleValueArray(arg), &rv)) {
      return false;
    }
  }

  args.rval().setUndefined();
  return true;
}

static void ExtractJSString(JSContext* aCx, JSString* aString,
                            StaticInfallibleVector<char>& aBuffer) {
  MOZ_RELEASE_ASSERT(JS_StringHasLatin1Chars(aString));

  JS::AutoAssertNoGC nogc(aCx);
  size_t dataLength;
  const JS::Latin1Char* dataChars =
      JS_GetLatin1StringCharsAndLength(aCx, nogc, aString, &dataLength);
  MOZ_RELEASE_ASSERT(dataChars);

  aBuffer.append(dataChars, dataLength);
}

static bool LoadedCallback(JSContext* aCx, unsigned aArgc, JS::Value* aVp) {
  JS::CallArgs args = CallArgsFromVp(aArgc, aVp);

  if (!args.get(0).isString() || !args.get(1).isString()) {
    JS_ReportErrorASCII(aCx, "Expected strings");
    return false;
  }

  ExtractJSString(aCx, args.get(0).toString(), gControlJS);
  ExtractJSString(aCx, args.get(1).toString(), gReplayJS);

  args.rval().setUndefined();
  return true;
}

static PersistentRootedObject* gRecordingSavedCallback;

void SetCloudRecordingSavedCallback(JS::HandleValue aCallback) {
  AutoSafeJSContext cx;
  if (!gRecordingSavedCallback) {
    gRecordingSavedCallback = new PersistentRootedObject(cx);
  }
  *gRecordingSavedCallback = aCallback.isObject() ? &aCallback.toObject() : nullptr;
}

void CloudRecordingSaved(const nsAString& aUUID) {
  if (gRecordingSavedCallback && *gRecordingSavedCallback) {
    AutoSafeJSContext cx;
    JSAutoRealm ar(cx, *gRecordingSavedCallback);

    JS::AutoValueArray<2> args(cx);
    args[0].setString(js::ConvertStringToJSString(cx, aUUID));
    args[1].setBoolean(true);

    JS::RootedObject thisv(cx);
    JS::RootedValue fval(cx, ObjectValue(**gRecordingSavedCallback));
    JS::RootedValue rv(cx);
    if (!JS_CallFunctionValue(cx, thisv, fval, args, &rv)) {
      JS_ClearPendingException(cx);
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
// Child Processes
///////////////////////////////////////////////////////////////////////////////

// The single recording child process, or null.
static ChildProcessInfo* gRecordingChild;

// Any replaying child processes that have been spawned.
static StaticInfallibleVector<UniquePtr<ChildProcessInfo>> gReplayingChildren;

void Shutdown() {
  delete gRecordingChild;
  gReplayingChildren.clear();
  _exit(0);
}

ChildProcessInfo* GetChildProcess(size_t aId) {
  if (gRecordingChild && gRecordingChild->GetId() == aId) {
    return gRecordingChild;
  }
  for (const auto& child : gReplayingChildren) {
    if (child->GetId() == aId) {
      return child.get();
    }
  }
  return nullptr;
}

void SpawnReplayingChild(size_t aChannelId) {
  ChildProcessInfo* child = new ChildProcessInfo(aChannelId, Nothing());
  gReplayingChildren.append(child);
}

///////////////////////////////////////////////////////////////////////////////
// Preferences
///////////////////////////////////////////////////////////////////////////////

static bool gChromeRegistered;

void ChromeRegistered() {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  if (gChromeRegistered) {
    return;
  }
  gChromeRegistered = true;

  Maybe<size_t> recordingChildId;

  if (gRecordingChild) {
    recordingChildId.emplace(gRecordingChild->GetId());
  }

  js::SetupMiddlemanControl(recordingChildId);
}

///////////////////////////////////////////////////////////////////////////////
// Saving Recordings
///////////////////////////////////////////////////////////////////////////////

StaticInfallibleVector<char> gRecordingContents;

static void SaveRecordingInternal(const ipc::FileDescriptor& aFile) {
  // Make sure the recording file is up to date and ready for copying.
  js::BeforeSaveRecording();

  // Copy the recording's contents to the new file.
  ipc::FileDescriptor::UniquePlatformHandle writefd =
      aFile.ClonePlatformHandle();
  DirectWrite(writefd.get(), gRecordingContents.begin(),
              gRecordingContents.length());

  PrintSpew("Saved Recording Copy.\n");

  js::AfterSaveRecording();
}

void SaveRecording(const ipc::FileDescriptor& aFile) {
  MOZ_RELEASE_ASSERT(IsMiddleman());

  if (NS_IsMainThread()) {
    SaveRecordingInternal(aFile);
  } else {
    MainThreadMessageLoop()->PostTask(NewRunnableFunction(
        "SaveRecordingInternal", SaveRecordingInternal, aFile));
  }
}

void SaveCloudRecording(const nsAString& aUUID) {
  MOZ_RELEASE_ASSERT(IsMiddleman());
  js::SaveCloudRecording(aUUID);
}

///////////////////////////////////////////////////////////////////////////////
// Cloud Processes
///////////////////////////////////////////////////////////////////////////////

bool UseCloudForReplayingProcesses() {
  nsAutoString cloudServer;
  Preferences::GetString("devtools.recordreplay.cloudServer", cloudServer);
  return cloudServer.Length() != 0;
}

static StaticInfallibleVector<Channel*> gConnectionChannels;

class SendMessageToCloudRunnable : public Runnable {
 public:
  int32_t mConnectionId;
  Message::UniquePtr mMsg;

  SendMessageToCloudRunnable(int32_t aConnectionId, Message::UniquePtr aMsg)
      : Runnable("SendMessageToCloudRunnable"),
        mConnectionId(aConnectionId), mMsg(std::move(aMsg)) {}

  NS_IMETHODIMP Run() {
    AutoSafeJSContext cx;
    JSAutoRealm ar(cx, xpc::PrivilegedJunkScope());

    JS::RootedObject data(cx, JS::NewArrayBuffer(cx, mMsg->mSize));
    MOZ_RELEASE_ASSERT(data);

    {
      JS::AutoCheckCannotGC nogc;

      bool isSharedMemory;
      uint8_t* ptr = JS::GetArrayBufferData(data, &isSharedMemory, nogc);
      MOZ_RELEASE_ASSERT(ptr);

      memcpy(ptr, mMsg.get(), mMsg->mSize);
    }

    JS::RootedValue dataValue(cx, JS::ObjectValue(*data));
    if (NS_FAILED(gConnection->SendMessage(mConnectionId, dataValue))) {
      MOZ_CRASH("SendMessageToCloud");
    }

    return NS_OK;
  }
};

static bool MessageCallback(JSContext* aCx, unsigned aArgc, JS::Value* aVp) {
  JS::CallArgs args = CallArgsFromVp(aArgc, aVp);

  if (!args.get(0).isNumber()) {
    JS_ReportErrorASCII(aCx, "Expected number");
    return false;
  }
  size_t id = args.get(0).toNumber();
  if (id >= gConnectionChannels.length() || !gConnectionChannels[id]) {
    JS_ReportErrorASCII(aCx, "Bad connection channel ID");
    return false;
  }

  if (!args.get(1).isObject()) {
    JS_ReportErrorASCII(aCx, "Expected object");
    return false;
  }

  bool sentData = false;
  {
    JS::AutoCheckCannotGC nogc;

    uint32_t length;
    uint8_t* ptr;
    bool isSharedMemory;
    JS::GetArrayBufferLengthAndData(&args.get(1).toObject(), &length,
                                    &isSharedMemory, &ptr);

    if (ptr) {
      Channel* channel = gConnectionChannels[id];
      channel->SendMessageData((const char*) ptr, length);
      sentData = true;
    }
  }
  if (!sentData) {
    JS_ReportErrorASCII(aCx, "Expected array buffer");
    return false;
  }

  args.rval().setUndefined();
  return true;
}

void CreateReplayingCloudProcess(base::ProcessId aProcessId,
                                 uint32_t aChannelId) {
  MOZ_RELEASE_ASSERT(XRE_IsParentProcess());
  MOZ_RELEASE_ASSERT(gConnection);

  AutoSafeJSContext cx;
  JSAutoRealm ar(cx, xpc::PrivilegedJunkScope());

  int32_t connectionId;
  if (NS_FAILED(gConnection->Connect(aChannelId, &connectionId))) {
    MOZ_CRASH("CreateReplayingCloudProcess");
  }

  Channel* channel = new Channel(
      aChannelId, Channel::Kind::ParentCloud,
      [=](Message::UniquePtr aMsg) {
        RefPtr<SendMessageToCloudRunnable> runnable =
          new SendMessageToCloudRunnable(connectionId, std::move(aMsg));
        NS_DispatchToMainThread(runnable);
      }, aProcessId);
  while ((size_t)connectionId >= gConnectionChannels.length()) {
    gConnectionChannels.append(nullptr);
  }
  gConnectionChannels[connectionId] = channel;
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
bool gActiveChildIsRecording;

static void ExtractCloudRecordingName(const char* aFileName,
                                      nsAutoCString& aRecordingName) {
  const char prefix[] = "cloud-replay://";
  if (!strncmp(aFileName, prefix, strlen(prefix))) {
    aRecordingName = nsCString(aFileName + strlen(prefix));
  }
}

void InitializeMiddleman(int aArgc, char* aArgv[], base::ProcessId aParentPid,
                         const base::SharedMemoryHandle& aPrefsHandle,
                         const ipc::FileDescriptor& aPrefMapHandle) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  CrashReporter::AnnotateCrashReport(CrashReporter::Annotation::RecordReplay,
                                     true);

  gParentPid = aParentPid;

  // Construct the message that will be sent to each child when starting up.
  IntroductionMessage* msg = IntroductionMessage::New(aParentPid, aArgc, aArgv);
  GetCurrentBuildId(&msg->mBuildId);

  ChildProcessInfo::SetIntroductionMessage(msg);

  MOZ_RELEASE_ASSERT(gProcessKind == ProcessKind::MiddlemanRecording ||
                     gProcessKind == ProcessKind::MiddlemanReplaying);

  InitializeGraphicsMemory();

  gMonitor = new Monitor();

  gMainThreadMessageLoop = MessageLoop::current();

  if (gProcessKind == ProcessKind::MiddlemanRecording) {
    RecordingProcessData data(aPrefsHandle, aPrefMapHandle);
    gRecordingChild = new ChildProcessInfo(0, Some(data));
    gActiveChildIsRecording = true;
  }

  InitializeForwarding();

  if (gProcessKind == ProcessKind::MiddlemanReplaying) {
    nsAutoCString cloudRecordingName;
    ExtractCloudRecordingName(gRecordingFilename, cloudRecordingName);
    if (cloudRecordingName.Length()) {
      SetBuildId(&msg->mBuildId, "cloud", cloudRecordingName.get());
    } else {
      // Load the entire recording into memory.
      ReadFileSync(nsCString(gRecordingFilename), gRecordingContents);

      // Update the build ID in the introduction message according to what we
      // find in the recording. The introduction message is sent first to each
      // replaying process, and when replaying in the cloud its contents will be
      // analyzed to determine what binaries to use for the replay.
      Recording::ExtractBuildId(gRecordingContents.begin(),
                                gRecordingContents.length(), &msg->mBuildId);
    }
  }
}

}  // namespace parent
}  // namespace recordreplay
}  // namespace mozilla
