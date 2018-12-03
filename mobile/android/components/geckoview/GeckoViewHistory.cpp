/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GeckoViewHistory.h"

#include "JavaBuiltins.h"
#include "jsapi.h"
#include "nsIURI.h"
#include "nsXULAppAPI.h"

#include "mozilla/ClearOnShutdown.h"

#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/Link.h"
#include "mozilla/dom/TabChild.h"

#include "mozilla/ipc/URIUtils.h"

#include "mozilla/widget/EventDispatcher.h"
#include "mozilla/widget/nsWindow.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::ipc;
using namespace mozilla::widget;

// Keep in sync with `GeckoSession.HistoryDelegate.VisitFlags`.
enum class GeckoViewVisitFlags : int32_t {
  VISIT_TOP_LEVEL = 1 << 0,
  VISIT_REDIRECT_TEMPORARY = 1 << 1,
  VISIT_REDIRECT_PERMANENT = 1 << 2,
  VISIT_REDIRECT_SOURCE = 1 << 3,
  VISIT_REDIRECT_SOURCE_PERMANENT = 1 << 4,
  VISIT_UNRECOVERABLE_ERROR = 1 << 5,
};

// The number of milliseconds to wait between tracking a link and dispatching a
// `GetVisited` request for the link to Java. Used to debounce requests and
// reduce the number of IPC and JNI calls.
static const uint32_t GET_VISITS_WAIT_MS = 250;

static inline nsIDocument* OwnerDocForLink(Link* aLink) {
  Element* element = aLink->GetElement();
  return element ? element->OwnerDoc() : nullptr;
}

GeckoViewHistory::GeckoViewHistory() {}

NS_IMPL_ISUPPORTS(GeckoViewHistory, IHistory, nsITimerCallback, nsINamed)

GeckoViewHistory::~GeckoViewHistory() {}

StaticRefPtr<GeckoViewHistory> GeckoViewHistory::sHistory;

/* static */
already_AddRefed<GeckoViewHistory> GeckoViewHistory::GetSingleton() {
  if (!sHistory) {
    sHistory = new GeckoViewHistory();
    ClearOnShutdown(&sHistory);
  }
  RefPtr<GeckoViewHistory> history = sHistory;
  return history.forget();
}

NS_IMETHODIMP
GeckoViewHistory::GetName(nsACString& aName) {
  aName.AssignLiteral("GeckoViewHistory");
  return NS_OK;
}

// Handles a request to fetch visited statuses for new tracked URIs in the
// content process (e10s).
void GeckoViewHistory::QueryVisitedStateInContentProcess() {
  // Holds an array of new tracked URIs for a tab in the content process.
  struct NewURIEntry {
    explicit NewURIEntry(TabChild* aTabChild, nsIURI* aURI)
        : mTabChild(aTabChild) {
      AddURI(aURI);
    }

    void AddURI(nsIURI* aURI) { SerializeURI(aURI, *mURIs.AppendElement()); }

    TabChild* mTabChild;
    nsTArray<URIParams> mURIs;
  };

  MOZ_ASSERT(XRE_IsContentProcess());

  // First, serialize all the new URIs that we need to look up. Note that this
  // could be written as `nsDataHashtable<nsUint64HashKey, nsTArray<URIParams>`
  // instead, but, since we don't expect to have many tab children, we can avoid
  // the cost of hashing.
  AutoTArray<NewURIEntry, 8> newEntries;
  for (auto newURIsIter = mNewURIs.Iter(); !newURIsIter.Done();
       newURIsIter.Next()) {
    nsIURI* uri = newURIsIter.Get()->GetKey();
    if (auto entry = mTrackedURIs.Lookup(uri)) {
      TrackedURI& trackedURI = entry.Data();
      if (!trackedURI.mLinks.IsEmpty()) {
        nsTObserverArray<Link*>::BackwardIterator linksIter(trackedURI.mLinks);
        while (linksIter.HasMore()) {
          Link* link = linksIter.GetNext();

          TabChild* tabChild = nullptr;
          nsIWidget* widget =
              nsContentUtils::WidgetForContent(link->GetElement());
          if (widget) {
            tabChild = widget->GetOwningTabChild();
          }
          if (!tabChild) {
            // We need the link's tab child to find the matching window in the
            // parent process, so stop tracking it if it doesn't have one.
            linksIter.Remove();
            continue;
          }

          // Add to the list of new URIs for this document, or make a new entry.
          bool hasEntry = false;
          for (NewURIEntry& entry : newEntries) {
            if (entry.mTabChild == tabChild) {
              entry.AddURI(uri);
              hasEntry = true;
              break;
            }
          }
          if (!hasEntry) {
            newEntries.AppendElement(NewURIEntry(tabChild, uri));
          }
        }
      }
      if (trackedURI.mLinks.IsEmpty()) {
        // If the list of tracked links is empty, remove the entry for the URI.
        // We'll need to query the history delegate again the next time we look
        // up the visited status for this URI.
        entry.Remove();
      }
    }
    newURIsIter.Remove();
  }

  // Send the request to the parent process, one message per tab child.
  for (const NewURIEntry& entry : newEntries) {
    Unused << NS_WARN_IF(!entry.mTabChild->SendQueryVisitedState(entry.mURIs));
  }
}

