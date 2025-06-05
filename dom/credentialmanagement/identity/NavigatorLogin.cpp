/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Components.h"
#include "mozilla/dom/CredentialsContainer.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/NavigatorLogin.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/Maybe.h"
#include "mozilla/net/SFVService.h"
#include "mozilla/dom/WebIdentityHandler.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIGlobalObject.h"
#include "nsIPermissionManager.h"
#include "nsIPrincipal.h"
#include "nsString.h"

namespace mozilla {
namespace dom {

static constexpr nsLiteralCString kLoginStatusPermission =
    "self-reported-logged-in"_ns;

uint32_t ConvertStatusToPermission(LoginStatus aStatus) {
  if (aStatus == LoginStatus::Logged_in) {
    return nsIPermissionManager::ALLOW_ACTION;
  } else if (aStatus == LoginStatus::Logged_out) {
    return nsIPermissionManager::DENY_ACTION;
  } else {
    // This should be unreachable, but let's return a real value
    return nsIPermissionManager::UNKNOWN_ACTION;
  }
}

Maybe<LoginStatus> PermissionToStatus(uint32_t aPermission) {
  if (aPermission == nsIPermissionManager::ALLOW_ACTION) {
    return Some(LoginStatus::Logged_in);
  } else if (aPermission == nsIPermissionManager::DENY_ACTION) {
    return Some(LoginStatus::Logged_out);
  } else {
    if (aPermission == nsIPermissionManager::UNKNOWN_ACTION) {
      MOZ_ASSERT(false, "Unexpected permission action from login status");
    }
    return Nothing();
  }
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(NavigatorLogin, mOwner)

NavigatorLogin::~NavigatorLogin() = default;

JSObject* NavigatorLogin::WrapObject(JSContext* aCx,
                                     JS::Handle<JSObject*> aGivenProto) {
  return NavigatorLogin_Binding::Wrap(aCx, this, aGivenProto);
}

NavigatorLogin::NavigatorLogin(nsPIDOMWindowInner* aGlobal) : mOwner(aGlobal) {
  MOZ_ASSERT(mOwner);
};

already_AddRefed<mozilla::dom::Promise> NavigatorLogin::SetStatus(
    LoginStatus aStatus, mozilla::ErrorResult& aRv) {
  RefPtr<Promise> promise = Promise::Create(mOwner->AsGlobal(), aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  if (!CredentialsContainer::IsSameOriginWithAncestors(mOwner)) {
    promise->MaybeRejectWithSecurityError(
        "navigator.login.setStatus must be called in a frame that is same-origin with its ancestors"_ns);
    return promise.forget();
  }

  WebIdentityHandler* identityHandler = mOwner->GetOrCreateWebIdentityHandler();
  if (!identityHandler) {
    promise->MaybeRejectWithOperationError("");
    return promise.forget();
  }

  identityHandler->SetLoginStatus(aStatus, promise);
  return promise.forget();
}

// static
nsresult NavigatorLogin::SetLoginStatus(nsIPrincipal* aPrincipal,
                                        LoginStatus aStatus) {
  MOZ_ASSERT(XRE_IsParentProcess());

  nsCOMPtr<nsIPermissionManager> permMgr =
      components::PermissionManager::Service();
  if (!permMgr) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  return permMgr->AddFromPrincipal(aPrincipal, kLoginStatusPermission,
                                   ConvertStatusToPermission(aStatus),
                                   nsIPermissionManager::EXPIRE_NEVER, 0);
}

// static
nsresult NavigatorLogin::SetLoginStatus(nsIPrincipal* aPrincipal,
                                        const nsACString& aStatus) {
  LoginStatus parsedStatus;
  nsresult rv = ParseLoginStatusHeader(aStatus, parsedStatus);
  NS_ENSURE_SUCCESS(rv, rv);
  return SetLoginStatus(aPrincipal, parsedStatus);
}

// static
nsresult NavigatorLogin::ParseLoginStatusHeader(const nsACString& aStatus,
                                                LoginStatus& aResult) {
  nsCOMPtr<nsISFVService> sfv = net::GetSFVService();
  nsCOMPtr<nsISFVItem> item;
  nsresult rv = sfv->ParseItem(aStatus, getter_AddRefs(item));
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsISFVBareItem> value;
  rv = item->GetValue(getter_AddRefs(value));
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsISFVToken> token = do_QueryInterface(value);
  if (!token) {
    return NS_ERROR_UNEXPECTED;
  }

  nsAutoCString parsedStatus;
  rv = token->GetValue(parsedStatus);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (parsedStatus == "logged-in") {
    aResult = LoginStatus::Logged_in;
    return NS_OK;
  }
  if (parsedStatus == "logged-out") {
    aResult = LoginStatus::Logged_out;
    return NS_OK;
  }
  return NS_ERROR_INVALID_ARG;
}

// static
nsresult GetLoginStatus(nsIPrincipal* aPrincipal, Maybe<LoginStatus>& aStatus) {
  nsCOMPtr<nsIPermissionManager> permMgr =
      components::PermissionManager::Service();
  if (!permMgr) {
    aStatus = Nothing();
    return NS_ERROR_SERVICE_NOT_AVAILABLE;
  }
  uint32_t action;
  nsresult rv = permMgr->TestPermissionFromPrincipal(
      aPrincipal, kLoginStatusPermission, &action);
  if (NS_FAILED(rv)) {
    aStatus = Nothing();
    return rv;
  }
  aStatus = PermissionToStatus(action);
  return NS_OK;
}

}  // namespace dom
}  // namespace mozilla
