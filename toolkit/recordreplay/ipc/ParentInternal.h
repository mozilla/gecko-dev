/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_recordreplay_ParentInternal_h
#define mozilla_recordreplay_ParentInternal_h

#include "ParentIPC.h"

#include "Channel.h"
#include "mozilla/ipc/GeckoChildProcessHost.h"

namespace mozilla {
namespace recordreplay {
namespace parent {

// This file has internal declarations for interaction between different
// components of middleman logic.

class ChildProcessInfo;

// Get the message loop for the main thread.
MessageLoop* MainThreadMessageLoop();

// Called after prefs are available to this process.
void PreferencesLoaded();

// Return whether replaying processes are allowed to save checkpoints and
// rewind. Can only be called after PreferencesLoaded().
bool CanRewind();

// Whether the child currently being interacted with is recording.
bool ActiveChildIsRecording();

// Get the active recording child process.
ChildProcessInfo* ActiveRecordingChild();

// Return whether the middleman's main thread is blocked waiting on a
// synchronous IPDL reply from the recording child.
bool MainThreadIsWaitingForIPDLReply();

// If necessary, resume execution in the child before the main thread begins
// to block while waiting on an IPDL reply from the child.
void ResumeBeforeWaitingForIPDLReply();

// Initialize state which handles incoming IPDL messages from the UI and
// recording child processes.
void InitializeForwarding();

// Terminate all children and kill this process.
void Shutdown();

// Monitor used for synchronizing between the main and channel or message loop
// threads.
static Monitor* gMonitor;

// Allow the child process to resume execution.
void Resume(bool aForward);

// Direct the child process to warp to a specific point.
void TimeWarp(const js::ExecutionPoint& target);

// Send a JSON request to the child process, and synchronously wait for a
// response.
void SendRequest(const js::CharBuffer& aBuffer, js::CharBuffer* aResponse);

// Set the breakpoints installed in the child process.
void AddBreakpoint(const js::BreakpointPosition& aPosition);
void ClearBreakpoints();

// If possible, make sure the active child is replaying, and that requests
// which might trigger an unhandled divergence can be processed (recording
// children cannot process such requests).
void MaybeSwitchToReplayingChild();

// Block until the active child has paused somewhere.
void WaitUntilActiveChildIsPaused();

// Notify the parent that the debugger has paused and will allow the user to
// interact with it and potentially start rewinding.
void MarkActiveChildExplicitPause();

///////////////////////////////////////////////////////////////////////////////
// Graphics
///////////////////////////////////////////////////////////////////////////////

extern void* gGraphicsMemory;

void InitializeGraphicsMemory();
void SendGraphicsMemoryToChild();

// Update the graphics painted in the UI process, per painting data received
// from a child process, or null if a repaint was triggered and failed due to
// an unhandled recording divergence.
void UpdateGraphicsInUIProcess(const PaintMessage* aMsg);

// If necessary, update graphics after the active child sends a paint message
// or reaches a checkpoint.
void MaybeUpdateGraphicsAtPaint(const PaintMessage& aMsg);
void MaybeUpdateGraphicsAtCheckpoint(size_t aCheckpointId);

// ID for the mach message sent from a child process to the middleman to
// request a port for the graphics shmem.
static const int32_t GraphicsHandshakeMessageId = 42;

// ID for the mach message sent from the middleman to a child process with the
// requested memory for.
static const int32_t GraphicsMemoryMessageId = 43;

// Fixed size of the graphics shared memory buffer.
static const size_t GraphicsMemorySize = 4096 * 4096 * 4;

// Return whether the environment variable activating repaint stress mode is
// set. This makes various changes in both the middleman and child processes to
// trigger a child to diverge from the recording and repaint on every vsync,
// making sure that repainting can handle all the system interactions that
// occur while painting the current tab.
bool InRepaintStressMode();

///////////////////////////////////////////////////////////////////////////////
// Child Processes
///////////////////////////////////////////////////////////////////////////////

// Information about the role which a child process is fulfilling, and governs
// how the process responds to incoming messages.
class ChildRole {
 public:
  // See ParentIPC.cpp for the meaning of these role types.
#define ForEachRoleType(Macro) Macro(Active) Macro(Standby) Macro(Inert)

  enum Type {
#define DefineType(Name) Name,
    ForEachRoleType(DefineType)
#undef DefineType
  };

