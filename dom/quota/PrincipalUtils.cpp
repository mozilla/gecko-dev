/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/quota/PrincipalUtils.h"

#include "mozilla/SystemPrincipal.h"
#include "mozilla/dom/quota/Constants.h"
#include "mozilla/dom/quota/CommonMetadata.h"
#include "mozilla/dom/quota/QuotaCommon.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "mozilla/dom/quota/ResultExtensions.h"
#include "mozilla/ipc/PBackgroundSharedTypes.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsNetUtil.h"
#include "nsPIDOMWindow.h"
#include "OriginParser.h"

namespace mozilla::dom::quota {

using namespace mozilla::ipc;

bool IsPrincipalInfoValid(const PrincipalInfo& aPrincipalInfo) {
  switch (aPrincipalInfo.type()) {
    // A system principal is acceptable.
    case PrincipalInfo::TSystemPrincipalInfo: {
      return true;
    }

    // Validate content principals to ensure that the spec, originNoSuffix and
    // baseDomain are sane.
    case PrincipalInfo::TContentPrincipalInfo: {
      const ContentPrincipalInfo& info =
          aPrincipalInfo.get_ContentPrincipalInfo();

      // Verify the principal spec parses.
      nsCOMPtr<nsIURI> uri;
      QM_TRY(MOZ_TO_RESULT(NS_NewURI(getter_AddRefs(uri), info.spec())), false);

      nsCOMPtr<nsIPrincipal> principal =
          BasePrincipal::CreateContentPrincipal(uri, info.attrs());
      QM_TRY(MOZ_TO_RESULT(principal), false);

      // Verify the principal originNoSuffix matches spec.
      QM_TRY_INSPECT(const auto& originNoSuffix,
                     MOZ_TO_RESULT_INVOKE_MEMBER_TYPED(nsAutoCString, principal,
                                                       GetOriginNoSuffix),
                     false);

      if (NS_WARN_IF(originNoSuffix != info.originNoSuffix())) {
        QM_WARNING("originNoSuffix (%s) doesn't match passed one (%s)!",
                   originNoSuffix.get(), info.originNoSuffix().get());
        return false;
      }

      if (NS_WARN_IF(info.originNoSuffix().EqualsLiteral(kChromeOrigin))) {
        return false;
      }

      if (NS_WARN_IF(info.originNoSuffix().FindChar('^', 0) != -1)) {
        QM_WARNING("originNoSuffix (%s) contains the '^' character!",
                   info.originNoSuffix().get());
        return false;
      }

      // Verify the principal baseDomain exists.
      if (NS_WARN_IF(info.baseDomain().IsVoid())) {
        return false;
      }

      // Verify the principal baseDomain matches spec.
      QM_TRY_INSPECT(const auto& baseDomain,
                     MOZ_TO_RESULT_INVOKE_MEMBER_TYPED(nsAutoCString, principal,
                                                       GetBaseDomain),
                     false);

      if (NS_WARN_IF(baseDomain != info.baseDomain())) {
        QM_WARNING("baseDomain (%s) doesn't match passed one (%s)!",
                   baseDomain.get(), info.baseDomain().get());
        return false;
      }

      return true;
    }

    default: {
      break;
    }
  }

  // Null and expanded principals are not acceptable.
  return false;
}

Result<PrincipalMetadata, nsresult> GetInfoFromValidatedPrincipalInfo(
    QuotaManager& aQuotaManager, const PrincipalInfo& aPrincipalInfo) {
  MOZ_ASSERT(IsPrincipalInfoValid(aPrincipalInfo));

  switch (aPrincipalInfo.type()) {
    case PrincipalInfo::TSystemPrincipalInfo: {
      return GetInfoForChrome();
    }

    case PrincipalInfo::TContentPrincipalInfo: {
      const ContentPrincipalInfo& info =
          aPrincipalInfo.get_ContentPrincipalInfo();

      nsCString suffix;
      info.attrs().CreateSuffix(suffix);

      nsCString origin = info.originNoSuffix() + suffix;

      if (IsUUIDOrigin(origin)) {
        QM_TRY_INSPECT(const auto& originalOrigin,
                       aQuotaManager.GetOriginFromStorageOrigin(origin));

        nsCOMPtr<nsIPrincipal> principal =
            BasePrincipal::CreateContentPrincipal(originalOrigin);
        QM_TRY(MOZ_TO_RESULT(principal));

        PrincipalInfo principalInfo;
        QM_TRY(
            MOZ_TO_RESULT(PrincipalToPrincipalInfo(principal, &principalInfo)));

        return GetInfoFromValidatedPrincipalInfo(aQuotaManager, principalInfo);
      }

      PrincipalMetadata principalMetadata;

      principalMetadata.mSuffix = suffix;

      principalMetadata.mGroup = info.baseDomain() + suffix;

      principalMetadata.mOrigin = origin;

      if (info.attrs().IsPrivateBrowsing()) {
        QM_TRY_UNWRAP(principalMetadata.mStorageOrigin,
                      aQuotaManager.EnsureStorageOriginFromOrigin(origin));
      } else {
        principalMetadata.mStorageOrigin = origin;
      }

      principalMetadata.mIsPrivate = info.attrs().IsPrivateBrowsing();

      return principalMetadata;
    }

    default: {
      MOZ_ASSERT_UNREACHABLE("Should never get here!");
      return Err(NS_ERROR_UNEXPECTED);
    }
  }
}

nsAutoCString GetGroupFromValidatedPrincipalInfo(
    const PrincipalInfo& aPrincipalInfo) {
  MOZ_ASSERT(IsPrincipalInfoValid(aPrincipalInfo));

  switch (aPrincipalInfo.type()) {
    case PrincipalInfo::TSystemPrincipalInfo: {
      return nsAutoCString{GetGroupForChrome()};
    }

    case PrincipalInfo::TContentPrincipalInfo: {
      const ContentPrincipalInfo& info =
          aPrincipalInfo.get_ContentPrincipalInfo();

      nsAutoCString suffix;

      info.attrs().CreateSuffix(suffix);

      return info.baseDomain() + suffix;
    }

    default: {
      MOZ_CRASH("Should never get here!");
    }
  }
}

nsAutoCString GetOriginFromValidatedPrincipalInfo(
    const PrincipalInfo& aPrincipalInfo) {
  MOZ_ASSERT(IsPrincipalInfoValid(aPrincipalInfo));

  switch (aPrincipalInfo.type()) {
    case PrincipalInfo::TSystemPrincipalInfo: {
      return nsAutoCString{GetOriginForChrome()};
    }

    case PrincipalInfo::TContentPrincipalInfo: {
      const ContentPrincipalInfo& info =
          aPrincipalInfo.get_ContentPrincipalInfo();

      nsAutoCString suffix;

      info.attrs().CreateSuffix(suffix);

      return info.originNoSuffix() + suffix;
    }

    default: {
      MOZ_CRASH("Should never get here!");
    }
  }
}

Result<PrincipalMetadata, nsresult> GetInfoFromPrincipal(
    nsIPrincipal* aPrincipal) {
  MOZ_ASSERT(aPrincipal);

  if (aPrincipal->IsSystemPrincipal()) {
    return GetInfoForChrome();
  }

  if (aPrincipal->GetIsNullPrincipal()) {
    NS_WARNING("IndexedDB not supported from this principal!");
    return Err(NS_ERROR_FAILURE);
  }

  PrincipalMetadata principalMetadata;

  QM_TRY(MOZ_TO_RESULT(aPrincipal->GetOrigin(principalMetadata.mOrigin)));

  if (principalMetadata.mOrigin.EqualsLiteral(kChromeOrigin)) {
    NS_WARNING("Non-chrome principal can't use chrome origin!");
    return Err(NS_ERROR_FAILURE);
  }

  aPrincipal->OriginAttributesRef().CreateSuffix(principalMetadata.mSuffix);

  nsAutoCString baseDomain;
  QM_TRY(MOZ_TO_RESULT(aPrincipal->GetBaseDomain(baseDomain)));

  MOZ_ASSERT(!baseDomain.IsEmpty());

  principalMetadata.mGroup = baseDomain + principalMetadata.mSuffix;

  principalMetadata.mStorageOrigin = principalMetadata.mOrigin;

  principalMetadata.mIsPrivate = aPrincipal->GetIsInPrivateBrowsing();

  return principalMetadata;
}

Result<PrincipalMetadata, nsresult> GetInfoFromWindow(
    nsPIDOMWindowOuter* aWindow) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aWindow);

  nsCOMPtr<nsIScriptObjectPrincipal> sop = do_QueryInterface(aWindow);
  QM_TRY(OkIf(sop), Err(NS_ERROR_FAILURE));

  nsCOMPtr<nsIPrincipal> principal = sop->GetPrincipal();
  QM_TRY(OkIf(principal), Err(NS_ERROR_FAILURE));

  return GetInfoFromPrincipal(principal);
}

