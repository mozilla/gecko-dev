/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/MerchantValidationEvent.h"
#include "nsIURLParser.h"
#include "nsNetCID.h"
#include "mozilla/dom/PaymentRequest.h"
#include "mozilla/dom/Location.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED(MerchantValidationEvent, Event, mRequest)

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(MerchantValidationEvent, Event)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(MerchantValidationEvent)
NS_INTERFACE_MAP_END_INHERITING(Event)

NS_IMPL_ADDREF_INHERITED(MerchantValidationEvent, Event)
NS_IMPL_RELEASE_INHERITED(MerchantValidationEvent, Event)

class Promise;
class EventTarget;

already_AddRefed<MerchantValidationEvent>
MerchantValidationEvent::Constructor(
  EventTarget* aOwner,
  const nsAString& aType,
  const MerchantValidationEventInit& aEventInitDict)
{
  RefPtr<MerchantValidationEvent> e = new MerchantValidationEvent(aOwner);
  bool trusted = e->Init(aOwner);
  e->InitEvent(aType, aEventInitDict.mBubbles, aEventInitDict.mCancelable);
  e->SetTrusted(trusted);
  e->SetComposed(aEventInitDict.mComposed);
  return e.forget();
}

already_AddRefed<MerchantValidationEvent>
MerchantValidationEvent::Constructor(
  const GlobalObject& aGlobal,
  const nsAString& aType,
  const MerchantValidationEventInit& aEventInitDict,
  ErrorResult& aRv)
{
  nsCOMPtr<EventTarget> owner = do_QueryInterface(aGlobal.GetAsSupports());
  auto event = Constructor(owner, aType, aEventInitDict);
  // Let base be the event’s relevant settings object’s API base URL.
  nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(owner);
  RefPtr<Location> location = window->GetLocation();
  // TODO: ENSURE LOCATION
  nsAutoString base;
  nsresult res = location->ToString(base);
  // TODO: ENSURE RESULT HERE
  NS_ConvertUTF16toUTF8 url(base);

  // Let input be the empty string.
  nsString input = aEventInitDict.mValidationURL;

  // If eventInitDict was passed, set input to the value of
  // eventInitDict["validationURL"]. Let validationURL be the result of URL
  // parsing input and base.
  nsCOMPtr<nsIURLParser> urlParser = do_GetService(NS_STDURLPARSER_CONTRACTID);
  MOZ_ASSERT(urlParser);
  uint32_t schemePos = 0;
  int32_t schemeLen = 0;
  uint32_t authorityPos = 0;
  int32_t authorityLen = 0;
  uint32_t pathPos = 0;
  int32_t pathLength = 0;
  nsresult rv = urlParser->ParseURL(url.get(),
                                    url.Length(),
                                    &schemePos,
                                    &schemeLen,
                                    &authorityPos,
                                    &authorityLen,
                                    &pathPos,
                                    &pathLength);
  // If validationURL is failure, throw a TypeError.
  if (NS_FAILED(rv)) {
    nsAutoString message;
    message.AssignLiteral("Invalid validationURL");
    aRv.ThrowTypeError<MSG_ILLEGAL_TYPE_PR_CONSTRUCTOR>(message);
    return nullptr;
  }
  // Initialize event.validationURL attribute to validationURL.

  //event->SetValidationURL(url.get());

  // Initialize event.[[waitForUpdate]] to false.
  return event;
}

MerchantValidationEvent::MerchantValidationEvent(EventTarget* aOwner)
  : Event(aOwner, nullptr, nullptr)
  , mWaitForUpdate(false)
  , mRequest(nullptr)
{
  MOZ_ASSERT(aOwner);
}

void
MerchantValidationEvent::ResolvedCallback(JSContext* aCx,
                                          JS::Handle<JS::Value> aValue)
{
  MOZ_ASSERT(aCx);
  MOZ_ASSERT(mRequest);

  if (NS_WARN_IF(!aValue.isObject()) || !mWaitForUpdate) {
    return;
  }

  mWaitForUpdate = false;
  mRequest->SetUpdating(false);
}

void
MerchantValidationEvent::RejectedCallback(JSContext* aCx,
                                          JS::Handle<JS::Value> aValue)
{
  MOZ_ASSERT(mRequest);

  mRequest->AbortUpdate(NS_ERROR_DOM_ABORT_ERR, false);
  mWaitForUpdate = false;
  mRequest->SetUpdating(false);
}

void
MerchantValidationEvent::Complete(Promise& aPromise, ErrorResult& aRv)
{
  if (!IsTrusted()) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  MOZ_ASSERT(mRequest);

  if (mWaitForUpdate || !mRequest->ReadyForUpdate()) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  aPromise.AppendNativeHandler(this);

  StopPropagation();
  StopImmediatePropagation();
  mWaitForUpdate = true;
  mRequest->SetUpdating(true);
}

void
MerchantValidationEvent::SetRequest(PaymentRequest* aRequest)
{
  MOZ_ASSERT(IsTrusted());
  MOZ_ASSERT(!mRequest);
  MOZ_ASSERT(aRequest);

  mRequest = aRequest;
}

void
MerchantValidationEvent::GetValidationURL(nsAString& aValidationURL)
{
  aValidationURL.Assign(mValidationURL);
}

void
MerchantValidationEvent::SetValidationURL(nsAString& aValidationURL)
{
  mValidationURL.Assign(aValidationURL);
}

MerchantValidationEvent::~MerchantValidationEvent() {}

JSObject*
MerchantValidationEvent::WrapObjectInternal(JSContext* aCx,
                                            JS::Handle<JSObject*> aGivenProto)
{
  return MerchantValidationEvent_Binding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla
