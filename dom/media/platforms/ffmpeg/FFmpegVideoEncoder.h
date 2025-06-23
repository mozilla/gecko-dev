/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_PLATFORMS_FFMPEG_FFMPEGVIDEOENCODER_H_
#define DOM_MEDIA_PLATFORMS_FFMPEG_FFMPEGVIDEOENCODER_H_

#include "FFmpegDataEncoder.h"
#include "FFmpegLibWrapper.h"
#include "PlatformEncoderModule.h"
#include "SimpleMap.h"

// This must be the last header included
#include "FFmpegLibs.h"

namespace mozilla {

template <int V>
class FFmpegVideoEncoder : public FFmpegDataEncoder<V> {};

template <>
class FFmpegVideoEncoder<LIBAV_VER> : public FFmpegDataEncoder<LIBAV_VER> {
  using PtsMap = SimpleMap<int64_t, int64_t, NoOpPolicy>;

 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(FFmpegVideoEncoder, final);

  FFmpegVideoEncoder(const FFmpegLibWrapper* aLib, AVCodecID aCodecID,
                     const RefPtr<TaskQueue>& aTaskQueue,
                     const EncoderConfig& aConfig);

  RefPtr<InitPromise> Init() override;

  nsCString GetDescriptionName() const override;

 protected:
  virtual ~FFmpegVideoEncoder() = default;
  // Methods only called on mTaskQueue.
  virtual MediaResult InitEncoder() override;
  MediaResult InitEncoderInternal(bool aHardware);
#if LIBAVCODEC_VERSION_MAJOR >= 58
  Result<EncodedData, MediaResult> EncodeInputWithModernAPIs(
      RefPtr<const MediaData> aSample) override;
#endif
  virtual Result<RefPtr<MediaRawData>, MediaResult> ToMediaRawData(
      AVPacket* aPacket) override;
  Result<already_AddRefed<MediaByteBuffer>, MediaResult> GetExtraData(
      AVPacket* aPacket) override;
  struct SVCSettings {
    nsTArray<uint8_t> mTemporalLayerIds;
    // A key-value pair for av_opt_set.
    std::pair<nsCString, nsCString> mSettingKeyValue;
  };
  bool SvcEnabled() const;
  Maybe<SVCSettings> GetSVCSettings();
  struct H264Settings {
    int mProfile;
    int mLevel;
    // A list of key-value pairs for av_opt_set.
    nsTArray<std::pair<nsCString, nsCString>> mSettingKeyValuePairs;
  };
  H264Settings GetH264Settings(const H264Specific& aH264Specific);
  struct SVCInfo {
    explicit SVCInfo(nsTArray<uint8_t>&& aTemporalLayerIds)
        : mTemporalLayerIds(std::move(aTemporalLayerIds)), mCurrentIndex(0) {}
    const nsTArray<uint8_t> mTemporalLayerIds;
    size_t mCurrentIndex;
    void UpdateTemporalLayerId();
    void ResetTemporalLayerId();
    uint8_t CurrentTemporalLayerId();
  };
  Maybe<SVCInfo> mSVCInfo{};
  // Some codecs use the input frames pts for rate control. We'd rather only use
  // the duration. Synthetize fake pts based on integrating over the duration of
  // input frames.
  int64_t mFakePts = 0;
  int64_t mCurrentFramePts = 0;
  PtsMap mPtsMap;
};

}  // namespace mozilla

#endif  // DOM_MEDIA_PLATFORMS_FFMPEG_FFMPEGVIDEOENCODER_H_
