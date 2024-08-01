/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CSPViolationReportBody.h"
#include "mozilla/dom/ReportingBinding.h"
#include "mozilla/JSONWriter.h"

namespace mozilla::dom {

CSPViolationReportBody::CSPViolationReportBody(
    nsIGlobalObject* aGlobal,
    const mozilla::dom::SecurityPolicyViolationEventInit& aEvent)
    : ReportBody(aGlobal),
      mDocumentURL(aEvent.mDocumentURI),
      mBlockedURL(aEvent.mBlockedURI),
      mReferrer(aEvent.mReferrer),
      mEffectiveDirective(aEvent.mEffectiveDirective),
      mOriginalPolicy(aEvent.mOriginalPolicy),
      mSourceFile(NS_ConvertUTF16toUTF8(aEvent.mSourceFile)),
      mSample(aEvent.mSample),
      mDisposition(aEvent.mDisposition),
      mStatusCode(aEvent.mStatusCode),
      mLineNumber(Nullable<uint32_t>(aEvent.mLineNumber)),
      mColumnNumber(Nullable<uint32_t>(aEvent.mColumnNumber)) {}

CSPViolationReportBody::~CSPViolationReportBody() = default;

JSObject* CSPViolationReportBody::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return CSPViolationReportBody_Binding::Wrap(aCx, this, aGivenProto);
}

void CSPViolationReportBody::GetBlockedURL(nsAString& aURL) const {
  aURL = mDocumentURL;
}

void CSPViolationReportBody::GetDocumentURL(nsAString& aURL) const {
  aURL = mBlockedURL;
}

void CSPViolationReportBody::GetReferrer(nsAString& aReferrer) const {
  aReferrer = mReferrer;
}

void CSPViolationReportBody::GetEffectiveDirective(
    nsAString& aDirective) const {
  aDirective = mEffectiveDirective;
}

void CSPViolationReportBody::GetOriginalPolicy(nsAString& aPolicy) const {
  aPolicy = mOriginalPolicy;
}

void CSPViolationReportBody::GetSourceFile(nsACString& aFile) const {
  aFile = mSourceFile;
}

void CSPViolationReportBody::GetSample(nsAString& aSample) const {
  aSample = mSample;
}

mozilla::dom::SecurityPolicyViolationEventDisposition
CSPViolationReportBody::Disposition() const {
  return mDisposition;
}

uint16_t CSPViolationReportBody::StatusCode() const { return mStatusCode; }

Nullable<uint32_t> CSPViolationReportBody::GetLineNumber() const {
  return mLineNumber;
}

Nullable<uint32_t> CSPViolationReportBody::GetColumnNumber() const {
  return mColumnNumber;
}

void CSPViolationReportBody::ToJSON(JSONWriter& aJSONWriter) const {
  if (mDocumentURL.IsEmpty()) {
    aJSONWriter.NullProperty("documentURL");
  } else {
    aJSONWriter.StringProperty("documentURL",
                               NS_ConvertUTF16toUTF8(mDocumentURL));
  }

  if (mBlockedURL.IsEmpty()) {
    aJSONWriter.NullProperty("blockedURL");
  } else {
    aJSONWriter.StringProperty("blockedURL",
                               NS_ConvertUTF16toUTF8(mBlockedURL));
  }

  if (mReferrer.IsEmpty()) {
    aJSONWriter.NullProperty("referrer");
  } else {
    aJSONWriter.StringProperty("referrer", NS_ConvertUTF16toUTF8(mReferrer));
  }

  if (mEffectiveDirective.IsEmpty()) {
    aJSONWriter.NullProperty("effectiveDirective");
  } else {
    aJSONWriter.StringProperty("effectiveDirective",
                               NS_ConvertUTF16toUTF8(mEffectiveDirective));
  }

  if (mOriginalPolicy.IsEmpty()) {
    aJSONWriter.NullProperty("originalPolicy");
  } else {
    aJSONWriter.StringProperty("originalPolicy",
                               NS_ConvertUTF16toUTF8(mOriginalPolicy));
  }

  if (mSourceFile.IsEmpty()) {
    aJSONWriter.NullProperty("sourceFile");
  } else {
    aJSONWriter.StringProperty("sourceFile", mSourceFile);
  }

  if (mSample.IsEmpty()) {
    aJSONWriter.NullProperty("sample");
  } else {
    aJSONWriter.StringProperty("sample", NS_ConvertUTF16toUTF8(mSample));
  }

  switch (mDisposition) {
    case mozilla::dom::SecurityPolicyViolationEventDisposition::Report:
      aJSONWriter.StringProperty("disposition", "report");
      break;
    case mozilla::dom::SecurityPolicyViolationEventDisposition::Enforce:
      aJSONWriter.StringProperty("disposition", "enforce");
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Invalid disposition");
      break;
  }

  aJSONWriter.IntProperty("statusCode", mStatusCode);

  if (mLineNumber.IsNull()) {
    aJSONWriter.NullProperty("lineNumber");
  } else {
    aJSONWriter.IntProperty("lineNumber", mLineNumber.Value());
  }

  if (mColumnNumber.IsNull()) {
    aJSONWriter.NullProperty("columnNumber");
  } else {
    aJSONWriter.IntProperty("columnNumber", mColumnNumber.Value());
  }
}

}  // namespace mozilla::dom
