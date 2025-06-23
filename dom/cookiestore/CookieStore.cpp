/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CookieStore.h"
#include "CookieStoreChild.h"
#include "CookieStoreNotifier.h"
#include "CookieStoreNotificationWatcherWrapper.h"

#include "mozilla/dom/Document.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/WorkerCommon.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "mozilla/net/CookieCommons.h"
#include "mozilla/net/NeckoChannelParams.h"
#include "mozilla/StorageAccess.h"
#include "nsICookie.h"
#include "nsIGlobalObject.h"
#include "nsIPrincipal.h"
#include "nsReadableUtils.h"
#include "nsSandboxFlags.h"
#include "ThirdPartyUtil.h"

using namespace mozilla::net;

namespace mozilla::dom {

namespace {

int64_t ComputeExpiry(const CookieInit& aOptions) {
  if (aOptions.mExpires.IsNull()) {  // Session cookie
    return INT64_MAX;
  }

  return CookieCommons::MaybeReduceExpiry(PR_Now() / PR_USEC_PER_MSEC,
                                          aOptions.mExpires.Value());
}

int32_t SameSiteToConst(const CookieSameSite& aSameSite) {
  switch (aSameSite) {
    case CookieSameSite::Strict:
      return nsICookie::SAMESITE_STRICT;
    case CookieSameSite::Lax:
      return nsICookie::SAMESITE_LAX;
    default:
      MOZ_ASSERT(aSameSite == CookieSameSite::None);
      return nsICookie::SAMESITE_NONE;
  }
}

bool ValidateCookieNameOrValue(const nsAString& aStr) {
  if (aStr.Length() > 0 && (aStr.First() == 0x20 || aStr.Last() == 0x20)) {
    return false;
  }
  for (auto iter = aStr.BeginReading(), end = aStr.EndReading(); iter < end;
       ++iter) {
    if (*iter == 0x3B || *iter == 0x7F || (*iter <= 0x1F && *iter != 0x09)) {
      return false;
    }
  }
  return true;
}

bool ValidateCookieNameAndValue(const nsAString& aName, const nsAString& aValue,
                                Promise* aPromise) {
  MOZ_ASSERT(aPromise);

  if (!ValidateCookieNameOrValue(aName)) {
    aPromise->MaybeRejectWithTypeError("Cookie name contains invalid chars");
    return false;
  }

  if (!ValidateCookieNameOrValue(aValue)) {
    aPromise->MaybeRejectWithTypeError("Cookie value contains invalid chars");
    return false;
  }

  if (aName.IsEmpty() && aValue.Contains('=')) {
    aPromise->MaybeRejectWithTypeError(
        "Cookie value cannot contain '=' if the name is empty");
    return false;
  }

  if (aName.IsEmpty() && aValue.IsEmpty()) {
    aPromise->MaybeRejectWithTypeError(
        "Cookie name and value both cannot be empty");
    return false;
  }

  if (aName.Length() + aValue.Length() > 4096) {
    aPromise->MaybeRejectWithTypeError(
        "Cookie name and value size cannot be greater than 4096 bytes");
    return false;
  }

  return true;
}

bool HasSecurePrefix(const nsAString& aString) {
  return StringBeginsWith(aString, u"__Secure-"_ns,
                          nsCaseInsensitiveStringComparator);
}

bool HasHostPrefix(const nsAString& aString) {
  return StringBeginsWith(aString, u"__Host-"_ns,
                          nsCaseInsensitiveStringComparator);
}

bool ValidateCookieDomain(nsIPrincipal* aPrincipal, const nsAString& aName,
                          const nsAString& aDomain, Promise* aPromise) {
  MOZ_ASSERT(aPromise);

  if (aDomain.IsEmpty()) {
    return true;
  }

  nsAutoCString utf8Domain;
  nsresult rv =
      nsContentUtils::GetHostOrIPv6WithBrackets(aPrincipal, utf8Domain);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aPromise->MaybeRejectWithNotAllowedError("Permission denied");
    return false;
  }

  // If the name has a __Host- prefix, then aDomain must be empty.
  if (HasHostPrefix(aName) && !aDomain.IsEmpty()) {
    aPromise->MaybeRejectWithTypeError(
        "Cookie domain is not allowed for cookies with a __Host- prefix");
    return false;
  }

