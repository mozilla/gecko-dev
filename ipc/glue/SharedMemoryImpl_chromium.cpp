/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ipc/SharedMemoryImpl_chromium.h"

namespace mozilla::ipc {

SharedMemoryImpl::Handle SharedMemoryImpl::CloneHandle() {
  return mSharedMemory.CloneHandle();
}

SharedMemoryImpl::Handle SharedMemoryImpl::TakeHandle() {
  return mSharedMemory.TakeHandle(false);
}

bool SharedMemoryImpl::IsHandleValid(const Handle& aHandle) const {
  return base::SharedMemory::IsHandleValid(aHandle);
}

bool SharedMemoryImpl::SetHandle(Handle aHandle, OpenRights aRights) {
  return mSharedMemory.SetHandle(std::move(aHandle), aRights == RightsReadOnly);
}

SharedMemoryImpl::Handle SharedMemoryImpl::NULLHandle() {
  return base::SharedMemory::NULLHandle();
}

void* SharedMemoryImpl::FindFreeAddressSpace(size_t size) {
  return base::SharedMemory::FindFreeAddressSpace(size);
}

bool SharedMemoryImpl::CreateImpl(size_t size) {
  return mSharedMemory.Create(size);
}

bool SharedMemoryImpl::MapImpl(size_t nBytes, void* fixedAddress) {
  return mSharedMemory.Map(nBytes, fixedAddress);
}

void SharedMemoryImpl::UnmapImpl(size_t mappedSize) { mSharedMemory.Unmap(); }

void* SharedMemoryImpl::MemoryImpl() const { return mSharedMemory.memory(); }

}  // namespace mozilla::ipc
