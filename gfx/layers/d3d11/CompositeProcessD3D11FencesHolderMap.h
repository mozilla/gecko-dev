/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_CompositeProcessD3D11FencesHolderMap_H
#define MOZILLA_GFX_CompositeProcessD3D11FencesHolderMap_H

#include <d3d11.h>
#include <vector>

#include "mozilla/layers/LayersTypes.h"
#include "mozilla/Maybe.h"
#include "mozilla/Monitor.h"
#include "mozilla/StaticPtr.h"

namespace mozilla {
namespace layers {

class FenceD3D11;

/**
 * A class to manage FenceD3D11 that is shared in GPU process.
 */
class CompositeProcessD3D11FencesHolderMap {
 public:
  static void Init();
  static void Shutdown();
  static CompositeProcessD3D11FencesHolderMap* Get() { return sInstance; }

  CompositeProcessD3D11FencesHolderMap();
  ~CompositeProcessD3D11FencesHolderMap();

  void Register(CompositeProcessFencesHolderId aHolderId);
  void RegisterReference(CompositeProcessFencesHolderId aHolderId);
  void Unregister(CompositeProcessFencesHolderId aHolderId);

  void SetWriteFence(CompositeProcessFencesHolderId aHolderId,
                     RefPtr<FenceD3D11> aWriteFence);
  void SetReadFence(CompositeProcessFencesHolderId aHolderId,
                    RefPtr<FenceD3D11> aReadFence);

  bool WaitWriteFence(CompositeProcessFencesHolderId aHolderId,
                      ID3D11Device* aDevice);
  bool WaitAllFencesAndForget(CompositeProcessFencesHolderId aHolderId,
                              ID3D11Device* aDevice);

 private:
  struct FencesHolder {
    FencesHolder() = default;

    RefPtr<FenceD3D11> mWriteFence;
    std::vector<RefPtr<FenceD3D11>> mReadFences;
    uint32_t mOwners = 1;
  };

  mutable Monitor mMonitor;

  std::unordered_map<CompositeProcessFencesHolderId, UniquePtr<FencesHolder>,
                     CompositeProcessFencesHolderId::HashFn>
      mFencesHolderById MOZ_GUARDED_BY(mMonitor);

  static StaticAutoPtr<CompositeProcessD3D11FencesHolderMap> sInstance;
};

}  // namespace layers
}  // namespace mozilla

#endif /* MOZILLA_GFX_CompositeProcessD3D11FencesHolderMap_H */
