/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <algorithm>
#include <limits>
#include <stdint.h>

#include "MP4Demuxer.h"
#include "mp4_demuxer/MoofParser.h"
#include "mp4_demuxer/MP4Metadata.h"
#include "mp4_demuxer/ResourceStream.h"
#include "mp4_demuxer/BufferStream.h"
#include "mp4_demuxer/Index.h"
#include "nsPrintfCString.h"

// Used for telemetry
#include "mozilla/Telemetry.h"
#include "mp4_demuxer/AnnexB.h"
#include "mp4_demuxer/H264.h"

#include "nsAutoPtr.h"

extern mozilla::LazyLogModule gMediaDemuxerLog;
mozilla::LogModule* GetDemuxerLog() {
  return gMediaDemuxerLog;
}

#define LOG(arg, ...) MOZ_LOG(gMediaDemuxerLog, mozilla::LogLevel::Debug, ("MP4Demuxer(%p)::%s: " arg, this, __func__, ##__VA_ARGS__))

namespace mozilla {

class MP4TrackDemuxer : public MediaTrackDemuxer
{
public:
  MP4TrackDemuxer(MP4Demuxer* aParent,
                  UniquePtr<TrackInfo>&& aInfo,
                  const nsTArray<mp4_demuxer::Index::Indice>& indices);

  UniquePtr<TrackInfo> GetInfo() const override;

  RefPtr<SeekPromise> Seek(media::TimeUnit aTime) override;

  RefPtr<SamplesPromise> GetSamples(int32_t aNumSamples = 1) override;

  void Reset() override;

  nsresult GetNextRandomAccessPoint(media::TimeUnit* aTime) override;

  RefPtr<SkipAccessPointPromise> SkipToNextRandomAccessPoint(media::TimeUnit aTimeThreshold) override;

  media::TimeIntervals GetBuffered() override;

  void BreakCycles() override;

  void NotifyDataRemoved();

private:
  friend class MP4Demuxer;
  void NotifyDataArrived();
  already_AddRefed<MediaRawData> GetNextSample();
  void EnsureUpToDateIndex();
  void SetNextKeyFrameTime();
  RefPtr<MP4Demuxer> mParent;
  RefPtr<mp4_demuxer::ResourceStream> mStream;
  UniquePtr<TrackInfo> mInfo;
  RefPtr<mp4_demuxer::Index> mIndex;
  UniquePtr<mp4_demuxer::SampleIterator> mIterator;
  Maybe<media::TimeUnit> mNextKeyframeTime;
  // Queued samples extracted by the demuxer, but not yet returned.
  RefPtr<MediaRawData> mQueuedSample;
  bool mNeedReIndex;
  bool mNeedSPSForTelemetry;
  bool mIsH264 = false;
};


// Returns true if no SPS was found and search for it should continue.
bool
AccumulateSPSTelemetry(const MediaByteBuffer* aExtradata)
{
  mp4_demuxer::SPSData spsdata;
  if (mp4_demuxer::H264::DecodeSPSFromExtraData(aExtradata, spsdata)) {
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
                          (spsdata.level_idc >= 10 && spsdata.level_idc <= 52) ?
                          spsdata.level_idc : 0);

    // max_num_ref_frames should be between 0 and 16, anything larger will
    // be treated as invalid.
    Telemetry::Accumulate(Telemetry::VIDEO_H264_SPS_MAX_NUM_REF_FRAMES,
                          std::min(spsdata.max_num_ref_frames, 17u));

    return false;
  }

  return true;
}

MP4Demuxer::MP4Demuxer(MediaResource* aResource)
  : mResource(aResource)
  , mStream(new mp4_demuxer::ResourceStream(aResource))
  , mInitData(new MediaByteBuffer)
{
}

