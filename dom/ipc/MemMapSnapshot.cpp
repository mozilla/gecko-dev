/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MemMapSnapshot.h"

#include "nsDebug.h"
#include "mozilla/ResultExtensions.h"
#include "mozilla/ipc/SharedMemoryHandle.h"

namespace mozilla::ipc {

Result<Ok, nsresult> MemMapSnapshot::Init(size_t aSize) {
  MOZ_ASSERT(!mMem);

  auto handle = shared_memory::CreateFreezable(aSize);
  if (NS_WARN_IF(!handle)) {
    return Err(NS_ERROR_FAILURE);
  }

  auto mem = std::move(handle).Map();
  if (NS_WARN_IF(!mem)) {
    return Err(NS_ERROR_FAILURE);
  }

  mMem = std::move(mem);
  return Ok();
}

Result<ReadOnlySharedMemoryHandle, nsresult> MemMapSnapshot::Finalize() {
  MOZ_ASSERT(mMem);

  auto readOnlyHandle = std::move(mMem).Freeze();
  if (NS_WARN_IF(!readOnlyHandle)) {
    return Err(NS_ERROR_FAILURE);
  }

  return std::move(readOnlyHandle);
}

}  // namespace mozilla::ipc