  if (aDomain[0] == '.') {
    aPromise->MaybeRejectWithTypeError("Cookie domain cannot start with '.'");
    return false;
  }

  NS_ConvertUTF8toUTF16 host(utf8Domain);
  if (host != aDomain) {
    if ((host.Length() < aDomain.Length() + 1) ||
        !StringEndsWith(host, aDomain) ||
        host[host.Length() - aDomain.Length() - 1] != '.') {
      aPromise->MaybeRejectWithTypeError(
          "Cookie domain must domain-match current host");
      return false;
    }
  }

  if (aDomain.Length() > 1024) {
    aPromise->MaybeRejectWithTypeError(
        "Cookie domain size cannot be greater than 1024 bytes");
    return false;
  }

  return true;
}

bool ValidateCookiePath(const nsAString& aPath, nsAString& retPath,
                        Promise* aPromise) {
  MOZ_ASSERT(aPromise);

  if (!aPath.IsEmpty() && aPath[0] != '/') {
    aPromise->MaybeRejectWithTypeError("Cookie path must start with '/'");
    return false;
  }

  nsString path(aPath);
  if (path.IsEmpty() || path[path.Length() - 1] != '/') {
    path.Append('/');
  }

  if (path.Length() > 1024) {
    aPromise->MaybeRejectWithTypeError(
        "Cookie domain size cannot be greater than 1024 bytes");
    return false;
  }

  retPath.Assign(path);
  return true;
}

// Reject cookies whose name starts with the magic prefixes from
// https://datatracker.ietf.org/doc/html/draft-ietf-httpbis-rfc6265bis
// if they do not meet the criteria required by the prefix.
bool ValidateCookieNamePrefix(const nsAString& aName, const nsAString& aValue,
                              const nsAString& aOptionDomain,
                              const nsAString& aPath, Promise* aPromise) {
  MOZ_ASSERT(aPromise);

  if (aName.IsEmpty() && (HasHostPrefix(aValue) || HasSecurePrefix(aValue))) {
    aPromise->MaybeRejectWithTypeError(
        "Nameless cookies should not begin with special prefixes");
    return false;
  }

  if (!HasHostPrefix(aName)) {
    return true;
  }

  if (!aOptionDomain.IsEmpty()) {
    aPromise->MaybeRejectWithTypeError(
        "Cookie domain cannot be used when the cookie name uses special "
        "prefixes");
    return false;
  }

  if (!aPath.EqualsLiteral("/")) {
    aPromise->MaybeRejectWithTypeError(
        "Cookie path cannot be different than '/' when the cookie name uses "
        "special prefixes");
    return false;
  }

  return true;
}

void CookieStructToList(const nsTArray<CookieStruct>& aData,
                        nsTArray<CookieListItem>& aResult) {
  for (const CookieStruct& data : aData) {
    CookieListItem* item = aResult.AppendElement();
    CookieStore::CookieStructToItem(data, item);
  }
}

void ResolvePromiseAsync(Promise* aPromise) {
  MOZ_ASSERT(aPromise);

  NS_DispatchToCurrentThread(NS_NewRunnableFunction(
      __func__,
      [promise = RefPtr(aPromise)] { promise->MaybeResolveWithUndefined(); }));
}

bool GetContextAttributes(CookieStore* aCookieStore, bool* aThirdPartyContext,
                          bool* aPartitionForeign, bool* aUsingStorageAccess,
                          bool* aIsOn3PCBExceptionList, Promise* aPromise) {
  MOZ_ASSERT(aCookieStore);
  MOZ_ASSERT(aThirdPartyContext);
  MOZ_ASSERT(aPartitionForeign);
  MOZ_ASSERT(aUsingStorageAccess);
  MOZ_ASSERT(aIsOn3PCBExceptionList);
  MOZ_ASSERT(aPromise);

  if (NS_IsMainThread()) {
    nsCOMPtr<nsPIDOMWindowInner> window = aCookieStore->GetOwnerWindow();
    if (NS_WARN_IF(!window)) {
      aPromise->MaybeReject(NS_ERROR_DOM_SECURITY_ERR);
      return false;
    }

    ThirdPartyUtil* thirdPartyUtil = ThirdPartyUtil::GetInstance();
    if (thirdPartyUtil) {
      Unused << thirdPartyUtil->IsThirdPartyWindow(window->GetOuterWindow(),
                                                   nullptr, aThirdPartyContext);
    }

    nsCOMPtr<Document> document = window->GetExtantDoc();
    if (NS_WARN_IF(!document)) {
      aPromise->MaybeReject(NS_ERROR_DOM_SECURITY_ERR);
      return false;
    }

    *aPartitionForeign = document->CookieJarSettings()->GetPartitionForeign();
    *aUsingStorageAccess = document->UsingStorageAccess();
    *aIsOn3PCBExceptionList = document->IsOn3PCBExceptionList();
    return true;
  }

  WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
  MOZ_ASSERT(workerPrivate);

  *aThirdPartyContext = workerPrivate->IsThirdPartyContext();
  *aPartitionForeign =
      workerPrivate->CookieJarSettings()->GetPartitionForeign();
  *aUsingStorageAccess = workerPrivate->UsingStorageAccess();
  *aIsOn3PCBExceptionList = workerPrivate->IsOn3PCBExceptionList();
  return true;
}

}  // namespace

