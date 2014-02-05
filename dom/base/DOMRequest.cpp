/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DOMRequest.h"

#include "DOMError.h"
#include "nsCxPusher.h"
#include "nsThreadUtils.h"
#include "DOMCursor.h"
#include "nsIDOMEvent.h"

using mozilla::dom::DOMRequest;
using mozilla::dom::DOMRequestService;
using mozilla::dom::DOMCursor;
using mozilla::AutoSafeJSContext;

DOMRequest::DOMRequest(nsPIDOMWindow* aWindow)
  : nsDOMEventTargetHelper(aWindow->IsInnerWindow() ?
                             aWindow : aWindow->GetCurrentInnerWindow())
  , mResult(JSVAL_VOID)
  , mDone(false)
{
}

NS_IMPL_CYCLE_COLLECTION_CLASS(DOMRequest)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(DOMRequest,
                                                  nsDOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mError)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(DOMRequest,
                                                nsDOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mError)
  tmp->mResult = JSVAL_VOID;
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(DOMRequest,
                                               nsDOMEventTargetHelper)
  // Don't need NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER because
  // nsDOMEventTargetHelper does it for us.
  NS_IMPL_CYCLE_COLLECTION_TRACE_JSVAL_MEMBER_CALLBACK(mResult)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(DOMRequest)
  NS_INTERFACE_MAP_ENTRY(nsIDOMDOMRequest)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(DOMRequest, nsDOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(DOMRequest, nsDOMEventTargetHelper)

/* virtual */ JSObject*
DOMRequest::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return DOMRequestBinding::Wrap(aCx, aScope, this);
}

NS_IMPL_EVENT_HANDLER(DOMRequest, success)
NS_IMPL_EVENT_HANDLER(DOMRequest, error)

NS_IMETHODIMP
DOMRequest::GetReadyState(nsAString& aReadyState)
{
  DOMRequestReadyState readyState = ReadyState();
  switch (readyState) {
    case DOMRequestReadyState::Pending:
      aReadyState.AssignLiteral("pending");
      break;
    case DOMRequestReadyState::Done:
      aReadyState.AssignLiteral("done");
      break;
    default:
      MOZ_CRASH("Unrecognized readyState.");
  }

  return NS_OK;
}

NS_IMETHODIMP
DOMRequest::GetResult(JS::MutableHandle<JS::Value> aResult)
{
  aResult.set(Result());
  return NS_OK;
}

NS_IMETHODIMP
DOMRequest::GetError(nsISupports** aError)
{
  NS_IF_ADDREF(*aError = GetError());
  return NS_OK;
}

void
DOMRequest::FireSuccess(JS::Handle<JS::Value> aResult)
{
  NS_ASSERTION(!mDone, "mDone shouldn't have been set to true already!");
  NS_ASSERTION(!mError, "mError shouldn't have been set!");
  NS_ASSERTION(mResult == JSVAL_VOID, "mResult shouldn't have been set!");

  mDone = true;
  if (JSVAL_IS_GCTHING(aResult)) {
    RootResultVal();
  }
  mResult = aResult;

  FireEvent(NS_LITERAL_STRING("success"), false, false);
}

void
DOMRequest::FireError(const nsAString& aError)
{
  NS_ASSERTION(!mDone, "mDone shouldn't have been set to true already!");
  NS_ASSERTION(!mError, "mError shouldn't have been set!");
  NS_ASSERTION(mResult == JSVAL_VOID, "mResult shouldn't have been set!");

  mDone = true;
  mError = new DOMError(GetOwner(), aError);

  FireEvent(NS_LITERAL_STRING("error"), true, true);
}

void
DOMRequest::FireError(nsresult aError)
{
  NS_ASSERTION(!mDone, "mDone shouldn't have been set to true already!");
  NS_ASSERTION(!mError, "mError shouldn't have been set!");
  NS_ASSERTION(mResult == JSVAL_VOID, "mResult shouldn't have been set!");

  mDone = true;
  mError = new DOMError(GetOwner(), aError);

  FireEvent(NS_LITERAL_STRING("error"), true, true);
}

void
DOMRequest::FireDetailedError(nsISupports* aError)
{
  NS_ASSERTION(!mDone, "mDone shouldn't have been set to true already!");
  NS_ASSERTION(!mError, "mError shouldn't have been set!");
  NS_ASSERTION(mResult == JSVAL_VOID, "mResult shouldn't have been set!");
  NS_ASSERTION(aError, "No detailed error provided");

  mDone = true;
  mError = aError;

  FireEvent(NS_LITERAL_STRING("error"), true, true);
}

void
DOMRequest::FireEvent(const nsAString& aType, bool aBubble, bool aCancelable)
{
  if (NS_FAILED(CheckInnerWindowCorrectness())) {
    return;
  }

  nsCOMPtr<nsIDOMEvent> event;
  NS_NewDOMEvent(getter_AddRefs(event), this, nullptr, nullptr);
  nsresult rv = event->InitEvent(aType, aBubble, aCancelable);
  if (NS_FAILED(rv)) {
    return;
  }

  event->SetTrusted(true);

  bool dummy;
  DispatchEvent(event, &dummy);
}

