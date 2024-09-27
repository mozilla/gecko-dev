/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOCK_MEDIA_DECODER_OWNER_H_
#define MOCK_MEDIA_DECODER_OWNER_H_

#include "MediaDecoderOwner.h"
#include "gmock/gmock.h"

namespace mozilla {

class MockMediaDecoderOwner final : public MediaDecoderOwner {
 public:
  MOCK_METHOD(void, PrincipalHandleChangedForVideoFrameContainer,
              (VideoFrameContainer * aContainer,
               const PrincipalHandle& aNewPrincipalHandle),
              (override));
  MOCK_METHOD(void, DownloadProgressed, (), (override));
  MOCK_METHOD(void, DispatchAsyncEvent, (const nsAString& aName), (override));
  MOCK_METHOD(void, UpdateReadyState, (), (override));
  MOCK_METHOD(void, MaybeQueueTimeupdateEvent, (), (override));
  MOCK_METHOD(bool, GetPaused, (), (override));
  MOCK_METHOD(void, MetadataLoaded,
              (const MediaInfo* aInfo, UniquePtr<const MetadataTags> aTags),
              (override));
  MOCK_METHOD(void, FirstFrameLoaded, (), (override));
  MOCK_METHOD(void, NetworkError, (const MediaResult& aError), (override));
  MOCK_METHOD(void, DecodeError, (const MediaResult& aError), (override));
  MOCK_METHOD(void, DecodeWarning, (const MediaResult& aError), (override));
  MOCK_METHOD(bool, HasError, (), (const, override));
  MOCK_METHOD(void, LoadAborted, (), (override));
  MOCK_METHOD(void, PlaybackEnded, (), (override));
  MOCK_METHOD(void, SeekStarted, (), (override));
  MOCK_METHOD(void, SeekCompleted, (), (override));
  MOCK_METHOD(void, SeekAborted, (), (override));
  MOCK_METHOD(void, DownloadSuspended, (), (override));
  MOCK_METHOD(void, NotifySuspendedByCache, (bool aSuspendedByCache),
              (override));
  MOCK_METHOD(void, NotifyDecoderPrincipalChanged, (), (override));
  MOCK_METHOD(void, SetAudibleState, (bool aAudible), (override));
  MOCK_METHOD(void, NotifyXPCOMShutdown, (), (override));
  MOCK_METHOD(void, DispatchEncrypted,
              (const nsTArray<uint8_t>& aInitData,
               const nsAString& aInitDataType),
              (override));
  MOCK_METHOD(bool, IsActuallyInvisible, (), (const, override));
  MOCK_METHOD(bool, ShouldResistFingerprinting, (RFPTarget aTarget),
              (const, override));
};

}  // namespace mozilla

#endif /* MOCK_MEDIA_DECODER_OWNER_H_ */
