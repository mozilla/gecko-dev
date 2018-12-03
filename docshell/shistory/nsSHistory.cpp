/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSHistory.h"

#include <algorithm>

#include "nsContentUtils.h"
#include "nsCOMArray.h"
#include "nsComponentManagerUtils.h"
#include "nsDocShell.h"
#include "nsIContentViewer.h"
#include "nsIDocShell.h"
#include "nsDocShellLoadState.h"
#include "nsIDocShellTreeItem.h"
#include "nsILayoutHistoryState.h"
#include "nsIObserverService.h"
#include "nsISHEntry.h"
#include "nsISHistoryListener.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsTArray.h"
#include "prsystem.h"

#include "mozilla/Attributes.h"
#include "mozilla/LinkedList.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/dom/TabGroup.h"

using namespace mozilla;

#define PREF_SHISTORY_SIZE "browser.sessionhistory.max_entries"
#define PREF_SHISTORY_MAX_TOTAL_VIEWERS \
  "browser.sessionhistory.max_total_viewers"
#define CONTENT_VIEWER_TIMEOUT_SECONDS \
  "browser.sessionhistory.contentViewerTimeout"

// Default this to time out unused content viewers after 30 minutes
#define CONTENT_VIEWER_TIMEOUT_SECONDS_DEFAULT (30 * 60)

static const char* kObservedPrefs[] = {
    PREF_SHISTORY_SIZE, PREF_SHISTORY_MAX_TOTAL_VIEWERS, nullptr};

static int32_t gHistoryMaxSize = 50;
// List of all SHistory objects, used for content viewer cache eviction
static LinkedList<nsSHistory> gSHistoryList;
// Max viewers allowed total, across all SHistory objects - negative default
// means we will calculate how many viewers to cache based on total memory
int32_t nsSHistory::sHistoryMaxTotalViewers = -1;

// A counter that is used to be able to know the order in which
// entries were touched, so that we can evict older entries first.
static uint32_t gTouchCounter = 0;

static LazyLogModule gSHistoryLog("nsSHistory");

#define LOG(format) MOZ_LOG(gSHistoryLog, mozilla::LogLevel::Debug, format)

// This macro makes it easier to print a log message which includes a URI's
// spec.  Example use:
//
//  nsIURI *uri = [...];
//  LOG_SPEC(("The URI is %s.", _spec), uri);
//
#define LOG_SPEC(format, uri)                             \
  PR_BEGIN_MACRO                                          \
  if (MOZ_LOG_TEST(gSHistoryLog, LogLevel::Debug)) {      \
    nsAutoCString _specStr(NS_LITERAL_CSTRING("(null)")); \
    if (uri) {                                            \
      _specStr = uri->GetSpecOrDefault();                 \
    }                                                     \
    const char* _spec = _specStr.get();                   \
    LOG(format);                                          \
  }                                                       \
  PR_END_MACRO

// This macro makes it easy to log a message including an SHEntry's URI.
// For example:
//
//  nsCOMPtr<nsISHEntry> shentry = [...];
//  LOG_SHENTRY_SPEC(("shentry %p has uri %s.", shentry.get(), _spec), shentry);
//
#define LOG_SHENTRY_SPEC(format, shentry)            \
  PR_BEGIN_MACRO                                     \
  if (MOZ_LOG_TEST(gSHistoryLog, LogLevel::Debug)) { \
    nsCOMPtr<nsIURI> uri = shentry->GetURI();        \
    LOG_SPEC(format, uri);                           \
  }                                                  \
  PR_END_MACRO

// Iterates over all registered session history listeners.
#define ITERATE_LISTENERS(body)                                              \
  PR_BEGIN_MACRO {                                                           \
    nsAutoTObserverArray<nsWeakPtr, 2>::EndLimitedIterator iter(mListeners); \
    while (iter.HasMore()) {                                                 \
      nsCOMPtr<nsISHistoryListener> listener =                               \
          do_QueryReferent(iter.GetNext());                                  \
      if (listener) {                                                        \
        body                                                                 \
      }                                                                      \
    }                                                                        \
  }                                                                          \
  PR_END_MACRO

// Calls a given method on all registered session history listeners.
#define NOTIFY_LISTENERS(method, args) \
  ITERATE_LISTENERS(listener->method args;);

// Calls a given method on all registered session history listeners.
// Listeners may return 'false' to cancel an action so make sure that we
// set the return value to 'false' if one of the listeners wants to cancel.
#define NOTIFY_LISTENERS_CANCELABLE(method, retval, args) \
  PR_BEGIN_MACRO {                                        \
    bool canceled = false;                                \
    retval = true;                                        \
    ITERATE_LISTENERS(listener->method args;              \
                      if (!retval) { canceled = true; }); \
    if (canceled) {                                       \
      retval = false;                                     \
    }                                                     \
  }                                                       \
  PR_END_MACRO

enum HistCmd { HIST_CMD_GOTOINDEX, HIST_CMD_RELOAD };

class nsSHistoryObserver final : public nsIObserver {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  nsSHistoryObserver() {}

  void PrefChanged(const char* aPref);

 protected:
  ~nsSHistoryObserver() {}
};

StaticRefPtr<nsSHistoryObserver> gObserver;

NS_IMPL_ISUPPORTS(nsSHistoryObserver, nsIObserver)

void nsSHistoryObserver::PrefChanged(const char* aPref) {
  nsSHistory::UpdatePrefs();
  nsSHistory::GloballyEvictContentViewers();
}

NS_IMETHODIMP
nsSHistoryObserver::Observe(nsISupports* aSubject, const char* aTopic,
                            const char16_t* aData) {
  if (!strcmp(aTopic, "cacheservice:empty-cache") ||
      !strcmp(aTopic, "memory-pressure")) {
    nsSHistory::GloballyEvictAllContentViewers();
  }

  return NS_OK;
}

namespace {

already_AddRefed<nsIContentViewer> GetContentViewerForEntry(
    nsISHEntry* aEntry) {
  nsCOMPtr<nsISHEntry> ownerEntry;
  nsCOMPtr<nsIContentViewer> viewer;
  aEntry->GetAnyContentViewer(getter_AddRefs(ownerEntry),
                              getter_AddRefs(viewer));
  return viewer.forget();
}

}  // namespace

void nsSHistory::EvictContentViewerForEntry(nsISHEntry* aEntry) {
  nsCOMPtr<nsIContentViewer> viewer;
  nsCOMPtr<nsISHEntry> ownerEntry;
  aEntry->GetAnyContentViewer(getter_AddRefs(ownerEntry),
                              getter_AddRefs(viewer));
  if (viewer) {
    NS_ASSERTION(ownerEntry, "Content viewer exists but its SHEntry is null");

    LOG_SHENTRY_SPEC(("Evicting content viewer 0x%p for "
                      "owning SHEntry 0x%p at %s.",
                      viewer.get(), ownerEntry.get(), _spec),
                     ownerEntry);

    // Drop the presentation state before destroying the viewer, so that
    // document teardown is able to correctly persist the state.
    ownerEntry->SetContentViewer(nullptr);
    ownerEntry->SyncPresentationState();
    viewer->Destroy();
  }

  // When dropping bfcache, we have to remove associated dynamic entries as
  // well.
  int32_t index = GetIndexOfEntry(aEntry);
  if (index != -1) {
    RemoveDynEntries(index, aEntry);
  }
}

