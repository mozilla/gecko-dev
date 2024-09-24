/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PopoverData.h"
#include "nsGenericHTMLElement.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/CloseWatcher.h"
#include "mozilla/dom/CloseWatcherManager.h"

#include "nsIDOMEventListener.h"

namespace mozilla::dom {

class PopoverCloseWatcherListener : public nsIDOMEventListener {
 public:
  NS_DECL_ISUPPORTS

  explicit PopoverCloseWatcherListener(nsGenericHTMLElement* aPopover) {
    mPopover = do_GetWeakReference(aPopover);
  };

  NS_IMETHODIMP MOZ_CAN_RUN_SCRIPT HandleEvent(Event* aEvent) override {
    RefPtr<nsINode> node = do_QueryReferent(mPopover);
    if (RefPtr<nsGenericHTMLElement> popover =
            nsGenericHTMLElement::FromNodeOrNull(node)) {
      nsAutoString eventType;
      aEvent->GetType(eventType);
      if (eventType.EqualsLiteral("close")) {
        popover->HidePopover(IgnoreErrors());
      }
    }
    return NS_OK;
  }

 private:
  virtual ~PopoverCloseWatcherListener() = default;
  nsWeakPtr mPopover;
};
NS_IMPL_ISUPPORTS(PopoverCloseWatcherListener, nsIDOMEventListener)

CloseWatcher& PopoverData::EnsureCloseWatcher(nsGenericHTMLElement* aElement) {
  if (!mCloseWatcher) {
    RefPtr<Document> doc = aElement->OwnerDoc();
    if (doc->IsActive() && doc->IsCurrentActiveDocument()) {
      if (RefPtr<nsPIDOMWindowInner> window = doc->GetInnerWindow()) {
        mCloseWatcher = new CloseWatcher(window);
        RefPtr<PopoverCloseWatcherListener> eventListener =
            new PopoverCloseWatcherListener(aElement);
        mCloseWatcher->AddSystemEventListener(u"close"_ns, eventListener,
                                              false /* aUseCapture */,
                                              false /* aWantsUntrusted */);
        RefPtr manager = window->EnsureCloseWatcherManager();
        manager->Add(*mCloseWatcher);
      }
    }
  }
  return *mCloseWatcher;
}

PopoverToggleEventTask::PopoverToggleEventTask(nsWeakPtr aElement,
                                               PopoverVisibilityState aOldState)
    : Runnable("PopoverToggleEventTask"),
      mElement(std::move(aElement)),
      mOldState(aOldState) {}

NS_IMETHODIMP
PopoverToggleEventTask::Run() {
  nsCOMPtr<Element> element = do_QueryReferent(mElement);
  if (!element) {
    return NS_OK;
  }
  if (auto* htmlElement = nsGenericHTMLElement::FromNode(element)) {
    MOZ_KnownLive(htmlElement)->RunPopoverToggleEventTask(this, mOldState);
  }
  return NS_OK;
};

}  // namespace mozilla::dom
