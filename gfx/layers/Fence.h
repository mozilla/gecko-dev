/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_Fence_H
#define MOZILLA_GFX_Fence_H

#include "mozilla/gfx/FileHandleWrapper.h"
#include "nsISupportsImpl.h"

namespace mozilla {

namespace layers {

class FenceD3D11;
class FenceFileHandle;

class Fence {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(Fence);

  virtual FenceD3D11* AsFenceD3D11() { return nullptr; }
  virtual FenceFileHandle* AsFenceFileHandle() { return nullptr; }

 protected:
  virtual ~Fence() = default;
};

class FenceFileHandle final : public Fence {
 public:
  explicit FenceFileHandle(UniqueFileHandle&& aFileHandle);

  FenceFileHandle* AsFenceFileHandle() override { return this; }

  UniqueFileHandle DuplicateFileHandle();

 protected:
  virtual ~FenceFileHandle();

  UniqueFileHandle mFileHandle;
};

}  // namespace layers
}  // namespace mozilla

#endif  // MOZILLA_GFX_Fence_H