nsSHistory::nsSHistory()
    : mIndex(-1), mRequestedIndex(-1), mRootDocShell(nullptr) {
  // Add this new SHistory object to the list
  gSHistoryList.insertBack(this);
}

nsSHistory::~nsSHistory() {}

NS_IMPL_ADDREF(nsSHistory)
NS_IMPL_RELEASE(nsSHistory)

NS_INTERFACE_MAP_BEGIN(nsSHistory)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsISHistory)
  NS_INTERFACE_MAP_ENTRY(nsISHistory)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END

// static
uint32_t nsSHistory::CalcMaxTotalViewers() {
// This value allows tweaking how fast the allowed amount of content viewers
// grows with increasing amounts of memory. Larger values mean slower growth.
#ifdef ANDROID
#define MAX_TOTAL_VIEWERS_BIAS 15.9
#else
#define MAX_TOTAL_VIEWERS_BIAS 14
#endif

  // Calculate an estimate of how many ContentViewers we should cache based
  // on RAM.  This assumes that the average ContentViewer is 4MB (conservative)
  // and caps the max at 8 ContentViewers
  //
  // TODO: Should we split the cache memory betw. ContentViewer caching and
  // nsCacheService?
  //
  // RAM    | ContentViewers | on Android
  // -------------------------------------
  // 32   Mb       0                0
  // 64   Mb       1                0
  // 128  Mb       2                0
  // 256  Mb       3                1
  // 512  Mb       5                2
  // 768  Mb       6                2
  // 1024 Mb       8                3
  // 2048 Mb       8                5
  // 3072 Mb       8                7
  // 4096 Mb       8                8
  uint64_t bytes = PR_GetPhysicalMemorySize();

  if (bytes == 0) {
    return 0;
  }

  // Conversion from unsigned int64_t to double doesn't work on all platforms.
  // We need to truncate the value at INT64_MAX to make sure we don't
  // overflow.
  if (bytes > INT64_MAX) {
    bytes = INT64_MAX;
  }

  double kBytesD = (double)(bytes >> 10);

  // This is essentially the same calculation as for nsCacheService,
  // except that we divide the final memory calculation by 4, since
  // we assume each ContentViewer takes on average 4MB
  uint32_t viewers = 0;
  double x = std::log(kBytesD) / std::log(2.0) - MAX_TOTAL_VIEWERS_BIAS;
  if (x > 0) {
    viewers = (uint32_t)(x * x - x + 2.001);  // add .001 for rounding
    viewers /= 4;
  }

  // Cap it off at 8 max
  if (viewers > 8) {
    viewers = 8;
  }
  return viewers;
}

// static
void nsSHistory::UpdatePrefs() {
  Preferences::GetInt(PREF_SHISTORY_SIZE, &gHistoryMaxSize);
  Preferences::GetInt(PREF_SHISTORY_MAX_TOTAL_VIEWERS,
                      &sHistoryMaxTotalViewers);
  // If the pref is negative, that means we calculate how many viewers
  // we think we should cache, based on total memory
  if (sHistoryMaxTotalViewers < 0) {
    sHistoryMaxTotalViewers = CalcMaxTotalViewers();
  }
}

// static
nsresult nsSHistory::Startup() {
  UpdatePrefs();

  // The goal of this is to unbreak users who have inadvertently set their
  // session history size to less than the default value.
  int32_t defaultHistoryMaxSize =
      Preferences::GetInt(PREF_SHISTORY_SIZE, 50, PrefValueKind::Default);
  if (gHistoryMaxSize < defaultHistoryMaxSize) {
    gHistoryMaxSize = defaultHistoryMaxSize;
  }

  // Allow the user to override the max total number of cached viewers,
  // but keep the per SHistory cached viewer limit constant
  if (!gObserver) {
    gObserver = new nsSHistoryObserver();
    Preferences::RegisterCallbacks(
        PREF_CHANGE_METHOD(nsSHistoryObserver::PrefChanged), kObservedPrefs,
        gObserver.get());

    nsCOMPtr<nsIObserverService> obsSvc =
        mozilla::services::GetObserverService();
    if (obsSvc) {
      // Observe empty-cache notifications so tahat clearing the disk/memory
      // cache will also evict all content viewers.
      obsSvc->AddObserver(gObserver, "cacheservice:empty-cache", false);

      // Same for memory-pressure notifications
      obsSvc->AddObserver(gObserver, "memory-pressure", false);
    }
  }

  return NS_OK;
}

// static
void nsSHistory::Shutdown() {
  if (gObserver) {
    Preferences::UnregisterCallbacks(
        PREF_CHANGE_METHOD(nsSHistoryObserver::PrefChanged), kObservedPrefs,
        gObserver.get());

    nsCOMPtr<nsIObserverService> obsSvc =
        mozilla::services::GetObserverService();
    if (obsSvc) {
      obsSvc->RemoveObserver(gObserver, "cacheservice:empty-cache");
      obsSvc->RemoveObserver(gObserver, "memory-pressure");
    }
    gObserver = nullptr;
  }
}

// static
nsISHEntry* nsSHistory::GetRootSHEntry(nsISHEntry* aEntry) {
  nsCOMPtr<nsISHEntry> rootEntry = aEntry;
  nsISHEntry* result = nullptr;
  while (rootEntry) {
    result = rootEntry;
    rootEntry = result->GetParent();
  }

  return result;
}

