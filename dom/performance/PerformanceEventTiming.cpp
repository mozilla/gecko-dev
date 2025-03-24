/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PerformanceEventTiming.h"
#include "PerformanceMainThread.h"
#include "mozilla/EventForwards.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/dom/PerformanceEventTimingBinding.h"
#include "PerformanceInteractionMetrics.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/Performance.h"
#include "mozilla/dom/Event.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/TextEvents.h"
#include "nsContentUtils.h"
#include "nsIDocShell.h"
#include <algorithm>

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED(PerformanceEventTiming, PerformanceEntry,
                                   mPerformance, mTarget)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(PerformanceEventTiming)
NS_INTERFACE_MAP_END_INHERITING(PerformanceEntry)

NS_IMPL_ADDREF_INHERITED(PerformanceEventTiming, PerformanceEntry)
NS_IMPL_RELEASE_INHERITED(PerformanceEventTiming, PerformanceEntry)

PerformanceEventTiming::PerformanceEventTiming(Performance* aPerformance,
                                               const nsAString& aName,
                                               const TimeStamp& aStartTime,
                                               bool aIsCacelable,
                                               uint64_t aInteractionId,
                                               EventMessage aMessage)
    : PerformanceEntry(aPerformance->GetParentObject(), aName, u"event"_ns),
      mPerformance(aPerformance),
      mProcessingStart(aPerformance->NowUnclamped()),
      mProcessingEnd(0),
      mStartTime(
          aPerformance->GetDOMTiming()->TimeStampToDOMHighRes(aStartTime)),
      mDuration(0),
      mCancelable(aIsCacelable),
      mInteractionId(Some(aInteractionId)),
      mMessage(aMessage) {}

PerformanceEventTiming::PerformanceEventTiming(
    const PerformanceEventTiming& aEventTimingEntry)
    : PerformanceEntry(aEventTimingEntry.mPerformance->GetParentObject(),
                       nsDependentAtomString(aEventTimingEntry.GetName()),
                       nsDependentAtomString(aEventTimingEntry.GetEntryType())),
      mPerformance(aEventTimingEntry.mPerformance),
      mProcessingStart(aEventTimingEntry.mProcessingStart),
      mProcessingEnd(aEventTimingEntry.mProcessingEnd),
      mTarget(aEventTimingEntry.mTarget),
      mStartTime(aEventTimingEntry.mStartTime),
      mDuration(aEventTimingEntry.mDuration),
      mCancelable(aEventTimingEntry.mCancelable),
      mInteractionId(aEventTimingEntry.mInteractionId),
      mMessage(aEventTimingEntry.mMessage) {}

JSObject* PerformanceEventTiming::WrapObject(
    JSContext* cx, JS::Handle<JSObject*> aGivenProto) {
  return PerformanceEventTiming_Binding::Wrap(cx, this, aGivenProto);
}

already_AddRefed<PerformanceEventTiming>
PerformanceEventTiming::TryGenerateEventTiming(const EventTarget* aTarget,
                                               const WidgetEvent* aEvent) {
  MOZ_ASSERT(NS_IsMainThread());
  if (!StaticPrefs::dom_enable_event_timing() ||
      aEvent->mFlags.mOnlyChromeDispatch) {
    return nullptr;
  }

  if (!aEvent->IsTrusted()) {
    return nullptr;
  }

  switch (aEvent->mMessage) {
    case eContextMenu:
    case eMouseDoubleClick:
    case eMouseDown:
    case eMouseEnter:
    case eMouseLeave:
    case eMouseOut:
    case eMouseOver:
    case eMouseUp:
    case ePointerAuxClick:
    case ePointerClick:
    case ePointerOver:
    case ePointerEnter:
    case ePointerDown:
    case ePointerUp:
    case ePointerCancel:
    case ePointerOut:
    case ePointerLeave:
    case ePointerGotCapture:
    case ePointerLostCapture:
    case eTouchStart:
    case eTouchEnd:
    case eTouchCancel:
    case eKeyDown:
    case eKeyPress:
    case eKeyUp:
    case eEditorBeforeInput:
    case eEditorInput:
    case eCompositionStart:
    case eCompositionUpdate:
    case eCompositionEnd:
    case eDragStart:
    case eDragEnd:
    case eDragEnter:
    case eDragLeave:
    case eDragOver:
    case eDrop:
      break;
    default:
      return nullptr;
  }

  nsCOMPtr<nsPIDOMWindowInner> innerWindow =
      do_QueryInterface(aTarget->GetOwnerGlobal());
  if (!innerWindow) {
    return nullptr;
  }

  if (Performance* performance = innerWindow->GetPerformance()) {
    const char16_t* eventName = Event::GetEventName(aEvent->mMessage);
    MOZ_ASSERT(eventName,
               "User defined events shouldn't be considered as event timing");
    return RefPtr<PerformanceEventTiming>(
               new PerformanceEventTiming(
                   performance, nsDependentString(eventName),
                   aEvent->mTimeStamp, aEvent->mFlags.mCancelable,
                   performance->ComputeInteractionId(aEvent), aEvent->mMessage))
        .forget();
  }
  return nullptr;
}

bool PerformanceEventTiming::ShouldAddEntryToBuffer(double aDuration) const {
  if (GetEntryType() == nsGkAtoms::firstInput) {
    return true;
  }
  MOZ_ASSERT(GetEntryType() == nsGkAtoms::event);
  return RawDuration() >= aDuration;
}

