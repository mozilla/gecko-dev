/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <math.h>                       // for fabsf, fabs, atan2
#include <stdint.h>                     // for uint32_t, uint64_t
#include <sys/types.h>                  // for int32_t
#include <algorithm>                    // for max, min
#include "AnimationCommon.h"            // for ComputedTimingFunction
#include "AsyncPanZoomController.h"     // for AsyncPanZoomController, etc
#include "CompositorParent.h"           // for CompositorParent
#include "FrameMetrics.h"               // for FrameMetrics, etc
#include "GestureEventListener.h"       // for GestureEventListener
#include "InputData.h"                  // for MultiTouchInput, etc
#include "LayerTransactionParent.h"     // for LayerTransactionParent
#include "Units.h"                      // for CSSRect, CSSPoint, etc
#include "base/message_loop.h"          // for MessageLoop
#include "base/task.h"                  // for NewRunnableMethod, etc
#include "base/tracked.h"               // for FROM_HERE
#include "gfxPlatform.h"                // for gfxPlatform::UseProgressiveTilePainting
#include "gfxTypes.h"                   // for gfxFloat
#include "mozilla/Assertions.h"         // for MOZ_ASSERT, etc
#include "mozilla/BasicEvents.h"        // for Modifiers, MODIFIER_*
#include "mozilla/ClearOnShutdown.h"    // for ClearOnShutdown
#include "mozilla/Constants.h"          // for M_PI
#include "mozilla/EventForwards.h"      // for nsEventStatus_*
#include "mozilla/Preferences.h"        // for Preferences
#include "mozilla/ReentrantMonitor.h"   // for ReentrantMonitorAutoEnter, etc
#include "mozilla/StaticPtr.h"          // for StaticAutoPtr
#include "mozilla/TimeStamp.h"          // for TimeDuration, TimeStamp
#include "mozilla/dom/Touch.h"          // for Touch
#include "mozilla/gfx/BasePoint.h"      // for BasePoint
#include "mozilla/gfx/BaseRect.h"       // for BaseRect
#include "mozilla/gfx/Point.h"          // for Point, RoundedToInt, etc
#include "mozilla/gfx/Rect.h"           // for RoundedIn
#include "mozilla/gfx/ScaleFactor.h"    // for ScaleFactor
#include "mozilla/layers/APZCTreeManager.h"  // for ScrollableLayerGuid
#include "mozilla/layers/AsyncCompositionManager.h"  // for ViewTransform
#include "mozilla/layers/Axis.h"        // for AxisX, AxisY, Axis, etc
#include "mozilla/layers/GeckoContentController.h"
#include "mozilla/layers/PCompositorParent.h" // for PCompositorParent
#include "mozilla/layers/TaskThrottler.h"  // for TaskThrottler
#include "mozilla/mozalloc.h"           // for operator new, etc
#include "mozilla/unused.h"             // for unused
#include "nsAlgorithm.h"                // for clamped
#include "nsAutoPtr.h"                  // for nsRefPtr
#include "nsCOMPtr.h"                   // for already_AddRefed
#include "nsDebug.h"                    // for NS_WARNING
#include "nsIDOMWindowUtils.h"          // for nsIDOMWindowUtils
#include "nsISupportsImpl.h"
#include "nsMathUtils.h"                // for NS_hypot
#include "nsPoint.h"                    // for nsIntPoint
#include "nsStyleConsts.h"
#include "nsStyleStruct.h"              // for nsTimingFunction
#include "nsTArray.h"                   // for nsTArray, nsTArray_Impl, etc
#include "nsThreadUtils.h"              // for NS_IsMainThread
#include "nsTraceRefcnt.h"              // for MOZ_COUNT_CTOR, etc
#include "SharedMemoryBasic.h"          // for SharedMemoryBasic

// #define APZC_ENABLE_RENDERTRACE

#define APZC_LOG(...)
// #define APZC_LOG(...) printf_stderr("APZC: " __VA_ARGS__)
#define APZC_LOG_FM(fm, prefix, ...) \
  APZC_LOG(prefix ":" \
           " i=(%ld %lld) cb=(%d %d %d %d) dp=(%.3f %.3f %.3f %.3f) v=(%.3f %.3f %.3f %.3f) " \
           "s=(%.3f %.3f) sr=(%.3f %.3f %.3f %.3f) z=(%.3f %.3f %.3f %.3f) %d\n", \
           __VA_ARGS__, \
           fm.mPresShellId, fm.mScrollId, \
           fm.mCompositionBounds.x, fm.mCompositionBounds.y, fm.mCompositionBounds.width, fm.mCompositionBounds.height, \
           fm.mDisplayPort.x, fm.mDisplayPort.y, fm.mDisplayPort.width, fm.mDisplayPort.height, \
           fm.mViewport.x, fm.mViewport.y, fm.mViewport.width, fm.mViewport.height, \
           fm.mScrollOffset.x, fm.mScrollOffset.y, \
           fm.mScrollableRect.x, fm.mScrollableRect.y, fm.mScrollableRect.width, fm.mScrollableRect.height, \
           fm.mDevPixelsPerCSSPixel.scale, fm.mResolution.scale, fm.mCumulativeResolution.scale, fm.mZoom.scale, \
           fm.mUpdateScrollOffset); \

// Static helper functions
namespace {

int32_t
WidgetModifiersToDOMModifiers(mozilla::Modifiers aModifiers)
{
  int32_t result = 0;
  if (aModifiers & mozilla::MODIFIER_SHIFT) {
    result |= nsIDOMWindowUtils::MODIFIER_SHIFT;
  }
  if (aModifiers & mozilla::MODIFIER_CONTROL) {
    result |= nsIDOMWindowUtils::MODIFIER_CONTROL;
  }
  if (aModifiers & mozilla::MODIFIER_ALT) {
    result |= nsIDOMWindowUtils::MODIFIER_ALT;
  }
  if (aModifiers & mozilla::MODIFIER_META) {
    result |= nsIDOMWindowUtils::MODIFIER_META;
  }
  if (aModifiers & mozilla::MODIFIER_ALTGRAPH) {
    result |= nsIDOMWindowUtils::MODIFIER_ALTGRAPH;
  }
  if (aModifiers & mozilla::MODIFIER_CAPSLOCK) {
    result |= nsIDOMWindowUtils::MODIFIER_CAPSLOCK;
  }
  if (aModifiers & mozilla::MODIFIER_FN) {
    result |= nsIDOMWindowUtils::MODIFIER_FN;
  }
  if (aModifiers & mozilla::MODIFIER_NUMLOCK) {
    result |= nsIDOMWindowUtils::MODIFIER_NUMLOCK;
  }
  if (aModifiers & mozilla::MODIFIER_SCROLLLOCK) {
    result |= nsIDOMWindowUtils::MODIFIER_SCROLLLOCK;
  }
  if (aModifiers & mozilla::MODIFIER_SYMBOLLOCK) {
    result |= nsIDOMWindowUtils::MODIFIER_SYMBOLLOCK;
  }
  if (aModifiers & mozilla::MODIFIER_OS) {
    result |= nsIDOMWindowUtils::MODIFIER_OS;
  }
  return result;
}

}

using namespace mozilla::css;

