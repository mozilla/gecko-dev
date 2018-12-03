/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ParentInternal.h"

#include "base/task.h"
#include "mozilla/dom/ContentChild.h"
#include "Thread.h"

namespace mozilla {
namespace recordreplay {
namespace parent {

// A saved introduction message for sending to all children.
static IntroductionMessage* gIntroductionMessage;

// How many channels have been constructed so far.
static size_t gNumChannels;

// Whether children might be debugged and should not be treated as hung.
static bool gChildrenAreDebugging;

/* static */ void ChildProcessInfo::SetIntroductionMessage(
    IntroductionMessage* aMessage) {
  gIntroductionMessage = aMessage;
}

ChildProcessInfo::ChildProcessInfo(
    UniquePtr<ChildRole> aRole,
    const Maybe<RecordingProcessData>& aRecordingProcessData)
    : mChannel(nullptr),
      mRecording(aRecordingProcessData.isSome()),
      mRecoveryStage(RecoveryStage::None),
      mPaused(false),
      mPausedMessage(nullptr),
      mLastCheckpoint(CheckpointId::Invalid),
      mNumRecoveredMessages(0),
      mRole(std::move(aRole)),
      mPauseNeeded(false),
      mHasBegunFatalError(false),
      mHasFatalError(false) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  static bool gFirst = false;
  if (!gFirst) {
    gFirst = true;
    gChildrenAreDebugging = !!getenv("WAIT_AT_START");
  }

  mRole->SetProcess(this);

  LaunchSubprocess(aRecordingProcessData);

  // Replaying processes always save the first checkpoint, if saving
  // checkpoints is allowed. This is currently assumed by the rewinding
  // mechanism in the replaying process, and would be nice to investigate
  // removing.
  if (!IsRecording() && CanRewind()) {
    SendMessage(SetSaveCheckpointMessage(CheckpointId::First, true));
  }

  mRole->Initialize();
}

ChildProcessInfo::~ChildProcessInfo() {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  if (IsRecording()) {
    SendMessage(TerminateMessage());
  }
}

ChildProcessInfo::Disposition ChildProcessInfo::GetDisposition() {
  // We can determine the disposition of the child by looking at the first
  // resume message sent since the last time it reached a checkpoint.
  for (Message* msg : mMessages) {
    if (msg->mType == MessageType::Resume) {
      const ResumeMessage& nmsg = static_cast<const ResumeMessage&>(*msg);
      return nmsg.mForward ? AfterLastCheckpoint : BeforeLastCheckpoint;
    }
  }
  return AtLastCheckpoint;
}

bool ChildProcessInfo::IsPausedAtCheckpoint() {
  return IsPaused() && mPausedMessage->mType == MessageType::HitCheckpoint;
}

bool ChildProcessInfo::IsPausedAtRecordingEndpoint() {
  if (!IsPaused()) {
    return false;
  }
  if (mPausedMessage->mType == MessageType::HitCheckpoint) {
    return static_cast<HitCheckpointMessage*>(mPausedMessage)
        ->mRecordingEndpoint;
  }
  if (mPausedMessage->mType == MessageType::HitBreakpoint) {
    return static_cast<HitBreakpointMessage*>(mPausedMessage)
        ->mRecordingEndpoint;
  }
  return false;
}

void ChildProcessInfo::GetInstalledBreakpoints(
    InfallibleVector<AddBreakpointMessage*>& aBreakpoints) {
  MOZ_RELEASE_ASSERT(aBreakpoints.empty());
  for (Message* msg : mMessages) {
    if (msg->mType == MessageType::AddBreakpoint) {
      aBreakpoints.append(static_cast<AddBreakpointMessage*>(msg));
    } else if (msg->mType == MessageType::ClearBreakpoints) {
      aBreakpoints.clear();
    }
  }
}

void ChildProcessInfo::AddMajorCheckpoint(size_t aId) {
  // Major checkpoints should be listed in order.
  MOZ_RELEASE_ASSERT(mMajorCheckpoints.empty() ||
                     aId > mMajorCheckpoints.back());
  mMajorCheckpoints.append(aId);
}

void ChildProcessInfo::SetRole(UniquePtr<ChildRole> aRole) {
  MOZ_RELEASE_ASSERT(!IsRecovering());

  PrintSpew("SetRole:%d %s\n", (int)GetId(),
            ChildRole::TypeString(aRole->GetType()));

  mRole = std::move(aRole);
  mRole->SetProcess(this);
  mRole->Initialize();
}

void ChildProcessInfo::OnIncomingMessage(size_t aChannelId,
                                         const Message& aMsg) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  // Ignore messages from channels for subprocesses we terminated already.
  if (aChannelId != mChannel->GetId()) {
    return;
  }

