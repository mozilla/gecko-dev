/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_recordreplay_JSControl_h
#define mozilla_recordreplay_JSControl_h

#include "jsapi.h"

#include "Channel.h"
#include "InfallibleVector.h"
#include "ProcessRewind.h"

#include "mozilla/DefineEnum.h"
#include "nsString.h"

namespace mozilla {
namespace recordreplay {

struct Message;

namespace parent {
class ChildProcessInfo;
}

namespace js {

// This file manages interactions between the record/replay infrastructure and
// JS code. This interaction can occur in two ways:
//
// - In the middleman process, devtools server code can use the
//   RecordReplayControl object to send requests to the recording/replaying
//   child process and control its behavior.
//
// - In the recording/replaying process, a JS sandbox is created before the
//   first checkpoint is reached, which responds to the middleman's requests.
//   The RecordReplayControl object is also provided here, but has a different
//   interface which allows the JS to query the current process.

// Buffer type used for encoding object data.
typedef InfallibleVector<char16_t> CharBuffer;

// Read the replay JS to load at the first checkpoint.
void ReadReplayJS(const char* aFile);

// JS state is initialized when the first checkpoint is reached.
bool IsInitialized();

// Start processing a manifest received from the middleman.
void ManifestStart(const CharBuffer& aContents);

// The following hooks are used in the middleman process to call methods defined
// by the middleman control logic.

// Set the status of a cloud connection.
void SetConnectionStatus(uint32_t aChannelId, const nsCString& aStatus);

// The following hooks are used in the recording/replaying process to
// call methods defined by the JS sandbox.

// Called when running forward and a normal checkpoint was reached.
void HitCheckpoint(size_t aCheckpoint, TimeDuration aTime);

// Return whether creating a checkpoint now is allowed.
bool CanCreateCheckpoint();

// Called when a child crashes, returning whether the crash was recovered from.
bool RecoverFromCrash(size_t aRootId, size_t aForkId);

// Called when painting the graphics at a checkpoint finishes.
void PaintComplete(size_t aCheckpoint);

// Called when a mouse event occurs.
void OnMouseEvent(const TimeDuration& aTime, const char* aType, int32_t aX, int32_t aY);

// Send recording data to the UI process for uploading to the cloud.
// aTotalLength and aRecordingDuration are set if a description should
// be included (this is the end of the recording).
void SendRecordingData(size_t aOffset, const uint8_t* aData, size_t aLength,
                       const Maybe<size_t>& aTotalLength,
                       const Maybe<TimeDuration>& aRecordingDuration);

// Accessors for state which can be accessed from JS.

// Mark a time span when the main thread is idle.
void BeginIdleTime();
void EndIdleTime();

// Get the total amount of main thread idle time.
double TotalIdleTime();

// Incorporate scan data into this process.
void AddScanDataMessage(Message::UniquePtr aMsg);

// Dump all parsed content to aFd.
void DumpContent(FileHandle aFd);

// Utilities.

JSString* ConvertStringToJSString(JSContext* aCx, const nsAString& aString);
void ConvertJSStringToCString(JSContext* aCx, JSString* aString,
                              nsAutoCString& aResult);

}  // namespace js
}  // namespace recordreplay
}  // namespace mozilla

#endif  // mozilla_recordreplay_JSControl_h
