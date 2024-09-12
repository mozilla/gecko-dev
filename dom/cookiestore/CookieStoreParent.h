/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CookieStoreParent_h
#define mozilla_dom_CookieStoreParent_h

#include "mozilla/dom/PCookieStoreParent.h"
#include "mozilla/MozPromise.h"

namespace mozilla::dom {

class CookieStoreService;

class CookieStoreParent final : public PCookieStoreParent {
  friend class PCookieStoreParent;

 public:
  using GetRequestPromise =
      MozPromise<CopyableTArray<CookieData>, nsresult, true>;
  using SetDeleteRequestPromise = MozPromise<bool, nsresult, true>;

  NS_INLINE_DECL_REFCOUNTING(CookieStoreParent)

  CookieStoreParent();

 private:
  ~CookieStoreParent();

  mozilla::ipc::IPCResult RecvGetRequest(
      const nsString& aDomain, const OriginAttributes& aOriginAttributes,
      const bool& aMatchName, const nsString& aName, const nsCString& aPath,
      const bool& aOnlyFirstMatch, GetRequestResolver&& aResolver);

  mozilla::ipc::IPCResult RecvSetRequest(
      const nsString& aDomain, const OriginAttributes& aOriginAttributes,
      const nsString& aName, const nsString& aValue, const bool& aSession,
      const int64_t& aExpires, const nsString& aPath, const int32_t& aSameSite,
      const bool& aPartitioned, SetRequestResolver&& aResolver);

  mozilla::ipc::IPCResult RecvDeleteRequest(
      const nsString& aDomain, const OriginAttributes& aOriginAttributes,
      const nsString& aName, const nsString& aPath, const bool& aPartitioned,
      DeleteRequestResolver&& aResolver);

  mozilla::ipc::IPCResult RecvClose();
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_CookieStoreParent_h
