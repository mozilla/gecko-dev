/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_image_ImageUtils_h
#define mozilla_image_ImageUtils_h

#include "FrameTimeout.h"
#include "mozilla/image/SurfaceFlags.h"
#include "mozilla/Assertions.h"
#include "mozilla/Maybe.h"
#include "mozilla/MozPromise.h"
#include "mozilla/RefPtr.h"
#include "mozilla/ThreadSafeWeakPtr.h"
#include "nsString.h"
#include "nsTArray.h"

namespace mozilla {
class ErrorResult;

namespace gfx {
class SourceSurface;
}

namespace image {
class Decoder;
class imgFrame;
class ImageMetadata;
class SourceBuffer;

/**
 * The type of decoder; this is usually determined from a MIME type using
 * DecoderFactory::GetDecoderType() or ImageUtils::GetDecoderType().
 */
enum class DecoderType {
  PNG,
  GIF,
  JPEG,
  BMP,
  BMP_CLIPBOARD,
  ICO,
  ICON,
  WEBP,
  AVIF,
  JXL,
  UNKNOWN
};

struct DecodeMetadataResult {
  int32_t mWidth = 0;
  int32_t mHeight = 0;
  int32_t mRepetitions = -1;
  uint32_t mFrameCount = 0;
  bool mAnimated = false;
  bool mFrameCountComplete = true;
};

struct DecodeFrameCountResult {
  uint32_t mFrameCount = 0;
  bool mFinished = false;
};

struct DecodedFrame {
  RefPtr<gfx::SourceSurface> mSurface;
  FrameTimeout mTimeout;
};

struct DecodeFramesResult {
  nsTArray<DecodedFrame> mFrames;
  bool mFinished = false;
};

using DecodeMetadataPromise = MozPromise<DecodeMetadataResult, nsresult, true>;
using DecodeFrameCountPromise =
    MozPromise<DecodeFrameCountResult, nsresult, true>;
using DecodeFramesPromise = MozPromise<DecodeFramesResult, nsresult, true>;

class AnonymousMetadataDecoderTask;
class AnonymousFrameCountDecoderTask;
class AnonymousFramesDecoderTask;

class AnonymousDecoder : public SupportsThreadSafeWeakPtr<AnonymousDecoder> {
 public:
  virtual RefPtr<DecodeMetadataPromise> DecodeMetadata() = 0;

  virtual void Destroy() = 0;

  virtual RefPtr<DecodeFrameCountPromise> DecodeFrameCount(
      uint32_t aKnownFrameCount) = 0;

  virtual RefPtr<DecodeFramesPromise> DecodeFrames(size_t aCount) = 0;

  virtual void CancelDecodeFrames() = 0;

#ifdef MOZ_REFCOUNTED_LEAK_CHECKING
  virtual const char* typeName() const = 0;
  virtual size_t typeSize() const = 0;
#endif

  virtual ~AnonymousDecoder();

 protected:
  AnonymousDecoder();

  // Returns true if successfully initialized else false.
  virtual bool Initialize(RefPtr<Decoder>&& aDecoder) = 0;

  virtual void OnMetadata(const ImageMetadata* aMetadata) = 0;

  virtual void OnFrameCount(uint32_t aFrameCount, bool aComplete) = 0;

  // Returns true if the caller should continue decoding more frames if
  // possible.
  virtual bool OnFrameAvailable(RefPtr<imgFrame>&& aFrame,
                                RefPtr<gfx::SourceSurface>&& aSurface) = 0;

  virtual void OnFramesComplete() = 0;

  friend class AnonymousMetadataDecoderTask;
  friend class AnonymousFrameCountDecoderTask;
  friend class AnonymousFramesDecoderTask;
};

class ImageUtils {
 public:
  static already_AddRefed<AnonymousDecoder> CreateDecoder(
      SourceBuffer* aSourceBuffer, DecoderType aType,
      const Maybe<gfx::IntSize>& aOutputSize, SurfaceFlags aSurfaceFlags);

  static DecoderType GetDecoderType(const nsACString& aMimeType);

 private:
  ImageUtils() = delete;
  ~ImageUtils() = delete;
};

}  // namespace image
}  // namespace mozilla

#endif  // mozilla_image_ImageUtils_h