  static const char* TypeString(Type aType) {
    switch (aType) {
#define GetTypeString(Name) \
  case Name:                \
    return #Name;
      ForEachRoleType(GetTypeString)
#undef GetTypeString
          default : MOZ_CRASH("Bad ChildRole type");
    }
  }

 protected:
  ChildProcessInfo* mProcess;
  Type mType;

  explicit ChildRole(Type aType) : mProcess(nullptr), mType(aType) {}

 public:
  void SetProcess(ChildProcessInfo* aProcess) {
    MOZ_RELEASE_ASSERT(!mProcess);
    mProcess = aProcess;
  }
  Type GetType() const { return mType; }

  virtual ~ChildRole() {}

  // The methods below are all called on the main thread.

  virtual void Initialize() {}

  // When the child is paused and potentially sitting idle, notify the role
  // that state affecting its behavior has changed and may want to become
  // active again.
  virtual void Poke() {}

  virtual void OnIncomingMessage(const Message& aMsg) = 0;
};

// Handle to the underlying recording process, if there is one. Recording
// processes are directly spawned by the middleman at startup, since they need
// to receive all the same IPC which the middleman receives from the UI process
// in order to initialize themselves. Replaying processes are all spawned by
// the UI process itself, due to sandboxing restrictions.
extern ipc::GeckoChildProcessHost* gRecordingProcess;

// Any information needed to spawn a recording child process, in addition to
// the contents of the introduction message.
struct RecordingProcessData {
  // File descriptors that will need to be remapped for the child process.
  const base::SharedMemoryHandle& mPrefsHandle;
  const ipc::FileDescriptor& mPrefMapHandle;

  RecordingProcessData(const base::SharedMemoryHandle& aPrefsHandle,
                       const ipc::FileDescriptor& aPrefMapHandle)
      : mPrefsHandle(aPrefsHandle), mPrefMapHandle(aPrefMapHandle) {}
};

// Information about a recording or replaying child process.
class ChildProcessInfo {
  // Channel for communicating with the process.
  Channel* mChannel;

  // The last time we sent or received a message from this process.
  TimeStamp mLastMessageTime;

  // Whether this process is recording.
  bool mRecording;

  // The current recovery stage of this process.
  //
  // Recovery is used when we are shepherding a child to a particular state:
  // a particular execution position and sets of installed breakpoints and
  // saved checkpoints. Recovery is used when changing a child's role, and when
  // spawning a new process to replace a crashed child process.
  //
  // When recovering, the child process won't yet be in the exact place
  // reflected by the state below, but the main thread will wait until it has
  // finished reaching this state before it is able to send or receive
  // messages.
  enum class RecoveryStage {
    // No recovery is being performed, and the process can be interacted with.
    None,

    // The process has not yet reached mLastCheckpoint.
    ReachingCheckpoint,

    // The process has reached mLastCheckpoint, and additional messages are
    // being sent to change its intra-checkpoint execution position or install
    // breakpoints.
    PlayingMessages
  };
  RecoveryStage mRecoveryStage;

  // Whether the process is currently paused.
  bool mPaused;

  // If the process is paused, or if it is running while handling a message
  // that won't cause it to change its execution point, the last message which
  // caused it to pause.
  Message* mPausedMessage;

  // The last checkpoint which the child process reached. The child is
  // somewhere between this and either the next or previous checkpoint,
  // depending on the messages that have been sent to it.
  size_t mLastCheckpoint;

  // Messages sent to the process which will affect its behavior as it runs
  // forward or backward from mLastCheckpoint. This includes all messages that
  // will need to be sent to another process to recover it to the same state as
  // this process.
  InfallibleVector<Message*> mMessages;

  // In the PlayingMessages recovery stage, how much of mMessages has been sent
  // to the process.
  size_t mNumRecoveredMessages;

  // Current role of this process.
  UniquePtr<ChildRole> mRole;

  // Unsorted list of the checkpoints the process has been instructed to save.
  // Those at or before the most recent checkpoint will have been saved.
  InfallibleVector<size_t> mShouldSaveCheckpoints;

  // Sorted major checkpoints for this process. See ParentIPC.cpp.
  InfallibleVector<size_t> mMajorCheckpoints;

  // Whether we need this child to pause while the recording is updated.
  bool mPauseNeeded;

  // Flags for whether we have received messages from the child indicating it
  // is crashing.
  bool mHasBegunFatalError;
  bool mHasFatalError;

