/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_places_ConcurrentConnection_h_
#define mozilla_places_ConcurrentConnection_h_

#include "mozilla/storage/StatementCache.h"
#include "mozIStorageCompletionCallback.h"
#include "mozIStorageStatementCallback.h"
#include "Helpers.h"
#include "nsCOMPtr.h"
#include "nsDeque.h"
#include "nsIAsyncShutdown.h"
#include "nsIObserver.h"
#include "nsISupportsImpl.h"
#include "nsWeakReference.h"

namespace mozilla::places {

/**
 * Tracks all the necessary information to asynchronously run a query, and
 * call back once done.
 */
struct PendingQuery final {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(PendingQuery);
  PendingQuery(const nsCString& aSQL, PendingStatementCallback* aCallback)
      : mSQL(aSQL), mCallback(aCallback) {}

  nsCString mSQL;
  RefPtr<PendingStatementCallback> mCallback;

 private:
  ~PendingQuery() = default;
};

/**
 * Wraps a concurrent SQLite connection, that has zero dependencies on Places.
 * This is useful to read from the database without fully initializing the
 * whole Places subsystem, e.g. link coloring, favicons...
 *
 * Since this is lacking any capability of setting up the database file, if it
 * doesn't exist, or has an outdated schema version, it will queue up requests
 * and await for Places to start up fully.
 */
class ConcurrentConnection final : public nsIObserver,
                                   public nsSupportsWeakReference,
                                   public nsIAsyncShutdownBlocker,
                                   public mozIStorageCompletionCallback,
                                   public mozIStorageStatementCallback {
  using StatementCache = mozilla::storage::StatementCache<mozIStorageStatement>;
  using AsyncStatementCache =
      mozilla::storage::StatementCache<mozIStorageAsyncStatement>;

 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOBSERVER
  NS_DECL_NSIASYNCSHUTDOWNBLOCKER
  NS_DECL_MOZISTORAGECOMPLETIONCALLBACK
  NS_DECL_MOZISTORAGESTATEMENTCALLBACK

  ConcurrentConnection();

  /**
   * Used by the Places singleton macro to initializes the instance.
   */
  nsresult Init();

  /**
   * Get the singleton instance of this class. This is how you normally get
   * a handle to this.
   *
   * @returns Singleton instance of this class.
   */
  static already_AddRefed<ConcurrentConnection> GetSingleton();

  /**
   * Enqueue a query or a Runnable.
   * Each consumers should only use one of these for proper serialization.
   * TODO: Unify the queues, maybe using a Union.
   */
  void Queue(const nsCString& aSQL, PendingStatementCallback* aCallback);
  void Queue(Runnable* aRunnable);

  /**
   * Gets a cached synchronous statement on the helper thread.
   *
   * @param aQuery
   *        nsCString of SQL query.
   * @returns The cached statement.
   * @note Always null check the result.
   * @note Always use a scoper to reset the statement.
   */
  already_AddRefed<mozIStorageStatement> GetStatementOnHelperThread(
      const nsCString& aQuery);

 private:
  /**
   * Gets a cached asynchronous statement on the main thread.
   * This is private, as you normally should use Queue.
   *
   * @param aQuery
   *        nsCString of SQL query.
   * @returns The cached statement.
   * @note Always null check the result.
   * @note As this returns an async statement, it's not necessary to use a
   *       scoper, as it will be reset automatically after execution.
   */
  already_AddRefed<mozIStorageAsyncStatement> GetStatement(
      const nsCString& aQuery);

  /**
   * Try to consume the queue.
   */
  void TryToConsumeQueues();

  /**
   * Try to open a database connection.
   * This may arguably fail, for example if the database was not created yet,
   * or has an outdated schema version. In that case this component will try
   * again later, once it is notified the Places subsystem is up and running.
   */
  void TryToOpenConnection();

  /**
   * Setups the connection, initializing functions and attaching other
   * databases.
   */
  void SetupConnection();

  /**
   * Close the currently tracked connection.
   */
  void CloseConnection();
  void CloseConnectionComplete(nsresult rv);

  /**
   * Shutdown and cleanup.
   * @note After invoking this the component cannot be resurrected.
   */
  void Shutdown();

  /**
   * Helper to attach a database file.
   */
  nsresult AttachDatabase(const nsString& aFileName,
                          const nsCString& aSchemaName);

  static ConcurrentConnection* gConcurrentConnection;

  ~ConcurrentConnection() = default;

  // The current state, used to track progress in AsyncShutdown.
  enum States {
    NOT_STARTED = 0,
    AWAITING_DATABASE_READY = 1,
    READY = 2,
    SHUTTING_DOWN = 3,
    AWAITING_DATABASE_CLOSED = 4,
    CLOSED = 5,
  };
  States mState = NOT_STARTED;

  bool mIsOpening = false;
  bool mPlacesIsInitialized = false;
  bool mRetryOpening = true;
  bool mIsShuttingDown = false;
  bool mIsConnectionReady = false;
  int32_t mSchemaVersion = -1;

  // Ideally this should be a mozIStorageAsyncConnection, as that would give us
  // additional checks we're not abusing the main-thread, though that would
  // limit us excessively, since `StatementCache` and `CreateStatement` only
  // work on a full-fledged Connection object. We'll have to take particular
  // care of not touching the main-thread.
  nsCOMPtr<mozIStorageConnection> mConn;

  /**
   * The parent object who registered this as a blocker.
   */
  nsCOMPtr<nsIAsyncShutdownClient> mShutdownBarrierClient;

  /**
   * Collections of queries and runnables to be executed.
   */
  nsRefPtrDeque<PendingQuery> mPendingQueries;
  nsRefPtrDeque<Runnable> mPendingRunnables;

  /**
   * Statements caches.
   */
  UniquePtr<AsyncStatementCache> mAsyncStatements;
  UniquePtr<StatementCache> mHelperThreadStatements;
};

}  // namespace mozilla::places

#endif  // mozilla_places_ConcurrentConnection_h_
