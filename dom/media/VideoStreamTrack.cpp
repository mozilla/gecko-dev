/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VideoStreamTrack.h"

#include "MediaStreamVideoSink.h"
#include "MediaStreamGraph.h"

#include "mozilla/dom/VideoStreamTrackBinding.h"

namespace mozilla {
namespace dom {

JSObject*
VideoStreamTrack::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return VideoStreamTrackBinding::Wrap(aCx, this, aGivenProto);
}

void
VideoStreamTrack::AddVideoOutput(MediaStreamVideoSink* aSink)
{
  GetOwnedStream()->AddVideoOutput(aSink, mTrackID);
}

void
VideoStreamTrack::RemoveVideoOutput(MediaStreamVideoSink* aSink)
{
  GetOwnedStream()->RemoveVideoOutput(aSink, mTrackID);
}

} // namespace dom
} // namespace mozilla
