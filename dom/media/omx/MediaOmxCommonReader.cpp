/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaOmxCommonReader.h"

#include <stagefright/MediaSource.h>

#include "AbstractMediaDecoder.h"
#include "AudioChannelService.h"
#include "MediaStreamSource.h"

#ifdef MOZ_AUDIO_OFFLOAD
#include <stagefright/Utils.h>
#include <cutils/properties.h>
#include <stagefright/MetaData.h>
#endif

using namespace android;

namespace mozilla {

extern PRLogModuleInfo* gMediaDecoderLog;
#define DECODER_LOG(type, msg) MOZ_LOG(gMediaDecoderLog, type, msg)

MediaOmxCommonReader::MediaOmxCommonReader(AbstractMediaDecoder *aDecoder)
  : MediaDecoderReader(aDecoder)
  , mStreamSource(nullptr)
{
  if (!gMediaDecoderLog) {
    gMediaDecoderLog = PR_NewLogModule("MediaDecoder");
  }

  mAudioChannel = dom::AudioChannelService::GetDefaultAudioChannel();
}

#ifdef MOZ_AUDIO_OFFLOAD
void MediaOmxCommonReader::CheckAudioOffload()
{
  MOZ_ASSERT(OnTaskQueue());

  char offloadProp[128];
  property_get("audio.offload.disable", offloadProp, "0");
  bool offloadDisable =  atoi(offloadProp) != 0;
  if (offloadDisable) {
    return;
  }

  sp<MediaSource> audioOffloadTrack = GetAudioOffloadTrack();
  sp<MetaData> meta = audioOffloadTrack.get()
      ? audioOffloadTrack->getFormat() : nullptr;

  // Supporting audio offload only when there is no video, no streaming
  bool hasNoVideo = !HasVideo();
  bool isNotStreaming
      = mDecoder->GetResource()->IsDataCachedToEndOfResource(0);

  // Not much benefit in trying to offload other channel types. Most of them
  // aren't supported and also duration would be less than a minute
  bool isTypeMusic = mAudioChannel == dom::AudioChannel::Content;

  DECODER_LOG(LogLevel::Debug, ("%s meta %p, no video %d, no streaming %d,"
      " channel type %d", __FUNCTION__, meta.get(), hasNoVideo,
      isNotStreaming, mAudioChannel));

  if ((meta.get()) && hasNoVideo && isNotStreaming && isTypeMusic &&
      canOffloadStream(meta, false, false, AUDIO_STREAM_MUSIC)) {
    DECODER_LOG(LogLevel::Debug, ("Can offload this audio stream"));
    mDecoder->SetPlatformCanOffloadAudio(true);
  }
}
#endif

} // namespace mozilla
