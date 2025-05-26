/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PerformanceInteractionMetrics.h"
#include "mozilla/EventForwards.h"
#include "mozilla/Maybe.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/RandomNum.h"
#include "mozilla/TextEvents.h"

// Interaction ID increment. We increase this value by an integer greater than 1
// to discourage developers from using the value to 'count' the number of user
// interactions. This is consistent with the spec, which allows the increasing
// the user interaction value by a small number chosen by the user agent.
constexpr uint32_t kInteractionIdIncrement = 7;
// Minimum potential value for the first Interaction ID.
constexpr uint32_t kMinFirstInteractionID = 100;
// Maximum potential value for the first Interaction ID.
constexpr uint32_t kMaxFirstInteractionID = 10000;

constexpr uint32_t kNonPointerId = -1;

namespace mozilla::dom {

PerformanceInteractionMetrics::PerformanceInteractionMetrics() {
  uint64_t randVal = RandomUint64().valueOr(kMinFirstInteractionID);
  // Choose a random integer as the initial value to discourage developers from
  // using interactionId to counter the number of interactions.
  // https://wicg.github.io/event-timing/#user-interaction-value
  mCurrentInteractionValue =
      kMinFirstInteractionID +
      (randVal % (kMaxFirstInteractionID - kMinFirstInteractionID + 1));
}

// https://w3c.github.io/event-timing/#sec-increasing-interaction-count
uint64_t PerformanceInteractionMetrics::IncreaseInteractionValueAndCount() {
  mCurrentInteractionValue += kInteractionIdIncrement;
  mInteractionCount++;
  return mCurrentInteractionValue;
}

// https://w3c.github.io/event-timing/#sec-computing-interactionid
Maybe<uint64_t> PerformanceInteractionMetrics::ComputeInteractionId(
    PerformanceEventTiming* aEventTiming, const WidgetEvent* aEvent) {
  // Step 1. If event’s isTrusted attribute value is false, return 0.
  if (!aEvent->IsTrusted()) {
    return Some(0);
  }

  // Step 2. Let type be event’s type attribute value.
  const EventMessage eventType = aEvent->mMessage;

  // Step 3. If type is not one among keyup, compositionstart, input,
  // pointercancel, pointerup, or click, return 0.
  // Note: keydown and pointerdown are handled in finalize event timing.
  switch (eventType) {
    case eKeyDown:
    case eKeyUp:
    case eCompositionStart:
    case eEditorInput:
    case ePointerCancel:
    case ePointerDown:
    case ePointerUp:
    case eContextMenu:
    case ePointerClick:
      break;
    default:
      return Some(0);
  }

  // Step 4-8. Happens in the class constructor.

  if (eventType == ePointerDown) {
    uint32_t pointerId = aEvent->AsPointerEvent()->pointerId;

    mPendingPointerDowns.InsertOrUpdate(pointerId, aEventTiming);
    mContextMenuTriggered = false;
    // InteractionId for this will be assigned by pointerup or pointercancel
    // later.
    return Nothing();
  }

  if (eventType == eKeyDown) {
    const WidgetKeyboardEvent* keyEvent = aEvent->AsKeyboardEvent();

    if (keyEvent->mIsComposing) {
      return Some(0);
    }

    auto code = keyEvent->mKeyCode;

    // This is not part of the spec yet, but it's being discussed and will be
    // added to the spec soon.
    // See: https://github.com/w3c/event-timing/issues/153
    mPendingKeyDowns.InsertOrUpdate(code, aEventTiming);
    uint64_t interactionId = IncreaseInteractionValueAndCount();
    return Some(interactionId);
  }

  // Step 8. If type is keyup:
  if (eventType == eKeyUp) {
    // Step 8.1. If event’s isComposing attribute value is true, return 0.
    const WidgetKeyboardEvent* keyEvent = aEvent->AsKeyboardEvent();
    if (keyEvent->mIsComposing) {
      return Some(0);
    }

    // Step 8.2. Let code be event’s keyCode attribute value.
    const uint32_t code = keyEvent->mKeyCode;

    // Step 8.4. Let entry be pendingKeyDowns[code].
    auto entry = mPendingKeyDowns.MaybeGet(code);
    // Step 8.3. If pendingKeyDowns[code] does not exist, return 0.
    if (!entry) {
      return Some(0);
    }

    uint64_t interactionId = (*entry)->InteractionId();

    // Step 8.9. Remove pendingKeyDowns[code].
    mPendingKeyDowns.Remove(code);
    mLastKeydownInteractionValue = Some(interactionId);

    // Step 8.10. Return interactionId.
    return Some(interactionId);
  }

  // Step 9. If type is compositionstart:
  if (eventType == eCompositionStart) {
    // Step 9.1 For each entry in the values of pendingKeyDowns:
    for (auto iter = mPendingKeyDowns.Iter(); !iter.Done(); iter.Next()) {
      PerformanceEventTiming* entry = iter.Data();
      // Step 9.1.1. Append entry to window’s entries to be queued.
      entry->SetInteractionId(0);
    }

    // Step 9.2. Clear pendingKeyDowns.
    mPendingKeyDowns.Clear();
    // Step 9.3. Return 0
    return Some(0);
  }

  // Step 10. If type is input:
  if (eventType == eEditorInput) {
    // Step 10.1. If event is not an instance of InputEvent, return 0.
    const auto* inputEvent = aEvent->AsEditorInputEvent();
    if (!inputEvent) {
      return Some(0);
    }

    // Step 10.2. If event’s isComposing attribute value is false, return 0.
    if (!inputEvent->mIsComposing) {
      return Some(0);
    }

    mLastKeydownInteractionValue = Nothing();
    return Some(IncreaseInteractionValueAndCount());
  }

  // Step 11. Otherwise (type is pointercancel, pointerup, or click):

  MOZ_ASSERT(eventType == ePointerCancel || eventType == ePointerUp ||
                 eventType == ePointerClick || eventType == eContextMenu,
             "Unexpected event type");
  const auto* mouseEvent = aEvent->AsMouseEvent();
  // Step 11.1. Let pointerId be event’s pointerId attribute value.
  auto pointerId = mouseEvent->pointerId;

  // Step 11.2. If type is click:
  if (eventType == ePointerClick) {
    if (pointerId == kNonPointerId) {
      // -1 pointerId is a reserved value to indicate events that were generated
      // by something other than a pointer device, like keydown.
      // Return the interaction value of the keydown event instead.
      return Some(mLastKeydownInteractionValue.valueOr(0));
    }

    // Step 11.2.2. Let value be pointerMap[pointerId].
    auto value = mPointerInteractionValueMap.MaybeGet(pointerId);
    // Step 11.2.1. If pointerMap[pointerId] does not exist, return 0.
    if (!value) {
      return Some(0);
    }

    // Step 11.2.3. Remove pointerMap[pointerId].
    mPointerInteractionValueMap.Remove(pointerId);
    // Step 11.2.4. Return value.
    return Some(*value);
  }

  // Step 11.3. Assert that type is pointerup or pointercancel.
  MOZ_RELEASE_ASSERT(eventType == ePointerUp || eventType == ePointerCancel ||
                     eventType == eContextMenu);

  // Step 11.5. Let pointerDownEntry be pendingPointerDowns[pointerId].
  auto entry = mPendingPointerDowns.MaybeGet(pointerId);
  // Step 11.4. If pendingPointerDowns[pointerId] does not exist, return 0.
  if (!entry) {
    // This is the case where we have seen a pointerup before a contextmenu
    // event. We return the same interactionId for the contextmenu.
    // See https://github.com/w3c/event-timing/issues/155.
    if (eventType == eContextMenu) {
      return Some(mCurrentInteractionValue);
    }

    // This is the case where we have seen a contextmenu before a pointerup
    // event. Similarly, we return the same interactionId, but also we reset the
    // "is contextmenu triggered" flag to make sure that the next events are
    // handled correctly. See https://github.com/w3c/event-timing/issues/155.
    if (eventType == ePointerUp && mContextMenuTriggered) {
      mContextMenuTriggered = false;
      return Some(mCurrentInteractionValue);
    }
    return Some(0);
  }

  // Step 11.7. If type is pointerup:
  if (eventType == ePointerUp || eventType == eContextMenu) {
    // Step 11.7.1. Increase interaction count on window.
    uint64_t interactionId = IncreaseInteractionValueAndCount();

    // Step 11.7.2. Set pointerMap[pointerId] to window’s user interaction
    // value.
    mPointerInteractionValueMap.InsertOrUpdate(pointerId, interactionId);

    // Step 11.7.3. Set pointerDownEntry’s interactionId to
    // pointerMap[pointerId].
    (*entry)->SetInteractionId(interactionId);
  } else {
    (*entry)->SetInteractionId(0);
  }

  // Step 11.9. Remove pendingPointerDowns[pointerId].
  mPendingPointerDowns.Remove(pointerId);

  if (eventType == eContextMenu) {
    mContextMenuTriggered = true;
  }

  // Step 11.10. If type is pointercancel, return 0.
  if (eventType == ePointerCancel) {
    return Some(0);
  }

  return Some(mPointerInteractionValueMap.Get(pointerId));
}

}  // namespace mozilla::dom