  // Always handle fatal errors in the same way.
  if (aMsg.mType == MessageType::BeginFatalError) {
    mHasBegunFatalError = true;
    return;
  } else if (aMsg.mType == MessageType::FatalError) {
    mHasFatalError = true;
    const FatalErrorMessage& nmsg = static_cast<const FatalErrorMessage&>(aMsg);
    OnCrash(nmsg.Error());
    return;
  }

  mLastMessageTime = TimeStamp::Now();

  if (IsRecovering()) {
    OnIncomingRecoveryMessage(aMsg);
    return;
  }

  // Update paused state.
  MOZ_RELEASE_ASSERT(!IsPaused());
  switch (aMsg.mType) {
    case MessageType::HitCheckpoint:
    case MessageType::HitBreakpoint:
      MOZ_RELEASE_ASSERT(!mPausedMessage);
      mPausedMessage = aMsg.Clone();
      MOZ_FALLTHROUGH;
    case MessageType::DebuggerResponse:
    case MessageType::RecordingFlushed:
      MOZ_RELEASE_ASSERT(mPausedMessage);
      mPaused = true;
      break;
    default:
      break;
  }

  if (aMsg.mType == MessageType::HitCheckpoint) {
    const HitCheckpointMessage& nmsg =
        static_cast<const HitCheckpointMessage&>(aMsg);
    mLastCheckpoint = nmsg.mCheckpointId;

    // All messages sent since the last checkpoint are now obsolete, except
    // those which establish the set of installed breakpoints.
    InfallibleVector<Message*> newMessages;
    for (Message* msg : mMessages) {
      if (msg->mType == MessageType::AddBreakpoint) {
        newMessages.append(msg);
      } else {
        if (msg->mType == MessageType::ClearBreakpoints) {
          for (Message* existing : newMessages) {
            free(existing);
          }
          newMessages.clear();
        }
        free(msg);
      }
    }
    mMessages = std::move(newMessages);
  }

  // The primordial HitCheckpoint messages is not forwarded to the role, as it
  // has not been initialized yet.
  if (aMsg.mType != MessageType::HitCheckpoint || mLastCheckpoint) {
    mRole->OnIncomingMessage(aMsg);
  }
}

void ChildProcessInfo::SendMessage(const Message& aMsg) {
  MOZ_RELEASE_ASSERT(!IsRecovering());
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  // Update paused state.
  MOZ_RELEASE_ASSERT(IsPaused() || aMsg.CanBeSentWhileUnpaused());
  switch (aMsg.mType) {
    case MessageType::Resume:
    case MessageType::RestoreCheckpoint:
    case MessageType::RunToPoint:
      free(mPausedMessage);
      mPausedMessage = nullptr;
      MOZ_FALLTHROUGH;
    case MessageType::DebuggerRequest:
    case MessageType::FlushRecording:
      mPaused = false;
      break;
    default:
      break;
  }

  // Keep track of messages which affect the child's behavior.
  switch (aMsg.mType) {
    case MessageType::Resume:
    case MessageType::RestoreCheckpoint:
    case MessageType::RunToPoint:
    case MessageType::DebuggerRequest:
    case MessageType::AddBreakpoint:
    case MessageType::ClearBreakpoints:
      mMessages.emplaceBack(aMsg.Clone());
      break;
    default:
      break;
  }

  // Keep track of the checkpoints the process will save.
  if (aMsg.mType == MessageType::SetSaveCheckpoint) {
    const SetSaveCheckpointMessage& nmsg =
        static_cast<const SetSaveCheckpointMessage&>(aMsg);
    MOZ_RELEASE_ASSERT(nmsg.mCheckpoint > MostRecentCheckpoint());
    VectorAddOrRemoveEntry(mShouldSaveCheckpoints, nmsg.mCheckpoint,
                           nmsg.mSave);
  }

  SendMessageRaw(aMsg);
}

void ChildProcessInfo::SendMessageRaw(const Message& aMsg) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  mLastMessageTime = TimeStamp::Now();
  mChannel->SendMessage(aMsg);
}