// static
nsresult nsSHistory::WalkHistoryEntries(nsISHEntry* aRootEntry,
                                        nsDocShell* aRootShell,
                                        WalkHistoryEntriesFunc aCallback,
                                        void* aData) {
  NS_ENSURE_TRUE(aRootEntry, NS_ERROR_FAILURE);

  int32_t childCount = aRootEntry->GetChildCount();
  for (int32_t i = 0; i < childCount; i++) {
    nsCOMPtr<nsISHEntry> childEntry;
    aRootEntry->GetChildAt(i, getter_AddRefs(childEntry));
    if (!childEntry) {
      // childEntry can be null for valid reasons, for example if the
      // docshell at index i never loaded anything useful.
      // Remember to clone also nulls in the child array (bug 464064).
      aCallback(nullptr, nullptr, i, aData);
      continue;
    }

    nsDocShell* childShell = nullptr;
    if (aRootShell) {
      // Walk the children of aRootShell and see if one of them
      // has srcChild as a SHEntry.
      int32_t length;
      aRootShell->GetChildCount(&length);
      for (int32_t i = 0; i < length; i++) {
        nsCOMPtr<nsIDocShellTreeItem> item;
        nsresult rv = aRootShell->GetChildAt(i, getter_AddRefs(item));
        NS_ENSURE_SUCCESS(rv, rv);
        nsDocShell* child = static_cast<nsDocShell*>(item.get());
        if (child->HasHistoryEntry(childEntry)) {
          childShell = child;
          break;
        }
      }
    }
    nsresult rv = aCallback(childEntry, childShell, i, aData);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

// callback data for WalkHistoryEntries
struct MOZ_STACK_CLASS CloneAndReplaceData {
  CloneAndReplaceData(uint32_t aCloneID, nsISHEntry* aReplaceEntry,
                      bool aCloneChildren, nsISHEntry* aDestTreeParent)
      : cloneID(aCloneID),
        cloneChildren(aCloneChildren),
        replaceEntry(aReplaceEntry),
        destTreeParent(aDestTreeParent) {}

  uint32_t cloneID;
  bool cloneChildren;
  nsISHEntry* replaceEntry;
  nsISHEntry* destTreeParent;
  nsCOMPtr<nsISHEntry> resultEntry;
};

// static
nsresult nsSHistory::CloneAndReplaceChild(nsISHEntry* aEntry,
                                          nsDocShell* aShell,
                                          int32_t aEntryIndex, void* aData) {
  nsCOMPtr<nsISHEntry> dest;

  CloneAndReplaceData* data = static_cast<CloneAndReplaceData*>(aData);
  uint32_t cloneID = data->cloneID;
  nsISHEntry* replaceEntry = data->replaceEntry;

  if (!aEntry) {
    if (data->destTreeParent) {
      data->destTreeParent->AddChild(nullptr, aEntryIndex);
    }
    return NS_OK;
  }

  uint32_t srcID = aEntry->GetID();

  nsresult rv = NS_OK;
  if (srcID == cloneID) {
    // Replace the entry
    dest = replaceEntry;
  } else {
    // Clone the SHEntry...
    rv = aEntry->Clone(getter_AddRefs(dest));
    NS_ENSURE_SUCCESS(rv, rv);
  }
  dest->SetIsSubFrame(true);

  if (srcID != cloneID || data->cloneChildren) {
    // Walk the children
    CloneAndReplaceData childData(cloneID, replaceEntry, data->cloneChildren,
                                  dest);
    rv = WalkHistoryEntries(aEntry, aShell, CloneAndReplaceChild, &childData);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (srcID != cloneID && aShell) {
    aShell->SwapHistoryEntries(aEntry, dest);
  }

  if (data->destTreeParent) {
    data->destTreeParent->AddChild(dest, aEntryIndex);
  }

  data->resultEntry = dest;
  return rv;
}

// static
nsresult nsSHistory::CloneAndReplace(nsISHEntry* aSrcEntry,
                                     nsDocShell* aSrcShell, uint32_t aCloneID,
                                     nsISHEntry* aReplaceEntry,
                                     bool aCloneChildren,
                                     nsISHEntry** aResultEntry) {
  NS_ENSURE_ARG_POINTER(aResultEntry);
  NS_ENSURE_TRUE(aReplaceEntry, NS_ERROR_FAILURE);

  CloneAndReplaceData data(aCloneID, aReplaceEntry, aCloneChildren, nullptr);
  nsresult rv = CloneAndReplaceChild(aSrcEntry, aSrcShell, 0, &data);

  data.resultEntry.swap(*aResultEntry);
  return rv;
}

// static
nsresult nsSHistory::SetChildHistoryEntry(nsISHEntry* aEntry,
                                          nsDocShell* aShell,
                                          int32_t aEntryIndex, void* aData) {
  SwapEntriesData* data = static_cast<SwapEntriesData*>(aData);
  nsDocShell* ignoreShell = data->ignoreShell;

  if (!aShell || aShell == ignoreShell) {
    return NS_OK;
  }

  nsISHEntry* destTreeRoot = data->destTreeRoot;

  nsCOMPtr<nsISHEntry> destEntry;

  if (data->destTreeParent) {
    // aEntry is a clone of some child of destTreeParent, but since the
    // trees aren't necessarily in sync, we'll have to locate it.
    // Note that we could set aShell's entry to null if we don't find a
    // corresponding entry under destTreeParent.

    uint32_t targetID = aEntry->GetID();

    // First look at the given index, since this is the common case.
    nsCOMPtr<nsISHEntry> entry;
    data->destTreeParent->GetChildAt(aEntryIndex, getter_AddRefs(entry));
    if (entry && entry->GetID() == targetID) {
      destEntry.swap(entry);
    } else {
      int32_t childCount;
      data->destTreeParent->GetChildCount(&childCount);
      for (int32_t i = 0; i < childCount; ++i) {
        data->destTreeParent->GetChildAt(i, getter_AddRefs(entry));
        if (!entry) {
          continue;
        }

        if (entry->GetID() == targetID) {
          destEntry.swap(entry);
          break;
        }
      }
    }
  } else {
    destEntry = destTreeRoot;
  }

  aShell->SwapHistoryEntries(aEntry, destEntry);

  // Now handle the children of aEntry.
  SwapEntriesData childData = {ignoreShell, destTreeRoot, destEntry};
  return WalkHistoryEntries(aEntry, aShell, SetChildHistoryEntry, &childData);
}

/* Add an entry to the History list at mIndex and
 * increment the index to point to the new entry
 */
NS_IMETHODIMP
nsSHistory::AddEntry(nsISHEntry* aSHEntry, bool aPersist) {
  NS_ENSURE_ARG(aSHEntry);

  nsCOMPtr<nsISHistory> shistoryOfEntry = aSHEntry->GetSHistory();
  if (shistoryOfEntry && shistoryOfEntry != this) {
    NS_WARNING(
        "The entry has been associated to another nsISHistory instance. "
        "Try nsISHEntry.clone() and nsISHEntry.abandonBFCacheEntry() "
        "first if you're copying an entry from another nsISHistory.");
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsISHEntry> currentTxn;
  if (mIndex >= 0) {
    nsresult rv = GetEntryAtIndex(mIndex, getter_AddRefs(currentTxn));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  aSHEntry->SetSHistory(this);

  // If we have a root docshell, update the docshell id of the root shentry to
  // match the id of that docshell
  if (mRootDocShell) {
    nsID docshellID = mRootDocShell->HistoryID();
    aSHEntry->SetDocshellID(&docshellID);
  }

  if (currentTxn && !currentTxn->GetPersist()) {
    NOTIFY_LISTENERS(OnHistoryReplaceEntry, (mIndex));
    aSHEntry->SetPersist(aPersist);
    mEntries[mIndex] = aSHEntry;
    return NS_OK;
  }

  nsCOMPtr<nsIURI> uri = aSHEntry->GetURI();
  NOTIFY_LISTENERS(OnHistoryNewEntry, (uri, mIndex));

  // Remove all entries after the current one, add the new one, and set the new
  // one as the current one.
  MOZ_ASSERT(mIndex >= -1);
  aSHEntry->SetPersist(aPersist);
  mEntries.TruncateLength(mIndex + 1);
  mEntries.AppendElement(aSHEntry);
  mIndex++;

  // Purge History list if it is too long
  if (gHistoryMaxSize >= 0 && Length() > gHistoryMaxSize) {
    PurgeHistory(Length() - gHistoryMaxSize);
  }

  return NS_OK;
}

/* Get size of the history list */
NS_IMETHODIMP
nsSHistory::GetCount(int32_t* aResult) {
  MOZ_ASSERT(aResult, "null out param?");
  *aResult = Length();
  return NS_OK;
}

NS_IMETHODIMP
nsSHistory::GetIndex(int32_t* aResult) {
  MOZ_ASSERT(aResult, "null out param?");
  *aResult = mIndex;
  return NS_OK;
}

NS_IMETHODIMP
nsSHistory::SetIndex(int32_t aIndex) {
  if (aIndex < 0 || aIndex >= Length()) {
    return NS_ERROR_FAILURE;
  }

  mIndex = aIndex;
  return NS_OK;
}

/* Get the requestedIndex */
NS_IMETHODIMP
nsSHistory::GetRequestedIndex(int32_t* aResult) {
  MOZ_ASSERT(aResult, "null out param?");
  *aResult = mRequestedIndex;
  return NS_OK;
}

NS_IMETHODIMP
nsSHistory::GetEntryAtIndex(int32_t aIndex, nsISHEntry** aResult) {
  NS_ENSURE_ARG_POINTER(aResult);

  if (aIndex < 0 || aIndex >= Length()) {
    return NS_ERROR_FAILURE;
  }

  *aResult = mEntries[aIndex];
  NS_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP_(int32_t)
nsSHistory::GetIndexOfEntry(nsISHEntry* aSHEntry) {
  for (int32_t i = 0; i < Length(); i++) {
    if (aSHEntry == mEntries[i]) {
      return i;
    }
  }

  return -1;
}

#ifdef DEBUG
nsresult nsSHistory::PrintHistory() {
  for (int32_t i = 0; i < Length(); i++) {
    nsCOMPtr<nsISHEntry> entry = mEntries[i];
    nsCOMPtr<nsILayoutHistoryState> layoutHistoryState =
        entry->GetLayoutHistoryState();
    nsCOMPtr<nsIURI> uri = entry->GetURI();
    nsString title;
    entry->GetTitle(title);

#if 0
    nsAutoCString url;
    if (uri) {
      uri->GetSpec(url);
    }

    printf("**** SH Entry #%d: %x\n", i, entry.get());
    printf("\t\t URL = %s\n", url.get());

    printf("\t\t Title = %s\n", NS_LossyConvertUTF16toASCII(title).get());
    printf("\t\t layout History Data = %x\n", layoutHistoryState.get());
#endif
  }

  return NS_OK;
}
#endif

void nsSHistory::WindowIndices(int32_t aIndex, int32_t* aOutStartIndex,
                               int32_t* aOutEndIndex) {
  *aOutStartIndex = std::max(0, aIndex - nsSHistory::VIEWER_WINDOW);
  *aOutEndIndex = std::min(Length() - 1, aIndex + nsSHistory::VIEWER_WINDOW);
}

NS_IMETHODIMP
nsSHistory::PurgeHistory(int32_t aNumEntries) {
  if (Length() <= 0 || aNumEntries <= 0) {
    return NS_ERROR_FAILURE;
  }

  aNumEntries = std::min(aNumEntries, Length());

  NOTIFY_LISTENERS(OnHistoryPurge, (aNumEntries));

  // Remove the first `aNumEntries` entries.
  mEntries.RemoveElementsAt(0, aNumEntries);

  // Adjust the indices, but don't let them go below -1.
  mIndex -= aNumEntries;
  mIndex = std::max(mIndex, -1);
  mRequestedIndex -= aNumEntries;
  mRequestedIndex = std::max(mRequestedIndex, -1);

  if (mRootDocShell) {
    mRootDocShell->HistoryPurged(aNumEntries);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsSHistory::AddSHistoryListener(nsISHistoryListener* aListener) {
  NS_ENSURE_ARG_POINTER(aListener);

  // Check if the listener supports Weak Reference. This is a must.
  // This listener functionality is used by embedders and we want to
  // have the right ownership with who ever listens to SHistory
  nsWeakPtr listener = do_GetWeakReference(aListener);
  if (!listener) {
    return NS_ERROR_FAILURE;
  }

  mListeners.AppendElementUnlessExists(listener);
  return NS_OK;
}

NS_IMETHODIMP
nsSHistory::RemoveSHistoryListener(nsISHistoryListener* aListener) {
  // Make sure the listener that wants to be removed is the
  // one we have in store.
  nsWeakPtr listener = do_GetWeakReference(aListener);
  mListeners.RemoveElement(listener);
  return NS_OK;
}

/* Replace an entry in the History list at a particular index.
 * Do not update index or count.
 */
NS_IMETHODIMP
nsSHistory::ReplaceEntry(int32_t aIndex, nsISHEntry* aReplaceEntry) {
  NS_ENSURE_ARG(aReplaceEntry);

  if (aIndex < 0 || aIndex >= Length()) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsISHistory> shistoryOfEntry = aReplaceEntry->GetSHistory();
  if (shistoryOfEntry && shistoryOfEntry != this) {
    NS_WARNING(
        "The entry has been associated to another nsISHistory instance. "
        "Try nsISHEntry.clone() and nsISHEntry.abandonBFCacheEntry() "
        "first if you're copying an entry from another nsISHistory.");
    return NS_ERROR_FAILURE;
  }

  aReplaceEntry->SetSHistory(this);

  NOTIFY_LISTENERS(OnHistoryReplaceEntry, (aIndex));

  aReplaceEntry->SetPersist(true);
  mEntries[aIndex] = aReplaceEntry;

  return NS_OK;
}

NS_IMETHODIMP
nsSHistory::NotifyOnHistoryReload(nsIURI* aReloadURI, uint32_t aReloadFlags,
                                  bool* aCanReload) {
  NOTIFY_LISTENERS_CANCELABLE(OnHistoryReload, *aCanReload,
                              (aReloadURI, aReloadFlags, aCanReload));
  return NS_OK;
}

NS_IMETHODIMP
nsSHistory::EvictOutOfRangeContentViewers(int32_t aIndex) {
  // Check our per SHistory object limit in the currently navigated SHistory
  EvictOutOfRangeWindowContentViewers(aIndex);
  // Check our total limit across all SHistory objects
  GloballyEvictContentViewers();
  return NS_OK;
}

NS_IMETHODIMP
nsSHistory::EvictAllContentViewers() {
  // XXXbz we don't actually do a good job of evicting things as we should, so
  // we might have viewers quite far from mIndex.  So just evict everything.
  for (int32_t i = 0; i < Length(); i++) {
    EvictContentViewerForEntry(mEntries[i]);
  }

  return NS_OK;
}

nsresult nsSHistory::Reload(uint32_t aReloadFlags) {
  uint32_t loadType;
  if (aReloadFlags & nsIWebNavigation::LOAD_FLAGS_BYPASS_PROXY &&
      aReloadFlags & nsIWebNavigation::LOAD_FLAGS_BYPASS_CACHE) {
    loadType = LOAD_RELOAD_BYPASS_PROXY_AND_CACHE;
  } else if (aReloadFlags & nsIWebNavigation::LOAD_FLAGS_BYPASS_PROXY) {
    loadType = LOAD_RELOAD_BYPASS_PROXY;
  } else if (aReloadFlags & nsIWebNavigation::LOAD_FLAGS_BYPASS_CACHE) {
    loadType = LOAD_RELOAD_BYPASS_CACHE;
  } else if (aReloadFlags & nsIWebNavigation::LOAD_FLAGS_CHARSET_CHANGE) {
    loadType = LOAD_RELOAD_CHARSET_CHANGE;
  } else if (aReloadFlags & nsIWebNavigation::LOAD_FLAGS_ALLOW_MIXED_CONTENT) {
    loadType = LOAD_RELOAD_ALLOW_MIXED_CONTENT;
  } else {
    loadType = LOAD_RELOAD_NORMAL;
  }

  // We are reloading. Send Reload notifications.
  // nsDocShellLoadFlagType is not public, where as nsIWebNavigation
  // is public. So send the reload notifications with the
  // nsIWebNavigation flags.
  bool canNavigate = true;
  nsCOMPtr<nsIURI> currentURI;
  GetCurrentURI(getter_AddRefs(currentURI));
  NOTIFY_LISTENERS_CANCELABLE(OnHistoryReload, canNavigate,
                              (currentURI, aReloadFlags, &canNavigate));
  if (!canNavigate) {
    return NS_OK;
  }

  return LoadEntry(mIndex, loadType, HIST_CMD_RELOAD);
}

NS_IMETHODIMP
nsSHistory::ReloadCurrentEntry() {
  // Notify listeners
  nsCOMPtr<nsIURI> currentURI;
  GetCurrentURI(getter_AddRefs(currentURI));
  NOTIFY_LISTENERS(OnHistoryGotoIndex, (mIndex, currentURI));

  return LoadEntry(mIndex, LOAD_HISTORY, HIST_CMD_RELOAD);
}

void nsSHistory::EvictOutOfRangeWindowContentViewers(int32_t aIndex) {
  // XXX rename method to EvictContentViewersExceptAroundIndex, or something.

  // We need to release all content viewers that are no longer in the range
  //
  //  aIndex - VIEWER_WINDOW to aIndex + VIEWER_WINDOW
  //
  // to ensure that this SHistory object isn't responsible for more than
  // VIEWER_WINDOW content viewers.  But our job is complicated by the
  // fact that two entries which are related by either hash navigations or
  // history.pushState will have the same content viewer.
  //
  // To illustrate the issue, suppose VIEWER_WINDOW = 3 and we have four
  // linked entries in our history.  Suppose we then add a new content
  // viewer and call into this function.  So the history looks like:
  //
  //   A A A A B
  //     +     *
  //
  // where the letters are content viewers and + and * denote the beginning and
  // end of the range aIndex +/- VIEWER_WINDOW.
  //
  // Although one copy of the content viewer A exists outside the range, we
  // don't want to evict A, because it has other copies in range!
  //
  // We therefore adjust our eviction strategy to read:
  //
  //   Evict each content viewer outside the range aIndex -/+
  //   VIEWER_WINDOW, unless that content viewer also appears within the
  //   range.
  //
  // (Note that it's entirely legal to have two copies of one content viewer
  // separated by a different content viewer -- call pushState twice, go back
  // once, and refresh -- so we can't rely on identical viewers only appearing
  // adjacent to one another.)

  if (aIndex < 0) {
    return;
  }
  NS_ENSURE_TRUE_VOID(aIndex < Length());

  // Calculate the range that's safe from eviction.
  int32_t startSafeIndex, endSafeIndex;
  WindowIndices(aIndex, &startSafeIndex, &endSafeIndex);

  LOG(
      ("EvictOutOfRangeWindowContentViewers(index=%d), "
       "Length()=%d. Safe range [%d, %d]",
       aIndex, Length(), startSafeIndex, endSafeIndex));

  // The content viewers in range aIndex -/+ VIEWER_WINDOW will not be
  // evicted.  Collect a set of them so we don't accidentally evict one of them
  // if it appears outside this range.
  nsCOMArray<nsIContentViewer> safeViewers;
  for (int32_t i = startSafeIndex; i <= endSafeIndex; i++) {
    nsCOMPtr<nsIContentViewer> viewer = GetContentViewerForEntry(mEntries[i]);
    safeViewers.AppendObject(viewer);
  }

  // Walk the SHistory list and evict any content viewers that aren't safe.
  // (It's important that the condition checks Length(), rather than a cached
  // copy of Length(), because the length might change between iterations.)
  for (int32_t i = 0; i < Length(); i++) {
    nsCOMPtr<nsISHEntry> entry = mEntries[i];
    nsCOMPtr<nsIContentViewer> viewer = GetContentViewerForEntry(entry);
    if (safeViewers.IndexOf(viewer) == -1) {
      EvictContentViewerForEntry(entry);
    }
  }
}

namespace {

class EntryAndDistance {
 public:
  EntryAndDistance(nsSHistory* aSHistory, nsISHEntry* aEntry, uint32_t aDist)
      : mSHistory(aSHistory),
        mEntry(aEntry),
        mLastTouched(0),
        mDistance(aDist) {
    mViewer = GetContentViewerForEntry(aEntry);
    NS_ASSERTION(mViewer, "Entry should have a content viewer");

    mLastTouched = mEntry->GetLastTouched();
  }

  bool operator<(const EntryAndDistance& aOther) const {
    // Compare distances first, and fall back to last-accessed times.
    if (aOther.mDistance != this->mDistance) {
      return this->mDistance < aOther.mDistance;
    }

    return this->mLastTouched < aOther.mLastTouched;
  }

  bool operator==(const EntryAndDistance& aOther) const {
    // This is a little silly; we need == so the default comaprator can be
    // instantiated, but this function is never actually called when we sort
    // the list of EntryAndDistance objects.
    return aOther.mDistance == this->mDistance &&
           aOther.mLastTouched == this->mLastTouched;
  }

  RefPtr<nsSHistory> mSHistory;
  nsCOMPtr<nsISHEntry> mEntry;
  nsCOMPtr<nsIContentViewer> mViewer;
  uint32_t mLastTouched;
  int32_t mDistance;
};

}  // namespace

// static
void nsSHistory::GloballyEvictContentViewers() {
  // First, collect from each SHistory object the entries which have a cached
  // content viewer. Associate with each entry its distance from its SHistory's
  // current index.

  nsTArray<EntryAndDistance> entries;

  for (auto shist : gSHistoryList) {
    // Maintain a list of the entries which have viewers and belong to
    // this particular shist object.  We'll add this list to the global list,
    // |entries|, eventually.
    nsTArray<EntryAndDistance> shEntries;

    // Content viewers are likely to exist only within shist->mIndex -/+
    // VIEWER_WINDOW, so only search within that range.
    //
    // A content viewer might exist outside that range due to either:
    //
    //   * history.pushState or hash navigations, in which case a copy of the
    //     content viewer should exist within the range, or
    //
    //   * bugs which cause us not to call nsSHistory::EvictContentViewers()
    //     often enough.  Once we do call EvictContentViewers() for the
    //     SHistory object in question, we'll do a full search of its history
    //     and evict the out-of-range content viewers, so we don't bother here.
    //
    int32_t startIndex, endIndex;
    shist->WindowIndices(shist->mIndex, &startIndex, &endIndex);
    for (int32_t i = startIndex; i <= endIndex; i++) {
      nsCOMPtr<nsISHEntry> entry = shist->mEntries[i];
      nsCOMPtr<nsIContentViewer> contentViewer =
          GetContentViewerForEntry(entry);

      if (contentViewer) {
        // Because one content viewer might belong to multiple SHEntries, we
        // have to search through shEntries to see if we already know
        // about this content viewer.  If we find the viewer, update its
        // distance from the SHistory's index and continue.
        bool found = false;
        for (uint32_t j = 0; j < shEntries.Length(); j++) {
          EntryAndDistance& container = shEntries[j];
          if (container.mViewer == contentViewer) {
            container.mDistance =
                std::min(container.mDistance, DeprecatedAbs(i - shist->mIndex));
            found = true;
            break;
          }
        }

        // If we didn't find a EntryAndDistance for this content viewer, make a
        // new one.
        if (!found) {
          EntryAndDistance container(shist, entry,
                                     DeprecatedAbs(i - shist->mIndex));
          shEntries.AppendElement(container);
        }
      }
    }

    // We've found all the entries belonging to shist which have viewers.
    // Add those entries to our global list and move on.
    entries.AppendElements(shEntries);
  }

  // We now have collected all cached content viewers.  First check that we
  // have enough that we actually need to evict some.
  if ((int32_t)entries.Length() <= sHistoryMaxTotalViewers) {
    return;
  }

  // If we need to evict, sort our list of entries and evict the largest
  // ones.  (We could of course get better algorithmic complexity here by using
  // a heap or something more clever.  But sHistoryMaxTotalViewers isn't large,
  // so let's not worry about it.)
  entries.Sort();

  for (int32_t i = entries.Length() - 1; i >= sHistoryMaxTotalViewers; --i) {
    (entries[i].mSHistory)->EvictContentViewerForEntry(entries[i].mEntry);
  }
}

nsresult nsSHistory::FindEntryForBFCache(nsIBFCacheEntry* aBFEntry,
                                         nsISHEntry** aResult,
                                         int32_t* aResultIndex) {
  *aResult = nullptr;
  *aResultIndex = -1;

  int32_t startIndex, endIndex;
  WindowIndices(mIndex, &startIndex, &endIndex);

  for (int32_t i = startIndex; i <= endIndex; ++i) {
    nsCOMPtr<nsISHEntry> shEntry = mEntries[i];

    // Does shEntry have the same BFCacheEntry as the argument to this method?
    if (shEntry->HasBFCacheEntry(aBFEntry)) {
      shEntry.forget(aResult);
      *aResultIndex = i;
      return NS_OK;
    }
  }
  return NS_ERROR_FAILURE;
}

nsresult nsSHistory::EvictExpiredContentViewerForEntry(
    nsIBFCacheEntry* aBFEntry) {
  int32_t index;
  nsCOMPtr<nsISHEntry> shEntry;
  FindEntryForBFCache(aBFEntry, getter_AddRefs(shEntry), &index);

  if (index == mIndex) {
    NS_WARNING("How did the current SHEntry expire?");
    return NS_OK;
  }

  if (shEntry) {
    EvictContentViewerForEntry(shEntry);
  }

  return NS_OK;
}

NS_IMETHODIMP_(void)
nsSHistory::AddToExpirationTracker(nsIBFCacheEntry* aBFEntry) {
  RefPtr<nsSHEntryShared> entry = static_cast<nsSHEntryShared*>(aBFEntry);
  if (!mHistoryTracker || !entry) {
    return;
  }

  mHistoryTracker->AddObject(entry);
  return;
}

NS_IMETHODIMP_(void)
nsSHistory::RemoveFromExpirationTracker(nsIBFCacheEntry* aBFEntry) {
  RefPtr<nsSHEntryShared> entry = static_cast<nsSHEntryShared*>(aBFEntry);
  MOZ_ASSERT(mHistoryTracker && !mHistoryTracker->IsEmpty());
  if (!mHistoryTracker || !entry) {
    return;
  }

  mHistoryTracker->RemoveObject(entry);
  return;
}

// Evicts all content viewers in all history objects.  This is very
// inefficient, because it requires a linear search through all SHistory
// objects for each viewer to be evicted.  However, this method is called
// infrequently -- only when the disk or memory cache is cleared.

// static
void nsSHistory::GloballyEvictAllContentViewers() {
  int32_t maxViewers = sHistoryMaxTotalViewers;
  sHistoryMaxTotalViewers = 0;
  GloballyEvictContentViewers();
  sHistoryMaxTotalViewers = maxViewers;
}

void GetDynamicChildren(nsISHEntry* aEntry, nsTArray<nsID>& aDocshellIDs,
                        bool aOnlyTopLevelDynamic) {
  int32_t count = aEntry->GetChildCount();
  for (int32_t i = 0; i < count; ++i) {
    nsCOMPtr<nsISHEntry> child;
    aEntry->GetChildAt(i, getter_AddRefs(child));
    if (child) {
      bool dynAdded = child->IsDynamicallyAdded();
      if (dynAdded) {
        nsID docshellID = child->DocshellID();
        aDocshellIDs.AppendElement(docshellID);
      }
      if (!dynAdded || !aOnlyTopLevelDynamic) {
        GetDynamicChildren(child, aDocshellIDs, aOnlyTopLevelDynamic);
      }
    }
  }
}

bool RemoveFromSessionHistoryEntry(nsISHEntry* aRoot,
                                   nsTArray<nsID>& aDocshellIDs) {
  bool didRemove = false;
  int32_t childCount = aRoot->GetChildCount();
  for (int32_t i = childCount - 1; i >= 0; --i) {
    nsCOMPtr<nsISHEntry> child;
    aRoot->GetChildAt(i, getter_AddRefs(child));
    if (child) {
      nsID docshelldID = child->DocshellID();
      if (aDocshellIDs.Contains(docshelldID)) {
        didRemove = true;
        aRoot->RemoveChild(child);
      } else {
        bool childRemoved = RemoveFromSessionHistoryEntry(child, aDocshellIDs);
        if (childRemoved) {
          didRemove = true;
        }
      }
    }
  }
  return didRemove;
}

bool RemoveChildEntries(nsISHistory* aHistory, int32_t aIndex,
                        nsTArray<nsID>& aEntryIDs) {
  nsCOMPtr<nsISHEntry> root;
  aHistory->GetEntryAtIndex(aIndex, getter_AddRefs(root));
  return root ? RemoveFromSessionHistoryEntry(root, aEntryIDs) : false;
}

bool IsSameTree(nsISHEntry* aEntry1, nsISHEntry* aEntry2) {
  if (!aEntry1 && !aEntry2) {
    return true;
  }
  if ((!aEntry1 && aEntry2) || (aEntry1 && !aEntry2)) {
    return false;
  }
  uint32_t id1 = aEntry1->GetID();
  uint32_t id2 = aEntry2->GetID();
  if (id1 != id2) {
    return false;
  }

  int32_t count1 = aEntry1->GetChildCount();
  int32_t count2 = aEntry2->GetChildCount();
  // We allow null entries in the end of the child list.
  int32_t count = std::max(count1, count2);
  for (int32_t i = 0; i < count; ++i) {
    nsCOMPtr<nsISHEntry> child1, child2;
    aEntry1->GetChildAt(i, getter_AddRefs(child1));
    aEntry2->GetChildAt(i, getter_AddRefs(child2));
    if (!IsSameTree(child1, child2)) {
      return false;
    }
  }

  return true;
}

bool nsSHistory::RemoveDuplicate(int32_t aIndex, bool aKeepNext) {
  NS_ASSERTION(aIndex >= 0, "aIndex must be >= 0!");
  NS_ASSERTION(aIndex != 0 || aKeepNext,
               "If we're removing index 0 we must be keeping the next");
  NS_ASSERTION(aIndex != mIndex, "Shouldn't remove mIndex!");

  int32_t compareIndex = aKeepNext ? aIndex + 1 : aIndex - 1;

  nsresult rv;
  nsCOMPtr<nsISHEntry> root1, root2;
  rv = GetEntryAtIndex(aIndex, getter_AddRefs(root1));
  NS_ENSURE_SUCCESS(rv, false);
  rv = GetEntryAtIndex(compareIndex, getter_AddRefs(root2));
  NS_ENSURE_SUCCESS(rv, false);

  if (IsSameTree(root1, root2)) {
    mEntries.RemoveElementAt(aIndex);

    if (mRootDocShell) {
      static_cast<nsDocShell*>(mRootDocShell)->HistoryEntryRemoved(aIndex);
    }

    // Adjust our indices to reflect the removed entry.
    if (mIndex > aIndex) {
      mIndex = mIndex - 1;
    }

    // NB: If the entry we are removing is the entry currently
    // being navigated to (mRequestedIndex) then we adjust the index
    // only if we're not keeping the next entry (because if we are keeping
    // the next entry (because the current is a duplicate of the next), then
    // that entry slides into the spot that we're currently pointing to.
    // We don't do this adjustment for mIndex because mIndex cannot equal
    // aIndex.

    // NB: We don't need to guard on mRequestedIndex being nonzero here,
    // because either they're strictly greater than aIndex which is at least
    // zero, or they are equal to aIndex in which case aKeepNext must be true
    // if aIndex is zero.
    if (mRequestedIndex > aIndex || (mRequestedIndex == aIndex && !aKeepNext)) {
      mRequestedIndex = mRequestedIndex - 1;
    }
    return true;
  }
  return false;
}

NS_IMETHODIMP_(void)
nsSHistory::RemoveEntries(nsTArray<nsID>& aIDs, int32_t aStartIndex) {
  int32_t index = aStartIndex;
  while (index >= 0 && RemoveChildEntries(this, --index, aIDs)) {
  }
  int32_t minIndex = index;
  index = aStartIndex;
  while (index >= 0 && RemoveChildEntries(this, index++, aIDs)) {
  }

  // We need to remove duplicate nsSHEntry trees.
  bool didRemove = false;
  while (index > minIndex) {
    if (index != mIndex) {
      didRemove = RemoveDuplicate(index, index < mIndex) || didRemove;
    }
    --index;
  }
  if (didRemove && mRootDocShell) {
    mRootDocShell->DispatchLocationChangeEvent();
  }
}

void nsSHistory::RemoveDynEntries(int32_t aIndex, nsISHEntry* aEntry) {
  // Remove dynamic entries which are at the index and belongs to the container.
  nsCOMPtr<nsISHEntry> entry(aEntry);
  if (!entry) {
    GetEntryAtIndex(aIndex, getter_AddRefs(entry));
  }

  if (entry) {
    AutoTArray<nsID, 16> toBeRemovedEntries;
    GetDynamicChildren(entry, toBeRemovedEntries, true);
    if (toBeRemovedEntries.Length()) {
      RemoveEntries(toBeRemovedEntries, aIndex);
    }
  }
}

void nsSHistory::RemoveDynEntriesForBFCacheEntry(nsIBFCacheEntry* aBFEntry) {
  int32_t index;
  nsCOMPtr<nsISHEntry> shEntry;
  FindEntryForBFCache(aBFEntry, getter_AddRefs(shEntry), &index);
  if (shEntry) {
    RemoveDynEntries(index, shEntry);
  }
}

NS_IMETHODIMP
nsSHistory::UpdateIndex() {
  // Update the actual index with the right value.
  if (mIndex != mRequestedIndex && mRequestedIndex != -1) {
    mIndex = mRequestedIndex;
  }

  mRequestedIndex = -1;
  return NS_OK;
}

nsresult nsSHistory::GetCurrentURI(nsIURI** aResultURI) {
  NS_ENSURE_ARG_POINTER(aResultURI);
  nsresult rv;

  nsCOMPtr<nsISHEntry> currentEntry;
  rv = GetEntryAtIndex(mIndex, getter_AddRefs(currentEntry));
  if (NS_FAILED(rv) && !currentEntry) {
    return rv;
  }
  nsCOMPtr<nsIURI> uri = currentEntry->GetURI();
  uri.forget(aResultURI);
  return rv;
}

NS_IMETHODIMP
nsSHistory::GotoIndex(int32_t aIndex) {
  return LoadEntry(aIndex, LOAD_HISTORY, HIST_CMD_GOTOINDEX);
}

nsresult nsSHistory::LoadNextPossibleEntry(int32_t aNewIndex, long aLoadType,
                                           uint32_t aHistCmd) {
  mRequestedIndex = -1;
  if (aNewIndex < mIndex) {
    return LoadEntry(aNewIndex - 1, aLoadType, aHistCmd);
  }
  if (aNewIndex > mIndex) {
    return LoadEntry(aNewIndex + 1, aLoadType, aHistCmd);
  }
  return NS_ERROR_FAILURE;
}

nsresult nsSHistory::LoadEntry(int32_t aIndex, long aLoadType,
                               uint32_t aHistCmd) {
  if (!mRootDocShell) {
    return NS_ERROR_FAILURE;
  }

  if (aIndex < 0 || aIndex >= Length()) {
    // The index is out of range
    return NS_ERROR_FAILURE;
  }

  // This is a normal local history navigation.
  // Keep note of requested history index in mRequestedIndex.
  mRequestedIndex = aIndex;

  nsCOMPtr<nsISHEntry> prevEntry;
  nsCOMPtr<nsISHEntry> nextEntry;
  GetEntryAtIndex(mIndex, getter_AddRefs(prevEntry));
  GetEntryAtIndex(mRequestedIndex, getter_AddRefs(nextEntry));
  if (!nextEntry || !prevEntry) {
    mRequestedIndex = -1;
    return NS_ERROR_FAILURE;
  }

  // Remember that this entry is getting loaded at this point in the sequence

  nextEntry->SetLastTouched(++gTouchCounter);

  // Get the uri for the entry we are about to visit
  nsCOMPtr<nsIURI> nextURI = nextEntry->GetURI();

  MOZ_ASSERT((prevEntry && nextEntry && nextURI),
             "prevEntry, nextEntry and nextURI can't be null");

  // Send appropriate listener notifications.
  if (aHistCmd == HIST_CMD_GOTOINDEX) {
    // We are going somewhere else. This is not reload either
    NOTIFY_LISTENERS(OnHistoryGotoIndex, (aIndex, nextURI));
  }

  if (mRequestedIndex == mIndex) {
    // Possibly a reload case
    return InitiateLoad(nextEntry, mRootDocShell, aLoadType);
  }

  // Going back or forward.
  bool differenceFound = false;
  nsresult rv = LoadDifferingEntries(prevEntry, nextEntry, mRootDocShell,
                                     aLoadType, differenceFound);
  if (!differenceFound) {
    // We did not find any differences. Go further in the history.
    return LoadNextPossibleEntry(aIndex, aLoadType, aHistCmd);
  }

  return rv;
}

nsresult nsSHistory::LoadDifferingEntries(nsISHEntry* aPrevEntry,
                                          nsISHEntry* aNextEntry,
                                          nsIDocShell* aParent, long aLoadType,
                                          bool& aDifferenceFound) {
  if (!aPrevEntry || !aNextEntry || !aParent) {
    return NS_ERROR_FAILURE;
  }

  nsresult result = NS_OK;
  uint32_t prevID = aPrevEntry->GetID();
  uint32_t nextID = aNextEntry->GetID();

  // Check the IDs to verify if the pages are different.
  if (prevID != nextID) {
    aDifferenceFound = true;

    // Set the Subframe flag if not navigating the root docshell.
    aNextEntry->SetIsSubFrame(aParent != mRootDocShell);
    return InitiateLoad(aNextEntry, aParent, aLoadType);
  }

  // The entries are the same, so compare any child frames
  int32_t pcnt = aPrevEntry->GetChildCount();
  int32_t ncnt = aNextEntry->GetChildCount();
  int32_t dsCount = 0;
  aParent->GetChildCount(&dsCount);

  // Create an array for child docshells.
  nsCOMArray<nsIDocShell> docshells;
  for (int32_t i = 0; i < dsCount; ++i) {
    nsCOMPtr<nsIDocShellTreeItem> treeItem;
    aParent->GetChildAt(i, getter_AddRefs(treeItem));
    nsCOMPtr<nsIDocShell> shell = do_QueryInterface(treeItem);
    if (shell) {
      docshells.AppendElement(shell.forget());
    }
  }

  // Search for something to load next.
  for (int32_t i = 0; i < ncnt; ++i) {
    // First get an entry which may cause a new page to be loaded.
    nsCOMPtr<nsISHEntry> nChild;
    aNextEntry->GetChildAt(i, getter_AddRefs(nChild));
    if (!nChild) {
      continue;
    }
    nsID docshellID = nChild->DocshellID();

    // Then find the associated docshell.
    nsIDocShell* dsChild = nullptr;
    int32_t count = docshells.Count();
    for (int32_t j = 0; j < count; ++j) {
      nsIDocShell* shell = docshells[j];
      nsID shellID = shell->HistoryID();
      if (shellID == docshellID) {
        dsChild = shell;
        break;
      }
    }
    if (!dsChild) {
      continue;
    }

    // Then look at the previous entries to see if there was
    // an entry for the docshell.
    nsCOMPtr<nsISHEntry> pChild;
    for (int32_t k = 0; k < pcnt; ++k) {
      nsCOMPtr<nsISHEntry> child;
      aPrevEntry->GetChildAt(k, getter_AddRefs(child));
      if (child) {
        nsID dID = child->DocshellID();
        if (dID == docshellID) {
          pChild = child;
          break;
        }
      }
    }

    // Finally recursively call this method.
    // This will either load a new page to shell or some subshell or
    // do nothing.
    LoadDifferingEntries(pChild, nChild, dsChild, aLoadType, aDifferenceFound);
  }
  return result;
}

nsresult nsSHistory::InitiateLoad(nsISHEntry* aFrameEntry,
                                  nsIDocShell* aFrameDS, long aLoadType) {
  NS_ENSURE_STATE(aFrameDS && aFrameEntry);

  RefPtr<nsDocShellLoadState> loadState = new nsDocShellLoadState();

  /* Set the loadType in the SHEntry too to  what was passed on.
   * This will be passed on to child subframes later in nsDocShell,
   * so that proper loadType is maintained through out a frameset
   */
  aFrameEntry->SetLoadType(aLoadType);

  loadState->SetLoadType(aLoadType);
  loadState->SetSHEntry(aFrameEntry);

  nsCOMPtr<nsIURI> originalURI = aFrameEntry->GetOriginalURI();
  loadState->SetOriginalURI(originalURI);

  loadState->SetLoadReplace(aFrameEntry->GetLoadReplace());

  nsCOMPtr<nsIURI> newURI = aFrameEntry->GetURI();
  loadState->SetURI(newURI);
  loadState->SetLoadFlags(nsIWebNavigation::LOAD_FLAGS_NONE);
  // TODO fix principal here in Bug 1508642
  loadState->SetTriggeringPrincipal(nsContentUtils::GetSystemPrincipal());
  loadState->SetFirstParty(false);

  // Time to initiate a document load
  return aFrameDS->LoadURI(loadState);
}

NS_IMETHODIMP_(void)
nsSHistory::SetRootDocShell(nsIDocShell* aDocShell) {
  mRootDocShell = aDocShell;

  // Init mHistoryTracker on setting mRootDocShell so we can bind its event
  // target to the tabGroup.
  if (mRootDocShell) {
    nsCOMPtr<nsPIDOMWindowOuter> win = mRootDocShell->GetWindow();
    if (!win) {
      return;
    }

    // Seamonkey moves shistory between <xul:browser>s when restoring a tab.
    // Let's try not to break our friend too badly...
    if (mHistoryTracker) {
      NS_WARNING(
          "Change the root docshell of a shistory is unsafe and "
          "potentially problematic.");
      mHistoryTracker->AgeAllGenerations();
    }

    nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(win);

    mHistoryTracker = mozilla::MakeUnique<HistoryTracker>(
        this,
        mozilla::Preferences::GetUint(CONTENT_VIEWER_TIMEOUT_SECONDS,
                                      CONTENT_VIEWER_TIMEOUT_SECONDS_DEFAULT),
        global->EventTargetFor(mozilla::TaskCategory::Other));
  }
}
