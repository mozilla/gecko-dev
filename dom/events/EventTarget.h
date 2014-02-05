/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_EventTarget_h_
#define mozilla_dom_EventTarget_h_

#include "nsIDOMEventTarget.h"
#include "nsWrapperCache.h"
#include "nsIAtom.h"

class nsDOMEvent;
class nsIDOMWindow;
class nsIDOMEventListener;

namespace mozilla {

class ErrorResult;

namespace dom {

class EventListener;
class EventHandlerNonNull;
template <class T> struct Nullable;

// IID for the dom::EventTarget interface
#define NS_EVENTTARGET_IID \
{ 0xce3817d0, 0x177b, 0x402f, \
 { 0xae, 0x75, 0xf8, 0x4e, 0xbe, 0x5a, 0x07, 0xc3 } }

class EventTarget : public nsIDOMEventTarget,
                    public nsWrapperCache
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_EVENTTARGET_IID)

  // WebIDL API
  using nsIDOMEventTarget::AddEventListener;
  using nsIDOMEventTarget::RemoveEventListener;
  using nsIDOMEventTarget::DispatchEvent;
  virtual void AddEventListener(const nsAString& aType,
                                EventListener* aCallback,
                                bool aCapture,
                                const Nullable<bool>& aWantsUntrusted,
                                ErrorResult& aRv) = 0;
  virtual void RemoveEventListener(const nsAString& aType,
                                   EventListener* aCallback,
                                   bool aCapture,
                                   ErrorResult& aRv);
  bool DispatchEvent(nsDOMEvent& aEvent, ErrorResult& aRv);

  // Note, this takes the type in onfoo form!
  EventHandlerNonNull* GetEventHandler(const nsAString& aType)
  {
    nsCOMPtr<nsIAtom> type = do_GetAtom(aType);
    return GetEventHandler(type, EmptyString());
  }

  // Note, this takes the type in onfoo form!
  void SetEventHandler(const nsAString& aType, EventHandlerNonNull* aHandler,
                       ErrorResult& rv);

  // Note, for an event 'foo' aType will be 'onfoo'.
  virtual void EventListenerAdded(nsIAtom* aType) {}
  virtual void EventListenerRemoved(nsIAtom* aType) {}

  // Returns an outer window that corresponds to the inner window this event
  // target is associated with.  Will return null if the inner window is not the
  // current inner or if there is no window around at all.
  virtual nsIDOMWindow* GetOwnerGlobal() = 0;

  /**
   * Get the event listener manager, creating it if it does not already exist.
   */
  virtual nsEventListenerManager* GetOrCreateListenerManager() = 0;

  /**
   * Get the event listener manager, returning null if it does not already
   * exist.
   */
  virtual nsEventListenerManager* GetExistingListenerManager() const = 0;

protected:
  EventHandlerNonNull* GetEventHandler(nsIAtom* aType,
                                       const nsAString& aTypeString);
  void SetEventHandler(nsIAtom* aType, const nsAString& aTypeString,
                       EventHandlerNonNull* aHandler);
};

NS_DEFINE_STATIC_IID_ACCESSOR(EventTarget, NS_EVENTTARGET_IID)

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_EventTarget_h_
