/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(RemoteEncoderModule_h_)
#  define RemoteEncoderModule_h_

#  include "PlatformEncoderModule.h"
#  include "RemoteMediaManagerChild.h"

namespace mozilla {

class RemoteEncoderModule final : public PlatformEncoderModule {
 public:
  static already_AddRefed<PlatformEncoderModule> Create(
      RemoteMediaIn aLocation);

  already_AddRefed<MediaDataEncoder> CreateVideoEncoder(
      const EncoderConfig& aConfig,
      const RefPtr<TaskQueue>& aTaskQueue) const override {
    return CreateEncoder(aConfig, aTaskQueue);
  }

  already_AddRefed<MediaDataEncoder> CreateAudioEncoder(
      const EncoderConfig& aConfig,
      const RefPtr<TaskQueue>& aTaskQueue) const override {
    return CreateEncoder(aConfig, aTaskQueue);
  }

  RefPtr<PlatformEncoderModule::CreateEncoderPromise> AsyncCreateEncoder(
      const EncoderConfig& aEncoderConfig,
      const RefPtr<TaskQueue>& aTaskQueue) override;

  media::EncodeSupportSet Supports(const EncoderConfig& aConfig) const override;
  media::EncodeSupportSet SupportsCodec(CodecType aCodecType) const override;

  const char* GetName() const override;

 private:
  already_AddRefed<MediaDataEncoder> CreateEncoder(
      const EncoderConfig& aConfig, const RefPtr<TaskQueue>& aTaskQueue) const;

  template <typename T, typename... Args>
  friend already_AddRefed<T> MakeAndAddRef(Args&&...);

  explicit RemoteEncoderModule(RemoteMediaIn aLocation);

  const RemoteMediaIn mLocation;
};

}  // namespace mozilla

#endif /* RemoteEncoderModule_h_ */
