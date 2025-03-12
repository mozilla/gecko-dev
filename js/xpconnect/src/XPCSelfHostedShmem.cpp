/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "XPCSelfHostedShmem.h"
#include "xpcprivate.h"

// static
mozilla::StaticRefPtr<xpc::SelfHostedShmem>
    xpc::SelfHostedShmem::sSelfHostedXdr;

NS_IMPL_ISUPPORTS(xpc::SelfHostedShmem, nsIMemoryReporter)

// static
xpc::SelfHostedShmem& xpc::SelfHostedShmem::GetSingleton() {
  MOZ_ASSERT_IF(!sSelfHostedXdr, NS_IsMainThread());

  if (!sSelfHostedXdr) {
    sSelfHostedXdr = new SelfHostedShmem;
  }

  return *sSelfHostedXdr;
}

void xpc::SelfHostedShmem::InitMemoryReporter() {
  mozilla::RegisterWeakMemoryReporter(this);
}

// static
void xpc::SelfHostedShmem::Shutdown() {
  MOZ_ASSERT(NS_IsMainThread());
  // NOTE: We cannot call UnregisterWeakMemoryReporter(sSelfHostedXdr) as the
  // service is shutdown at the time this call is made. In any cases, this would
  // already be done when the memory reporter got destroyed.
  sSelfHostedXdr = nullptr;
}

void xpc::SelfHostedShmem::InitFromParent(ContentType aXdr) {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mHandle && !mMem, "Shouldn't call this more than once");

  size_t len = aXdr.Length();
  auto handle = mozilla::ipc::shared_memory::CreateFreezable(len);
  if (NS_WARN_IF(!handle)) {
    return;
  }

  auto mapping = std::move(handle).Map();
  if (NS_WARN_IF(!mapping)) {
    return;
  }

  void* address = mapping.Address();
  memcpy(address, aXdr.Elements(), aXdr.LengthBytes());

  mHandle = std::move(mapping).Freeze();
  mMem = mHandle.Map();
}

bool xpc::SelfHostedShmem::InitFromChild(
    mozilla::ipc::ReadOnlySharedMemoryHandle aHandle) {
  MOZ_ASSERT(!XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mMem, "Shouldn't call this more than once");

  mMem = aHandle.Map();
  return mMem.IsValid();
}

xpc::SelfHostedShmem::ContentType xpc::SelfHostedShmem::Content() const {
  if (!mMem) {
    return ContentType();
  }
  return mMem.DataAsSpan<uint8_t>();
}

const mozilla::ipc::ReadOnlySharedMemoryHandle& xpc::SelfHostedShmem::Handle()
    const {
  return mHandle;
}

NS_IMETHODIMP
xpc::SelfHostedShmem::CollectReports(nsIHandleReportCallback* aHandleReport,
                                     nsISupports* aData, bool aAnonymize) {
  // If this is the parent process, then we have a handle and this instance owns
  // the data and shares it with other processes, otherwise this is shared data.
  if (XRE_IsParentProcess()) {
    // This does not exactly report the amount of data mapped by the system,
    // but the space requested when creating the handle.
    MOZ_COLLECT_REPORT("explicit/js-non-window/shared-memory/self-hosted-xdr",
                       KIND_NONHEAP, UNITS_BYTES, mMem.Size(),
                       "Memory used to initialize the JS engine with the "
                       "self-hosted code encoded by the parent process.");
  }
  return NS_OK;
}
