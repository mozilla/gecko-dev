/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsChildView_h_
#define nsChildView_h_

// formal protocols
#include "mozView.h"
#ifdef ACCESSIBILITY
#  include "mozilla/a11y/LocalAccessible.h"
#  include "mozAccessibleProtocol.h"
#endif

#include "nsISupports.h"
#include "nsIWeakReferenceUtils.h"
#include "TextInputHandler.h"
#include "nsCocoaUtils.h"
#include "gfxQuartzSurface.h"
#include "GLContextTypes.h"
#include "mozilla/DataMutex.h"
#include "mozilla/Mutex.h"
#include "nsRegion.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/webrender/WebRenderTypes.h"

#include "nsString.h"
#include "nsIDragService.h"
#include "ViewRegion.h"
#include "CFTypeRefPtr.h"

#import <Carbon/Carbon.h>
#import <Cocoa/Cocoa.h>
#import <AppKit/NSOpenGL.h>

class nsChildView;
class nsCocoaWindow;

namespace {
class GLPresenter;
}  // namespace

namespace mozilla {
enum class NativeKeyBindingsType : uint8_t;

class InputData;
class PanGestureInput;
class VibrancyManager;
namespace layers {
class GLManager;
class NativeLayerRootCA;
class NativeLayerCA;
}  // namespace layers
namespace widget {
class WidgetRenderingContext;
}  // namespace widget
}  // namespace mozilla

@class PixelHostingView;

@interface NSEvent (Undocumented)

// Return Cocoa event's corresponding Carbon event.  Not initialized (on
// synthetic events) until the OS actually "sends" the event.  This method
// has been present in the same form since at least OS X 10.2.8.
- (EventRef)_eventRef;

// stage From 10.10.3 for force touch event
@property(readonly) NSInteger stage;

@end

@interface NSView (Undocumented)

// Undocumented method of one or more of NSFrameView's subclasses.  Called
// when one or more of the titlebar buttons needs to be repositioned, to
// disappear, or to reappear (say if the window's style changes).  If
// 'redisplay' is true, the entire titlebar (the window's top 22 pixels) is
// marked as needing redisplay.  This method has been present in the same
// format since at least OS X 10.5.
- (void)_tileTitlebarAndRedisplay:(BOOL)redisplay;

// The following undocumented methods are used to work around bug 1069658,
// which is an Apple bug or design flaw that effects Yosemite.  None of them
// were present prior to Yosemite (OS X 10.10).
- (NSView*)titlebarView;           // Method of NSThemeFrame
- (NSView*)titlebarContainerView;  // Method of NSThemeFrame
- (BOOL)transparent;  // Method of NSTitlebarView and NSTitlebarContainerView
- (void)setTransparent:(BOOL)transparent;  // Method of NSTitlebarView and
                                           // NSTitlebarContainerView

// Available since 10.7.4:
- (void)viewDidChangeBackingProperties;
@end

@interface ChildView : NSView <
#ifdef ACCESSIBILITY
                           mozAccessible,
#endif
                           mozView,
                           NSTextInputClient,
                           NSDraggingSource,
                           NSDraggingDestination,
                           NSPasteboardItemDataProvider,
                           NSStandardKeyBindingResponding> {
 @private
  // the nsCocoaWindow that created the view. It retains this NSView, so
  // the link back to it must be weak.
  nsCocoaWindow* mGeckoChild;

  // Text input handler for mGeckoChild and us.  Note that this is a weak
  // reference.  Ideally, this should be a strong reference but a ChildView
  // object can live longer than the mGeckoChild that owns it.  And if
  // mTextInputHandler were a strong reference, this would make it difficult
  // for Gecko's leak detector to detect leaked TextInputHandler objects.
  // This is initialized by [mozView installTextInputHandler:aHandler] and
  // cleared by [mozView uninstallTextInputHandler].
  mozilla::widget::TextInputHandler* mTextInputHandler;  // [WEAK]

  // when mouseDown: is called, we store its event here (strong)
  NSEvent* mLastMouseDownEvent;

  // Needed for IME support in e10s mode.  Strong.
  NSEvent* mLastKeyDownEvent;

  // Whether the last mouse down event was blocked from Gecko.
  BOOL mBlockedLastMouseDown;

  // when acceptsFirstMouse: is called, we store the event here (strong)
  NSEvent* mClickThroughMouseDownEvent;

  // WheelStart/Stop events should always come in pairs. This BOOL records the
  // last received event so that, when we receive one of the events, we make
  // sure to send its pair event first, in case we didn't yet for any reason.
  BOOL mExpectingWheelStop;

  // Whether we're inside updateRootCALayer at the moment.
  BOOL mIsUpdatingLayer;

  // Whether the drag and drop was performed.
  BOOL mPerformedDrag;

  // Holds our drag service across multiple drag calls. The reference to the
  // service is obtained when the mouse enters the view and is released when
  // the mouse exits or there is a drop. This prevents us from having to
  // re-establish the connection to the service manager many times per second
  // when handling |draggingUpdated:| messages.
  nsIDragService* mDragService;

  // Gestures support
  //
  // mGestureState is used to detect when Cocoa has called both
  // magnifyWithEvent and rotateWithEvent within the same
  // beginGestureWithEvent and endGestureWithEvent sequence. We
  // discard the spurious gesture event so as not to confuse Gecko.
  //
  // mCumulativeRotation keeps track of the total amount of rotation
  // performed during a rotate gesture so we can send that value with
  // the final MozRotateGesture event.
  enum {
    eGestureState_None,
    eGestureState_StartGesture,
    eGestureState_MagnifyGesture,
    eGestureState_RotateGesture
  } mGestureState;
  float mCumulativeRotation;

#ifdef __LP64__
  // Support for fluid swipe tracking.
  BOOL* mCancelSwipeAnimation;
#endif

  // Whether this uses off-main-thread compositing.
  BOOL mUsingOMTCompositor;

  // Subviews of self, which act as container views for vibrancy views and
  // non-draggable views.
  NSView* mVibrancyViewsContainer;      // [STRONG]
  NSView* mNonDraggableViewsContainer;  // [STRONG]

  // The layer-backed view that hosts our drawing. Always non-null.
  // This is a subview of self so that it can be ordered on top of
  // mVibrancyViewsContainer.
  PixelHostingView* mPixelHostingView;

  // The CALayer that wraps Gecko's rendered contents. It's a sublayer of
  // mPixelHostingView's backing layer. Always non-null.
  CALayer* mRootCALayer;  // [STRONG]

  // Last pressure stage by trackpad's force click
  NSInteger mLastPressureStage;
}

