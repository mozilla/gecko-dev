/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "ChromeObserver.h"

#include "nsIBaseWindow.h"
#include "nsIWidget.h"
#include "nsIFrame.h"

#include "nsContentUtils.h"
#include "nsView.h"
#include "nsPresContext.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/DocumentInlines.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/MutationEventBinding.h"
#include "nsXULElement.h"

namespace mozilla::dom {

NS_IMPL_ISUPPORTS(ChromeObserver, nsIMutationObserver)

ChromeObserver::ChromeObserver(Document* aDocument)
    : nsStubMutationObserver(), mDocument(aDocument) {}

ChromeObserver::~ChromeObserver() = default;

void ChromeObserver::Init() {
  mDocument->AddMutationObserver(this);
  Element* rootElement = mDocument->GetRootElement();
  if (!rootElement) {
    return;
  }
  nsAutoScriptBlocker scriptBlocker;
  uint32_t attributeCount = rootElement->GetAttrCount();
  for (uint32_t i = 0; i < attributeCount; i++) {
    BorrowedAttrInfo info = rootElement->GetAttrInfoAt(i);
    const nsAttrName* name = info.mName;
    if (name->LocalName() == nsGkAtoms::customtitlebar) {
      // Some linux windows managers have an issue when the customtitlebar is
      // applied while the browser is loading (bug 1598848). For now, skip
      // applying this attribute when initializing.
      continue;
    }
    AttributeChanged(rootElement, name->NamespaceID(), name->LocalName(),
                     MutationEvent_Binding::ADDITION, nullptr);
  }
}

nsIWidget* ChromeObserver::GetWindowWidget() {
  // only top level chrome documents can set the titlebar color
  if (mDocument && mDocument->IsRootDisplayDocument()) {
    nsCOMPtr<nsISupports> container = mDocument->GetContainer();
    nsCOMPtr<nsIBaseWindow> baseWindow = do_QueryInterface(container);
    if (baseWindow) {
      nsCOMPtr<nsIWidget> mainWidget;
      baseWindow->GetMainWidget(getter_AddRefs(mainWidget));
      return mainWidget;
    }
  }
  return nullptr;
}

void ChromeObserver::SetDrawsTitle(bool aState) {
  nsIWidget* mainWidget = GetWindowWidget();
  if (mainWidget) {
    // We can do this synchronously because SetDrawsTitle doesn't have any
    // synchronous effects apart from a harmless invalidation.
    mainWidget->SetDrawsTitle(aState);
  }
}

void ChromeObserver::AttributeChanged(dom::Element* aElement,
                                      int32_t aNamespaceID, nsAtom* aName,
                                      int32_t aModType,
                                      const nsAttrValue* aOldValue) {
  // We only care about changes to the root element.
  if (!mDocument || aElement != mDocument->GetRootElement()) {
    return;
  }

  if (aModType == dom::MutationEvent_Binding::ADDITION ||
      aModType == dom::MutationEvent_Binding::REMOVAL) {
    const bool added = aModType == dom::MutationEvent_Binding::ADDITION;
    if (aName == nsGkAtoms::hidechrome) {
      HideWindowChrome(added);
    } else if (aName == nsGkAtoms::customtitlebar) {
      SetCustomTitlebar(added);
    } else if (aName == nsGkAtoms::drawtitle) {
      SetDrawsTitle(added);
    }
  }
  if (aName == nsGkAtoms::localedir) {
    // if the localedir changed on the root element, reset the document
    // direction
    mDocument->ResetDocumentDirection();
  }
  if (aName == nsGkAtoms::title &&
      aModType != dom::MutationEvent_Binding::REMOVAL) {
    mDocument->NotifyPossibleTitleChange(false);
  }
}

void ChromeObserver::NodeWillBeDestroyed(nsINode* aNode) {
  mDocument = nullptr;
}

void ChromeObserver::SetCustomTitlebar(bool aCustomTitlebar) {
  if (nsIWidget* mainWidget = GetWindowWidget()) {
    // SetCustomTitlebar can dispatch native events, hence doing it off a
    // script runner
    nsContentUtils::AddScriptRunner(NewRunnableMethod<bool>(
        "SetCustomTitlebar", mainWidget, &nsIWidget::SetCustomTitlebar,
        aCustomTitlebar));
  }
}

nsresult ChromeObserver::HideWindowChrome(bool aShouldHide) {
  // only top level chrome documents can hide the window chrome
  if (!mDocument->IsRootDisplayDocument()) return NS_OK;

  nsPresContext* presContext = mDocument->GetPresContext();

  if (presContext && presContext->IsChrome()) {
    nsIFrame* frame = mDocument->GetDocumentElement()->GetPrimaryFrame();

    if (frame) {
      nsView* view = frame->GetClosestView();

      if (view) {
        nsIWidget* w = view->GetWidget();
        NS_ENSURE_STATE(w);
        w->HideWindowChrome(aShouldHide);
      }
    }
  }

  return NS_OK;
}

}  // namespace mozilla::dom
