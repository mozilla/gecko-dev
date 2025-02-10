/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef places_test_harness_h__
#define places_test_harness_h__

#include "gtest/gtest.h"
#include "mozilla/dom/PlacesEventBinding.h"
#include "nsIWeakReference.h"
#include "nsThreadUtils.h"
#include "nsDocShellCID.h"

#include "nsToolkitCompsCID.h"
#include "nsServiceManagerUtils.h"
#include "nsINavHistoryService.h"
#include "nsIObserverService.h"
#include "nsIThread.h"
#include "nsIURI.h"
#include "mozilla/IHistory.h"
#include "mozilla/SpinEventLoopUntil.h"
#include "mozIStorageConnection.h"
#include "mozIStorageStatement.h"
#include "mozIStorageAsyncStatement.h"
#include "mozIStorageStatementCallback.h"
#include "mozIStoragePendingStatement.h"
#include "nsIObserver.h"
#include "nsIUserIdleService.h"
#include "nsWidgetsCID.h"
#include "prinrval.h"
#include "prtime.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/PlacesEvent.h"
#include "mozilla/dom/PlacesObservers.h"
#include "mozilla/places/INativePlacesEventCallback.h"

using mozilla::dom::PlacesEventType;
using mozilla::dom::PlacesObservers;
using mozilla::places::INativePlacesEventCallback;

#define WAIT_TIMEOUT_USEC (5 * PR_USEC_PER_SEC)

#define do_check_true(aCondition) EXPECT_TRUE(aCondition)

#define do_check_false(aCondition) EXPECT_FALSE(aCondition)

#define do_check_success(aResult) do_check_true(NS_SUCCEEDED(aResult))

#define do_check_eq(aExpected, aActual) do_check_true((aExpected) == (aActual))

struct Test {
  void (*func)(void);
  const char* const name;
};
#define PTEST(aName) {aName, #aName}

#define TEST_INFO_STR "TEST-INFO | "

/**
 * Runs the next text.
 */
void run_next_test();

/**
 * To be used around asynchronous work.
 */
void do_test_pending();
void do_test_finished();

/**
 * Spins current thread until a topic is received.
 */
class WaitForTopicSpinner final : public nsIObserver {
 public:
  NS_DECL_ISUPPORTS

  explicit WaitForTopicSpinner(const char* const aTopic)
      : mTopicReceived(false), mStartTime(PR_IntervalNow()) {
    nsCOMPtr<nsIObserverService> observerService =
        do_GetService(NS_OBSERVERSERVICE_CONTRACTID);
    do_check_true(observerService);
    (void)observerService->AddObserver(this, aTopic, false);
  }

  void Spin() {
    bool timedOut = false;
    mozilla::SpinEventLoopUntil(
        "places:WaitForTopicSpinner::Spin"_ns, [&]() -> bool {
          if (mTopicReceived) {
            return true;
          }

          if ((PR_IntervalNow() - mStartTime) > (WAIT_TIMEOUT_USEC)) {
            timedOut = true;
            return true;
          }

          return false;
        });

    if (timedOut) {
      // Timed out waiting for the topic.
      do_check_true(false);
    }
  }

  NS_IMETHOD Observe(nsISupports* aSubject, const char* aTopic,
                     const char16_t* aData) override {
    mTopicReceived = true;
    nsCOMPtr<nsIObserverService> observerService =
        do_GetService(NS_OBSERVERSERVICE_CONTRACTID);
    do_check_true(observerService);
    (void)observerService->RemoveObserver(this, aTopic);
    return NS_OK;
  }

 private:
  ~WaitForTopicSpinner() = default;

  bool mTopicReceived;
  PRIntervalTime mStartTime;
};

/**
 * Spins current thread until a Places notification is received.
 */
class WaitForNotificationSpinner final : public INativePlacesEventCallback {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(WaitForNotificationSpinner, override)

  explicit WaitForNotificationSpinner(const PlacesEventType aEventType)
      : mEventType(aEventType), mStartTime(PR_IntervalNow()) {
    AutoTArray<PlacesEventType, 1> events;
    events.AppendElement(mEventType);
    PlacesObservers::AddListener(events, this);
  }

