/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScrollbarActivity.h"
#include "nsIScrollbarMediator.h"
#include "nsIContent.h"
#include "nsIFrame.h"
#include "nsContentUtils.h"
#include "nsITimer.h"
#include "nsQueryFrame.h"
#include "PresShell.h"
#include "nsLayoutUtils.h"
#include "nsScrollbarFrame.h"
#include "nsRefreshDriver.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/Document.h"
#include "mozilla/LookAndFeel.h"
#include "mozilla/ScrollContainerFrame.h"

namespace mozilla::layout {

using mozilla::dom::Element;

NS_IMPL_ISUPPORTS(ScrollbarActivity, nsIDOMEventListener)

static bool DisplayOnMouseMove() {
  return LookAndFeel::GetInt(LookAndFeel::IntID::ScrollbarDisplayOnMouseMove);
}

void ScrollbarActivity::Destroy() {
  StopListeningForScrollbarEvents();
  StopListeningForScrollAreaEvents();
  CancelFadeTimer();
}

void ScrollbarActivity::ActivityOccurred() {
  ActivityStarted();
  ActivityStopped();
}

static void SetScrollbarActive(Element* aScrollbar, bool aIsActive) {
  if (!aScrollbar) {
    return;
  }
  if (aIsActive) {
    if (nsScrollbarFrame* sf = do_QueryFrame(aScrollbar->GetPrimaryFrame())) {
      sf->WillBecomeActive();
    }
    aScrollbar->SetAttr(kNameSpaceID_None, nsGkAtoms::active, u""_ns, true);
  } else {
    aScrollbar->UnsetAttr(kNameSpaceID_None, nsGkAtoms::active, true);
  }
}

void ScrollbarActivity::ActivityStarted() {
  const bool wasActive = IsActive();
  mNestedActivityCounter++;
  if (wasActive) {
    return;
  }
  CancelFadeTimer();
  if (mScrollbarEffectivelyVisible) {
    return;
  }
  StartListeningForScrollbarEvents();
  StartListeningForScrollAreaEvents();
  SetScrollbarActive(GetHorizontalScrollbar(), true);
  SetScrollbarActive(GetVerticalScrollbar(), true);
  mScrollbarEffectivelyVisible = true;
}

void ScrollbarActivity::ActivityStopped() {
  if (!IsActive()) {
    // This can happen if there was a frame reconstruction while the activity
    // was ongoing. In this case we just do nothing. We should probably handle
    // this case better.
    return;
  }
  mNestedActivityCounter--;
  if (IsActive()) {
    return;
  }
  StartFadeTimer();
}

NS_IMETHODIMP
ScrollbarActivity::HandleEvent(dom::Event* aEvent) {
  if (!mScrollbarEffectivelyVisible && !DisplayOnMouseMove()) {
    return NS_OK;
  }

  nsAutoString type;
  aEvent->GetType(type);

  if (type.EqualsLiteral("mousemove")) {
    // Mouse motions anywhere in the scrollable frame should keep the
    // scrollbars visible, but we have to be careful as content descendants of
    // our scrollable content aren't necessarily scrolled by our scroll frame
    // (if they are out of flow and their containing block is not a descendant
    // of our scroll frame) and we don't want those to activate us.
    nsIFrame* scrollFrame = do_QueryFrame(mScrollableFrame);
    MOZ_ASSERT(scrollFrame);
    ScrollContainerFrame* scrollContainerFrame = do_QueryFrame(scrollFrame);
    nsCOMPtr<nsIContent> targetContent =
        do_QueryInterface(aEvent->GetOriginalTarget());
    nsIFrame* targetFrame =
        targetContent ? targetContent->GetPrimaryFrame() : nullptr;
    if ((scrollContainerFrame &&
         scrollContainerFrame->IsRootScrollFrameOfDocument()) ||
        !targetFrame ||
        nsLayoutUtils::IsAncestorFrameCrossDocInProcess(
            scrollFrame, targetFrame,
            scrollFrame->PresShell()->GetRootFrame())) {
      ActivityOccurred();
    }
    return NS_OK;
  }

  nsCOMPtr<nsIContent> targetContent =
      do_QueryInterface(aEvent->GetOriginalTarget());

  HandleEventForScrollbar(type, targetContent, GetHorizontalScrollbar(),
                          &mHScrollbarHovered);
  HandleEventForScrollbar(type, targetContent, GetVerticalScrollbar(),
                          &mVScrollbarHovered);

  return NS_OK;
}

void ScrollbarActivity::HandleEventForScrollbar(const nsAString& aType,
                                                nsIContent* aTarget,
                                                Element* aScrollbar,
                                                bool* aStoredHoverState) {
  if (!aTarget || !aScrollbar ||
      !aTarget->IsInclusiveDescendantOf(aScrollbar)) {
    return;
  }

  if (aType.EqualsLiteral("mousedown")) {
    ActivityStarted();
  } else if (aType.EqualsLiteral("mouseup")) {
    ActivityStopped();
  } else if (aType.EqualsLiteral("mouseover") ||
             aType.EqualsLiteral("mouseout")) {
    bool newHoveredState = aType.EqualsLiteral("mouseover");
    if (newHoveredState && !*aStoredHoverState) {
      ActivityStarted();
    } else if (*aStoredHoverState && !newHoveredState) {
      ActivityStopped();
    }
    *aStoredHoverState = newHoveredState;
  }
}

void ScrollbarActivity::StartListeningForScrollbarEvents() {
  if (mListeningForScrollbarEvents) {
    return;
  }

  mHorizontalScrollbar = GetHorizontalScrollbar();
  mVerticalScrollbar = GetVerticalScrollbar();

  AddScrollbarEventListeners(mHorizontalScrollbar);
  AddScrollbarEventListeners(mVerticalScrollbar);

  mListeningForScrollbarEvents = true;
}

void ScrollbarActivity::StopListeningForScrollbarEvents() {
  if (!mListeningForScrollbarEvents) return;

  RemoveScrollbarEventListeners(mHorizontalScrollbar);
  RemoveScrollbarEventListeners(mVerticalScrollbar);

  mHorizontalScrollbar = nullptr;
  mVerticalScrollbar = nullptr;
  mListeningForScrollbarEvents = false;
}

void ScrollbarActivity::StartListeningForScrollAreaEvents() {
  if (mListeningForScrollAreaEvents) {
    return;
  }
  nsIFrame* scrollArea = do_QueryFrame(mScrollableFrame);
  scrollArea->GetContent()->AddEventListener(u"mousemove"_ns, this, true);
  mListeningForScrollAreaEvents = true;
}

void ScrollbarActivity::StopListeningForScrollAreaEvents() {
  if (!mListeningForScrollAreaEvents) {
    return;
  }
  nsIFrame* scrollArea = do_QueryFrame(mScrollableFrame);
  scrollArea->GetContent()->RemoveEventListener(u"mousemove"_ns, this, true);
  mListeningForScrollAreaEvents = false;
}

void ScrollbarActivity::AddScrollbarEventListeners(
    dom::EventTarget* aScrollbar) {
  if (aScrollbar) {
    aScrollbar->AddEventListener(u"mousedown"_ns, this, true);
    aScrollbar->AddEventListener(u"mouseup"_ns, this, true);
    aScrollbar->AddEventListener(u"mouseover"_ns, this, true);
    aScrollbar->AddEventListener(u"mouseout"_ns, this, true);
  }
}

void ScrollbarActivity::RemoveScrollbarEventListeners(
    dom::EventTarget* aScrollbar) {
  if (aScrollbar) {
    aScrollbar->RemoveEventListener(u"mousedown"_ns, this, true);
    aScrollbar->RemoveEventListener(u"mouseup"_ns, this, true);
    aScrollbar->RemoveEventListener(u"mouseover"_ns, this, true);
    aScrollbar->RemoveEventListener(u"mouseout"_ns, this, true);
  }
}

void ScrollbarActivity::CancelFadeTimer() {
  if (mFadeTimer) {
    mFadeTimer->Cancel();
  }
}

void ScrollbarActivity::StartFadeTimer() {
  CancelFadeTimer();
  if (StaticPrefs::layout_testing_overlay_scrollbars_always_visible()) {
    return;
  }
  if (!mFadeTimer) {
    mFadeTimer = NS_NewTimer();
  }
  mFadeTimer->InitWithNamedFuncCallback(
      [](nsITimer*, void* aClosure) {
        RefPtr<ScrollbarActivity> activity =
            static_cast<ScrollbarActivity*>(aClosure);
        activity->BeginFade();
      },
      this, LookAndFeel::GetInt(LookAndFeel::IntID::ScrollbarFadeBeginDelay),
      nsITimer::TYPE_ONE_SHOT, "ScrollbarActivity::FadeBeginTimerFired");
}

void ScrollbarActivity::BeginFade() {
  MOZ_ASSERT(!IsActive());
  mScrollbarEffectivelyVisible = false;
  SetScrollbarActive(GetHorizontalScrollbar(), false);
  SetScrollbarActive(GetVerticalScrollbar(), false);
}

Element* ScrollbarActivity::GetScrollbarContent(bool aVertical) {
  nsIFrame* box = mScrollableFrame->GetScrollbarBox(aVertical);
  return box ? box->GetContent()->AsElement() : nullptr;
}

}  // namespace mozilla::layout
