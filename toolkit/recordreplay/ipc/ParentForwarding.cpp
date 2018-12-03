/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file has the logic which the middleman process uses to forward IPDL
// messages from the recording process to the UI process, and from the UI
// process to either itself, the recording process, or both.

#include "ParentInternal.h"

#include "mozilla/dom/PBrowserChild.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/layers/CompositorBridgeChild.h"

namespace mozilla {
namespace recordreplay {
namespace parent {

static bool HandleMessageInMiddleman(ipc::Side aSide,
                                     const IPC::Message& aMessage) {
  IPC::Message::msgid_t type = aMessage.type();

  if (aSide == ipc::ParentSide) {
    return false;
  }

  // Handle messages that should be sent to both the middleman and the
  // child process.
  if (  // Initialization that must be performed in both processes.
      type == dom::PContent::Msg_PBrowserConstructor__ID ||
      type == dom::PContent::Msg_RegisterChrome__ID ||
      type == dom::PContent::Msg_SetXPCOMProcessAttributes__ID ||
      type == dom::PContent::Msg_UpdateSharedData__ID ||
      type == dom::PContent::Msg_SetProcessSandbox__ID ||
      // Graphics messages that affect both processes.
      type == dom::PBrowser::Msg_InitRendering__ID ||
      type == dom::PBrowser::Msg_SetDocShellIsActive__ID ||
      type == dom::PBrowser::Msg_RenderLayers__ID ||
      type == dom::PBrowser::Msg_UpdateDimensions__ID ||
      // These messages perform some graphics related initialization.
      type == dom::PBrowser::Msg_LoadURL__ID ||
      type == dom::PBrowser::Msg_Show__ID ||
      // May be loading devtools code that runs in the middleman process.
      type == dom::PBrowser::Msg_LoadRemoteScript__ID ||
      // May be sending a message for receipt by devtools code.
      type == dom::PBrowser::Msg_AsyncMessage__ID ||
      // Teardown that must be performed in both processes.
      type == dom::PBrowser::Msg_Destroy__ID) {
    dom::ContentChild* contentChild = dom::ContentChild::GetSingleton();

    if (type >= dom::PBrowser::PBrowserStart &&
        type <= dom::PBrowser::PBrowserEnd) {
      // Ignore messages sent from the parent to browsers that do not have an
      // actor in the middleman process. PBrowser may be allocated on either
      // side of the IPDL channel, and when allocated by the recording child
      // there will not be a corresponding actor in the middleman.
      nsTArray<dom::PBrowserChild*> browsers;
      contentChild->ManagedPBrowserChild(browsers);
      bool found = false;
      for (ipc::IProtocol* child : browsers) {
        if (child->Id() == aMessage.routing_id()) {
          found = true;
          break;
        }
      }
      if (!found) {
        return false;
      }
    }

    ipc::IProtocol::Result r =
        contentChild->PContentChild::OnMessageReceived(aMessage);
    MOZ_RELEASE_ASSERT(r == ipc::IProtocol::MsgProcessed);
    if (type == dom::PContent::Msg_SetXPCOMProcessAttributes__ID) {
      // Preferences are initialized via the SetXPCOMProcessAttributes message.
      PreferencesLoaded();
    }
    return false;
  }

  // Handle messages that should only be sent to the middleman.
  if (  // Initialization that should only happen in the middleman.
      type == dom::PContent::Msg_InitRendering__ID ||
      // Record/replay specific messages.
      type == dom::PContent::Msg_SaveRecording__ID ||
      // Teardown that should only happen in the middleman.
      type == dom::PContent::Msg_Shutdown__ID) {
    ipc::IProtocol::Result r =
        dom::ContentChild::GetSingleton()->PContentChild::OnMessageReceived(
            aMessage);
    MOZ_RELEASE_ASSERT(r == ipc::IProtocol::MsgProcessed);
    return true;
  }

  // Send input events to the middleman when the active child is replaying,
  // so that UI elements such as the replay overlay can be interacted with.
  if (!ActiveChildIsRecording() &&
      nsContentUtils::IsMessageInputEvent(aMessage)) {
    ipc::IProtocol::Result r =
        dom::ContentChild::GetSingleton()->PContentChild::OnMessageReceived(
            aMessage);
    MOZ_RELEASE_ASSERT(r == ipc::IProtocol::MsgProcessed);
    return true;
  }

  // The content process has its own compositor, so compositor messages from
  // the UI process should only be handled in the middleman.
  if (type >= layers::PCompositorBridge::PCompositorBridgeStart &&
      type <= layers::PCompositorBridge::PCompositorBridgeEnd) {
    layers::CompositorBridgeChild* compositorChild =
        layers::CompositorBridgeChild::Get();
    ipc::IProtocol::Result r = compositorChild->OnMessageReceived(aMessage);
    MOZ_RELEASE_ASSERT(r == ipc::IProtocol::MsgProcessed);
    return true;
  }

  return false;
}

// Return whether a message should be sent to the recording child, even if it
// is not currently active.
static bool AlwaysForwardMessage(const IPC::Message& aMessage) {
  // Always forward messages in repaint stress mode, as the active child is
  // almost always a replaying child and lost messages make it hard to load
  // pages completely.
  if (InRepaintStressMode()) {
    return true;
  }

  IPC::Message::msgid_t type = aMessage.type();

  // Forward close messages so that the tab shuts down properly even if it is
  // currently replaying.
  return type == dom::PBrowser::Msg_Destroy__ID;
}

static bool gMainThreadIsWaitingForIPDLReply = false;

bool MainThreadIsWaitingForIPDLReply() {
  return gMainThreadIsWaitingForIPDLReply;
}

// Helper for places where the main thread will block while waiting on a
// synchronous IPDL reply from a child process. Incoming messages from the
// child must be handled immediately.
struct MOZ_RAII AutoMarkMainThreadWaitingForIPDLReply {
  AutoMarkMainThreadWaitingForIPDLReply() {
    MOZ_RELEASE_ASSERT(NS_IsMainThread());
    MOZ_RELEASE_ASSERT(!gMainThreadIsWaitingForIPDLReply);
    ResumeBeforeWaitingForIPDLReply();
    gMainThreadIsWaitingForIPDLReply = true;
  }

