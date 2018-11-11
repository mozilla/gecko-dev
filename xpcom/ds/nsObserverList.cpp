/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsObserverList.h"

#include "nsAutoPtr.h"
#include "nsCOMArray.h"
#include "nsISimpleEnumerator.h"
#include "xpcpublic.h"

nsresult
nsObserverList::AddObserver(nsIObserver* anObserver, bool ownsWeak)
{
  NS_ASSERTION(anObserver, "Null input");

  if (!ownsWeak) {
    ObserverRef* o = mObservers.AppendElement(anObserver);
    if (!o) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    return NS_OK;
  }

  nsCOMPtr<nsIWeakReference> weak = do_GetWeakReference(anObserver);
  if (!weak) {
    return NS_NOINTERFACE;
  }

  ObserverRef* o = mObservers.AppendElement(weak);
  if (!o) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

nsresult
nsObserverList::RemoveObserver(nsIObserver* anObserver)
{
  NS_ASSERTION(anObserver, "Null input");

  if (mObservers.RemoveElement(static_cast<nsISupports*>(anObserver))) {
    return NS_OK;
  }

  nsCOMPtr<nsIWeakReference> observerRef = do_GetWeakReference(anObserver);
  if (!observerRef) {
    return NS_ERROR_FAILURE;
  }

  if (!mObservers.RemoveElement(observerRef)) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

void
nsObserverList::GetObserverList(nsISimpleEnumerator** anEnumerator)
{
  RefPtr<nsObserverEnumerator> e(new nsObserverEnumerator(this));
  e.forget(anEnumerator);
}

void
nsObserverList::FillObserverArray(nsCOMArray<nsIObserver>& aArray)
{
  aArray.SetCapacity(mObservers.Length());

  nsTArray<ObserverRef> observers(mObservers);

  for (int32_t i = observers.Length() - 1; i >= 0; --i) {
    if (observers[i].isWeakRef) {
      nsCOMPtr<nsIObserver> o(do_QueryReferent(observers[i].asWeak()));
      if (o) {
        aArray.AppendObject(o);
      } else {
        // the object has gone away, remove the weakref
        mObservers.RemoveElement(observers[i].asWeak());
      }
    } else {
      aArray.AppendObject(observers[i].asObserver());
    }
  }
}

void
nsObserverList::AppendStrongObservers(nsCOMArray<nsIObserver>& aArray)
{
  aArray.SetCapacity(aArray.Length() + mObservers.Length());

  for (int32_t i = mObservers.Length() - 1; i >= 0; --i) {
    if (!mObservers[i].isWeakRef) {
      aArray.AppendObject(mObservers[i].asObserver());
    }
  }
}

void
nsObserverList::NotifyObservers(nsISupports* aSubject,
                                const char* aTopic,
                                const char16_t* someData)
{
  nsCOMArray<nsIObserver> observers;
  FillObserverArray(observers);

  for (int32_t i = 0; i < observers.Count(); ++i) {
    observers[i]->Observe(aSubject, aTopic, someData);
  }
}

NS_IMPL_ISUPPORTS(nsObserverEnumerator, nsISimpleEnumerator)

nsObserverEnumerator::nsObserverEnumerator(nsObserverList* aObserverList)
  : mIndex(0)
{
  aObserverList->FillObserverArray(mObservers);
}

NS_IMETHODIMP
nsObserverEnumerator::HasMoreElements(bool* aResult)
{
  *aResult = (mIndex < mObservers.Count());
  return NS_OK;
}

NS_IMETHODIMP
nsObserverEnumerator::GetNext(nsISupports** aResult)
{
  if (mIndex == mObservers.Count()) {
    NS_ERROR("Enumerating after HasMoreElements returned false.");
    return NS_ERROR_UNEXPECTED;
  }

  NS_ADDREF(*aResult = mObservers[mIndex]);
  ++mIndex;
  return NS_OK;
}