// class initialization
+ (void)initialize;

+ (void)registerViewForDraggedTypes:(NSView*)aView;

// these are sent to the first responder when the window key status changes
- (void)viewsWindowDidBecomeKey;
- (void)viewsWindowDidResignKey;

// Stop NSView hierarchy being changed during [ChildView drawRect:]
- (void)delayedTearDown;

- (void)handleMouseMoved:(NSEvent*)aEvent;

- (void)sendMouseEnterOrExitEvent:(NSEvent*)aEvent
                            enter:(BOOL)aEnter
                         exitFrom:
                             (mozilla::WidgetMouseEvent::ExitFrom)aExitFrom;

// Call this during operations that will likely trigger a main thread
// CoreAnimation paint of the window, during which Gecko should do its own
// painting and present the results atomically with that main thread
// transaction. This method will suspend off-thread window updates so that the
// upcoming paint can be atomic, and mark the layer as needing display so that
// HandleMainThreadCATransaction gets called and Gecko gets a chance to paint.
- (void)ensureNextCompositeIsAtomicWithMainThreadPaint;

- (NSView*)vibrancyViewsContainer;
- (NSView*)nonDraggableViewsContainer;
- (NSView*)pixelHostingView;

- (void)viewWillStartLiveResize;
- (void)viewDidEndLiveResize;

- (void)showContextMenuForSelection:(id)sender;

/*
 * Gestures support
 *
 * The prototypes swipeWithEvent, beginGestureWithEvent, smartMagnifyWithEvent,
 * rotateWithEvent and endGestureWithEvent were obtained from the following
 * links:
 * https://developer.apple.com/library/mac/#documentation/Cocoa/Reference/ApplicationKit/Classes/NSResponder_Class/Reference/Reference.html
 * https://developer.apple.com/library/mac/#releasenotes/Cocoa/AppKit.html
 */
- (void)swipeWithEvent:(NSEvent*)anEvent;
- (void)beginGestureWithEvent:(NSEvent*)anEvent;
- (void)magnifyWithEvent:(NSEvent*)anEvent;
- (void)smartMagnifyWithEvent:(NSEvent*)anEvent;
- (void)rotateWithEvent:(NSEvent*)anEvent;
- (void)endGestureWithEvent:(NSEvent*)anEvent;

- (void)scrollWheel:(NSEvent*)anEvent;

- (NSEvent*)lastKeyDownEvent;

+ (uint32_t)sUniqueKeyEventId;

+ (NSMutableDictionary*)sNativeKeyEventsMap;
@end

class ChildViewMouseTracker {
 public:
  static void MouseMoved(NSEvent* aEvent);
  static void MouseScrolled(NSEvent* aEvent);
  static void OnDestroyView(ChildView* aView);
  static void OnDestroyWindow(NSWindow* aWindow);
  static BOOL WindowAcceptsEvent(NSWindow* aWindow, NSEvent* aEvent,
                                 ChildView* aView, BOOL isClickThrough = NO);
  static void MouseExitedWindow(NSEvent* aEvent);
  static void MouseEnteredWindow(NSEvent* aEvent);
  static void NativeMenuOpened();
  static void NativeMenuClosed();
  static void ReEvaluateMouseEnterState(NSEvent* aEvent = nil,
                                        ChildView* aOldView = nil);
  static void ResendLastMouseMoveEvent();
  static ChildView* ViewForEvent(NSEvent* aEvent);

  static ChildView* sLastMouseEventView;
  static NSEvent* sLastMouseMoveEvent;
  static NSWindow* sWindowUnderMouse;
  static NSPoint sLastScrollEventScreenLocation;
};

#endif  // nsChildView_h_
