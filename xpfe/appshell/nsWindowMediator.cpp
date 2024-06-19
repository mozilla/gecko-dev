/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMPtr.h"
#include "nsEnumeratorUtils.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "nsTArray.h"
#include "nsIBaseWindow.h"
#include "nsIWidget.h"
#include "nsIObserverService.h"
#include "nsISimpleEnumerator.h"
#include "nsAppShellWindowEnumerator.h"
#include "nsWindowMediator.h"
#include "nsIWindowMediatorListener.h"
#include "nsGlobalWindowInner.h"
#include "nsGlobalWindowOuter.h"
#include "nsServiceManagerUtils.h"

#include "nsIDocShell.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIAppWindow.h"

using namespace mozilla;

nsresult nsWindowMediator::GetDOMWindow(
    nsIAppWindow* inWindow, nsCOMPtr<nsPIDOMWindowOuter>& outDOMWindow) {
  nsCOMPtr<nsIDocShell> docShell;

  outDOMWindow = nullptr;
  inWindow->GetDocShell(getter_AddRefs(docShell));
  NS_ENSURE_TRUE(docShell, NS_ERROR_FAILURE);

  outDOMWindow = docShell->GetWindow();
  return outDOMWindow ? NS_OK : NS_ERROR_FAILURE;
}

nsWindowMediator::nsWindowMediator()
    : mOldestWindow(nullptr),
      mTopmostWindow(nullptr),
      mTimeStamp(0),
      mReady(false) {}

nsWindowMediator::~nsWindowMediator() {
  while (mOldestWindow) UnregisterWindow(mOldestWindow);
}

nsresult nsWindowMediator::Init() {
  nsresult rv;
  nsCOMPtr<nsIObserverService> obsSvc =
      do_GetService("@mozilla.org/observer-service;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = obsSvc->AddObserver(this, "xpcom-shutdown", true);
  NS_ENSURE_SUCCESS(rv, rv);

  mReady = true;
  return NS_OK;
}

NS_IMETHODIMP nsWindowMediator::RegisterWindow(nsIAppWindow* inWindow) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  if (!mReady) {
    NS_ERROR("Mediator is not initialized or about to die.");
    return NS_ERROR_FAILURE;
  }

  if (GetInfoFor(inWindow)) {
    NS_ERROR("multiple window registration");
    return NS_ERROR_FAILURE;
  }

  mTimeStamp++;

  // Create window info struct and add to list of windows
  nsWindowInfo* windowInfo = new nsWindowInfo(inWindow, mTimeStamp);

  for (const auto& listener : mListeners.ForwardRange()) {
    listener->OnOpenWindow(inWindow);
  }

  if (mOldestWindow)
    windowInfo->InsertAfter(mOldestWindow->mOlder, nullptr);
  else
    mOldestWindow = windowInfo;

  return NS_OK;
}

NS_IMETHODIMP
nsWindowMediator::UnregisterWindow(nsIAppWindow* inWindow) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mReady);
  NS_ENSURE_STATE(mReady);
  nsWindowInfo* info = GetInfoFor(inWindow);
  if (info) return UnregisterWindow(info);
  return NS_ERROR_INVALID_ARG;
}

nsresult nsWindowMediator::UnregisterWindow(nsWindowInfo* inInfo) {
  // Inform the iterators
  uint32_t index = 0;
  while (index < mEnumeratorList.Length()) {
    mEnumeratorList[index]->WindowRemoved(inInfo);
    index++;
  }

  nsIAppWindow* window = inInfo->mWindow.get();
  for (const auto& listener : mListeners.ForwardRange()) {
    listener->OnCloseWindow(window);
  }

  // Remove from the lists and free up
  if (inInfo == mOldestWindow) mOldestWindow = inInfo->mYounger;
  if (inInfo == mTopmostWindow) mTopmostWindow = inInfo->mLower;
  inInfo->Unlink(true, true);
  if (inInfo == mOldestWindow) mOldestWindow = nullptr;
  if (inInfo == mTopmostWindow) mTopmostWindow = nullptr;
  delete inInfo;

  return NS_OK;
}

nsWindowInfo* nsWindowMediator::GetInfoFor(nsIAppWindow* aWindow) {
  nsWindowInfo *info, *listEnd;

  if (!aWindow) return nullptr;

  info = mOldestWindow;
  listEnd = nullptr;
  while (info != listEnd) {
    if (info->mWindow.get() == aWindow) return info;
    info = info->mYounger;
    listEnd = mOldestWindow;
  }
  return nullptr;
}

