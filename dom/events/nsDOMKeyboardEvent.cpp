/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDOMKeyboardEvent.h"
#include "prtime.h"
#include "mozilla/TextEvents.h"

using namespace mozilla;

nsDOMKeyboardEvent::nsDOMKeyboardEvent(mozilla::dom::EventTarget* aOwner,
                                       nsPresContext* aPresContext,
                                       WidgetKeyboardEvent* aEvent)
  : nsDOMUIEvent(aOwner, aPresContext, aEvent ? aEvent :
                 new WidgetKeyboardEvent(false, 0, nullptr))
{
  NS_ASSERTION(mEvent->eventStructType == NS_KEY_EVENT, "event type mismatch");

  if (aEvent) {
    mEventIsInternal = false;
  }
  else {
    mEventIsInternal = true;
    mEvent->time = PR_Now();
    mEvent->AsKeyboardEvent()->mKeyNameIndex = KEY_NAME_INDEX_USE_STRING;
  }
}

NS_IMPL_ADDREF_INHERITED(nsDOMKeyboardEvent, nsDOMUIEvent)
NS_IMPL_RELEASE_INHERITED(nsDOMKeyboardEvent, nsDOMUIEvent)

NS_INTERFACE_MAP_BEGIN(nsDOMKeyboardEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMKeyEvent)
NS_INTERFACE_MAP_END_INHERITING(nsDOMUIEvent)

bool
nsDOMKeyboardEvent::AltKey()
{
  return mEvent->AsKeyboardEvent()->IsAlt();
}

NS_IMETHODIMP
nsDOMKeyboardEvent::GetAltKey(bool* aIsDown)
{
  NS_ENSURE_ARG_POINTER(aIsDown);
  *aIsDown = AltKey();
  return NS_OK;
}

bool
nsDOMKeyboardEvent::CtrlKey()
{
  return mEvent->AsKeyboardEvent()->IsControl();
}

NS_IMETHODIMP
nsDOMKeyboardEvent::GetCtrlKey(bool* aIsDown)
{
  NS_ENSURE_ARG_POINTER(aIsDown);
  *aIsDown = CtrlKey();
  return NS_OK;
}

bool
nsDOMKeyboardEvent::ShiftKey()
{
  return mEvent->AsKeyboardEvent()->IsShift();
}

NS_IMETHODIMP
nsDOMKeyboardEvent::GetShiftKey(bool* aIsDown)
{
  NS_ENSURE_ARG_POINTER(aIsDown);
  *aIsDown = ShiftKey();
  return NS_OK;
}

bool
nsDOMKeyboardEvent::MetaKey()
{
  return mEvent->AsKeyboardEvent()->IsMeta();
}

NS_IMETHODIMP
nsDOMKeyboardEvent::GetMetaKey(bool* aIsDown)
{
  NS_ENSURE_ARG_POINTER(aIsDown);
  *aIsDown = MetaKey();
  return NS_OK;
}

bool
nsDOMKeyboardEvent::Repeat()
{
  return mEvent->AsKeyboardEvent()->mIsRepeat;
}

NS_IMETHODIMP
nsDOMKeyboardEvent::GetRepeat(bool* aIsRepeat)
{
  NS_ENSURE_ARG_POINTER(aIsRepeat);
  *aIsRepeat = Repeat();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMKeyboardEvent::GetModifierState(const nsAString& aKey,
                                     bool* aState)
{
  NS_ENSURE_ARG_POINTER(aState);

  *aState = GetModifierState(aKey);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMKeyboardEvent::GetKey(nsAString& aKeyName)
{
  mEvent->AsKeyboardEvent()->GetDOMKeyName(aKeyName);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMKeyboardEvent::GetCharCode(uint32_t* aCharCode)
{
  NS_ENSURE_ARG_POINTER(aCharCode);
  *aCharCode = CharCode();
  return NS_OK;
}

uint32_t
nsDOMKeyboardEvent::CharCode()
{
  switch (mEvent->message) {
  case NS_KEY_UP:
  case NS_KEY_DOWN:
    return 0;
  case NS_KEY_PRESS:
    return mEvent->AsKeyboardEvent()->charCode;
  }
  return 0;
}

NS_IMETHODIMP
nsDOMKeyboardEvent::GetKeyCode(uint32_t* aKeyCode)
{
  NS_ENSURE_ARG_POINTER(aKeyCode);
  *aKeyCode = KeyCode();
  return NS_OK;
}

uint32_t
nsDOMKeyboardEvent::KeyCode()
{
  switch (mEvent->message) {
  case NS_KEY_UP:
  case NS_KEY_PRESS:
  case NS_KEY_DOWN:
    return mEvent->AsKeyboardEvent()->keyCode;
  }
  return 0;
}

uint32_t
nsDOMKeyboardEvent::Which()
{
  switch (mEvent->message) {
    case NS_KEY_UP:
    case NS_KEY_DOWN:
      return KeyCode();
    case NS_KEY_PRESS:
      //Special case for 4xp bug 62878.  Try to make value of which
      //more closely mirror the values that 4.x gave for RETURN and BACKSPACE
      {
        uint32_t keyCode = mEvent->AsKeyboardEvent()->keyCode;
        if (keyCode == NS_VK_RETURN || keyCode == NS_VK_BACK) {
          return keyCode;
        }
        return CharCode();
      }
  }

  return 0;
}

NS_IMETHODIMP
nsDOMKeyboardEvent::GetLocation(uint32_t* aLocation)
{
  NS_ENSURE_ARG_POINTER(aLocation);

  *aLocation = Location();
  return NS_OK;
}

uint32_t
nsDOMKeyboardEvent::Location()
{
  return mEvent->AsKeyboardEvent()->location;
}

NS_IMETHODIMP
nsDOMKeyboardEvent::InitKeyEvent(const nsAString& aType, bool aCanBubble, bool aCancelable,
                                 nsIDOMWindow* aView, bool aCtrlKey, bool aAltKey,
                                 bool aShiftKey, bool aMetaKey,
                                 uint32_t aKeyCode, uint32_t aCharCode)
{
  nsresult rv = nsDOMUIEvent::InitUIEvent(aType, aCanBubble, aCancelable, aView, 0);
  NS_ENSURE_SUCCESS(rv, rv);

  WidgetKeyboardEvent* keyEvent = mEvent->AsKeyboardEvent();
  keyEvent->InitBasicModifiers(aCtrlKey, aAltKey, aShiftKey, aMetaKey);
  keyEvent->keyCode = aKeyCode;
  keyEvent->charCode = aCharCode;

  return NS_OK;
}

nsresult NS_NewDOMKeyboardEvent(nsIDOMEvent** aInstancePtrResult,
                                mozilla::dom::EventTarget* aOwner,
                                nsPresContext* aPresContext,
                                WidgetKeyboardEvent* aEvent)
{
  nsDOMKeyboardEvent* it = new nsDOMKeyboardEvent(aOwner, aPresContext, aEvent);
  return CallQueryInterface(it, aInstancePtrResult);
}
