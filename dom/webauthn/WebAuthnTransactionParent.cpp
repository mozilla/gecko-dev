/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WebAuthnTransactionParent.h"
#include "mozilla/ipc/PBackgroundParent.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/StaticPrefs_security.h"

#include "nsThreadUtils.h"
#include "WebAuthnArgs.h"

namespace mozilla::dom {

void WebAuthnTransactionParent::CompleteTransaction() {
  if (mTransactionId.isSome()) {
    if (mRegisterPromiseRequest.Exists()) {
      mRegisterPromiseRequest.Complete();
    }
    if (mSignPromiseRequest.Exists()) {
      mSignPromiseRequest.Complete();
    }
    if (mWebAuthnService) {
      // We have to do this to work around Bug 1864526.
      mWebAuthnService->Cancel(mTransactionId.ref());
    }
    mTransactionId.reset();
  }
}

void WebAuthnTransactionParent::DisconnectTransaction() {
  mTransactionId.reset();
  mRegisterPromiseRequest.DisconnectIfExists();
  mSignPromiseRequest.DisconnectIfExists();
  if (mWebAuthnService) {
    mWebAuthnService->Reset();
  }
}

mozilla::ipc::IPCResult WebAuthnTransactionParent::RecvRequestRegister(
    const WebAuthnMakeCredentialInfo& aTransactionInfo,
    RequestRegisterResolver&& aResolver) {
  MOZ_ASSERT(NS_IsMainThread());

  if (!mWebAuthnService) {
    mWebAuthnService = do_GetService("@mozilla.org/webauthn/service;1");
    if (!mWebAuthnService) {
      return IPC_FAIL_NO_REASON(this);
    }
  }

  // If there's an ongoing transaction, abort it.
  if (mTransactionId.isSome()) {
    DisconnectTransaction();
  }
  uint64_t aTransactionId = NextId();
  mTransactionId = Some(aTransactionId);

  RefPtr<WebAuthnRegisterPromiseHolder> promiseHolder =
      new WebAuthnRegisterPromiseHolder(GetCurrentSerialEventTarget());

  RefPtr<WebAuthnRegisterPromise> promise = promiseHolder->Ensure();
  promise
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [self = RefPtr{this},
           inputClientData = aTransactionInfo.ClientDataJSON(),
           resolver = std::move(aResolver)](
              const WebAuthnRegisterPromise::ResolveOrRejectValue& aValue) {
            self->CompleteTransaction();

            if (aValue.IsReject()) {
              resolver(aValue.RejectValue());
              return;
            }

            auto rejectWithNotAllowed = MakeScopeExit(
                [&]() { resolver(NS_ERROR_DOM_NOT_ALLOWED_ERR); });

            RefPtr<nsIWebAuthnRegisterResult> registerResult =
                aValue.ResolveValue();

            nsCString clientData;
            nsresult rv = registerResult->GetClientDataJSON(clientData);
            if (rv == NS_ERROR_NOT_AVAILABLE) {
              clientData = inputClientData;
            } else if (NS_FAILED(rv)) {
              return;
            }

            nsTArray<uint8_t> attObj;
            rv = registerResult->GetAttestationObject(attObj);
            if (NS_WARN_IF(NS_FAILED(rv))) {
              return;
            }

            nsTArray<uint8_t> credentialId;
            rv = registerResult->GetCredentialId(credentialId);
            if (NS_WARN_IF(NS_FAILED(rv))) {
              return;
            }

            nsTArray<nsString> transports;
            rv = registerResult->GetTransports(transports);
            if (NS_WARN_IF(NS_FAILED(rv))) {
              return;
            }

            Maybe<nsString> authenticatorAttachment;
            nsString maybeAuthenticatorAttachment;
            rv = registerResult->GetAuthenticatorAttachment(
                maybeAuthenticatorAttachment);
            if (rv != NS_ERROR_NOT_AVAILABLE) {
              if (NS_WARN_IF(NS_FAILED(rv))) {
                return;
              }
              authenticatorAttachment = Some(maybeAuthenticatorAttachment);
            }

            nsTArray<WebAuthnExtensionResult> extensions;
            bool credPropsRk;
            rv = registerResult->GetCredPropsRk(&credPropsRk);
            if (rv != NS_ERROR_NOT_AVAILABLE) {
              if (NS_WARN_IF(NS_FAILED(rv))) {
                return;
              }
              extensions.AppendElement(
                  WebAuthnExtensionResultCredProps(credPropsRk));
            }

            bool hmacCreateSecret;
            rv = registerResult->GetHmacCreateSecret(&hmacCreateSecret);
            if (rv != NS_ERROR_NOT_AVAILABLE) {
              if (NS_WARN_IF(NS_FAILED(rv))) {
                return;
              }
              extensions.AppendElement(
                  WebAuthnExtensionResultHmacSecret(hmacCreateSecret));
            }

            {
              Maybe<bool> prfEnabledMaybe = Nothing();
              Maybe<WebAuthnExtensionPrfValues> prfResults = Nothing();

              bool prfEnabled;
              rv = registerResult->GetPrfEnabled(&prfEnabled);
              if (rv != NS_ERROR_NOT_AVAILABLE) {
                if (NS_WARN_IF(NS_FAILED(rv))) {
                  return;
                }
                prfEnabledMaybe = Some(prfEnabled);
              }

              nsTArray<uint8_t> prfResultsFirst;
              rv = registerResult->GetPrfResultsFirst(prfResultsFirst);
              if (rv != NS_ERROR_NOT_AVAILABLE) {
                if (NS_WARN_IF(NS_FAILED(rv))) {
                  return;
                }

                bool prfResultsSecondMaybe = false;
                nsTArray<uint8_t> prfResultsSecond;
                rv = registerResult->GetPrfResultsSecond(prfResultsSecond);
                if (rv != NS_ERROR_NOT_AVAILABLE) {
                  if (NS_WARN_IF(NS_FAILED(rv))) {
                    return;
                  }
                  prfResultsSecondMaybe = true;
                }

                prfResults = Some(WebAuthnExtensionPrfValues(
                    prfResultsFirst, prfResultsSecondMaybe, prfResultsSecond));
              }

              if (prfEnabledMaybe.isSome() || prfResults.isSome()) {
                extensions.AppendElement(
                    WebAuthnExtensionResultPrf(prfEnabledMaybe, prfResults));
              }
            }

            WebAuthnMakeCredentialResult result(
                clientData, attObj, credentialId, transports, extensions,
                authenticatorAttachment);

            rejectWithNotAllowed.release();
            resolver(result);
          })
      ->Track(mRegisterPromiseRequest);