Result<nsAutoCString, nsresult> GetOriginFromPrincipal(
    nsIPrincipal* aPrincipal) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPrincipal);

  if (aPrincipal->IsSystemPrincipal()) {
    return nsAutoCString{GetOriginForChrome()};
  }

  if (aPrincipal->GetIsNullPrincipal()) {
    NS_WARNING("IndexedDB not supported from this principal!");
    return Err(NS_ERROR_FAILURE);
  }

  QM_TRY_UNWRAP(const auto origin, MOZ_TO_RESULT_INVOKE_MEMBER_TYPED(
                                       nsAutoCString, aPrincipal, GetOrigin));

  if (origin.EqualsLiteral(kChromeOrigin)) {
    NS_WARNING("Non-chrome principal can't use chrome origin!");
    return Err(NS_ERROR_FAILURE);
  }

  return origin;
}

Result<nsAutoCString, nsresult> GetOriginFromWindow(
    nsPIDOMWindowOuter* aWindow) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aWindow);

  nsCOMPtr<nsIScriptObjectPrincipal> sop = do_QueryInterface(aWindow);
  QM_TRY(OkIf(sop), Err(NS_ERROR_FAILURE));

  nsCOMPtr<nsIPrincipal> principal = sop->GetPrincipal();
  QM_TRY(OkIf(principal), Err(NS_ERROR_FAILURE));

  QM_TRY_RETURN(GetOriginFromPrincipal(principal));
}

PrincipalMetadata GetInfoForChrome() {
  return {{},
          GetGroupForChrome(),
          GetOriginForChrome(),
          GetOriginForChrome(),
          false};
}

nsLiteralCString GetGroupForChrome() { return nsLiteralCString{kChromeOrigin}; }

nsLiteralCString GetOriginForChrome() {
  return nsLiteralCString{kChromeOrigin};
}

}  // namespace mozilla::dom::quota
