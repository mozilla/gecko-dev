/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ParentInternal.h"

#include "ChildInternal.h"
#include "JSControl.h"

#include "base/task.h"
#include "mozilla/dom/ContentChild.h"
#include "Thread.h"

namespace mozilla {
namespace recordreplay {
namespace parent {

// A saved introduction message for sending to all children.
static IntroductionMessage* gIntroductionMessage;

/* static */
void ChildProcessInfo::SetIntroductionMessage(IntroductionMessage* aMessage) {
  gIntroductionMessage = aMessage;
}

ChildProcessInfo::ChildProcessInfo(
    size_t aId, const Maybe<RecordingProcessData>& aRecordingProcessData,
    size_t aInitialReplayingLength)
    : mRecording(aRecordingProcessData.isSome()) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  Channel::Kind kind =
    IsRecording() ? Channel::Kind::MiddlemanRecord : Channel::Kind::MiddlemanReplay;
  mChannel = new Channel(aId, kind, [=](Message::UniquePtr aMsg) {
        ReceiveChildMessageOnMainThread(aId, std::move(aMsg));
      });

  LaunchSubprocess(aId, aRecordingProcessData, aInitialReplayingLength);
}

ChildProcessInfo::~ChildProcessInfo() {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  SendMessage(TerminateMessage(0));
}

void ChildProcessInfo::OnIncomingMessage(const Message& aMsg, double aDelay) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  switch (aMsg.mType) {
    case MessageType::FatalError: {
      const FatalErrorMessage& nmsg =
          static_cast<const FatalErrorMessage&>(aMsg);
      OnCrash(nmsg.mForkId, nmsg.Error());
      return;
    }
    case MessageType::Paint:
      UpdateGraphicsAfterPaint(static_cast<const PaintMessage&>(aMsg));
      break;
    default:
      break;
  }
}

void ChildProcessInfo::SendMessage(Message&& aMsg) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  mChannel->SendMessage(std::move(aMsg));
}

///////////////////////////////////////////////////////////////////////////////
// Subprocess Management
///////////////////////////////////////////////////////////////////////////////

ipc::GeckoChildProcessHost* gRecordingProcess;

void GetArgumentsForChildProcess(base::ProcessId aMiddlemanPid,
                                 uint32_t aChannelId,
                                 const char* aRecordingFile, bool aRecording,
                                 std::vector<std::string>& aExtraArgs) {
  MOZ_RELEASE_ASSERT(IsMiddleman() || XRE_IsParentProcess());

  aExtraArgs.push_back(gMiddlemanPidOption);
  aExtraArgs.push_back(nsPrintfCString("%d", aMiddlemanPid).get());

  aExtraArgs.push_back(gChannelIDOption);
  aExtraArgs.push_back(nsPrintfCString("%d", (int)aChannelId).get());

  aExtraArgs.push_back(gProcessKindOption);
  aExtraArgs.push_back(nsPrintfCString("%d", aRecording
                                                 ? (int)ProcessKind::Recording
                                                 : (int)ProcessKind::Replaying)
                           .get());

  aExtraArgs.push_back(gRecordingFileOption);
  aExtraArgs.push_back(aRecordingFile);
}

void ChildProcessInfo::LaunchSubprocess(
    size_t aChannelId,
    const Maybe<RecordingProcessData>& aRecordingProcessData,
    size_t aInitialReplayingLength) {
  MOZ_RELEASE_ASSERT(IsRecording() == aRecordingProcessData.isSome());
  MOZ_RELEASE_ASSERT(IsRecording());

  MOZ_RELEASE_ASSERT(gIntroductionMessage);
  SendMessage(std::move(*gIntroductionMessage));

  std::vector<std::string> extraArgs;
  GetArgumentsForChildProcess(base::GetCurrentProcId(), aChannelId,
                              gRecordingFilename, /* aRecording = */ true,
                              extraArgs);

  MOZ_RELEASE_ASSERT(!gRecordingProcess);
  gRecordingProcess = new ipc::GeckoChildProcessHost(GeckoProcessType_Content);

  // Preferences data is conveyed to the recording process via fixed file
  // descriptors on macOS.
  gRecordingProcess->AddFdToRemap(aRecordingProcessData.ref().mPrefsHandle.fd,
                                  kPrefsFileDescriptor);
  ipc::FileDescriptor::UniquePlatformHandle prefMapHandle =
    aRecordingProcessData.ref().mPrefMapHandle.ClonePlatformHandle();
  gRecordingProcess->AddFdToRemap(prefMapHandle.get(),
                                  kPrefMapFileDescriptor);

  if (!gRecordingProcess->LaunchAndWaitForProcessHandle(extraArgs)) {
    MOZ_CRASH("ChildProcessInfo::LaunchSubprocess");
  }

  SendGraphicsMemoryToChild();
}