void
DOMRequest::RootResultVal()
{
  mozilla::HoldJSObjects(this);
}

NS_IMPL_ISUPPORTS1(DOMRequestService, nsIDOMRequestService)

NS_IMETHODIMP
DOMRequestService::CreateRequest(nsIDOMWindow* aWindow,
                                 nsIDOMDOMRequest** aRequest)
{
  nsCOMPtr<nsPIDOMWindow> win(do_QueryInterface(aWindow));
  NS_ENSURE_STATE(win);
  NS_ADDREF(*aRequest = new DOMRequest(win));

  return NS_OK;
}

NS_IMETHODIMP
DOMRequestService::CreateCursor(nsIDOMWindow* aWindow,
                                nsICursorContinueCallback* aCallback,
                                nsIDOMDOMCursor** aCursor)
{
  nsCOMPtr<nsPIDOMWindow> win(do_QueryInterface(aWindow));
  NS_ENSURE_STATE(win);
  NS_ADDREF(*aCursor = new DOMCursor(win, aCallback));

  return NS_OK;
}

NS_IMETHODIMP
DOMRequestService::FireSuccess(nsIDOMDOMRequest* aRequest,
                               JS::Handle<JS::Value> aResult)
{
  NS_ENSURE_STATE(aRequest);
  static_cast<DOMRequest*>(aRequest)->FireSuccess(aResult);

  return NS_OK;
}

NS_IMETHODIMP
DOMRequestService::FireError(nsIDOMDOMRequest* aRequest,
                             const nsAString& aError)
{
  NS_ENSURE_STATE(aRequest);
  static_cast<DOMRequest*>(aRequest)->FireError(aError);

  return NS_OK;
}

NS_IMETHODIMP
DOMRequestService::FireDetailedError(nsIDOMDOMRequest* aRequest,
                                     nsISupports* aError)
{
  NS_ENSURE_STATE(aRequest);
  static_cast<DOMRequest*>(aRequest)->FireDetailedError(aError);

  return NS_OK;
}

class FireSuccessAsyncTask : public nsRunnable
{

  FireSuccessAsyncTask(DOMRequest* aRequest,
                       const JS::Value& aResult) :
    mReq(aRequest),
    mResult(aResult),
    mIsSetup(false)
  {
  }

public:

  void
  Setup()
  {
    AutoSafeJSContext cx;
    JS_AddValueRoot(cx, &mResult);
    mIsSetup = true;
  }

  // Due to the fact that initialization can fail during shutdown (since we
  // can't fetch a js context), set up an initiatization function to make sure
  // we can return the failure appropriately
  static nsresult
  Dispatch(DOMRequest* aRequest,
           const JS::Value& aResult)
  {
    NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
    nsRefPtr<FireSuccessAsyncTask> asyncTask = new FireSuccessAsyncTask(aRequest, aResult);
    asyncTask->Setup();
    if (NS_FAILED(NS_DispatchToMainThread(asyncTask))) {
      NS_WARNING("Failed to dispatch to main thread!");
      return NS_ERROR_FAILURE;
    }
    return NS_OK;
  }

  NS_IMETHODIMP
  Run()
  {
    mReq->FireSuccess(JS::Handle<JS::Value>::fromMarkedLocation(&mResult));
    return NS_OK;
  }

  ~FireSuccessAsyncTask()
  {
    NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
    if(!mIsSetup) {
      // If we never set up, no reason to unroot
      return;
    }

    AutoSafeJSContext cx;
    JS_RemoveValueRoot(cx, &mResult);
  }
private:
  nsRefPtr<DOMRequest> mReq;
  JS::Value mResult;
  bool mIsSetup;
};

class FireErrorAsyncTask : public nsRunnable
{
public:
  FireErrorAsyncTask(DOMRequest* aRequest,
                     const nsAString& aError) :
    mReq(aRequest),
    mError(aError)
  {
  }

  NS_IMETHODIMP
  Run()
  {
    mReq->FireError(mError);
    return NS_OK;
  }
private:
  nsRefPtr<DOMRequest> mReq;
  nsString mError;
};

NS_IMETHODIMP
DOMRequestService::FireSuccessAsync(nsIDOMDOMRequest* aRequest,
                                    JS::Handle<JS::Value> aResult)
{
  NS_ENSURE_STATE(aRequest);
  return FireSuccessAsyncTask::Dispatch(static_cast<DOMRequest*>(aRequest), aResult);
}

NS_IMETHODIMP
DOMRequestService::FireErrorAsync(nsIDOMDOMRequest* aRequest,
                                  const nsAString& aError)
{
  NS_ENSURE_STATE(aRequest);
  nsCOMPtr<nsIRunnable> asyncTask =
    new FireErrorAsyncTask(static_cast<DOMRequest*>(aRequest), aError);
  if (NS_FAILED(NS_DispatchToMainThread(asyncTask))) {
    NS_WARNING("Failed to dispatch to main thread!");
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP
DOMRequestService::FireDone(nsIDOMDOMCursor* aCursor) {
  NS_ENSURE_STATE(aCursor);
  static_cast<DOMCursor*>(aCursor)->FireDone();

  return NS_OK;
}
