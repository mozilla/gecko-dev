/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_recordreplay_ChildInternal_h
#define mozilla_recordreplay_ChildInternal_h

#include "Channel.h"
#include "ChildIPC.h"
#include "JSControl.h"
#include "ExternalCall.h"
#include "Monitor.h"

// This file has internal definitions for communication between the main
// record/replay infrastructure and child side IPC code.

namespace mozilla {
namespace recordreplay {
namespace child {

void SetupRecordReplayChannel(int aArgc, char* aArgv[]);

// Information about a crash that occurred.
struct MinidumpInfo {
  int mExceptionType;
  int mCode;
  int mSubcode;
  mach_port_t mThread;
  mach_port_t mTask;

  MinidumpInfo(int aExceptionType, int aCode, int aSubcode, mach_port_t aThread,
               mach_port_t aTask)
      : mExceptionType(aExceptionType),
        mCode(aCode),
        mSubcode(aSubcode),
        mThread(aThread),
        mTask(aTask) {}
};

void ReportCrash(const MinidumpInfo& aMinidumpInfo, void* aFaultingAddress);

// Return whether this process is shutting down.
bool ExitCalled();

// Generate a minidump and report a fatal error to the middleman process.
void ReportFatalError(const char* aFormat, ...);

// Report an error that will abort the record/replay tab's execution.
void ReportCriticalError(const char* aMessage);

// Report to the middleman that we had an unhandled recording divergence, and
// that execution in this process cannot continue.
void ReportUnhandledDivergence();

// If unhandled divergences are not allowed then we will crash instead of
// reporting them.
void SetUnhandledDivergenceAllowed(bool aAllowed);

// Get the unique ID of this child.
size_t GetId();
size_t GetForkId();

// Monitor used for various synchronization tasks.
extern Monitor* gMonitor;

// Block until this child has loaded the specified amount of recording data.
void EnsureRecordingLength(size_t aLength);

// Notify the middleman that the last manifest was finished.
void ManifestFinished(const js::CharBuffer& aResponse);

// Send messages operating on external calls.
void SendExternalCallRequest(ExternalCallId aId,
                             const char* aInputData, size_t aInputSize,
                             InfallibleVector<char>* aOutputData,
                             bool* aOutputUnavailable);

// Send the output from an external call to the root replaying process,
// to fill in its external call cache.
void SendExternalCallOutput(ExternalCallId aId,
                            const char* aOutputData, size_t aOutputSize);

// Store graphics data in aData. If aRepaint is set then the graphics will be
// repainted according to the current state, otherwise the most recently painted
// graphics will be returned.
bool GetGraphics(bool aRepaint, const nsACString& aMimeType,
                 const nsACString& aEncodeOptions, nsACString& aData);

// If appropriate, associate a checkpoint with the most recent paint.
void MaybeSetCheckpointForLastPaint(size_t aCheckpoint);

// Fork this process and assign a new fork ID to the new process.
void PerformFork(size_t aForkId);

// Called to perform the actual fork. Returns whether this is the original process.
bool RawFork();

// Incorporate any new data into the recording. If there is no more data and
// aRequireMore is set, crash.
void AddPendingRecordingData(bool aRequireMore);

// Set any text to be printed if this process crashes.
void SetCrashNote(const char* aNote);

// Send scan data to be incorporated in the root process.
void SendScanDataToRoot(const char* aData, size_t aSize);

// Get the amount of memory used by this process, in bytes.
uint64_t GetMemoryUsage();

extern nsCString gReplayJS;

// Handle a child-side log message.
void PrintLog(const nsAString& aText);
void PrintLog(const char* aFormat, ...);

// Access the shared key-value database in the root replaying process.
void SetSharedKey(const nsAutoCString& aKey, const nsAutoCString& aValue);
void GetSharedKey(const nsAutoCString& aKey, nsAutoCString& aValue);

}  // namespace child
}  // namespace recordreplay
}  // namespace mozilla

#endif  // mozilla_recordreplay_ChildInternal_h