namespace mozilla {
namespace layers {

typedef mozilla::layers::AllowedTouchBehavior AllowedTouchBehavior;

/**
 * Specifies whether touch-action property is in force.
 */
static bool gTouchActionPropertyEnabled = false;

/**
 * Constant describing the tolerance in distance we use, multiplied by the
 * device DPI, before we start panning the screen. This is to prevent us from
 * accidentally processing taps as touch moves, and from very short/accidental
 * touches moving the screen.
 */
static float gTouchStartTolerance = 1.0f/4.5f;

/**
 * Default touch behavior (is used when not touch behavior is set).
 */
static const uint32_t DefaultTouchBehavior = AllowedTouchBehavior::VERTICAL_PAN |
                                             AllowedTouchBehavior::HORIZONTAL_PAN |
                                             AllowedTouchBehavior::ZOOM;

/**
 * Angle from axis within which we stay axis-locked
 */
static const double AXIS_LOCK_ANGLE = M_PI / 6.0; // 30 degrees

/**
 * The distance in inches the user must pan before axis lock can be broken
 */
static const float AXIS_BREAKOUT_THRESHOLD = 1.0f/32.0f;

/**
 * The angle at which axis lock can be broken
 */
static const double AXIS_BREAKOUT_ANGLE = M_PI / 8.0; // 22.5 degrees

/**
 * Angle from axis to the line drawn by pan move.
 * If angle is less than this value we can assume that panning
 * can be done in allowed direction (horizontal or vertical).
 * Currently used only for touch-action css property stuff and was
 * added to keep behavior consistent with IE.
 */
static const double ALLOWED_DIRECT_PAN_ANGLE = M_PI / 3.0; // 60 degrees

/**
 * The preferred axis locking style. See AxisLockMode for possible values.
 */
static int32_t gAxisLockMode = 0;

/**
 * Maximum amount of time while panning before sending a viewport change. This
 * will asynchronously repaint the page. It is also forced when panning stops.
 */
static int32_t gPanRepaintInterval = 250;

/**
 * Maximum amount of time flinging before sending a viewport change. This will
 * asynchronously repaint the page.
 */
static int32_t gFlingRepaintInterval = 75;

/**
 * Minimum amount of speed along an axis before we switch to "skate" multipliers
 * rather than using the "stationary" multipliers.
 */
static float gMinSkateSpeed = 1.0f;

/**
 * Whether or not to use the estimated paint duration as a factor when projecting
 * the displayport in the direction of scrolling. If this value is set to false,
 * a constant 50ms paint time is used; the projection can be scaled as desired
 * using the gVelocityBias pref below.
 */
static bool gUsePaintDuration = true;

/**
 * How much to adjust the displayport in the direction of scrolling. This value
 * is multiplied by the velocity and added to the displayport offset.
 */
static float gVelocityBias = 1.0f;

/**
 * Duration of a zoom to animation.
 */
static const TimeDuration ZOOM_TO_DURATION = TimeDuration::FromSeconds(0.25);

/**
 * Computed time function used for sampling frames of a zoom to animation.
 */
StaticAutoPtr<ComputedTimingFunction> gComputedTimingFunction;

/**
 * Maximum zoom amount, always used, even if a page asks for higher.
 */
static const CSSToScreenScale MAX_ZOOM(8.0f);

/**
 * Minimum zoom amount, always used, even if a page asks for lower.
 */
static const CSSToScreenScale MIN_ZOOM(0.125f);

/**
 * Amount of time before we timeout response from content. For example, if
 * content is being unruly/slow and we don't get a response back within this
 * time, we will just pretend that content did not preventDefault any touch
 * events we dispatched to it.
 */
static int gContentResponseTimeout = 300;

/**
 * Number of samples to store of how long it took to paint after the previous
 * requests.
 */
static int gNumPaintDurationSamples = 3;

/**
 * The multiplier we apply to the displayport size if it is skating (current
 * velocity is above gMinSkateSpeed). We prefer to increase the size of the
 * Y axis because it is more natural in the case that a user is reading a page
 * that scrolls up/down. Note that one, both or neither of these may be used
 * at any instant.
 * In general we want g[XY]SkateSizeMultiplier to be smaller than the corresponding
 * stationary size multiplier because when panning fast we would like to paint
 * less and get faster, more predictable paint times. When panning slowly we
 * can afford to paint more even though it's slower.
 */
static float gXSkateSizeMultiplier = 1.5f;
static float gYSkateSizeMultiplier = 2.5f;

/**
 * The multiplier we apply to the displayport size if it is not skating (see
 * documentation for gXSkateSizeMultiplier).
 */
static float gXStationarySizeMultiplier = 3.0f;
static float gYStationarySizeMultiplier = 3.5f;

/**
 * The time period in ms that throttles mozbrowserasyncscroll event.
 * Default is 100ms if there is no "apz.asyncscroll.throttle" in preference.
 */

static int gAsyncScrollThrottleTime = 100;

/**
 * The timeout in ms for mAsyncScrollTimeoutTask delay task.
 * Default is 300ms if there is no "apz.asyncscroll.timeout" in preference.
 */
static int gAsyncScrollTimeout = 300;

/**
 * Pref that enables integration with the Metro "cross-slide" gesture.
 */
static bool gCrossSlideEnabled = false;

/**
 * Pref that enables progressive tile painting
 */
static bool gUseProgressiveTilePainting = false;

/**
 * Is aAngle within the given threshold of the horizontal axis?
 * @param aAngle an angle in radians in the range [0, pi]
 * @param aThreshold an angle in radians in the range [0, pi/2]
 */
static bool IsCloseToHorizontal(float aAngle, float aThreshold)
{
  return (aAngle < aThreshold || aAngle > (M_PI - aThreshold));
}

// As above, but for the vertical axis.
static bool IsCloseToVertical(float aAngle, float aThreshold)
{
  return (fabs(aAngle - (M_PI / 2)) < aThreshold);
}

static inline void LogRendertraceRect(const ScrollableLayerGuid& aGuid, const char* aDesc, const char* aColor, const CSSRect& aRect)
{
#ifdef APZC_ENABLE_RENDERTRACE
  static const TimeStamp sRenderStart = TimeStamp::Now();
  TimeDuration delta = TimeStamp::Now() - sRenderStart;
  printf_stderr("(%llu,%lu,%llu)%s RENDERTRACE %f rect %s %f %f %f %f\n",
    aGuid.mLayersId, aGuid.mPresShellId, aGuid.mScrollId,
    aDesc, delta.ToMilliseconds(), aColor,
    aRect.x, aRect.y, aRect.width, aRect.height);
#endif
}

static TimeStamp sFrameTime;

// Counter used to give each APZC a unique id
static uint32_t sAsyncPanZoomControllerCount = 0;

static TimeStamp
GetFrameTime() {
  if (sFrameTime.IsNull()) {
    return TimeStamp::Now();
  }
  return sFrameTime;
}

class FlingAnimation: public AsyncPanZoomAnimation {
public:
  FlingAnimation(AxisX& aX, AxisY& aY)
    : AsyncPanZoomAnimation(TimeDuration::FromMilliseconds(gFlingRepaintInterval))
    , mX(aX)
    , mY(aY)
  {}
  /**
   * Advances a fling by an interpolated amount based on the passed in |aDelta|.
   * This should be called whenever sampling the content transform for this
   * frame. Returns true if the fling animation should be advanced by one frame,
   * or false if there is no fling or the fling has ended.
   */
  virtual bool Sample(FrameMetrics& aFrameMetrics,
                      const TimeDuration& aDelta);

private:
  AxisX& mX;
  AxisY& mY;
};

class ZoomAnimation: public AsyncPanZoomAnimation {
public:
  ZoomAnimation(CSSPoint aStartOffset, CSSToScreenScale aStartZoom,
                CSSPoint aEndOffset, CSSToScreenScale aEndZoom)
    : mStartOffset(aStartOffset)
    , mStartZoom(aStartZoom)
    , mEndOffset(aEndOffset)
    , mEndZoom(aEndZoom)
  {}

  virtual bool Sample(FrameMetrics& aFrameMetrics,
                      const TimeDuration& aDelta);

private:
  TimeDuration mDuration;

  // Old metrics from before we started a zoom animation. This is only valid
  // when we are in the "ANIMATED_ZOOM" state. This is used so that we can
  // interpolate between the start and end frames. We only use the
  // |mViewportScrollOffset| and |mResolution| fields on this.
  CSSPoint mStartOffset;
  CSSToScreenScale mStartZoom;

