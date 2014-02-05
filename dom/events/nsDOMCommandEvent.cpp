/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDOMCommandEvent.h"
#include "prtime.h"
#include "mozilla/MiscEvents.h"

using namespace mozilla;

nsDOMCommandEvent::nsDOMCommandEvent(mozilla::dom::EventTarget* aOwner,
                                     nsPresContext* aPresContext,
                                     WidgetCommandEvent* aEvent)
  : nsDOMEvent(aOwner, aPresContext, aEvent ? aEvent :
               new WidgetCommandEvent(false, nullptr, nullptr, nullptr))
{
  mEvent->time = PR_Now();
  if (aEvent) {
    mEventIsInternal = false;
  } else {
    mEventIsInternal = true;
  }
}

NS_INTERFACE_MAP_BEGIN(nsDOMCommandEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCommandEvent)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEvent)

NS_IMPL_ADDREF_INHERITED(nsDOMCommandEvent, nsDOMEvent)
NS_IMPL_RELEASE_INHERITED(nsDOMCommandEvent, nsDOMEvent)

NS_IMETHODIMP
nsDOMCommandEvent::GetCommand(nsAString& aCommand)
{
  nsIAtom* command = mEvent->AsCommandEvent()->command;
  if (command) {
    command->ToString(aCommand);
  } else {
    aCommand.Truncate();
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDOMCommandEvent::InitCommandEvent(const nsAString& aTypeArg,
                                    bool aCanBubbleArg,
                                    bool aCancelableArg,
                                    const nsAString& aCommand)
{
  nsresult rv = nsDOMEvent::InitEvent(aTypeArg, aCanBubbleArg, aCancelableArg);
  NS_ENSURE_SUCCESS(rv, rv);

  mEvent->AsCommandEvent()->command = do_GetAtom(aCommand);
  return NS_OK;
}

nsresult NS_NewDOMCommandEvent(nsIDOMEvent** aInstancePtrResult,
                               mozilla::dom::EventTarget* aOwner,
                               nsPresContext* aPresContext,
                               WidgetCommandEvent* aEvent)
{
  nsDOMCommandEvent* it = new nsDOMCommandEvent(aOwner, aPresContext, aEvent);

  return CallQueryInterface(it, aInstancePtrResult);
}