// Handles a request to fetch visited statuses for new tracked URIs in the
// parent process (non-e10s).
void GeckoViewHistory::QueryVisitedStateInParentProcess() {
  // Holds an array of new URIs for a window in the parent process. Unlike
  // the content process case, we don't need to track tab children, since we
  // have the outer window and can send the request directly to Java.
  struct NewURIEntry {
    explicit NewURIEntry(nsIWidget* aWidget, nsIURI* aURI) : mWidget(aWidget) {
      AddURI(aURI);
    }

    void AddURI(nsIURI* aURI) { mURIs.AppendElement(aURI); }

    nsCOMPtr<nsIWidget> mWidget;
    nsTArray<nsCOMPtr<nsIURI>> mURIs;
  };

  MOZ_ASSERT(XRE_IsParentProcess());

  nsTArray<NewURIEntry> newEntries;
  for (auto newURIsIter = mNewURIs.Iter(); !newURIsIter.Done();
       newURIsIter.Next()) {
    nsIURI* uri = newURIsIter.Get()->GetKey();
    if (auto entry = mTrackedURIs.Lookup(uri)) {
      TrackedURI& trackedURI = entry.Data();
      if (!trackedURI.mLinks.IsEmpty()) {
        nsTObserverArray<Link*>::BackwardIterator linksIter(trackedURI.mLinks);
        while (linksIter.HasMore()) {
          Link* link = linksIter.GetNext();

          nsIWidget* widget =
              nsContentUtils::WidgetForContent(link->GetElement());
          if (!widget) {
            linksIter.Remove();
            continue;
          }

          bool hasEntry = false;
          for (NewURIEntry& entry : newEntries) {
            if (entry.mWidget == widget) {
              entry.AddURI(uri);
              hasEntry = true;
              break;
            }
          }
          if (!hasEntry) {
            newEntries.AppendElement(NewURIEntry(widget, uri));
          }
        }
      }
      if (trackedURI.mLinks.IsEmpty()) {
        entry.Remove();
      }
    }
  }
  mNewURIs.Clear();

  for (const NewURIEntry& entry : newEntries) {
    QueryVisitedState(entry.mWidget, entry.mURIs);
  }
}

