/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/IdentityNetworkHelpers.h"

namespace mozilla::dom {

RefPtr<MozPromise<IdentityProviderWellKnown, nsresult, true>>
IdentityNetworkHelpers::FetchWellKnownHelper(
    nsIURI* aWellKnown, nsIPrincipal* aTriggeringPrincipal) {
  RefPtr<MozPromise<IdentityProviderWellKnown, nsresult, true>::Private>
      result =
          new MozPromise<IdentityProviderWellKnown, nsresult, true>::Private(
              __func__);
  nsresult rv;
  nsCOMPtr<nsICredentialChooserService> ccService =
      mozilla::components::CredentialChooserService::Service(&rv);
  if (NS_FAILED(rv) || !ccService) {
    result->Reject(rv, __func__);
    return result;
  }

  RefPtr<Promise> serviceResult;
  rv = ccService->FetchWellKnown(aWellKnown, aTriggeringPrincipal,
                                 getter_AddRefs(serviceResult));
  if (NS_FAILED(rv)) {
    result->Reject(rv, __func__);
    return result;
  }
  serviceResult->AddCallbacksWithCycleCollectedArgs(
      [result](JSContext* aCx, JS::Handle<JS::Value> aValue, ErrorResult&) {
        IdentityProviderWellKnown value;
        bool success = value.Init(aCx, aValue);
        if (!success) {
          JS_ClearPendingException(aCx);
          result->Reject(NS_ERROR_INVALID_ARG, __func__);
          return;
        }
        result->Resolve(value, __func__);
      },
      [result](JSContext* aCx, JS::Handle<JS::Value> aValue, ErrorResult&) {
        result->Reject(Promise::TryExtractNSResultFromRejectionValue(aValue),
                       __func__);
      });
  return result;
}

RefPtr<MozPromise<
    std::tuple<Maybe<IdentityProviderWellKnown>, IdentityProviderAPIConfig>,
    nsresult, true>>
IdentityNetworkHelpers::FetchConfigHelper(
    nsIURI* aConfig, nsIPrincipal* aTriggeringPrincipal,
    Maybe<IdentityProviderWellKnown> aWellKnownConfig) {
  RefPtr<MozPromise<
      std::tuple<Maybe<IdentityProviderWellKnown>, IdentityProviderAPIConfig>,
      nsresult, true>::Private>
      result = new MozPromise<std::tuple<Maybe<IdentityProviderWellKnown>,
                                         IdentityProviderAPIConfig>,
                              nsresult, true>::Private(__func__);
  nsresult rv;
  nsCOMPtr<nsICredentialChooserService> ccService =
      mozilla::components::CredentialChooserService::Service(&rv);
  if (NS_FAILED(rv) || !ccService) {
    result->Reject(rv, __func__);
    return result;
  }

  RefPtr<Promise> serviceResult;
  rv = ccService->FetchConfig(aConfig, aTriggeringPrincipal,
                              getter_AddRefs(serviceResult));
  if (NS_FAILED(rv)) {
    result->Reject(rv, __func__);
    return result;
  }
  serviceResult->AddCallbacksWithCycleCollectedArgs(
      [result, aWellKnownConfig](JSContext* aCx, JS::Handle<JS::Value> aValue,
                                 ErrorResult&) {
        IdentityProviderAPIConfig value;
        bool success = value.Init(aCx, aValue);
        if (!success) {
          JS_ClearPendingException(aCx);
          result->Reject(NS_ERROR_INVALID_ARG, __func__);
          return;
        }
        result->Resolve(std::make_tuple(aWellKnownConfig, value), __func__);
      },
      [result](JSContext* aCx, JS::Handle<JS::Value> aValue, ErrorResult&) {
        result->Reject(Promise::TryExtractNSResultFromRejectionValue(aValue),
                       __func__);
      });
  return result;
}

RefPtr<MozPromise<IdentityProviderAccountList, nsresult, true>>
IdentityNetworkHelpers::FetchAccountsHelper(
    nsIURI* aAccountsEndpoint, nsIPrincipal* aTriggeringPrincipal) {
  RefPtr<MozPromise<IdentityProviderAccountList, nsresult, true>::Private>
      result =
          new MozPromise<IdentityProviderAccountList, nsresult, true>::Private(
              __func__);
  nsresult rv;
  nsCOMPtr<nsICredentialChooserService> ccService =
      mozilla::components::CredentialChooserService::Service(&rv);
  if (NS_FAILED(rv) || !ccService) {
    result->Reject(rv, __func__);
    return result;
  }

  RefPtr<Promise> serviceResult;
  rv = ccService->FetchAccounts(aAccountsEndpoint, aTriggeringPrincipal,
                                getter_AddRefs(serviceResult));
  if (NS_FAILED(rv)) {
    result->Reject(rv, __func__);
    return result;
  }
  serviceResult->AddCallbacksWithCycleCollectedArgs(
      [result](JSContext* aCx, JS::Handle<JS::Value> aValue, ErrorResult&) {
        IdentityProviderAccountList value;
        bool success = value.Init(aCx, aValue);
        if (!success) {
          JS_ClearPendingException(aCx);
          result->Reject(NS_ERROR_INVALID_ARG, __func__);
          return;
        }
        result->Resolve(value, __func__);
      },
      [result](JSContext* aCx, JS::Handle<JS::Value> aValue, ErrorResult&) {
        result->Reject(Promise::TryExtractNSResultFromRejectionValue(aValue),
                       __func__);
      });
  return result;
}

RefPtr<MozPromise<IdentityProviderToken, nsresult, true>>
IdentityNetworkHelpers::FetchTokenHelper(nsIURI* aAccountsEndpoint,
                                         const nsCString& aBody,
                                         nsIPrincipal* aTriggeringPrincipal) {
  RefPtr<MozPromise<IdentityProviderToken, nsresult, true>::Private> result =
      new MozPromise<IdentityProviderToken, nsresult, true>::Private(__func__);
  nsresult rv;
  nsCOMPtr<nsICredentialChooserService> ccService =
      mozilla::components::CredentialChooserService::Service(&rv);
  if (NS_FAILED(rv) || !ccService) {
    result->Reject(rv, __func__);
    return result;
  }

  RefPtr<Promise> serviceResult;
  rv = ccService->FetchToken(aAccountsEndpoint, aBody.get(),
                             aTriggeringPrincipal,
                             getter_AddRefs(serviceResult));
  if (NS_FAILED(rv)) {
    result->Reject(rv, __func__);
    return result;
  }
  serviceResult->AddCallbacksWithCycleCollectedArgs(
      [result](JSContext* aCx, JS::Handle<JS::Value> aValue, ErrorResult&) {
        IdentityProviderToken value;
        bool success = value.Init(aCx, aValue);
        if (!success) {
          JS_ClearPendingException(aCx);
          result->Reject(NS_ERROR_INVALID_ARG, __func__);
          return;
        }
        result->Resolve(value, __func__);
      },
      [result](JSContext* aCx, JS::Handle<JS::Value> aValue, ErrorResult&) {
        JS_ClearPendingException(aCx);
        result->Reject(Promise::TryExtractNSResultFromRejectionValue(aValue),
                       __func__);
      });
  return result;
}

RefPtr<MozPromise<DisconnectedAccount, nsresult, true>>
IdentityNetworkHelpers::FetchDisconnectHelper(
    nsIURI* aAccountsEndpoint, const nsCString& aBody,
    nsIPrincipal* aTriggeringPrincipal) {
  RefPtr<MozPromise<DisconnectedAccount, nsresult, true>::Private> result =
      new MozPromise<DisconnectedAccount, nsresult, true>::Private(__func__);
  nsresult rv;
  nsCOMPtr<nsICredentialChooserService> ccService =
      mozilla::components::CredentialChooserService::Service(&rv);
  if (NS_FAILED(rv) || !ccService) {
    result->Reject(rv, __func__);
    return result;
  }

  RefPtr<Promise> serviceResult;
  rv = ccService->FetchToken(aAccountsEndpoint, aBody.get(),
                             aTriggeringPrincipal,
                             getter_AddRefs(serviceResult));
  if (NS_FAILED(rv)) {
    result->Reject(rv, __func__);
    return result;
  }
  serviceResult->AddCallbacksWithCycleCollectedArgs(
      [result](JSContext* aCx, JS::Handle<JS::Value> aValue, ErrorResult&) {
        DisconnectedAccount value;
        bool success = value.Init(aCx, aValue);
        if (!success) {
          JS_ClearPendingException(aCx);
          result->Reject(NS_ERROR_INVALID_ARG, __func__);
          return;
        }
        result->Resolve(value, __func__);
      },
      [result](JSContext* aCx, JS::Handle<JS::Value> aValue, ErrorResult&) {
        JS_ClearPendingException(aCx);
        result->Reject(Promise::TryExtractNSResultFromRejectionValue(aValue),
                       __func__);
      });
  return result;
}

}  // namespace mozilla::dom