nsWindowInfo* nsWindowMediator::GetInfoFor(nsIWidget* aWindow) {
  nsWindowInfo *info, *listEnd;

  if (!aWindow) return nullptr;

  info = mOldestWindow;
  listEnd = nullptr;

  nsCOMPtr<nsIWidget> scanWidget;
  while (info != listEnd) {
    nsCOMPtr<nsIBaseWindow> base(do_QueryInterface(info->mWindow));
    if (base) base->GetMainWidget(getter_AddRefs(scanWidget));
    if (aWindow == scanWidget.get()) return info;
    info = info->mYounger;
    listEnd = mOldestWindow;
  }
  return nullptr;
}

NS_IMETHODIMP
nsWindowMediator::GetEnumerator(const char16_t* inType,
                                nsISimpleEnumerator** outEnumerator) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  NS_ENSURE_ARG_POINTER(outEnumerator);
  if (!mReady) {
    // If we get here with mReady false, we most likely did observe
    // xpcom-shutdown. We will return an empty enumerator such that
    // we make happy Javascripts calling late without throwing.
    return NS_NewEmptyEnumerator(outEnumerator);
  }
  RefPtr<nsAppShellWindowEnumerator> enumerator =
      new nsASDOMWindowEarlyToLateEnumerator(inType, *this);
  enumerator.forget(outEnumerator);
  return NS_OK;
}

NS_IMETHODIMP
nsWindowMediator::GetAppWindowEnumerator(const char16_t* inType,
                                         nsISimpleEnumerator** outEnumerator) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  NS_ENSURE_ARG_POINTER(outEnumerator);
  if (!mReady) {
    // If we get here with mReady false, we most likely did observe
    // xpcom-shutdown. We will return an empty enumerator such that
    // we make happy Javascripts calling late without throwing.
    return NS_NewEmptyEnumerator(outEnumerator);
  }
  RefPtr<nsAppShellWindowEnumerator> enumerator =
      new nsASAppWindowEarlyToLateEnumerator(inType, *this);
  enumerator.forget(outEnumerator);
  return NS_OK;
}

NS_IMETHODIMP
nsWindowMediator::GetZOrderAppWindowEnumerator(const char16_t* aWindowType,
                                               bool aFrontToBack,
                                               nsISimpleEnumerator** _retval) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  NS_ENSURE_ARG_POINTER(_retval);
  if (!mReady) {
    // If we get here with mReady false, we most likely did observe
    // xpcom-shutdown. We will return an empty enumerator such that
    // we make happy Javascripts calling late without throwing.
    return NS_NewEmptyEnumerator(_retval);
  }
  RefPtr<nsAppShellWindowEnumerator> enumerator;
  if (aFrontToBack)
    enumerator = new nsASAppWindowFrontToBackEnumerator(aWindowType, *this);
  else
    enumerator = new nsASAppWindowBackToFrontEnumerator(aWindowType, *this);

  enumerator.forget(_retval);
  return NS_OK;
}

void nsWindowMediator::AddEnumerator(nsAppShellWindowEnumerator* inEnumerator) {
  mEnumeratorList.AppendElement(inEnumerator);
}

int32_t nsWindowMediator::RemoveEnumerator(
    nsAppShellWindowEnumerator* inEnumerator) {
  return mEnumeratorList.RemoveElement(inEnumerator);
}

// Returns the window of type inType ( if null return any window type ) which
// has the most recent time stamp
NS_IMETHODIMP
nsWindowMediator::GetMostRecentWindow(const char16_t* inType,
                                      mozIDOMWindowProxy** outWindow) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  NS_ENSURE_ARG_POINTER(outWindow);
  *outWindow = nullptr;
  if (!mReady) return NS_OK;

  // Find the most window with the highest time stamp that matches
  // the requested type
  nsWindowInfo* info = MostRecentWindowInfo(inType, false);
  if (info && info->mWindow) {
    nsCOMPtr<nsPIDOMWindowOuter> DOMWindow;
    if (NS_SUCCEEDED(GetDOMWindow(info->mWindow, DOMWindow))) {
      DOMWindow.forget(outWindow);
      return NS_OK;
    }
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsWindowMediator::GetMostRecentBrowserWindow(mozIDOMWindowProxy** outWindow) {
  nsresult rv = GetMostRecentWindow(u"navigator:browser", outWindow);
  NS_ENSURE_SUCCESS(rv, rv);

#ifdef MOZ_WIDGET_ANDROID
  if (!*outWindow) {
    rv = GetMostRecentWindow(u"navigator:geckoview", outWindow);
    NS_ENSURE_SUCCESS(rv, rv);
  }
#endif

#ifdef MOZ_THUNDERBIRD
  if (!*outWindow) {
    rv = GetMostRecentWindow(u"mail:3pane", outWindow);
    NS_ENSURE_SUCCESS(rv, rv);
  }
#endif

  return NS_OK;
}

NS_IMETHODIMP
nsWindowMediator::GetMostRecentNonPBWindow(const char16_t* aType,
                                           mozIDOMWindowProxy** aWindow) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  NS_ENSURE_ARG_POINTER(aWindow);
  *aWindow = nullptr;

  nsWindowInfo* info = MostRecentWindowInfo(aType, true);
  nsCOMPtr<nsPIDOMWindowOuter> domWindow;
  if (info && info->mWindow) {
    GetDOMWindow(info->mWindow, domWindow);
  }

  if (!domWindow) {
    return NS_ERROR_FAILURE;
  }

  domWindow.forget(aWindow);
  return NS_OK;
}

