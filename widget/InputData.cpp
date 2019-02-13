/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InputData.h"

#include "mozilla/dom/Touch.h"
#include "nsDebug.h"
#include "nsThreadUtils.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/TouchEvents.h"
#include "UnitTransforms.h"

namespace mozilla {

using namespace dom;

already_AddRefed<Touch> SingleTouchData::ToNewDOMTouch() const
{
  MOZ_ASSERT(NS_IsMainThread(),
             "Can only create dom::Touch instances on main thread");
  nsRefPtr<Touch> touch = new Touch(mIdentifier,
                                    LayoutDeviceIntPoint(mScreenPoint.x, mScreenPoint.y),
                                    nsIntPoint(mRadius.width, mRadius.height),
                                    mRotationAngle,
                                    mForce);
  return touch.forget();
}

MultiTouchInput::MultiTouchInput(const WidgetTouchEvent& aTouchEvent)
  : InputData(MULTITOUCH_INPUT, aTouchEvent.time, aTouchEvent.timeStamp,
              aTouchEvent.modifiers)
{
  MOZ_ASSERT(NS_IsMainThread(),
             "Can only copy from WidgetTouchEvent on main thread");

  switch (aTouchEvent.message) {
    case NS_TOUCH_START:
      mType = MULTITOUCH_START;
      break;
    case NS_TOUCH_MOVE:
      mType = MULTITOUCH_MOVE;
      break;
    case NS_TOUCH_END:
      mType = MULTITOUCH_END;
      break;
    case NS_TOUCH_CANCEL:
      mType = MULTITOUCH_CANCEL;
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Did not assign a type to a MultiTouchInput");
      break;
  }

  for (size_t i = 0; i < aTouchEvent.touches.Length(); i++) {
    const Touch* domTouch = aTouchEvent.touches[i];

    // Extract data from weird interfaces.
    int32_t identifier = domTouch->Identifier();
    int32_t radiusX = domTouch->RadiusX();
    int32_t radiusY = domTouch->RadiusY();
    float rotationAngle = domTouch->RotationAngle();
    float force = domTouch->Force();

    SingleTouchData data(identifier,
                         ScreenIntPoint::FromUnknownPoint(
                           gfx::IntPoint(domTouch->mRefPoint.x,
                                         domTouch->mRefPoint.y)),
                         ScreenSize(radiusX, radiusY),
                         rotationAngle,
                         force);

    mTouches.AppendElement(data);
  }
}

WidgetTouchEvent
MultiTouchInput::ToWidgetTouchEvent(nsIWidget* aWidget) const
{
  MOZ_ASSERT(NS_IsMainThread(),
             "Can only convert To WidgetTouchEvent on main thread");

  uint32_t touchType = NS_EVENT_NULL;
  switch (mType) {
  case MULTITOUCH_START:
    touchType = NS_TOUCH_START;
    break;
  case MULTITOUCH_MOVE:
    touchType = NS_TOUCH_MOVE;
    break;
  case MULTITOUCH_END:
    touchType = NS_TOUCH_END;
    break;
  case MULTITOUCH_CANCEL:
    touchType = NS_TOUCH_CANCEL;
    break;
  default:
    MOZ_ASSERT_UNREACHABLE("Did not assign a type to WidgetTouchEvent in MultiTouchInput");
    break;
  }

  WidgetTouchEvent event(true, touchType, aWidget);
  if (touchType == NS_EVENT_NULL) {
    return event;
  }

  event.modifiers = this->modifiers;
  event.time = this->mTime;
  event.timeStamp = this->mTimeStamp;

  for (size_t i = 0; i < mTouches.Length(); i++) {
    *event.touches.AppendElement() = mTouches[i].ToNewDOMTouch();
  }

  return event;
}

WidgetMouseEvent
MultiTouchInput::ToWidgetMouseEvent(nsIWidget* aWidget) const
{
  MOZ_ASSERT(NS_IsMainThread(),
             "Can only convert To WidgetMouseEvent on main thread");

  uint32_t mouseEventType = NS_EVENT_NULL;
  switch (mType) {
    case MultiTouchInput::MULTITOUCH_START:
      mouseEventType = NS_MOUSE_BUTTON_DOWN;
      break;
    case MultiTouchInput::MULTITOUCH_MOVE:
      mouseEventType = NS_MOUSE_MOVE;
      break;
    case MultiTouchInput::MULTITOUCH_CANCEL:
    case MultiTouchInput::MULTITOUCH_END:
      mouseEventType = NS_MOUSE_BUTTON_UP;
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Did not assign a type to WidgetMouseEvent");
      break;
  }

  WidgetMouseEvent event(true, mouseEventType, aWidget,
                         WidgetMouseEvent::eReal, WidgetMouseEvent::eNormal);

  const SingleTouchData& firstTouch = mTouches[0];
  event.refPoint.x = firstTouch.mScreenPoint.x;
  event.refPoint.y = firstTouch.mScreenPoint.y;

  event.time = mTime;
  event.button = WidgetMouseEvent::eLeftButton;
  event.inputSource = nsIDOMMouseEvent::MOZ_SOURCE_TOUCH;
  event.modifiers = modifiers;

  if (mouseEventType != NS_MOUSE_MOVE) {
    event.clickCount = 1;
  }

  return event;
}

int32_t
MultiTouchInput::IndexOfTouch(int32_t aTouchIdentifier)
{
  for (size_t i = 0; i < mTouches.Length(); i++) {
    if (mTouches[i].mIdentifier == aTouchIdentifier) {
      return (int32_t)i;
    }
  }
  return -1;
}

// This conversion from WidgetMouseEvent to MultiTouchInput is needed because on
// the B2G emulator we can only receive mouse events, but we need to be able
// to pan correctly. To do this, we convert the events into a format that the
// panning code can handle. This code is very limited and only supports
// SingleTouchData. It also sends garbage for the identifier, radius, force
// and rotation angle.
MultiTouchInput::MultiTouchInput(const WidgetMouseEvent& aMouseEvent)
  : InputData(MULTITOUCH_INPUT, aMouseEvent.time, aMouseEvent.timeStamp,
              aMouseEvent.modifiers)
{
  MOZ_ASSERT(NS_IsMainThread(),
             "Can only copy from WidgetMouseEvent on main thread");
  switch (aMouseEvent.message) {
  case NS_MOUSE_BUTTON_DOWN:
    mType = MULTITOUCH_START;
    break;
  case NS_MOUSE_MOVE:
    mType = MULTITOUCH_MOVE;
    break;
  case NS_MOUSE_BUTTON_UP:
    mType = MULTITOUCH_END;
    break;
  // The mouse pointer has been interrupted in an implementation-specific
  // manner, such as a synchronous event or action cancelling the touch, or a
  // touch point leaving the document window and going into a non-document
  // area capable of handling user interactions.
  case NS_MOUSE_EXIT_WIDGET:
    mType = MULTITOUCH_CANCEL;
    break;
  default:
    NS_WARNING("Did not assign a type to a MultiTouchInput");
    break;
  }

  mTouches.AppendElement(SingleTouchData(0,
                                         ScreenIntPoint::FromUnknownPoint(
                                           gfx::IntPoint(aMouseEvent.refPoint.x,
                                                         aMouseEvent.refPoint.y)),
                                         ScreenSize(1, 1),
                                         180.0f,
                                         1.0f));
}

void
MultiTouchInput::TransformToLocal(const gfx::Matrix4x4& aTransform)
{
  for (size_t i = 0; i < mTouches.Length(); i++) {
    mTouches[i].mLocalScreenPoint = TransformTo<ParentLayerPixel>(aTransform, ScreenPoint(mTouches[i].mScreenPoint));
  }
}

void
PanGestureInput::TransformToLocal(const gfx::Matrix4x4& aTransform)
{
  mLocalPanStartPoint = TransformTo<ParentLayerPixel>(aTransform, mPanStartPoint);
  mLocalPanDisplacement = TransformVector<ParentLayerPixel>(aTransform, mPanDisplacement, mPanStartPoint);
}

void
PinchGestureInput::TransformToLocal(const gfx::Matrix4x4& aTransform)
{
  mLocalFocusPoint = TransformTo<ParentLayerPixel>(aTransform, mFocusPoint);
}

void
TapGestureInput::TransformToLocal(const gfx::Matrix4x4& aTransform)
{
  mLocalPoint = TransformTo<ParentLayerPixel>(aTransform, mPoint);
}

void
ScrollWheelInput::TransformToLocal(const gfx::Matrix4x4& aTransform)
{
  mLocalOrigin = TransformTo<ParentLayerPixel>(aTransform, mOrigin);
}

} // namespace mozilla
