/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOCK_MEDIA_DATA_DEMUXER_H_
#define MOCK_MEDIA_DATA_DEMUXER_H_

#include "MediaContainerType.h"
#include "MediaDecoderOwner.h"
#include "MediaMIMETypes.h"
#include "VideoUtils.h"
#include "gmock/gmock.h"

namespace mozilla {

class MockMediaDataDemuxer : public MediaDataDemuxer {
 public:
  MockMediaDataDemuxer() {
    ON_CALL(*this, Init).WillByDefault([]() {
      return InitPromise::CreateAndResolve(NS_OK, __func__);
    });
  }

  MOCK_METHOD(RefPtr<InitPromise>, Init, (), (override));
  MOCK_METHOD(uint32_t, GetNumberTracks, (TrackInfo::TrackType aType),
              (const, override));
  MOCK_METHOD(already_AddRefed<MediaTrackDemuxer>, GetTrackDemuxer,
              (TrackInfo::TrackType aType, uint32_t aTrackNumber), (override));
  MOCK_METHOD(bool, IsSeekable, (), (const, override));

 protected:
  ~MockMediaDataDemuxer() override = default;
};

class MockMediaTrackDemuxer : public MediaTrackDemuxer {
 public:
  // aExtendedMimeType must be a static string or otherwise alive when
  // GetInfo() is called.
  explicit MockMediaTrackDemuxer(const char* aExtendedMimeType) {
    ON_CALL(*this, GetInfo).WillByDefault([=]() {
      auto extended = MakeMediaContainerType(aExtendedMimeType).value();
      return CreateTrackInfoWithMIMETypeAndContainerTypeExtraParameters(
          extended.Type().AsString(), extended);
    });
  }

  MOCK_METHOD(UniquePtr<TrackInfo>, GetInfo, (), (const, override));
  MOCK_METHOD(RefPtr<SeekPromise>, Seek, (const media::TimeUnit& aTime),
              (override));
  RefPtr<SamplesPromise> GetSamples(int32_t aNumSamples = 1) override {
    EXPECT_EQ(aNumSamples, 1) << "Multiple samples not implemented";
    return MockGetSamples();
  }
  MOCK_METHOD(RefPtr<SamplesPromise>, MockGetSamples, ());
  MOCK_METHOD(void, Reset, (), (override));
  MOCK_METHOD(RefPtr<SkipAccessPointPromise>, SkipToNextRandomAccessPoint,
              (const media::TimeUnit& aTimeThreshold), (override));
  MOCK_METHOD(media::TimeIntervals, GetBuffered, (), (override));

 protected:
  ~MockMediaTrackDemuxer() override = default;
};

}  // namespace mozilla

#endif /* MOCK_MEDIA_DATA_DEMUXER_H_ */
