/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsXBLWindowKeyHandler_h__
#define nsXBLWindowKeyHandler_h__

#include "nsWeakPtr.h"
#include "nsIDOMEventListener.h"

class nsIAtom;
class nsIDOMElement;
class nsIDOMKeyEvent;
class nsXBLSpecialDocInfo;
class nsXBLPrototypeHandler;

namespace mozilla {
namespace dom {
class Element;
class EventTarget;
}
}

class nsXBLWindowKeyHandler : public nsIDOMEventListener
{
public:
  nsXBLWindowKeyHandler(nsIDOMElement* aElement, mozilla::dom::EventTarget* aTarget);
  virtual ~nsXBLWindowKeyHandler();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMEVENTLISTENER

protected:
  nsresult WalkHandlers(nsIDOMKeyEvent* aKeyEvent, nsIAtom* aEventType);

  // walk the handlers, looking for one to handle the event
  nsresult WalkHandlersInternal(nsIDOMKeyEvent* aKeyEvent,
                                nsIAtom* aEventType, 
                                nsXBLPrototypeHandler* aHandler);

  // walk the handlers for aEvent, aCharCode and aIgnoreShiftKey
  bool WalkHandlersAndExecute(nsIDOMKeyEvent* aKeyEvent, nsIAtom* aEventType,
                                nsXBLPrototypeHandler* aHandler,
                                uint32_t aCharCode, bool aIgnoreShiftKey);

  // lazily load the handlers. Overridden to handle being attached
  // to a particular element rather than the document
  nsresult EnsureHandlers();

  // check if the given handler cares about the given key event
  bool EventMatched(nsXBLPrototypeHandler* inHandler, nsIAtom* inEventType,
                      nsIDOMKeyEvent* inEvent, uint32_t aCharCode,
                      bool aIgnoreShiftKey);

  // Is an HTML editable element focused
  bool IsHTMLEditableFieldFocused();

  // Returns the element which was passed as a parameter to the constructor,
  // unless the element has been removed from the document.
  already_AddRefed<mozilla::dom::Element> GetElement();
  // Using weak pointer to the DOM Element.
  nsWeakPtr              mWeakPtrForElement;
  mozilla::dom::EventTarget* mTarget; // weak ref

  // these are not owning references; the prototype handlers are owned
  // by the prototype bindings which are owned by the docinfo.
  nsXBLPrototypeHandler* mHandler;     // platform bindings
  nsXBLPrototypeHandler* mUserHandler; // user-specific bindings

  // holds document info about bindings
  static nsXBLSpecialDocInfo* sXBLSpecialDocInfo;
  static uint32_t sRefCnt;
};

already_AddRefed<nsXBLWindowKeyHandler>
NS_NewXBLWindowKeyHandler(nsIDOMElement* aElement,
                          mozilla::dom::EventTarget* aTarget);

#endif