  // Target metrics for a zoom to animation. This is only valid when we are in
  // the "ANIMATED_ZOOM" state. We only use the |mViewportScrollOffset| and
  // |mResolution| fields on this.
  CSSPoint mEndOffset;
  CSSToScreenScale mEndZoom;
};

void
AsyncPanZoomController::SetFrameTime(const TimeStamp& aTime) {
  sFrameTime = aTime;
}

/*static*/ void
AsyncPanZoomController::InitializeGlobalState()
{
  MOZ_ASSERT(NS_IsMainThread());

  static bool sInitialized = false;
  if (sInitialized)
    return;
  sInitialized = true;

  Preferences::AddBoolVarCache(&gTouchActionPropertyEnabled, "layout.css.touch_action.enabled", gTouchActionPropertyEnabled);
  Preferences::AddIntVarCache(&gPanRepaintInterval, "apz.pan_repaint_interval", gPanRepaintInterval);
  Preferences::AddIntVarCache(&gFlingRepaintInterval, "apz.fling_repaint_interval", gFlingRepaintInterval);
  Preferences::AddFloatVarCache(&gMinSkateSpeed, "apz.min_skate_speed", gMinSkateSpeed);
  Preferences::AddBoolVarCache(&gUsePaintDuration, "apz.use_paint_duration", gUsePaintDuration);
  Preferences::AddFloatVarCache(&gVelocityBias, "apz.velocity_bias", gVelocityBias);
  Preferences::AddIntVarCache(&gContentResponseTimeout, "apz.content_response_timeout", gContentResponseTimeout);
  Preferences::AddIntVarCache(&gNumPaintDurationSamples, "apz.num_paint_duration_samples", gNumPaintDurationSamples);
  Preferences::AddFloatVarCache(&gTouchStartTolerance, "apz.touch_start_tolerance", gTouchStartTolerance);
  Preferences::AddFloatVarCache(&gXSkateSizeMultiplier, "apz.x_skate_size_multiplier", gXSkateSizeMultiplier);
  Preferences::AddFloatVarCache(&gYSkateSizeMultiplier, "apz.y_skate_size_multiplier", gYSkateSizeMultiplier);
  Preferences::AddFloatVarCache(&gXStationarySizeMultiplier, "apz.x_stationary_size_multiplier", gXStationarySizeMultiplier);
  Preferences::AddFloatVarCache(&gYStationarySizeMultiplier, "apz.y_stationary_size_multiplier", gYStationarySizeMultiplier);
  Preferences::AddIntVarCache(&gAsyncScrollThrottleTime, "apz.asyncscroll.throttle", gAsyncScrollThrottleTime);
  Preferences::AddIntVarCache(&gAsyncScrollTimeout, "apz.asyncscroll.timeout", gAsyncScrollTimeout);
  Preferences::AddBoolVarCache(&gCrossSlideEnabled, "apz.cross_slide.enabled", gCrossSlideEnabled);
  Preferences::AddIntVarCache(&gAxisLockMode, "apz.axis_lock_mode", gAxisLockMode);
  gUseProgressiveTilePainting = gfxPlatform::UseProgressiveTilePainting();

  gComputedTimingFunction = new ComputedTimingFunction();
  gComputedTimingFunction->Init(
    nsTimingFunction(NS_STYLE_TRANSITION_TIMING_FUNCTION_EASE));
  ClearOnShutdown(&gComputedTimingFunction);
}

AsyncPanZoomController::AsyncPanZoomController(uint64_t aLayersId,
                                               APZCTreeManager* aTreeManager,
                                               GeckoContentController* aGeckoContentController,
                                               GestureBehavior aGestures)
  :  mLayersId(aLayersId),
     mCrossProcessCompositorParent(nullptr),
     mPaintThrottler(GetFrameTime()),
     mGeckoContentController(aGeckoContentController),
     mRefPtrMonitor("RefPtrMonitor"),
     mMonitor("AsyncPanZoomController"),
     mTouchActionPropertyEnabled(gTouchActionPropertyEnabled),
     mContentResponseTimeoutTask(nullptr),
     mX(MOZ_THIS_IN_INITIALIZER_LIST()),
     mY(MOZ_THIS_IN_INITIALIZER_LIST()),
     mPanDirRestricted(false),
     mZoomConstraints(false, MIN_ZOOM, MAX_ZOOM),
     mLastSampleTime(GetFrameTime()),
     mState(NOTHING),
     mLastAsyncScrollTime(GetFrameTime()),
     mLastAsyncScrollOffset(0, 0),
     mCurrentAsyncScrollOffset(0, 0),
     mAsyncScrollTimeoutTask(nullptr),
     mHandlingTouchQueue(false),
     mAllowedTouchBehaviorSet(false),
     mPreventDefault(false),
     mPreventDefaultSet(false),
     mTreeManager(aTreeManager),
     mAPZCId(sAsyncPanZoomControllerCount++),
     mSharedFrameMetricsBuffer(nullptr),
     mSharedLock(nullptr)
{
  MOZ_COUNT_CTOR(AsyncPanZoomController);

  if (aGestures == USE_GESTURE_DETECTOR) {
    mGestureEventListener = new GestureEventListener(this);
  }
}

AsyncPanZoomController::~AsyncPanZoomController() {

  PCompositorParent* compositor =
    (mCrossProcessCompositorParent ? mCrossProcessCompositorParent : mCompositorParent.get());

  // Only send the release message if the SharedFrameMetrics has been created.
  if (compositor && mSharedFrameMetricsBuffer) {
    unused << compositor->SendReleaseSharedCompositorFrameMetrics(mFrameMetrics.mScrollId, mAPZCId);
  }

  delete mSharedFrameMetricsBuffer;
  delete mSharedLock;

  MOZ_COUNT_DTOR(AsyncPanZoomController);
}

already_AddRefed<GeckoContentController>
AsyncPanZoomController::GetGeckoContentController() {
  MonitorAutoLock lock(mRefPtrMonitor);
  nsRefPtr<GeckoContentController> controller = mGeckoContentController;
  return controller.forget();
}

already_AddRefed<GestureEventListener>
AsyncPanZoomController::GetGestureEventListener() {
  MonitorAutoLock lock(mRefPtrMonitor);
  nsRefPtr<GestureEventListener> listener = mGestureEventListener;
  return listener.forget();
}

void
AsyncPanZoomController::Destroy()
{
  { // scope the lock
    MonitorAutoLock lock(mRefPtrMonitor);
    mGeckoContentController = nullptr;
    mGestureEventListener = nullptr;
  }
  mPrevSibling = nullptr;
  mLastChild = nullptr;
  mParent = nullptr;
  mTreeManager = nullptr;
}

bool
AsyncPanZoomController::IsDestroyed()
{
  return mTreeManager == nullptr;
}

/* static */float
AsyncPanZoomController::GetTouchStartTolerance()
{
  return (gTouchStartTolerance * APZCTreeManager::GetDPI());
}

/* static */AsyncPanZoomController::AxisLockMode AsyncPanZoomController::GetAxisLockMode()
{
  return static_cast<AxisLockMode>(gAxisLockMode);
}

nsEventStatus AsyncPanZoomController::ReceiveInputEvent(const InputData& aEvent) {
  // If we may have touch listeners and touch action property is enabled, we
  // enable the machinery that allows touch listeners to preventDefault any touch inputs
  // and also waits for the allowed touch behavior values to be received from the outside.
  // This should not happen unless there are actually touch listeners and touch-action property
  // enable as it introduces potentially unbounded lag because it causes a round-trip through
  // content.  Usually, if content is responding in a timely fashion, this only introduces a
  // nearly constant few hundred ms of lag.
  if (mFrameMetrics.mMayHaveTouchListeners && aEvent.mInputType == MULTITOUCH_INPUT &&
      (mState == NOTHING || mState == TOUCHING || IsPanningState(mState))) {
    const MultiTouchInput& multiTouchInput = aEvent.AsMultiTouchInput();
    if (multiTouchInput.mType == MultiTouchInput::MULTITOUCH_START) {
      mAllowedTouchBehaviors.Clear();
      mAllowedTouchBehaviorSet = false;
      mPreventDefault = false;
      mPreventDefaultSet = false;
      SetState(WAITING_CONTENT_RESPONSE);
    }
  }

  if (mState == WAITING_CONTENT_RESPONSE || mHandlingTouchQueue) {
    if (aEvent.mInputType == MULTITOUCH_INPUT) {
      const MultiTouchInput& multiTouchInput = aEvent.AsMultiTouchInput();
      mTouchQueue.AppendElement(multiTouchInput);

      if (!mContentResponseTimeoutTask) {
        mContentResponseTimeoutTask =
          NewRunnableMethod(this, &AsyncPanZoomController::TimeoutContentResponse);

        PostDelayedTask(mContentResponseTimeoutTask, gContentResponseTimeout);
      }
    }
    return nsEventStatus_eIgnore;
  }

  return HandleInputEvent(aEvent);
}

nsEventStatus AsyncPanZoomController::HandleInputEvent(const InputData& aEvent) {
  nsEventStatus rv = nsEventStatus_eIgnore;

  nsRefPtr<GestureEventListener> listener = GetGestureEventListener();
  if (listener) {
    rv = listener->HandleInputEvent(aEvent);
    if (rv == nsEventStatus_eConsumeNoDefault)
      return rv;
  }

  switch (aEvent.mInputType) {
  case MULTITOUCH_INPUT: {
    const MultiTouchInput& multiTouchInput = aEvent.AsMultiTouchInput();
    switch (multiTouchInput.mType) {
      case MultiTouchInput::MULTITOUCH_START: rv = OnTouchStart(multiTouchInput); break;
      case MultiTouchInput::MULTITOUCH_MOVE: rv = OnTouchMove(multiTouchInput); break;
      case MultiTouchInput::MULTITOUCH_END: rv = OnTouchEnd(multiTouchInput); break;
      case MultiTouchInput::MULTITOUCH_CANCEL: rv = OnTouchCancel(multiTouchInput); break;
      default: NS_WARNING("Unhandled multitouch"); break;
    }
    break;
  }
  case PINCHGESTURE_INPUT: {
    const PinchGestureInput& pinchGestureInput = aEvent.AsPinchGestureInput();
    switch (pinchGestureInput.mType) {
      case PinchGestureInput::PINCHGESTURE_START: rv = OnScaleBegin(pinchGestureInput); break;
      case PinchGestureInput::PINCHGESTURE_SCALE: rv = OnScale(pinchGestureInput); break;
      case PinchGestureInput::PINCHGESTURE_END: rv = OnScaleEnd(pinchGestureInput); break;
      default: NS_WARNING("Unhandled pinch gesture"); break;
    }
    break;
  }
  case TAPGESTURE_INPUT: {
    const TapGestureInput& tapGestureInput = aEvent.AsTapGestureInput();
    switch (tapGestureInput.mType) {
      case TapGestureInput::TAPGESTURE_LONG: rv = OnLongPress(tapGestureInput); break;
      case TapGestureInput::TAPGESTURE_LONG_UP: rv = OnLongPressUp(tapGestureInput); break;
      case TapGestureInput::TAPGESTURE_UP: rv = OnSingleTapUp(tapGestureInput); break;
      case TapGestureInput::TAPGESTURE_CONFIRMED: rv = OnSingleTapConfirmed(tapGestureInput); break;
      case TapGestureInput::TAPGESTURE_DOUBLE: rv = OnDoubleTap(tapGestureInput); break;
      case TapGestureInput::TAPGESTURE_CANCEL: rv = OnCancelTap(tapGestureInput); break;
      default: NS_WARNING("Unhandled tap gesture"); break;
    }
    break;
  }
  default: NS_WARNING("Unhandled input event"); break;
  }

  mLastEventTime = aEvent.mTime;
  return rv;
}

nsEventStatus AsyncPanZoomController::OnTouchStart(const MultiTouchInput& aEvent) {
  APZC_LOG("%p got a touch-start in state %d\n", this, mState);
  mPanDirRestricted = false;
  ScreenIntPoint point = GetFirstTouchScreenPoint(aEvent);

  switch (mState) {
    case ANIMATING_ZOOM:
      // We just interrupted a double-tap animation, so force a redraw in case
      // this touchstart is just a tap that doesn't end up triggering a redraw.
      {
        ReentrantMonitorAutoEnter lock(mMonitor);
        RequestContentRepaint();
        ScheduleComposite();
        UpdateSharedCompositorFrameMetrics();
      }
      // Fall through.
    case FLING:
      CancelAnimation();
      // Fall through.
    case NOTHING:
      mX.StartTouch(point.x);
      mY.StartTouch(point.y);
      SetState(TOUCHING);
      break;
    case TOUCHING:
    case PANNING:
    case PANNING_LOCKED_X:
    case PANNING_LOCKED_Y:
    case CROSS_SLIDING_X:
    case CROSS_SLIDING_Y:
    case PINCHING:
    case WAITING_CONTENT_RESPONSE:
      NS_WARNING("Received impossible touch in OnTouchStart");
      break;
    default:
      NS_WARNING("Unhandled case in OnTouchStart");
      break;
  }

  return nsEventStatus_eConsumeNoDefault;
}

nsEventStatus AsyncPanZoomController::OnTouchMove(const MultiTouchInput& aEvent) {
  APZC_LOG("%p got a touch-move in state %d\n", this, mState);
  switch (mState) {
    case FLING:
    case NOTHING:
    case ANIMATING_ZOOM:
      // May happen if the user double-taps and drags without lifting after the
      // second tap. Ignore the move if this happens.
      return nsEventStatus_eIgnore;

    case CROSS_SLIDING_X:
    case CROSS_SLIDING_Y:
      // While cross-sliding, we don't want to consume any touchmove events for
      // panning or zooming, and let the caller handle them instead.
      return nsEventStatus_eIgnore;

    case TOUCHING: {
      float panThreshold = GetTouchStartTolerance();
      UpdateWithTouchAtDevicePoint(aEvent);

      if (PanDistance() < panThreshold) {
        return nsEventStatus_eIgnore;
      }

      if (mTouchActionPropertyEnabled &&
          (GetTouchBehavior(0) & AllowedTouchBehavior::VERTICAL_PAN) &&
          (GetTouchBehavior(0) & AllowedTouchBehavior::HORIZONTAL_PAN)) {
        // User tries to trigger a touch behavior. If allowed touch behavior is vertical pan
        // + horizontal pan (touch-action value is equal to AUTO) we can return ConsumeNoDefault
        // status immediately to trigger cancel event further. It should happen independent of
        // the parent type (whether it is scrolling or not).
        StartPanning(aEvent);
        return nsEventStatus_eConsumeNoDefault;
      }

      return StartPanning(aEvent);
    }

    case PANNING:
    case PANNING_LOCKED_X:
    case PANNING_LOCKED_Y:
      TrackTouch(aEvent);
      return nsEventStatus_eConsumeNoDefault;

    case PINCHING:
      // The scale gesture listener should have handled this.
      NS_WARNING("Gesture listener should have handled pinching in OnTouchMove.");
      return nsEventStatus_eIgnore;

    case WAITING_CONTENT_RESPONSE:
      NS_WARNING("Received impossible touch in OnTouchMove");
      break;
  }

  return nsEventStatus_eConsumeNoDefault;
}

nsEventStatus AsyncPanZoomController::OnTouchEnd(const MultiTouchInput& aEvent) {
  APZC_LOG("%p got a touch-end in state %d\n", this, mState);

  // In case no touch behavior triggered previously we can avoid sending
  // scroll events or requesting content repaint. This condition is added
  // to make tests consistent - in case touch-action is NONE (and therefore
  // no pans/zooms can be performed) we expected neither scroll or repaint
  // events.
  if (mState != NOTHING) {
    ReentrantMonitorAutoEnter lock(mMonitor);
    SendAsyncScrollEvent();
  }

  switch (mState) {
  case FLING:
    // Should never happen.
    NS_WARNING("Received impossible touch end in OnTouchEnd.");
    // Fall through.
  case ANIMATING_ZOOM:
  case NOTHING:
    // May happen if the user double-taps and drags without lifting after the
    // second tap. Ignore if this happens.
    return nsEventStatus_eIgnore;

  case TOUCHING:
  case CROSS_SLIDING_X:
  case CROSS_SLIDING_Y:
    SetState(NOTHING);
    return nsEventStatus_eIgnore;

  case PANNING:
  case PANNING_LOCKED_X:
  case PANNING_LOCKED_Y:
    {
      ReentrantMonitorAutoEnter lock(mMonitor);
      RequestContentRepaint();
      UpdateSharedCompositorFrameMetrics();
    }
    mX.EndTouch();
    mY.EndTouch();
    SetState(FLING);
    StartAnimation(new FlingAnimation(mX, mY));
    return nsEventStatus_eConsumeNoDefault;

  case PINCHING:
    SetState(NOTHING);
    // Scale gesture listener should have handled this.
    NS_WARNING("Gesture listener should have handled pinching in OnTouchEnd.");
    return nsEventStatus_eIgnore;

  case WAITING_CONTENT_RESPONSE:
    NS_WARNING("Received impossible touch in OnTouchEnd");
    break;
  }

  return nsEventStatus_eConsumeNoDefault;
}

nsEventStatus AsyncPanZoomController::OnTouchCancel(const MultiTouchInput& aEvent) {
  APZC_LOG("%p got a touch-cancel in state %d\n", this, mState);
  SetState(NOTHING);
  return nsEventStatus_eConsumeNoDefault;
}

nsEventStatus AsyncPanZoomController::OnScaleBegin(const PinchGestureInput& aEvent) {
  APZC_LOG("%p got a scale-begin in state %d\n", this, mState);

  if (!TouchActionAllowZoom()) {
    return nsEventStatus_eIgnore;
  }

  if (!AllowZoom()) {
    return nsEventStatus_eConsumeNoDefault;
  }

  SetState(PINCHING);
  mLastZoomFocus = aEvent.mFocusPoint - mFrameMetrics.mCompositionBounds.TopLeft();

  return nsEventStatus_eConsumeNoDefault;
}

nsEventStatus AsyncPanZoomController::OnScale(const PinchGestureInput& aEvent) {
  APZC_LOG("%p got a scale in state %d\n", this, mState);
  if (mState != PINCHING) {
    return nsEventStatus_eConsumeNoDefault;
  }

  float prevSpan = aEvent.mPreviousSpan;
  if (fabsf(prevSpan) <= EPSILON || fabsf(aEvent.mCurrentSpan) <= EPSILON) {
    // We're still handling it; we've just decided to throw this event away.
    return nsEventStatus_eConsumeNoDefault;
  }

  float spanRatio = aEvent.mCurrentSpan / aEvent.mPreviousSpan;

  {
    ReentrantMonitorAutoEnter lock(mMonitor);

    CSSToScreenScale userZoom = mFrameMetrics.mZoom;
    ScreenPoint focusPoint = aEvent.mFocusPoint - mFrameMetrics.mCompositionBounds.TopLeft();
    CSSPoint cssFocusPoint = focusPoint / userZoom;

    CSSPoint focusChange = (mLastZoomFocus - focusPoint) / userZoom;
    // If displacing by the change in focus point will take us off page bounds,
    // then reduce the displacement such that it doesn't.
    if (mX.DisplacementWillOverscroll(focusChange.x) != Axis::OVERSCROLL_NONE) {
      focusChange.x -= mX.DisplacementWillOverscrollAmount(focusChange.x);
    }
    if (mY.DisplacementWillOverscroll(focusChange.y) != Axis::OVERSCROLL_NONE) {
      focusChange.y -= mY.DisplacementWillOverscrollAmount(focusChange.y);
    }
    ScrollBy(focusChange);

    // When we zoom in with focus, we can zoom too much towards the boundaries
    // that we actually go over them. These are the needed displacements along
    // either axis such that we don't overscroll the boundaries when zooming.
    CSSPoint neededDisplacement;

    CSSToScreenScale realMinZoom = mZoomConstraints.mMinZoom;
    CSSToScreenScale realMaxZoom = mZoomConstraints.mMaxZoom;
    realMinZoom.scale = std::max(realMinZoom.scale,
                                 mFrameMetrics.mCompositionBounds.width / mFrameMetrics.mScrollableRect.width);
    realMinZoom.scale = std::max(realMinZoom.scale,
                                 mFrameMetrics.mCompositionBounds.height / mFrameMetrics.mScrollableRect.height);
    if (realMaxZoom < realMinZoom) {
      realMaxZoom = realMinZoom;
    }

    bool doScale = (spanRatio > 1.0 && userZoom < realMaxZoom) ||
                   (spanRatio < 1.0 && userZoom > realMinZoom);

    if (doScale) {
      spanRatio = clamped(spanRatio,
                          realMinZoom.scale / userZoom.scale,
                          realMaxZoom.scale / userZoom.scale);

      // Note that the spanRatio here should never put us into OVERSCROLL_BOTH because
      // up above we clamped it.
      neededDisplacement.x = -mX.ScaleWillOverscrollAmount(spanRatio, cssFocusPoint.x);
      neededDisplacement.y = -mY.ScaleWillOverscrollAmount(spanRatio, cssFocusPoint.y);

      ScaleWithFocus(spanRatio, cssFocusPoint);

      if (neededDisplacement != CSSPoint()) {
        ScrollBy(neededDisplacement);
      }

      ScheduleComposite();
      // We don't want to redraw on every scale, so don't use
      // RequestContentRepaint()
      UpdateSharedCompositorFrameMetrics();
    }

    mLastZoomFocus = focusPoint;
  }

  return nsEventStatus_eConsumeNoDefault;
}

nsEventStatus AsyncPanZoomController::OnScaleEnd(const PinchGestureInput& aEvent) {
  APZC_LOG("%p got a scale-end in state %d\n", this, mState);

  SetState(NOTHING);

  {
    ReentrantMonitorAutoEnter lock(mMonitor);
    ScheduleComposite();
    RequestContentRepaint();
    UpdateSharedCompositorFrameMetrics();
  }

  return nsEventStatus_eConsumeNoDefault;
}

bool
AsyncPanZoomController::ConvertToGecko(const ScreenPoint& aPoint, CSSIntPoint* aOut)
{
  APZCTreeManager* treeManagerLocal = mTreeManager;
  if (treeManagerLocal) {
    gfx3DMatrix transformToApzc;
    gfx3DMatrix transformToGecko;
    treeManagerLocal->GetInputTransforms(this, transformToApzc, transformToGecko);
    gfxPoint result = transformToGecko.Transform(gfxPoint(aPoint.x, aPoint.y));
    // NOTE: This isn't *quite* LayoutDevicePoint, we just don't have a name
    // for this coordinate space and it maps the closest to LayoutDevicePoint.
    LayoutDevicePoint layoutPoint = LayoutDevicePoint(result.x, result.y);
    { // scoped lock to access mFrameMetrics
      ReentrantMonitorAutoEnter lock(mMonitor);
      CSSPoint cssPoint = layoutPoint / mFrameMetrics.mDevPixelsPerCSSPixel;
      *aOut = gfx::RoundedToInt(cssPoint);
    }
    return true;
  }
  return false;
}

nsEventStatus AsyncPanZoomController::OnLongPress(const TapGestureInput& aEvent) {
  APZC_LOG("%p got a long-press in state %d\n", this, mState);
  nsRefPtr<GeckoContentController> controller = GetGeckoContentController();
  if (controller) {
    int32_t modifiers = WidgetModifiersToDOMModifiers(aEvent.modifiers);
    CSSIntPoint geckoScreenPoint;
    if (ConvertToGecko(aEvent.mPoint, &geckoScreenPoint)) {
      controller->HandleLongTap(geckoScreenPoint, modifiers);
      return nsEventStatus_eConsumeNoDefault;
    }
  }
  return nsEventStatus_eIgnore;
}

nsEventStatus AsyncPanZoomController::OnLongPressUp(const TapGestureInput& aEvent) {
  APZC_LOG("%p got a long-tap-up in state %d\n", this, mState);
  nsRefPtr<GeckoContentController> controller = GetGeckoContentController();
  if (controller) {
    int32_t modifiers = WidgetModifiersToDOMModifiers(aEvent.modifiers);
    CSSIntPoint geckoScreenPoint;
    if (ConvertToGecko(aEvent.mPoint, &geckoScreenPoint)) {
      controller->HandleLongTapUp(geckoScreenPoint, modifiers);
      return nsEventStatus_eConsumeNoDefault;
    }
  }
  return nsEventStatus_eIgnore;
}

nsEventStatus AsyncPanZoomController::OnSingleTapUp(const TapGestureInput& aEvent) {
  APZC_LOG("%p got a single-tap-up in state %d\n", this, mState);
  nsRefPtr<GeckoContentController> controller = GetGeckoContentController();
  // If mZoomConstraints.mAllowZoom is true we wait for a call to OnSingleTapConfirmed before
  // sending event to content
  if (controller && !AllowZoom()) {
    int32_t modifiers = WidgetModifiersToDOMModifiers(aEvent.modifiers);
    CSSIntPoint geckoScreenPoint;
    if (ConvertToGecko(aEvent.mPoint, &geckoScreenPoint)) {
      controller->HandleSingleTap(geckoScreenPoint, modifiers);
      return nsEventStatus_eConsumeNoDefault;
    }
  }
  return nsEventStatus_eIgnore;
}

nsEventStatus AsyncPanZoomController::OnSingleTapConfirmed(const TapGestureInput& aEvent) {
  APZC_LOG("%p got a single-tap-confirmed in state %d\n", this, mState);
  nsRefPtr<GeckoContentController> controller = GetGeckoContentController();
  if (controller) {
    int32_t modifiers = WidgetModifiersToDOMModifiers(aEvent.modifiers);
    CSSIntPoint geckoScreenPoint;
    if (ConvertToGecko(aEvent.mPoint, &geckoScreenPoint)) {
      controller->HandleSingleTap(geckoScreenPoint, modifiers);
      return nsEventStatus_eConsumeNoDefault;
    }
  }
  return nsEventStatus_eIgnore;
}

nsEventStatus AsyncPanZoomController::OnDoubleTap(const TapGestureInput& aEvent) {
  APZC_LOG("%p got a double-tap in state %d\n", this, mState);
  nsRefPtr<GeckoContentController> controller = GetGeckoContentController();
  if (controller) {
    if (AllowZoom()) {
      int32_t modifiers = WidgetModifiersToDOMModifiers(aEvent.modifiers);
      CSSIntPoint geckoScreenPoint;
      if (ConvertToGecko(aEvent.mPoint, &geckoScreenPoint)) {
        controller->HandleDoubleTap(geckoScreenPoint, modifiers);
      }
    }

    return nsEventStatus_eConsumeNoDefault;
  }
  return nsEventStatus_eIgnore;
}

nsEventStatus AsyncPanZoomController::OnCancelTap(const TapGestureInput& aEvent) {
  APZC_LOG("%p got a cancel-tap in state %d\n", this, mState);
  // XXX: Implement this.
  return nsEventStatus_eIgnore;
}

float AsyncPanZoomController::PanDistance() {
  ReentrantMonitorAutoEnter lock(mMonitor);
  return NS_hypot(mX.PanDistance(), mY.PanDistance());
}

const ScreenPoint AsyncPanZoomController::GetVelocityVector() {
  return ScreenPoint(mX.GetVelocity(), mY.GetVelocity());
}

const gfx::Point AsyncPanZoomController::GetAccelerationVector() {
  return gfx::Point(mX.GetAccelerationFactor(), mY.GetAccelerationFactor());
}

void AsyncPanZoomController::HandlePanningWithTouchAction(double aAngle, TouchBehaviorFlags aBehavior) {
  // Handling of cross sliding will need to be added in this method after touch-action released
  // enabled by default.
  if ((aBehavior & AllowedTouchBehavior::VERTICAL_PAN) && (aBehavior & AllowedTouchBehavior::HORIZONTAL_PAN)) {
    if (mX.Scrollable() && mY.Scrollable()) {
      if (IsCloseToHorizontal(aAngle, AXIS_LOCK_ANGLE)) {
        mY.SetAxisLocked(true);
        SetState(PANNING_LOCKED_X);
      } else if (IsCloseToVertical(aAngle, AXIS_LOCK_ANGLE)) {
        mX.SetAxisLocked(true);
        SetState(PANNING_LOCKED_Y);
      } else {
        SetState(PANNING);
      }
    } else if (mX.Scrollable() || mY.Scrollable()) {
      SetState(PANNING);
    } else {
      SetState(NOTHING);
    }
  } else if (aBehavior & AllowedTouchBehavior::HORIZONTAL_PAN) {
    // Using bigger angle for panning to keep behavior consistent
    // with IE.
    if (IsCloseToHorizontal(aAngle, ALLOWED_DIRECT_PAN_ANGLE)) {
      mY.SetAxisLocked(true);
      SetState(PANNING_LOCKED_X);
      mPanDirRestricted = true;
    } else {
      // Don't treat these touches as pan/zoom movements since 'touch-action' value
      // requires it.
      SetState(NOTHING);
    }
  } else if (aBehavior & AllowedTouchBehavior::VERTICAL_PAN) {
    if (IsCloseToVertical(aAngle, ALLOWED_DIRECT_PAN_ANGLE)) {
      mX.SetAxisLocked(true);
      SetState(PANNING_LOCKED_Y);
      mPanDirRestricted = true;
    } else {
      SetState(NOTHING);
    }
  } else {
    SetState(NOTHING);
  }
}

void AsyncPanZoomController::HandlePanning(double aAngle) {
  if (!gCrossSlideEnabled && (!mX.Scrollable() || !mY.Scrollable())) {
    SetState(PANNING);
  } else if (IsCloseToHorizontal(aAngle, AXIS_LOCK_ANGLE)) {
    mY.SetAxisLocked(true);
    if (mX.Scrollable()) {
      SetState(PANNING_LOCKED_X);
    } else {
      SetState(CROSS_SLIDING_X);
      mX.SetAxisLocked(true);
    }
  } else if (IsCloseToVertical(aAngle, AXIS_LOCK_ANGLE)) {
    mX.SetAxisLocked(true);
    if (mY.Scrollable()) {
      SetState(PANNING_LOCKED_Y);
    } else {
      SetState(CROSS_SLIDING_Y);
      mY.SetAxisLocked(true);
    }
  } else {
    SetState(PANNING);
  }
}

nsEventStatus AsyncPanZoomController::StartPanning(const MultiTouchInput& aEvent) {
  ReentrantMonitorAutoEnter lock(mMonitor);

  ScreenIntPoint point = GetFirstTouchScreenPoint(aEvent);
  float dx = mX.PanDistance(point.x);
  float dy = mY.PanDistance(point.y);

  // When the touch move breaks through the pan threshold, reposition the touch down origin
  // so the page won't jump when we start panning.
  mX.StartTouch(point.x);
  mY.StartTouch(point.y);
  mLastEventTime = aEvent.mTime;

  double angle = atan2(dy, dx); // range [-pi, pi]
  angle = fabs(angle); // range [0, pi]

  if (mTouchActionPropertyEnabled) {
    HandlePanningWithTouchAction(angle, GetTouchBehavior(0));
  } else {
    if (GetAxisLockMode() == FREE) {
      SetState(PANNING);
      return nsEventStatus_eConsumeNoDefault;
    }

    HandlePanning(angle);
  }

  // Don't consume an event that didn't trigger a panning.
  return IsPanningState(mState) ? nsEventStatus_eConsumeNoDefault
                                : nsEventStatus_eIgnore;
}

void AsyncPanZoomController::UpdateWithTouchAtDevicePoint(const MultiTouchInput& aEvent) {
  ScreenIntPoint point = GetFirstTouchScreenPoint(aEvent);
  TimeDuration timeDelta = TimeDuration().FromMilliseconds(aEvent.mTime - mLastEventTime);

  // Probably a duplicate event, just throw it away.
  if (timeDelta.ToMilliseconds() <= EPSILON) {
    return;
  }

  mX.UpdateWithTouchAtDevicePoint(point.x, timeDelta);
  mY.UpdateWithTouchAtDevicePoint(point.y, timeDelta);
}

void AsyncPanZoomController::AttemptScroll(const ScreenPoint& aStartPoint,
                                           const ScreenPoint& aEndPoint,
                                           uint32_t aOverscrollHandoffChainIndex) {

  // "start - end" rather than "end - start" because e.g. moving your finger
  // down (*positive* direction along y axis) causes the vertical scroll offset
  // to *decrease* as the page follows your finger.
  ScreenPoint displacement = aStartPoint - aEndPoint;

  ScreenPoint overscroll;  // will be used outside monitor block
  {
    ReentrantMonitorAutoEnter lock(mMonitor);

    CSSToScreenScale zoom = mFrameMetrics.mZoom;

    // Inversely scale the offset by the resolution (when you're zoomed further in,
    // a larger swipe should move you a shorter distance).
    CSSPoint cssDisplacement = displacement / zoom;

    CSSPoint cssOverscroll;
    gfx::Point scrollOffset(mX.AdjustDisplacement(cssDisplacement.x,
                                                  cssOverscroll.x,
                                                  mFrameMetrics.GetDisableScrollingX()),
                            mY.AdjustDisplacement(cssDisplacement.y,
                                                  cssOverscroll.y,
                                                  mFrameMetrics.GetDisableScrollingY()));
    overscroll = cssOverscroll * zoom;

    if (fabs(scrollOffset.x) > EPSILON || fabs(scrollOffset.y) > EPSILON) {
      ScrollBy(CSSPoint::FromUnknownPoint(scrollOffset));
      ScheduleComposite();

      TimeDuration timePaintDelta = mPaintThrottler.TimeSinceLastRequest(GetFrameTime());
      if (timePaintDelta.ToMilliseconds() > gPanRepaintInterval) {
        RequestContentRepaint();
      }
      UpdateSharedCompositorFrameMetrics();
    }
  }

  if (fabs(overscroll.x) > EPSILON || fabs(overscroll.y) > EPSILON) {
    // "+ overscroll" rather than "- overscroll" because "overscroll" is what's
    // left of "displacement", and "displacement" is "start - end".
    CallDispatchScroll(aEndPoint + overscroll, aEndPoint, aOverscrollHandoffChainIndex + 1);
  }
}

void AsyncPanZoomController::CallDispatchScroll(const ScreenPoint& aStartPoint, const ScreenPoint& aEndPoint,
                                                uint32_t aOverscrollHandoffChainIndex) {
  // Make a local copy of the tree manager pointer and check if it's not
  // null before calling HandleOverscroll(). This is necessary because
  // Destroy(), which nulls out mTreeManager, could be called concurrently.
  APZCTreeManager* treeManagerLocal = mTreeManager;
  if (treeManagerLocal) {
    treeManagerLocal->DispatchScroll(this, aStartPoint, aEndPoint,
                                     aOverscrollHandoffChainIndex);
  }
}

void AsyncPanZoomController::TrackTouch(const MultiTouchInput& aEvent) {
  ScreenIntPoint prevTouchPoint(mX.GetPos(), mY.GetPos());
  ScreenIntPoint touchPoint = GetFirstTouchScreenPoint(aEvent);
  TimeDuration timeDelta = TimeDuration().FromMilliseconds(aEvent.mTime - mLastEventTime);

  // Probably a duplicate event, just throw it away.
  if (timeDelta.ToMilliseconds() <= EPSILON) {
    return;
  }

  // If we're axis-locked, check if the user is trying to break the lock
  if (GetAxisLockMode() == STICKY && !mPanDirRestricted) {
    ScreenIntPoint point = GetFirstTouchScreenPoint(aEvent);
    float dx = mX.PanDistance(point.x);
    float dy = mY.PanDistance(point.y);

    double angle = atan2(dy, dx); // range [-pi, pi]
    angle = fabs(angle); // range [0, pi]

    float breakThreshold = AXIS_BREAKOUT_THRESHOLD * APZCTreeManager::GetDPI();

    if (fabs(dx) > breakThreshold || fabs(dy) > breakThreshold) {
      if (mState == PANNING_LOCKED_X || mState == CROSS_SLIDING_X) {
        if (!IsCloseToHorizontal(angle, AXIS_BREAKOUT_ANGLE)) {
          mY.SetAxisLocked(false);
          SetState(PANNING);
        }
      } else if (mState == PANNING_LOCKED_Y || mState == CROSS_SLIDING_Y) {
        if (!IsCloseToVertical(angle, AXIS_BREAKOUT_ANGLE)) {
          mX.SetAxisLocked(false);
          SetState(PANNING);
        }
      }
    }
  }

  UpdateWithTouchAtDevicePoint(aEvent);

  CallDispatchScroll(prevTouchPoint, touchPoint, 0);
}

ScreenIntPoint& AsyncPanZoomController::GetFirstTouchScreenPoint(const MultiTouchInput& aEvent) {
  return ((SingleTouchData&)aEvent.mTouches[0]).mScreenPoint;
}

bool FlingAnimation::Sample(FrameMetrics& aFrameMetrics,
                            const TimeDuration& aDelta) {
  bool shouldContinueFlingX = mX.FlingApplyFrictionOrCancel(aDelta),
       shouldContinueFlingY = mY.FlingApplyFrictionOrCancel(aDelta);
  // If we shouldn't continue the fling, let's just stop and repaint.
  if (!shouldContinueFlingX && !shouldContinueFlingY) {
    return false;
  }

  CSSPoint overscroll; // overscroll is ignored for flings
  ScreenPoint offset(aDelta.ToMilliseconds() * mX.GetVelocity(),
                     aDelta.ToMilliseconds() * mY.GetVelocity());

  // Inversely scale the offset by the resolution (when you're zoomed further in,
  // a larger swipe should move you a shorter distance).
  CSSPoint cssOffset = offset / aFrameMetrics.mZoom;
  aFrameMetrics.mScrollOffset += CSSPoint::FromUnknownPoint(gfx::Point(
    mX.AdjustDisplacement(cssOffset.x, overscroll.x,
                          aFrameMetrics.GetDisableScrollingX()),
    mY.AdjustDisplacement(cssOffset.y, overscroll.y,
                          aFrameMetrics.GetDisableScrollingY())
  ));

  return true;
}

void AsyncPanZoomController::StartAnimation(AsyncPanZoomAnimation* aAnimation)
{
  ReentrantMonitorAutoEnter lock(mMonitor);
  mAnimation = aAnimation;
  mLastSampleTime = GetFrameTime();
  ScheduleComposite();
}

void AsyncPanZoomController::CancelAnimation() {
  ReentrantMonitorAutoEnter lock(mMonitor);
  SetState(NOTHING);
  mAnimation = nullptr;
}

void AsyncPanZoomController::SetCompositorParent(CompositorParent* aCompositorParent) {
  mCompositorParent = aCompositorParent;
}

void AsyncPanZoomController::SetCrossProcessCompositorParent(PCompositorParent* aCrossProcessCompositorParent) {
  mCrossProcessCompositorParent = aCrossProcessCompositorParent;
}

void AsyncPanZoomController::ScrollBy(const CSSPoint& aOffset) {
  mFrameMetrics.mScrollOffset += aOffset;
}

void AsyncPanZoomController::ScaleWithFocus(float aScale,
                                            const CSSPoint& aFocus) {
  mFrameMetrics.mZoom.scale *= aScale;
  // We want to adjust the scroll offset such that the CSS point represented by aFocus remains
  // at the same position on the screen before and after the change in zoom. The below code
  // accomplishes this; see https://bugzilla.mozilla.org/show_bug.cgi?id=923431#c6 for an
  // in-depth explanation of how.
  mFrameMetrics.mScrollOffset = (mFrameMetrics.mScrollOffset + aFocus) - (aFocus / aScale);
}

/**
 * Attempts to enlarge the displayport along a single axis based on the
 * velocity. aOffset and aLength are in/out parameters; they are initially set
 * to the currently visible area and will be transformed to the area we should
 * be drawing to minimize checkerboarding.
 */
static void
EnlargeDisplayPortAlongAxis(float* aOutOffset, float* aOutLength,
                            double aEstimatedPaintDurationMillis, float aVelocity,
                            float aStationarySizeMultiplier, float aSkateSizeMultiplier)
{
  // Scale up the length using the appropriate multiplier and center the
  // displayport around the visible area.
  float multiplier = (fabsf(aVelocity) < gMinSkateSpeed
                        ? aStationarySizeMultiplier
                        : aSkateSizeMultiplier);
  float newLength = (*aOutLength) * multiplier;
  *aOutOffset -= (newLength - (*aOutLength)) / 2;
  *aOutLength = newLength;

  // Project the displayport out based on the estimated time it will take to paint,
  // if the gUsePaintDuration flag is set. If not, just use a constant 50ms paint
  // time. Setting the gVelocityBias pref appropriately can cancel this out if so
  // desired.
  double paintFactor = (gUsePaintDuration ? aEstimatedPaintDurationMillis : 50.0);
  *aOutOffset += (aVelocity * paintFactor * gVelocityBias);
}

/* static */
const CSSRect AsyncPanZoomController::CalculatePendingDisplayPort(
  const FrameMetrics& aFrameMetrics,
  const ScreenPoint& aVelocity,
  const gfx::Point& aAcceleration,
  double aEstimatedPaintDuration)
{
  // convert to milliseconds
  double estimatedPaintDurationMillis = aEstimatedPaintDuration * 1000;

  CSSRect compositionBounds = aFrameMetrics.CalculateCompositedRectInCssPixels();
  CSSPoint scrollOffset = aFrameMetrics.mScrollOffset;
  CSSRect displayPort(scrollOffset, compositionBounds.Size());
  CSSPoint velocity = aVelocity / aFrameMetrics.mZoom;

  // If scrolling is disabled here then our actual velocity is going
  // to be zero, so treat the displayport accordingly.
  if (aFrameMetrics.GetDisableScrollingX()) {
    velocity.x = 0;
  }
  if (aFrameMetrics.GetDisableScrollingY()) {
    velocity.y = 0;
  }

  // Enlarge the displayport along both axes depending on how fast we're moving
  // on that axis and how long it takes to paint. Apply some heuristics to try
  // to minimize checkerboarding.
  EnlargeDisplayPortAlongAxis(&(displayPort.x), &(displayPort.width),
    estimatedPaintDurationMillis, velocity.x,
    gXStationarySizeMultiplier, gXSkateSizeMultiplier);
  EnlargeDisplayPortAlongAxis(&(displayPort.y), &(displayPort.height),
    estimatedPaintDurationMillis, velocity.y,
    gYStationarySizeMultiplier, gYSkateSizeMultiplier);

  CSSRect scrollableRect = aFrameMetrics.GetExpandedScrollableRect();
  displayPort = displayPort.ForceInside(scrollableRect) - scrollOffset;

  APZC_LOG_FM(aFrameMetrics,
    "Calculated displayport as (%f %f %f %f) from velocity (%f %f) acceleration (%f %f) paint time %f metrics",
    displayPort.x, displayPort.y, displayPort.width, displayPort.height,
    aVelocity.x, aVelocity.y, aAcceleration.x, aAcceleration.y,
    (float)estimatedPaintDurationMillis);

  return displayPort;
}

void AsyncPanZoomController::ScheduleComposite() {
  if (mCompositorParent) {
    mCompositorParent->ScheduleRenderOnCompositorThread();
  }
}

void AsyncPanZoomController::RequestContentRepaint() {
  RequestContentRepaint(mFrameMetrics);
}

void AsyncPanZoomController::RequestContentRepaint(FrameMetrics& aFrameMetrics) {
  aFrameMetrics.mDisplayPort =
    CalculatePendingDisplayPort(aFrameMetrics,
                                GetVelocityVector(),
                                GetAccelerationVector(),
                                mPaintThrottler.AverageDuration().ToSeconds());

  // If we're trying to paint what we already think is painted, discard this
  // request since it's a pointless paint.
  CSSRect oldDisplayPort = mLastPaintRequestMetrics.mDisplayPort
                         + mLastPaintRequestMetrics.mScrollOffset;
  CSSRect newDisplayPort = aFrameMetrics.mDisplayPort
                         + aFrameMetrics.mScrollOffset;

  if (fabsf(oldDisplayPort.x - newDisplayPort.x) < EPSILON &&
      fabsf(oldDisplayPort.y - newDisplayPort.y) < EPSILON &&
      fabsf(oldDisplayPort.width - newDisplayPort.width) < EPSILON &&
      fabsf(oldDisplayPort.height - newDisplayPort.height) < EPSILON &&
      fabsf(mLastPaintRequestMetrics.mScrollOffset.x -
            aFrameMetrics.mScrollOffset.x) < EPSILON &&
      fabsf(mLastPaintRequestMetrics.mScrollOffset.y -
            aFrameMetrics.mScrollOffset.y) < EPSILON &&
      aFrameMetrics.mZoom == mLastPaintRequestMetrics.mZoom &&
      fabsf(aFrameMetrics.mViewport.width - mLastPaintRequestMetrics.mViewport.width) < EPSILON &&
      fabsf(aFrameMetrics.mViewport.height - mLastPaintRequestMetrics.mViewport.height) < EPSILON) {
    return;
  }

  SendAsyncScrollEvent();
  mPaintThrottler.PostTask(
    FROM_HERE,
    NewRunnableMethod(this,
                      &AsyncPanZoomController::DispatchRepaintRequest,
                      aFrameMetrics),
    GetFrameTime());

  aFrameMetrics.mPresShellId = mLastContentPaintMetrics.mPresShellId;
  mLastPaintRequestMetrics = aFrameMetrics;
}

void
AsyncPanZoomController::DispatchRepaintRequest(const FrameMetrics& aFrameMetrics) {
  nsRefPtr<GeckoContentController> controller = GetGeckoContentController();
  if (controller) {
    APZC_LOG_FM(aFrameMetrics, "%p requesting content repaint", this);

    LogRendertraceRect(GetGuid(), "requested displayport", "yellow",
        aFrameMetrics.mDisplayPort + aFrameMetrics.mScrollOffset);

    controller->RequestContentRepaint(aFrameMetrics);
    mLastDispatchedPaintMetrics = aFrameMetrics;
  }
}

void
AsyncPanZoomController::FireAsyncScrollOnTimeout()
{
  if (mCurrentAsyncScrollOffset != mLastAsyncScrollOffset) {
    ReentrantMonitorAutoEnter lock(mMonitor);
    SendAsyncScrollEvent();
  }
  mAsyncScrollTimeoutTask = nullptr;
}

bool ZoomAnimation::Sample(FrameMetrics& aFrameMetrics,
                           const TimeDuration& aDelta) {
  mDuration += aDelta;
  double animPosition = mDuration / ZOOM_TO_DURATION;

  if (animPosition >= 1.0) {
    aFrameMetrics.mZoom = mEndZoom;
    aFrameMetrics.mScrollOffset = mEndOffset;
    return false;
  }

  // Sample the zoom at the current time point.  The sampled zoom
  // will affect the final computed resolution.
  double sampledPosition = gComputedTimingFunction->GetValue(animPosition);

  // We scale the scrollOffset linearly with sampledPosition, so the zoom
  // needs to scale inversely to match.
  aFrameMetrics.mZoom = CSSToScreenScale(1 /
    (sampledPosition / mEndZoom.scale +
    (1 - sampledPosition) / mStartZoom.scale));

  aFrameMetrics.mScrollOffset = CSSPoint::FromUnknownPoint(gfx::Point(
    mEndOffset.x * sampledPosition + mStartOffset.x * (1 - sampledPosition),
    mEndOffset.y * sampledPosition + mStartOffset.y * (1 - sampledPosition)
  ));

  return true;
}

bool AsyncPanZoomController::UpdateAnimation(const TimeStamp& aSampleTime)
{
  if (mAnimation) {
    if (mAnimation->Sample(mFrameMetrics, aSampleTime - mLastSampleTime)) {
      if (mPaintThrottler.TimeSinceLastRequest(aSampleTime) >
          mAnimation->mRepaintInterval) {
        RequestContentRepaint();
      }
    } else {
      mAnimation = nullptr;
      SetState(NOTHING);
      SendAsyncScrollEvent();
      RequestContentRepaint();
    }
    UpdateSharedCompositorFrameMetrics();
    mLastSampleTime = aSampleTime;
    return true;
  }
  return false;
}

bool AsyncPanZoomController::SampleContentTransformForFrame(const TimeStamp& aSampleTime,
                                                            ViewTransform* aNewTransform,
                                                            ScreenPoint& aScrollOffset) {
  // The eventual return value of this function. The compositor needs to know
  // whether or not to advance by a frame as soon as it can. For example, if a
  // fling is happening, it has to keep compositing so that the animation is
  // smooth. If an animation frame is requested, it is the compositor's
  // responsibility to schedule a composite.
  bool requestAnimationFrame = false;

  {
    ReentrantMonitorAutoEnter lock(mMonitor);

    requestAnimationFrame = UpdateAnimation(aSampleTime);

    aScrollOffset = mFrameMetrics.mScrollOffset * mFrameMetrics.mZoom;
    *aNewTransform = GetCurrentAsyncTransform();

    LogRendertraceRect(GetGuid(), "viewport", "red",
      CSSRect(mFrameMetrics.mScrollOffset,
              ScreenSize(mFrameMetrics.mCompositionBounds.Size()) / mFrameMetrics.mZoom));

    mCurrentAsyncScrollOffset = mFrameMetrics.mScrollOffset;
  }

  // Cancel the mAsyncScrollTimeoutTask because we will fire a
  // mozbrowserasyncscroll event or renew the mAsyncScrollTimeoutTask again.
  if (mAsyncScrollTimeoutTask) {
    mAsyncScrollTimeoutTask->Cancel();
    mAsyncScrollTimeoutTask = nullptr;
  }
  // Fire the mozbrowserasyncscroll event immediately if it's been
  // sAsyncScrollThrottleTime ms since the last time we fired the event and the
  // current scroll offset is different than the mLastAsyncScrollOffset we sent
  // with the last event.
  // Otherwise, start a timer to fire the event sAsyncScrollTimeout ms from now.
  TimeDuration delta = aSampleTime - mLastAsyncScrollTime;
  if (delta.ToMilliseconds() > gAsyncScrollThrottleTime &&
      mCurrentAsyncScrollOffset != mLastAsyncScrollOffset) {
    ReentrantMonitorAutoEnter lock(mMonitor);
    mLastAsyncScrollTime = aSampleTime;
    mLastAsyncScrollOffset = mCurrentAsyncScrollOffset;
    SendAsyncScrollEvent();
  }
  else {
    mAsyncScrollTimeoutTask =
      NewRunnableMethod(this, &AsyncPanZoomController::FireAsyncScrollOnTimeout);
    MessageLoop::current()->PostDelayedTask(FROM_HERE,
                                            mAsyncScrollTimeoutTask,
                                            gAsyncScrollTimeout);
  }

  return requestAnimationFrame;
}

ViewTransform AsyncPanZoomController::GetCurrentAsyncTransform() {
  ReentrantMonitorAutoEnter lock(mMonitor);

  CSSPoint lastPaintScrollOffset;
  if (mLastContentPaintMetrics.IsScrollable()) {
    lastPaintScrollOffset = mLastContentPaintMetrics.mScrollOffset;
  }
  LayerPoint translation = (mFrameMetrics.mScrollOffset - lastPaintScrollOffset)
                         * mLastContentPaintMetrics.LayersPixelsPerCSSPixel();

  return ViewTransform(-translation,
                       mFrameMetrics.mZoom
                     / mLastContentPaintMetrics.mDevPixelsPerCSSPixel
                     / mFrameMetrics.GetParentResolution());
}

gfx3DMatrix AsyncPanZoomController::GetNontransientAsyncTransform() {
  ReentrantMonitorAutoEnter lock(mMonitor);
  return gfx3DMatrix::ScalingMatrix(mLastContentPaintMetrics.mResolution.scale,
                                    mLastContentPaintMetrics.mResolution.scale,
                                    1.0f);
}

gfx3DMatrix AsyncPanZoomController::GetTransformToLastDispatchedPaint() {
  ReentrantMonitorAutoEnter lock(mMonitor);
  CSSPoint scrollChange = mLastContentPaintMetrics.mScrollOffset - mLastDispatchedPaintMetrics.mScrollOffset;
  float zoomChange = mLastContentPaintMetrics.mZoom.scale / mLastDispatchedPaintMetrics.mZoom.scale;
  return gfx3DMatrix::Translation(scrollChange.x, scrollChange.y, 0) *
         gfx3DMatrix::ScalingMatrix(zoomChange, zoomChange, 1);
}

void AsyncPanZoomController::NotifyLayersUpdated(const FrameMetrics& aLayerMetrics, bool aIsFirstPaint) {
  ReentrantMonitorAutoEnter lock(mMonitor);

  mLastContentPaintMetrics = aLayerMetrics;

  bool isDefault = mFrameMetrics.IsDefault();
  mFrameMetrics.mMayHaveTouchListeners = aLayerMetrics.mMayHaveTouchListeners;
  APZC_LOG_FM(aLayerMetrics, "%p got a NotifyLayersUpdated with aIsFirstPaint=%d", this, aIsFirstPaint);

  LogRendertraceRect(GetGuid(), "page", "brown", aLayerMetrics.mScrollableRect);
  LogRendertraceRect(GetGuid(), "painted displayport", "green",
    aLayerMetrics.mDisplayPort + aLayerMetrics.mScrollOffset);

  mPaintThrottler.TaskComplete(GetFrameTime());
  bool needContentRepaint = false;
  if (aLayerMetrics.mCompositionBounds.width == mFrameMetrics.mCompositionBounds.width &&
      aLayerMetrics.mCompositionBounds.height == mFrameMetrics.mCompositionBounds.height) {
    // Remote content has sync'd up to the composition geometry
    // change, so we can accept the viewport it's calculated.
    if (mFrameMetrics.mViewport.width != aLayerMetrics.mViewport.width ||
        mFrameMetrics.mViewport.height != aLayerMetrics.mViewport.height) {
      needContentRepaint = true;
    }
    mFrameMetrics.mViewport = aLayerMetrics.mViewport;
  }

  if (aIsFirstPaint || isDefault) {
    // Initialize our internal state to something sane when the content
    // that was just painted is something we knew nothing about previously
    mPaintThrottler.ClearHistory();
    mPaintThrottler.SetMaxDurations(gNumPaintDurationSamples);

    mX.CancelTouch();
    mY.CancelTouch();
    SetState(NOTHING);

    mFrameMetrics = aLayerMetrics;
    mLastDispatchedPaintMetrics = aLayerMetrics;
    ShareCompositorFrameMetrics();
  } else {
    // If we're not taking the aLayerMetrics wholesale we still need to pull
    // in some things into our local mFrameMetrics because these things are
    // determined by Gecko and our copy in mFrameMetrics may be stale.
    mFrameMetrics.mScrollableRect = aLayerMetrics.mScrollableRect;
    mFrameMetrics.mCompositionBounds = aLayerMetrics.mCompositionBounds;
    float parentResolutionChange = aLayerMetrics.GetParentResolution().scale
                                 / mFrameMetrics.GetParentResolution().scale;
    mFrameMetrics.mZoom.scale *= parentResolutionChange;
    mFrameMetrics.mResolution = aLayerMetrics.mResolution;
    mFrameMetrics.mCumulativeResolution = aLayerMetrics.mCumulativeResolution;
    mFrameMetrics.mHasScrollgrab = aLayerMetrics.mHasScrollgrab;
    mFrameMetrics.SetDisableScrollingX(aLayerMetrics.GetDisableScrollingX());
    mFrameMetrics.SetDisableScrollingY(aLayerMetrics.GetDisableScrollingY());

    // If the layers update was not triggered by our own repaint request, then
    // we want to take the new scroll offset.
    if (aLayerMetrics.mUpdateScrollOffset) {
      APZC_LOG("%p updating scroll offset from (%f, %f) to (%f, %f)\n", this,
        mFrameMetrics.mScrollOffset.x, mFrameMetrics.mScrollOffset.y,
        aLayerMetrics.mScrollOffset.x, aLayerMetrics.mScrollOffset.y);

      mFrameMetrics.mScrollOffset = aLayerMetrics.mScrollOffset;

      // It is possible that when we receive this mUpdateScrollOffset flag, we have
      // just sent a content repaint request, and it is pending inflight. That repaint
      // request would have our old scroll offset, and will get processed on the content
      // thread as we're processing this mUpdateScrollOffset flag. This would leave
      // things in a state where content has the old APZC scroll offset and the APZC
      // has the new content-specified scroll offset. In such a case we want to trigger
      // another repaint request to bring things back in sync. In most cases this repaint
      // request will be a no-op and get filtered out in RequestContentRepaint, so it
      // shouldn't have bad performance implications.
      needContentRepaint = true;
    }
  }

  if (needContentRepaint) {
    RequestContentRepaint();
  }
  UpdateSharedCompositorFrameMetrics();
}

const FrameMetrics& AsyncPanZoomController::GetFrameMetrics() {
  mMonitor.AssertCurrentThreadIn();
  return mFrameMetrics;
}

void AsyncPanZoomController::ZoomToRect(CSSRect aRect) {
  SetState(ANIMATING_ZOOM);

  {
    ReentrantMonitorAutoEnter lock(mMonitor);

    ScreenIntRect compositionBounds = mFrameMetrics.mCompositionBounds;
    CSSRect cssPageRect = mFrameMetrics.mScrollableRect;
    CSSPoint scrollOffset = mFrameMetrics.mScrollOffset;
    CSSToScreenScale currentZoom = mFrameMetrics.mZoom;
    CSSToScreenScale targetZoom;

    // The minimum zoom to prevent over-zoom-out.
    // If the zoom factor is lower than this (i.e. we are zoomed more into the page),
    // then the CSS content rect, in layers pixels, will be smaller than the
    // composition bounds. If this happens, we can't fill the target composited
    // area with this frame.
    CSSToScreenScale localMinZoom(std::max(mZoomConstraints.mMinZoom.scale,
                                  std::max(compositionBounds.width / cssPageRect.width,
                                           compositionBounds.height / cssPageRect.height)));
    CSSToScreenScale localMaxZoom = mZoomConstraints.mMaxZoom;

    if (!aRect.IsEmpty()) {
      // Intersect the zoom-to-rect to the CSS rect to make sure it fits.
      aRect = aRect.Intersect(cssPageRect);
      targetZoom = CSSToScreenScale(std::min(compositionBounds.width / aRect.width,
                                             compositionBounds.height / aRect.height));
    }
    // 1. If the rect is empty, request received from browserElementScrolling.js
    // 2. currentZoom is equal to mZoomConstraints.mMaxZoom and user still double-tapping it
    // 3. currentZoom is equal to localMinZoom and user still double-tapping it
    // Treat these three cases as a request to zoom out as much as possible.
    if (aRect.IsEmpty() ||
        (currentZoom == localMaxZoom && targetZoom >= localMaxZoom) ||
        (currentZoom == localMinZoom && targetZoom <= localMinZoom)) {
      CSSRect compositedRect = mFrameMetrics.CalculateCompositedRectInCssPixels();
      float y = scrollOffset.y;
      float newHeight =
        cssPageRect.width * (compositedRect.height / compositedRect.width);
      float dh = compositedRect.height - newHeight;

      aRect = CSSRect(0.0f,
                           y + dh/2,
                           cssPageRect.width,
                           newHeight);
      aRect = aRect.Intersect(cssPageRect);
      targetZoom = CSSToScreenScale(std::min(compositionBounds.width / aRect.width,
                                             compositionBounds.height / aRect.height));
    }

    targetZoom.scale = clamped(targetZoom.scale, localMinZoom.scale, localMaxZoom.scale);
    FrameMetrics endZoomToMetrics = mFrameMetrics;
    endZoomToMetrics.mZoom = targetZoom;

    // Adjust the zoomToRect to a sensible position to prevent overscrolling.
    CSSRect rectAfterZoom = endZoomToMetrics.CalculateCompositedRectInCssPixels();

    // If either of these conditions are met, the page will be
    // overscrolled after zoomed
    if (aRect.y + rectAfterZoom.height > cssPageRect.height) {
      aRect.y = cssPageRect.height - rectAfterZoom.height;
      aRect.y = aRect.y > 0 ? aRect.y : 0;
    }
    if (aRect.x + rectAfterZoom.width > cssPageRect.width) {
      aRect.x = cssPageRect.width - rectAfterZoom.width;
      aRect.x = aRect.x > 0 ? aRect.x : 0;
    }

    endZoomToMetrics.mScrollOffset = aRect.TopLeft();
    endZoomToMetrics.mDisplayPort =
      CalculatePendingDisplayPort(endZoomToMetrics,
                                  ScreenPoint(0,0),
                                  gfx::Point(0,0),
                                  0);

    StartAnimation(new ZoomAnimation(
        mFrameMetrics.mScrollOffset,
        mFrameMetrics.mZoom,
        endZoomToMetrics.mScrollOffset,
        endZoomToMetrics.mZoom));

    // Schedule a repaint now, so the new displayport will be painted before the
    // animation finishes.
    RequestContentRepaint(endZoomToMetrics);
  }
}

void AsyncPanZoomController::ContentReceivedTouch(bool aPreventDefault) {
  mPreventDefaultSet = true;
  mPreventDefault = aPreventDefault;
  CheckContentResponse();
}

void AsyncPanZoomController::CheckContentResponse() {
  bool canProceedToTouchState = true;

  if (mFrameMetrics.mMayHaveTouchListeners) {
    canProceedToTouchState &= mPreventDefaultSet;
  }

  if (mTouchActionPropertyEnabled) {
    canProceedToTouchState &= mAllowedTouchBehaviorSet;
  }

  if (!canProceedToTouchState) {
    return;
  }

  if (mContentResponseTimeoutTask) {
    mContentResponseTimeoutTask->Cancel();
    mContentResponseTimeoutTask = nullptr;
  }

  if (mState == WAITING_CONTENT_RESPONSE) {
    if (!mPreventDefault) {
      SetState(NOTHING);
    }

    mHandlingTouchQueue = true;

    while (!mTouchQueue.IsEmpty()) {
      if (!mPreventDefault) {
        HandleInputEvent(mTouchQueue[0]);
      }

      if (mTouchQueue[0].mType == MultiTouchInput::MULTITOUCH_END ||
          mTouchQueue[0].mType == MultiTouchInput::MULTITOUCH_CANCEL) {
        mTouchQueue.RemoveElementAt(0);
        break;
      }

      mTouchQueue.RemoveElementAt(0);
    }

    mHandlingTouchQueue = false;
  }
}

bool AsyncPanZoomController::TouchActionAllowZoom() {
  if (!mTouchActionPropertyEnabled) {
    return true;
  }

  // Pointer events specification implies all touch points to allow zoom
  // to perform it.
  for (size_t i = 0; i < mAllowedTouchBehaviors.Length(); i++) {
    if (!(mAllowedTouchBehaviors[i] & AllowedTouchBehavior::ZOOM)) {
      return false;
    }
  }

  return true;
}

AsyncPanZoomController::TouchBehaviorFlags
AsyncPanZoomController::GetTouchBehavior(uint32_t touchIndex) {
  if (touchIndex < mAllowedTouchBehaviors.Length()) {
    return mAllowedTouchBehaviors[touchIndex];
  }
  return DefaultTouchBehavior;
}

AsyncPanZoomController::TouchBehaviorFlags
AsyncPanZoomController::GetAllowedTouchBehavior(ScreenIntPoint& aPoint) {
  // Here we need to perform a hit testing over the touch-action regions attached to the
  // layer associated with current apzc.
  // Currently they are in progress, for more info see bug 928833.
  return AllowedTouchBehavior::UNKNOWN;
}

void AsyncPanZoomController::SetAllowedTouchBehavior(const nsTArray<TouchBehaviorFlags>& aBehaviors) {
  mAllowedTouchBehaviors.Clear();
  mAllowedTouchBehaviors.AppendElements(aBehaviors);
  mAllowedTouchBehaviorSet = true;
  CheckContentResponse();
}

void AsyncPanZoomController::SetState(PanZoomState aNewState) {

  PanZoomState oldState;

  // Intentional scoping for mutex
  {
    ReentrantMonitorAutoEnter lock(mMonitor);
    oldState = mState;
    mState = aNewState;
  }

  if (mGeckoContentController) {
    if (!IsTransformingState(oldState) && IsTransformingState(aNewState)) {
      mGeckoContentController->NotifyTransformBegin(
        ScrollableLayerGuid(mLayersId, mFrameMetrics.mPresShellId, mFrameMetrics.mScrollId));
    } else if (IsTransformingState(oldState) && !IsTransformingState(aNewState)) {
      mGeckoContentController->NotifyTransformEnd(
        ScrollableLayerGuid(mLayersId, mFrameMetrics.mPresShellId, mFrameMetrics.mScrollId));
    }
  }
}

bool AsyncPanZoomController::IsTransformingState(PanZoomState aState) {
  return !(aState == NOTHING || aState == TOUCHING || aState == WAITING_CONTENT_RESPONSE);
}

bool AsyncPanZoomController::IsPanningState(PanZoomState aState) {
  return (aState == PANNING || aState == PANNING_LOCKED_X || aState == PANNING_LOCKED_Y);
}

bool AsyncPanZoomController::AllowZoom() {
  // In addition to looking at the zoom constraints, which comes from the meta
  // viewport tag, disallow zooming if we are overflow:hidden in either direction.
  ReentrantMonitorAutoEnter lock(mMonitor);
  return mZoomConstraints.mAllowZoom
      && !(mFrameMetrics.GetDisableScrollingX() || mFrameMetrics.GetDisableScrollingY());
}

void AsyncPanZoomController::TimeoutContentResponse() {
  mContentResponseTimeoutTask = nullptr;
  ContentReceivedTouch(false);
}

void AsyncPanZoomController::UpdateZoomConstraints(const ZoomConstraints& aConstraints) {
  APZC_LOG("%p updating zoom constraints to %d %f %f\n", this, aConstraints.mAllowZoom,
    aConstraints.mMinZoom.scale, aConstraints.mMaxZoom.scale);
  mZoomConstraints.mAllowZoom = aConstraints.mAllowZoom;
  mZoomConstraints.mMinZoom = (MIN_ZOOM > aConstraints.mMinZoom ? MIN_ZOOM : aConstraints.mMinZoom);
  mZoomConstraints.mMaxZoom = (MAX_ZOOM > aConstraints.mMaxZoom ? aConstraints.mMaxZoom : MAX_ZOOM);
}

ZoomConstraints
AsyncPanZoomController::GetZoomConstraints() const
{
  return mZoomConstraints;
}


void AsyncPanZoomController::PostDelayedTask(Task* aTask, int aDelayMs) {
  nsRefPtr<GeckoContentController> controller = GetGeckoContentController();
  if (controller) {
    controller->PostDelayedTask(aTask, aDelayMs);
  }
}

void AsyncPanZoomController::SendAsyncScrollEvent() {
  nsRefPtr<GeckoContentController> controller = GetGeckoContentController();
  if (!controller) {
    return;
  }

  bool isRoot;
  CSSRect contentRect;
  CSSSize scrollableSize;
  {
    ReentrantMonitorAutoEnter lock(mMonitor);

    isRoot = mFrameMetrics.mIsRoot;
    scrollableSize = mFrameMetrics.mScrollableRect.Size();
    contentRect = mFrameMetrics.CalculateCompositedRectInCssPixels();
    contentRect.MoveTo(mCurrentAsyncScrollOffset);
  }

  controller->SendAsyncScrollDOMEvent(isRoot, contentRect, scrollableSize);
}

bool AsyncPanZoomController::Matches(const ScrollableLayerGuid& aGuid)
{
  return aGuid == GetGuid();
}

void AsyncPanZoomController::GetGuid(ScrollableLayerGuid* aGuidOut)
{
  if (aGuidOut) {
    *aGuidOut = GetGuid();
  }
}

ScrollableLayerGuid AsyncPanZoomController::GetGuid()
{
  return ScrollableLayerGuid(mLayersId, mFrameMetrics);
}

void AsyncPanZoomController::UpdateSharedCompositorFrameMetrics()
{
  mMonitor.AssertCurrentThreadIn();

  FrameMetrics* frame = mSharedFrameMetricsBuffer ?
      static_cast<FrameMetrics*>(mSharedFrameMetricsBuffer->memory()) : nullptr;

  if (gUseProgressiveTilePainting && frame && mSharedLock) {
    mSharedLock->Lock();
    *frame = mFrameMetrics;
    mSharedLock->Unlock();
  }
}

void AsyncPanZoomController::ShareCompositorFrameMetrics() {

  PCompositorParent* compositor =
    (mCrossProcessCompositorParent ? mCrossProcessCompositorParent : mCompositorParent.get());

  // Only create the shared memory buffer if it hasn't already been created,
  // we are using progressive tile painting, and we have a
  // compositor to pass the shared memory back to the content process/thread.
  if (!mSharedFrameMetricsBuffer && gUseProgressiveTilePainting && compositor) {

    // Create shared memory and initialize it with the current FrameMetrics value
    mSharedFrameMetricsBuffer = new ipc::SharedMemoryBasic;
    FrameMetrics* frame = nullptr;
    mSharedFrameMetricsBuffer->Create(sizeof(FrameMetrics));
    mSharedFrameMetricsBuffer->Map(sizeof(FrameMetrics));
    frame = static_cast<FrameMetrics*>(mSharedFrameMetricsBuffer->memory());

    if (frame) {

      { // scope the monitor, only needed to copy the FrameMetrics.
        ReentrantMonitorAutoEnter lock(mMonitor);
        *frame = mFrameMetrics;
      }

      // Get the process id of the content process
      base::ProcessHandle processHandle = compositor->OtherProcess();
      ipc::SharedMemoryBasic::Handle mem = ipc::SharedMemoryBasic::NULLHandle();

      // Get the shared memory handle to share with the content process
      mSharedFrameMetricsBuffer->ShareToProcess(processHandle, &mem);

      // Get the cross process mutex handle to share with the content process
      mSharedLock = new CrossProcessMutex("AsyncPanZoomControlLock");
      CrossProcessMutexHandle handle = mSharedLock->ShareToProcess(processHandle);

      // Send the shared memory handle and cross process handle to the content
      // process by an asynchronous ipc call. Include the APZC unique ID
      // so the content process know which APZC sent this shared FrameMetrics.
      if (!compositor->SendSharedCompositorFrameMetrics(mem, handle, mAPZCId)) {
        APZC_LOG("%p failed to share FrameMetrics with content process.", this);
      }
    }
  }
}

}
}
