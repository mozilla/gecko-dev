/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SimpleGestureEvent.h"

#include "mozilla/dom/MouseEventBinding.h"
#include "mozilla/TouchEvents.h"

namespace mozilla::dom {

SimpleGestureEvent::SimpleGestureEvent(EventTarget* aOwner,
                                       nsPresContext* aPresContext,
                                       WidgetSimpleGestureEvent* aEvent)
    : MouseEvent(
          aOwner, aPresContext,
          aEvent ? aEvent
                 : new WidgetSimpleGestureEvent(false, eVoidEvent, nullptr)) {
  NS_ASSERTION(mEvent->mClass == eSimpleGestureEventClass,
               "event type mismatch");

  if (aEvent) {
    mEventIsInternal = false;
  } else {
    mEventIsInternal = true;
    mEvent->mRefPoint = LayoutDeviceIntPoint(0, 0);
    static_cast<WidgetMouseEventBase*>(mEvent)->mInputSource =
        MouseEvent_Binding::MOZ_SOURCE_UNKNOWN;
  }
}

uint32_t SimpleGestureEvent::AllowedDirections() const {
  return mEvent->AsSimpleGestureEvent()->mAllowedDirections;
}

void SimpleGestureEvent::SetAllowedDirections(uint32_t aAllowedDirections) {
  mEvent->AsSimpleGestureEvent()->mAllowedDirections = aAllowedDirections;
}

uint32_t SimpleGestureEvent::Direction() const {
  return mEvent->AsSimpleGestureEvent()->mDirection;
}

double SimpleGestureEvent::Delta() const {
  return mEvent->AsSimpleGestureEvent()->mDelta;
}

uint32_t SimpleGestureEvent::ClickCount() const {
  return mEvent->AsSimpleGestureEvent()->mClickCount;
}

void SimpleGestureEvent::InitSimpleGestureEventInternal(
    const nsAString& aTypeArg, bool aCanBubbleArg, bool aCancelableArg,
    nsGlobalWindowInner* aViewArg, int32_t aDetailArg, double aScreenX,
    double aScreenY, double aClientX, double aClientY, bool aCtrlKeyArg,
    bool aAltKeyArg, bool aShiftKeyArg, bool aMetaKeyArg, uint16_t aButton,
    EventTarget* aRelatedTarget, uint32_t aAllowedDirectionsArg,
    uint32_t aDirectionArg, double aDeltaArg, uint32_t aClickCountArg) {
  NS_ENSURE_TRUE_VOID(!mEvent->mFlags.mIsBeingDispatched);

  MouseEvent::InitMouseEventInternal(
      aTypeArg, aCanBubbleArg, aCancelableArg, aViewArg, aDetailArg, aScreenX,
      aScreenY, aClientX, aClientY, aCtrlKeyArg, aAltKeyArg, aShiftKeyArg,
      aMetaKeyArg, aButton, aRelatedTarget);

  WidgetSimpleGestureEvent* simpleGestureEvent = mEvent->AsSimpleGestureEvent();
  simpleGestureEvent->mAllowedDirections = aAllowedDirectionsArg;
  simpleGestureEvent->mDirection = aDirectionArg;
  simpleGestureEvent->mDelta = aDeltaArg;
  simpleGestureEvent->mClickCount = aClickCountArg;
}

}  // namespace mozilla::dom

using namespace mozilla;
using namespace mozilla::dom;

already_AddRefed<SimpleGestureEvent> NS_NewDOMSimpleGestureEvent(
    EventTarget* aOwner, nsPresContext* aPresContext,
    WidgetSimpleGestureEvent* aEvent) {
  RefPtr<SimpleGestureEvent> it =
      new SimpleGestureEvent(aOwner, aPresContext, aEvent);
  return it.forget();
}
