/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ContentBridgeChild.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/StructuredCloneUtils.h"
#include "mozilla/dom/TabChild.h"
#include "mozilla/dom/ipc/Blob.h"
#include "mozilla/dom/ipc/nsIRemoteBlob.h"
#include "mozilla/ipc/InputStreamUtils.h"
#include "nsDOMFile.h"
#include "JavaScriptChild.h"

using namespace base;
using namespace mozilla::ipc;
using namespace mozilla::jsipc;

namespace mozilla {
namespace dom {

NS_IMPL_ISUPPORTS(ContentBridgeChild, nsIContentChild)

ContentBridgeChild::ContentBridgeChild(Transport* aTransport)
  : mTransport(aTransport)
{}

ContentBridgeChild::~ContentBridgeChild()
{
  XRE_GetIOMessageLoop()->PostTask(FROM_HERE, new DeleteTask<Transport>(mTransport));
}

void
ContentBridgeChild::ActorDestroy(ActorDestroyReason aWhy)
{
  MessageLoop::current()->PostTask(
    FROM_HERE,
    NewRunnableMethod(this, &ContentBridgeChild::DeferredDestroy));
}

/*static*/ ContentBridgeChild*
ContentBridgeChild::Create(Transport* aTransport, ProcessId aOtherProcess)
{
  nsRefPtr<ContentBridgeChild> bridge =
    new ContentBridgeChild(aTransport);
  ProcessHandle handle;
  if (!base::OpenProcessHandle(aOtherProcess, &handle)) {
    // XXX need to kill |aOtherProcess|, it's boned
    return nullptr;
  }
  bridge->mSelfRef = bridge;

  DebugOnly<bool> ok = bridge->Open(aTransport, handle, XRE_GetIOMessageLoop());
  MOZ_ASSERT(ok);
  return bridge;
}

void
ContentBridgeChild::DeferredDestroy()
{
  mSelfRef = nullptr;
  // |this| was just destroyed, hands off
}

bool
ContentBridgeChild::RecvAsyncMessage(const nsString& aMsg,
                                     const ClonedMessageData& aData,
                                     const InfallibleTArray<jsipc::CpowEntry>& aCpows,
                                     const IPC::Principal& aPrincipal)
{
  return nsIContentChild::RecvAsyncMessage(aMsg, aData, aCpows, aPrincipal);
}

PBlobChild*
ContentBridgeChild::SendPBlobConstructor(PBlobChild* actor,
                                         const BlobConstructorParams& params)
{
  return PContentBridgeChild::SendPBlobConstructor(actor, params);
}

bool
ContentBridgeChild::SendPBrowserConstructor(PBrowserChild* aActor,
                                            const IPCTabContext& aContext,
                                            const uint32_t& aChromeFlags,
                                            const uint64_t& aID,
                                            const bool& aIsForApp,
                                            const bool& aIsForBrowser)
{
  return PContentBridgeChild::SendPBrowserConstructor(aActor,
                                                      aContext,
                                                      aChromeFlags,
                                                      aID,
                                                      aIsForApp,
                                                      aIsForBrowser);
}

// This implementation is identical to ContentChild::GetCPOWManager but we can't
// move it to nsIContentChild because it calls ManagedPJavaScriptChild() which
// only exists in PContentChild and PContentBridgeChild.
jsipc::JavaScriptChild *
ContentBridgeChild::GetCPOWManager()
{
  if (ManagedPJavaScriptChild().Length()) {
    return static_cast<JavaScriptChild*>(ManagedPJavaScriptChild()[0]);
  }
  JavaScriptChild* actor = static_cast<JavaScriptChild*>(SendPJavaScriptConstructor());
  return actor;
}

mozilla::jsipc::PJavaScriptChild *
ContentBridgeChild::AllocPJavaScriptChild()
{
  return nsIContentChild::AllocPJavaScriptChild();
}

bool
ContentBridgeChild::DeallocPJavaScriptChild(PJavaScriptChild *child)
{
  return nsIContentChild::DeallocPJavaScriptChild(child);
}

PBrowserChild*
ContentBridgeChild::AllocPBrowserChild(const IPCTabContext &aContext,
                                       const uint32_t& aChromeFlags,
                                       const uint64_t& aID,
                                       const bool& aIsForApp,
                                       const bool& aIsForBrowser)
{
  return nsIContentChild::AllocPBrowserChild(aContext,
                                             aChromeFlags,
                                             aID,
                                             aIsForApp,
                                             aIsForBrowser);
}

bool
ContentBridgeChild::DeallocPBrowserChild(PBrowserChild* aChild)
{
  return nsIContentChild::DeallocPBrowserChild(aChild);
}

bool
ContentBridgeChild::RecvPBrowserConstructor(PBrowserChild* aActor,
                                            const IPCTabContext& aContext,
                                            const uint32_t& aChromeFlags,
                                            const uint64_t& aID,
                                            const bool& aIsForApp,
                                            const bool& aIsForBrowser)
{
  return ContentChild::GetSingleton()->RecvPBrowserConstructor(aActor,
                                                               aContext,
                                                               aChromeFlags,
                                                               aID,
                                                               aIsForApp,
                                                               aIsForBrowser);
}

PBlobChild*
ContentBridgeChild::AllocPBlobChild(const BlobConstructorParams& aParams)
{
  return nsIContentChild::AllocPBlobChild(aParams);
}

bool
ContentBridgeChild::DeallocPBlobChild(PBlobChild* aActor)
{
  return nsIContentChild::DeallocPBlobChild(aActor);
}

} // namespace dom
} // namespace mozilla
