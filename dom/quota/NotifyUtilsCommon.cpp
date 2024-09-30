/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/quota/NotifyUtilsCommon.h"

#include "mozilla/Services.h"
#include "mozilla/dom/quota/QuotaCommon.h"
#include "mozilla/dom/quota/ResultExtensions.h"
#include "nsCOMPtr.h"
#include "nsError.h"
#include "nsIObserverService.h"
#include "nsThreadUtils.h"

namespace mozilla::dom::quota {

void NotifyObserversOnMainThread(
    const char* aTopic,
    std::function<already_AddRefed<nsISupports>()>&& aSubjectGetter) {
  auto mainThreadFunction = [topic = aTopic,
                             subjectGetter = std::move(aSubjectGetter)]() {
    nsCOMPtr<nsIObserverService> observerService =
        mozilla::services::GetObserverService();
    QM_TRY(MOZ_TO_RESULT(observerService), QM_VOID);

    nsCOMPtr<nsISupports> subject;
    if (subjectGetter) {
      subject = subjectGetter();
    }

    observerService->NotifyObservers(subject, topic, u"");
  };

  MOZ_ALWAYS_SUCCEEDS(NS_DispatchToMainThread(
      NS_NewRunnableFunction("dom::quota::NotifyObserversOnMainThread",
                             std::move(mainThreadFunction))));
}

}  // namespace mozilla::dom::quota
