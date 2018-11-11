/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(GMPVideoDecoder_h_)
#define GMPVideoDecoder_h_

#include "GMPVideoDecoderProxy.h"
#include "ImageContainer.h"
#include "MediaDataDecoderProxy.h"
#include "PlatformDecoderModule.h"
#include "mozIGeckoMediaPluginService.h"
#include "MediaInfo.h"

namespace mozilla {

class VideoCallbackAdapter : public GMPVideoDecoderCallbackProxy {
public:
  VideoCallbackAdapter(MediaDataDecoderCallbackProxy* aCallback,
                       VideoInfo aVideoInfo,
                       layers::ImageContainer* aImageContainer)
   : mCallback(aCallback)
   , mLastStreamOffset(0)
   , mVideoInfo(aVideoInfo)
   , mImageContainer(aImageContainer)
  {}

  MediaDataDecoderCallbackProxy* Callback() const { return mCallback; }

  // GMPVideoDecoderCallbackProxy
  void Decoded(GMPVideoi420Frame* aDecodedFrame) override;
  void ReceivedDecodedReferenceFrame(const uint64_t aPictureId) override;
  void ReceivedDecodedFrame(const uint64_t aPictureId) override;
  void InputDataExhausted() override;
  void DrainComplete() override;
  void ResetComplete() override;
  void Error(GMPErr aErr) override;
  void Terminated() override;

  void SetLastStreamOffset(int64_t aStreamOffset) {
    mLastStreamOffset = aStreamOffset;
  }

private:
  MediaDataDecoderCallbackProxy* mCallback;
  int64_t mLastStreamOffset;

  VideoInfo mVideoInfo;
  RefPtr<layers::ImageContainer> mImageContainer;
};

struct GMPVideoDecoderParams {
  explicit GMPVideoDecoderParams(const CreateDecoderParams& aParams);
  GMPVideoDecoderParams& WithCallback(MediaDataDecoderProxy* aWrapper);
  GMPVideoDecoderParams& WithAdapter(VideoCallbackAdapter* aAdapter);

  const VideoInfo& mConfig;
  TaskQueue* mTaskQueue;
  MediaDataDecoderCallbackProxy* mCallback;
  VideoCallbackAdapter* mAdapter;
  layers::ImageContainer* mImageContainer;
  layers::LayersBackend mLayersBackend;
  RefPtr<GMPCrashHelper> mCrashHelper;
};

class GMPVideoDecoder : public MediaDataDecoder {
public:
  explicit GMPVideoDecoder(const GMPVideoDecoderParams& aParams);

  RefPtr<InitPromise> Init() override;
  void Input(MediaRawData* aSample) override;
  void Flush() override;
  void Drain() override;
  void Shutdown() override;
  const char* GetDescriptionName() const override
  {
    return "GMP video decoder";
  }

protected:
  virtual void InitTags(nsTArray<nsCString>& aTags);
  virtual nsCString GetNodeId();
  virtual uint32_t DecryptorId() const { return 0; }
  virtual GMPUniquePtr<GMPVideoEncodedFrame> CreateFrame(MediaRawData* aSample);
  virtual const VideoInfo& GetConfig() const;

private:

  class GMPInitDoneCallback : public GetGMPVideoDecoderCallback
  {
  public:
    explicit GMPInitDoneCallback(GMPVideoDecoder* aDecoder)
      : mDecoder(aDecoder)
    {
    }

    void Done(GMPVideoDecoderProxy* aGMP, GMPVideoHost* aHost) override
    {
      mDecoder->GMPInitDone(aGMP, aHost);
    }

  private:
    RefPtr<GMPVideoDecoder> mDecoder;
  };
  void GMPInitDone(GMPVideoDecoderProxy* aGMP, GMPVideoHost* aHost);

  const VideoInfo mConfig;
  MediaDataDecoderCallbackProxy* mCallback;
  nsCOMPtr<mozIGeckoMediaPluginService> mMPS;
  GMPVideoDecoderProxy* mGMP;
  GMPVideoHost* mHost;
  nsAutoPtr<VideoCallbackAdapter> mAdapter;
  bool mConvertNALUnitLengths;
  MozPromiseHolder<InitPromise> mInitPromise;
  RefPtr<GMPCrashHelper> mCrashHelper;
};

} // namespace mozilla

#endif // GMPVideoDecoder_h_
