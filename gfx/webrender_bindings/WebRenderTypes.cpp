/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebRenderTypes.h"

#include "mozilla/ipc/ByteBuf.h"

namespace mozilla {
namespace wr {

WindowId NewWindowId() {
  static uint64_t sNextId = 1;

  WindowId id;
  id.mHandle = sNextId++;
  return id;
}

void Assign_WrVecU8(wr::WrVecU8& aVec, mozilla::ipc::ByteBuf&& aOther) {
  aVec.data = aOther.mData;
  aVec.length = aOther.mLen;
  aVec.capacity = aOther.mCapacity;
  aOther.mData = nullptr;
  aOther.mLen = 0;
  aOther.mCapacity = 0;
}

/*static*/ WrClipId RootScrollNode() {
  return WrClipId{wr_root_scroll_node_id()};
}

}  // namespace wr
}  // namespace mozilla
