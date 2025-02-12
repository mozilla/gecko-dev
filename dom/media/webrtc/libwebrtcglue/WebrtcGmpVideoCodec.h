/*
 *  Copyright (c) 2012, The WebRTC project authors. All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 *
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *
 *    * Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in
 *      the documentation and/or other materials provided with the
 *      distribution.
 *
 *    * Neither the name of Google nor the names of its contributors may
 *      be used to endorse or promote products derived from this software
 *      without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WEBRTCGMPVIDEOCODEC_H_
#define WEBRTCGMPVIDEOCODEC_H_

#include <string>

#include "nsThreadUtils.h"
#include "nsTArray.h"
#include "mozilla/EventTargetCapability.h"
#include "mozilla/Mutex.h"
#include "mozilla/glean/DomMediaWebrtcMetrics.h"

#include "mozIGeckoMediaPluginService.h"
#include "MediaConduitInterface.h"
#include "PerformanceRecorder.h"
#include "VideoConduit.h"
#include "api/video/video_frame_type.h"
#include "common_video/h264/h264_bitstream_parser.h"
#include "modules/video_coding/include/video_error_codes.h"
#include "modules/video_coding/svc/scalable_video_controller.h"

#include "gmp-video-host.h"
#include "GMPVideoDecoderProxy.h"
#include "GMPVideoEncoderProxy.h"

#include "jsapi/PeerConnectionImpl.h"

namespace mozilla::detail {
struct InputImageData {
  uint64_t rtp_timestamp = 0;
  int64_t timestamp_us = 0;
  webrtc::ScalableVideoController::LayerFrameConfig frame_config;
};
}  // namespace mozilla::detail

// webrtc::CodecSpecificInfo has members that are not always memmovable, which
// is required by nsTArray/AutoTArray by default. Use move constructors where
// necessary.
template <>
struct nsTArray_RelocationStrategy<mozilla::detail::InputImageData> {
  using IID = mozilla::detail::InputImageData;
  // This logic follows MemMoveAnnotation.h.
  using Type =
      std::conditional_t<(std::is_trivially_move_constructible_v<IID> ||
                          (!std::is_move_constructible_v<IID> &&
                           std::is_trivially_copy_constructible_v<IID>)) &&
                             std::is_trivially_destructible_v<IID>,
                         nsTArray_RelocateUsingMemutils,
                         nsTArray_RelocateUsingMoveConstructor<IID>>;
};

namespace mozilla {

static void NotifyGmpInitDone(const std::string& aPCHandle, int32_t aResult,
                              const std::string& aError = "") {
  if (!NS_IsMainThread()) {
    MOZ_ALWAYS_SUCCEEDS(GetMainThreadSerialEventTarget()->Dispatch(
        NS_NewRunnableFunction(__func__, [aPCHandle, aResult, aError] {
          NotifyGmpInitDone(aPCHandle, aResult, aError);
        })));
    return;
  }

  glean::webrtc::gmp_init_success
      .EnumGet(static_cast<glean::webrtc::GmpInitSuccessLabel>(
          aResult == WEBRTC_VIDEO_CODEC_OK))
      .Add();
  if (aResult == WEBRTC_VIDEO_CODEC_OK) {
    // Might be useful to notify the PeerConnection about successful init
    // someday.
    return;
  }

  PeerConnectionWrapper wrapper(aPCHandle);
  if (wrapper.impl()) {
    wrapper.impl()->OnMediaError(aError);
  }
}

// Hold a frame for later decode
class GMPDecodeData {
 public:
  GMPDecodeData(const webrtc::EncodedImage& aInputImage, bool aMissingFrames,
                int64_t aRenderTimeMs)
      : mImage(aInputImage),
        mMissingFrames(aMissingFrames),
        mRenderTimeMs(aRenderTimeMs) {
    // We want to use this for queuing, and the calling code recycles the
    // buffer on return from Decode()
    MOZ_RELEASE_ASSERT(aInputImage.size() <
                       (std::numeric_limits<size_t>::max() >> 1));
  }

  ~GMPDecodeData() = default;

  const webrtc::EncodedImage mImage;
  const bool mMissingFrames;
  const int64_t mRenderTimeMs;
};

class RefCountedWebrtcVideoEncoder {
 public:
  NS_INLINE_DECL_PURE_VIRTUAL_REFCOUNTING

  // Implement sort of WebrtcVideoEncoder interface and support refcounting.
  // (We cannot use |Release|, since that's needed for nsRefPtr)
  virtual int32_t InitEncode(
      const webrtc::VideoCodec* aCodecSettings,
      const webrtc::VideoEncoder::Settings& aSettings) = 0;

  virtual int32_t Encode(
      const webrtc::VideoFrame& aInputImage,
      const std::vector<webrtc::VideoFrameType>* aFrameTypes) = 0;

  virtual int32_t RegisterEncodeCompleteCallback(
      webrtc::EncodedImageCallback* aCallback) = 0;

  virtual int32_t Shutdown() = 0;

  virtual int32_t SetRates(
      const webrtc::VideoEncoder::RateControlParameters& aParameters) = 0;

  virtual MediaEventSource<uint64_t>* InitPluginEvent() = 0;

  virtual MediaEventSource<uint64_t>* ReleasePluginEvent() = 0;

  virtual WebrtcVideoEncoder::EncoderInfo GetEncoderInfo() const = 0;

 protected:
  virtual ~RefCountedWebrtcVideoEncoder() = default;
};

class WebrtcGmpVideoEncoder final : public GMPVideoEncoderCallbackProxy,
                                    public RefCountedWebrtcVideoEncoder {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(WebrtcGmpVideoEncoder, final);

  WebrtcGmpVideoEncoder(const webrtc::SdpVideoFormat& aFormat,
                        std::string aPCHandle);

  // Implement VideoEncoder interface, sort of.
  // (We cannot use |Release|, since that's needed for nsRefPtr)
  int32_t InitEncode(const webrtc::VideoCodec* aCodecSettings,
                     const webrtc::VideoEncoder::Settings& aSettings) override;

  int32_t Encode(
      const webrtc::VideoFrame& aInputImage,
      const std::vector<webrtc::VideoFrameType>* aFrameTypes) override;

  int32_t RegisterEncodeCompleteCallback(
      webrtc::EncodedImageCallback* aCallback) override;

  int32_t Shutdown() override;

  int32_t SetRates(
      const webrtc::VideoEncoder::RateControlParameters& aParameters) override;

  WebrtcVideoEncoder::EncoderInfo GetEncoderInfo() const override;

  MediaEventSource<uint64_t>* InitPluginEvent() override {
    return &mInitPluginEvent;
  }

  MediaEventSource<uint64_t>* ReleasePluginEvent() override {
    return &mReleasePluginEvent;
  }

  // GMPVideoEncoderCallback virtual functions.
  void Terminated() override;

  void Encoded(GMPVideoEncodedFrame* aEncodedFrame,
               const nsTArray<uint8_t>& aCodecSpecificInfo) override;

  void Error(GMPErr aError) override {}

 private:
  virtual ~WebrtcGmpVideoEncoder();

  void InitEncode_g(const GMPVideoCodec& aCodecParams, int32_t aNumberOfCores,
                    uint32_t aMaxPayloadSize);
  int32_t GmpInitDone_g(GMPVideoEncoderProxy* aGMP, GMPVideoHost* aHost,
                        const GMPVideoCodec& aCodecParams,
                        std::string* aErrorOut);
  int32_t GmpInitDone_g(GMPVideoEncoderProxy* aGMP, GMPVideoHost* aHost,
                        std::string* aErrorOut);
  int32_t InitEncoderForSize(unsigned short aWidth, unsigned short aHeight,
                             std::string* aErrorOut);
  void Close_g();

  class InitDoneCallback final : public GetGMPVideoEncoderCallback {
   public:
    InitDoneCallback(const RefPtr<WebrtcGmpVideoEncoder>& aEncoder,
                     const GMPVideoCodec& aCodecParams)
        : mEncoder(aEncoder), mCodecParams(aCodecParams) {}

    void Done(GMPVideoEncoderProxy* aGMP, GMPVideoHost* aHost) override {
      std::string errorOut;
      int32_t result =
          mEncoder->GmpInitDone_g(aGMP, aHost, mCodecParams, &errorOut);
      NotifyGmpInitDone(mEncoder->mPCHandle, result, errorOut);
    }

   private:
    const RefPtr<WebrtcGmpVideoEncoder> mEncoder;
    const GMPVideoCodec mCodecParams;
  };

  void Encode_g(const webrtc::VideoFrame& aInputImage,
                std::vector<webrtc::VideoFrameType> aFrameTypes);
  void RegetEncoderForResolutionChange(uint32_t aWidth, uint32_t aHeight);

  class InitDoneForResolutionChangeCallback final
      : public GetGMPVideoEncoderCallback {
   public:
    InitDoneForResolutionChangeCallback(
        const RefPtr<WebrtcGmpVideoEncoder>& aEncoder, uint32_t aWidth,
        uint32_t aHeight)
        : mEncoder(aEncoder), mWidth(aWidth), mHeight(aHeight) {}

    void Done(GMPVideoEncoderProxy* aGMP, GMPVideoHost* aHost) override {
      std::string errorOut;
      int32_t result = mEncoder->GmpInitDone_g(aGMP, aHost, &errorOut);
      if (result != WEBRTC_VIDEO_CODEC_OK) {
        NotifyGmpInitDone(mEncoder->mPCHandle, result, errorOut);
        return;
      }

      result = mEncoder->InitEncoderForSize(mWidth, mHeight, &errorOut);
      NotifyGmpInitDone(mEncoder->mPCHandle, result, errorOut);
    }

   private:
    const RefPtr<WebrtcGmpVideoEncoder> mEncoder;
    const uint32_t mWidth;
    const uint32_t mHeight;
  };

  int32_t SetRates_g(uint32_t aOldBitRateKbps, uint32_t aNewBitRateKbps,
                     Maybe<double> aFrameRate);

  nsCOMPtr<mozIGeckoMediaPluginService> mMPS;
  nsCOMPtr<nsIThread> mGMPThread;
  GMPVideoEncoderProxy* mGMP;
  Maybe<EventTargetCapability<nsISerialEventTarget>> mEncodeQueue;
  // Used to handle a race where Release() is called while init is in progress
  bool mInitting;
  uint32_t mConfiguredBitrateKbps MOZ_GUARDED_BY(mEncodeQueue);
  GMPVideoHost* mHost;
  GMPVideoCodec mCodecParams{};
  uint32_t mMaxPayloadSize;
  bool mNeedKeyframe;
  int mSyncLayerCap;
  const webrtc::CodecParameterMap mFormatParams;
  webrtc::H264BitstreamParser mH264BitstreamParser;
  std::unique_ptr<webrtc::ScalableVideoController> mSvcController;
  Mutex mCallbackMutex;
  webrtc::EncodedImageCallback* mCallback MOZ_GUARDED_BY(mCallbackMutex);
  Maybe<uint64_t> mCachedPluginId;
  const std::string mPCHandle;

  static constexpr size_t kMaxImagesInFlight = 1;
  // Map rtp time -> input image data
  AutoTArray<detail::InputImageData, kMaxImagesInFlight> mInputImageMap;

  MediaEventProducer<uint64_t> mInitPluginEvent;
  MediaEventProducer<uint64_t> mReleasePluginEvent;
};

// Basically a strong ref to a RefCountedWebrtcVideoEncoder, that also
// translates from Release() to RefCountedWebrtcVideoEncoder::Shutdown(),
// since we need RefCountedWebrtcVideoEncoder::Release() for managing the
// refcount. The webrtc.org code gets one of these, so it doesn't unilaterally
// delete the "real" encoder.
class WebrtcVideoEncoderProxy final : public WebrtcVideoEncoder {
 public:
  explicit WebrtcVideoEncoderProxy(
      RefPtr<RefCountedWebrtcVideoEncoder> aEncoder)
      : mEncoderImpl(std::move(aEncoder)) {}

  virtual ~WebrtcVideoEncoderProxy() {
    RegisterEncodeCompleteCallback(nullptr);
  }

  MediaEventSource<uint64_t>* InitPluginEvent() override {
    return mEncoderImpl->InitPluginEvent();
  }

  MediaEventSource<uint64_t>* ReleasePluginEvent() override {
    return mEncoderImpl->ReleasePluginEvent();
  }

  int32_t InitEncode(const webrtc::VideoCodec* aCodecSettings,
                     const WebrtcVideoEncoder::Settings& aSettings) override {
    return mEncoderImpl->InitEncode(aCodecSettings, aSettings);
  }

  int32_t Encode(
      const webrtc::VideoFrame& aInputImage,
      const std::vector<webrtc::VideoFrameType>* aFrameTypes) override {
    return mEncoderImpl->Encode(aInputImage, aFrameTypes);
  }

  int32_t RegisterEncodeCompleteCallback(
      webrtc::EncodedImageCallback* aCallback) override {
    return mEncoderImpl->RegisterEncodeCompleteCallback(aCallback);
  }

  int32_t Release() override { return mEncoderImpl->Shutdown(); }

  void SetRates(const RateControlParameters& aParameters) override {
    mEncoderImpl->SetRates(aParameters);
  }

  EncoderInfo GetEncoderInfo() const override {
    return mEncoderImpl->GetEncoderInfo();
  }

 private:
  const RefPtr<RefCountedWebrtcVideoEncoder> mEncoderImpl;
};

class WebrtcGmpVideoDecoder final : public GMPVideoDecoderCallbackProxy {
 public:
  WebrtcGmpVideoDecoder(std::string aPCHandle, TrackingId aTrackingId);
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(WebrtcGmpVideoDecoder, final);

  // Implement VideoEncoder interface, sort of.
  // (We cannot use |Release|, since that's needed for nsRefPtr)
  virtual bool Configure(const webrtc::VideoDecoder::Settings& settings);
  virtual int32_t Decode(const webrtc::EncodedImage& aInputImage,
                         bool aMissingFrames, int64_t aRenderTimeMs);
  virtual int32_t RegisterDecodeCompleteCallback(
      webrtc::DecodedImageCallback* aCallback);

  virtual int32_t ReleaseGmp();

  MediaEventSource<uint64_t>* InitPluginEvent() { return &mInitPluginEvent; }

  MediaEventSource<uint64_t>* ReleasePluginEvent() {
    return &mReleasePluginEvent;
  }

  // GMPVideoDecoderCallbackProxy
  void Terminated() override;

  void Decoded(GMPVideoi420Frame* aDecodedFrame) override;

  void ReceivedDecodedReferenceFrame(const uint64_t aPictureId) override {
    MOZ_CRASH();
  }

  void ReceivedDecodedFrame(const uint64_t aPictureId) override { MOZ_CRASH(); }

  void InputDataExhausted() override {}

  void DrainComplete() override {}

  void ResetComplete() override {}

  void Error(GMPErr aError) override { mDecoderStatus = aError; }

 private:
  virtual ~WebrtcGmpVideoDecoder();

  void Configure_g(const webrtc::VideoDecoder::Settings& settings);
  int32_t GmpInitDone_g(GMPVideoDecoderProxy* aGMP, GMPVideoHost* aHost,
                        std::string* aErrorOut);
  void Close_g();

  class InitDoneCallback final : public GetGMPVideoDecoderCallback {
   public:
    explicit InitDoneCallback(const RefPtr<WebrtcGmpVideoDecoder>& aDecoder)
        : mDecoder(aDecoder) {}

    void Done(GMPVideoDecoderProxy* aGMP, GMPVideoHost* aHost) override {
      std::string errorOut;
      int32_t result = mDecoder->GmpInitDone_g(aGMP, aHost, &errorOut);
      NotifyGmpInitDone(mDecoder->mPCHandle, result, errorOut);
    }

   private:
    const RefPtr<WebrtcGmpVideoDecoder> mDecoder;
  };

  void Decode_g(UniquePtr<GMPDecodeData>&& aDecodeData);

  nsCOMPtr<mozIGeckoMediaPluginService> mMPS;
  nsCOMPtr<nsIThread> mGMPThread;
  GMPVideoDecoderProxy* mGMP;  // Addref is held for us
  // Used to handle a race where Release() is called while init is in progress
  bool mInitting;
  // Frames queued for decode while mInitting is true
  nsTArray<UniquePtr<GMPDecodeData>> mQueuedFrames;
  GMPVideoHost* mHost;
  // Protects mCallback
  Mutex mCallbackMutex;
  webrtc::DecodedImageCallback* mCallback MOZ_GUARDED_BY(mCallbackMutex);
  Maybe<uint64_t> mCachedPluginId;
  Atomic<GMPErr, ReleaseAcquire> mDecoderStatus;
  const std::string mPCHandle;
  const TrackingId mTrackingId;
  PerformanceRecorderMulti<DecodeStage> mPerformanceRecorder;

  MediaEventProducer<uint64_t> mInitPluginEvent;
  MediaEventProducer<uint64_t> mReleasePluginEvent;
};

// Basically a strong ref to a WebrtcGmpVideoDecoder, that also translates
// from Release() to WebrtcGmpVideoDecoder::ReleaseGmp(), since we need
// WebrtcGmpVideoDecoder::Release() for managing the refcount.
// The webrtc.org code gets one of these, so it doesn't unilaterally delete
// the "real" encoder.
class WebrtcVideoDecoderProxy final : public WebrtcVideoDecoder {
 public:
  explicit WebrtcVideoDecoderProxy(std::string aPCHandle,
                                   TrackingId aTrackingId)
      : mDecoderImpl(new WebrtcGmpVideoDecoder(std::move(aPCHandle),
                                               std::move(aTrackingId))) {}

  virtual ~WebrtcVideoDecoderProxy() {
    RegisterDecodeCompleteCallback(nullptr);
  }

  MediaEventSource<uint64_t>* InitPluginEvent() override {
    return mDecoderImpl->InitPluginEvent();
  }

  MediaEventSource<uint64_t>* ReleasePluginEvent() override {
    return mDecoderImpl->ReleasePluginEvent();
  }

  bool Configure(const Settings& settings) override {
    return mDecoderImpl->Configure(settings);
  }

  int32_t Decode(const webrtc::EncodedImage& aInputImage, bool aMissingFrames,
                 int64_t aRenderTimeMs) override {
    return mDecoderImpl->Decode(aInputImage, aMissingFrames, aRenderTimeMs);
  }

  int32_t RegisterDecodeCompleteCallback(
      webrtc::DecodedImageCallback* aCallback) override {
    return mDecoderImpl->RegisterDecodeCompleteCallback(aCallback);
  }

  int32_t Release() override { return mDecoderImpl->ReleaseGmp(); }

 private:
  const RefPtr<WebrtcGmpVideoDecoder> mDecoderImpl;
};

}  // namespace mozilla

#endif
