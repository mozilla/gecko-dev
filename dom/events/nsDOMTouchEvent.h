/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsDOMTouchEvent_h_
#define nsDOMTouchEvent_h_

#include "nsDOMUIEvent.h"
#include "nsTArray.h"
#include "mozilla/Attributes.h"
#include "mozilla/EventForwards.h"
#include "nsJSEnvironment.h"
#include "mozilla/dom/Touch.h"
#include "mozilla/dom/TouchEventBinding.h"
#include "nsWrapperCache.h"


class nsAString;

class nsDOMTouchList MOZ_FINAL : public nsISupports
                               , public nsWrapperCache
{
  typedef mozilla::dom::Touch Touch;

public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(nsDOMTouchList)

  nsDOMTouchList(nsISupports* aParent)
    : mParent(aParent)
  {
    SetIsDOMBinding();
    nsJSContext::LikelyShortLivingObjectCreated();
  }
  nsDOMTouchList(nsISupports* aParent,
                 const nsTArray< nsRefPtr<Touch> >& aTouches)
    : mParent(aParent)
    , mPoints(aTouches)
  {
    SetIsDOMBinding();
    nsJSContext::LikelyShortLivingObjectCreated();
  }

  void Append(Touch* aPoint)
  {
    mPoints.AppendElement(aPoint);
  }

  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

  nsISupports* GetParentObject() const
  {
    return mParent;
  }

  static bool PrefEnabled();

  uint32_t Length() const
  {
    return mPoints.Length();
  }
  Touch* Item(uint32_t aIndex) const
  {
    return mPoints.SafeElementAt(aIndex);
  }
  Touch* IndexedGetter(uint32_t aIndex, bool& aFound) const
  {
    aFound = aIndex < mPoints.Length();
    if (!aFound) {
      return nullptr;
    }
    return mPoints[aIndex];
  }
  Touch* IdentifiedTouch(int32_t aIdentifier) const;

protected:
  nsCOMPtr<nsISupports> mParent;
  nsTArray< nsRefPtr<Touch> > mPoints;
};

class nsDOMTouchEvent : public nsDOMUIEvent
{
public:
  nsDOMTouchEvent(mozilla::dom::EventTarget* aOwner,
                  nsPresContext* aPresContext,
                  mozilla::WidgetTouchEvent* aEvent);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsDOMTouchEvent, nsDOMUIEvent)

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aScope) MOZ_OVERRIDE
  {
    return mozilla::dom::TouchEventBinding::Wrap(aCx, aScope, this);
  }

  nsDOMTouchList* Touches();
  nsDOMTouchList* TargetTouches();
  nsDOMTouchList* ChangedTouches();

  bool AltKey();
  bool MetaKey();
  bool CtrlKey();
  bool ShiftKey();

  void InitTouchEvent(const nsAString& aType,
                      bool aCanBubble,
                      bool aCancelable,
                      nsIDOMWindow* aView,
                      int32_t aDetail,
                      bool aCtrlKey,
                      bool aAltKey,
                      bool aShiftKey,
                      bool aMetaKey,
                      nsDOMTouchList* aTouches,
                      nsDOMTouchList* aTargetTouches,
                      nsDOMTouchList* aChangedTouches,
                      mozilla::ErrorResult& aRv);

  static bool PrefEnabled();
protected:
  nsRefPtr<nsDOMTouchList> mTouches;
  nsRefPtr<nsDOMTouchList> mTargetTouches;
  nsRefPtr<nsDOMTouchList> mChangedTouches;
};

#endif /* !defined(nsDOMTouchEvent_h_) */