nsWindowInfo* nsWindowMediator::MostRecentWindowInfo(
    const char16_t* inType, bool aSkipPrivateBrowsingOrClosed) {
  int32_t lastTimeStamp = -1;
  nsAutoString typeString(inType);
  bool allWindows = !inType || typeString.IsEmpty();

  // Find the most recent window with the highest time stamp that matches
  // the requested type and has the correct browsing mode.
  nsWindowInfo* searchInfo = mOldestWindow;
  nsWindowInfo* listEnd = nullptr;
  nsWindowInfo* foundInfo = nullptr;
  for (; searchInfo != listEnd; searchInfo = searchInfo->mYounger) {
    listEnd = mOldestWindow;

    if (!allWindows && !searchInfo->TypeEquals(typeString)) {
      continue;
    }
    if (searchInfo->mTimeStamp < lastTimeStamp) {
      continue;
    }
    if (!searchInfo->mWindow) {
      continue;
    }
    if (aSkipPrivateBrowsingOrClosed) {
      nsCOMPtr<nsIDocShell> docShell;
      searchInfo->mWindow->GetDocShell(getter_AddRefs(docShell));
      nsCOMPtr<nsILoadContext> loadContext = do_QueryInterface(docShell);
      if (!loadContext || loadContext->UsePrivateBrowsing()) {
        continue;
      }

      nsCOMPtr<nsPIDOMWindowOuter> piwindow = docShell->GetWindow();
      if (!piwindow || piwindow->Closed()) {
        continue;
      }
    }

    foundInfo = searchInfo;
    lastTimeStamp = searchInfo->mTimeStamp;
  }

  return foundInfo;
}

NS_IMETHODIMP
nsWindowMediator::GetOuterWindowWithId(uint64_t aWindowID,
                                       mozIDOMWindowProxy** aWindow) {
  RefPtr<nsGlobalWindowOuter> window =
      nsGlobalWindowOuter::GetOuterWindowWithId(aWindowID);
  window.forget(aWindow);
  return NS_OK;
}

NS_IMETHODIMP
nsWindowMediator::GetCurrentInnerWindowWithId(uint64_t aWindowID,
                                              mozIDOMWindow** aWindow) {
  RefPtr<nsGlobalWindowInner> window =
      nsGlobalWindowInner::GetInnerWindowWithId(aWindowID);

  // not found
  if (!window) return NS_OK;

  nsCOMPtr<nsPIDOMWindowOuter> outer = window->GetOuterWindow();
  NS_ENSURE_TRUE(outer, NS_ERROR_UNEXPECTED);

  // outer is already using another inner, so it's same as not found
  if (outer->GetCurrentInnerWindow() != window) return NS_OK;

  window.forget(aWindow);
  return NS_OK;
}

NS_IMETHODIMP
nsWindowMediator::UpdateWindowTimeStamp(nsIAppWindow* inWindow) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mReady);
  NS_ENSURE_STATE(mReady);
  nsWindowInfo* info = GetInfoFor(inWindow);
  if (info) {
    // increment the window's time stamp
    info->mTimeStamp = ++mTimeStamp;
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

NS_IMPL_ISUPPORTS(nsWindowMediator, nsIWindowMediator, nsIObserver,
                  nsISupportsWeakReference)

NS_IMETHODIMP
nsWindowMediator::AddListener(nsIWindowMediatorListener* aListener) {
  NS_ENSURE_ARG_POINTER(aListener);

  mListeners.AppendElement(aListener);

  return NS_OK;
}

NS_IMETHODIMP
nsWindowMediator::RemoveListener(nsIWindowMediatorListener* aListener) {
  NS_ENSURE_ARG_POINTER(aListener);

  mListeners.RemoveElement(aListener);

  return NS_OK;
}

NS_IMETHODIMP
nsWindowMediator::Observe(nsISupports* aSubject, const char* aTopic,
                          const char16_t* aData) {
  if (!strcmp(aTopic, "xpcom-shutdown") && mReady) {
    MOZ_RELEASE_ASSERT(NS_IsMainThread());
    while (mOldestWindow) UnregisterWindow(mOldestWindow);
    mReady = false;
  }
  return NS_OK;
}
