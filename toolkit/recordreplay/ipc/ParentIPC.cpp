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

// Used in parent and middleman processes.
static TimeStamp gStartupTime;

// Used in all processes.
AtomicBool gLoggingEnabled;

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
static bool ConnectedCallback(JSContext* aCx, unsigned aArgc, JS::Value* aVp);
static bool DisconnectedCallback(JSContext* aCx, unsigned aArgc, JS::Value* aVp);

static const JSFunctionSpec gCallbacks[] = {
  JS_FN("updateStatus", StatusCallback, 1, 0),
  JS_FN("loadedJS", LoadedCallback, 3, 0),
  JS_FN("onMessage", MessageCallback, 2, 0),
  JS_FN("onConnected", ConnectedCallback, 1, 0),
  JS_FN("onDisconnected", DisconnectedCallback, 1, 0),
  JS_FS_END
};

static nsString gCloudReplayStatus;

bool UseCloudForReplayingProcesses() {
  if (getenv("WEBREPLAY_OFFLINE")) {
    return false;
  }

  nsAutoString cloudServer;
  Preferences::GetString("devtools.recordreplay.cloudServer", cloudServer);
  return cloudServer.Length() != 0;
}

static bool gUIStateInitialized;

void EnsureUIStateInitialized() {
  if (gUIStateInitialized) {
    return;
  }
  gUIStateInitialized = true;
  MOZ_RELEASE_ASSERT(!gConnection);

  gStartupTime = TimeStamp::Now();

  if (Preferences::GetBool("devtools.recordreplay.logging.enabled")) {
    gLoggingEnabled = true;
  }

  const char* sourcesPath = getenv("WEBREPLAY_SOURCES");
  if (sourcesPath && gControlJS.empty()) {
    ReadFileSync(nsPrintfCString("%s/control.js", sourcesPath), gControlJS);
    ReadFileSync(nsPrintfCString("%s/replay.js", sourcesPath), gReplayJS);
  }

  if (!UseCloudForReplayingProcesses()) {
    if (!sourcesPath) {
      gCloudReplayStatus.AssignLiteral("cloudNotSet.label");
    }
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

  JS::RootedObject callbacks(cx, JS_NewObject(cx, nullptr));
  MOZ_RELEASE_ASSERT(callbacks);

  if (!JS_DefineFunctions(cx, callbacks, gCallbacks)) {
    MOZ_CRASH("EnsureUIStateInitialized");
  }

  JS::RootedValue callbacksValue(cx, JS::ObjectValue(*callbacks));
  if (NS_FAILED(gConnection->Initialize(cloudServer, callbacksValue))) {
    MOZ_CRASH("EnsureUIStateInitialized");
  }

  gCloudReplayStatus.AssignLiteral("cloudConnecting.label");
}

void GetWebReplayJS(nsAutoCString& aControlJS, nsAutoCString& aReplayJS) {
  if (!gControlJS.length() || !gReplayJS.length()) {
    fprintf(stderr, "Control/Replay JS not set, crashing...\n");
    MOZ_CRASH("Control/Replay JS not set");
  }

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

    JS::AutoValueArray<3> newArgs(aCx);
    newArgs[0].set(args.get(0));
    newArgs[1].set(args.get(1));
    newArgs[2].set(args.get(2));

    JS_WrapValue(aCx, newArgs[0]);
    JS_WrapValue(aCx, newArgs[1]);
    JS_WrapValue(aCx, newArgs[2]);

    JS::RootedObject thisv(aCx);
    JS::RootedValue fval(aCx, ObjectValue(**gStatusCallback));
    JS::RootedValue rv(aCx);
    if (!JS_CallFunctionValue(aCx, thisv, fval, newArgs, &rv)) {
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

  aBuffer.clear();
  aBuffer.append(dataChars, dataLength);
}

// ID which has been assigned to this browser session by the cloud server.
nsAutoCString gSessionId;

static bool LoadedCallback(JSContext* aCx, unsigned aArgc, JS::Value* aVp) {
  JS::CallArgs args = CallArgsFromVp(aArgc, aVp);

  if (!args.get(0).isString() ||
      !args.get(1).isString() ||
      !args.get(2).isString()) {
    JS_ReportErrorASCII(aCx, "Expected strings");
    return false;
  }

  js::ConvertJSStringToCString(aCx, args.get(0).toString(), gSessionId);

  if (!getenv("WEBREPLAY_SOURCES")) {
    ExtractJSString(aCx, args.get(1).toString(), gControlJS);
    ExtractJSString(aCx, args.get(2).toString(), gReplayJS);
  }

  args.rval().setUndefined();
  return true;
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
// Preferences / Logging
///////////////////////////////////////////////////////////////////////////////

static bool gChromeRegistered;

void ChromeRegistered() {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  MOZ_RELEASE_ASSERT(IsMiddleman());

  if (gChromeRegistered) {
    return;
  }
  gChromeRegistered = true;

  if (Preferences::GetBool("devtools.recordreplay.logging.enabled")) {
    gLoggingEnabled = true;
    if (gRecordingChild) {
      gRecordingChild->SendMessage(EnableLoggingMessage());
    }
  }

  Maybe<size_t> recordingChildId;

  if (gRecordingChild) {
    recordingChildId.emplace(gRecordingChild->GetId());
  }

  js::SetupMiddlemanControl(recordingChildId);
}

static void LogFromUIProcess(const nsACString& aText);

void AddToLog(const nsAString& aText, bool aIncludePrefix /* = true */) {
  if (!gLoggingEnabled) {
    return;
  }

  if (IsRecordingOrReplaying()) {
    child::PrintLog(aText);
    return;
  }

  nsCString text;
  if (aIncludePrefix) {
    double elapsed = (TimeStamp::Now() - gStartupTime).ToSeconds();
    text = nsPrintfCString("[%s %.2f] %s\n",
                           XRE_IsParentProcess() ? "UI" : "Control",
                           elapsed, NS_ConvertUTF16toUTF8(aText).get());
  } else {
    text = NS_ConvertUTF16toUTF8(aText);
  }

  if (XRE_IsParentProcess()) {
    LogFromUIProcess(text);
    return;
  }

  MOZ_RELEASE_ASSERT(IsMiddleman());

  for (const auto& child : gReplayingChildren) {
    if (child) {
      UniquePtr<Message> msg(LogTextMessage::New(
          0, 0, text.BeginReading(), text.Length() + 1));
      child->SendMessage(std::move(*msg));
    }
  }
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

// In the UI process, all replayer cloud connections in existence.
struct ConnectionChannel {
  // ContentParent hosting the middleman.
  dom::ContentParent* mParent = nullptr;

  // Channel for sending messages to the middleman.
  Channel* mChannel = nullptr;

  // Whether this connection is established, and can be used for logging
  // messages originating from this process.
  bool mConnected = false;
};

static StaticInfallibleVector<ConnectionChannel> gConnectionChannels;

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

static ConnectionChannel* GetConnectionChannel(JSContext* aCx,
                                               JS::HandleValue aValue) {
  if (!aValue.isNumber()) {
    JS_ReportErrorASCII(aCx, "Expected number");
    return nullptr;
  }
  size_t id = aValue.toNumber();
  if (id >= gConnectionChannels.length() || !gConnectionChannels[id].mChannel) {
    JS_ReportErrorASCII(aCx, "Bad connection channel ID");
    return nullptr;
  }
  return &gConnectionChannels[id];
}

static bool MessageCallback(JSContext* aCx, unsigned aArgc, JS::Value* aVp) {
  JS::CallArgs args = CallArgsFromVp(aArgc, aVp);

  ConnectionChannel* info = GetConnectionChannel(aCx, args.get(0));
  if (!info) {
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
      Channel* channel = info->mChannel;
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

void CreateReplayingCloudProcess(dom::ContentParent* aParent, uint32_t aChannelId) {
  MOZ_RELEASE_ASSERT(XRE_IsParentProcess());
  MOZ_RELEASE_ASSERT(gConnection);

  AutoSafeJSContext cx;
  JSAutoRealm ar(cx, xpc::PrivilegedJunkScope());

  int32_t connectionId;
  if (NS_FAILED(gConnection->Connect(aChannelId, &connectionId))) {
    MOZ_CRASH("CreateReplayingCloudProcess");
  }

  base::ProcessId pid = aParent->Pid();

  Channel* channel = new Channel(
      aChannelId, Channel::Kind::ParentCloud,
      [=](Message::UniquePtr aMsg) {
        RefPtr<SendMessageToCloudRunnable> runnable =
          new SendMessageToCloudRunnable(connectionId, std::move(aMsg));
        NS_DispatchToMainThread(runnable);
      }, pid);
  while ((size_t)connectionId >= gConnectionChannels.length()) {
    gConnectionChannels.emplaceBack();
  }
  ConnectionChannel& info = gConnectionChannels[connectionId];
  info.mParent = aParent;
  info.mChannel = channel;
}

void ContentParentDestroyed(dom::ContentParent* aParent) {
  for (auto& info : gConnectionChannels) {
    if (info.mParent == aParent) {
      info.mParent = nullptr;
      delete info.mChannel;
      info.mChannel = nullptr;
      info.mConnected = false;
    }
  }
}

static bool ConnectedCallback(JSContext* aCx, unsigned aArgc, JS::Value* aVp) {
  JS::CallArgs args = CallArgsFromVp(aArgc, aVp);

  ConnectionChannel* info = GetConnectionChannel(aCx, args.get(0));
  if (!info) {
    return false;
  }

  info->mConnected = true;

  args.rval().setUndefined();
  return true;
}

static bool DisconnectedCallback(JSContext* aCx, unsigned aArgc, JS::Value* aVp) {
  JS::CallArgs args = CallArgsFromVp(aArgc, aVp);

  ConnectionChannel* info = GetConnectionChannel(aCx, args.get(0));
  if (info) {
    info->mParent = nullptr;
    delete info->mChannel;
    info->mChannel = nullptr;
    info->mConnected = false;
  } else {
    JS_ClearPendingException(aCx);
  }

  args.rval().setUndefined();
  return true;
}

static void LogFromUIProcess(const nsACString& aText) {
  AutoSafeJSContext cx;
  JSAutoRealm ar(cx, xpc::PrivilegedJunkScope());

  if (NS_FAILED(gConnection->AddToLog(aText))) {
    MOZ_CRASH("LogFromUIProcess");
  }
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
  const char prefix[] = "webreplay://";
  if (!strncmp(aFileName, prefix, strlen(prefix))) {
    aRecordingName = nsCString(aFileName + strlen(prefix));
  }
}

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