void ChildProcessInfo::Recover(bool aPaused, Message* aPausedMessage,
                               size_t aLastCheckpoint, Message** aMessages,
                               size_t aNumMessages) {
  MOZ_RELEASE_ASSERT(IsPaused());

  SendMessageRaw(SetIsActiveMessage(false));

  size_t mostRecentCheckpoint = MostRecentCheckpoint();
  bool pausedAtCheckpoint = IsPausedAtCheckpoint();

  // Clear out all messages that have been sent to this process.
  for (Message* msg : mMessages) {
    free(msg);
  }
  mMessages.clear();
  SendMessageRaw(ClearBreakpointsMessage());

  mPaused = aPaused;
  mPausedMessage = aPausedMessage;
  mLastCheckpoint = aLastCheckpoint;
  for (size_t i = 0; i < aNumMessages; i++) {
    mMessages.append(aMessages[i]->Clone());
  }

  mNumRecoveredMessages = 0;

  if (mostRecentCheckpoint < mLastCheckpoint) {
    mRecoveryStage = RecoveryStage::ReachingCheckpoint;
    SendMessageRaw(ResumeMessage(/* aForward = */ true));
  } else if (mostRecentCheckpoint > mLastCheckpoint || !pausedAtCheckpoint) {
    mRecoveryStage = RecoveryStage::ReachingCheckpoint;
    // Rewind to the last saved checkpoint at or prior to the target.
    size_t targetCheckpoint = CheckpointId::Invalid;
    for (size_t saved : mShouldSaveCheckpoints) {
      if (saved <= mLastCheckpoint && saved > targetCheckpoint) {
        targetCheckpoint = saved;
      }
    }
    MOZ_RELEASE_ASSERT(targetCheckpoint != CheckpointId::Invalid);
    SendMessageRaw(RestoreCheckpointMessage(targetCheckpoint));
  } else {
    mRecoveryStage = RecoveryStage::PlayingMessages;
    SendNextRecoveryMessage();
  }

  WaitUntil([=]() { return !IsRecovering(); });
}

void ChildProcessInfo::Recover(ChildProcessInfo* aTargetProcess) {
  MOZ_RELEASE_ASSERT(aTargetProcess->IsPaused());
  Recover(true, aTargetProcess->mPausedMessage->Clone(),
          aTargetProcess->mLastCheckpoint, aTargetProcess->mMessages.begin(),
          aTargetProcess->mMessages.length());
}

void ChildProcessInfo::RecoverToCheckpoint(size_t aCheckpoint) {
  HitCheckpointMessage pausedMessage(aCheckpoint,
                                     /* aRecordingEndpoint = */ false,
                                     /* aDuration = */ 0);
  Recover(true, pausedMessage.Clone(), aCheckpoint, nullptr, 0);
}

