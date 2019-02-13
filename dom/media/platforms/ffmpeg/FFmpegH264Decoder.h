/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __FFmpegH264Decoder_h__
#define __FFmpegH264Decoder_h__

#include "FFmpegDataDecoder.h"

namespace mozilla
{

template <int V>
class FFmpegH264Decoder : public FFmpegDataDecoder<V>
{
};

template <>
class FFmpegH264Decoder<LIBAV_VER> : public FFmpegDataDecoder<LIBAV_VER>
{
  typedef mozilla::layers::Image Image;
  typedef mozilla::layers::ImageContainer ImageContainer;

  enum DecodeResult {
    DECODE_FRAME,
    DECODE_NO_FRAME,
    DECODE_ERROR
  };

public:
  FFmpegH264Decoder(FlushableMediaTaskQueue* aTaskQueue,
                    MediaDataDecoderCallback* aCallback,
                    const VideoInfo& aConfig,
                    ImageContainer* aImageContainer);
  virtual ~FFmpegH264Decoder();

  virtual nsresult Init() override;
  virtual nsresult Input(MediaRawData* aSample) override;
  virtual nsresult Drain() override;
  virtual nsresult Flush() override;
  static AVCodecID GetCodecId(const nsACString& aMimeType);

private:
  void DecodeFrame(MediaRawData* aSample);
  DecodeResult DoDecodeFrame(MediaRawData* aSample);
  void DoDrain();
  void OutputDelayedFrames();

  /**
   * This method allocates a buffer for FFmpeg's decoder, wrapped in an Image.
   * Currently it only supports Planar YUV420, which appears to be the only
   * non-hardware accelerated image format that FFmpeg's H264 decoder is
   * capable of outputting.
   */
  int AllocateYUV420PVideoBuffer(AVCodecContext* aCodecContext,
                                 AVFrame* aFrame);

  static int AllocateBufferCb(AVCodecContext* aCodecContext, AVFrame* aFrame);
  static void ReleaseBufferCb(AVCodecContext* aCodecContext, AVFrame* aFrame);
  int64_t GetPts(const AVPacket& packet);

  MediaDataDecoderCallback* mCallback;
  nsRefPtr<ImageContainer> mImageContainer;
  uint32_t mDisplayWidth;
  uint32_t mDisplayHeight;
};

} // namespace mozilla

#endif // __FFmpegH264Decoder_h__
