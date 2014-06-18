/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef OMXCodecWrapper_h_
#define OMXCodecWrapper_h_

#include <gui/Surface.h>
#include <utils/RefBase.h>
#include <stagefright/foundation/ABuffer.h>
#include <stagefright/foundation/AMessage.h>
#include <stagefright/MediaCodec.h>

#include "AudioSegment.h"
#include "GonkNativeWindow.h"
#include "GonkNativeWindowClient.h"

#include "IMediaResourceManagerService.h"
#include "MediaResourceManagerClient.h"

#include <speex/speex_resampler.h>

namespace android {

// Wrapper class for managing HW codec reservations
class OMXCodecReservation : public MediaResourceManagerClient::EventListener
{
public:
  OMXCodecReservation(bool aEncoder)
  {
    mType = aEncoder ? IMediaResourceManagerService::HW_VIDEO_ENCODER :
            IMediaResourceManagerService::HW_VIDEO_DECODER;
  }

  virtual ~OMXCodecReservation()
  {
    ReleaseOMXCodec();
  }

  /** Reserve the Encode or Decode resource for this instance */
  virtual bool ReserveOMXCodec();

  /** Release the Encode or Decode resource for this instance */
  virtual void ReleaseOMXCodec();

  // MediaResourceManagerClient::EventListener
  virtual void statusChanged(int event) {}

private:
  IMediaResourceManagerService::ResourceType mType;

  sp<MediaResourceManagerClient> mClient;
  sp<IMediaResourceManagerService> mManagerService;
};


class OMXAudioEncoder;
class OMXVideoEncoder;

/**
 * This class (and its subclasses) wraps the video and audio codec from
 * MediaCodec API in libstagefright. Currently only AVC/H.264 video encoder and
 * AAC audio encoder are supported.
 *
 * OMXCodecWrapper has static creator functions that returns actual codec
 * instances for different types of codec supported and serves as superclass to
 * provide a function to read encoded data as byte array from codec. Two
 * subclasses, OMXAudioEncoder and OMXVideoEncoder, respectively provides
 * functions for encoding data from audio and video track.
 *
 * A typical usage is as follows:
 * - Call one of the creator function Create...() to get either a
 *   OMXAudioEncoder or OMXVideoEncoder object.
 * - Configure codec by providing characteristics of input raw data, such as
 *   video frame width and height, using Configure().
 * - Send raw data (and notify end of stream) with Encode().
 * - Get encoded data through GetNextEncodedFrame().
 * - Repeat previous 2 steps until end of stream.
 * - Destroy the object.
 *
 * The lifecycle of underlying OMX codec is binded with construction and
 * destruction of OMXCodecWrapper and subclass objects. For some types of
 * codecs, such as HW accelerated AVC/H.264 encoder, there can be only one
 * instance system-wise at a time, attempting to create another instance will
 * fail.
 */
class OMXCodecWrapper
{
public:
  // Codec types.
  enum CodecType {
    AAC_ENC, // AAC encoder.
    AMR_NB_ENC, // AMR_NB encoder.
    AVC_ENC, // AVC/H.264 encoder.
    TYPE_COUNT
  };

  // Input and output flags.
  enum {
    // For Encode() and Encode, it indicates the end of input stream;
    // For GetNextEncodedFrame(), it indicates the end of output
    // stream.
    BUFFER_EOS = MediaCodec::BUFFER_FLAG_EOS,
    // For GetNextEncodedFrame(). It indicates the output buffer is an I-frame.
    BUFFER_SYNC_FRAME = MediaCodec::BUFFER_FLAG_SYNCFRAME,
    // For GetNextEncodedFrame(). It indicates that the output buffer contains
    // codec specific configuration info. (SPS & PPS for AVC/H.264;
    // DecoderSpecificInfo for AAC)
    BUFFER_CODEC_CONFIG = MediaCodec::BUFFER_FLAG_CODECCONFIG,
  };

  // Hard-coded values for AAC DecoderConfigDescriptor in libstagefright.
  // See MPEG4Writer::Track::writeMp4aEsdsBox()
  // Exposed for the need of MP4 container writer.
  enum {
    kAACBitrate = 96000,      // kbps
    kAACFrameSize = 768,      // bytes
    kAACFrameDuration = 1024, // How many samples per AAC frame.
  };

  /** Create a AAC audio encoder. Returns nullptr when failed. */
  static OMXAudioEncoder* CreateAACEncoder();

