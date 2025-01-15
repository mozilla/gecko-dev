/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TemporaryAccessGrantObserver.h"

#include "mozilla/AppShutdown.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/PermissionManager.h"
#include "mozilla/Services.h"
#include "nsIObserverService.h"
#include "nsTHashtable.h"
#include "nsXULAppAPI.h"

using namespace mozilla;

StaticAutoPtr<TemporaryAccessGrantObserver::ObserversTable>
    TemporaryAccessGrantObserver::sObservers;

TemporaryAccessGrantObserver::TemporaryAccessGrantObserver(
    PermissionManager* aPM, nsIPrincipal* aPrincipal, const nsACString& aType)
    : mPM(aPM), mPrincipal(aPrincipal), mType(aType) {
  MOZ_ASSERT(XRE_IsParentProcess(),
             "Enforcing temporary access grant lifetimes can only be done in "
             "the parent process");
}

NS_IMPL_ISUPPORTS(TemporaryAccessGrantObserver, nsIObserver, nsINamed)

// static
void TemporaryAccessGrantObserver::Create(PermissionManager* aPM,
                                          nsIPrincipal* aPrincipal,
                                          const nsACString& aType) {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());

  if (!sObservers) {
    // Prevent us from resurrection during shutdown. Note that ClearOnShutdown
    // would kill us right away in this case, but we want to return with no-op.
    if (AppShutdown::IsInOrBeyond(ShutdownPhase::XPCOMWillShutdown)) {
      return;
    }
    sObservers = new ObserversTable();
    // PermissionManager shuts down in this phase, so we should avoid to keep
    // references to it for longer. Clearing will cancel all outstanding timers
    // and clear the table. This is safe as the timers have been created from
    // and target to the main thread (so we do not need an explicit Cancel).
    ClearOnShutdown(&sObservers, ShutdownPhase::XPCOMWillShutdown);
  }
  sObservers->LookupOrInsertWith(
      std::make_pair(nsCOMPtr<nsIPrincipal>(aPrincipal), nsCString(aType)),
      [&]() -> nsCOMPtr<nsITimer> {
        // Only create a new observer if we don't have a matching
        // entry in our hashtable.
        nsCOMPtr<nsITimer> timer;
        RefPtr<TemporaryAccessGrantObserver> observer =
            new TemporaryAccessGrantObserver(aPM, aPrincipal, aType);
        nsresult rv = NS_NewTimerWithObserver(getter_AddRefs(timer), observer,
                                              24 * 60 * 60 * 1000,  // 24 hours
                                              nsITimer::TYPE_ONE_SHOT);

        if (NS_SUCCEEDED(rv)) {
          observer->SetTimer(timer);
          return timer;
        }
        timer->Cancel();
        return nullptr;
      });
}

void TemporaryAccessGrantObserver::SetTimer(nsITimer* aTimer) {
  mTimer = aTimer;
}

NS_IMETHODIMP
TemporaryAccessGrantObserver::Observe(nsISupports* aSubject, const char* aTopic,
                                      const char16_t* aData) {
  if (strcmp(aTopic, NS_TIMER_CALLBACK_TOPIC) == 0) {
    Unused << mPM->RemoveFromPrincipal(mPrincipal, mType);

    MOZ_ASSERT(sObservers);
    sObservers->Remove(std::make_pair(mPrincipal, mType));
  }

  return NS_OK;
}

NS_IMETHODIMP
TemporaryAccessGrantObserver::GetName(nsACString& aName) {
  aName.AssignLiteral("TemporaryAccessGrantObserver");
  return NS_OK;
}
