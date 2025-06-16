/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_PointerEventHandler_h
#define mozilla_PointerEventHandler_h

#include "mozilla/EventForwards.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/TouchEvents.h"
#include "mozilla/WeakPtr.h"

// XXX Avoid including this here by moving function bodies to the cpp file
#include "mozilla/dom/Document.h"
#include "mozilla/dom/Element.h"

class nsIFrame;
class nsIContent;
class nsPresContext;

namespace mozilla {

class PresShell;

namespace dom {
class BrowserParent;
class Document;
class Element;
};  // namespace dom

class PointerCaptureInfo final {
 public:
  RefPtr<dom::Element> mPendingElement;
  RefPtr<dom::Element> mOverrideElement;

  explicit PointerCaptureInfo(dom::Element* aPendingElement)
      : mPendingElement(aPendingElement) {
    MOZ_COUNT_CTOR(PointerCaptureInfo);
  }

  MOZ_COUNTED_DTOR(PointerCaptureInfo)

  bool Empty() { return !(mPendingElement || mOverrideElement); }
};

struct PointerInfo final {
  enum class Active : bool { No, Yes };
  enum class Primary : bool { No, Yes };
  enum class FromTouchEvent : bool { No, Yes };
  enum class SynthesizeForTests : bool { No, Yes };
  PointerInfo() = delete;
  PointerInfo(const PointerInfo&) = default;
  PointerInfo(PointerInfo&&) = default;
  explicit PointerInfo(
      Active aActiveState, uint16_t aInputSource, Primary aPrimaryState,
      FromTouchEvent aFromTouchEvent, dom::Document* aActiveDocument,
      SynthesizeForTests aIsSynthesizedForTests = SynthesizeForTests::No)
      : mActiveDocument(aActiveDocument),
        mInputSource(aInputSource),
        mIsActive(static_cast<bool>(aActiveState)),
        mIsPrimary(static_cast<bool>(aPrimaryState)),
        mFromTouchEvent(static_cast<bool>(aFromTouchEvent)),
        mPreventMouseEventByContent(false),
        mIsSynthesizedForTests(static_cast<bool>(aIsSynthesizedForTests)) {}
  explicit PointerInfo(Active aActiveState,
                       const WidgetPointerEvent& aPointerEvent,
                       dom::Document* aActiveDocument)
      : mActiveDocument(aActiveDocument),
        mInputSource(aPointerEvent.mInputSource),
        mIsActive(static_cast<bool>(aActiveState)),
        mIsPrimary(aPointerEvent.mIsPrimary),
        mFromTouchEvent(aPointerEvent.mFromTouchEvent),
        mPreventMouseEventByContent(false),
        mIsSynthesizedForTests(aPointerEvent.mFlags.mIsSynthesizedForTests) {}

  WeakPtr<dom::Document> mActiveDocument;
  // mInputSource indicates which input source caused the last event.  E.g.,
  // if the last event is a compatibility mouse event, the input source is
  // "touch".
  uint16_t mInputSource = 0;
  bool mIsActive : 1;
  bool mIsPrimary : 1;
  // mFromTouchEvent is set to true if the last event is a touch event or a
  // pointer event caused by a touch event.  If the last event is a
  // compatibility mouse event, this is set to false even though the input
  // source is "touch".
  bool mFromTouchEvent : 1;
  bool mPreventMouseEventByContent : 1;
  // Set to true if the pointer is activated only by synthesized mouse events.
  bool mIsSynthesizedForTests : 1;
};

class PointerEventHandler final {
 public:
  // Called in nsLayoutStatics::Initialize/Shutdown to initialize pointer event
  // related static variables.
  static void InitializeStatics();
  static void ReleaseStatics();

  // Return the preference value of implicit capture.
  static bool IsPointerEventImplicitCaptureForTouchEnabled();

  /**
   * Return true if click/auxclick/contextmenu event should be fired on
   * an element which was capturing the pointer at dispatching ePointerUp.
   *
   * @param aSourceEvent    [Optional] The source event which causes the
   *                        `click`, `auxclick` or `contextmenu` event.  I.e.,
   *                        must be one of `mouseup`, `pointerup` or `touchend`.
   *                        If specifying nullptr, this method checks only
   *                        whether the behavior is enabled.
   */
  [[nodiscard]] static bool ShouldDispatchClickEventOnCapturingElement(
      const WidgetGUIEvent* aSourceEvent = nullptr);

