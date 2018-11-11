/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/Touch.h"

#include "mozilla/dom/EventTarget.h"
#include "mozilla/dom/TouchEvent.h"
#include "nsGlobalWindow.h"
#include "nsContentUtils.h"
#include "nsIContent.h"

namespace mozilla {
namespace dom {

// static
already_AddRefed<Touch>
Touch::Constructor(const GlobalObject& aGlobal,
                   const TouchInit& aParam,
                   ErrorResult& aRv)
{
  // Annoyingly many parameters, make sure the ordering is the same as in the
  // Touch constructor.
  RefPtr<Touch> touch = new Touch(aParam.mTarget,
                                  aParam.mIdentifier,
                                  aParam.mPageX,
                                  aParam.mPageY,
                                  aParam.mScreenX,
                                  aParam.mScreenY,
                                  aParam.mClientX,
                                  aParam.mClientY,
                                  aParam.mRadiusX,
                                  aParam.mRadiusY,
                                  aParam.mRotationAngle,
                                  aParam.mForce);
  return touch.forget();
}

Touch::Touch(EventTarget* aTarget,
             int32_t aIdentifier,
             int32_t aPageX,
             int32_t aPageY,
             int32_t aScreenX,
             int32_t aScreenY,
             int32_t aClientX,
             int32_t aClientY,
             int32_t aRadiusX,
             int32_t aRadiusY,
             float aRotationAngle,
             float aForce)
{
  mTarget = aTarget;
  mIdentifier = aIdentifier;
  mPagePoint = CSSIntPoint(aPageX, aPageY);
  mScreenPoint = CSSIntPoint(aScreenX, aScreenY);
  mClientPoint = CSSIntPoint(aClientX, aClientY);
  mRefPoint = LayoutDeviceIntPoint(0, 0);
  mPointsInitialized = true;
  mRadius.x = aRadiusX;
  mRadius.y = aRadiusY;
  mRotationAngle = aRotationAngle;
  mForce = aForce;

  mChanged = false;
  mMessage = 0;
  nsJSContext::LikelyShortLivingObjectCreated();
}

Touch::Touch(int32_t aIdentifier,
             LayoutDeviceIntPoint aPoint,
             LayoutDeviceIntPoint aRadius,
             float aRotationAngle,
             float aForce)
{
  mIdentifier = aIdentifier;
  mPagePoint = CSSIntPoint(0, 0);
  mScreenPoint = CSSIntPoint(0, 0);
  mClientPoint = CSSIntPoint(0, 0);
  mRefPoint = aPoint;
  mPointsInitialized = false;
  mRadius = aRadius;
  mRotationAngle = aRotationAngle;
  mForce = aForce;

  mChanged = false;
  mMessage = 0;
  nsJSContext::LikelyShortLivingObjectCreated();
}

Touch::Touch(const Touch& aOther)
  : mTarget(aOther.mTarget)
  , mRefPoint(aOther.mRefPoint)
  , mChanged(aOther.mChanged)
  , mMessage(aOther.mMessage)
  , mIdentifier(aOther.mIdentifier)
  , mPagePoint(aOther.mPagePoint)
  , mClientPoint(aOther.mClientPoint)
  , mScreenPoint(aOther.mScreenPoint)
  , mRadius(aOther.mRadius)
  , mRotationAngle(aOther.mRotationAngle)
  , mForce(aOther.mForce)
  , mPointsInitialized(aOther.mPointsInitialized)
{
  nsJSContext::LikelyShortLivingObjectCreated();
}

Touch::~Touch()
{
}

// static
bool
Touch::PrefEnabled(JSContext* aCx, JSObject* aGlobal)
{
  return TouchEvent::PrefEnabled(aCx, aGlobal);
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(Touch, mTarget)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Touch)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(Touch)
NS_IMPL_CYCLE_COLLECTING_RELEASE(Touch)

EventTarget*
Touch::GetTarget() const
{
  nsCOMPtr<nsIContent> content = do_QueryInterface(mTarget);
  if (content && content->ChromeOnlyAccess() &&
      !nsContentUtils::LegacyIsCallerNativeCode() &&
      !nsContentUtils::CanAccessNativeAnon()) {
    return content->FindFirstNonChromeOnlyAccessContent();
  }

  return mTarget;
}

void
Touch::InitializePoints(nsPresContext* aPresContext, WidgetEvent* aEvent)
{
  if (mPointsInitialized) {
    return;
  }
  mClientPoint = Event::GetClientCoords(
    aPresContext, aEvent, mRefPoint, mClientPoint);
  mPagePoint = Event::GetPageCoords(
    aPresContext, aEvent, mRefPoint, mClientPoint);
  mScreenPoint = Event::GetScreenCoords(aPresContext, aEvent, mRefPoint);
  mPointsInitialized = true;
}

void
Touch::SetTarget(EventTarget* aTarget)
{
  mTarget = aTarget;
}

bool
Touch::Equals(Touch* aTouch)
{
  return mRefPoint == aTouch->mRefPoint &&
         mForce == aTouch->Force() &&
         mRotationAngle == aTouch->RotationAngle() &&
         mRadius.x == aTouch->RadiusX() &&
         mRadius.y == aTouch->RadiusY();
}

JSObject*
Touch::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return TouchBinding::Wrap(aCx, this, aGivenProto);
}

// Parent ourselves to the global of the target. This achieves the desirable
// effects of parenting to the target, but avoids making the touch inaccessible
// when the target happens to be NAC and therefore reflected into the XBL scope.
nsIGlobalObject*
Touch::GetParentObject()
{
  if (!mTarget) {
    return nullptr;
  }
  return mTarget->GetOwnerGlobal();
}

} // namespace dom
} // namespace mozilla
