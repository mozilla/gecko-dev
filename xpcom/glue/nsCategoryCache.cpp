/* vim:set st=2 sts=2 ts=2 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIObserverService.h"
#include "mozilla/Services.h"
#include "nsISupportsPrimitives.h"
#include "nsIStringEnumerator.h"

#include "nsXPCOMCID.h"

#include "nsCategoryCache.h"

nsCategoryObserver::nsCategoryObserver(const char* aCategory)
  : mCategory(aCategory)
  , mObserversRemoved(false)
{
  // First, enumerate the currently existing entries
  nsCOMPtr<nsICategoryManager> catMan =
    do_GetService(NS_CATEGORYMANAGER_CONTRACTID);
  if (!catMan) {
    return;
  }

  nsCOMPtr<nsISimpleEnumerator> enumerator;
  nsresult rv = catMan->EnumerateCategory(aCategory,
                                          getter_AddRefs(enumerator));
  if (NS_FAILED(rv)) {
    return;
  }

  nsCOMPtr<nsIUTF8StringEnumerator> strings = do_QueryInterface(enumerator);
  MOZ_ASSERT(strings);

  bool more;
  while (NS_SUCCEEDED(strings->HasMore(&more)) && more) {
    nsAutoCString entryName;
    strings->GetNext(entryName);

    nsCString entryValue;
    rv = catMan->GetCategoryEntry(aCategory,
                                  entryName.get(),
                                  getter_Copies(entryValue));
    if (NS_SUCCEEDED(rv)) {
      nsCOMPtr<nsISupports> service = do_GetService(entryValue.get());
      if (service) {
        mHash.Put(entryName, service);
      }
    }
  }

  // Now, listen for changes
  nsCOMPtr<nsIObserverService> serv =
    mozilla::services::GetObserverService();
  if (serv) {
    serv->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false);
    serv->AddObserver(this, NS_XPCOM_CATEGORY_ENTRY_ADDED_OBSERVER_ID, false);
    serv->AddObserver(this, NS_XPCOM_CATEGORY_ENTRY_REMOVED_OBSERVER_ID, false);
    serv->AddObserver(this, NS_XPCOM_CATEGORY_CLEARED_OBSERVER_ID, false);
  }
}

nsCategoryObserver::~nsCategoryObserver()
{
}

NS_IMPL_ISUPPORTS(nsCategoryObserver, nsIObserver)

void
nsCategoryObserver::ListenerDied()
{
  RemoveObservers();
}

void
nsCategoryObserver::RemoveObservers()
{
  if (mObserversRemoved) {
    return;
  }

  mObserversRemoved = true;
  nsCOMPtr<nsIObserverService> obsSvc =
    mozilla::services::GetObserverService();
  if (obsSvc) {
    obsSvc->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
    obsSvc->RemoveObserver(this, NS_XPCOM_CATEGORY_ENTRY_ADDED_OBSERVER_ID);
    obsSvc->RemoveObserver(this, NS_XPCOM_CATEGORY_ENTRY_REMOVED_OBSERVER_ID);
    obsSvc->RemoveObserver(this, NS_XPCOM_CATEGORY_CLEARED_OBSERVER_ID);
  }
}

NS_IMETHODIMP
nsCategoryObserver::Observe(nsISupports* aSubject, const char* aTopic,
                            const char16_t* aData)
{
  if (strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID) == 0) {
    mHash.Clear();
    RemoveObservers();

    return NS_OK;
  }

  if (!aData ||
      !nsDependentString(aData).Equals(NS_ConvertASCIItoUTF16(mCategory))) {
    return NS_OK;
  }

  nsAutoCString str;
  nsCOMPtr<nsISupportsCString> strWrapper(do_QueryInterface(aSubject));
  if (strWrapper) {
    strWrapper->GetData(str);
  }

  if (strcmp(aTopic, NS_XPCOM_CATEGORY_ENTRY_ADDED_OBSERVER_ID) == 0) {
    // We may get an add notification even when we already have an entry. This
    // is due to the notification happening asynchronously, so if the entry gets
    // added and an nsCategoryObserver gets instantiated before events get
    // processed, we'd get the notification for an existing entry.
    // Do nothing in that case.
    if (mHash.GetWeak(str)) {
      return NS_OK;
    }

    nsCOMPtr<nsICategoryManager> catMan =
      do_GetService(NS_CATEGORYMANAGER_CONTRACTID);
    if (!catMan) {
      return NS_OK;
    }

    nsCString entryValue;
    catMan->GetCategoryEntry(mCategory.get(),
                             str.get(),
                             getter_Copies(entryValue));

    nsCOMPtr<nsISupports> service = do_GetService(entryValue.get());

    if (service) {
      mHash.Put(str, service);
    }
  } else if (strcmp(aTopic, NS_XPCOM_CATEGORY_ENTRY_REMOVED_OBSERVER_ID) == 0) {
    mHash.Remove(str);
  } else if (strcmp(aTopic, NS_XPCOM_CATEGORY_CLEARED_OBSERVER_ID) == 0) {
    mHash.Clear();
  }
  return NS_OK;
}