  void SpinUntilCompleted() {
    bool timedOut = false;
    mozilla::SpinEventLoopUntil(
        "places::WaitForNotificationSpinner::SpinUntilCompleted"_ns,
        [&]() -> bool {
          if (mEventReceived) {
            return true;
          }

          if ((PR_IntervalNow() - mStartTime) > (WAIT_TIMEOUT_USEC)) {
            timedOut = true;
            return true;
          }

          return false;
        });

    if (timedOut) {
      // Timed out waiting for the notification.
      do_check_true(false);
    }
  }

  void HandlePlacesEvent(const PlacesEventSequence& aEvents) override {
    for (const auto& event : aEvents) {
      if (event->Type() == mEventType) {
        mEventReceived = true;
        AutoTArray<PlacesEventType, 1> events;
        events.AppendElement(mEventType);
        PlacesObservers::RemoveListener(events, this);
        return;
      }
    }
  }

 private:
  ~WaitForNotificationSpinner() = default;

  bool mEventReceived = false;
  PlacesEventType mEventType;
  PRIntervalTime mStartTime;
};

/**
 * Spins current thread until an async statement is executed.
 */
class PlacesAsyncStatementSpinner final : public mozIStorageStatementCallback {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_MOZISTORAGESTATEMENTCALLBACK

  PlacesAsyncStatementSpinner();
  void SpinUntilCompleted();
  uint16_t completionReason;

 protected:
  ~PlacesAsyncStatementSpinner() = default;

  volatile bool mCompleted;
};

using PlaceRecord = struct PlaceRecord {
  int64_t id = -1;
  int32_t hidden = 0;
  int32_t typed = 0;
  int32_t visitCount = 0;
  nsCString guid;
  int64_t frecency = -1;
};

using VisitRecord = struct VisitRecord {
  int64_t id = -1;
  int64_t lastVisitId = -1;
  int32_t transitionType = 0;
};

already_AddRefed<mozilla::IHistory> do_get_IHistory();

already_AddRefed<nsINavHistoryService> do_get_NavHistory();

already_AddRefed<mozIStorageConnection> do_get_db();

/**
 * Get the place record from the database.
 *
 * @param aURI The unique URI of the place we are looking up
 * @param result Out parameter where the result is stored
 */
void do_get_place(nsIURI* aURI, PlaceRecord& result);

/**
 * Gets the most recent visit to a place.
 *
 * @param placeID ID from the moz_places table
 * @param result Out parameter where visit is stored
 */
void do_get_lastVisit(int64_t placeId, VisitRecord& result);

void do_wait_async_updates();

/**
 * Adds a URI to the database.
 *
 * @param aURI
 *        The URI to add to the database.
 */
void addURI(nsIURI* aURI);

static const char TOPIC_PROFILE_CHANGE_QM[] = "profile-before-change-qm";
static const char TOPIC_PLACES_CONNECTION_CLOSED[] = "places-connection-closed";

class WaitForConnectionClosed final : public nsIObserver {
  RefPtr<WaitForTopicSpinner> mSpinner;

  ~WaitForConnectionClosed() = default;

 public:
  NS_DECL_ISUPPORTS

  WaitForConnectionClosed() {
    nsCOMPtr<nsIObserverService> os =
        do_GetService(NS_OBSERVERSERVICE_CONTRACTID);
    MOZ_ASSERT(os);
    if (os) {
      // The places-connection-closed notification happens because of things
      // that occur during profile-before-change, so we use the stage after that
      // to wait for it.
      MOZ_ALWAYS_SUCCEEDS(
          os->AddObserver(this, TOPIC_PROFILE_CHANGE_QM, false));
    }
    mSpinner = new WaitForTopicSpinner(TOPIC_PLACES_CONNECTION_CLOSED);
  }

  NS_IMETHOD Observe(nsISupports* aSubject, const char* aTopic,
                     const char16_t* aData) override {
    nsCOMPtr<nsIObserverService> os =
        do_GetService(NS_OBSERVERSERVICE_CONTRACTID);
    MOZ_ASSERT(os);
    if (os) {
      MOZ_ALWAYS_SUCCEEDS(os->RemoveObserver(this, aTopic));
    }

    mSpinner->Spin();

    return NS_OK;
  }
};

void disable_idle_service();

#endif  // places_test_harness_h__
