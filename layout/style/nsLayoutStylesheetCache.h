/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsLayoutStylesheetCache_h__
#define nsLayoutStylesheetCache_h__

#include "nsIMemoryReporter.h"
#include "nsIObserver.h"
#include "nsAutoPtr.h"
#include "mozilla/Attributes.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/StaticPtr.h"

class nsIFile;
class nsIURI;

namespace mozilla {
class CSSStyleSheet;
namespace css {
class Loader;
}
}

class nsLayoutStylesheetCache MOZ_FINAL
 : public nsIObserver
 , public nsIMemoryReporter
{
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
  NS_DECL_NSIMEMORYREPORTER

  static mozilla::CSSStyleSheet* ScrollbarsSheet();
  static mozilla::CSSStyleSheet* FormsSheet();
  // This function is expected to return nullptr when the dom.forms.number
  // pref is disabled.
  static mozilla::CSSStyleSheet* NumberControlSheet();
  static mozilla::CSSStyleSheet* UserContentSheet();
  static mozilla::CSSStyleSheet* UserChromeSheet();
  static mozilla::CSSStyleSheet* UASheet();
  static mozilla::CSSStyleSheet* HTMLSheet();
  static mozilla::CSSStyleSheet* MinimalXULSheet();
  static mozilla::CSSStyleSheet* XULSheet();
  static mozilla::CSSStyleSheet* QuirkSheet();
  static mozilla::CSSStyleSheet* FullScreenOverrideSheet();
  static mozilla::CSSStyleSheet* SVGSheet();
  static mozilla::CSSStyleSheet* MathMLSheet();
  static mozilla::CSSStyleSheet* CounterStylesSheet();

  static void Shutdown();

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

private:
  nsLayoutStylesheetCache();
  ~nsLayoutStylesheetCache();

  static void EnsureGlobal();
  void InitFromProfile();
  void InitMemoryReporter();
  static void LoadSheetFile(nsIFile* aFile,
                            nsRefPtr<mozilla::CSSStyleSheet>& aSheet);
  static void LoadSheet(nsIURI* aURI, nsRefPtr<mozilla::CSSStyleSheet>& aSheet,
                        bool aEnableUnsafeRules);

  static mozilla::StaticRefPtr<nsLayoutStylesheetCache> gStyleCache;
  static mozilla::css::Loader* gCSSLoader;
  nsRefPtr<mozilla::CSSStyleSheet> mScrollbarsSheet;
  nsRefPtr<mozilla::CSSStyleSheet> mFormsSheet;
  nsRefPtr<mozilla::CSSStyleSheet> mNumberControlSheet;
  nsRefPtr<mozilla::CSSStyleSheet> mUserContentSheet;
  nsRefPtr<mozilla::CSSStyleSheet> mUserChromeSheet;
  nsRefPtr<mozilla::CSSStyleSheet> mUASheet;
  nsRefPtr<mozilla::CSSStyleSheet> mHTMLSheet;
  nsRefPtr<mozilla::CSSStyleSheet> mMinimalXULSheet;
  nsRefPtr<mozilla::CSSStyleSheet> mXULSheet;
  nsRefPtr<mozilla::CSSStyleSheet> mQuirkSheet;
  nsRefPtr<mozilla::CSSStyleSheet> mFullScreenOverrideSheet;
  nsRefPtr<mozilla::CSSStyleSheet> mSVGSheet;
  nsRefPtr<mozilla::CSSStyleSheet> mMathMLSheet;
  nsRefPtr<mozilla::CSSStyleSheet> mCounterStylesSheet;
};

#endif
