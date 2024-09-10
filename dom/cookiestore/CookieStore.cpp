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
#include "mozilla/StorageAccess.h"
#include "nsICookie.h"
#include "nsIGlobalObject.h"
#include "nsIPrincipal.h"
#include "nsReadableUtils.h"
#include "nsSandboxFlags.h"

using namespace mozilla::net;

namespace mozilla::dom {

namespace {

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

  if (aName.Length() + aValue.Length() > 1024) {
    aPromise->MaybeRejectWithTypeError(
        "Cookie name and value size cannot be greater than 1024 bytes");
    return false;
  }

  return true;
}

bool ValidateCookieDomain(const nsAString& aHost, const nsAString& aDomain,
                          Promise* aPromise) {
  MOZ_ASSERT(aPromise);

  if (aDomain.IsEmpty()) {
    return true;
  }

  if (aDomain[0] == '.') {
    aPromise->MaybeRejectWithTypeError("Cookie domain cannot start with '.'");
    return false;
  }

  if (aHost != aDomain) {
    if ((aHost.Length() < aDomain.Length() + 1) ||
        !StringEndsWith(aHost, aDomain) ||
        aHost[aHost.Length() - aDomain.Length() - 1] != '.') {
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
bool ValidateCookieNamePrefix(const nsAString& aName,
                              const nsAString& aOptionDomain,
                              const nsAString& aPath, Promise* aPromise) {
  MOZ_ASSERT(aPromise);

  if (!StringBeginsWith(aName, u"__Host-"_ns,
                        nsCaseInsensitiveStringComparator)) {
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

void CookieDataToItem(const CookieData& aData, CookieListItem* aItem) {
  aItem->mName.Construct(aData.name());
  aItem->mValue.Construct(aData.value());
}

void CookieDataToList(const nsTArray<CookieData>& aData,
                      nsTArray<CookieListItem>& aResult) {
  for (const CookieData& data : aData) {
    CookieListItem* item = aResult.AppendElement();
    CookieDataToItem(data, item);
  }
}

void ResolvePromiseAsync(Promise* aPromise) {
  MOZ_ASSERT(aPromise);

  NS_DispatchToCurrentThread(NS_NewRunnableFunction(
      __func__,
      [promise = RefPtr(aPromise)] { promise->MaybeResolveWithUndefined(); }));
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
  mNotifier = CookieStoreNotifier::Create(this);

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

        nsAutoCString baseDomainUtf8;
        nsresult rv =
            net::CookieCommons::GetBaseDomain(cookiePrincipal, baseDomainUtf8);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          promise->MaybeRejectWithNotAllowedError("Permission denied");
          return;
        }

        NS_ConvertUTF8toUTF16 baseDomain(baseDomainUtf8);

        if (!ValidateCookieDomain(baseDomain, aOptions.mDomain, promise)) {
          return;
        }

        nsString path;
        if (!ValidateCookiePath(aOptions.mPath, path, promise)) {
          return;
        }

        if (!ValidateCookieNamePrefix(aOptions.mName, aOptions.mDomain, path,
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
        rv = nsID::GenerateUUIDInPlace(operationID);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          promise->MaybeReject(NS_ERROR_UNEXPECTED);
          return;
        }

        self->mNotificationWatcher->ResolvePromiseWhenNotified(operationID,
                                                               promise);

        RefPtr<CookieStoreChild::SetRequestPromise> ipcPromise =
            self->mActor->SendSetRequest(
                aOptions.mDomain.IsEmpty() ? nsString(baseDomain)
                                           : nsString(aOptions.mDomain),
                cookiePrincipal->OriginAttributesRef(),
                nsString(aOptions.mName), nsString(aOptions.mValue),
                // If expires is not set, it's a session cookie.
                aOptions.mExpires.IsNull(),
                aOptions.mExpires.IsNull()
                    ? INT64_MAX
                    : static_cast<int64_t>(aOptions.mExpires.Value() / 1000),
                path, SameSiteToConst(aOptions.mSameSite),
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
              if (!aResult.ResolveValue()) {
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
        nsAutoCString baseDomainUtf8;
        nsresult rv =
            net::CookieCommons::GetBaseDomain(cookiePrincipal, baseDomainUtf8);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          promise->MaybeRejectWithNotAllowedError("Permission denied");
          return;
        }

        NS_ConvertUTF8toUTF16 baseDomain(baseDomainUtf8);
        if (!ValidateCookieDomain(baseDomain, aOptions.mDomain, promise)) {
          return;
        }

        nsString path;
        if (!ValidateCookiePath(aOptions.mPath, path, promise)) {
          return;
        }

        if (!ValidateCookieNamePrefix(aOptions.mName, aOptions.mDomain, path,
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
        rv = nsID::GenerateUUIDInPlace(operationID);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          promise->MaybeReject(NS_ERROR_UNEXPECTED);
          return;
        }

        self->mNotificationWatcher->ResolvePromiseWhenNotified(operationID,
                                                               promise);

        RefPtr<CookieStoreChild::DeleteRequestPromise> ipcPromise =
            self->mActor->SendDeleteRequest(
                aOptions.mDomain.IsEmpty() ? nsString(baseDomain)
                                           : nsString(aOptions.mDomain),
                cookiePrincipal->OriginAttributesRef(),
                nsString(aOptions.mName), path, aOptions.mPartitioned,
                operationID);
        if (NS_WARN_IF(!ipcPromise)) {
          promise->MaybeResolveWithUndefined();
          return;
        }

        ipcPromise->Then(
            NS_GetCurrentThread(), __func__,
            [promise = RefPtr<dom::Promise>(promise), self = RefPtr(self),
             operationID](const CookieStoreChild::DeleteRequestPromise::
                              ResolveOrRejectValue& aResult) {
              MOZ_ASSERT(aResult.IsResolve());
              if (!aResult.ResolveValue()) {
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
  switch (CookieCommons::CheckGlobalAndRetrieveCookiePrincipals(
      MaybeGetDocument(), getter_AddRefs(cookiePrincipal), nullptr)) {
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
       cookiePrincipal = RefPtr(cookiePrincipal.get()), aOnlyTheFirstMatch]() {
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
            MOZ_ASSERT(window);

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

        RefPtr<CookieStoreChild::GetRequestPromise> ipcPromise =
            self->mActor->SendGetRequest(NS_ConvertUTF8toUTF16(baseDomain),
                                         cookiePrincipal->OriginAttributesRef(),
                                         aOptions.mName.WasPassed(),
                                         nsString(name), path,
                                         aOnlyTheFirstMatch);
        if (NS_WARN_IF(!ipcPromise)) {
          promise->MaybeResolveWithUndefined();
          return;
        }

        ipcPromise->Then(
            NS_GetCurrentThread(), __func__,
            [promise = RefPtr<dom::Promise>(promise), aOnlyTheFirstMatch](
                const CookieStoreChild::GetRequestPromise::ResolveOrRejectValue&
                    aResult) {
              nsTArray<CookieListItem> list;
              MOZ_ASSERT(aResult.IsResolve());

              CookieDataToList(aResult.ResolveValue(), list);

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

}  // namespace mozilla::dom
