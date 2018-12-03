/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ChildInternal.h"

#include "ProcessRecordReplay.h"

namespace mozilla {
namespace recordreplay {
namespace navigation {

typedef js::BreakpointPosition BreakpointPosition;
typedef js::ExecutionPoint ExecutionPoint;

static void BreakpointPositionToString(const BreakpointPosition& aPos,
                                       nsAutoCString& aStr) {
  aStr.AppendPrintf("{ Kind: %s, Script: %d, Offset: %d, Frame: %d }",
                    aPos.KindString(), (int)aPos.mScript, (int)aPos.mOffset,
                    (int)aPos.mFrameIndex);
}

static void ExecutionPointToString(const ExecutionPoint& aPoint,
                                   nsAutoCString& aStr) {
  aStr.AppendPrintf("{ Checkpoint %d", (int)aPoint.mCheckpoint);
  if (aPoint.HasPosition()) {
    aStr.AppendPrintf(" Progress %llu Position ", aPoint.mProgress);
    BreakpointPositionToString(aPoint.mPosition, aStr);
  }
  aStr.AppendPrintf(" }");
}

///////////////////////////////////////////////////////////////////////////////
// Navigation State
///////////////////////////////////////////////////////////////////////////////

// The navigation state of a recording/replaying process describes where the
// process currently is and what it is doing in order to respond to messages
// from the middleman process.
//
// At all times, the navigation state will be in exactly one of the following
// phases:
//
// - Paused: The process is paused somewhere.
// - Forward: The process is running forward and scanning for breakpoint hits.
// - ReachBreakpoint: The process is running forward from a checkpoint to a
//     particular execution point before the next checkpoint.
// - FindLastHit: The process is running forward and keeping track of the last
//     point a breakpoint was hit within an execution region.
//
// This file manages data associated with each of these phases and the
// transitions that occur between them as the process executes or new
// messages are received from the middleman.

typedef AllocPolicy<MemoryKind::Navigation> UntrackedAllocPolicy;

// Abstract class for where we are at in the navigation state machine.
// Each subclass has a single instance contained in NavigationState (see below)
// and it and all its data are allocated using untracked memory that is not
// affected by restoring earlier checkpoints.
class NavigationPhase {
  // All virtual members should only be accessed through NavigationState.
  friend class NavigationState;

 private:
  MOZ_NORETURN void Unsupported(const char* aOperation) {
    nsAutoCString str;
    ToString(str);

    Print("Operation %s not supported: %s\n", aOperation, str.get());
    MOZ_CRASH("Unsupported navigation operation");
  }

 public:
  virtual void ToString(nsAutoCString& aStr) = 0;

  // The process has just reached or rewound to a checkpoint.
  virtual void AfterCheckpoint(const CheckpointId& aCheckpoint) {
    Unsupported("AfterCheckpoint");
  }

  // Called when some position with an installed handler has been reached.
  virtual void PositionHit(const ExecutionPoint& aPoint) {
    Unsupported("PositionHit");
  }

  // Called after receiving a resume command from the middleman.
  virtual void Resume(bool aForward) { Unsupported("Resume"); }

  // Called after the middleman tells us to rewind to a specific checkpoint.
  virtual void RestoreCheckpoint(size_t aCheckpoint) {
    Unsupported("RestoreCheckpoint");
  }

  // Called after the middleman tells us to run forward to a specific point.
  virtual void RunToPoint(const ExecutionPoint& aTarget) {
    Unsupported("RunToPoint");
  }

  // Process an incoming debugger request from the middleman.
  virtual void HandleDebuggerRequest(js::CharBuffer* aRequestBuffer) {
    Unsupported("HandleDebuggerRequest");
  }

  // Called when a debugger request wants to try an operation that may
  // trigger an unhandled divergence from the recording.
  virtual bool MaybeDivergeFromRecording() {
    Unsupported("MaybeDivergeFromRecording");
  }

  // Get the current execution point.
  virtual ExecutionPoint CurrentExecutionPoint() {
    Unsupported("CurrentExecutionPoint");
  }

  // Called when execution reaches the endpoint of the recording.
  virtual void HitRecordingEndpoint(const ExecutionPoint& aPoint) {
    Unsupported("HitRecordingEndpoint");
  }
};

// Information about a debugger request sent by the middleman.
struct RequestInfo {
  // JSON contents for the request and response.
  InfallibleVector<char16_t, 0, UntrackedAllocPolicy> mRequestBuffer;
  InfallibleVector<char16_t, 0, UntrackedAllocPolicy> mResponseBuffer;

  // Whether processing this request triggered an unhandled divergence.
  bool mUnhandledDivergence;

  RequestInfo() : mUnhandledDivergence(false) {}

  RequestInfo(const RequestInfo& o)
      : mUnhandledDivergence(o.mUnhandledDivergence) {
    mRequestBuffer.append(o.mRequestBuffer.begin(), o.mRequestBuffer.length());
    mResponseBuffer.append(o.mResponseBuffer.begin(),
                           o.mResponseBuffer.length());
  }
};
typedef InfallibleVector<RequestInfo, 4, UntrackedAllocPolicy>
    UntrackedRequestVector;

// Phase when the replaying process is paused.
class PausedPhase final : public NavigationPhase {
  // Location of the pause.
  ExecutionPoint mPoint;

  // Whether we are paused at the end of the recording.
  bool mRecordingEndpoint;

  // All debugger requests we have seen while paused here.
  UntrackedRequestVector mRequests;

