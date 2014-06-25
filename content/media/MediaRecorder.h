/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MediaRecorder_h
#define MediaRecorder_h

#include "mozilla/dom/MediaRecorderBinding.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "nsIDocumentActivity.h"

// Max size for allowing queue encoded data in memory
#define MAX_ALLOW_MEMORY_BUFFER 1024000
namespace mozilla {

class ErrorResult;
class DOMMediaStream;
class EncodedBufferCache;
class MediaEncoder;
class ProcessedMediaStream;
class MediaInputPort;
struct MediaRecorderOptions;

namespace dom {

/**
 * Implementation of https://dvcs.w3.org/hg/dap/raw-file/default/media-stream-capture/MediaRecorder.html
 * The MediaRecorder accepts a mediaStream as input source passed from UA. When recorder starts,
 * a MediaEncoder will be created and accept the mediaStream as input source.
 * Encoder will get the raw data by track data changes, encode it by selected MIME Type, then store the encoded in EncodedBufferCache object.
 * The encoded data will be extracted on every timeslice passed from Start function call or by RequestData function.
 * Thread model:
 * When the recorder starts, it creates a "Media Encoder" thread to read data from MediaEncoder object and store buffer in EncodedBufferCache object.
 * Also extract the encoded data and create blobs on every timeslice passed from start function or RequestData function called by UA.
 */

class MediaRecorder : public DOMEventTargetHelper,
                      public nsIDocumentActivity
{
  class Session;
  friend class CreateAndDispatchBlobEventRunnable;

public:
  MediaRecorder(DOMMediaStream&, nsPIDOMWindow* aOwnerWindow);
  virtual ~MediaRecorder();

  // nsWrapperCache
  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  nsPIDOMWindow* GetParentObject() { return GetOwner(); }

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(MediaRecorder,
                                           DOMEventTargetHelper)

  // WebIDL
  // Start recording. If timeSlice has been provided, mediaRecorder will
  // raise a dataavailable event containing the Blob of collected data on every timeSlice milliseconds.
  // If timeSlice isn't provided, UA should call the RequestData to obtain the Blob data, also set the mTimeSlice to zero.
  void Start(const Optional<int32_t>& timeSlice, ErrorResult & aResult);
  // Stop the recording activiy. Including stop the Media Encoder thread, un-hook the mediaStreamListener to encoder.
  void Stop(ErrorResult& aResult);
  // Pause the mTrackUnionStream
  void Pause(ErrorResult& aResult);

  void Resume(ErrorResult& aResult);
  // Extract encoded data Blob from EncodedBufferCache.
  void RequestData(ErrorResult& aResult);
  // Return the The DOMMediaStream passed from UA.
  DOMMediaStream* Stream() const { return mStream; }
  // The current state of the MediaRecorder object.
  RecordingState State() const { return mState; }
  // Return the current encoding MIME type selected by the MediaEncoder.
  void GetMimeType(nsString &aMimeType);

  static already_AddRefed<MediaRecorder>
  Constructor(const GlobalObject& aGlobal,
              DOMMediaStream& aStream,
              const MediaRecorderOptions& aInitDict,
              ErrorResult& aRv);

  // EventHandler
  IMPL_EVENT_HANDLER(dataavailable)
  IMPL_EVENT_HANDLER(error)
  IMPL_EVENT_HANDLER(stop)
  IMPL_EVENT_HANDLER(warning)

  NS_DECL_NSIDOCUMENTACTIVITY

protected:
  MediaRecorder& operator = (const MediaRecorder& x) MOZ_DELETE;
  // Create dataavailable event with Blob data and it runs in main thread
  nsresult CreateAndDispatchBlobEvent(already_AddRefed<nsIDOMBlob>&& aBlob);
  // Creating a simple event to notify UA simple event.
  void DispatchSimpleEvent(const nsAString & aStr);
  // Creating a error event with message.
  void NotifyError(nsresult aRv);
  // Check if the recorder's principal is the subsume of mediaStream
  bool CheckPrincipal();
  // Set encoded MIME type.
  void SetMimeType(const nsString &aMimeType);

  MediaRecorder(const MediaRecorder& x) MOZ_DELETE; // prevent bad usage
  // Remove session pointer.
  void RemoveSession(Session* aSession);
  // MediaStream passed from js context
  nsRefPtr<DOMMediaStream> mStream;
  // The current state of the MediaRecorder object.
  RecordingState mState;
  // Hold the sessions pointer and clean it when the DestroyRunnable for a
  // session is running.
  nsTArray<Session*> mSessions;
  // Thread safe for mMimeType.
  Mutex mMutex;
  // It specifies the container format as well as the audio and video capture formats.
  nsString mMimeType;

private:
  // Register MediaRecorder into Document to listen the activity changes.
  void RegisterActivityObserver();
  void UnRegisterActivityObserver();
};

}
}

#endif
