/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Activity.h"
#include "mozilla/dom/ToJSValue.h"
#include "mozilla/dom/ContentChild.h"
#include "nsContentUtils.h"
#include "nsDOMClassInfo.h"
#include "nsIConsoleService.h"
#include "nsIDocShell.h"
#include "nsIDocument.h"

using namespace mozilla::dom;

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(Activity)
NS_INTERFACE_MAP_END_INHERITING(DOMRequest)

NS_IMPL_ADDREF_INHERITED(Activity, DOMRequest)
NS_IMPL_RELEASE_INHERITED(Activity, DOMRequest)

NS_IMPL_CYCLE_COLLECTION_INHERITED(Activity, DOMRequest,
                                   mProxy)

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(Activity, DOMRequest)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

/* virtual */ JSObject*
Activity::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return MozActivityBinding::Wrap(aCx, this, aGivenProto);
}

nsresult
Activity::Initialize(nsPIDOMWindow* aWindow,
                     JSContext* aCx,
                     const ActivityOptions& aOptions)
{
  MOZ_ASSERT(aWindow);

  nsCOMPtr<nsIDocument> document = aWindow->GetExtantDoc();

  bool isActive;
  aWindow->GetDocShell()->GetIsActive(&isActive);

  if (!isActive &&
      !nsContentUtils::IsChromeDoc(document)) {
    nsCOMPtr<nsIDOMRequestService> rs =
      do_GetService("@mozilla.org/dom/dom-request-service;1");
    rs->FireErrorAsync(static_cast<DOMRequest*>(this),
                       NS_LITERAL_STRING("NotUserInput"));

    nsCOMPtr<nsIConsoleService> console(
      do_GetService("@mozilla.org/consoleservice;1"));
    NS_ENSURE_TRUE(console, NS_OK);

    nsString message =
      NS_LITERAL_STRING("Can only start activity from user input or chrome code");
    console->LogStringMessage(message.get());

    return NS_OK;
  }

  // Instantiate a JS proxy that will do the child <-> parent communication
  // with the JS implementation of the backend.
  nsresult rv;
  mProxy = do_CreateInstance("@mozilla.org/dom/activities/proxy;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  // We're about the pass the dictionary to a JS-implemented component, so
  // rehydrate it in a system scode so that security wrappers don't get in the
  // way. See bug 1161748 comment 16.
  bool ok;
  JS::Rooted<JS::Value> optionsValue(aCx);
  {
    JSAutoCompartment ac(aCx, xpc::PrivilegedJunkScope());
    ok = ToJSValue(aCx, aOptions, &optionsValue);
    NS_ENSURE_TRUE(ok, NS_ERROR_FAILURE);
  }
  ok = JS_WrapValue(aCx, &optionsValue);
  NS_ENSURE_TRUE(ok, NS_ERROR_FAILURE);

  ContentChild *cpc = ContentChild::GetSingleton();
  uint64_t childID = cpc ? cpc->GetID() : 0;

  mProxy->StartActivity(static_cast<nsIDOMDOMRequest*>(this), optionsValue,
                        aWindow, childID);
  return NS_OK;
}

Activity::~Activity()
{
  if (mProxy) {
    mProxy->Cleanup();
  }
}

Activity::Activity(nsPIDOMWindow* aWindow)
  : DOMRequest(aWindow)
{
}

