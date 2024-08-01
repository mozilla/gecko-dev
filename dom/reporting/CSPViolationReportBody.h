/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CSPViolationReportBody_h
#define mozilla_dom_CSPViolationReportBody_h

#include "mozilla/dom/Nullable.h"
#include "mozilla/dom/ReportBody.h"
#include "mozilla/dom/SecurityPolicyViolationEvent.h"

namespace mozilla::dom {

class CSPViolationReportBody final : public ReportBody {
 public:
  CSPViolationReportBody(
      nsIGlobalObject* aGlobal,
      const mozilla::dom::SecurityPolicyViolationEventInit& aEvent);

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  void GetBlockedURL(nsAString& aURL) const;

  void GetDocumentURL(nsAString& aURL) const;

  void GetReferrer(nsAString& aReferrer) const;

  void GetEffectiveDirective(nsAString& aDirective) const;

  void GetOriginalPolicy(nsAString& aPolicy) const;

  void GetSourceFile(nsACString& aFile) const;

  void GetSample(nsAString& aSample) const;

  mozilla::dom::SecurityPolicyViolationEventDisposition Disposition() const;

  uint16_t StatusCode() const;

  Nullable<uint32_t> GetLineNumber() const;

  Nullable<uint32_t> GetColumnNumber() const;

 protected:
  void ToJSON(JSONWriter& aJSONWriter) const override;

 private:
  ~CSPViolationReportBody();

  const nsString mDocumentURL;
  const nsString mBlockedURL;
  const nsString mReferrer;
  const nsString mEffectiveDirective;
  const nsString mOriginalPolicy;
  const nsCString mSourceFile;
  const nsString mSample;
  const mozilla::dom::SecurityPolicyViolationEventDisposition mDisposition;
  const uint16_t mStatusCode;
  const Nullable<uint32_t> mLineNumber;
  const Nullable<uint32_t> mColumnNumber;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_CSPViolationReportBody_h
