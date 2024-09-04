/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ViewTransition_h
#define mozilla_dom_ViewTransition_h

#include "nsWrapperCache.h"

class nsIGlobalObject;
class nsITimer;

namespace mozilla {

class ErrorResult;

namespace dom {

class Document;
class Promise;
class ViewTransitionUpdateCallback;

enum class SkipTransitionReason : uint8_t {
  JS,
  DocumentHidden,
  ClobberedActiveTransition,
  Timeout,
  UpdateCallbackRejected,
};

// https://drafts.csswg.org/css-view-transitions-1/#viewtransition-phase
enum class ViewTransitionPhase : uint8_t {
  PendingCapture = 0,
  UpdateCallbackCalled,
  Animating,
  Done,
};

class ViewTransition final : public nsISupports, public nsWrapperCache {
 public:
  using Phase = ViewTransitionPhase;

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(ViewTransition)

  ViewTransition(Document&, ViewTransitionUpdateCallback*);

  Promise* GetUpdateCallbackDone(ErrorResult&);
  Promise* GetReady(ErrorResult&);
  Promise* GetFinished(ErrorResult&);

  void SkipTransition(SkipTransitionReason = SkipTransitionReason::JS);
  void PerformPendingOperations();

  nsIGlobalObject* GetParentObject() const;
  JSObject* WrapObject(JSContext*, JS::Handle<JSObject*> aGivenProto) override;

 private:
  enum class CallIfDone : bool { No, Yes };
  MOZ_CAN_RUN_SCRIPT void CallUpdateCallbackIgnoringErrors(CallIfDone);
  MOZ_CAN_RUN_SCRIPT void CallUpdateCallback(ErrorResult&);
  void Activate();

  void ClearActiveTransition();
  void Timeout();
  void Setup();
  void HandleFrame();
  void SkipTransition(SkipTransitionReason, JS::Handle<JS::Value>);
  void ClearTimeoutTimer();

  ~ViewTransition();

  // Stored for the whole lifetime of the object (until CC).
  RefPtr<Document> mDocument;
  RefPtr<ViewTransitionUpdateCallback> mUpdateCallback;

  // Allocated lazily, but same object once allocated (again until CC).
  RefPtr<Promise> mUpdateCallbackDonePromise;
  RefPtr<Promise> mReadyPromise;
  RefPtr<Promise> mFinishedPromise;

  static void TimeoutCallback(nsITimer*, void*);
  RefPtr<nsITimer> mTimeoutTimer;

  Phase mPhase = Phase::PendingCapture;
};

}  // namespace dom
}  // namespace mozilla

#endif