NS_IMPL_CYCLE_COLLECTION_CLASS(CookieStore)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(CookieStore,
                                                DOMEventTargetHelper)
  tmp->Shutdown();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(CookieStore,
                                                  DOMEventTargetHelper)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(CookieStore)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(CookieStore, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(CookieStore, DOMEventTargetHelper)

// static
already_AddRefed<CookieStore> CookieStore::Create(nsIGlobalObject* aGlobal) {
  return do_AddRef(new CookieStore(aGlobal));
}

CookieStore::CookieStore(nsIGlobalObject* aGlobal)
    : DOMEventTargetHelper(aGlobal) {
  if (NS_IsMainThread()) {
    mNotifier = CookieStoreNotifier::Create(this);
  }

  // This must be created _after_ CookieStoreNotifier because we rely on the
  // notification order.
  mNotificationWatcher = CookieStoreNotificationWatcherWrapper::Create(this);
}

CookieStore::~CookieStore() { Shutdown(); }

JSObject* CookieStore::WrapObject(JSContext* aCx,
                                  JS::Handle<JSObject*> aGivenProto) {
  return CookieStore_Binding::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<Promise> CookieStore::Get(const nsAString& aName,
                                           ErrorResult& aRv) {
  CookieStoreGetOptions options;
  options.mName.Construct(aName);
  return Get(options, aRv);
}

already_AddRefed<Promise> CookieStore::Get(
    const CookieStoreGetOptions& aOptions, ErrorResult& aRv) {
  if (!aOptions.mName.WasPassed() && !aOptions.mUrl.WasPassed()) {
    aRv.ThrowTypeError("CookieStoreGetOptions must not be empty");
    return nullptr;
  }

  return GetInternal(aOptions, true, aRv);
}

already_AddRefed<Promise> CookieStore::GetAll(const nsAString& aName,
                                              ErrorResult& aRv) {
  CookieStoreGetOptions options;
  options.mName.Construct(aName);
  return GetAll(options, aRv);
}

already_AddRefed<Promise> CookieStore::GetAll(
    const CookieStoreGetOptions& aOptions, ErrorResult& aRv) {
  return GetInternal(aOptions, false, aRv);
}

already_AddRefed<Promise> CookieStore::Set(const nsAString& aName,
                                           const nsAString& aValue,
                                           ErrorResult& aRv) {
  CookieInit init;
  init.mName = aName;
  init.mValue = aValue;
  return Set(init, aRv);
}

already_AddRefed<Promise> CookieStore::Set(const CookieInit& aOptions,
                                           ErrorResult& aRv) {
  RefPtr<Promise> promise = Promise::Create(GetOwnerGlobal(), aRv);
  if (NS_WARN_IF(!promise)) {
    return nullptr;
  }

  nsCOMPtr<nsIPrincipal> cookiePrincipal;
  switch (CookieCommons::CheckGlobalAndRetrieveCookiePrincipals(
      MaybeGetDocument(), getter_AddRefs(cookiePrincipal), nullptr)) {
    case CookieCommons::SecurityChecksResult::eSandboxedError:
      [[fallthrough]];

    case CookieCommons::SecurityChecksResult::eSecurityError:
      aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
      return nullptr;

    case CookieCommons::SecurityChecksResult::eDoNotContinue:
      ResolvePromiseAsync(promise);
      return promise.forget();

    case CookieCommons::SecurityChecksResult::eContinue:
      MOZ_ASSERT(cookiePrincipal);
      break;
  }

  NS_DispatchToCurrentThread(NS_NewRunnableFunction(
      __func__, [self = RefPtr(this), promise = RefPtr(promise), aOptions,
                 cookiePrincipal = RefPtr(cookiePrincipal.get())]() {
        if (!ValidateCookieNameAndValue(aOptions.mName, aOptions.mValue,
                                        promise)) {
          return;
        }

        if (!ValidateCookieDomain(cookiePrincipal, aOptions.mName,
                                  aOptions.mDomain, promise)) {
          return;
        }

        nsString path;
        if (!ValidateCookiePath(aOptions.mPath, path, promise)) {
          return;
        }

        if (!ValidateCookieNamePrefix(aOptions.mName, aOptions.mValue,
                                      aOptions.mDomain, path, promise)) {
          return;
        }

        bool thirdPartyContext = true;
        bool partitionForeign = true;
        bool usingStorageAccess = false;
        bool isOn3PCBExceptionList = false;

        if (!GetContextAttributes(self, &thirdPartyContext, &partitionForeign,
                                  &usingStorageAccess, &isOn3PCBExceptionList,
                                  promise)) {
          return;
        }

        if (!self->MaybeCreateActor()) {
          promise->MaybeRejectWithNotAllowedError("Permission denied");
          return;
        }

        if (!self->mNotificationWatcher) {
          promise->MaybeReject(NS_ERROR_UNEXPECTED);
          return;
        }

        nsID operationID;
        nsresult rv = nsID::GenerateUUIDInPlace(operationID);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          promise->MaybeReject(NS_ERROR_UNEXPECTED);
          return;
        }

        self->mNotificationWatcher->ResolvePromiseWhenNotified(operationID,
                                                               promise);

        nsCOMPtr<nsIURI> cookieURI = cookiePrincipal->GetURI();
        RefPtr<CookieStoreChild::SetRequestPromise> ipcPromise =
            self->mActor->SendSetRequest(
                mozilla::WrapNotNull(cookieURI.get()),
                cookiePrincipal->OriginAttributesRef(), thirdPartyContext,
                partitionForeign, usingStorageAccess, isOn3PCBExceptionList,
                nsString(aOptions.mName), nsString(aOptions.mValue),
                // If expires is not set, it's a session cookie.
                aOptions.mExpires.IsNull(), ComputeExpiry(aOptions),
                aOptions.mDomain, path, SameSiteToConst(aOptions.mSameSite),
                aOptions.mPartitioned, operationID);
        if (NS_WARN_IF(!ipcPromise)) {
          promise->MaybeResolveWithUndefined();
          return;
        }

        ipcPromise->Then(
            NS_GetCurrentThread(), __func__,
            [promise = RefPtr<dom::Promise>(promise), self = RefPtr(self),
             operationID](
                const CookieStoreChild::SetRequestPromise::ResolveOrRejectValue&
                    aResult) {
              if (!aResult.IsResolve() || !aResult.ResolveValue()) {
                self->mNotificationWatcher->ForgetOperationID(operationID);
                promise->MaybeResolveWithUndefined();
              }
            });
      }));

  return promise.forget();
}

already_AddRefed<Promise> CookieStore::Delete(const nsAString& aName,
                                              ErrorResult& aRv) {
  CookieStoreDeleteOptions options;
  options.mName = aName;
  return Delete(options, aRv);
}

already_AddRefed<Promise> CookieStore::Delete(
    const CookieStoreDeleteOptions& aOptions, ErrorResult& aRv) {
  RefPtr<Promise> promise = Promise::Create(GetOwnerGlobal(), aRv);
  if (NS_WARN_IF(!promise)) {
    return nullptr;
  }

  nsCOMPtr<nsIPrincipal> cookiePrincipal;
  switch (CookieCommons::CheckGlobalAndRetrieveCookiePrincipals(
      MaybeGetDocument(), getter_AddRefs(cookiePrincipal), nullptr)) {
    case CookieCommons::SecurityChecksResult::eSandboxedError:
      [[fallthrough]];

    case CookieCommons::SecurityChecksResult::eSecurityError:
      aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
      return nullptr;

    case CookieCommons::SecurityChecksResult::eDoNotContinue:
      ResolvePromiseAsync(promise);
      return promise.forget();

    case CookieCommons::SecurityChecksResult::eContinue:
      MOZ_ASSERT(cookiePrincipal);
      break;
  }

  NS_DispatchToCurrentThread(NS_NewRunnableFunction(
      __func__, [self = RefPtr(this), promise = RefPtr(promise), aOptions,
                 cookiePrincipal = RefPtr(cookiePrincipal.get())]() {
        if (!ValidateCookieDomain(cookiePrincipal, aOptions.mName,
                                  aOptions.mDomain, promise)) {
          return;
        }

        nsString path;
        if (!ValidateCookiePath(aOptions.mPath, path, promise)) {
          return;
        }

        if (!ValidateCookieNamePrefix(aOptions.mName, u""_ns, aOptions.mDomain,
                                      path, promise)) {
          return;
        }

        bool thirdPartyContext = true;
        bool partitionForeign = true;
        bool usingStorageAccess = false;
        bool isOn3PCBExceptionList = false;

        if (!GetContextAttributes(self, &thirdPartyContext, &partitionForeign,
                                  &usingStorageAccess, &isOn3PCBExceptionList,
                                  promise)) {
          return;
        }

        if (!self->MaybeCreateActor()) {
          promise->MaybeRejectWithNotAllowedError("Permission denied");
          return;
        }

        if (!self->mNotificationWatcher) {
          promise->MaybeReject(NS_ERROR_UNEXPECTED);
          return;
        }

        nsID operationID;
        nsresult rv = nsID::GenerateUUIDInPlace(operationID);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          promise->MaybeReject(NS_ERROR_UNEXPECTED);
          return;
        }

        self->mNotificationWatcher->ResolvePromiseWhenNotified(operationID,
                                                               promise);
        nsCOMPtr<nsIURI> cookieURI = cookiePrincipal->GetURI();
        RefPtr<CookieStoreChild::DeleteRequestPromise> ipcPromise =
            self->mActor->SendDeleteRequest(
                mozilla::WrapNotNull(cookieURI.get()),
                cookiePrincipal->OriginAttributesRef(), thirdPartyContext,
                partitionForeign, usingStorageAccess, isOn3PCBExceptionList,
                nsString(aOptions.mName), nsString(aOptions.mDomain), path,
                aOptions.mPartitioned, operationID);
        if (NS_WARN_IF(!ipcPromise)) {
          promise->MaybeResolveWithUndefined();
          return;
        }

        ipcPromise->Then(
            NS_GetCurrentThread(), __func__,
            [promise = RefPtr<dom::Promise>(promise), self = RefPtr(self),
             operationID](const CookieStoreChild::DeleteRequestPromise::
                              ResolveOrRejectValue& aResult) {
              if (!aResult.IsResolve() || !aResult.ResolveValue()) {
                self->mNotificationWatcher->ForgetOperationID(operationID);
                promise->MaybeResolveWithUndefined();
              }
            });
      }));

  return promise.forget();
}

