/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_UrlClassifierExceptionList_h
#define mozilla_UrlClassifierExceptionList_h

#include "nsIUrlClassifierExceptionList.h"
#include "nsISupports.h"
#include "nsTArray.h"
#include "nsString.h"

namespace mozilla::net {

/**
 * @see nsIUrlClassifierExceptionList
 */
class UrlClassifierExceptionList final : public nsIUrlClassifierExceptionList {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIURLCLASSIFIEREXCEPTIONLIST

  UrlClassifierExceptionList() = default;

 private:
  ~UrlClassifierExceptionList() = default;

  nsCString mFeature;
  nsTArray<RefPtr<nsIUrlClassifierExceptionListEntry>> mEntries;
};

}  // namespace mozilla::net

#endif
