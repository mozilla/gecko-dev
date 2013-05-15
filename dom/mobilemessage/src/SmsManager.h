/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_mobilemessage_SmsManager_h
#define mozilla_dom_mobilemessage_SmsManager_h

#include "nsIDOMSmsManager.h"
#include "nsIObserver.h"
#include "nsDOMEventTargetHelper.h"

class nsIDOMMozSmsMessage;

namespace mozilla {
namespace dom {

class SmsManager : public nsDOMEventTargetHelper
                 , public nsIDOMMozSmsManager
                 , public nsIObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
  NS_DECL_NSIDOMMOZSMSMANAGER

  NS_FORWARD_NSIDOMEVENTTARGET(nsDOMEventTargetHelper::)

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(SmsManager,
                                           nsDOMEventTargetHelper)

  static already_AddRefed<SmsManager>
  CreateInstanceIfAllowed(nsPIDOMWindow *aWindow);

  void Init(nsPIDOMWindow *aWindow);
  void Shutdown();

private:
  /**
   * Internal Send() method used to send one message.
   */
  nsresult Send(JSContext* aCx, JSObject* aGlobal, JSString* aNumber,
                const nsAString& aMessage, jsval* aRequest);

  nsresult DispatchTrustedSmsEventToSelf(const nsAString& aEventName,
                                         nsIDOMMozSmsMessage* aMessage);

  /**
   * Helper to get message ID from SMS Message object
   */
  nsresult GetSmsMessageId(JSContext* aCx, const JS::Value &aSmsMessage,
                           int32_t &aId);
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_mobilemessage_SmsManager_h
