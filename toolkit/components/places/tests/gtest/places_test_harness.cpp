/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "places_test_harness.h"

NS_IMPL_ISUPPORTS(WaitForTopicSpinner, nsIObserver)

NS_IMPL_ISUPPORTS(PlacesAsyncStatementSpinner, mozIStorageStatementCallback)

NS_IMPL_ISUPPORTS(WaitForConnectionClosed, nsIObserver)

already_AddRefed<mozilla::IHistory> do_get_IHistory() {
  nsCOMPtr<mozilla::IHistory> history = do_GetService(NS_IHISTORY_CONTRACTID);
  do_check_true(history);
  return history.forget();
}

already_AddRefed<nsINavHistoryService> do_get_NavHistory() {
  nsCOMPtr<nsINavHistoryService> serv =
      do_GetService(NS_NAVHISTORYSERVICE_CONTRACTID);
  do_check_true(serv);
  return serv.forget();
}

already_AddRefed<mozIStorageConnection> do_get_db() {
  nsCOMPtr<nsINavHistoryService> history = do_get_NavHistory();
  do_check_true(history);

  nsCOMPtr<mozIStorageConnection> dbConn;
  nsresult rv = history->GetDBConnection(getter_AddRefs(dbConn));
  do_check_success(rv);
  return dbConn.forget();
}

PlacesAsyncStatementSpinner::PlacesAsyncStatementSpinner()
    : completionReason(0), mCompleted(false) {}

NS_IMETHODIMP
PlacesAsyncStatementSpinner::HandleResult(mozIStorageResultSet* aResultSet) {
  return NS_OK;
}

NS_IMETHODIMP
PlacesAsyncStatementSpinner::HandleError(mozIStorageError* aError) {
  return NS_OK;
}

NS_IMETHODIMP
PlacesAsyncStatementSpinner::HandleCompletion(uint16_t aReason) {
  completionReason = aReason;
  mCompleted = true;
  return NS_OK;
}

void PlacesAsyncStatementSpinner::SpinUntilCompleted() {
  nsCOMPtr<nsIThread> thread(::do_GetCurrentThread());
  nsresult rv = NS_OK;
  bool processed = true;
  while (!mCompleted && NS_SUCCEEDED(rv)) {
    rv = thread->ProcessNextEvent(true, &processed);
  }
}

/**
 * Get the place record from the database.
 *
 * @param aURI The unique URI of the place we are looking up
 * @param result Out parameter where the result is stored
 */
void do_get_place(nsIURI* aURI, PlaceRecord& result) {
  nsCOMPtr<mozIStorageConnection> dbConn = do_get_db();
  nsCOMPtr<mozIStorageStatement> stmt;

  nsCString spec;
  nsresult rv = aURI->GetSpec(spec);
  do_check_success(rv);

  rv = dbConn->CreateStatement(
      nsLiteralCString("SELECT id, hidden, typed, visit_count, guid, frecency "
                       "FROM moz_places "
                       "WHERE url_hash = hash(?1) AND url = ?1"),
      getter_AddRefs(stmt));
  do_check_success(rv);

  rv = stmt->BindUTF8StringByIndex(0, spec);
  do_check_success(rv);

  bool hasResults;
  rv = stmt->ExecuteStep(&hasResults);
  do_check_success(rv);
  if (!hasResults) {
    result.id = 0;
    return;
  }

  rv = stmt->GetInt64(0, &result.id);
  do_check_success(rv);
  rv = stmt->GetInt32(1, &result.hidden);
  do_check_success(rv);
  rv = stmt->GetInt32(2, &result.typed);
  do_check_success(rv);
  rv = stmt->GetInt32(3, &result.visitCount);
  do_check_success(rv);
  rv = stmt->GetUTF8String(4, result.guid);
  do_check_success(rv);
  rv = stmt->GetInt64(5, &result.frecency);
  do_check_success(rv);
}

/**
 * Gets the most recent visit to a place.
 *
 * @param placeID ID from the moz_places table
 * @param result Out parameter where visit is stored
 */
void do_get_lastVisit(int64_t placeId, VisitRecord& result) {
  nsCOMPtr<mozIStorageConnection> dbConn = do_get_db();
  nsCOMPtr<mozIStorageStatement> stmt;

  nsresult rv = dbConn->CreateStatement(
      nsLiteralCString(
          "SELECT id, from_visit, visit_type FROM moz_historyvisits "
          "WHERE place_id=?1 "
          "LIMIT 1"),
      getter_AddRefs(stmt));
  do_check_success(rv);

  rv = stmt->BindInt64ByIndex(0, placeId);
  do_check_success(rv);

  bool hasResults;
  rv = stmt->ExecuteStep(&hasResults);
  do_check_success(rv);

  if (!hasResults) {
    result.id = 0;
    return;
  }

  rv = stmt->GetInt64(0, &result.id);
  do_check_success(rv);
  rv = stmt->GetInt64(1, &result.lastVisitId);
  do_check_success(rv);
  rv = stmt->GetInt32(2, &result.transitionType);
  do_check_success(rv);
}

void do_wait_async_updates() {
  nsCOMPtr<mozIStorageConnection> db = do_get_db();
  nsCOMPtr<mozIStorageAsyncStatement> stmt;

  db->CreateAsyncStatement("BEGIN EXCLUSIVE"_ns, getter_AddRefs(stmt));
  nsCOMPtr<mozIStoragePendingStatement> pending;
  (void)stmt->ExecuteAsync(nullptr, getter_AddRefs(pending));

  db->CreateAsyncStatement("COMMIT"_ns, getter_AddRefs(stmt));
  RefPtr<PlacesAsyncStatementSpinner> spinner =
      new PlacesAsyncStatementSpinner();
  (void)stmt->ExecuteAsync(spinner, getter_AddRefs(pending));

  spinner->SpinUntilCompleted();
}

/**
 * Adds a URI to the database.
 *
 * @param aURI
 *        The URI to add to the database.
 */
void addURI(nsIURI* aURI) {
  nsCOMPtr<mozilla::IHistory> history = do_GetService(NS_IHISTORY_CONTRACTID);
  do_check_true(history);
  nsresult rv = history->VisitURI(nullptr, aURI, nullptr,
                                  mozilla::IHistory::TOP_LEVEL, 0);
  do_check_success(rv);

  do_wait_async_updates();
}

void disable_idle_service() {
  (void)fprintf(stderr, TEST_INFO_STR "Disabling Idle Service.\n");

  nsCOMPtr<nsIUserIdleService> idle =
      do_GetService("@mozilla.org/widget/useridleservice;1");
  idle->SetDisabled(true);
}