void CookieStore::Shutdown() {
  if (mActor) {
    mActor->Close();
    mActor = nullptr;
  }

  if (mNotifier) {
    mNotifier->Disentangle();
    mNotifier = nullptr;
  }
}

bool CookieStore::MaybeCreateActor() {
  if (mActor) {
    return mActor->CanSend();
  }

  mozilla::ipc::PBackgroundChild* actorChild =
      mozilla::ipc::BackgroundChild::GetOrCreateForCurrentThread();
  if (NS_WARN_IF(!actorChild)) {
    // The process is probably shutting down. Let's return a 'generic' error.
    return false;
  }

  PCookieStoreChild* actor = actorChild->SendPCookieStoreConstructor();
  if (!actor) {
    return false;
  }

  mActor = static_cast<CookieStoreChild*>(actor);

  return true;
}

already_AddRefed<Promise> CookieStore::GetInternal(
    const CookieStoreGetOptions& aOptions, bool aOnlyTheFirstMatch,
    ErrorResult& aRv) {
  RefPtr<Promise> promise = Promise::Create(GetOwnerGlobal(), aRv);
  if (NS_WARN_IF(!promise)) {
    return nullptr;
  }

  nsCOMPtr<nsIPrincipal> cookiePrincipal;
  nsCOMPtr<nsIPrincipal> partitionedCookiePrincipal;
  switch (CookieCommons::CheckGlobalAndRetrieveCookiePrincipals(
      MaybeGetDocument(), getter_AddRefs(cookiePrincipal),
      getter_AddRefs(partitionedCookiePrincipal))) {
    case CookieCommons::SecurityChecksResult::eSandboxedError:
      [[fallthrough]];

    case CookieCommons::SecurityChecksResult::eSecurityError:
      aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
      return nullptr;

    case CookieCommons::SecurityChecksResult::eDoNotContinue:
      ResolvePromiseAsync(promise);
      return promise.forget();

    case CookieCommons::SecurityChecksResult::eContinue:
      MOZ_ASSERT(cookiePrincipal);
      break;
  }

  NS_DispatchToCurrentThread(NS_NewRunnableFunction(
      __func__,
      [self = RefPtr(this), promise = RefPtr(promise), aOptions,
       cookiePrincipal = RefPtr(cookiePrincipal.get()),
       partitionedCookiePrincipal = RefPtr(partitionedCookiePrincipal.get()),
       aOnlyTheFirstMatch]() {
        nsAutoString name;
        if (aOptions.mName.WasPassed()) {
          name = aOptions.mName.Value();
        }

        nsAutoCString path;
        nsresult rv = cookiePrincipal->GetFilePath(path);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          promise->MaybeReject(NS_ERROR_DOM_SECURITY_ERR);
          return;
        }

        if (aOptions.mUrl.WasPassed()) {
          nsString url(aOptions.mUrl.Value());

          if (NS_IsMainThread()) {
            nsCOMPtr<nsPIDOMWindowInner> window = self->GetOwnerWindow();
            if (NS_WARN_IF(!window)) {
              promise->MaybeReject(NS_ERROR_DOM_SECURITY_ERR);
              return;
            }

            nsCOMPtr<Document> document = window->GetExtantDoc();
            if (NS_WARN_IF(!document)) {
              promise->MaybeReject(NS_ERROR_DOM_SECURITY_ERR);
              return;
            }

            nsIURI* creationURI = document->GetOriginalURI();
            if (NS_WARN_IF(!creationURI)) {
              promise->MaybeReject(NS_ERROR_DOM_SECURITY_ERR);
              return;
            }

            nsCOMPtr<nsIURI> resolvedURI;
            rv = NS_NewURI(getter_AddRefs(resolvedURI), url, nullptr,
                           creationURI);
            if (NS_WARN_IF(NS_FAILED(rv))) {
              promise->MaybeRejectWithTypeError<MSG_INVALID_URL>(
                  NS_ConvertUTF16toUTF8(url));
              return;
            }

            bool equal = false;
            if (!resolvedURI ||
                NS_WARN_IF(NS_FAILED(
                    resolvedURI->EqualsExceptRef(creationURI, &equal))) ||
                !equal) {
              promise->MaybeRejectWithTypeError<MSG_INVALID_URL>(
                  NS_ConvertUTF16toUTF8(url));
              return;
            }
          } else {
            nsCOMPtr<nsIURI> baseURI = cookiePrincipal->GetURI();
            if (NS_WARN_IF(!baseURI)) {
              promise->MaybeReject(NS_ERROR_DOM_SECURITY_ERR);
              return;
            }

            nsCOMPtr<nsIURI> resolvedURI;
            rv = NS_NewURI(getter_AddRefs(resolvedURI), url, nullptr, baseURI);
            if (NS_WARN_IF(NS_FAILED(rv))) {
              promise->MaybeRejectWithTypeError<MSG_INVALID_URL>(
                  NS_ConvertUTF16toUTF8(url));
              return;
            }

            if (!cookiePrincipal->IsSameOrigin(resolvedURI)) {
              promise->MaybeRejectWithTypeError<MSG_INVALID_URL>(
                  NS_ConvertUTF16toUTF8(url));
              return;
            }

            rv = resolvedURI->GetFilePath(path);
            if (NS_WARN_IF(NS_FAILED(rv))) {
              promise->MaybeReject(NS_ERROR_DOM_SECURITY_ERR);
              return;
            }
          }
        }

        bool thirdPartyContext = true;
        bool partitionForeign = true;
        bool usingStorageAccess = false;
        bool isOn3PCBExceptionList = false;

        if (!GetContextAttributes(self, &thirdPartyContext, &partitionForeign,
                                  &usingStorageAccess, &isOn3PCBExceptionList,
                                  promise)) {
          return;
        }

        if (!self->MaybeCreateActor()) {
          promise->MaybeRejectWithNotAllowedError("Permission denied");
          return;
        }

        nsAutoCString baseDomain;
        rv = net::CookieCommons::GetBaseDomain(cookiePrincipal, baseDomain);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          promise->MaybeRejectWithNotAllowedError("Permission denied");
          return;
        }

        nsCOMPtr<nsIURI> cookieURI = cookiePrincipal->GetURI();
        RefPtr<CookieStoreChild::GetRequestPromise> ipcPromise =
            self->mActor->SendGetRequest(
                mozilla::WrapNotNull(cookieURI.get()),
                cookiePrincipal->OriginAttributesRef(),
                partitionedCookiePrincipal
                    ? Some(partitionedCookiePrincipal->OriginAttributesRef())
                    : Nothing(),
                thirdPartyContext, partitionForeign, usingStorageAccess,
                isOn3PCBExceptionList, aOptions.mName.WasPassed(),
                nsString(name), path, aOnlyTheFirstMatch);
        if (NS_WARN_IF(!ipcPromise)) {
          promise->MaybeResolveWithUndefined();
          return;
        }

        ipcPromise->Then(
            NS_GetCurrentThread(), __func__,
            [promise = RefPtr<dom::Promise>(promise), aOnlyTheFirstMatch](
                const CookieStoreChild::GetRequestPromise::ResolveOrRejectValue&
                    aResult) {
              if (!aResult.IsResolve()) {
                promise->MaybeResolveWithUndefined();
                return;
              }

              nsTArray<CookieListItem> list;
              CookieStructToList(aResult.ResolveValue(), list);

              if (!aOnlyTheFirstMatch) {
                promise->MaybeResolve(list);
                return;
              }

              if (list.IsEmpty()) {
                promise->MaybeResolve(JS::NullHandleValue);
                return;
              }

              promise->MaybeResolve(list[0]);
            });
      }));

  return promise.forget();
}

