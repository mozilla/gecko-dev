/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __FFmpegAACDecoder_h__
#define __FFmpegAACDecoder_h__

#include "FFmpegDataDecoder.h"

namespace mozilla
{

template <int V> class FFmpegAudioDecoder
{
};

template <>
class FFmpegAudioDecoder<LIBAV_VER> : public FFmpegDataDecoder<LIBAV_VER>
{
public:
  FFmpegAudioDecoder(FlushableMediaTaskQueue* aTaskQueue,
                     MediaDataDecoderCallback* aCallback,
                     const AudioInfo& aConfig);
  virtual ~FFmpegAudioDecoder();

  virtual nsresult Init() override;
  virtual nsresult Input(MediaRawData* aSample) override;
  virtual nsresult Drain() override;
  static AVCodecID GetCodecId(const nsACString& aMimeType);

private:
  void DecodePacket(MediaRawData* aSample);

  MediaDataDecoderCallback* mCallback;
};

} // namespace mozilla

#endif // __FFmpegAACDecoder_h__
