/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ViewTransition_h
#define mozilla_dom_ViewTransition_h

#include "mozilla/layers/IpcResourceUpdateQueue.h"
#include "nsRect.h"
#include "nsWrapperCache.h"
#include "nsAtomHashKeys.h"
#include "nsClassHashtable.h"
#include "nsRefPtrHashtable.h"

class nsIGlobalObject;
class nsITimer;

namespace mozilla {

class ErrorResult;
struct Keyframe;
struct PseudoStyleRequest;
struct StyleLockedDeclarationBlock;

namespace gfx {
class DataSourceSurface;
}

namespace layers {
class RenderRootStateManager;
}

namespace wr {
struct ImageKey;
class IpcResourceUpdateQueue;
}  // namespace wr

namespace dom {

class Document;
class Element;
class Promise;
class ViewTransitionUpdateCallback;

enum class SkipTransitionReason : uint8_t {
  JS,
  DocumentHidden,
  ClobberedActiveTransition,
  Timeout,
  UpdateCallbackRejected,
  DuplicateTransitionNameCapturingOldState,
  DuplicateTransitionNameCapturingNewState,
  PseudoUpdateFailure,
  Resize,
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
  MOZ_CAN_RUN_SCRIPT void PerformPendingOperations();

  Element* GetRoot() const { return mViewTransitionRoot; }
  Maybe<nsSize> GetOldSize(nsAtom* aName) const;
  Maybe<nsSize> GetNewSize(nsAtom* aName) const;
  const wr::ImageKey* GetOldImageKey(nsAtom* aName,
                                     layers::RenderRootStateManager*,
                                     wr::IpcResourceUpdateQueue&) const;
  const wr::ImageKey* GetNewImageKey(nsAtom* aName) const;
  const wr::ImageKey* GetImageKeyForCapturedFrame(
      nsIFrame* aFrame, layers::RenderRootStateManager*,
      wr::IpcResourceUpdateQueue&) const;

  Element* FindPseudo(const PseudoStyleRequest&) const;

  const StyleLockedDeclarationBlock* GetDynamicRuleFor(const Element&) const;

  static constexpr nsLiteralString kGroupAnimPrefix =
      u"-ua-view-transition-group-anim-"_ns;

  [[nodiscard]] bool GetGroupKeyframes(nsAtom* aAnimationName,
                                       nsTArray<Keyframe>&) const;

  nsIGlobalObject* GetParentObject() const;
  JSObject* WrapObject(JSContext*, JS::Handle<JSObject*> aGivenProto) override;

  struct CapturedElement;

  MOZ_CAN_RUN_SCRIPT void CallUpdateCallback(ErrorResult&);

 private:
  MOZ_CAN_RUN_SCRIPT void MaybeScheduleUpdateCallback();
  void Activate();

  void ClearActiveTransition(bool aIsDocumentHidden);
  void Timeout();
  MOZ_CAN_RUN_SCRIPT void Setup();
  [[nodiscard]] Maybe<SkipTransitionReason> CaptureOldState();
  [[nodiscard]] Maybe<SkipTransitionReason> CaptureNewState();
  void SetupTransitionPseudoElements();
  [[nodiscard]] bool UpdatePseudoElementStyles(bool aNeedsInvalidation);
  void ClearNamedElements();
  void HandleFrame();
  bool CheckForActiveAnimations() const;
  void SkipTransition(SkipTransitionReason, JS::Handle<JS::Value>);
  void ClearTimeoutTimer();

  nsRect SnapshotContainingBlockRect() const;

  ~ViewTransition();

  // Stored for the whole lifetime of the object (until CC).
  RefPtr<Document> mDocument;
  RefPtr<ViewTransitionUpdateCallback> mUpdateCallback;

  // https://drafts.csswg.org/css-view-transitions/#viewtransition-named-elements
  using NamedElements = nsClassHashtable<nsAtomHashKey, CapturedElement>;
  NamedElements mNamedElements;
  // mNamedElements is an unordered map, we need to keep the tree order.
  AutoTArray<RefPtr<nsAtom>, 8> mNames;

  // https://drafts.csswg.org/css-view-transitions/#viewtransition-initial-snapshot-containing-block-size
  nsSize mInitialSnapshotContainingBlockSize;

  // Allocated lazily, but same object once allocated (again until CC).
  RefPtr<Promise> mUpdateCallbackDonePromise;
  RefPtr<Promise> mReadyPromise;
  RefPtr<Promise> mFinishedPromise;

  static void TimeoutCallback(nsITimer*, void*);
  RefPtr<nsITimer> mTimeoutTimer;

  Phase mPhase = Phase::PendingCapture;
  RefPtr<Element> mViewTransitionRoot;
};

}  // namespace dom
}  // namespace mozilla

#endif