void ChildProcessInfo::OnCrash(size_t aForkId, const char* aWhy) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  // If a child process crashes or hangs then annotate the crash report.
  CrashReporter::AnnotateCrashReport(
      CrashReporter::Annotation::RecordReplayError, nsAutoCString(aWhy));

  // Shut down cleanly so that we don't mask the report with our own crash.
  Shutdown();
}

///////////////////////////////////////////////////////////////////////////////
// Handling Channel Messages
///////////////////////////////////////////////////////////////////////////////

// When messages are received from child processes, we want their handler to
// execute on the main thread. The main thread might be blocked in WaitUntil,
// so runnables associated with child processes have special handling.

// All messages received on a channel thread which the main thread has not
// processed yet. This is protected by gMonitor.
struct PendingMessage {
  size_t mChildId = 0;
  Message::UniquePtr mMsg;
  double mTime = 0;

  PendingMessage() {}

  PendingMessage& operator=(PendingMessage&& aOther) {
    mChildId = aOther.mChildId;
    mMsg = std::move(aOther.mMsg);
    mTime = aOther.mTime;
    return *this;
  }

  PendingMessage(PendingMessage&& aOther) { *this = std::move(aOther); }
};
static StaticInfallibleVector<PendingMessage> gPendingMessages;

static Message::UniquePtr ExtractChildMessage(ChildProcessInfo** aProcess,
                                              double* aDelay) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  if (!gPendingMessages.length()) {
    return nullptr;
  }

  PendingMessage& pending = gPendingMessages[0];
  *aProcess = gRecordingChild;
  MOZ_RELEASE_ASSERT(*aProcess);

  *aDelay = ElapsedTime() - pending.mTime;

  Message::UniquePtr msg = std::move(pending.mMsg);
  gPendingMessages.erase(&pending);

  return msg;
}

/* static */
void ChildProcessInfo::MaybeProcessNextMessage() {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  MaybeHandleForwardedMessages();

  Maybe<MonitorAutoLock> lock;
  lock.emplace(*gMonitor);

  ChildProcessInfo* process;
  double delay;
  Message::UniquePtr msg = ExtractChildMessage(&process, &delay);

  if (msg) {
    lock.reset();
    process->OnIncomingMessage(*msg, delay);
  } else {
    // Limit how long we are willing to wait before returning to the caller.
    TimeStamp deadline = TimeStamp::Now() + TimeDuration::FromMilliseconds(200);
    gMonitor->WaitUntil(deadline);
  }
}

// Whether there is a pending task on the main thread's message loop to handle
// all pending messages.
static bool gHasPendingMessageRunnable;

// Runnable created on the main thread to handle any tasks sent by the replay
// message loop thread which were not handled while the main thread was blocked.
/* static */
void ChildProcessInfo::MaybeProcessPendingMessageRunnable() {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  MonitorAutoLock lock(*gMonitor);
  MOZ_RELEASE_ASSERT(gHasPendingMessageRunnable);

  ChildProcessInfo* process = nullptr;
  double delay;
  Message::UniquePtr msg = ExtractChildMessage(&process, &delay);

  if (msg) {
    MainThreadMessageLoop()->PostTask(
        NewRunnableFunction("MaybeProcessPendingMessageRunnable",
                            MaybeProcessPendingMessageRunnable));

    MonitorAutoUnlock unlock(*gMonitor);
    process->OnIncomingMessage(*msg, delay);
  } else {
    gHasPendingMessageRunnable = false;
  }
}

// Execute a task that processes a message received from the child. This is
// called on a channel thread, and the function executes asynchronously on
// the main thread.
  /* static */ void ChildProcessInfo::ReceiveChildMessageOnMainThread(
    size_t aChildId, Message::UniquePtr aMsg) {
  MOZ_RELEASE_ASSERT(!NS_IsMainThread());

  MonitorAutoLock lock(*gMonitor);

  PendingMessage pending;
  pending.mChildId = aChildId;
  pending.mMsg = std::move(aMsg);
  pending.mTime = ElapsedTime();
  gPendingMessages.append(std::move(pending));

  // Notify the main thread, if it is waiting in WaitUntilPaused.
  gMonitor->NotifyAll();

  // Make sure there is a task on the main thread's message loop that can
  // process this task if necessary.
  if (!gHasPendingMessageRunnable) {
    gHasPendingMessageRunnable = true;
    MainThreadMessageLoop()->PostTask(
        NewRunnableFunction("MaybeProcessPendingMessageRunnable",
                            MaybeProcessPendingMessageRunnable));
  }
}

}  // namespace parent
}  // namespace recordreplay
}  // namespace mozilla
