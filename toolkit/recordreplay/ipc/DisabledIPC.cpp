/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ParentIPC.h"
#include "ChildIPC.h"

namespace mozilla {
  namespace recordreplay {
    void BeginRunEvent(const TimeStamp&) {}
    void EndRunEvent() {}
    void MaybeCreateCheckpoint() {}
    void CreateCheckpoint() {}
    void OnWidgetEvent(dom::BrowserChild*, const WidgetEvent&) {}

    namespace parent {
      const char* CurrentFirefoxVersion() {
        return "74.0a1";
      }
      base::ProcessId ParentProcessId() { return 0; }
      void GetCloudReplayStatus(nsAString&) {}
      double ElapsedTime() { return 0; }
      void OnCloudMessage(long, JS::HandleObject) {}
      void SaveCloudRecording(const nsAString&) {}
      void SetCloudReplayStatusCallback(JS::HandleValue) {}
      void AddToLog(const nsAString&, bool) {}
      void RegisterConnectionWorker(JS::HandleObject) {}
      void SetConnectionStatus(uint32_t, const nsCString&) {}
      void SaveRecording(const ipc::FileDescriptor&) {}
      const char* SaveAllRecordingsDirectory() { return nullptr; }
      void InitializeMiddleman(int, char* aArgv[], base::ProcessId,
                               const base::SharedMemoryHandle&,
                               const ipc::FileDescriptor&) {}
      ipc::MessageChannel* ChannelToUIProcess() { MOZ_CRASH(); }
      void OpenChannel(base::ProcessId, uint32_t, ipc::FileDescriptor*) {}
      void CreateReplayingCloudProcess(dom::ContentParent*, uint32_t) {}
      void GetArgumentsForChildProcess(base::ProcessId aMiddlemanPid,
                                       uint32_t aChannelId,
                                       const char* aRecordingFile, bool aRecording,
                                       std::vector<std::string>& aExtraArgs) {}
      void ContentParentDestroyed(dom::ContentParent*) {}
      void InitializeUIProcess(int, char**) {}
      bool UseCloudForReplayingProcesses() { return true; }
      void GetWebReplayJS(nsAutoCString&, nsAutoCString&) {}
      void EnsureUIStateInitialized() {}
    }

    namespace child {
      void InitRecordingOrReplayingProcess(int*, char***) {}
      base::ProcessId ParentProcessId() { return 0; }
      base::ProcessId MiddlemanProcessId() { return 0; }
      void NotifyPaintStart() {}
      void NotifyPaintComplete() {}
      void SetWebReplayJS(const nsCString&, const nsCString&) {}
      void SetVsyncObserver(VsyncObserver*) {}
      already_AddRefed<gfx::DrawTarget> DrawTargetForRemoteDrawing(
          LayoutDeviceIntSize) {
        MOZ_CRASH();
      }
      bool OnVsync() { return true; }
    }
  }
}