  /** Create a AMR audio encoder. Returns nullptr when failed. */
  static OMXAudioEncoder* CreateAMRNBEncoder();

  /** Create a AVC/H.264 video encoder. Returns nullptr when failed. */
  static OMXVideoEncoder* CreateAVCEncoder();

  virtual ~OMXCodecWrapper();

  /**
   * Get the next available encoded data from MediaCodec. The data will be
   * copied into aOutputBuf array, with its timestamp (in microseconds) in
   * aOutputTimestamp.
   * Wait at most aTimeout microseconds to dequeue a output buffer.
   */
  nsresult GetNextEncodedFrame(nsTArray<uint8_t>* aOutputBuf,
                               int64_t* aOutputTimestamp, int* aOutputFlags,
                               int64_t aTimeOut);
  /*
   * Get the codec type
   */
  int GetCodecType() { return mCodecType; }
protected:
  /**
   * See whether the object has been initialized successfully and is ready to
   * use.
   */
  virtual bool IsValid() { return mCodec != nullptr; }

  /**
   * Construct codec specific configuration blob with given data aData generated
   * by media codec and append it into aOutputBuf. Needed by MP4 container
   * writer for generating decoder config box, or WebRTC for generating RTP
   * packets. Returns OK if succeed.
   */
  virtual status_t AppendDecoderConfig(nsTArray<uint8_t>* aOutputBuf,
                                       ABuffer* aData) = 0;

  /**
   * Append encoded frame data generated by media codec (stored in aData and
   * is aSize bytes long) into aOutputBuf. Subclasses can override this function
   * to process the data for specific container writer.
   */
  virtual void AppendFrame(nsTArray<uint8_t>* aOutputBuf,
                           const uint8_t* aData, size_t aSize)
  {
    aOutputBuf->AppendElements(aData, aSize);
  }

private:
  // Hide these. User should always use creator functions to get a media codec.
  OMXCodecWrapper() MOZ_DELETE;
  OMXCodecWrapper(const OMXCodecWrapper&) MOZ_DELETE;
  OMXCodecWrapper& operator=(const OMXCodecWrapper&) MOZ_DELETE;

  /**
   * Create a media codec of given type. It will be a AVC/H.264 video encoder if
   * aCodecType is CODEC_AVC_ENC, or AAC audio encoder if aCodecType is
   * CODEC_AAC_ENC.
   */
  OMXCodecWrapper(CodecType aCodecType);

  // For subclasses to access hidden constructor and implementation details.
  friend class OMXAudioEncoder;
  friend class OMXVideoEncoder;

  /**
   * Start the media codec.
   */
  status_t Start();

  /**
   * Stop the media codec.
   */
  status_t Stop();

  // The actual codec instance provided by libstagefright.
  sp<MediaCodec> mCodec;

  // A dedicate message loop with its own thread used by MediaCodec.
  sp<ALooper> mLooper;

  Vector<sp<ABuffer> > mInputBufs;  // MediaCodec buffers to hold input data.
  Vector<sp<ABuffer> > mOutputBufs; // MediaCodec buffers to hold output data.

  int mCodecType;
  bool mStarted; // Has MediaCodec been started?
  bool mAMRCSDProvided;
};

/**
 * Audio encoder.
 */
class OMXAudioEncoder MOZ_FINAL : public OMXCodecWrapper
{
public:
  /**
   * Configure audio codec parameters and start media codec. It must be called
   * before calling Encode() and GetNextEncodedFrame().
   * aReSamplingRate = 0 means no resampler required
   */
  nsresult Configure(int aChannelCount, int aInputSampleRate, int aEncodedSampleRate);

  /**
   * Encode 16-bit PCM audio samples stored in aSegment. To notify end of
   * stream, set aInputFlags to BUFFER_EOS. Since encoder has limited buffers,
   * this function might not be able to encode all chunks in one call, however
   * it will remove chunks it consumes from aSegment.
   */
  nsresult Encode(mozilla::AudioSegment& aSegment, int aInputFlags = 0);

  ~OMXAudioEncoder();
protected:
  virtual status_t AppendDecoderConfig(nsTArray<uint8_t>* aOutputBuf,
                                       ABuffer* aData) MOZ_OVERRIDE;
private:
  // Hide these. User should always use creator functions to get a media codec.
  OMXAudioEncoder() MOZ_DELETE;
  OMXAudioEncoder(const OMXAudioEncoder&) MOZ_DELETE;
  OMXAudioEncoder& operator=(const OMXAudioEncoder&) MOZ_DELETE;