  uint64_t browsingContextId = aTransactionInfo.BrowsingContextId();
  RefPtr<WebAuthnRegisterArgs> args(new WebAuthnRegisterArgs(aTransactionInfo));

  nsresult rv = mWebAuthnService->MakeCredential(
      aTransactionId, browsingContextId, args, promiseHolder);
  if (NS_FAILED(rv)) {
    promiseHolder->Reject(NS_ERROR_DOM_NOT_ALLOWED_ERR);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult WebAuthnTransactionParent::RecvRequestSign(
    const WebAuthnGetAssertionInfo& aTransactionInfo,
    RequestSignResolver&& aResolver) {
  MOZ_ASSERT(NS_IsMainThread());

  if (!mWebAuthnService) {
    mWebAuthnService = do_GetService("@mozilla.org/webauthn/service;1");
    if (!mWebAuthnService) {
      return IPC_FAIL_NO_REASON(this);
    }
  }

  if (mTransactionId.isSome()) {
    DisconnectTransaction();
  }
  uint64_t aTransactionId = NextId();
  mTransactionId = Some(aTransactionId);

  RefPtr<WebAuthnSignPromiseHolder> promiseHolder =
      new WebAuthnSignPromiseHolder(GetCurrentSerialEventTarget());

  RefPtr<WebAuthnSignPromise> promise = promiseHolder->Ensure();
  promise
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [self = RefPtr{this},
           inputClientData = aTransactionInfo.ClientDataJSON(),
           resolver = std::move(aResolver)](
              const WebAuthnSignPromise::ResolveOrRejectValue& aValue) {
            self->CompleteTransaction();

            if (aValue.IsReject()) {
              resolver(aValue.RejectValue());
              return;
            }

            auto rejectWithNotAllowed = MakeScopeExit(
                [&]() { resolver(NS_ERROR_DOM_NOT_ALLOWED_ERR); });

            RefPtr<nsIWebAuthnSignResult> signResult = aValue.ResolveValue();

            nsCString clientData;
            nsresult rv = signResult->GetClientDataJSON(clientData);
            if (rv == NS_ERROR_NOT_AVAILABLE) {
              clientData = inputClientData;
            } else if (NS_FAILED(rv)) {
              return;
            }

            nsTArray<uint8_t> credentialId;
            rv = signResult->GetCredentialId(credentialId);
            if (NS_WARN_IF(NS_FAILED(rv))) {
              return;
            }

            nsTArray<uint8_t> signature;
            rv = signResult->GetSignature(signature);
            if (NS_WARN_IF(NS_FAILED(rv))) {
              return;
            }

            nsTArray<uint8_t> authenticatorData;
            rv = signResult->GetAuthenticatorData(authenticatorData);
            if (NS_WARN_IF(NS_FAILED(rv))) {
              return;
            }

            nsTArray<uint8_t> userHandle;
            Unused << signResult->GetUserHandle(userHandle);  // optional

            Maybe<nsString> authenticatorAttachment;
            nsString maybeAuthenticatorAttachment;
            rv = signResult->GetAuthenticatorAttachment(
                maybeAuthenticatorAttachment);
            if (rv != NS_ERROR_NOT_AVAILABLE) {
              if (NS_WARN_IF(NS_FAILED(rv))) {
                return;
              }
              authenticatorAttachment = Some(maybeAuthenticatorAttachment);
            }

            nsTArray<WebAuthnExtensionResult> extensions;
            bool usedAppId;
            rv = signResult->GetUsedAppId(&usedAppId);
            if (rv != NS_ERROR_NOT_AVAILABLE) {
              if (NS_FAILED(rv)) {
                return;
              }
              extensions.AppendElement(WebAuthnExtensionResultAppId(usedAppId));
            }

            {
              Maybe<WebAuthnExtensionPrfValues> prfResults;
              bool prfMaybe = false;
              rv = signResult->GetPrfMaybe(&prfMaybe);
              if (rv == NS_OK && prfMaybe) {
                nsTArray<uint8_t> prfResultsFirst;
                rv = signResult->GetPrfResultsFirst(prfResultsFirst);
                if (rv != NS_ERROR_NOT_AVAILABLE) {
                  if (NS_WARN_IF(NS_FAILED(rv))) {
                    return;
                  }

                  bool prfResultsSecondMaybe = false;
                  nsTArray<uint8_t> prfResultsSecond;
                  rv = signResult->GetPrfResultsSecond(prfResultsSecond);
                  if (rv != NS_ERROR_NOT_AVAILABLE) {
                    if (NS_WARN_IF(NS_FAILED(rv))) {
                      return;
                    }
                    prfResultsSecondMaybe = true;
                  }

                  prfResults = Some(WebAuthnExtensionPrfValues(
                      prfResultsFirst, prfResultsSecondMaybe,
                      prfResultsSecond));
                } else {
                  prfResults = Nothing();
                }

                extensions.AppendElement(
                    WebAuthnExtensionResultPrf(Nothing(), prfResults));
              }
            }

            WebAuthnGetAssertionResult result(
                clientData, credentialId, signature, authenticatorData,
                extensions, userHandle, authenticatorAttachment);

            rejectWithNotAllowed.release();
            resolver(result);
          })
      ->Track(mSignPromiseRequest);

  RefPtr<WebAuthnSignArgs> args(new WebAuthnSignArgs(aTransactionInfo));

  nsresult rv = mWebAuthnService->GetAssertion(
      aTransactionId, aTransactionInfo.BrowsingContextId(), args,
      promiseHolder);
  if (NS_FAILED(rv)) {
    promiseHolder->Reject(NS_ERROR_DOM_NOT_ALLOWED_ERR);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult WebAuthnTransactionParent::RecvRequestCancel() {
  MOZ_ASSERT(NS_IsMainThread());

  if (mTransactionId.isNothing()) {
    return IPC_OK();
  }

  DisconnectTransaction();
  return IPC_OK();
}

mozilla::ipc::IPCResult WebAuthnTransactionParent::RecvRequestIsUVPAA(
    RequestIsUVPAAResolver&& aResolver) {
  MOZ_ASSERT(NS_IsMainThread());

#ifdef MOZ_WIDGET_ANDROID
  // Try the nsIWebAuthnService. If we're configured for tests we
  // will get a result. Otherwise we expect NS_ERROR_NOT_IMPLEMENTED.
  nsCOMPtr<nsIWebAuthnService> service(
      do_GetService("@mozilla.org/webauthn/service;1"));
  bool available;
  nsresult rv = service->GetIsUVPAA(&available);
  if (NS_SUCCEEDED(rv)) {
    aResolver(available);
    return IPC_OK();
  }

  // Don't consult the platform API if resident key support is disabled.
  if (!StaticPrefs::
          security_webauthn_webauthn_enable_android_fido2_residentkey()) {
    aResolver(false);
    return IPC_OK();
  }

  // The GeckoView implementation of
  // isUserVerifiyingPlatformAuthenticatorAvailable dispatches the work to a
  // background thread and returns a MozPromise which we can ->Then to call
  // aResolver on the current thread.
  nsCOMPtr<nsISerialEventTarget> target = GetCurrentSerialEventTarget();
  auto result = java::WebAuthnTokenManager::
      WebAuthnIsUserVerifyingPlatformAuthenticatorAvailable();
  auto geckoResult = java::GeckoResult::LocalRef(std::move(result));
  MozPromise<bool, bool, false>::FromGeckoResult(geckoResult)
      ->Then(target, __func__,
             [resolver = std::move(aResolver)](
                 const MozPromise<bool, bool, false>::ResolveOrRejectValue&
                     aValue) {
               if (aValue.IsResolve()) {
                 resolver(aValue.ResolveValue());
               } else {
                 resolver(false);
               }
             });
  return IPC_OK();

#else

  nsCOMPtr<nsISerialEventTarget> target = GetCurrentSerialEventTarget();
  nsCOMPtr<nsIRunnable> runnable(NS_NewRunnableFunction(
      __func__, [target, resolver = std::move(aResolver)]() {
        bool available;
        nsCOMPtr<nsIWebAuthnService> service(
            do_GetService("@mozilla.org/webauthn/service;1"));
        nsresult rv = service->GetIsUVPAA(&available);
        if (NS_FAILED(rv)) {
          available = false;
        }
        BoolPromise::CreateAndResolve(available, __func__)
            ->Then(target, __func__,
                   [resolver](const BoolPromise::ResolveOrRejectValue& value) {
                     if (value.IsResolve()) {
                       resolver(value.ResolveValue());
                     } else {
                       resolver(false);
                     }
                   });
      }));
  NS_DispatchBackgroundTask(runnable.forget(), NS_DISPATCH_EVENT_MAY_BLOCK);
  return IPC_OK();
#endif
}

void WebAuthnTransactionParent::ActorDestroy(ActorDestroyReason aWhy) {
  // Called either by Send__delete__() in RecvDestroyMe() above, or when
  // the channel disconnects. Ensure the token manager forgets about us.
  MOZ_ASSERT(NS_IsMainThread());

  if (mTransactionId.isSome()) {
    DisconnectTransaction();
  }
}

}  // namespace mozilla::dom
