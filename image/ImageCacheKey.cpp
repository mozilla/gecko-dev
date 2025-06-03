/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ImageCacheKey.h"

#include <utility>

#include "mozilla/AntiTrackingUtils.h"
#include "mozilla/HashFunctions.h"
#include "mozilla/StorageAccess.h"
#include "mozilla/StoragePrincipalHelper.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/ServiceWorkerManager.h"
#include "mozilla/StaticPrefs_privacy.h"
#include "mozilla/StorageAccess.h"
#include "nsContentUtils.h"
#include "nsHashKeys.h"
#include "nsLayoutUtils.h"

namespace mozilla {

using namespace dom;

namespace image {

static nsIPrincipal* GetLoaderPrincipal(Document* aDocument) {
  return aDocument ? aDocument->NodePrincipal()
                   : nsContentUtils::GetSystemPrincipal();
}

static nsIPrincipal* GetPartitionPrincipal(Document* aDocument) {
  return aDocument ? aDocument->PartitionedPrincipal()
                   : nsContentUtils::GetSystemPrincipal();
}

ImageCacheKey::ImageCacheKey(nsIURI* aURI, CORSMode aCORSMode,
                             Document* aDocument)
    : mURI(aURI),
      mControlledDocument(GetSpecialCaseDocumentToken(aDocument)),
      mLoaderPrincipal(GetLoaderPrincipal(aDocument)),
      mPartitionPrincipal(GetPartitionPrincipal(aDocument)),
      mCORSMode(aCORSMode),
      mAppType(GetAppType(aDocument)) {
  MOZ_ASSERT(mLoaderPrincipal);
  MOZ_ASSERT(mPartitionPrincipal);
}

ImageCacheKey::ImageCacheKey(const ImageCacheKey& aOther) = default;
ImageCacheKey::ImageCacheKey(ImageCacheKey&& aOther) = default;

bool ImageCacheKey::operator==(const ImageCacheKey& aOther) const {
  // Don't share the image cache between a controlled document and anything
  // else.
  if (mControlledDocument != aOther.mControlledDocument) {
    return false;
  }

  if (!mPartitionPrincipal->Equals(aOther.mPartitionPrincipal)) {
    return false;
  }

  if (mCORSMode != aOther.mCORSMode) {
    return false;
  }
  // Don't share the image cache between two different appTypes
  if (mAppType != aOther.mAppType) {
    return false;
  }

  // For non-blob URIs, compare the URIs.
  bool equals = false;
  nsresult rv = mURI->Equals(aOther.mURI, &equals);
  return NS_SUCCEEDED(rv) && equals;
}

void ImageCacheKey::EnsureHash() const {
  MOZ_ASSERT(mHash.isNothing());

  // NOTE(emilio): Not adding the partition principal to the hash, since it
  // can mutate (see bug 1955775).
  nsAutoCString spec;
  Unused << mURI->GetSpec(spec);
  mHash.emplace(
      AddToHash(HashString(spec), mControlledDocument, mAppType, mCORSMode));
}

/* static */
void* ImageCacheKey::GetSpecialCaseDocumentToken(Document* aDocument) {
  // Cookie-averse documents can never have storage granted to them.  Since they
  // may not have inner windows, they would require special handling below, so
  // just bail out early here.
  if (!aDocument || aDocument->IsCookieAverse()) {
    return nullptr;
  }

  // For controlled documents, we cast the pointer into a void* to avoid
  // dereferencing it (since we only use it for comparisons).
  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  if (swm && aDocument->GetController().isSome()) {
    return aDocument;
  }

  return nullptr;
}

/* static */
nsIDocShell::AppType ImageCacheKey::GetAppType(Document* aDocument) {
  if (!aDocument) {
    return nsIDocShell::APP_TYPE_UNKNOWN;
  }

  nsCOMPtr<nsIDocShellTreeItem> dsti = aDocument->GetDocShell();
  if (!dsti) {
    return nsIDocShell::APP_TYPE_UNKNOWN;
  }

  nsCOMPtr<nsIDocShellTreeItem> root;
  dsti->GetInProcessRootTreeItem(getter_AddRefs(root));
  if (nsCOMPtr<nsIDocShell> docShell = do_QueryInterface(root)) {
    return docShell->GetAppType();
  }
  return nsIDocShell::APP_TYPE_UNKNOWN;
}

}  // namespace image
}  // namespace mozilla