  ~AutoMarkMainThreadWaitingForIPDLReply() {
    gMainThreadIsWaitingForIPDLReply = false;
  }
};

static void BeginShutdown() {
  // If there is a channel error or anything that could result from the child
  // crashing, cleanly shutdown this process so that we don't generate a
  // separate minidump which masks the initial failure.
  MainThreadMessageLoop()->PostTask(NewRunnableFunction("Shutdown", Shutdown));
}

class MiddlemanProtocol : public ipc::IToplevelProtocol {
 public:
  ipc::Side mSide;
  MiddlemanProtocol* mOpposite;
  MessageLoop* mOppositeMessageLoop;

  explicit MiddlemanProtocol(ipc::Side aSide)
      : ipc::IToplevelProtocol("MiddlemanProtocol", PContentMsgStart, aSide),
        mSide(aSide),
        mOpposite(nullptr),
        mOppositeMessageLoop(nullptr) {}

  virtual void RemoveManagee(int32_t, IProtocol*) override {
    MOZ_CRASH("MiddlemanProtocol::RemoveManagee");
  }

  static void ForwardMessageAsync(MiddlemanProtocol* aProtocol,
                                  Message* aMessage) {
    if (ActiveChildIsRecording() || AlwaysForwardMessage(*aMessage)) {
      PrintSpew("ForwardAsyncMsg %s %s %d\n",
                (aProtocol->mSide == ipc::ChildSide) ? "Child" : "Parent",
                IPC::StringFromIPCMessageType(aMessage->type()),
                (int)aMessage->routing_id());
      if (!aProtocol->GetIPCChannel()->Send(aMessage)) {
        MOZ_RELEASE_ASSERT(aProtocol->mSide == ipc::ParentSide);
        BeginShutdown();
      }
    } else {
      delete aMessage;
    }
  }

  virtual Result OnMessageReceived(const Message& aMessage) override {
    // If we do not have a recording process then just see if the message can
    // be handled in the middleman.
    if (!mOppositeMessageLoop) {
      MOZ_RELEASE_ASSERT(mSide == ipc::ChildSide);
      HandleMessageInMiddleman(mSide, aMessage);
      return MsgProcessed;
    }

    // Copy the message first, since HandleMessageInMiddleman may destructively
    // modify it through OnMessageReceived calls.
    Message* nMessage = new Message();
    nMessage->CopyFrom(aMessage);

    if (HandleMessageInMiddleman(mSide, aMessage)) {
      delete nMessage;
      return MsgProcessed;
    }

    mOppositeMessageLoop->PostTask(NewRunnableFunction(
        "ForwardMessageAsync", ForwardMessageAsync, mOpposite, nMessage));
    return MsgProcessed;
  }

  static void ForwardMessageSync(MiddlemanProtocol* aProtocol,
                                 Message* aMessage, Message** aReply) {
    PrintSpew("ForwardSyncMsg %s\n",
              IPC::StringFromIPCMessageType(aMessage->type()));

    MOZ_RELEASE_ASSERT(!*aReply);
    Message* nReply = new Message();
    if (!aProtocol->GetIPCChannel()->Send(aMessage, nReply)) {
      MOZ_RELEASE_ASSERT(aProtocol->mSide == ipc::ParentSide);
      BeginShutdown();
    }

    MonitorAutoLock lock(*gMonitor);
    *aReply = nReply;
    gMonitor->Notify();
  }

