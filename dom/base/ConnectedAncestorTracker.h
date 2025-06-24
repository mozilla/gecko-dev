/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ConnectedAncestorTracker_h
#define mozilla_ConnectedAncestorTracker_h

#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/PresShell.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/Element.h"
#include "nsIContent.h"

namespace mozilla {

/**
 * AutoConnectedAncestorTracker is a struct which keeps referring the connected
 * and closest ancestor of a content node.  E.g., say, there are nodes are:
 * Document -> <html> -> <body> -> <div> and starts tracking it with the <div>,
 * and the <body> is removed, this refers the <html> with mConnectedAncestor.
 * Note that even after reconnected the <body>, this won't refer the <div> as
 * connected one.
 */
struct MOZ_STACK_CLASS AutoConnectedAncestorTracker final {
  explicit AutoConnectedAncestorTracker(nsIContent& aContent)
      : mContent(aContent),
        mPresShell(aContent.IsInComposedDoc()
                       ? aContent.OwnerDoc()->GetPresShell()
                       : nullptr) {
    if (mPresShell) {
      mPresShell->AddConnectedAncestorTracker(*this);
    }
  }
  ~AutoConnectedAncestorTracker() {
    if (mPresShell) {
      mPresShell->RemoveConnectedAncestorTracker(*this);
    }
  }

  [[nodiscard]] bool ContentWasRemoved() const {
    return mPresShell && mConnectedAncestor;
  }
  [[nodiscard]] dom::Element* GetConnectedElement() const {
    return ContentWasRemoved()
               ? mConnectedAncestor->GetAsElementOrParentElement()
               : mContent->GetAsElementOrParentElement();
  }
  [[nodiscard]] nsIContent* GetConnectedContent() const {
    return ContentWasRemoved() ? nsIContent::FromNode(mConnectedAncestor)
                               : mContent.get();
  }
  [[nodiscard]] nsINode& ConnectedNode() const {
    return ContentWasRemoved() ? *mConnectedAncestor : mContent.ref();
  }

  // Store the original content node.
  const OwningNonNull<nsIContent> mContent;
  // Store the connected ancestor node if and only if mContent has been deleted
  // from the document.
  nsCOMPtr<nsINode> mConnectedAncestor;

  // The PresShell which manages this instance.
  const RefPtr<PresShell> mPresShell;

  AutoConnectedAncestorTracker* mPreviousTracker = nullptr;
};

}  // namespace mozilla

#endif  // #ifndef mozilla_ConnectedAncestorTracker_h