  // Called in ESM::PreHandleEvent to update current active pointers in a hash
  // table.
  static void UpdateActivePointerState(WidgetMouseEvent* aEvent,
                                       nsIContent* aTargetContent = nullptr);

  // Request/release pointer capture of the specified pointer by the element.
  static void RequestPointerCaptureById(uint32_t aPointerId,
                                        dom::Element* aElement);
  static void ReleasePointerCaptureById(uint32_t aPointerId);
  static void ReleaseAllPointerCapture();

  // Set/release pointer capture of the specified pointer by the remote target.
  // Should only be called in parent process.
  static bool SetPointerCaptureRemoteTarget(uint32_t aPointerId,
                                            dom::BrowserParent* aBrowserParent);
  static void ReleasePointerCaptureRemoteTarget(
      dom::BrowserParent* aBrowserParent);
  static void ReleasePointerCaptureRemoteTarget(uint32_t aPointerId);
  static void ReleaseAllPointerCaptureRemoteTarget();

  // Get the pointer capturing remote target of the specified pointer.
  static dom::BrowserParent* GetPointerCapturingRemoteTarget(
      uint32_t aPointerId);

  // Get the pointer captured info of the specified pointer.
  static PointerCaptureInfo* GetPointerCaptureInfo(uint32_t aPointerId);

  // Return the PointerInfo if the pointer with aPointerId is situated in device
  // , nullptr otherwise.
  // Note that the result may be activated only by synthesized events for test.
  // If you don't want it, check PointerInfo::mIsSynthesizedForTests.
  static const PointerInfo* GetPointerInfo(uint32_t aPointerId);

  // CheckPointerCaptureState checks cases, when got/lostpointercapture events
  // should be fired.
  MOZ_CAN_RUN_SCRIPT
  static void MaybeProcessPointerCapture(WidgetGUIEvent* aEvent);
  MOZ_CAN_RUN_SCRIPT
  static void ProcessPointerCaptureForMouse(WidgetMouseEvent* aEvent);
  MOZ_CAN_RUN_SCRIPT
  static void ProcessPointerCaptureForTouch(WidgetTouchEvent* aEvent);
  MOZ_CAN_RUN_SCRIPT
  static void CheckPointerCaptureState(WidgetPointerEvent* aEvent);

  // Implicitly get and release capture of current pointer for touch.
  static void ImplicitlyCapturePointer(nsIFrame* aFrame, WidgetEvent* aEvent);
  MOZ_CAN_RUN_SCRIPT
  static void ImplicitlyReleasePointerCapture(WidgetEvent* aEvent);
  MOZ_CAN_RUN_SCRIPT static void MaybeImplicitlyReleasePointerCapture(
      WidgetGUIEvent* aEvent);

  /**
   * GetPointerCapturingContent returns a target element which captures the
   * pointer. It's applied to mouse or pointer event (except mousedown and
   * pointerdown). When capturing, return the element. Otherwise, nullptr.
   *
   * @param aEvent               A mouse event or pointer event which may be
   *                             captured.
   *
   * @return                     Target element for aEvent.
   */
  static dom::Element* GetPointerCapturingElement(const WidgetGUIEvent* aEvent);

  static dom::Element* GetPointerCapturingElement(uint32_t aPointerId);

  /**
   * Return pending capture element of for the pointerId (of the event).
   * - If the element has already overriden the pointer capture and there is no
   * new pending capture element, the result is what captures the pointer right
   * now.
   * - If the element has not overriden the pointer capture, the result will
   * start capturing the pointer once the pending pointer capture is processed
   * at dispatching a pointer event later.
   *
   * So, in other words, the result is the element which will capture the next
   * pointer event for the pointerId.
   */
  static dom::Element* GetPendingPointerCapturingElement(
      const WidgetGUIEvent* aEvent);
  static dom::Element* GetPendingPointerCapturingElement(uint32_t aPointerId);

