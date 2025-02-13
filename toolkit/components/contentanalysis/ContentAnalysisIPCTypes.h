/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ContentAnalysisIPCTypes_h
#define mozilla_ContentAnalysisIPCTypes_h

#include "ipc/EnumSerializer.h"
#include "ipc/IPCMessageUtilsSpecializations.h"
#include "js/PropertyAndElement.h"
#include "mozilla/Variant.h"
#include "nsIContentAnalysis.h"
#include "mozilla/RefPtr.h"

namespace mozilla {
namespace contentanalysis {

enum class NoContentAnalysisResult : uint8_t {
  ALLOW_DUE_TO_CONTENT_ANALYSIS_NOT_ACTIVE,
  ALLOW_DUE_TO_CONTEXT_EXEMPT_FROM_CONTENT_ANALYSIS,
  ALLOW_DUE_TO_SAME_TAB_SOURCE,
  ALLOW_DUE_TO_COULD_NOT_GET_DATA,
  DENY_DUE_TO_CANCELED,
  DENY_DUE_TO_INVALID_JSON_RESPONSE,
  DENY_DUE_TO_OTHER_ERROR,
  LAST_VALUE
};

class ContentAnalysisActionResult : public nsIContentAnalysisResult {
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSICONTENTANALYSISRESULT

 private:
  // Use MakeRefPtr as factory.
  template <typename T, typename... Args>
  friend RefPtr<T> mozilla::MakeRefPtr(Args&&...);
  explicit ContentAnalysisActionResult(
      nsIContentAnalysisResponse::Action aAction)
      : mValue(aAction) {}
  virtual ~ContentAnalysisActionResult() = default;
  nsIContentAnalysisResponse::Action mValue;
};

class ContentAnalysisNoResult : public nsIContentAnalysisResult {
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSICONTENTANALYSISRESULT

 private:
  // Use MakeRefPtr as factory.
  template <typename T, typename... Args>
  friend RefPtr<T> mozilla::MakeRefPtr(Args&&...);
  explicit ContentAnalysisNoResult(NoContentAnalysisResult aResult)
      : mValue(aResult) {}

  virtual ~ContentAnalysisNoResult() = default;
  NoContentAnalysisResult mValue;
};

}  // namespace contentanalysis
}  // namespace mozilla

namespace IPC {

template <>
struct ParamTraits<mozilla::contentanalysis::NoContentAnalysisResult>
    : public ContiguousEnumSerializer<
          mozilla::contentanalysis::NoContentAnalysisResult,
          static_cast<mozilla::contentanalysis::NoContentAnalysisResult>(0),
          mozilla::contentanalysis::NoContentAnalysisResult::LAST_VALUE> {};

template <>
struct ParamTraits<nsIContentAnalysisResponse::Action>
    : public ContiguousEnumSerializerInclusive<
          nsIContentAnalysisResponse::Action,
          nsIContentAnalysisResponse::Action::eUnspecified,
          nsIContentAnalysisResponse::Action::eCanceled> {};

}  // namespace IPC

#endif  // mozilla_ContentAnalysisIPCTypes_h
