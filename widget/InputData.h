/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef InputData_h__
#define InputData_h__

#include "nsDebug.h"
#include "nsPoint.h"
#include "nsTArray.h"
#include "Units.h"
#include "mozilla/EventForwards.h"
#include "mozilla/TimeStamp.h"

namespace mozilla {


enum InputType
{
  MULTITOUCH_INPUT,
  PANGESTURE_INPUT,
  PINCHGESTURE_INPUT,
  TAPGESTURE_INPUT
};

class MultiTouchInput;
class PanGestureInput;
class PinchGestureInput;
class TapGestureInput;

// This looks unnecessary now, but as we add more and more classes that derive
// from InputType (eventually probably almost as many as *Events.h has), it
// will be more and more clear what's going on with a macro that shortens the
// definition of the RTTI functions.
#define INPUTDATA_AS_CHILD_TYPE(type, enumID) \
  const type& As##type() const \
  { \
    NS_ABORT_IF_FALSE(mInputType == enumID, "Invalid cast of InputData."); \
    return (const type&) *this; \
  }

/** Base input data class. Should never be instantiated. */
class InputData
{
public:
  InputType mInputType;
  // Time in milliseconds that this data is relevant to. This only really
  // matters when this data is used as an event. We use uint32_t instead of
  // TimeStamp because it is easier to convert from WidgetInputEvent. The time
  // is platform-specific but it in the case of B2G and Fennec it is since
  // startup.
  uint32_t mTime;
  // Set in parallel to mTime until we determine it is safe to drop
  // platform-specific event times (see bug 77992).
  TimeStamp mTimeStamp;

  Modifiers modifiers;

  INPUTDATA_AS_CHILD_TYPE(MultiTouchInput, MULTITOUCH_INPUT)
  INPUTDATA_AS_CHILD_TYPE(PanGestureInput, PANGESTURE_INPUT)
  INPUTDATA_AS_CHILD_TYPE(PinchGestureInput, PINCHGESTURE_INPUT)
  INPUTDATA_AS_CHILD_TYPE(TapGestureInput, TAPGESTURE_INPUT)

  InputData()
  {
  }

protected:
  InputData(InputType aInputType, uint32_t aTime, TimeStamp aTimeStamp,
            Modifiers aModifiers)
    : mInputType(aInputType),
      mTime(aTime),
      mTimeStamp(aTimeStamp),
      modifiers(aModifiers)
  {


  }
};

/**
 * Data container for a single touch input. Similar to dom::Touch, but used in
 * off-main-thread situations. This is more for just storing touch data, whereas
 * dom::Touch is more useful for dispatching through the DOM (which can only
 * happen on the main thread). dom::Touch also bears the problem of storing
 * pointers to nsIWidget instances which can only be used on the main thread,
 * so if instead we used dom::Touch and ever set these pointers
 * off-main-thread, Bad Things Can Happen(tm).
 *
 * Note that this doesn't inherit from InputData because this itself is not an
 * event. It is only a container/struct that should have any number of instances
 * within a MultiTouchInput.
 *
 * fixme/bug 775746: Make dom::Touch inherit from this class.
 */
class SingleTouchData
{
public:
  SingleTouchData(int32_t aIdentifier,
                  ScreenIntPoint aScreenPoint,
                  ScreenSize aRadius,
                  float aRotationAngle,
                  float aForce)
    : mIdentifier(aIdentifier),
      mScreenPoint(aScreenPoint),
      mRadius(aRadius),
      mRotationAngle(aRotationAngle),
      mForce(aForce)
  {


  }

  SingleTouchData()
  {
  }

  // A unique number assigned to each SingleTouchData within a MultiTouchInput so
  // that they can be easily distinguished when handling a touch start/move/end.
  int32_t mIdentifier;

  // Point on the screen that the touch hit, in device pixels. They are
  // coordinates on the screen.
  ScreenIntPoint mScreenPoint;

  // Radius that the touch covers, i.e. if you're using your thumb it will
  // probably be larger than using your pinky, even with the same force.
  // Radius can be different along x and y. For example, if you press down with
  // your entire finger vertically, the y radius will be much larger than the x
  // radius.
  ScreenSize mRadius;

  float mRotationAngle;

  // How hard the screen is being pressed.
  float mForce;
};

/**
 * Similar to WidgetTouchEvent, but for use off-main-thread. Also only stores a
 * screen touch point instead of the many different coordinate spaces
 * WidgetTouchEvent stores its touch point in. This includes a way to initialize
 * itself from a WidgetTouchEvent by copying all relevant data over. Note that
 * this copying from WidgetTouchEvent functionality can only be used on the main
 * thread.
 *
 * Stores an array of SingleTouchData.
 */
class MultiTouchInput : public InputData
{
public:
  enum MultiTouchType
  {
    MULTITOUCH_START,
    MULTITOUCH_MOVE,
    MULTITOUCH_END,
    MULTITOUCH_ENTER,
    MULTITOUCH_LEAVE,
    MULTITOUCH_CANCEL
  };

  MultiTouchInput(MultiTouchType aType, uint32_t aTime, TimeStamp aTimeStamp,
                  Modifiers aModifiers)
    : InputData(MULTITOUCH_INPUT, aTime, aTimeStamp, aModifiers),
      mType(aType)
  {


  }

  MultiTouchInput()
  {
  }

  MultiTouchInput(const WidgetTouchEvent& aTouchEvent);

  // This conversion from WidgetMouseEvent to MultiTouchInput is needed because
  // on the B2G emulator we can only receive mouse events, but we need to be
  // able to pan correctly. To do this, we convert the events into a format that
  // the panning code can handle. This code is very limited and only supports
  // SingleTouchData. It also sends garbage for the identifier, radius, force
  // and rotation angle.
  MultiTouchInput(const WidgetMouseEvent& aMouseEvent);