  /**
   * Return an element which captured the pointer at dispatching the last
   * ePointerUp event caused by eMouseUp except the compatibility mouse events
   * of Touch Events or caused by eTouchEnd whose number of touches is one,
   * i.e., the last touch release.
   */
  [[nodiscard]] static RefPtr<dom::Element>
  GetPointerCapturingElementAtLastPointerUp();

  /**
   * Forget the pointer capturing element at dispatching the last ePointerUp.
   */
  static void ReleasePointerCapturingElementAtLastPointerUp();

  // Release pointer capture if captured by the specified content or it's
  // descendant. This is called to handle the case that the pointer capturing
  // content or it's parent is removed from the document.
  static void ReleaseIfCaptureByDescendant(nsIContent* aContent);

  /*
   * This function handles the case when content had called preventDefault on
   * the active pointer. In that case we have to prevent firing subsequent mouse
   * to content. We check the flag PointerInfo::mPreventMouseEventByContent and
   * call PreventDefault(false) to stop default behaviors and stop firing mouse
   * events to content and chrome.
   *
   * note: mouse transition events are excluded
   * note: we have to clean mPreventMouseEventByContent on pointerup for those
   *       devices support hover
   * note: we don't suppress firing mouse events to chrome and system group
   *       handlers because they may implement default behaviors
   */
  static void PreHandlePointerEventsPreventDefault(
      WidgetPointerEvent* aPointerEvent, WidgetGUIEvent* aMouseOrTouchEvent);

  /*
   * This function handles the preventDefault behavior of pointerdown. When user
   * preventDefault on pointerdown, We have to mark the active pointer to
   * prevent sebsequent mouse events (except mouse transition events) and
   * default behaviors.
   *
   * We add mPreventMouseEventByContent flag in PointerInfo to represent the
   * active pointer won't firing compatible mouse events. It's set to true when
   * content preventDefault on pointerdown
   */
  static void PostHandlePointerEventsPreventDefault(
      WidgetPointerEvent* aPointerEvent, WidgetGUIEvent* aMouseOrTouchEvent);

  /**
   * Dispatch a pointer event for aMouseOrTouchEvent to aEventTargetContent.
   *
   * @param aShell              The PresShell which is handling the event.
   * @param aEventTargetFrame   The frame for aEventTargetContent.
   * @param aEventTargetContent The event target node.
   * @param aPointerCapturingElement
   *                            The pointer capturing element.
   * @param aMouseOrTouchEvent  A mouse or touch event.
   * @param aDontRetargetEvents If true, this won't dispatch event with
   *                            different PresShell from aShell.  Otherwise,
   *                            pointer events may be fired on different
   *                            document if and only if aMouseOrTOuchEvent is a
   *                            touch event except eTouchStart.
   * @param aState              [out] The result of the pointer event.
   * @param aMouseOrTouchEventTarget
   *                            [out] The event target for the following mouse
   *                            or touch event. If aEventTargetContent has not
   *                            been removed from the tree, this is always set
   *                            to it. If aEventTargetContent is removed from
   *                            the tree and aMouseOrTouchEvent is a mouse
   *                            event, this is set to inclusive ancestor of
   *                            aEventTargetContent which is still connected.
   *                            If aEventTargetContent is removed from the tree
   *                            and aMouseOrTouchEvent is a touch event, this is
   *                            set to aEventTargetContent because touch event
   *                            should be dispatched even on disconnected node.
   *                            FIXME: If the event is a touch event but the
   *                            message is not eTouchStart, this won't be set.
   */
  MOZ_CAN_RUN_SCRIPT static void DispatchPointerFromMouseOrTouch(
      PresShell* aShell, nsIFrame* aEventTargetFrame,
      nsIContent* aEventTargetContent, dom::Element* aPointerCapturingElement,
      WidgetGUIEvent* aMouseOrTouchEvent, bool aDontRetargetEvents,
      nsEventStatus* aStatus, nsIContent** aMouseOrTouchEventTarget = nullptr);

  /**
   * Synthesize eMouseMove or ePointerMove to dispatch mouse/pointer boundary
   * events if they are required.  This dispatches the event on the widget.
   * Therefore, this dispatches the event on correct document in the same
   * process.  However, if there is a popup under the pointer or a document in a
   * different process, this does not work as you expected.
   */
  MOZ_CAN_RUN_SCRIPT static void SynthesizeMoveToDispatchBoundaryEvents(
      const WidgetMouseEvent* aEvent);

