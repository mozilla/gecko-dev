/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <algorithm>
#include <limits>
#include <stdint.h>

#include "MP4Demuxer.h"

#include "AnnexB.h"
#include "BufferStream.h"
#include "H264.h"
#include "Index.h"
#include "MP4Metadata.h"
#include "MoofParser.h"
#include "ResourceStream.h"
#include "mozilla/StaticPrefs.h"
#include "mozilla/Telemetry.h"
#include "nsAutoPtr.h"
#include "nsPrintfCString.h"

extern mozilla::LazyLogModule gMediaDemuxerLog;
mozilla::LogModule* GetDemuxerLog() { return gMediaDemuxerLog; }

#define LOG(arg, ...)                                                 \
  DDMOZ_LOG(gMediaDemuxerLog, mozilla::LogLevel::Debug, "::%s: " arg, \
            __func__, ##__VA_ARGS__)

namespace mozilla {

DDLoggedTypeDeclNameAndBase(MP4TrackDemuxer, MediaTrackDemuxer);

class MP4TrackDemuxer : public MediaTrackDemuxer,
                        public DecoderDoctorLifeLogger<MP4TrackDemuxer> {
 public:
  MP4TrackDemuxer(MediaResource* aResource, UniquePtr<TrackInfo>&& aInfo,
                  const IndiceWrapper& aIndices);

  UniquePtr<TrackInfo> GetInfo() const override;

  RefPtr<SeekPromise> Seek(const media::TimeUnit& aTime) override;

  RefPtr<SamplesPromise> GetSamples(int32_t aNumSamples = 1) override;

  void Reset() override;

  nsresult GetNextRandomAccessPoint(media::TimeUnit* aTime) override;

  RefPtr<SkipAccessPointPromise> SkipToNextRandomAccessPoint(
      const media::TimeUnit& aTimeThreshold) override;

  media::TimeIntervals GetBuffered() override;

  void NotifyDataRemoved();
  void NotifyDataArrived();

 private:
  already_AddRefed<MediaRawData> GetNextSample();
  void EnsureUpToDateIndex();
  void SetNextKeyFrameTime();
  RefPtr<MediaResource> mResource;
  RefPtr<ResourceStream> mStream;
  UniquePtr<TrackInfo> mInfo;
  RefPtr<Index> mIndex;
  UniquePtr<SampleIterator> mIterator;
  Maybe<media::TimeUnit> mNextKeyframeTime;
  // Queued samples extracted by the demuxer, but not yet returned.
  RefPtr<MediaRawData> mQueuedSample;
  bool mNeedReIndex;
  bool mNeedSPSForTelemetry;
  bool mIsH264 = false;
};

// Returns true if no SPS was found and search for it should continue.
bool AccumulateSPSTelemetry(const MediaByteBuffer* aExtradata) {
  SPSData spsdata;
  if (H264::DecodeSPSFromExtraData(aExtradata, spsdata)) {
    uint8_t constraints = (spsdata.constraint_set0_flag ? (1 << 0) : 0) |
                          (spsdata.constraint_set1_flag ? (1 << 1) : 0) |
                          (spsdata.constraint_set2_flag ? (1 << 2) : 0) |
                          (spsdata.constraint_set3_flag ? (1 << 3) : 0) |
                          (spsdata.constraint_set4_flag ? (1 << 4) : 0) |
                          (spsdata.constraint_set5_flag ? (1 << 5) : 0);
    Telemetry::Accumulate(Telemetry::VIDEO_DECODED_H264_SPS_CONSTRAINT_SET_FLAG,
                          constraints);

    // Collect profile_idc values up to 244, otherwise 0 for unknown.
    Telemetry::Accumulate(Telemetry::VIDEO_DECODED_H264_SPS_PROFILE,
                          spsdata.profile_idc <= 244 ? spsdata.profile_idc : 0);

    // Make sure level_idc represents a value between levels 1 and 5.2,
    // otherwise collect 0 for unknown level.
    Telemetry::Accumulate(Telemetry::VIDEO_DECODED_H264_SPS_LEVEL,
                          (spsdata.level_idc >= 10 && spsdata.level_idc <= 52)
                              ? spsdata.level_idc
                              : 0);

    // max_num_ref_frames should be between 0 and 16, anything larger will
    // be treated as invalid.
    Telemetry::Accumulate(Telemetry::VIDEO_H264_SPS_MAX_NUM_REF_FRAMES,
                          std::min(spsdata.max_num_ref_frames, 17u));

    return false;
  }

  return true;
}

MP4Demuxer::MP4Demuxer(MediaResource* aResource)
    : mResource(aResource),
      mStream(new ResourceStream(aResource)),
      mIsSeekable(false) {
  DDLINKCHILD("resource", aResource);
  DDLINKCHILD("stream", mStream.get());
}

RefPtr<MP4Demuxer::InitPromise> MP4Demuxer::Init() {
  AutoPinned<ResourceStream> stream(mStream);

  // 'result' will capture the first warning, if any.
  MediaResult result{NS_OK};

  MP4Metadata::ResultAndByteBuffer initData = MP4Metadata::Metadata(stream);
  if (!initData.Ref()) {
    return InitPromise::CreateAndReject(
        NS_FAILED(initData.Result())
            ? std::move(initData.Result())
            : MediaResult(NS_ERROR_DOM_MEDIA_DEMUXER_ERR,
                          RESULT_DETAIL("Invalid MP4 metadata or OOM")),
        __func__);
  } else if (NS_FAILED(initData.Result()) && result == NS_OK) {
    result = std::move(initData.Result());
  }

  RefPtr<BufferStream> bufferstream = new BufferStream(initData.Ref());

  MP4Metadata metadata{bufferstream};
  DDLINKCHILD("metadata", &metadata);
  nsresult rv = metadata.Parse();
  if (NS_FAILED(rv)) {
    return InitPromise::CreateAndReject(
        MediaResult(rv, RESULT_DETAIL("Parse MP4 metadata failed")), __func__);
  }

  auto audioTrackCount = metadata.GetNumberTracks(TrackInfo::kAudioTrack);
  if (audioTrackCount.Ref() == MP4Metadata::NumberTracksError()) {
    if (StaticPrefs::MediaPlaybackWarningsAsErrors()) {
      return InitPromise::CreateAndReject(
          MediaResult(
              NS_ERROR_DOM_MEDIA_DEMUXER_ERR,
              RESULT_DETAIL("Invalid audio track (%s)",
                            audioTrackCount.Result().Description().get())),
          __func__);
    }
    audioTrackCount.Ref() = 0;
  }

  auto videoTrackCount = metadata.GetNumberTracks(TrackInfo::kVideoTrack);
  if (videoTrackCount.Ref() == MP4Metadata::NumberTracksError()) {
    if (StaticPrefs::MediaPlaybackWarningsAsErrors()) {
      return InitPromise::CreateAndReject(
          MediaResult(
              NS_ERROR_DOM_MEDIA_DEMUXER_ERR,
              RESULT_DETAIL("Invalid video track (%s)",
                            videoTrackCount.Result().Description().get())),
          __func__);
    }
    videoTrackCount.Ref() = 0;
  }

  if (audioTrackCount.Ref() == 0 && videoTrackCount.Ref() == 0) {
    return InitPromise::CreateAndReject(
        MediaResult(
            NS_ERROR_DOM_MEDIA_DEMUXER_ERR,
            RESULT_DETAIL("No MP4 audio (%s) or video (%s) tracks",
                          audioTrackCount.Result().Description().get(),
                          videoTrackCount.Result().Description().get())),
        __func__);
  }

  if (NS_FAILED(audioTrackCount.Result()) && result == NS_OK) {
    result = std::move(audioTrackCount.Result());
  }
  if (NS_FAILED(videoTrackCount.Result()) && result == NS_OK) {
    result = std::move(videoTrackCount.Result());
  }

  if (audioTrackCount.Ref() != 0) {
    for (size_t i = 0; i < audioTrackCount.Ref(); i++) {
      MP4Metadata::ResultAndTrackInfo info =
          metadata.GetTrackInfo(TrackInfo::kAudioTrack, i);
      if (!info.Ref()) {
        if (StaticPrefs::MediaPlaybackWarningsAsErrors()) {
          return InitPromise::CreateAndReject(
              MediaResult(NS_ERROR_DOM_MEDIA_DEMUXER_ERR,
                          RESULT_DETAIL("Invalid MP4 audio track (%s)",
                                        info.Result().Description().get())),
              __func__);
        }
        if (result == NS_OK) {
          result =
              MediaResult(NS_ERROR_DOM_MEDIA_DEMUXER_ERR,
                          RESULT_DETAIL("Invalid MP4 audio track (%s)",
                                        info.Result().Description().get()));
        }
        continue;
      } else if (NS_FAILED(info.Result()) && result == NS_OK) {
        result = std::move(info.Result());
      }
      MP4Metadata::ResultAndIndice indices =
          metadata.GetTrackIndice(info.Ref()->mTrackId);
      if (!indices.Ref()) {
        if (NS_FAILED(info.Result()) && result == NS_OK) {
          result = std::move(indices.Result());
        }
        continue;
      }
      RefPtr<MP4TrackDemuxer> demuxer = new MP4TrackDemuxer(
          mResource, std::move(info.Ref()), *indices.Ref().get());
      DDLINKCHILD("audio demuxer", demuxer.get());
      mAudioDemuxers.AppendElement(std::move(demuxer));
    }
  }

  if (videoTrackCount.Ref() != 0) {
    for (size_t i = 0; i < videoTrackCount.Ref(); i++) {
      MP4Metadata::ResultAndTrackInfo info =
          metadata.GetTrackInfo(TrackInfo::kVideoTrack, i);
      if (!info.Ref()) {
        if (StaticPrefs::MediaPlaybackWarningsAsErrors()) {
          return InitPromise::CreateAndReject(
              MediaResult(NS_ERROR_DOM_MEDIA_DEMUXER_ERR,
                          RESULT_DETAIL("Invalid MP4 video track (%s)",
                                        info.Result().Description().get())),
              __func__);
        }
        if (result == NS_OK) {
          result =
              MediaResult(NS_ERROR_DOM_MEDIA_DEMUXER_ERR,
                          RESULT_DETAIL("Invalid MP4 video track (%s)",
                                        info.Result().Description().get()));
        }
        continue;
      } else if (NS_FAILED(info.Result()) && result == NS_OK) {
        result = std::move(info.Result());
      }
      MP4Metadata::ResultAndIndice indices =
          metadata.GetTrackIndice(info.Ref()->mTrackId);
      if (!indices.Ref()) {
        if (NS_FAILED(info.Result()) && result == NS_OK) {
          result = std::move(indices.Result());
        }
        continue;
      }
      RefPtr<MP4TrackDemuxer> demuxer = new MP4TrackDemuxer(
          mResource, std::move(info.Ref()), *indices.Ref().get());
      DDLINKCHILD("video demuxer", demuxer.get());
      mVideoDemuxers.AppendElement(std::move(demuxer));
    }
  }

  MP4Metadata::ResultAndCryptoFile cryptoFile = metadata.Crypto();
  if (NS_FAILED(cryptoFile.Result()) && result == NS_OK) {
    result = std::move(cryptoFile.Result());
  }
  MOZ_ASSERT(cryptoFile.Ref());
  if (cryptoFile.Ref()->valid) {
    const nsTArray<PsshInfo>& psshs = cryptoFile.Ref()->pssh;
    for (uint32_t i = 0; i < psshs.Length(); i++) {
      mCryptoInitData.AppendElements(psshs[i].data);
    }
  }

  mIsSeekable = metadata.CanSeek();

  return InitPromise::CreateAndResolve(result, __func__);
}

uint32_t MP4Demuxer::GetNumberTracks(TrackInfo::TrackType aType) const {
  switch (aType) {
    case TrackInfo::kAudioTrack:
      return uint32_t(mAudioDemuxers.Length());
    case TrackInfo::kVideoTrack:
      return uint32_t(mVideoDemuxers.Length());
    default:
      return 0;
  }
}

already_AddRefed<MediaTrackDemuxer> MP4Demuxer::GetTrackDemuxer(
    TrackInfo::TrackType aType, uint32_t aTrackNumber) {
  switch (aType) {
    case TrackInfo::kAudioTrack:
      if (aTrackNumber >= uint32_t(mAudioDemuxers.Length())) {
        return nullptr;
      }
      return RefPtr<MediaTrackDemuxer>(mAudioDemuxers[aTrackNumber]).forget();
    case TrackInfo::kVideoTrack:
      if (aTrackNumber >= uint32_t(mVideoDemuxers.Length())) {
        return nullptr;
      }
      return RefPtr<MediaTrackDemuxer>(mVideoDemuxers[aTrackNumber]).forget();
    default:
      return nullptr;
  }
}

bool MP4Demuxer::IsSeekable() const { return mIsSeekable; }

void MP4Demuxer::NotifyDataArrived() {
  for (auto& dmx : mAudioDemuxers) {
    dmx->NotifyDataArrived();
  }
  for (auto& dmx : mVideoDemuxers) {
    dmx->NotifyDataArrived();
  }
}

void MP4Demuxer::NotifyDataRemoved() {
  for (auto& dmx : mAudioDemuxers) {
    dmx->NotifyDataRemoved();
  }
  for (auto& dmx : mVideoDemuxers) {
    dmx->NotifyDataRemoved();
  }
}

UniquePtr<EncryptionInfo> MP4Demuxer::GetCrypto() {
  UniquePtr<EncryptionInfo> crypto;
  if (!mCryptoInitData.IsEmpty()) {
    crypto.reset(new EncryptionInfo{});
    crypto->AddInitData(NS_LITERAL_STRING("cenc"), mCryptoInitData);
  }
  return crypto;
}

MP4TrackDemuxer::MP4TrackDemuxer(MediaResource* aResource,
                                 UniquePtr<TrackInfo>&& aInfo,
                                 const IndiceWrapper& aIndices)
    : mResource(aResource),
      mStream(new ResourceStream(aResource)),
      mInfo(std::move(aInfo)),
      mIndex(new Index(aIndices, mStream, mInfo->mTrackId, mInfo->IsAudio())),
      mIterator(MakeUnique<SampleIterator>(mIndex)),
      mNeedReIndex(true) {
  EnsureUpToDateIndex();  // Force update of index

  VideoInfo* videoInfo = mInfo->GetAsVideoInfo();
  // Collect telemetry from h264 AVCC SPS.
  if (videoInfo && (mInfo->mMimeType.EqualsLiteral("video/mp4") ||
                    mInfo->mMimeType.EqualsLiteral("video/avc"))) {
    mIsH264 = true;
    RefPtr<MediaByteBuffer> extraData = videoInfo->mExtraData;
    mNeedSPSForTelemetry = AccumulateSPSTelemetry(extraData);
    SPSData spsdata;
    if (H264::DecodeSPSFromExtraData(extraData, spsdata) &&
        spsdata.pic_width > 0 && spsdata.pic_height > 0 &&
        H264::EnsureSPSIsSane(spsdata)) {
      videoInfo->mImage.width = spsdata.pic_width;
      videoInfo->mImage.height = spsdata.pic_height;
      videoInfo->mDisplay.width = spsdata.display_width;
      videoInfo->mDisplay.height = spsdata.display_height;
    }
  } else {
    // No SPS to be found.
    mNeedSPSForTelemetry = false;
  }
}

UniquePtr<TrackInfo> MP4TrackDemuxer::GetInfo() const { return mInfo->Clone(); }

void MP4TrackDemuxer::EnsureUpToDateIndex() {
  if (!mNeedReIndex) {
    return;
  }
  AutoPinned<MediaResource> resource(mResource);
  MediaByteRangeSet byteRanges;
  nsresult rv = resource->GetCachedRanges(byteRanges);
  if (NS_FAILED(rv)) {
    return;
  }
  mIndex->UpdateMoofIndex(byteRanges);
  mNeedReIndex = false;
}

RefPtr<MP4TrackDemuxer::SeekPromise> MP4TrackDemuxer::Seek(
    const media::TimeUnit& aTime) {
  auto seekTime = aTime;
  mQueuedSample = nullptr;

  mIterator->Seek(seekTime.ToMicroseconds());

  // Check what time we actually seeked to.
  do {
    RefPtr<MediaRawData> sample = GetNextSample();
    if (!sample) {
      return SeekPromise::CreateAndReject(NS_ERROR_DOM_MEDIA_END_OF_STREAM,
                                          __func__);
    }
    if (!sample->Size()) {
      // This sample can't be decoded, continue searching.
      continue;
    }
    if (sample->mKeyframe) {
      mQueuedSample = sample;
      seekTime = mQueuedSample->mTime;
    }
  } while (!mQueuedSample);

  SetNextKeyFrameTime();

  return SeekPromise::CreateAndResolve(seekTime, __func__);
}

already_AddRefed<MediaRawData> MP4TrackDemuxer::GetNextSample() {
  RefPtr<MediaRawData> sample = mIterator->GetNext();
  if (!sample) {
    return nullptr;
  }
  if (mInfo->GetAsVideoInfo()) {
    sample->mExtraData = mInfo->GetAsVideoInfo()->mExtraData;
    if (mIsH264 && !sample->mCrypto.mValid) {
      H264::FrameType type = H264::GetFrameType(sample);
      switch (type) {
        case H264::FrameType::I_FRAME:
          MOZ_FALLTHROUGH;
        case H264::FrameType::OTHER: {
          bool keyframe = type == H264::FrameType::I_FRAME;
          if (sample->mKeyframe != keyframe) {
            NS_WARNING(nsPrintfCString("Frame incorrectly marked as %skeyframe "
                                       "@ pts:%" PRId64 " dur:%" PRId64
                                       " dts:%" PRId64,
                                       keyframe ? "" : "non-",
                                       sample->mTime.ToMicroseconds(),
                                       sample->mDuration.ToMicroseconds(),
                                       sample->mTimecode.ToMicroseconds())
                           .get());
            sample->mKeyframe = keyframe;
          }
          break;
        }
        case H264::FrameType::INVALID:
          NS_WARNING(nsPrintfCString("Invalid H264 frame @ pts:%" PRId64
                                     " dur:%" PRId64 " dts:%" PRId64,
                                     sample->mTime.ToMicroseconds(),
                                     sample->mDuration.ToMicroseconds(),
                                     sample->mTimecode.ToMicroseconds())
                         .get());
          // We could reject the sample now, however demuxer errors are fatal.
          // So we keep the invalid frame, relying on the H264 decoder to
          // handle the error later.
          // TODO: make demuxer errors non-fatal.
          break;
      }
    }
  }

  if (sample->mCrypto.mValid) {
    UniquePtr<MediaRawDataWriter> writer(sample->CreateWriter());
    writer->mCrypto.mMode = mInfo->mCrypto.mMode;

    // Only use the default key parsed from the moov if we haven't already got
    // one from the sample group description.
    if (writer->mCrypto.mKeyId.Length() == 0) {
      writer->mCrypto.mIVSize = mInfo->mCrypto.mIVSize;
      writer->mCrypto.mKeyId.AppendElements(mInfo->mCrypto.mKeyId);
    }
  }
  return sample.forget();
}

RefPtr<MP4TrackDemuxer::SamplesPromise> MP4TrackDemuxer::GetSamples(
    int32_t aNumSamples) {
  EnsureUpToDateIndex();
  RefPtr<SamplesHolder> samples = new SamplesHolder;
  if (!aNumSamples) {
    return SamplesPromise::CreateAndReject(NS_ERROR_DOM_MEDIA_DEMUXER_ERR,
                                           __func__);
  }

  if (mQueuedSample) {
    NS_ASSERTION(mQueuedSample->mKeyframe, "mQueuedSample must be a keyframe");
    samples->mSamples.AppendElement(mQueuedSample);
    mQueuedSample = nullptr;
    aNumSamples--;
  }
  RefPtr<MediaRawData> sample;
  while (aNumSamples && (sample = GetNextSample())) {
    if (!sample->Size()) {
      continue;
    }
    samples->mSamples.AppendElement(sample);
    aNumSamples--;
  }

  if (samples->mSamples.IsEmpty()) {
    return SamplesPromise::CreateAndReject(NS_ERROR_DOM_MEDIA_END_OF_STREAM,
                                           __func__);
  }
  for (const auto& sample : samples->mSamples) {
    // Collect telemetry from h264 Annex B SPS.
    if (mNeedSPSForTelemetry && mIsH264 && AnnexB::IsAVCC(sample)) {
      RefPtr<MediaByteBuffer> extradata = H264::ExtractExtraData(sample);
      if (H264::HasSPS(extradata)) {
        RefPtr<MediaByteBuffer> extradata = H264::ExtractExtraData(sample);
        mNeedSPSForTelemetry = AccumulateSPSTelemetry(extradata);
      }
    }
  }

  if (mNextKeyframeTime.isNothing() ||
      samples->mSamples.LastElement()->mTime >= mNextKeyframeTime.value()) {
    SetNextKeyFrameTime();
  }
  return SamplesPromise::CreateAndResolve(samples, __func__);
}

void MP4TrackDemuxer::SetNextKeyFrameTime() {
  mNextKeyframeTime.reset();
  Microseconds frameTime = mIterator->GetNextKeyframeTime();
  if (frameTime != -1) {
    mNextKeyframeTime.emplace(media::TimeUnit::FromMicroseconds(frameTime));
  }
}

void MP4TrackDemuxer::Reset() {
  mQueuedSample = nullptr;
  // TODO, Seek to first frame available, which isn't always 0.
  mIterator->Seek(0);
  SetNextKeyFrameTime();
}

nsresult MP4TrackDemuxer::GetNextRandomAccessPoint(media::TimeUnit* aTime) {
  if (mNextKeyframeTime.isNothing()) {
    // There's no next key frame.
    *aTime = media::TimeUnit::FromInfinity();
  } else {
    *aTime = mNextKeyframeTime.value();
  }
  return NS_OK;
}

RefPtr<MP4TrackDemuxer::SkipAccessPointPromise>
MP4TrackDemuxer::SkipToNextRandomAccessPoint(
    const media::TimeUnit& aTimeThreshold) {
  mQueuedSample = nullptr;
  // Loop until we reach the next keyframe after the threshold.
  uint32_t parsed = 0;
  bool found = false;
  RefPtr<MediaRawData> sample;
  while (!found && (sample = GetNextSample())) {
    parsed++;
    if (sample->mKeyframe && sample->mTime >= aTimeThreshold) {
      found = true;
      mQueuedSample = sample;
    }
  }
  SetNextKeyFrameTime();
  if (found) {
    return SkipAccessPointPromise::CreateAndResolve(parsed, __func__);
  }
  SkipFailureHolder failure(NS_ERROR_DOM_MEDIA_END_OF_STREAM, parsed);
  return SkipAccessPointPromise::CreateAndReject(std::move(failure), __func__);
}

media::TimeIntervals MP4TrackDemuxer::GetBuffered() {
  EnsureUpToDateIndex();
  AutoPinned<MediaResource> resource(mResource);
  MediaByteRangeSet byteRanges;
  nsresult rv = resource->GetCachedRanges(byteRanges);

  if (NS_FAILED(rv)) {
    return media::TimeIntervals();
  }

  return mIndex->ConvertByteRangesToTimeRanges(byteRanges);
}

void MP4TrackDemuxer::NotifyDataArrived() { mNeedReIndex = true; }

void MP4TrackDemuxer::NotifyDataRemoved() {
  AutoPinned<MediaResource> resource(mResource);
  MediaByteRangeSet byteRanges;
  nsresult rv = resource->GetCachedRanges(byteRanges);
  if (NS_FAILED(rv)) {
    return;
  }
  mIndex->UpdateMoofIndex(byteRanges, true /* can evict */);
  mNeedReIndex = false;
}

}  // namespace mozilla

#undef LOG