  MultiTouchType mType;
  nsTArray<SingleTouchData> mTouches;
};

/**
 * Encapsulation class for pan events, can be used off-main-thread.
 * These events are currently only used for scrolling on desktop.
 */
class PanGestureInput : public InputData
{
public:
  enum PanGestureType
  {
    // MayStart: Dispatched before any actual panning has occurred but when a
    // pan gesture is probably about to start, for example when the user
    // starts touching the touchpad. Should interrupt any ongoing APZ
    // animation and can be used to trigger scrollability indicators (e.g.
    // flashing overlay scrollbars).
    PANGESTURE_MAYSTART,

    // Cancelled: Dispatched after MayStart when no pan gesture is going to
    // happen after all, for example when the user lifts their fingers from a
    // touchpad without having done any scrolling.
    PANGESTURE_CANCELLED,

    // Start: A pan gesture is starting.
    // For devices that do not support the MayStart event type, this event can
    // be used to interrupt ongoing APZ animations.
    PANGESTURE_START,

    // Pan: The actual pan motion by mPanDisplacement.
    PANGESTURE_PAN,

    // End: The pan gesture has ended, for example because the user has lifted
    // their fingers from a touchpad after scrolling.
    // Any potential momentum events fire after this event.
    PANGESTURE_END,

    // The following momentum event types are used in order to control the pan
    // momentum animation. Using these instead of our own animation ensures
    // that the animation curve is OS native and that the animation stops
    // reliably if it is cancelled by the user.

    // MomentumStart: Dispatched between the End event of the actual
    // user-controlled pan, and the first MomentumPan event of the momentum
    // animation.
    PANGESTURE_MOMENTUMSTART,

    // MomentumPan: The actual momentum motion by mPanDisplacement.
    PANGESTURE_MOMENTUMPAN,

    // MomentumEnd: The momentum animation has ended, for example because the
    // momentum velocity has gone below the stopping threshold, or because the
    // user has stopped the animation by putting their fingers on a touchpad.
    PANGESTURE_MOMENTUMEND
  };

  PanGestureInput(PanGestureType aType,
                  uint32_t aTime,
                  TimeStamp aTimeStamp,
                  const ScreenPoint& aPanStartPoint,
                  const ScreenPoint& aPanDisplacement,
                  Modifiers aModifiers)
    : InputData(PANGESTURE_INPUT, aTime, aTimeStamp, aModifiers),
      mType(aType),
      mPanStartPoint(aPanStartPoint),
      mPanDisplacement(aPanDisplacement)
  {
  }

  PanGestureType mType;
  ScreenPoint mPanStartPoint;

  // Only non-zero if mType is PANGESTURE_PAN or PANGESTURE_MOMENTUMPAN.
  ScreenPoint mPanDisplacement;
};

/**
 * Encapsulation class for pinch events. In general, these will be generated by
 * a gesture listener by looking at SingleTouchData/MultiTouchInput instances and
 * determining whether or not the user was trying to do a gesture.
 */
class PinchGestureInput : public InputData
{
public:
  enum PinchGestureType
  {
    PINCHGESTURE_START,
    PINCHGESTURE_SCALE,
    PINCHGESTURE_END
  };

  PinchGestureInput(PinchGestureType aType,
                    uint32_t aTime,
                    TimeStamp aTimeStamp,
                    const ScreenPoint& aFocusPoint,
                    float aCurrentSpan,
                    float aPreviousSpan,
                    Modifiers aModifiers)
    : InputData(PINCHGESTURE_INPUT, aTime, aTimeStamp, aModifiers),
      mType(aType),
      mFocusPoint(aFocusPoint),
      mCurrentSpan(aCurrentSpan),
      mPreviousSpan(aPreviousSpan)
  {


  }

  PinchGestureType mType;

  // Center point of the pinch gesture. That is, if there are two fingers on the
  // screen, it is their midpoint. In the case of more than two fingers, the
  // point is implementation-specific, but can for example be the midpoint
  // between the very first and very last touch. This is in device pixels and
  // are the coordinates on the screen of this midpoint.
  ScreenPoint mFocusPoint;

  // The distance in device pixels (though as a float for increased precision
  // and because it is the distance along both the x and y axis) between the
  // touches responsible for the pinch gesture.
  float mCurrentSpan;

  // The previous |mCurrentSpan| in the PinchGestureInput preceding this one.
  // This is only really relevant during a PINCHGESTURE_SCALE because when it is
  // of this type then there must have been a history of spans.
  float mPreviousSpan;
};

/**
 * Encapsulation class for tap events. In general, these will be generated by
 * a gesture listener by looking at SingleTouchData/MultiTouchInput instances and
 * determining whether or not the user was trying to do a gesture.
 */
class TapGestureInput : public InputData
{
public:
  enum TapGestureType
  {
    TAPGESTURE_LONG,
    TAPGESTURE_LONG_UP,
    TAPGESTURE_UP,
    TAPGESTURE_CONFIRMED,
    TAPGESTURE_DOUBLE,
    TAPGESTURE_CANCEL
  };

  TapGestureInput(TapGestureType aType,
                  uint32_t aTime,
                  TimeStamp aTimeStamp,
                  const ScreenIntPoint& aPoint,
                  Modifiers aModifiers)
    : InputData(TAPGESTURE_INPUT, aTime, aTimeStamp, aModifiers),
      mType(aType),
      mPoint(aPoint)
  {


  }

  TapGestureType mType;
  ScreenIntPoint mPoint;
};

}

#endif // InputData_h__
