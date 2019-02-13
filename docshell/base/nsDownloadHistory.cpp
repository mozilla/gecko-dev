/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDownloadHistory.h"
#include "nsCOMPtr.h"
#include "nsServiceManagerUtils.h"
#include "nsIGlobalHistory2.h"
#include "nsIObserverService.h"
#include "nsIURI.h"
#include "mozilla/Services.h"

NS_IMPL_ISUPPORTS(nsDownloadHistory, nsIDownloadHistory)

NS_IMETHODIMP
nsDownloadHistory::AddDownload(nsIURI* aSource,
                               nsIURI* aReferrer,
                               PRTime aStartTime,
                               nsIURI* aDestination)
{
  NS_ENSURE_ARG_POINTER(aSource);

  nsCOMPtr<nsIGlobalHistory2> history =
    do_GetService("@mozilla.org/browser/global-history;2");
  if (!history) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  bool visited;
  nsresult rv = history->IsVisited(aSource, &visited);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = history->AddURI(aSource, false, true, aReferrer);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!visited) {
    nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
    if (os) {
      os->NotifyObservers(aSource, NS_LINK_VISITED_EVENT_TOPIC, nullptr);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDownloadHistory::RemoveAllDownloads()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}