  static void InitPointerEventFromMouse(WidgetPointerEvent* aPointerEvent,
                                        WidgetMouseEvent* aMouseEvent,
                                        EventMessage aMessage);

  static void InitPointerEventFromTouch(WidgetPointerEvent& aPointerEvent,
                                        const WidgetTouchEvent& aTouchEvent,
                                        const mozilla::dom::Touch& aTouch);

  static void InitCoalescedEventFromPointerEvent(
      WidgetPointerEvent& aCoalescedEvent,
      const WidgetPointerEvent& aSourceEvent);

  static bool ShouldGeneratePointerEventFromMouse(WidgetGUIEvent* aEvent) {
    return aEvent->mMessage == eMouseRawUpdate ||
           aEvent->mMessage == eMouseDown || aEvent->mMessage == eMouseUp ||
           (aEvent->mMessage == eMouseMove &&
            aEvent->AsMouseEvent()->IsReal()) ||
           aEvent->mMessage == eMouseExitFromWidget;
  }

  static bool ShouldGeneratePointerEventFromTouch(WidgetGUIEvent* aEvent) {
    return aEvent->mMessage == eTouchRawUpdate ||
           aEvent->mMessage == eTouchStart || aEvent->mMessage == eTouchMove ||
           aEvent->mMessage == eTouchEnd || aEvent->mMessage == eTouchCancel ||
           aEvent->mMessage == eTouchPointerCancel;
  }

  static MOZ_ALWAYS_INLINE int32_t GetSpoofedPointerIdForRFP() {
    return sSpoofedPointerId.valueOr(0);
  }

  static void NotifyDestroyPresContext(nsPresContext* aPresContext);

  static bool IsDragAndDropEnabled(WidgetMouseEvent& aEvent);

  // Get proper pointer event message for a mouse or touch event.
  [[nodiscard]] static EventMessage ToPointerEventMessage(
      const WidgetGUIEvent* aMouseOrTouchEvent);

  /**
   * Return true if the window containing aDocument has had a
   * `pointerrawupdate` event listener.
   */
  [[nodiscard]] static bool NeedToDispatchPointerRawUpdate(
      const dom::Document* aDocument);

 private:
  // Set pointer capture of the specified pointer by the element.
  static void SetPointerCaptureById(uint32_t aPointerId,
                                    dom::Element* aElement);

  // GetPointerType returns pointer type like mouse, pen or touch for pointer
  // event with pointerId. The return value must be one of
  // MouseEvent_Binding::MOZ_SOURCE_*
  static uint16_t GetPointerType(uint32_t aPointerId);

  // GetPointerPrimaryState returns state of attribute isPrimary for pointer
  // event with pointerId
  static bool GetPointerPrimaryState(uint32_t aPointerId);

  // HasActiveTouchPointer returns true if there is active pointer event that is
  // generated from touch event.
  static bool HasActiveTouchPointer();

  MOZ_CAN_RUN_SCRIPT
  static void DispatchGotOrLostPointerCaptureEvent(
      bool aIsGotCapture, const WidgetPointerEvent* aPointerEvent,
      dom::Element* aCaptureTarget);

  enum class CapturingState { Pending, Override };
  static dom::Element* GetPointerCapturingElementInternal(
      CapturingState aCapturingState, const WidgetGUIEvent* aEvent);

  // The cached spoofed pointer ID for fingerprinting resistance. We will use a
  // mouse pointer id for desktop. For mobile, we should use the touch pointer
  // id as the spoofed one, and this work will be addressed in Bug 1492775.
  static Maybe<int32_t> sSpoofedPointerId;

  // A helper function to cache the pointer id of the spoofed interface, we
  // would only cache the pointer id once. After that, we would always stick to
  // that pointer id for fingerprinting resistance.
  static void MaybeCacheSpoofedPointerID(uint16_t aInputSource,
                                         uint32_t aPointerId);

  /**
   * Store the pointer capturing element.
   */
  static void SetPointerCapturingElementAtLastPointerUp(
      nsWeakPtr&& aPointerCapturingElement);
};

}  // namespace mozilla

#endif  // mozilla_PointerEventHandler_h