NS_IMETHODIMP
GeckoViewHistory::Notify(nsITimer* aTimer) {
  MOZ_ASSERT(aTimer == mQueryVisitedStateTimer);

  if (mNewURIs.Count() > 0) {
    if (XRE_IsContentProcess()) {
      QueryVisitedStateInContentProcess();
    } else {
      QueryVisitedStateInParentProcess();
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
GeckoViewHistory::RegisterVisitedCallback(nsIURI* aURI, Link* aLink) {
  if (!aLink || !aURI) {
    return NS_OK;
  }

  auto entry = mTrackedURIs.LookupForAdd(aURI);
  if (entry) {
    // Start tracking the link for this URI.
    TrackedURI& trackedURI = entry.Data();
    trackedURI.mLinks.AppendElement(aLink);

    if (trackedURI.mVisited) {
      // If we already know that the URI was visited, update the link state now.
      DispatchNotifyVisited(aURI, OwnerDocForLink(aLink));
    }
  } else {
    // Otherwise, track the link, and start the timer to request the visited
    // status from the history delegate for this and any other new URIs. If the
    // delegate reports that the URI is unvisited, we'll keep tracking the link,
    // and update its state from `VisitedCallback` once it's visited. If the URI
    // is already visited, `GetVisitedCallback` will update this and all other
    // visited links, and stop tracking them.
    entry.OrInsert([aLink]() {
      TrackedURI trackedURI;
      trackedURI.mLinks.AppendElement(aLink);
      return trackedURI;
    });
    mNewURIs.PutEntry(aURI);
    if (!mQueryVisitedStateTimer) {
      mQueryVisitedStateTimer = NS_NewTimer();
    }
    Unused << NS_WARN_IF(NS_FAILED(mQueryVisitedStateTimer->InitWithCallback(
        this, GET_VISITS_WAIT_MS, nsITimer::TYPE_ONE_SHOT)));
  }

  return NS_OK;
}

NS_IMETHODIMP
GeckoViewHistory::UnregisterVisitedCallback(nsIURI* aURI, Link* aLink) {
  if (!aLink || !aURI) {
    return NS_OK;
  }

  if (auto entry = mTrackedURIs.Lookup(aURI)) {
    TrackedURI& trackedURI = entry.Data();
    if (!trackedURI.mLinks.IsEmpty()) {
      nsTObserverArray<Link*>::BackwardIterator iter(trackedURI.mLinks);
      while (iter.HasMore()) {
        Link* link = iter.GetNext();
        if (link == aLink) {
          iter.Remove();
          break;
        }
      }
    }

    if (trackedURI.mLinks.IsEmpty()) {
      // If the list of tracked links is empty, remove the entry for the URI.
      // We'll need to query the history delegate again the next time we look
      // up the visited status for this URI.
      entry.Remove();
    }
  }

  mNewURIs.RemoveEntry(aURI);

  return NS_OK;
}

/**
 * Called from the session handler for the history delegate, after the new
 * visit is recorded.
 */
class OnVisitedCallback final : public nsIAndroidEventCallback {
 public:
  explicit OnVisitedCallback(GeckoViewHistory* aHistory,
                             nsIGlobalObject* aGlobalObject, nsIURI* aURI)
      : mHistory(aHistory), mGlobalObject(aGlobalObject), mURI(aURI) {}

  NS_DECL_ISUPPORTS

  NS_IMETHOD
  OnSuccess(JS::HandleValue aData) override {
    bool shouldNotify = false;
    {
      // Scope `jsapi`.
      dom::AutoJSAPI jsapi;
      if (NS_WARN_IF(!jsapi.Init(mGlobalObject))) {
        return NS_ERROR_FAILURE;
      }
      shouldNotify = ShouldNotifyVisited(jsapi.cx(), aData);
      JS_ClearPendingException(jsapi.cx());
    }
    if (shouldNotify) {
      AutoTArray<VisitedURI, 1> visitedURIs;
      visitedURIs.AppendElement(VisitedURI{mURI, true});
      mHistory->HandleVisitedState(visitedURIs);
    }
    return NS_OK;
  }

  NS_IMETHOD
  OnError(JS::HandleValue aData) override { return NS_OK; }

 private:
  virtual ~OnVisitedCallback() {}

  bool ShouldNotifyVisited(JSContext* aCx, JS::HandleValue aData) {
    if (NS_WARN_IF(!aData.isBoolean())) {
      return false;
    }
    return aData.toBoolean();
  }

  RefPtr<GeckoViewHistory> mHistory;
  nsCOMPtr<nsIGlobalObject> mGlobalObject;
  nsCOMPtr<nsIURI> mURI;
};

NS_IMPL_ISUPPORTS(OnVisitedCallback, nsIAndroidEventCallback)

NS_IMETHODIMP
GeckoViewHistory::VisitURI(nsIWidget* aWidget, nsIURI* aURI,
                           nsIURI* aLastVisitedURI, uint32_t aFlags) {
  if (!aURI) {
    return NS_OK;
  }

  if (XRE_IsContentProcess()) {
    URIParams uri;
    SerializeURI(aURI, uri);

    OptionalURIParams lastVisitedURI;
    SerializeURI(aLastVisitedURI, lastVisitedURI);

    // If we're in the content process, send the visit to the parent. The parent
    // will find the matching chrome window for the content process and tab,
    // then forward the visit to Java.
    if (NS_WARN_IF(!aWidget)) {
      return NS_OK;
    }
    TabChild* tabChild = aWidget->GetOwningTabChild();
    if (NS_WARN_IF(!tabChild)) {
      return NS_OK;
    }
    Unused << NS_WARN_IF(!tabChild->SendVisitURI(uri, lastVisitedURI, aFlags));
    return NS_OK;
  }

  // Otherwise, we're in the parent process. Wrap the URIs up in a bundle, and
  // send them to Java.
  MOZ_ASSERT(XRE_IsParentProcess());
  RefPtr<nsWindow> window = nsWindow::From(aWidget);
  if (NS_WARN_IF(!window)) {
    return NS_OK;
  }
  widget::EventDispatcher* dispatcher = window->GetEventDispatcher();
  if (NS_WARN_IF(!dispatcher)) {
    return NS_OK;
  }

  AutoTArray<jni::String::LocalRef, 3> keys;
  AutoTArray<jni::Object::LocalRef, 3> values;

  nsAutoCString uriSpec;
  if (NS_WARN_IF(NS_FAILED(aURI->GetSpec(uriSpec)))) {
    return NS_OK;
  }
  keys.AppendElement(jni::StringParam(NS_LITERAL_STRING("url")));
  values.AppendElement(jni::StringParam(uriSpec));

  if (aLastVisitedURI) {
    nsAutoCString lastVisitedURISpec;
    if (NS_WARN_IF(NS_FAILED(aLastVisitedURI->GetSpec(lastVisitedURISpec)))) {
      return NS_OK;
    }
    keys.AppendElement(jni::StringParam(NS_LITERAL_STRING("lastVisitedURL")));
    values.AppendElement(jni::StringParam(lastVisitedURISpec));
  }

  int32_t flags = 0;
  if (aFlags & TOP_LEVEL) {
    flags |= static_cast<int32_t>(GeckoViewVisitFlags::VISIT_TOP_LEVEL);
  }
  if (aFlags & REDIRECT_TEMPORARY) {
    flags |=
        static_cast<int32_t>(GeckoViewVisitFlags::VISIT_REDIRECT_TEMPORARY);
  }
  if (aFlags & REDIRECT_PERMANENT) {
    flags |=
        static_cast<int32_t>(GeckoViewVisitFlags::VISIT_REDIRECT_PERMANENT);
  }
  if (aFlags & REDIRECT_SOURCE) {
    flags |= static_cast<int32_t>(GeckoViewVisitFlags::VISIT_REDIRECT_SOURCE);
  }
  if (aFlags & REDIRECT_SOURCE_PERMANENT) {
    flags |= static_cast<int32_t>(
        GeckoViewVisitFlags::VISIT_REDIRECT_SOURCE_PERMANENT);
  }
  if (aFlags & UNRECOVERABLE_ERROR) {
    flags |=
        static_cast<int32_t>(GeckoViewVisitFlags::VISIT_UNRECOVERABLE_ERROR);
  }
  keys.AppendElement(jni::StringParam(NS_LITERAL_STRING("flags")));
  values.AppendElement(java::sdk::Integer::ValueOf(flags));

  MOZ_ASSERT(keys.Length() == values.Length());

  auto bundleKeys = jni::ObjectArray::New<jni::String>(keys.Length());
  auto bundleValues = jni::ObjectArray::New<jni::Object>(values.Length());
  for (size_t i = 0; i < keys.Length(); ++i) {
    bundleKeys->SetElement(i, keys[i]);
    bundleValues->SetElement(i, values[i]);
  }
  auto bundle = java::GeckoBundle::New(bundleKeys, bundleValues);

  nsCOMPtr<nsIAndroidEventCallback> callback =
      new OnVisitedCallback(this, dispatcher->GetGlobalObject(), aURI);

  Unused << NS_WARN_IF(NS_FAILED(
      dispatcher->Dispatch(u"GeckoView:OnVisited", bundle, callback)));

  return NS_OK;
}

NS_IMETHODIMP
GeckoViewHistory::SetURITitle(nsIURI* aURI, const nsAString& aTitle) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
GeckoViewHistory::NotifyVisited(nsIURI* aURI) {
  if (NS_WARN_IF(!aURI)) {
    return NS_OK;
  }

  if (auto entry = mTrackedURIs.Lookup(aURI)) {
    TrackedURI& trackedURI = entry.Data();
    trackedURI.mVisited = true;
    nsTArray<nsIDocument*> seen;
    nsTObserverArray<Link*>::BackwardIterator iter(trackedURI.mLinks);
    while (iter.HasMore()) {
      Link* link = iter.GetNext();
      nsIDocument* doc = OwnerDocForLink(link);
      if (seen.Contains(doc)) {
        continue;
      }
      seen.AppendElement(doc);
      DispatchNotifyVisited(aURI, doc);
    }
  }

  return NS_OK;
}

/**
 * Called from the session handler for the history delegate, with visited
 * statuses for all requested URIs.
 */
class GetVisitedCallback final : public nsIAndroidEventCallback {
 public:
  explicit GetVisitedCallback(GeckoViewHistory* aHistory,
                              nsIGlobalObject* aGlobalObject,
                              const nsTArray<nsCOMPtr<nsIURI>>& aURIs)
      : mHistory(aHistory), mGlobalObject(aGlobalObject), mURIs(aURIs) {}

  NS_DECL_ISUPPORTS

  NS_IMETHOD
  OnSuccess(JS::HandleValue aData) override {
    nsTArray<VisitedURI> visitedURIs;
    {
      // Scope `jsapi`.
      dom::AutoJSAPI jsapi;
      if (NS_WARN_IF(!jsapi.Init(mGlobalObject))) {
        return NS_ERROR_FAILURE;
      }
      if (!ExtractVisitedURIs(jsapi.cx(), aData, visitedURIs)) {
        JS_ClearPendingException(jsapi.cx());
        return NS_ERROR_FAILURE;
      }
    }
    mHistory->HandleVisitedState(visitedURIs);
    return NS_OK;
  }

  NS_IMETHOD
  OnError(JS::HandleValue aData) override { return NS_OK; }

 private:
  virtual ~GetVisitedCallback() {}

  /**
   * Unpacks an array of Boolean visited statuses from the session handler into
   * an array of `VisitedURI` structs. Each element in the array corresponds to
   * a URI in `mURIs`.
   *
   * Returns `false` on error, `true` if the array is `null` or was successfully
   * unpacked.
   *
   * TODO (bug 1503482): Remove this unboxing.
   */
  bool ExtractVisitedURIs(JSContext* aCx, JS::HandleValue aData,
                          nsTArray<VisitedURI>& aVisitedURIs) {
    if (aData.isNull()) {
      return true;
    }
    bool isArray = false;
    if (NS_WARN_IF(!JS_IsArrayObject(aCx, aData, &isArray))) {
      return false;
    }
    if (NS_WARN_IF(!isArray)) {
      return false;
    }
    JS::Rooted<JSObject*> visited(aCx, &aData.toObject());
    uint32_t length = 0;
    if (NS_WARN_IF(!JS_GetArrayLength(aCx, visited, &length))) {
      return false;
    }
    if (NS_WARN_IF(length != mURIs.Length())) {
      return false;
    }
    if (!aVisitedURIs.SetCapacity(length, mozilla::fallible)) {
      return false;
    }
    for (uint32_t i = 0; i < length; ++i) {
      JS::Rooted<JS::Value> value(aCx);
      if (NS_WARN_IF(!JS_GetElement(aCx, visited, i, &value))) {
        JS_ClearPendingException(aCx);
        aVisitedURIs.AppendElement(VisitedURI{mURIs[i], false});
        continue;
      }
      if (NS_WARN_IF(!value.isBoolean())) {
        aVisitedURIs.AppendElement(VisitedURI{mURIs[i], false});
        continue;
      }
      aVisitedURIs.AppendElement(VisitedURI{mURIs[i], value.toBoolean()});
    }
    return true;
  }

  RefPtr<GeckoViewHistory> mHistory;
  nsCOMPtr<nsIGlobalObject> mGlobalObject;
  nsTArray<nsCOMPtr<nsIURI>> mURIs;
};

NS_IMPL_ISUPPORTS(GetVisitedCallback, nsIAndroidEventCallback)

/**
 * Queries the history delegate to find which URIs have been visited. This
 * is always called in the parent process: from `GetVisited` in non-e10s, and
 * from `ContentParent::RecvGetVisited` in e10s.
 */
void GeckoViewHistory::QueryVisitedState(
    nsIWidget* aWidget, const nsTArray<nsCOMPtr<nsIURI>>& aURIs) {
  MOZ_ASSERT(XRE_IsParentProcess());
  RefPtr<nsWindow> window = nsWindow::From(aWidget);
  if (NS_WARN_IF(!window)) {
    return;
  }
  widget::EventDispatcher* dispatcher = window->GetEventDispatcher();
  if (NS_WARN_IF(!dispatcher)) {
    return;
  }

  // Assemble a bundle like `{ urls: ["http://example.com/1", ...] }`.
  auto uris = jni::ObjectArray::New<jni::String>(aURIs.Length());
  for (size_t i = 0; i < aURIs.Length(); ++i) {
    nsAutoCString uriSpec;
    if (NS_WARN_IF(NS_FAILED(aURIs[i]->GetSpec(uriSpec)))) {
      continue;
    }
    jni::String::LocalRef value{jni::StringParam(uriSpec)};
    uris->SetElement(i, value);
  }

  auto bundleKeys = jni::ObjectArray::New<jni::String>(1);
  jni::String::LocalRef key(jni::StringParam(NS_LITERAL_STRING("urls")));
  bundleKeys->SetElement(0, key);

  auto bundleValues = jni::ObjectArray::New<jni::Object>(1);
  jni::Object::LocalRef value(uris);
  bundleValues->SetElement(0, value);

  auto bundle = java::GeckoBundle::New(bundleKeys, bundleValues);

  nsCOMPtr<nsIAndroidEventCallback> callback =
      new GetVisitedCallback(this, dispatcher->GetGlobalObject(), aURIs);

  Unused << NS_WARN_IF(NS_FAILED(
      dispatcher->Dispatch(u"GeckoView:GetVisited", bundle, callback)));
}

/**
 * Updates link states for all tracked links, forwarding the visited statuses to
 * the content process in e10s. This is always called in the parent process,
 * from `VisitedCallback::OnSuccess` and `GetVisitedCallback::OnSuccess`.
 */
void GeckoViewHistory::HandleVisitedState(
    const nsTArray<VisitedURI>& aVisitedURIs) {
  MOZ_ASSERT(XRE_IsParentProcess());
  if (aVisitedURIs.IsEmpty()) {
    return;
  }

  nsTArray<ContentParent*> cplist;
  ContentParent::GetAll(cplist);
  if (!cplist.IsEmpty()) {
    nsTArray<URIParams> visitedURIs(aVisitedURIs.Length());
    for (const VisitedURI& visitedURI : aVisitedURIs) {
      if (!visitedURI.mVisited) {
        continue;
      }
      URIParams uri;
      SerializeURI(visitedURI.mURI, uri);
      visitedURIs.AppendElement(uri);
    }
    if (visitedURIs.IsEmpty()) {
      return;
    }
    for (ContentParent* cp : cplist) {
      Unused << NS_WARN_IF(!cp->SendNotifyVisited(visitedURIs));
    }
  }

  // We might still have child processes even if e10s is disabled, so always
  // check if we're tracking any links in the parent, and notify them if so.
  if (mTrackedURIs.Count() > 0) {
    for (const VisitedURI& visitedURI : aVisitedURIs) {
      if (visitedURI.mVisited) {
        Unused << NS_WARN_IF(NS_FAILED(NotifyVisited(visitedURI.mURI)));
      }
    }
  }
}

/**
 * Asynchronously updates the link state for all links associated with `aURI` in
 * `aDocument`. This is mostly copied from `History::DispatchNotifyVisited` and
 * `History::NotifyVisitedForDocument`.
 */
void GeckoViewHistory::DispatchNotifyVisited(nsIURI* aURI,
                                             nsIDocument* aDocument) {
  // Capture strong references to the arguments to capture in the closure.
  RefPtr<GeckoViewHistory> kungFuDeathGrip(this);
  nsCOMPtr<nsIDocument> doc(aDocument);
  nsCOMPtr<nsIURI> uri(aURI);

  nsCOMPtr<nsIRunnable> runnable = NS_NewRunnableFunction(
      "GeckoViewHistory::DispatchNotifyVisited",
      [this, kungFuDeathGrip, uri, doc] {
        nsAutoScriptBlocker scriptBlocker;
        auto entry = mTrackedURIs.Lookup(uri);
        if (NS_WARN_IF(!entry)) {
          return;
        }
        TrackedURI& trackedURI = entry.Data();
        if (!trackedURI.mLinks.IsEmpty()) {
          nsTObserverArray<Link*>::BackwardIterator iter(trackedURI.mLinks);
          while (iter.HasMore()) {
            Link* link = iter.GetNext();
            if (OwnerDocForLink(link) == doc) {
              link->SetLinkState(eLinkState_Visited);
              iter.Remove();
            }
          }
        }
        if (trackedURI.mLinks.IsEmpty()) {
          entry.Remove();
        }
      });

  if (doc) {
    Unused << NS_WARN_IF(
        NS_FAILED(doc->Dispatch(TaskCategory::Other, runnable.forget())));
  } else {
    Unused << NS_WARN_IF(NS_FAILED(NS_DispatchToMainThread(runnable.forget())));
  }
}