void CookieStore::FireDelayedDOMEvents() {
  MOZ_ASSERT(NS_IsMainThread());

  if (mNotifier) {
    mNotifier->FireDelayedDOMEvents();
  }
}

Document* CookieStore::MaybeGetDocument() const {
  if (NS_IsMainThread()) {
    nsCOMPtr<nsPIDOMWindowInner> window = GetOwnerWindow();
    MOZ_ASSERT(window);
    return window->GetExtantDoc();
  }

  return nullptr;
}

// static
void CookieStore::CookieStructToItem(const CookieStruct& aData,
                                     CookieListItem* aItem) {
  aItem->mName.Construct(aData.name());
  aItem->mValue.Construct(aData.value());
  aItem->mPath.Construct(aData.path());

  if (aData.host().IsEmpty() || aData.host()[0] != '.') {
    aItem->mDomain.Construct(VoidCString());
  } else {
    aItem->mDomain.Construct(nsDependentCSubstring(aData.host(), 1));
  }

  if (!aData.isSession()) {
    aItem->mExpires.Construct(aData.expiry());
  } else {
    aItem->mExpires.Construct(nullptr);
  }

  aItem->mSecure.Construct(aData.isSecure());

  CookieSameSite sameSite = CookieSameSite::None;
  switch (aData.sameSite()) {
    case nsICookie::SAMESITE_STRICT:
      sameSite = CookieSameSite::Strict;
      break;

    case nsICookie::SAMESITE_LAX:
      sameSite = CookieSameSite::Lax;
      break;

    default:
      // FIXME: lax by default?
      break;
  }

  aItem->mSameSite.Construct(sameSite);
  aItem->mPartitioned.Construct(aData.isPartitioned());
}

}  // namespace mozilla::dom