  // Index of the request currently being processed. Normally this is the
  // last entry in |mRequests|, though may be earlier if we are recovering
  // from an unhandled divergence.
  size_t mRequestIndex;

  // Whether we have saved a temporary checkpoint.
  bool mSavedTemporaryCheckpoint;

  // Whether we had to restore a checkpoint to deal with an unhandled
  // recording divergence, and haven't finished rehandling old requests.
  bool mRecoveringFromDivergence;

  // Set when we were told to resume forward and need to clean up our state.
  bool mResumeForward;

 public:
  void Enter(const ExecutionPoint& aPoint, bool aRewind = false,
             bool aRecordingEndpoint = false);

  void ToString(nsAutoCString& aStr) override {
    aStr.AppendPrintf("Paused RecoveringFromDivergence %d",
                      mRecoveringFromDivergence);
  }

  void AfterCheckpoint(const CheckpointId& aCheckpoint) override;
  void PositionHit(const ExecutionPoint& aPoint) override;
  void Resume(bool aForward) override;
  void RestoreCheckpoint(size_t aCheckpoint) override;
  void RunToPoint(const ExecutionPoint& aTarget) override;
  void HandleDebuggerRequest(js::CharBuffer* aRequestBuffer) override;
  bool MaybeDivergeFromRecording() override;
  ExecutionPoint CurrentExecutionPoint() override;

  bool EnsureTemporaryCheckpoint();
};

// Phase when execution is proceeding forwards in search of breakpoint hits.
class ForwardPhase final : public NavigationPhase {
  // Some execution point in the recent past. There are no checkpoints or
  // breakpoint hits between this point and the current point of execution.
  ExecutionPoint mPoint;

 public:
  void Enter(const ExecutionPoint& aPoint);

  void ToString(nsAutoCString& aStr) override { aStr.AppendPrintf("Forward"); }

  void AfterCheckpoint(const CheckpointId& aCheckpoint) override;
  void PositionHit(const ExecutionPoint& aPoint) override;
  void HitRecordingEndpoint(const ExecutionPoint& aPoint) override;
};

// Phase when the replaying process is running forward from a checkpoint to a
// breakpoint at a particular execution point.
class ReachBreakpointPhase final : public NavigationPhase {
 private:
  // Where to start running from.
  CheckpointId mStart;

  // The point we are running to.
  ExecutionPoint mPoint;

  // Point at which to decide whether to save a temporary checkpoint.
  Maybe<ExecutionPoint> mTemporaryCheckpoint;

  // Whether we have saved a temporary checkpoint at the specified point.
  bool mSavedTemporaryCheckpoint;

  // The time at which we started running forward from the initial
  // checkpoint, in microseconds.
  double mStartTime;

 public:
  void Enter(const CheckpointId& aStart, bool aRewind,
             const ExecutionPoint& aPoint,
             const Maybe<ExecutionPoint>& aTemporaryCheckpoint);

  void ToString(nsAutoCString& aStr) override {
    aStr.AppendPrintf("ReachBreakpoint: ");
    ExecutionPointToString(mPoint, aStr);
    if (mTemporaryCheckpoint.isSome()) {
      aStr.AppendPrintf(" TemporaryCheckpoint: ");
      ExecutionPointToString(mTemporaryCheckpoint.ref(), aStr);
    }
  }

  void AfterCheckpoint(const CheckpointId& aCheckpoint) override;
  void PositionHit(const ExecutionPoint& aPoint) override;
};

// Phase when the replaying process is searching forward from a checkpoint to
// find the last point a breakpoint is hit before reaching an execution point.
class FindLastHitPhase final : public NavigationPhase {
  // Where we started searching from.
  CheckpointId mStart;

  // Endpoint of the search, nothing if the endpoint is the next checkpoint.
  Maybe<ExecutionPoint> mEnd;

  // Whether the endpoint itself is considered to be part of the search space.
  bool mIncludeEnd;

  // Counter that increases as we run forward, for ordering hits.
  size_t mCounter;

  // All positions we are interested in hits for, including all breakpoint
  // positions (and possibly other positions).
  struct TrackedPosition {
    BreakpointPosition mPosition;

    // The last time this was hit so far, or invalid.
    ExecutionPoint mLastHit;

    // The value of the counter when the last hit occurred.
    size_t mLastHitCount;

    explicit TrackedPosition(const BreakpointPosition& aPosition)
        : mPosition(aPosition), mLastHitCount(0) {}
  };
  InfallibleVector<TrackedPosition, 4, UntrackedAllocPolicy> mTrackedPositions;

  const TrackedPosition& FindTrackedPosition(const BreakpointPosition& aPos);
  void CheckForRegionEnd(const ExecutionPoint& aPoint);
  void OnRegionEnd();

 public:
  // Note: this always rewinds.
  void Enter(const CheckpointId& aStart, const Maybe<ExecutionPoint>& aEnd,
             bool aIncludeEnd);

  void ToString(nsAutoCString& aStr) override {
    aStr.AppendPrintf("FindLastHit");
  }

  void AfterCheckpoint(const CheckpointId& aCheckpoint) override;
  void PositionHit(const ExecutionPoint& aPoint) override;
  void HitRecordingEndpoint(const ExecutionPoint& aPoint) override;
};

// Structure which manages state about the breakpoints in existence and about
// how the process is being navigated through. This is allocated in untracked
// memory and its contents will not change when restoring an earlier
// checkpoint.
class NavigationState {
  // When replaying, the last known recording endpoint. There may be other,
  // later endpoints we haven't been informed about.
  ExecutionPoint mRecordingEndpoint;
  size_t mRecordingEndpointIndex;

