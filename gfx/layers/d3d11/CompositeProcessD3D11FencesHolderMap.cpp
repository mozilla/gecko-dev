/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CompositeProcessD3D11FencesHolderMap.h"

#include "mozilla/layers/FenceD3D11.h"

namespace mozilla {

namespace layers {

StaticAutoPtr<CompositeProcessD3D11FencesHolderMap>
    CompositeProcessD3D11FencesHolderMap::sInstance;

/* static */
void CompositeProcessD3D11FencesHolderMap::Init() {
  MOZ_ASSERT(XRE_IsGPUProcess() || XRE_IsParentProcess());
  sInstance = new CompositeProcessD3D11FencesHolderMap();
}

/* static */
void CompositeProcessD3D11FencesHolderMap::Shutdown() {
  MOZ_ASSERT(XRE_IsGPUProcess() || XRE_IsParentProcess());
  sInstance = nullptr;
}

CompositeProcessD3D11FencesHolderMap::CompositeProcessD3D11FencesHolderMap()
    : mMonitor("CompositeProcessD3D11FencesHolderMap::mMonitor") {}

CompositeProcessD3D11FencesHolderMap::~CompositeProcessD3D11FencesHolderMap() {}

void CompositeProcessD3D11FencesHolderMap::Register(
    CompositeProcessFencesHolderId aHolderId) {
  MOZ_ASSERT(aHolderId.IsValid());

  MonitorAutoLock lock(mMonitor);

  DebugOnly<bool> inserted =
      mFencesHolderById.emplace(aHolderId, MakeUnique<FencesHolder>()).second;
  MOZ_ASSERT(inserted, "Map already contained FencesHolder for id!");
}

void CompositeProcessD3D11FencesHolderMap::RegisterReference(
    CompositeProcessFencesHolderId aHolderId) {
  if (!aHolderId.IsValid()) {
    return;
  }

  MonitorAutoLock lock(mMonitor);

  auto it = mFencesHolderById.find(aHolderId);
  if (it == mFencesHolderById.end()) {
    MOZ_ASSERT_UNREACHABLE("Map missing FencesHolder for id!");
    return;
  }

  MOZ_ASSERT(it->second->mOwners > 0);
  ++it->second->mOwners;
}

void CompositeProcessD3D11FencesHolderMap::Unregister(
    CompositeProcessFencesHolderId aHolderId) {
  if (!aHolderId.IsValid()) {
    return;
  }

  MonitorAutoLock lock(mMonitor);

  auto it = mFencesHolderById.find(aHolderId);
  if (it == mFencesHolderById.end()) {
    MOZ_ASSERT_UNREACHABLE("Map missing FencesHolder for id!");
    return;
  }

  MOZ_ASSERT(it->second->mOwners > 0);
  if (--it->second->mOwners == 0) {
    mFencesHolderById.erase(it);
  }
}

void CompositeProcessD3D11FencesHolderMap::SetWriteFence(
    CompositeProcessFencesHolderId aHolderId, RefPtr<FenceD3D11> aWriteFence) {
  MOZ_ASSERT(aWriteFence);

  if (!aWriteFence) {
    return;
  }

  MonitorAutoLock lock(mMonitor);

  MOZ_ASSERT(aHolderId.IsValid());
  auto it = mFencesHolderById.find(aHolderId);
  if (it == mFencesHolderById.end()) {
    MOZ_ASSERT_UNREACHABLE("unexpected to be called");
    return;
  }

  RefPtr<FenceD3D11> fence = aWriteFence->CloneFromHandle();
  if (!fence) {
    MOZ_ASSERT_UNREACHABLE("unexpected to be called");
    return;
  }

  MOZ_ASSERT(!it->second->mWriteFence);
  MOZ_ASSERT(it->second->mReadFences.empty());

  it->second->mWriteFence = fence;
}

void CompositeProcessD3D11FencesHolderMap::SetReadFence(
    CompositeProcessFencesHolderId aHolderId, RefPtr<FenceD3D11> aReadFence) {
  MOZ_ASSERT(aReadFence);

  if (!aReadFence) {
    return;
  }

  MonitorAutoLock lock(mMonitor);

  MOZ_ASSERT(aHolderId.IsValid());
  auto it = mFencesHolderById.find(aHolderId);
  if (it == mFencesHolderById.end()) {
    MOZ_ASSERT_UNREACHABLE("unexpected to be called");
    return;
  }

  RefPtr<FenceD3D11> fence = aReadFence->CloneFromHandle();
  if (!fence) {
    MOZ_ASSERT_UNREACHABLE("unexpected to be called");
    return;
  }

  it->second->mReadFences.push_back(fence);
}

bool CompositeProcessD3D11FencesHolderMap::WaitWriteFence(
    CompositeProcessFencesHolderId aHolderId, ID3D11Device* aDevice) {
  MOZ_ASSERT(aDevice);

  if (!aDevice) {
    return false;
  }

  RefPtr<FenceD3D11> writeFence;
  {
    MonitorAutoLock lock(mMonitor);

    MOZ_ASSERT(aHolderId.IsValid());
    auto it = mFencesHolderById.find(aHolderId);
    if (it == mFencesHolderById.end()) {
      MOZ_ASSERT_UNREACHABLE("unexpected to be called");
      return false;
    }
    writeFence = it->second->mWriteFence;
  }

  if (!writeFence) {
    return true;
  }

  return writeFence->Wait(aDevice);
}

bool CompositeProcessD3D11FencesHolderMap::WaitAllFencesAndForget(
    CompositeProcessFencesHolderId aHolderId, ID3D11Device* aDevice) {
  MOZ_ASSERT(aDevice);

  if (!aDevice) {
    return false;
  }

  RefPtr<FenceD3D11> writeFence;
  std::vector<RefPtr<FenceD3D11>> readFences;
  {
    MonitorAutoLock lock(mMonitor);

    MOZ_ASSERT(aHolderId.IsValid());
    auto it = mFencesHolderById.find(aHolderId);
    if (it == mFencesHolderById.end()) {
      MOZ_ASSERT_UNREACHABLE("unexpected to be called");
      return false;
    }
    writeFence = it->second->mWriteFence.forget();
    readFences.swap(it->second->mReadFences);

    MOZ_ASSERT(!it->second->mWriteFence);
    MOZ_ASSERT(it->second->mReadFences.empty());
  }

  if (writeFence) {
    writeFence->Wait(aDevice);
  }

  for (auto& fence : readFences) {
    fence->Wait(aDevice);
  }

  return true;
}

}  // namespace layers
}  // namespace mozilla
