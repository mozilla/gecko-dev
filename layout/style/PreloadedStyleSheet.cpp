/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* a CSS style sheet returned from nsIStyleSheetService.preloadSheet */

#include "PreloadedStyleSheet.h"

#include "mozilla/css/Loader.h"
#include "mozilla/dom/Promise.h"
#include "nsICSSLoaderObserver.h"
#include "nsLayoutUtils.h"

namespace mozilla {

PreloadedStyleSheet::PreloadedStyleSheet(nsIURI* aURI,
                                         css::SheetParsingMode aParsingMode)
    : mLoaded(false), mURI(aURI), mParsingMode(aParsingMode) {}

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(PreloadedStyleSheet)
  NS_INTERFACE_MAP_ENTRY(nsIPreloadedStyleSheet)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(PreloadedStyleSheet)
NS_IMPL_CYCLE_COLLECTING_RELEASE(PreloadedStyleSheet)

NS_IMPL_CYCLE_COLLECTION(PreloadedStyleSheet, mSheet)

Result<StyleSheet*, nsresult> PreloadedStyleSheet::GetSheet() {
  MOZ_DIAGNOSTIC_ASSERT(mLoaded);

  if (!mSheet) {
    auto loader = MakeRefPtr<css::Loader>();
    MOZ_TRY_VAR(mSheet,
                loader->LoadSheetSync(mURI, mParsingMode,
                                      css::Loader::UseSystemPrincipal::Yes));
  }
  return {mSheet.get()};
}

nsresult PreloadedStyleSheet::Preload() {
  MOZ_DIAGNOSTIC_ASSERT(!mLoaded);
  mLoaded = true;
  MOZ_TRY(GetSheet());
  return NS_OK;
}

NS_IMPL_ISUPPORTS(PreloadedStyleSheet::StylesheetPreloadObserver,
                  nsICSSLoaderObserver)

NS_IMETHODIMP
PreloadedStyleSheet::StylesheetPreloadObserver::StyleSheetLoaded(
    StyleSheet* aSheet, bool aWasDeferred, nsresult aStatus) {
  MOZ_DIAGNOSTIC_ASSERT(!mPreloadedSheet->mLoaded);
  mPreloadedSheet->mLoaded = true;

  if (NS_FAILED(aStatus)) {
    mPromise->MaybeReject(aStatus);
  } else {
    mPromise->MaybeResolve(mPreloadedSheet);
  }

  return NS_OK;
}

// Note: After calling this method, the preloaded sheet *must not* be used
// until the observer is notified that the sheet has finished loading.
nsresult PreloadedStyleSheet::PreloadAsync(NotNull<dom::Promise*> aPromise) {
  MOZ_DIAGNOSTIC_ASSERT(!mLoaded);

  auto loader = MakeRefPtr<css::Loader>();
  auto obs = MakeRefPtr<StylesheetPreloadObserver>(aPromise, this);
  auto result = loader->LoadSheet(mURI, mParsingMode,
                                  css::Loader::UseSystemPrincipal::No, obs);
  if (result.isErr()) {
    return result.unwrapErr();
  }
  mSheet = result.unwrap();
  return NS_OK;
}

}  // namespace mozilla