  /**
   * Create a audio codec. It will be a AAC encoder if aCodecType is
   * CODEC_AAC_ENC.
   */
  OMXAudioEncoder(CodecType aCodecType)
    : OMXCodecWrapper(aCodecType)
    , mResampler(nullptr)
    , mChannels(0)
    , mTimestamp(0)
    , mSampleDuration(0)
    , mResamplingRatio(0) {}

  // For creator function to access hidden constructor.
  friend class OMXCodecWrapper;

  /**
   * If the input sample rate does not divide 48kHz evenly, the input data are
   * resampled.
   */
  SpeexResamplerState* mResampler;
  // Number of audio channels.
  size_t mChannels;

  float mResamplingRatio;
  // The total duration of audio samples that have been encoded in microseconds.
  int64_t mTimestamp;
  // Time per audio sample in microseconds.
  int64_t mSampleDuration;
};

/**
 * Video encoder.
 */
class OMXVideoEncoder MOZ_FINAL : public OMXCodecWrapper
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(OMXVideoEncoder)
public:
  // Types of output blob format.
  enum BlobFormat {
    AVC_MP4, // MP4 file config descripter (defined in ISO/IEC 14496-15 5.2.4.1.1)
    AVC_NAL  // NAL (Network Abstract Layer) (defined in ITU-T H.264 7.4.1)
  };

  /**
   * Configure video codec parameters and start media codec. It must be called
   * before calling Encode() and GetNextEncodedFrame().
   * aBlobFormat specifies output blob format provided by encoder. It can be
   * AVC_MP4 or AVC_NAL.
   * Configure sets up most format value to values appropriate for camera use.
   * ConfigureDirect lets the caller determine all the defaults.
   */
  nsresult Configure(int aWidth, int aHeight, int aFrameRate,
                     BlobFormat aBlobFormat = BlobFormat::AVC_MP4);
  nsresult ConfigureDirect(sp<AMessage>& aFormat,
                           BlobFormat aBlobFormat = BlobFormat::AVC_MP4);

  /**
   * Encode a aWidth pixels wide and aHeight pixels tall video frame of
   * semi-planar YUV420 format stored in the buffer of aImage. aTimestamp gives
   * the frame timestamp/presentation time (in microseconds). To notify end of
   * stream, set aInputFlags to BUFFER_EOS.
   */
  nsresult Encode(const mozilla::layers::Image* aImage, int aWidth, int aHeight,
                  int64_t aTimestamp, int aInputFlags = 0);

#if ANDROID_VERSION >= 18
  /** Set encoding bitrate (in kbps). */
  nsresult SetBitrate(int32_t aKbps);
#endif

  /**
   * Ask codec to generate an instantaneous decoding refresh (IDR) frame
   * (defined in ISO/IEC 14496-10).
   */
  nsresult RequestIDRFrame();

protected:
  virtual status_t AppendDecoderConfig(nsTArray<uint8_t>* aOutputBuf,
                                       ABuffer* aData) MOZ_OVERRIDE;

  // If configured to output MP4 format blob, AVC/H.264 encoder has to replace
  // NAL unit start code with the unit length as specified in
  // ISO/IEC 14496-15 5.2.3.
  virtual void AppendFrame(nsTArray<uint8_t>* aOutputBuf,
                           const uint8_t* aData, size_t aSize) MOZ_OVERRIDE;

private:
  // Hide these. User should always use creator functions to get a media codec.
  OMXVideoEncoder() MOZ_DELETE;
  OMXVideoEncoder(const OMXVideoEncoder&) MOZ_DELETE;
  OMXVideoEncoder& operator=(const OMXVideoEncoder&) MOZ_DELETE;

  /**
   * Create a video codec. It will be a AVC/H.264 encoder if aCodecType is
   * CODEC_AVC_ENC.
   */
  OMXVideoEncoder(CodecType aCodecType)
    : OMXCodecWrapper(aCodecType)
    , mWidth(0)
    , mHeight(0)
    , mBlobFormat(BlobFormat::AVC_MP4)
  {}

  // For creator function to access hidden constructor.
  friend class OMXCodecWrapper;

  int mWidth;
  int mHeight;
  BlobFormat mBlobFormat;
};

} // namespace android
#endif // OMXCodecWrapper_h_
