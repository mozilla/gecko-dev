/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_QUOTA_PRINCIPALUTILS_H_
#define DOM_QUOTA_PRINCIPALUTILS_H_

#include <cstdint>

#include "nsStringFwd.h"

class nsIPrincipal;
class nsPIDOMWindowOuter;
enum class nsresult : uint32_t;

namespace mozilla {

template <typename V, typename E>
class Result;

}

namespace mozilla::ipc {

class PrincipalInfo;

}

namespace mozilla::dom::quota {

struct PrincipalMetadata;
class QuotaManager;

bool IsPrincipalInfoValid(const mozilla::ipc::PrincipalInfo& aPrincipalInfo);

Result<PrincipalMetadata, nsresult> GetInfoFromValidatedPrincipalInfo(
    QuotaManager& aQuotaManager,
    const mozilla::ipc::PrincipalInfo& aPrincipalInfo);

nsAutoCString GetGroupFromValidatedPrincipalInfo(
    const mozilla::ipc::PrincipalInfo& aPrincipalInfo);

nsAutoCString GetOriginFromValidatedPrincipalInfo(
    const mozilla::ipc::PrincipalInfo& aPrincipalInfo);

Result<PrincipalMetadata, nsresult> GetInfoFromPrincipal(
    nsIPrincipal* aPrincipal);

Result<PrincipalMetadata, nsresult> GetInfoFromWindow(
    nsPIDOMWindowOuter* aWindow);

Result<nsAutoCString, nsresult> GetOriginFromPrincipal(
    nsIPrincipal* aPrincipal);

Result<nsAutoCString, nsresult> GetOriginFromWindow(
    nsPIDOMWindowOuter* aWindow);

nsLiteralCString GetGroupForChrome();

nsLiteralCString GetOriginForChrome();

PrincipalMetadata GetInfoForChrome();

}  // namespace mozilla::dom::quota

#endif  // DOM_QUOTA_PRINCIPALUTILS_H_
