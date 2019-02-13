/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaStreamAudioDestinationNode.h"
#include "nsIDocument.h"
#include "mozilla/dom/MediaStreamAudioDestinationNodeBinding.h"
#include "AudioNodeEngine.h"
#include "AudioNodeStream.h"
#include "DOMMediaStream.h"
#include "TrackUnionStream.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED(MediaStreamAudioDestinationNode, AudioNode, mDOMStream)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(MediaStreamAudioDestinationNode)
NS_INTERFACE_MAP_END_INHERITING(AudioNode)

NS_IMPL_ADDREF_INHERITED(MediaStreamAudioDestinationNode, AudioNode)
NS_IMPL_RELEASE_INHERITED(MediaStreamAudioDestinationNode, AudioNode)

MediaStreamAudioDestinationNode::MediaStreamAudioDestinationNode(AudioContext* aContext)
  : AudioNode(aContext,
              2,
              ChannelCountMode::Explicit,
              ChannelInterpretation::Speakers)
  , mDOMStream(
      DOMAudioNodeMediaStream::CreateTrackUnionStream(GetOwner(),
                                                      this,
                                                      aContext->Graph()))
{
  // Ensure an audio track with the correct ID is exposed to JS
  mDOMStream->CreateDOMTrack(AudioNodeStream::AUDIO_TRACK, MediaSegment::AUDIO);

  ProcessedMediaStream* outputStream = mDOMStream->GetStream()->AsProcessedStream();
  MOZ_ASSERT(!!outputStream);
  AudioNodeEngine* engine = new AudioNodeEngine(this);
  mStream = aContext->Graph()->CreateAudioNodeStream(engine, MediaStreamGraph::EXTERNAL_STREAM);
  mPort = outputStream->AllocateInputPort(mStream);

  nsIDocument* doc = aContext->GetParentObject()->GetExtantDoc();
  if (doc) {
    mDOMStream->CombineWithPrincipal(doc->NodePrincipal());
  }
}

MediaStreamAudioDestinationNode::~MediaStreamAudioDestinationNode()
{
}

size_t
MediaStreamAudioDestinationNode::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const
{
  // Future:
  // - mDOMStream
  size_t amount = AudioNode::SizeOfExcludingThis(aMallocSizeOf);
  amount += mPort->SizeOfIncludingThis(aMallocSizeOf);
  return amount;
}

size_t
MediaStreamAudioDestinationNode::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
}

void
MediaStreamAudioDestinationNode::DestroyMediaStream()
{
  AudioNode::DestroyMediaStream();
  if (mPort) {
    mPort->Destroy();
    mPort = nullptr;
  }
}

JSObject*
MediaStreamAudioDestinationNode::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return MediaStreamAudioDestinationNodeBinding::Wrap(aCx, this, aGivenProto);
}

}
}