bool PerformanceEventTiming::ShouldAddEntryToObserverBuffer(
    PerformanceObserverInit& aOption) const {
  if (!PerformanceEntry::ShouldAddEntryToObserverBuffer(aOption)) {
    return false;
  }

  const double minDuration =
      aOption.mDurationThreshold.WasPassed()
          ? std::max(aOption.mDurationThreshold.Value(),
                     PerformanceMainThread::kDefaultEventTimingMinDuration)
          : PerformanceMainThread::kDefaultEventTimingDurationThreshold;

  return ShouldAddEntryToBuffer(minDuration);
}

void PerformanceEventTiming::BufferEntryIfNeeded() {
  if (ShouldAddEntryToBuffer(
          PerformanceMainThread::kDefaultEventTimingDurationThreshold)) {
    if (GetEntryType() != nsGkAtoms::firstInput) {
      MOZ_ASSERT(GetEntryType() == nsGkAtoms::event);
      mPerformance->BufferEventTimingEntryIfNeeded(this);
    }
  }
}

nsINode* PerformanceEventTiming::GetTarget() const {
  nsCOMPtr<Element> element = do_QueryReferent(mTarget);
  if (!element) {
    return nullptr;
  }

  nsCOMPtr<nsPIDOMWindowInner> global =
      do_QueryInterface(element->GetOwnerGlobal());
  if (!global) {
    return nullptr;
  }
  return nsContentUtils::GetAnElementForTiming(element, global->GetExtantDoc(),
                                               mPerformance->GetParentObject());
}

void PerformanceEventTiming::FinalizeEventTiming(const WidgetEvent* aEvent) {
  MOZ_ASSERT(aEvent);
  EventTarget* target = aEvent->mTarget;
  if (!target) {
    return;
  }
  nsCOMPtr<nsPIDOMWindowInner> global =
      do_QueryInterface(target->GetOwnerGlobal());
  if (!global) {
    return;
  }

  mProcessingEnd = mPerformance->NowUnclamped();

  Element* element = Element::FromEventTarget(target);
  if (!element || element->ChromeOnlyAccess()) {
    return;
  }

  mTarget = do_GetWeakReference(element);

  if (!StaticPrefs::dom_performance_event_timing_enable_interactionid()) {
    mPerformance->InsertEventTimingEntry(this);
    return;
  }

  if (aEvent->mMessage == ePointerDown) {
    auto& interactionMetrics = mPerformance->GetPerformanceInteractionMetrics();
    // Step 8.1. Let pendingPointerDowns be relevantGlobal’s pending pointer
    // downs.
    auto& pendingPointerDowns = interactionMetrics.PendingPointerDowns();

    // Step 8.2. Let pointerId be event’s pointerId.
    uint32_t pointerId = aEvent->AsPointerEvent()->pointerId;

    // Step 8.3. If pendingPointerDowns[pointerId] exists, append
    // pendingPointerDowns[pointerId] to relevantGlobal’s entries to be queued.
    auto entry = pendingPointerDowns.MaybeGet(pointerId);
    if (entry.isSome()) {
      mPerformance->InsertEventTimingEntry(*entry);
    }

    // Step 8.4. Set pendingPointerDowns[pointerId] to timingEntry.
    pendingPointerDowns.InsertOrUpdate(pointerId, this);
  } else if (aEvent->mMessage == eKeyDown) {
    const WidgetKeyboardEvent* keyEvent = aEvent->AsKeyboardEvent();

    // Step 9.1. If event’s isComposing attribute value is true:
    if (keyEvent->mIsComposing) {
      // Step 9.1.1. Append timingEntry to relevantGlobal’s entries to be
      // queued.
      mPerformance->InsertEventTimingEntry(this);
      // Step 9.1.2. Return.
      return;
    }

    auto& interactionMetrics = mPerformance->GetPerformanceInteractionMetrics();

    // Step 9.2. Let pendingKeyDowns be relevantGlobal’s pending key downs.
    auto& pendingKeyDowns = interactionMetrics.PendingKeyDowns();
    // Step 9.3. Let code be event’s keyCode attribute value.
    auto code = keyEvent->mKeyCode;

    // Step 9.4.1 Let entry be pendingKeyDowns[code].
    auto entry = pendingKeyDowns.MaybeGet(code);
    // Step 9.4. If pendingKeyDowns[code] exists:
    if (entry) {
      // Step 9.4.2. If code is not 229:
      if (code != 229) {
        // Step 9.4.2.1. Increase window’s user interaction value value by a
        // small number chosen by the user agent.
        uint64_t interactionId =
            interactionMetrics.IncreaseInteractionValueAndCount();
        // Step 9.4.2.2. Set entry’s interactionId to window’s user interaction
        // value.
        SetInteractionId(interactionId);
      }

      // Step 9.4.3. Add entry to window’s entries to be queued.
      mPerformance->InsertEventTimingEntry(*entry);
    }

    // Step 9.5. Set pendingKeyDowns[code] to timingEntry.
    pendingKeyDowns.InsertOrUpdate(code, this);
  } else {
    // Insert the rest of the event timings to the entries to be queued.
    mPerformance->InsertEventTimingEntry(this);
  }
}
}  // namespace mozilla::dom