  virtual Result OnMessageReceived(const Message& aMessage,
                                   Message*& aReply) override {
    MOZ_RELEASE_ASSERT(mOppositeMessageLoop);

    Message* nMessage = new Message();
    nMessage->CopyFrom(aMessage);
    mOppositeMessageLoop->PostTask(
        NewRunnableFunction("ForwardMessageSync", ForwardMessageSync, mOpposite,
                            nMessage, &aReply));

    if (mSide == ipc::ChildSide) {
      AutoMarkMainThreadWaitingForIPDLReply blocked;
      ActiveRecordingChild()->WaitUntil([&]() { return !!aReply; });
    } else {
      MonitorAutoLock lock(*gMonitor);
      while (!aReply) {
        gMonitor->Wait();
      }
    }

    PrintSpew("SyncMsgDone\n");
    return MsgProcessed;
  }

  static void ForwardCallMessage(MiddlemanProtocol* aProtocol,
                                 Message* aMessage, Message** aReply) {
    PrintSpew("ForwardSyncCall %s\n",
              IPC::StringFromIPCMessageType(aMessage->type()));

    MOZ_RELEASE_ASSERT(!*aReply);
    Message* nReply = new Message();
    if (!aProtocol->GetIPCChannel()->Call(aMessage, nReply)) {
      MOZ_RELEASE_ASSERT(aProtocol->mSide == ipc::ParentSide);
      BeginShutdown();
    }

    MonitorAutoLock lock(*gMonitor);
    *aReply = nReply;
    gMonitor->Notify();
  }

  virtual Result OnCallReceived(const Message& aMessage,
                                Message*& aReply) override {
    MOZ_RELEASE_ASSERT(mOppositeMessageLoop);

    Message* nMessage = new Message();
    nMessage->CopyFrom(aMessage);
    mOppositeMessageLoop->PostTask(
        NewRunnableFunction("ForwardCallMessage", ForwardCallMessage, mOpposite,
                            nMessage, &aReply));

    if (mSide == ipc::ChildSide) {
      AutoMarkMainThreadWaitingForIPDLReply blocked;
      ActiveRecordingChild()->WaitUntil([&]() { return !!aReply; });
    } else {
      MonitorAutoLock lock(*gMonitor);
      while (!aReply) {
        gMonitor->Wait();
      }
    }

    PrintSpew("SyncCallDone\n");
    return MsgProcessed;
  }

  virtual int32_t GetProtocolTypeId() override {
    MOZ_CRASH("MiddlemanProtocol::GetProtocolTypeId");
  }

  virtual void OnChannelClose() override {
    MOZ_RELEASE_ASSERT(mSide == ipc::ChildSide);
    BeginShutdown();
  }

  virtual void OnChannelError() override { BeginShutdown(); }
};

static MiddlemanProtocol* gChildProtocol;
static MiddlemanProtocol* gParentProtocol;

ipc::MessageChannel* ChannelToUIProcess() {
  return gChildProtocol->GetIPCChannel();
}

// Message loop for forwarding messages between the parent process and a
// recording process.
static MessageLoop* gForwardingMessageLoop;

static bool gParentProtocolOpened = false;

// Main routine for the forwarding message loop thread.
static void ForwardingMessageLoopMain(void*) {
  MOZ_RELEASE_ASSERT(ActiveChildIsRecording());

  MessageLoop messageLoop;
  gForwardingMessageLoop = &messageLoop;

  gChildProtocol->mOppositeMessageLoop = gForwardingMessageLoop;

  gParentProtocol->Open(
      gRecordingProcess->GetChannel(),
      base::GetProcId(gRecordingProcess->GetChildProcessHandle()));

  // Notify the main thread that we have finished initialization.
  {
    MonitorAutoLock lock(*gMonitor);
    gParentProtocolOpened = true;
    gMonitor->Notify();
  }

  messageLoop.Run();
}

void InitializeForwarding() {
  gChildProtocol = new MiddlemanProtocol(ipc::ChildSide);

  if (gProcessKind == ProcessKind::MiddlemanRecording) {
    gParentProtocol = new MiddlemanProtocol(ipc::ParentSide);
    gParentProtocol->mOpposite = gChildProtocol;
    gChildProtocol->mOpposite = gParentProtocol;

    gParentProtocol->mOppositeMessageLoop = MainThreadMessageLoop();

    if (!PR_CreateThread(PR_USER_THREAD, ForwardingMessageLoopMain, nullptr,
                         PR_PRIORITY_NORMAL, PR_GLOBAL_THREAD,
                         PR_JOINABLE_THREAD, 0)) {
      MOZ_CRASH("parent::Initialize");
    }

    // Wait for the forwarding message loop thread to finish initialization.
    MonitorAutoLock lock(*gMonitor);
    while (!gParentProtocolOpened) {
      gMonitor->Wait();
    }
  }
}

}  // namespace parent
}  // namespace recordreplay
}  // namespace mozilla
