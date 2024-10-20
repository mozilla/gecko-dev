/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_QUOTA_FORWARD_DECLS_H_
#define DOM_QUOTA_FORWARD_DECLS_H_

#include <cstdint>
#include <functional>

#include "nsStringFwd.h"
#include "nsTArrayForwardDeclare.h"
#include "mozilla/dom/quota/CommonMetadataArrayFwd.h"
#include "mozilla/dom/quota/Config.h"

enum class nsresult : uint32_t;
template <class T>
class RefPtr;

namespace mozilla {

using CStringArray = nsTArray<nsCString>;

template <class T>
class Maybe;

using MaybeCStringArray = Maybe<CStringArray>;

#ifdef QM_ERROR_STACKS_ENABLED
class QMResult;
#else
using QMResult = nsresult;
#endif

struct Ok;
template <typename V, typename E>
class Result;

using OkOrErr = Result<Ok, QMResult>;

template <typename ResolveValueT, typename RejectValueT, bool IsExclusive>
class MozPromise;

using BoolPromise = MozPromise<bool, nsresult, false>;
using Int64Promise = MozPromise<int64_t, nsresult, false>;
using UInt64Promise = MozPromise<uint64_t, nsresult, false>;

using ExclusiveBoolPromise = MozPromise<bool, nsresult, true>;

using MaybeCStringArrayPromise = MozPromise<MaybeCStringArray, nsresult, true>;

namespace ipc {

class BoolResponse;
class UInt64Response;
enum class ResponseRejectReason;

using BoolResponsePromise =
    MozPromise<BoolResponse, ResponseRejectReason, true>;
using UInt64ResponsePromise =
    MozPromise<UInt64Response, ResponseRejectReason, true>;

using NSResultResolver = std::function<void(const nsresult&)>;

using BoolResponseResolver = std::function<void(const BoolResponse&)>;
using UInt64ResponseResolver = std::function<void(const UInt64Response&)>;

}  // namespace ipc

namespace dom::quota {

class ClientDirectoryLock;
class UniversalDirectoryLock;

using ClientDirectoryLockPromise =
    MozPromise<RefPtr<ClientDirectoryLock>, nsresult, true>;
using UniversalDirectoryLockPromise =
    MozPromise<RefPtr<UniversalDirectoryLock>, nsresult, true>;

struct OriginMetadata;
struct PrincipalMetadata;
using OriginMetadataArray = nsTArray<OriginMetadata>;
using PrincipalMetadataArray = nsTArray<PrincipalMetadata>;
using MaybePrincipalMetadataArray = Maybe<PrincipalMetadataArray>;
class UsageInfo;

using OriginMetadataArrayPromise =
    MozPromise<OriginMetadataArray, nsresult, true>;
using OriginUsageMetadataArrayPromise =
    MozPromise<OriginUsageMetadataArray, nsresult, true>;
using MaybePrincipalMetadataArrayPromise =
    MozPromise<MaybePrincipalMetadataArray, nsresult, true>;
using UsageInfoPromise = MozPromise<UsageInfo, nsresult, false>;

class OriginUsageMetadataArrayResponse;
class UsageInfoResponse;

using OriginUsageMetadataArrayResponsePromise =
    MozPromise<OriginUsageMetadataArrayResponse,
               mozilla::ipc::ResponseRejectReason, true>;
using UsageInfoResponsePromise =
    MozPromise<UsageInfoResponse, mozilla::ipc::ResponseRejectReason, true>;

using OriginUsageMetadataArrayResponseResolver =
    std::function<void(OriginUsageMetadataArrayResponse&&)>;
using UsageInfoResponseResolver = std::function<void(const UsageInfoResponse&)>;

}  // namespace dom::quota

}  // namespace mozilla

#endif  // DOM_QUOTA_FORWARD_DECLS_H_