RefPtr<MP4Demuxer::InitPromise>
MP4Demuxer::Init()
{
  AutoPinned<mp4_demuxer::ResourceStream> stream(mStream);

  // Check that we have enough data to read the metadata.
  if (!mp4_demuxer::MP4Metadata::HasCompleteMetadata(stream)) {
    return InitPromise::CreateAndReject(NS_ERROR_DOM_MEDIA_DEMUXER_ERR, __func__);
  }

  mInitData = mp4_demuxer::MP4Metadata::Metadata(stream);
  if (!mInitData) {
    // OOM
    return InitPromise::CreateAndReject(NS_ERROR_DOM_MEDIA_DEMUXER_ERR, __func__);
  }

  RefPtr<mp4_demuxer::BufferStream> bufferstream =
    new mp4_demuxer::BufferStream(mInitData);

  mMetadata = MakeUnique<mp4_demuxer::MP4Metadata>(bufferstream);

  if (!mMetadata->GetNumberTracks(mozilla::TrackInfo::kAudioTrack) &&
      !mMetadata->GetNumberTracks(mozilla::TrackInfo::kVideoTrack)) {
    return InitPromise::CreateAndReject(NS_ERROR_DOM_MEDIA_DEMUXER_ERR, __func__);
  }

  return InitPromise::CreateAndResolve(NS_OK, __func__);
}

bool
MP4Demuxer::HasTrackType(TrackInfo::TrackType aType) const
{
  return !!GetNumberTracks(aType);
}

uint32_t
MP4Demuxer::GetNumberTracks(TrackInfo::TrackType aType) const
{
  return mMetadata->GetNumberTracks(aType);
}

already_AddRefed<MediaTrackDemuxer>
MP4Demuxer::GetTrackDemuxer(TrackInfo::TrackType aType, uint32_t aTrackNumber)
{
  if (mMetadata->GetNumberTracks(aType) <= aTrackNumber) {
    return nullptr;
  }
  UniquePtr<TrackInfo> info = mMetadata->GetTrackInfo(aType, aTrackNumber);
  if (!info) {
    return nullptr;
  }
  FallibleTArray<mp4_demuxer::Index::Indice> indices;
  if (!mMetadata->ReadTrackIndex(indices, info->mTrackId)) {
    return nullptr;
  }
  RefPtr<MP4TrackDemuxer> e = new MP4TrackDemuxer(this, Move(info), indices);
  mDemuxers.AppendElement(e);

  return e.forget();
}

bool
MP4Demuxer::IsSeekable() const
{
  return mMetadata->CanSeek();
}

void
MP4Demuxer::NotifyDataArrived()
{
  for (uint32_t i = 0; i < mDemuxers.Length(); i++) {
    mDemuxers[i]->NotifyDataArrived();
  }
}

void
MP4Demuxer::NotifyDataRemoved()
{
  for (uint32_t i = 0; i < mDemuxers.Length(); i++) {
    mDemuxers[i]->NotifyDataRemoved();
  }
}

UniquePtr<EncryptionInfo>
MP4Demuxer::GetCrypto()
{
  const mp4_demuxer::CryptoFile& cryptoFile = mMetadata->Crypto();
  if (!cryptoFile.valid) {
    return nullptr;
  }

  const nsTArray<mp4_demuxer::PsshInfo>& psshs = cryptoFile.pssh;
  nsTArray<uint8_t> initData;
  for (uint32_t i = 0; i < psshs.Length(); i++) {
    initData.AppendElements(psshs[i].data);
  }

  if (initData.IsEmpty()) {
    return nullptr;
  }

  auto crypto = MakeUnique<EncryptionInfo>();
  crypto->AddInitData(NS_LITERAL_STRING("cenc"), Move(initData));

  return crypto;
}