void ChildProcessInfo::OnIncomingRecoveryMessage(const Message& aMsg) {
  switch (aMsg.mType) {
    case MessageType::HitCheckpoint: {
      MOZ_RELEASE_ASSERT(mRecoveryStage == RecoveryStage::ReachingCheckpoint);
      const HitCheckpointMessage& nmsg =
          static_cast<const HitCheckpointMessage&>(aMsg);
      if (nmsg.mCheckpointId < mLastCheckpoint) {
        SendMessageRaw(ResumeMessage(/* aForward = */ true));
      } else {
        MOZ_RELEASE_ASSERT(nmsg.mCheckpointId == mLastCheckpoint);
        mRecoveryStage = RecoveryStage::PlayingMessages;
        SendNextRecoveryMessage();
      }
      break;
    }
    case MessageType::HitBreakpoint:
    case MessageType::DebuggerResponse:
      SendNextRecoveryMessage();
      break;
    case MessageType::MiddlemanCallRequest: {
      // Middleman call messages can arrive in different orders when recovering
      // than they originally did in the original process, so handle them afresh
      // even when recovering.
      MiddlemanCallResponseMessage* response =
          ProcessMiddlemanCallMessage((MiddlemanCallRequestMessage&)aMsg);
      SendMessageRaw(*response);
      free(response);
      break;
    }
    case MessageType::ResetMiddlemanCalls:
      ResetMiddlemanCalls();
      break;
    default:
      MOZ_CRASH("Unexpected message during recovery");
  }
}

void ChildProcessInfo::SendNextRecoveryMessage() {
  MOZ_RELEASE_ASSERT(mRecoveryStage == RecoveryStage::PlayingMessages);

  // Keep sending messages to the child as long as it stays paused.
  Message* msg;
  do {
    // Check if we have recovered to the desired paused state.
    if (mNumRecoveredMessages == mMessages.length()) {
      MOZ_RELEASE_ASSERT(IsPaused());
      mRecoveryStage = RecoveryStage::None;
      return;
    }
    msg = mMessages[mNumRecoveredMessages++];
    SendMessageRaw(*msg);

    // Messages operating on breakpoints preserve the paused state of the
    // child, so keep sending more messages.
  } while (msg->mType == MessageType::AddBreakpoint ||
           msg->mType == MessageType::ClearBreakpoints);

  // If we have sent all messages and are in an unpaused state, we are done
  // recovering.
  if (mNumRecoveredMessages == mMessages.length() && !IsPaused()) {
    mRecoveryStage = RecoveryStage::None;
  }
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
    const Maybe<RecordingProcessData>& aRecordingProcessData) {
  size_t channelId = gNumChannels++;

  // Create a new channel every time we launch a new subprocess, without
  // deleting or tearing down the old one's state. This is pretty lame and it
  // would be nice if we could do something better here, especially because
  // with restarts we could create any number of channels over time.
  mChannel = new Channel(channelId, IsRecording(), [=](Message* aMsg) {
    ReceiveChildMessageOnMainThread(channelId, aMsg);
  });

  MOZ_RELEASE_ASSERT(IsRecording() == aRecordingProcessData.isSome());
  if (IsRecording()) {
    std::vector<std::string> extraArgs;
    GetArgumentsForChildProcess(base::GetCurrentProcId(), channelId,
                                gRecordingFilename, /* aRecording = */ true,
                                extraArgs);

    MOZ_RELEASE_ASSERT(!gRecordingProcess);
    gRecordingProcess =
        new ipc::GeckoChildProcessHost(GeckoProcessType_Content);

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
  } else {
    dom::ContentChild::GetSingleton()->SendCreateReplayingProcess(channelId);
  }

  mLastMessageTime = TimeStamp::Now();

  SendGraphicsMemoryToChild();

  // The child should send us a HitCheckpoint with an invalid ID to pause.
  WaitUntilPaused();

  MOZ_RELEASE_ASSERT(gIntroductionMessage);
  SendMessage(*gIntroductionMessage);
}

