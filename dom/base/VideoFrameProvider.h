/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_VideoFrameProvider_h
#define mozilla_dom_VideoFrameProvider_h

#include "mozilla/dom/VideoFrameBinding.h"
#include "mozilla/dom/HTMLVideoElementBinding.h"
#include "mozilla/dom/RequestCallbackManager.h"

namespace mozilla::dom {

using VideoFrameRequest = RequestCallbackEntry<VideoFrameRequestCallback>;

using VideoFrameRequestManager =
    RequestCallbackManager<VideoFrameRequestCallback>;

// Force instantiation.
template void ImplCycleCollectionUnlink(VideoFrameRequestManager& aField);
template void ImplCycleCollectionTraverse(
    nsCycleCollectionTraversalCallback& aCallback,
    VideoFrameRequestManager& aField, const char* aName, uint32_t aFlags);

}  // namespace mozilla::dom
#endif  // mozilla_dom_VideoFrameProvider_h