MP4TrackDemuxer::MP4TrackDemuxer(MP4Demuxer* aParent,
                                 UniquePtr<TrackInfo>&& aInfo,
                                 const nsTArray<mp4_demuxer::Index::Indice>& indices)
  : mParent(aParent)
  , mStream(new mp4_demuxer::ResourceStream(mParent->mResource))
  , mInfo(Move(aInfo))
  , mIndex(new mp4_demuxer::Index(indices,
                                  mStream,
                                  mInfo->mTrackId,
                                  mInfo->IsAudio()))
  , mIterator(MakeUnique<mp4_demuxer::SampleIterator>(mIndex))
  , mNeedReIndex(true)
{
  EnsureUpToDateIndex(); // Force update of index

  VideoInfo* videoInfo = mInfo->GetAsVideoInfo();
  // Collect telemetry from h264 AVCC SPS.
  if (videoInfo &&
      (mInfo->mMimeType.EqualsLiteral("video/mp4") ||
       mInfo->mMimeType.EqualsLiteral("video/avc"))) {
    mIsH264 = true;
    RefPtr<MediaByteBuffer> extraData = videoInfo->mExtraData;
    mNeedSPSForTelemetry = AccumulateSPSTelemetry(extraData);
    mp4_demuxer::SPSData spsdata;
    if (mp4_demuxer::H264::DecodeSPSFromExtraData(extraData, spsdata) &&
        spsdata.pic_width > 0 && spsdata.pic_height > 0 &&
        mp4_demuxer::H264::EnsureSPSIsSane(spsdata)) {
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

UniquePtr<TrackInfo>
MP4TrackDemuxer::GetInfo() const
{
  return mInfo->Clone();
}

void
MP4TrackDemuxer::EnsureUpToDateIndex()
{
  if (!mNeedReIndex) {
    return;
  }
  AutoPinned<MediaResource> resource(mParent->mResource);
  MediaByteRangeSet byteRanges;
  nsresult rv = resource->GetCachedRanges(byteRanges);
  if (NS_FAILED(rv)) {
    return;
  }
  mIndex->UpdateMoofIndex(byteRanges);
  mNeedReIndex = false;
}

RefPtr<MP4TrackDemuxer::SeekPromise>
MP4TrackDemuxer::Seek(media::TimeUnit aTime)
{
  int64_t seekTime = aTime.ToMicroseconds();
  mQueuedSample = nullptr;

  mIterator->Seek(seekTime);

  // Check what time we actually seeked to.
  mQueuedSample = GetNextSample();
  if (mQueuedSample) {
    seekTime = mQueuedSample->mTime;
  }

  SetNextKeyFrameTime();

  return SeekPromise::CreateAndResolve(media::TimeUnit::FromMicroseconds(seekTime), __func__);
}

already_AddRefed<MediaRawData>
MP4TrackDemuxer::GetNextSample()
{
  RefPtr<MediaRawData> sample = mIterator->GetNext();
  if (!sample) {
    return nullptr;
  }
  if (mInfo->GetAsVideoInfo()) {
    sample->mExtraData = mInfo->GetAsVideoInfo()->mExtraData;
    if (mIsH264) {
      mp4_demuxer::H264::FrameType type =
        mp4_demuxer::H264::GetFrameType(sample);
      switch (type) {
        case mp4_demuxer::H264::FrameType::I_FRAME: MOZ_FALLTHROUGH;
        case mp4_demuxer::H264::FrameType::OTHER:
        {
          bool keyframe = type == mp4_demuxer::H264::FrameType::I_FRAME;
          if (sample->mKeyframe != keyframe) {
            NS_WARNING(nsPrintfCString("Frame incorrectly marked as %skeyframe @ pts:%lld dur:%u dts:%lld",
                                       keyframe ? "" : "non-",
                                       sample->mTime,
                                       sample->mDuration,
                                       sample->mTimecode).get());
            sample->mKeyframe = keyframe;
          }
          break;
        }
        case mp4_demuxer::H264::FrameType::INVALID:
          NS_WARNING(nsPrintfCString("Invalid H264 frame @ pts:%lld dur:%u dts:%lld",
                                     sample->mTime,
                                     sample->mDuration,
                                     sample->mTimecode).get());
          // We could reject the sample now, however demuxer errors are fatal.
          // So we keep the invalid frame, relying on the H264 decoder to
          // handle the error later.
          // TODO: make demuxer errors non-fatal.
          break;
      }
    }
  }
  if (sample->mCrypto.mValid) {
    nsAutoPtr<MediaRawDataWriter> writer(sample->CreateWriter());
    writer->mCrypto.mMode = mInfo->mCrypto.mMode;
    writer->mCrypto.mIVSize = mInfo->mCrypto.mIVSize;
    writer->mCrypto.mKeyId.AppendElements(mInfo->mCrypto.mKeyId);
  }
  return sample.forget();
}

RefPtr<MP4TrackDemuxer::SamplesPromise>
MP4TrackDemuxer::GetSamples(int32_t aNumSamples)
{
  EnsureUpToDateIndex();
  RefPtr<SamplesHolder> samples = new SamplesHolder;
  if (!aNumSamples) {
    return SamplesPromise::CreateAndReject(NS_ERROR_DOM_MEDIA_DEMUXER_ERR, __func__);
  }

  if (mQueuedSample) {
    MOZ_ASSERT(mQueuedSample->mKeyframe,
               "mQueuedSample must be a keyframe");
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
    return SamplesPromise::CreateAndReject(NS_ERROR_DOM_MEDIA_END_OF_STREAM, __func__);
  } else {
    for (const auto& sample : samples->mSamples) {
      // Collect telemetry from h264 Annex B SPS.
      if (mNeedSPSForTelemetry && mp4_demuxer::AnnexB::HasSPS(sample)) {
        RefPtr<MediaByteBuffer> extradata =
        mp4_demuxer::AnnexB::ExtractExtraData(sample);
        mNeedSPSForTelemetry = AccumulateSPSTelemetry(extradata);
      }
    }

    if (mNextKeyframeTime.isNothing() ||
        samples->mSamples.LastElement()->mTime >= mNextKeyframeTime.value().ToMicroseconds()) {
      SetNextKeyFrameTime();
    }
    return SamplesPromise::CreateAndResolve(samples, __func__);
  }
}

void
MP4TrackDemuxer::SetNextKeyFrameTime()
{
  mNextKeyframeTime.reset();
  mp4_demuxer::Microseconds frameTime = mIterator->GetNextKeyframeTime();
  if (frameTime != -1) {
    mNextKeyframeTime.emplace(
      media::TimeUnit::FromMicroseconds(frameTime));
  }
}

void
MP4TrackDemuxer::Reset()
{
  mQueuedSample = nullptr;
  // TODO, Seek to first frame available, which isn't always 0.
  mIterator->Seek(0);
  SetNextKeyFrameTime();
}

nsresult
MP4TrackDemuxer::GetNextRandomAccessPoint(media::TimeUnit* aTime)
{
  if (mNextKeyframeTime.isNothing()) {
    // There's no next key frame.
    *aTime =
      media::TimeUnit::FromMicroseconds(std::numeric_limits<int64_t>::max());
  } else {
    *aTime = mNextKeyframeTime.value();
  }
  return NS_OK;
}

RefPtr<MP4TrackDemuxer::SkipAccessPointPromise>
MP4TrackDemuxer::SkipToNextRandomAccessPoint(media::TimeUnit aTimeThreshold)
{
  mQueuedSample = nullptr;
  // Loop until we reach the next keyframe after the threshold.
  uint32_t parsed = 0;
  bool found = false;
  RefPtr<MediaRawData> sample;
  while (!found && (sample = GetNextSample())) {
    parsed++;
    if (sample->mKeyframe && sample->mTime >= aTimeThreshold.ToMicroseconds()) {
      found = true;
      mQueuedSample = sample;
    }
  }
  SetNextKeyFrameTime();
  if (found) {
    return SkipAccessPointPromise::CreateAndResolve(parsed, __func__);
  } else {
    SkipFailureHolder failure(NS_ERROR_DOM_MEDIA_END_OF_STREAM, parsed);
    return SkipAccessPointPromise::CreateAndReject(Move(failure), __func__);
  }
}

media::TimeIntervals
MP4TrackDemuxer::GetBuffered()
{
  EnsureUpToDateIndex();
  AutoPinned<MediaResource> resource(mParent->mResource);
  MediaByteRangeSet byteRanges;
  nsresult rv = resource->GetCachedRanges(byteRanges);

  if (NS_FAILED(rv)) {
    return media::TimeIntervals();
  }

  return mIndex->ConvertByteRangesToTimeRanges(byteRanges);
}

void
MP4TrackDemuxer::NotifyDataArrived()
{
  mNeedReIndex = true;
}

void
MP4TrackDemuxer::NotifyDataRemoved()
{
  AutoPinned<MediaResource> resource(mParent->mResource);
  MediaByteRangeSet byteRanges;
  nsresult rv = resource->GetCachedRanges(byteRanges);
  if (NS_FAILED(rv)) {
    return;
  }
  mIndex->UpdateMoofIndex(byteRanges, true /* can evict */);
  mNeedReIndex = false;
}

void
MP4TrackDemuxer::BreakCycles()
{
  mParent = nullptr;
}

} // namespace mozilla

#undef LOG