void ChildProcessInfo::OnCrash(const char* aWhy) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  // If a child process crashes or hangs then annotate the crash report.
  CrashReporter::AnnotateCrashReport(
      CrashReporter::Annotation::RecordReplayError, nsAutoCString(aWhy));

  // If we received a FatalError message then the child generated a minidump.
  // Shut down cleanly so that we don't mask the report with our own crash.
  if (mHasFatalError) {
    Shutdown();
  }

  // Indicate when we crash if the child tried to send us a fatal error message
  // but had a problem either unprotecting system memory or generating the
  // minidump.
  MOZ_RELEASE_ASSERT(!mHasBegunFatalError);

  // The child crashed without producing a minidump, produce one ourselves.
  MOZ_CRASH("Unexpected child crash");
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
  ChildProcessInfo* mProcess;
  size_t mChannelId;
  Message* mMsg;
};
static StaticInfallibleVector<PendingMessage> gPendingMessages;

// Whether there is a pending task on the main thread's message loop to handle
// all pending messages.
static bool gHasPendingMessageRunnable;

// Process a pending message from aProcess (or any process if aProcess is null)
// and return whether such a message was found. This must be called on the main
// thread with gMonitor held.
/* static */ bool ChildProcessInfo::MaybeProcessPendingMessage(
    ChildProcessInfo* aProcess) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  for (size_t i = 0; i < gPendingMessages.length(); i++) {
    if (!aProcess || gPendingMessages[i].mProcess == aProcess) {
      PendingMessage copy = gPendingMessages[i];
      gPendingMessages.erase(&gPendingMessages[i]);

      MonitorAutoUnlock unlock(*gMonitor);
      copy.mProcess->OnIncomingMessage(copy.mChannelId, *copy.mMsg);
      free(copy.mMsg);
      return true;
    }
  }

  return false;
}

// How many seconds to wait without hearing from an unpaused child before
// considering that child to be hung.
static const size_t HangSeconds = 30;

void ChildProcessInfo::WaitUntil(const std::function<bool()>& aCallback) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  bool sentTerminateMessage = false;
  while (!aCallback()) {
    MonitorAutoLock lock(*gMonitor);
    if (!MaybeProcessPendingMessage(this)) {
      if (gChildrenAreDebugging || IsRecording()) {
        // Don't watch for hangs when children are being debugged. Recording
        // children are never treated as hanged both because they cannot be
        // restarted and because they may just be idling.
        gMonitor->Wait();
      } else {
        TimeStamp deadline =
            mLastMessageTime + TimeDuration::FromSeconds(HangSeconds);
        if (TimeStamp::Now() >= deadline) {
          MonitorAutoUnlock unlock(*gMonitor);
          if (!sentTerminateMessage) {
            // Try to get the child to crash, so that we can get a minidump.
            // Sending the message will reset mLastMessageTime so we get to
            // wait another HangSeconds before hitting the restart case below.
            // Use SendMessageRaw to avoid problems if we are recovering.
            CrashReporter::AnnotateCrashReport(
                CrashReporter::Annotation::RecordReplayHang, true);
            SendMessageRaw(TerminateMessage());
            sentTerminateMessage = true;
          } else {
            // The child is still non-responsive after sending the terminate
            // message.
            OnCrash("Child process non-responsive");
          }
        }
        gMonitor->WaitUntil(deadline);
      }
    }
  }
}

// Runnable created on the main thread to handle any tasks sent by the replay
// message loop thread which were not handled while the main thread was blocked.
/* static */ void ChildProcessInfo::MaybeProcessPendingMessageRunnable() {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  MonitorAutoLock lock(*gMonitor);
  MOZ_RELEASE_ASSERT(gHasPendingMessageRunnable);
  gHasPendingMessageRunnable = false;
  while (MaybeProcessPendingMessage(nullptr)) {
  }
}

// Execute a task that processes a message received from the child. This is
// called on a channel thread, and the function executes asynchronously on
// the main thread.
void ChildProcessInfo::ReceiveChildMessageOnMainThread(size_t aChannelId,
                                                       Message* aMsg) {
  MOZ_RELEASE_ASSERT(!NS_IsMainThread());

  MonitorAutoLock lock(*gMonitor);

  PendingMessage pending;
  pending.mProcess = this;
  pending.mChannelId = aChannelId;
  pending.mMsg = aMsg;
  gPendingMessages.append(pending);

  // Notify the main thread, if it is waiting in WaitUntil.
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
