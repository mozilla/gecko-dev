/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "URLMainThread.h"

#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/Blob.h"
#include "mozilla/dom/BlobURLProtocolHandler.h"
#include "mozilla/Unused.h"
#include "nsContentUtils.h"
#include "nsNetUtil.h"
#include "nsThreadUtils.h"
#include "mozilla/dom/Document.h"

namespace mozilla::dom {

/* static */
void URLMainThread::CreateObjectURL(const GlobalObject& aGlobal,
                                    const BlobOrMediaSource& aObj,
                                    nsACString& aResult, ErrorResult& aRv) {
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  if (NS_WARN_IF(!global)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  nsAutoString partKey;
  if (nsCOMPtr<nsPIDOMWindowInner> owner = do_QueryInterface(global)) {
    if (Document* doc = owner->GetExtantDoc()) {
      nsCOMPtr<nsICookieJarSettings> cookieJarSettings =
          doc->CookieJarSettings();

      cookieJarSettings->GetPartitionKey(partKey);
    }
  }

  nsCOMPtr<nsIPrincipal> principal =
      nsContentUtils::ObjectPrincipal(aGlobal.Get());

  if (aObj.IsBlob()) {
    aRv = BlobURLProtocolHandler::AddDataEntry(
        aObj.GetAsBlob().Impl(), principal, NS_ConvertUTF16toUTF8(partKey),
        aResult);
  } else if (aObj.IsMediaSource()) {
    aRv = BlobURLProtocolHandler::AddDataEntry(
        &aObj.GetAsMediaSource(), principal, NS_ConvertUTF16toUTF8(partKey),
        aResult);
  } else {
    MOZ_CRASH("Invalid type for a BlobOrMediaSource");
  }

  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  global->RegisterHostObjectURI(aResult);
}

/* static */
void URLMainThread::RevokeObjectURL(const GlobalObject& aGlobal,
                                    const nsACString& aURL, ErrorResult& aRv) {
  MOZ_ASSERT(NS_IsMainThread());
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  nsAutoString partKey;
  if (nsCOMPtr<nsPIDOMWindowInner> owner = do_QueryInterface(global)) {
    if (Document* doc = owner->GetExtantDoc()) {
      nsCOMPtr<nsICookieJarSettings> cookieJarSettings =
          doc->CookieJarSettings();

      cookieJarSettings->GetPartitionKey(partKey);
    }
  }

  if (BlobURLProtocolHandler::RemoveDataEntry(
          aURL, nsContentUtils::ObjectPrincipal(aGlobal.Get()),
          NS_ConvertUTF16toUTF8(partKey))) {
    global->UnregisterHostObjectURI(aURL);
  }
}

// static
bool URLMainThread::IsBoundToBlob(const GlobalObject& aGlobal,
                                  const nsACString& aURL, ErrorResult& aRv) {
  MOZ_ASSERT(NS_IsMainThread());
  return BlobURLProtocolHandler::HasDataEntryTypeBlob(aURL);
}

}  // namespace mozilla::dom
