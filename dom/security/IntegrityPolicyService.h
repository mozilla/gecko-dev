/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef IntegrityPolicyService_h___
#define IntegrityPolicyService_h___

#include "nsIContentPolicy.h"

#define NS_INTEGRITYPOLICYSERVICE_CONTRACTID \
  "@mozilla.org/integritypolicyservice;1"
#define INTEGRITYPOLICYSERVICE_CID \
  {0x3662958c, 0x0e34, 0x4f57, {0xbc, 0x6d, 0x2a, 0xcc, 0xde, 0xb4, 0x34, 0x2e}}

namespace mozilla::dom {

class IntegrityPolicyService : public nsIContentPolicy {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICONTENTPOLICY

  IntegrityPolicyService() = default;

  bool ShouldRequestBeBlocked(nsIURI* aContentLocation, nsILoadInfo* aLoadInfo);

  void MaybeReport(nsIURI* aContentLocation, nsILoadInfo* aLoadInfo,
                   bool aEnforce, bool aReportOnly);

 protected:
  virtual ~IntegrityPolicyService();
};
}  // namespace mozilla::dom

#endif /* IntegrityPolicyService_h___ */