  // The last checkpoint we ran forward or rewound to.
  CheckpointId mLastCheckpoint;

  // The locations of all temporary checkpoints we have saved. Temporary
  // checkpoints are taken immediately prior to reaching these points.
  InfallibleVector<ExecutionPoint, 0, UntrackedAllocPolicy>
      mTemporaryCheckpoints;

 public:
  // All the currently installed breakpoints.
  InfallibleVector<BreakpointPosition, 4, UntrackedAllocPolicy> mBreakpoints;

  CheckpointId LastCheckpoint() { return mLastCheckpoint; }

  // The current phase of the process.
  NavigationPhase* mPhase;

  void SetPhase(NavigationPhase* phase) {
    mPhase = phase;

    if (SpewEnabled()) {
      nsAutoCString str;
      mPhase->ToString(str);

      PrintSpew("SetNavigationPhase %s\n", str.get());
    }
  }

  PausedPhase mPausedPhase;
  ForwardPhase mForwardPhase;
  ReachBreakpointPhase mReachBreakpointPhase;
  FindLastHitPhase mFindLastHitPhase;

  // For testing, specify that temporary checkpoints should be taken regardless
  // of how much time has elapsed.
  bool mAlwaysSaveTemporaryCheckpoints;

  // Progress counts for all checkpoints that have been encountered.
  InfallibleVector<ProgressCounter, 0, UntrackedAllocPolicy>
      mCheckpointProgress;

  // Note: NavigationState is initially zeroed.
  NavigationState() : mPhase(&mForwardPhase) {
    if (IsReplaying()) {
      // The recording must include everything up to the first
      // checkpoint. After that point we will ask the record/replay
      // system to notify us about any further endpoints.
      mRecordingEndpoint = ExecutionPoint(CheckpointId::First, 0);
    }
    mCheckpointProgress.append(0);
  }

  void AfterCheckpoint(const CheckpointId& aCheckpoint) {
    mLastCheckpoint = aCheckpoint;

    // Forget any temporary checkpoints we just rewound past, or made
    // obsolete by reaching the next normal checkpoint.
    while (mTemporaryCheckpoints.length() > aCheckpoint.mTemporary) {
      mTemporaryCheckpoints.popBack();
    }

    // Update the progress counter for each normal checkpoint.
    if (!aCheckpoint.mTemporary) {
      ProgressCounter progress = *ExecutionProgressCounter();
      if (aCheckpoint.mNormal < mCheckpointProgress.length()) {
        MOZ_RELEASE_ASSERT(progress ==
                           mCheckpointProgress[aCheckpoint.mNormal]);
      } else {
        MOZ_RELEASE_ASSERT(aCheckpoint.mNormal == mCheckpointProgress.length());
        mCheckpointProgress.append(progress);
      }
    }

    mPhase->AfterCheckpoint(aCheckpoint);

    // Make sure we don't run past the end of the recording.
    if (!aCheckpoint.mTemporary) {
      CheckForRecordingEndpoint(CheckpointExecutionPoint(aCheckpoint.mNormal));
    }

    MOZ_RELEASE_ASSERT(IsRecording() ||
                       aCheckpoint.mNormal <= mRecordingEndpoint.mCheckpoint);
    if (aCheckpoint.mNormal == mRecordingEndpoint.mCheckpoint &&
        mRecordingEndpoint.HasPosition()) {
      js::EnsurePositionHandler(mRecordingEndpoint.mPosition);
    }
  }

  void PositionHit(const ExecutionPoint& aPoint) {
    mPhase->PositionHit(aPoint);
    CheckForRecordingEndpoint(aPoint);
  }

  void Resume(bool aForward) { mPhase->Resume(aForward); }

  void RestoreCheckpoint(size_t aCheckpoint) {
    mPhase->RestoreCheckpoint(aCheckpoint);
  }

  void RunToPoint(const ExecutionPoint& aTarget) {
    mPhase->RunToPoint(aTarget);
  }

  void HandleDebuggerRequest(js::CharBuffer* aRequestBuffer) {
    mPhase->HandleDebuggerRequest(aRequestBuffer);
  }

  bool MaybeDivergeFromRecording() {
    return mPhase->MaybeDivergeFromRecording();
  }

  ExecutionPoint CurrentExecutionPoint() {
    return mPhase->CurrentExecutionPoint();
  }

  void SetRecordingEndpoint(size_t aIndex, const ExecutionPoint& aEndpoint) {
    // Ignore endpoints older than the last one we know about.
    if (aIndex <= mRecordingEndpointIndex) {
      return;
    }
    MOZ_RELEASE_ASSERT(mRecordingEndpoint.mCheckpoint <= aEndpoint.mCheckpoint);
    mRecordingEndpointIndex = aIndex;
    mRecordingEndpoint = aEndpoint;
    if (aEndpoint.HasPosition()) {
      js::EnsurePositionHandler(aEndpoint.mPosition);
    }
  }

  void CheckForRecordingEndpoint(const ExecutionPoint& aPoint) {
    while (aPoint == mRecordingEndpoint) {
      // The recording ended after the checkpoint, but maybe there is
      // another, later endpoint now. This may call back into
      // setRecordingEndpoint and notify us there is more recording data
      // available.
      if (!recordreplay::HitRecordingEndpoint()) {
        mPhase->HitRecordingEndpoint(mRecordingEndpoint);
      }
    }
  }

