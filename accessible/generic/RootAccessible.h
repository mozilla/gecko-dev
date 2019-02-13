/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_RootAccessible_h__
#define mozilla_a11y_RootAccessible_h__

#include "HyperTextAccessible.h"
#include "DocAccessibleWrap.h"

#include "nsIDOMEventListener.h"

class nsIDocument;

namespace mozilla {
namespace a11y {

class RootAccessible : public DocAccessibleWrap,
                       public nsIDOMEventListener
{
  NS_DECL_ISUPPORTS_INHERITED

public:
  RootAccessible(nsIDocument* aDocument, nsIContent* aRootContent,
                 nsIPresShell* aPresShell);

  // nsIDOMEventListener
  NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent) override;

  // Accessible
  virtual void Shutdown() override;
  virtual mozilla::a11y::ENameValueFlag Name(nsString& aName) override;
  virtual Relation RelationByType(RelationType aType) override;
  virtual mozilla::a11y::role NativeRole() override;
  virtual uint64_t NativeState() override;

  // RootAccessible

  /**
   * Notify that the sub document presshell was activated.
   */
  virtual void DocumentActivated(DocAccessible* aDocument);

protected:
  virtual ~RootAccessible();

  /**
   * Add/remove DOM event listeners.
   */
  virtual nsresult AddEventListeners() override;
  virtual nsresult RemoveEventListeners() override;

  /**
   * Process the DOM event.
   */
  void ProcessDOMEvent(nsIDOMEvent* aEvent);

  /**
   * Process "popupshown" event. Used by HandleEvent().
   */
  void HandlePopupShownEvent(Accessible* aAccessible);

  /*
   * Process "popuphiding" event. Used by HandleEvent().
   */
  void HandlePopupHidingEvent(nsINode* aNode);

#ifdef MOZ_XUL
    void HandleTreeRowCountChangedEvent(nsIDOMEvent* aEvent,
                                        XULTreeAccessible* aAccessible);
    void HandleTreeInvalidatedEvent(nsIDOMEvent* aEvent,
                                    XULTreeAccessible* aAccessible);

    uint32_t GetChromeFlags();
#endif
};

inline RootAccessible*
Accessible::AsRoot()
{
  return IsRoot() ? static_cast<mozilla::a11y::RootAccessible*>(this) : nullptr;
}

} // namespace a11y
} // namespace mozilla

#endif
