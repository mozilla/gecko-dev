/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ImageDecoderReadRequest_h
#define mozilla_dom_ImageDecoderReadRequest_h

#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/ReadRequest.h"

namespace mozilla {
class MediaResult;

namespace image {
class SourceBuffer;
}

namespace dom {
class ImageDecoder;
class ReadableStream;
class ReadableStreamDefaultReader;

struct ImageDecoderReadRequest final : public ReadRequest {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(ImageDecoderReadRequest, ReadRequest)

 public:
  explicit ImageDecoderReadRequest(image::SourceBuffer* aSourceBuffer);

  bool Initialize(const GlobalObject& aGlobal, ImageDecoder* aDecoder,
                  ReadableStream& aStream);
  void Destroy(bool aCancel);

  MOZ_CAN_RUN_SCRIPT_BOUNDARY void ChunkSteps(JSContext* aCx,
                                              JS::Handle<JS::Value> aChunk,
                                              ErrorResult& aRv) override;

  MOZ_CAN_RUN_SCRIPT_BOUNDARY void CloseSteps(JSContext* aCx,
                                              ErrorResult& aRv) override;

  MOZ_CAN_RUN_SCRIPT_BOUNDARY void ErrorSteps(JSContext* aCx,
                                              JS::Handle<JS::Value> aError,
                                              ErrorResult& aRv) override;

 private:
  ~ImageDecoderReadRequest() override;

  void QueueRead();
  MOZ_CAN_RUN_SCRIPT_BOUNDARY void Read();
  MOZ_CAN_RUN_SCRIPT_BOUNDARY void Cancel();
  void Complete(const MediaResult& aResult);

  RefPtr<ImageDecoder> mDecoder;
  RefPtr<ReadableStreamDefaultReader> mReader;
  RefPtr<image::SourceBuffer> mSourceBuffer;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_ImageDecoderReadRequest_h