  void OnIncomingMessage(size_t aChannelId, const Message& aMsg);
  void OnIncomingRecoveryMessage(const Message& aMsg);
  void SendNextRecoveryMessage();
  void SendMessageRaw(const Message& aMsg);

  static void MaybeProcessPendingMessageRunnable();
  void ReceiveChildMessageOnMainThread(size_t aChannelId, Message* aMsg);

  // Get the position of this process relative to its last checkpoint.
  enum Disposition {
    AtLastCheckpoint,
    BeforeLastCheckpoint,
    AfterLastCheckpoint
  };
  Disposition GetDisposition();

  void Recover(bool aPaused, Message* aPausedMessage, size_t aLastCheckpoint,
               Message** aMessages, size_t aNumMessages);

  void OnCrash(const char* aWhy);
  void LaunchSubprocess(
      const Maybe<RecordingProcessData>& aRecordingProcessData);

 public:
  ChildProcessInfo(UniquePtr<ChildRole> aRole,
                   const Maybe<RecordingProcessData>& aRecordingProcessData);
  ~ChildProcessInfo();

  ChildRole* Role() { return mRole.get(); }
  size_t GetId() { return mChannel->GetId(); }
  bool IsRecording() { return mRecording; }
  size_t LastCheckpoint() { return mLastCheckpoint; }
  bool IsRecovering() { return mRecoveryStage != RecoveryStage::None; }
  bool PauseNeeded() { return mPauseNeeded; }
  const InfallibleVector<size_t>& MajorCheckpoints() {
    return mMajorCheckpoints;
  }

  bool IsPaused() { return mPaused; }
  bool IsPausedAtCheckpoint();
  bool IsPausedAtRecordingEndpoint();

  // Get all breakpoints currently installed for this process.
  void GetInstalledBreakpoints(
      InfallibleVector<AddBreakpointMessage*>& aBreakpoints);

  typedef std::function<bool(js::BreakpointPosition::Kind)> BreakpointFilter;

  // Get the checkpoint at or earlier to the process' position. This is either
  // the last reached checkpoint or the previous one.
  size_t MostRecentCheckpoint() {
    return (GetDisposition() == BeforeLastCheckpoint) ? mLastCheckpoint - 1
                                                      : mLastCheckpoint;
  }

  // Get the checkpoint which needs to be saved in order for this process
  // (or another at the same place) to rewind.
  size_t RewindTargetCheckpoint() {
    switch (GetDisposition()) {
      case BeforeLastCheckpoint:
      case AtLastCheckpoint:
        // This will return CheckpointId::Invalid if we are the beginning of the
        // recording.
        return LastCheckpoint() - 1;
      case AfterLastCheckpoint:
        return LastCheckpoint();
    }
  }

  bool ShouldSaveCheckpoint(size_t aId) {
    return VectorContains(mShouldSaveCheckpoints, aId);
  }

  bool IsMajorCheckpoint(size_t aId) {
    return VectorContains(mMajorCheckpoints, aId);
  }

  bool HasSavedCheckpoint(size_t aId) {
    return (aId <= MostRecentCheckpoint()) && ShouldSaveCheckpoint(aId);
  }

  size_t MostRecentSavedCheckpoint() {
    size_t id = MostRecentCheckpoint();
    while (!ShouldSaveCheckpoint(id)) {
      id--;
    }
    return id;
  }

  void SetPauseNeeded() { mPauseNeeded = true; }

  void ClearPauseNeeded() {
    MOZ_RELEASE_ASSERT(IsPaused());
    mPauseNeeded = false;
    mRole->Poke();
  }

  void AddMajorCheckpoint(size_t aId);
  void SetRole(UniquePtr<ChildRole> aRole);
  void SendMessage(const Message& aMessage);

  // Recover to the same state as another process.
  void Recover(ChildProcessInfo* aTargetProcess);

  // Recover to be paused at a checkpoint with no breakpoints set.
  void RecoverToCheckpoint(size_t aCheckpoint);

  // Handle incoming messages from this process (and no others) until the
  // callback succeeds.
  void WaitUntil(const std::function<bool()>& aCallback);

  void WaitUntilPaused() {
    WaitUntil([=]() { return IsPaused(); });
  }

  static bool MaybeProcessPendingMessage(ChildProcessInfo* aProcess);

  static void SetIntroductionMessage(IntroductionMessage* aMessage);
};

}  // namespace parent
}  // namespace recordreplay
}  // namespace mozilla

#endif  // mozilla_recordreplay_ParentInternal_h
