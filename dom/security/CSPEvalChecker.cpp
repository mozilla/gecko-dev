/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CSPEvalChecker.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/WorkerRunnable.h"
#include "mozilla/ErrorResult.h"
#include "nsGlobalWindowInner.h"
#include "nsIDocument.h"
#include "nsCOMPtr.h"
#include "nsJSUtils.h"

using namespace mozilla;
using namespace mozilla::dom;

namespace {

nsresult CheckInternal(nsIContentSecurityPolicy* aCSP,
                       nsICSPEventListener* aCSPEventListener,
                       const nsAString& aExpression,
                       const nsAString& aFileNameString, uint32_t aLineNum,
                       uint32_t aColumnNum, bool* aAllowed) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aAllowed);

  // The value is set at any "return", but better to have a default value here.
  *aAllowed = false;

  if (!aCSP) {
    *aAllowed = true;
    return NS_OK;
  }

  bool reportViolation = false;
  nsresult rv = aCSP->GetAllowsEval(&reportViolation, aAllowed);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    *aAllowed = false;
    return rv;
  }

  if (reportViolation) {
    aCSP->LogViolationDetails(nsIContentSecurityPolicy::VIOLATION_TYPE_EVAL,
                              nullptr,  // triggering element
                              aCSPEventListener, aFileNameString, aExpression,
                              aLineNum, aColumnNum, EmptyString(),
                              EmptyString());
  }

  return NS_OK;
}

class WorkerCSPCheckRunnable final : public WorkerMainThreadRunnable {
 public:
  WorkerCSPCheckRunnable(WorkerPrivate* aWorkerPrivate,
                         const nsAString& aExpression,
                         const nsAString& aFileNameString, uint32_t aLineNum,
                         uint32_t aColumnNum)
      : WorkerMainThreadRunnable(aWorkerPrivate,
                                 NS_LITERAL_CSTRING("CSP Eval Check")),
        mExpression(aExpression),
        mFileNameString(aFileNameString),
        mLineNum(aLineNum),
        mColumnNum(aColumnNum),
        mEvalAllowed(false) {}

  bool MainThreadRun() override {
    mResult = CheckInternal(
        mWorkerPrivate->GetCSP(), mWorkerPrivate->CSPEventListener(),
        mExpression, mFileNameString, mLineNum, mColumnNum, &mEvalAllowed);
    return true;
  }

  nsresult GetResult(bool* aAllowed) {
    MOZ_ASSERT(aAllowed);
    *aAllowed = mEvalAllowed;
    return mResult;
  }

 private:
  const nsString mExpression;
  const nsString mFileNameString;
  const uint32_t mLineNum;
  const uint32_t mColumnNum;
  bool mEvalAllowed;
  nsresult mResult;
};

}  // namespace

/* static */ nsresult CSPEvalChecker::CheckForWindow(
    JSContext* aCx, nsGlobalWindowInner* aWindow, const nsAString& aExpression,
    bool* aAllowEval) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aWindow);
  MOZ_ASSERT(aAllowEval);

  // The value is set at any "return", but better to have a default value here.
  *aAllowEval = false;

  // if CSP is enabled, and setTimeout/setInterval was called with a string,
  // disable the registration and log an error
  nsCOMPtr<nsIDocument> doc = aWindow->GetExtantDoc();
  if (!doc) {
    // if there's no document, we don't have to do anything.
    *aAllowEval = true;
    return NS_OK;
  }

  nsCOMPtr<nsIContentSecurityPolicy> csp;
  nsresult rv = doc->NodePrincipal()->GetCsp(getter_AddRefs(csp));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    *aAllowEval = false;
    return rv;
  }

  // Get the calling location.
  uint32_t lineNum = 0;
  uint32_t columnNum = 0;
  nsAutoString fileNameString;
  if (!nsJSUtils::GetCallingLocation(aCx, fileNameString, &lineNum,
                                     &columnNum)) {
    fileNameString.AssignLiteral("unknown");
  }

  rv = CheckInternal(csp, nullptr /* no CSPEventListener for window */,
                     aExpression, fileNameString, lineNum, columnNum,
                     aAllowEval);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    *aAllowEval = false;
    return rv;
  }

  return NS_OK;
}

/* static */ nsresult CSPEvalChecker::CheckForWorker(
    JSContext* aCx, WorkerPrivate* aWorkerPrivate, const nsAString& aExpression,
    bool* aAllowEval) {
  MOZ_ASSERT(aWorkerPrivate);
  aWorkerPrivate->AssertIsOnWorkerThread();
  MOZ_ASSERT(aAllowEval);

  // The value is set at any "return", but better to have a default value here.
  *aAllowEval = false;

  // Get the calling location.
  uint32_t lineNum = 0;
  uint32_t columnNum = 0;
  nsAutoString fileNameString;
  if (!nsJSUtils::GetCallingLocation(aCx, fileNameString, &lineNum,
                                     &columnNum)) {
    fileNameString.AssignLiteral("unknown");
  }

  RefPtr<WorkerCSPCheckRunnable> r = new WorkerCSPCheckRunnable(
      aWorkerPrivate, aExpression, fileNameString, lineNum, columnNum);
  ErrorResult error;
  r->Dispatch(Canceling, error);
  if (NS_WARN_IF(error.Failed())) {
    *aAllowEval = false;
    return error.StealNSResult();
  }

  nsresult rv = r->GetResult(aAllowEval);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    *aAllowEval = false;
    return rv;
  }

  return NS_OK;
}