  ExecutionPoint LastRecordingEndpoint() {
    // Get the last recording endpoint in the recording file.
    while (recordreplay::HitRecordingEndpoint()) {
    }
    return mRecordingEndpoint;
  }

  size_t NumTemporaryCheckpoints() { return mTemporaryCheckpoints.length(); }

  bool SaveTemporaryCheckpoint(const ExecutionPoint& aPoint) {
    MOZ_RELEASE_ASSERT(aPoint.mCheckpoint == mLastCheckpoint.mNormal);
    mTemporaryCheckpoints.append(aPoint);
    return NewCheckpoint(/* aTemporary = */ true);
  }

  ExecutionPoint LastTemporaryCheckpointLocation() {
    MOZ_RELEASE_ASSERT(!mTemporaryCheckpoints.empty());
    return mTemporaryCheckpoints.back();
  }

  ExecutionPoint CheckpointExecutionPoint(size_t aCheckpoint) {
    MOZ_RELEASE_ASSERT(aCheckpoint < mCheckpointProgress.length());
    return ExecutionPoint(aCheckpoint, mCheckpointProgress[aCheckpoint]);
  }
};

static NavigationState* gNavigation;

///////////////////////////////////////////////////////////////////////////////
// Paused Phase
///////////////////////////////////////////////////////////////////////////////

static bool ThisProcessCanRewind() { return HasSavedCheckpoint(); }

void PausedPhase::Enter(const ExecutionPoint& aPoint, bool aRewind,
                        bool aRecordingEndpoint) {
  mPoint = aPoint;
  mRecordingEndpoint = aRecordingEndpoint;
  mRequests.clear();
  mRequestIndex = 0;
  mSavedTemporaryCheckpoint = false;
  mRecoveringFromDivergence = false;
  mResumeForward = false;

  gNavigation->SetPhase(this);

  if (aRewind) {
    MOZ_RELEASE_ASSERT(!aPoint.HasPosition());
    RestoreCheckpointAndResume(CheckpointId(aPoint.mCheckpoint));
    Unreachable();
  }

  if (aPoint.HasPosition()) {
    child::HitBreakpoint(aRecordingEndpoint);
  } else {
    child::HitCheckpoint(aPoint.mCheckpoint, aRecordingEndpoint);
  }
}

void PausedPhase::AfterCheckpoint(const CheckpointId& aCheckpoint) {
  MOZ_RELEASE_ASSERT(!mRecoveringFromDivergence);
  if (!aCheckpoint.mTemporary) {
    // We just rewound here, and are now where we should pause.
    MOZ_RELEASE_ASSERT(
        mPoint == gNavigation->CheckpointExecutionPoint(aCheckpoint.mNormal));
    child::HitCheckpoint(mPoint.mCheckpoint, mRecordingEndpoint);
  } else {
    // We just saved or restored the temporary checkpoint taken while
    // processing debugger requests here.
    MOZ_RELEASE_ASSERT(ThisProcessCanRewind());
    MOZ_RELEASE_ASSERT(mSavedTemporaryCheckpoint);
  }
}

void PausedPhase::PositionHit(const ExecutionPoint& aPoint) {
  // Ignore positions hit while paused (we're probably doing an eval).
}

void PausedPhase::Resume(bool aForward) {
  MOZ_RELEASE_ASSERT(!mRecoveringFromDivergence);
  MOZ_RELEASE_ASSERT(!mResumeForward);

  if (aForward) {
    // If we have saved any temporary checkpoint, we performed an operation
    // that may have side effects. Clear these unwanted changes by restoring
    // the temporary checkpoint we saved earlier.
    if (mSavedTemporaryCheckpoint) {
      mResumeForward = true;
      RestoreCheckpointAndResume(gNavigation->LastCheckpoint());
      Unreachable();
    }

    js::ClearPausedState();

    // Run forward from the current execution point.
    gNavigation->mForwardPhase.Enter(mPoint);
    return;
  }

  // Search backwards in the execution space.
  if (mPoint.HasPosition()) {
    CheckpointId start = gNavigation->LastCheckpoint();

    // Skip over any temporary checkpoint we saved.
    if (mSavedTemporaryCheckpoint) {
      MOZ_RELEASE_ASSERT(start.mTemporary);
      start.mTemporary--;
    }
    gNavigation->mFindLastHitPhase.Enter(start, Some(mPoint),
                                         /* aIncludeEnd = */ false);
  } else {
    // We can't rewind past the beginning of the replay.
    MOZ_RELEASE_ASSERT(mPoint.mCheckpoint != CheckpointId::First);

    CheckpointId start(mPoint.mCheckpoint - 1);
    gNavigation->mFindLastHitPhase.Enter(start, Nothing(),
                                         /* aIncludeEnd = */ false);
  }
  Unreachable();
}

void PausedPhase::RestoreCheckpoint(size_t aCheckpoint) {
  ExecutionPoint target = gNavigation->CheckpointExecutionPoint(aCheckpoint);
  bool rewind = target != mPoint;
  Enter(target, rewind, /* aRecordingEndpoint = */ false);
}

void PausedPhase::RunToPoint(const ExecutionPoint& aTarget) {
  // This may only be used when we are paused at a normal checkpoint.
  MOZ_RELEASE_ASSERT(!mPoint.HasPosition());
  size_t checkpoint = mPoint.mCheckpoint;

  MOZ_RELEASE_ASSERT(aTarget.mCheckpoint == checkpoint);
  ResumeExecution();
  gNavigation->mReachBreakpointPhase.Enter(
      CheckpointId(checkpoint), /* aRewind = */ false, aTarget,
      /* aTemporaryCheckpoint = */ Nothing());
}

void PausedPhase::HandleDebuggerRequest(js::CharBuffer* aRequestBuffer) {
  MOZ_RELEASE_ASSERT(!mRecoveringFromDivergence);
  MOZ_RELEASE_ASSERT(!mResumeForward);

  mRequests.emplaceBack();
  size_t index = mRequests.length() - 1;
  mRequests[index].mRequestBuffer.append(aRequestBuffer->begin(),
                                         aRequestBuffer->length());

  mRequestIndex = index;

  js::CharBuffer responseBuffer;
  js::ProcessRequest(aRequestBuffer->begin(), aRequestBuffer->length(),
                     &responseBuffer);

  delete aRequestBuffer;

  if (gNavigation->mPhase != this) {
    // We saved a temporary checkpoint by calling MaybeDivergeFromRecording
    // within ProcessRequest, then restored it while scanning backwards.
    ResumeExecution();
    return;
  }

  if (!mResumeForward && !mRecoveringFromDivergence) {
    // We processed this request normally. Remember the response and send it to
    // the middleman process.
    MOZ_RELEASE_ASSERT(index == mRequestIndex);
    mRequests[index].mResponseBuffer.append(responseBuffer.begin(),
                                            responseBuffer.length());
    child::RespondToRequest(responseBuffer);
    return;
  }

  if (mResumeForward) {
    // We rewound to erase side effects from the temporary checkpoint we saved
    // under ProcessRequest. Just start running forward.
    MOZ_RELEASE_ASSERT(!mRecoveringFromDivergence);
    gNavigation->mForwardPhase.Enter(mPoint);
    return;
  }

  // We rewound after having an unhandled recording divergence while processing
  // mRequests[index] or some later request. We need to redo all requests up to
  // the last request we received.

  // Remember that the last request triggered an unhandled divergence.
  MOZ_RELEASE_ASSERT(!mRequests.back().mUnhandledDivergence);
  mRequests.back().mUnhandledDivergence = true;

  for (size_t i = index; i < mRequests.length(); i++) {
    RequestInfo& info = mRequests[i];
    mRequestIndex = i;

    if (i == index) {
      // We just performed this request, and responseBuffer has the right
      // contents.
    } else {
      responseBuffer.clear();
      js::ProcessRequest(info.mRequestBuffer.begin(),
                         info.mRequestBuffer.length(), &responseBuffer);
    }

    if (i < mRequests.length() - 1) {
      // This is an old request, and we don't need to send another
      // response to it. Make sure the response we just generated matched
      // the earlier one we sent, though.
      MOZ_RELEASE_ASSERT(responseBuffer.length() ==
                         info.mResponseBuffer.length());
      MOZ_RELEASE_ASSERT(
          memcmp(responseBuffer.begin(), info.mResponseBuffer.begin(),
                 responseBuffer.length() * sizeof(char16_t)) == 0);
    } else {
      // This is the current request we need to respond to.
      MOZ_RELEASE_ASSERT(info.mResponseBuffer.empty());
      info.mResponseBuffer.append(responseBuffer.begin(),
                                  responseBuffer.length());
      child::RespondToRequest(responseBuffer);
    }
  }

  // We've finished recovering, and can now process new incoming requests.
  mRecoveringFromDivergence = false;
}

bool PausedPhase::MaybeDivergeFromRecording() {
  if (!ThisProcessCanRewind()) {
    // Recording divergence is not supported if we can't rewind. We can't
    // simply allow execution to proceed from here as if we were not
    // diverged, since any events or other activity that show up afterwards
    // will not be reflected in the recording.
    return false;
  }

  size_t index = mRequestIndex;

  if (!EnsureTemporaryCheckpoint()) {
    // One of the premature exit cases was hit in EnsureTemporaryCheckpoint.
    // Don't allow any operations that can diverge from the recording.
    return false;
  }

  if (mRequests[index].mUnhandledDivergence) {
    // We tried to process this request before and had an unhandled divergence.
    // Disallow the request handler from doing anything that might diverge from
    // the recording.
    return false;
  }

  DivergeFromRecording();
  return true;
}

bool PausedPhase::EnsureTemporaryCheckpoint() {
  if (mSavedTemporaryCheckpoint) {
    return true;
  }

  // We need to save a temporary checkpoint that we can restore if we hit
  // a recording divergence.
  mSavedTemporaryCheckpoint = true;

  size_t index = mRequestIndex;
  if (gNavigation->SaveTemporaryCheckpoint(mPoint)) {
    // We just saved the temporary checkpoint.
    return true;
  }

  // We just rewound here.
  if (gNavigation->mPhase != this) {
    // We are no longer paused at this point. We should be searching
    // backwards in the region after this temporary checkpoint was taken.
    // Return false to ensure we don't perform any side effects before
    // resuming forward.
    return false;
  }

  // We are still paused at this point. Either we had an unhandled
  // recording divergence, or we intentionally rewound to erase side
  // effects that occurred while paused here.
  MOZ_RELEASE_ASSERT(!mRecoveringFromDivergence);

  if (mResumeForward) {
    // We can't diverge from the recording before resuming forward execution.
    return false;
  }

  mRecoveringFromDivergence = true;

  if (index == mRequestIndex) {
    // We had an unhandled divergence for the same request where we
    // created the temporary checkpoint. mUnhandledDivergence hasn't been
    // set yet, but return now to avoid triggering the same divergence
    // and rewinding again.
    return false;
  }

  // Allow the caller to check mUnhandledDivergence.
  return true;
}

ExecutionPoint PausedPhase::CurrentExecutionPoint() { return mPoint; }

///////////////////////////////////////////////////////////////////////////////
// ForwardPhase
///////////////////////////////////////////////////////////////////////////////

void ForwardPhase::Enter(const ExecutionPoint& aPoint) {
  mPoint = aPoint;

  gNavigation->SetPhase(this);

  // Install handlers for all breakpoints.
  for (const BreakpointPosition& breakpoint : gNavigation->mBreakpoints) {
    js::EnsurePositionHandler(breakpoint);
  }

  ResumeExecution();
}

void ForwardPhase::AfterCheckpoint(const CheckpointId& aCheckpoint) {
  MOZ_RELEASE_ASSERT(!aCheckpoint.mTemporary &&
                     aCheckpoint.mNormal == mPoint.mCheckpoint + 1);
  gNavigation->mPausedPhase.Enter(
      gNavigation->CheckpointExecutionPoint(aCheckpoint.mNormal));
}

void ForwardPhase::PositionHit(const ExecutionPoint& aPoint) {
  bool hitBreakpoint = false;
  for (const BreakpointPosition& breakpoint : gNavigation->mBreakpoints) {
    if (breakpoint.Subsumes(aPoint.mPosition)) {
      hitBreakpoint = true;
    }
  }

  if (hitBreakpoint) {
    gNavigation->mPausedPhase.Enter(aPoint);
  }
}

void ForwardPhase::HitRecordingEndpoint(const ExecutionPoint& aPoint) {
  nsAutoCString str;
  ExecutionPointToString(aPoint, str);

  gNavigation->mPausedPhase.Enter(aPoint, /* aRewind = */ false,
                                  /* aRecordingEndpoint = */ true);
}

///////////////////////////////////////////////////////////////////////////////
// ReachBreakpointPhase
///////////////////////////////////////////////////////////////////////////////

void ReachBreakpointPhase::Enter(
    const CheckpointId& aStart, bool aRewind, const ExecutionPoint& aPoint,
    const Maybe<ExecutionPoint>& aTemporaryCheckpoint) {
  MOZ_RELEASE_ASSERT(aPoint.HasPosition());
  MOZ_RELEASE_ASSERT(aTemporaryCheckpoint.isNothing() ||
                     (aTemporaryCheckpoint.ref().HasPosition() &&
                      aTemporaryCheckpoint.ref() != aPoint));
  mStart = aStart;
  mPoint = aPoint;
  mTemporaryCheckpoint = aTemporaryCheckpoint;
  mSavedTemporaryCheckpoint = false;

  gNavigation->SetPhase(this);

  if (aRewind) {
    RestoreCheckpointAndResume(aStart);
    Unreachable();
  } else {
    AfterCheckpoint(aStart);
  }
}

void ReachBreakpointPhase::AfterCheckpoint(const CheckpointId& aCheckpoint) {
  if (aCheckpoint == mStart && mTemporaryCheckpoint.isSome()) {
    js::EnsurePositionHandler(mTemporaryCheckpoint.ref().mPosition);

    // Remember the time we started running forwards from the initial
    // checkpoint.
    mStartTime = CurrentTime();
  } else {
    MOZ_RELEASE_ASSERT(
        (aCheckpoint == mStart && mTemporaryCheckpoint.isNothing()) ||
        (aCheckpoint == mStart.NextCheckpoint(/* aTemporary = */ true) &&
         mSavedTemporaryCheckpoint));
  }

  js::EnsurePositionHandler(mPoint.mPosition);
}

// The number of milliseconds to elapse during a ReachBreakpoint search before
// we will save a temporary checkpoint.
static const double kTemporaryCheckpointThresholdMs = 10;

void AlwaysSaveTemporaryCheckpoints() {
  gNavigation->mAlwaysSaveTemporaryCheckpoints = true;
}

void ReachBreakpointPhase::PositionHit(const ExecutionPoint& aPoint) {
  if (mTemporaryCheckpoint.isSome() && mTemporaryCheckpoint.ref() == aPoint) {
    // We've reached the point at which we have the option of saving a
    // temporary checkpoint.
    double elapsedMs = (CurrentTime() - mStartTime) / 1000.0;
    if (elapsedMs >= kTemporaryCheckpointThresholdMs ||
        gNavigation->mAlwaysSaveTemporaryCheckpoints) {
      MOZ_RELEASE_ASSERT(!mSavedTemporaryCheckpoint);
      mSavedTemporaryCheckpoint = true;

      if (!gNavigation->SaveTemporaryCheckpoint(aPoint)) {
        // We just restored the checkpoint, and could be in any phase.
        gNavigation->PositionHit(aPoint);
        return;
      }
    }
  }

  if (mPoint == aPoint) {
    gNavigation->mPausedPhase.Enter(aPoint);
  }
}

///////////////////////////////////////////////////////////////////////////////
// FindLastHitPhase
///////////////////////////////////////////////////////////////////////////////

void FindLastHitPhase::Enter(const CheckpointId& aStart,
                             const Maybe<ExecutionPoint>& aEnd,
                             bool aIncludeEnd) {
  MOZ_RELEASE_ASSERT(aEnd.isNothing() || aEnd.ref().HasPosition());

  mStart = aStart;
  mEnd = aEnd;
  mIncludeEnd = aIncludeEnd;
  mCounter = 0;
  mTrackedPositions.clear();

  gNavigation->SetPhase(this);

  // All breakpoints are tracked positions.
  for (const BreakpointPosition& breakpoint : gNavigation->mBreakpoints) {
    if (breakpoint.IsValid()) {
      mTrackedPositions.emplaceBack(breakpoint);
    }
  }

  // Entry points to scripts containing breakpoints are tracked positions.
  for (const BreakpointPosition& breakpoint : gNavigation->mBreakpoints) {
    Maybe<BreakpointPosition> entry = GetEntryPosition(breakpoint);
    if (entry.isSome()) {
      mTrackedPositions.emplaceBack(entry.ref());
    }
  }

  RestoreCheckpointAndResume(mStart);
  Unreachable();
}

void FindLastHitPhase::AfterCheckpoint(const CheckpointId& aCheckpoint) {
  if (aCheckpoint == mStart.NextCheckpoint(/* aTemporary = */ false)) {
    // We reached the next checkpoint, and are done searching.
    MOZ_RELEASE_ASSERT(mEnd.isNothing());
    OnRegionEnd();
    Unreachable();
  }

  // We are at the start of the search.
  MOZ_RELEASE_ASSERT(aCheckpoint == mStart);

  for (const TrackedPosition& tracked : mTrackedPositions) {
    js::EnsurePositionHandler(tracked.mPosition);
  }

  if (mEnd.isSome()) {
    js::EnsurePositionHandler(mEnd.ref().mPosition);
  }
}

void FindLastHitPhase::PositionHit(const ExecutionPoint& aPoint) {
  if (!mIncludeEnd) {
    CheckForRegionEnd(aPoint);
  }

  ++mCounter;

  for (TrackedPosition& tracked : mTrackedPositions) {
    if (tracked.mPosition.Subsumes(aPoint.mPosition)) {
      tracked.mLastHit = aPoint;
      tracked.mLastHitCount = mCounter;
      break;
    }
  }

  if (mIncludeEnd) {
    CheckForRegionEnd(aPoint);
  }
}

void FindLastHitPhase::CheckForRegionEnd(const ExecutionPoint& aPoint) {
  if (mEnd.isSome() && mEnd.ref() == aPoint) {
    OnRegionEnd();
    Unreachable();
  }
}

void FindLastHitPhase::HitRecordingEndpoint(const ExecutionPoint& aPoint) {
  OnRegionEnd();
  Unreachable();
}

const FindLastHitPhase::TrackedPosition& FindLastHitPhase::FindTrackedPosition(
    const BreakpointPosition& aPos) {
  for (const TrackedPosition& tracked : mTrackedPositions) {
    if (tracked.mPosition == aPos) {
      return tracked;
    }
  }
  MOZ_CRASH("Could not find tracked position");
}

void FindLastHitPhase::OnRegionEnd() {
  // Find the point of the last hit which coincides with a breakpoint.
  Maybe<TrackedPosition> lastBreakpoint;
  for (const BreakpointPosition& breakpoint : gNavigation->mBreakpoints) {
    const TrackedPosition& tracked = FindTrackedPosition(breakpoint);
    if (tracked.mLastHit.HasPosition() &&
        (lastBreakpoint.isNothing() ||
         lastBreakpoint.ref().mLastHitCount < tracked.mLastHitCount)) {
      lastBreakpoint = Some(tracked);
    }
  }

  if (lastBreakpoint.isNothing()) {
    // No breakpoints were encountered in the search space.
    if (mStart.mTemporary) {
      // We started searching forwards from a temporary checkpoint.
      // Continue searching backwards without notifying the middleman.
      CheckpointId start = mStart;
      start.mTemporary--;
      ExecutionPoint end = gNavigation->LastTemporaryCheckpointLocation();
      if (end.HasPosition()) {
        // The temporary checkpoint comes immediately after its associated
        // execution point. As we search backwards we need to look for hits at
        // that execution point itself.
        gNavigation->mFindLastHitPhase.Enter(start, Some(end),
                                             /* aIncludeEnd = */ true);
        Unreachable();
      } else {
        // The last temporary checkpoint may be at the same execution point as
        // the last normal checkpoint, if it was created while handling
        // debugger requests there.
      }
    }

    // Rewind to the last normal checkpoint and pause.
    gNavigation->mPausedPhase.Enter(
        gNavigation->CheckpointExecutionPoint(mStart.mNormal),
        /* aRewind = */ true);
    Unreachable();
  }

  // When running backwards, we don't want to place temporary checkpoints at
  // the breakpoint where we are going to stop at. If the user continues
  // rewinding then we will just have to discard the checkpoint and waste the
  // work we did in saving it.
  //
  // Instead, try to place a temporary checkpoint at the last time the
  // breakpoint's script was entered. This optimizes for the case of stepping
  // around within a frame.
  Maybe<BreakpointPosition> baseEntry =
      GetEntryPosition(lastBreakpoint.ref().mPosition);
  if (baseEntry.isSome()) {
    const TrackedPosition& tracked = FindTrackedPosition(baseEntry.ref());
    if (tracked.mLastHit.HasPosition() &&
        tracked.mLastHitCount < lastBreakpoint.ref().mLastHitCount) {
      gNavigation->mReachBreakpointPhase.Enter(mStart, /* aRewind = */ true,
                                               lastBreakpoint.ref().mLastHit,
                                               Some(tracked.mLastHit));
      Unreachable();
    }
  }

  // There was no suitable place for a temporary checkpoint, so rewind to the
  // last checkpoint and play forward to the last breakpoint hit we found.
  gNavigation->mReachBreakpointPhase.Enter(
      mStart, /* aRewind = */ true, lastBreakpoint.ref().mLastHit, Nothing());
  Unreachable();
}

///////////////////////////////////////////////////////////////////////////////
// Hooks
///////////////////////////////////////////////////////////////////////////////

bool IsInitialized() { return !!gNavigation; }

void BeforeCheckpoint() {
  if (!IsInitialized()) {
    void* navigationMem =
        AllocateMemory(sizeof(NavigationState), MemoryKind::Navigation);
    gNavigation = new (navigationMem) NavigationState();

    js::SetupDevtoolsSandbox();

    // Set the progress counter to zero before the first checkpoint. Execution
    // that occurred before this checkpoint cannot be rewound to.
    *ExecutionProgressCounter() = 0;
  }

  AutoDisallowThreadEvents disallow;

  // Reset the debugger to a consistent state before each checkpoint.
  js::ClearPositionHandlers();
}

void AfterCheckpoint(const CheckpointId& aCheckpoint) {
  AutoDisallowThreadEvents disallow;

  MOZ_RELEASE_ASSERT(IsRecordingOrReplaying());
  gNavigation->AfterCheckpoint(aCheckpoint);
}

size_t LastNormalCheckpoint() { return gNavigation->LastCheckpoint().mNormal; }

void DebuggerRequest(js::CharBuffer* aRequestBuffer) {
  gNavigation->HandleDebuggerRequest(aRequestBuffer);
}

void AddBreakpoint(const BreakpointPosition& aPosition) {
  gNavigation->mBreakpoints.append(aPosition);
}

void ClearBreakpoints() {
  if (gNavigation) {
    gNavigation->mBreakpoints.clear();
  }
}

void Resume(bool aForward) {
  // For the primordial resume sent at startup, the navigation state will not
  // have been initialized yet.
  if (!gNavigation) {
    ResumeExecution();
    return;
  }
  gNavigation->Resume(aForward);
}

void RestoreCheckpoint(size_t aId) { gNavigation->RestoreCheckpoint(aId); }

void RunToPoint(const ExecutionPoint& aTarget) {
  gNavigation->RunToPoint(aTarget);
}

ExecutionPoint GetRecordingEndpoint() {
  if (IsRecording()) {
    return gNavigation->CurrentExecutionPoint();
  } else {
    return gNavigation->LastRecordingEndpoint();
  }
}

void SetRecordingEndpoint(size_t aIndex, const ExecutionPoint& aEndpoint) {
  MOZ_RELEASE_ASSERT(IsReplaying());
  gNavigation->SetRecordingEndpoint(aIndex, aEndpoint);
}

static ProgressCounter gProgressCounter;

extern "C" {

MOZ_EXPORT ProgressCounter* RecordReplayInterface_ExecutionProgressCounter() {
  return &gProgressCounter;
}

}  // extern "C"

ExecutionPoint CurrentExecutionPoint(
    const Maybe<BreakpointPosition>& aPosition) {
  if (aPosition.isSome()) {
    return ExecutionPoint(gNavigation->LastCheckpoint().mNormal,
                          gProgressCounter, aPosition.ref());
  }
  return gNavigation->CurrentExecutionPoint();
}

void PositionHit(const BreakpointPosition& position) {
  AutoDisallowThreadEvents disallow;
  gNavigation->PositionHit(CurrentExecutionPoint(Some(position)));
}

extern "C" {

MOZ_EXPORT ProgressCounter RecordReplayInterface_NewTimeWarpTarget() {
  if (AreThreadEventsDisallowed()) {
    return 0;
  }

  // NewTimeWarpTarget() must be called at consistent points between recording
  // and replaying.
  RecordReplayAssert("NewTimeWarpTarget");

  if (!gNavigation) {
    return 0;
  }

  // Advance the progress counter for each time warp target. This can be called
  // at any place and any number of times where recorded events are allowed.
  ProgressCounter progress = ++gProgressCounter;

  PositionHit(BreakpointPosition(BreakpointPosition::WarpTarget));
  return progress;
}

}  // extern "C"

ExecutionPoint TimeWarpTargetExecutionPoint(ProgressCounter aTarget) {
  // To construct an ExecutionPoint, we need the most recent checkpoint prior
  // to aTarget. We could do a binary search here, but this code is cold and a
  // linear search is more straightforwardly correct.
  size_t checkpoint;
  for (checkpoint = gNavigation->mCheckpointProgress.length() - 1;
       checkpoint >= CheckpointId::First; checkpoint--) {
    if (gNavigation->mCheckpointProgress[checkpoint] < aTarget) {
      break;
    }
  }
  MOZ_RELEASE_ASSERT(checkpoint >= CheckpointId::First);

  return ExecutionPoint(checkpoint, aTarget,
                        BreakpointPosition(BreakpointPosition::WarpTarget));
}

bool MaybeDivergeFromRecording() {
  return gNavigation->MaybeDivergeFromRecording();
}

}  // namespace navigation
}  // namespace recordreplay
}  // namespace mozilla
