/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MemMapSnapshot.h"

#include "mozilla/ResultExtensions.h"

namespace mozilla::ipc {

Result<Ok, nsresult> MemMapSnapshot::Init(size_t aSize) {
  MOZ_ASSERT(!mMem);

  auto aMem = MakeRefPtr<SharedMemory>();
  if (NS_WARN_IF(!aMem->CreateFreezable(aSize))) {
    return Err(NS_ERROR_FAILURE);
  }
  if (NS_WARN_IF(!aMem->Map(aSize))) {
    return Err(NS_ERROR_FAILURE);
  }

  mMem = std::move(aMem);
  return Ok();
}

Result<Ok, nsresult> MemMapSnapshot::Finalize(RefPtr<SharedMemory>& aMem) {
  MOZ_ASSERT(mMem);

  auto size = mMem->Size();
  if (NS_WARN_IF(!mMem->Freeze())) {
    return Err(NS_ERROR_FAILURE);
  }

  aMem = std::move(mMem);

  // We need to re-map the memory as `Freeze()` unmaps it.
  if (NS_WARN_IF(!aMem->Map(size))) {
    return Err(NS_ERROR_FAILURE);
  }

  return Ok();
}

}  // namespace mozilla::ipc
