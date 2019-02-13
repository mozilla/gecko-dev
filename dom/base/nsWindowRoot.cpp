/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/BasicEvents.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/EventListenerManager.h"
#include "mozilla/dom/WindowRootBinding.h"
#include "nsCOMPtr.h"
#include "nsWindowRoot.h"
#include "nsPIDOMWindow.h"
#include "nsPresContext.h"
#include "nsLayoutCID.h"
#include "nsContentCID.h"
#include "nsString.h"
#include "nsGlobalWindow.h"
#include "nsFocusManager.h"
#include "nsIContent.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIDOMHTMLTextAreaElement.h"
#include "nsIControllers.h"
#include "nsIController.h"
#include "xpcpublic.h"
#include "nsCycleCollectionParticipant.h"
#include "mozilla/dom/TabParent.h"

#ifdef MOZ_XUL
#include "nsIDOMXULElement.h"
#endif

using namespace mozilla;
using namespace mozilla::dom;

nsWindowRoot::nsWindowRoot(nsPIDOMWindow* aWindow)
{
  mWindow = aWindow;
  MOZ_ASSERT(mWindow->IsOuterWindow());
}

nsWindowRoot::~nsWindowRoot()
{
  if (mListenerManager) {
    mListenerManager->Disconnect();
  }
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(nsWindowRoot,
                                      mWindow,
                                      mListenerManager,
                                      mPopupNode,
                                      mParent)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsWindowRoot)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMEventTarget)
  NS_INTERFACE_MAP_ENTRY(nsPIWindowRoot)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventTarget)
  NS_INTERFACE_MAP_ENTRY(mozilla::dom::EventTarget)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsWindowRoot)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsWindowRoot)

NS_IMPL_DOMTARGET_DEFAULTS(nsWindowRoot)

NS_IMETHODIMP
nsWindowRoot::RemoveEventListener(const nsAString& aType, nsIDOMEventListener* aListener, bool aUseCapture)
{
  if (nsRefPtr<EventListenerManager> elm = GetExistingListenerManager()) {
    elm->RemoveEventListener(aType, aListener, aUseCapture);
  }
  return NS_OK;
}

NS_IMPL_REMOVE_SYSTEM_EVENT_LISTENER(nsWindowRoot)

NS_IMETHODIMP
nsWindowRoot::DispatchEvent(nsIDOMEvent* aEvt, bool *aRetVal)
{
  nsEventStatus status = nsEventStatus_eIgnore;
  nsresult rv =  EventDispatcher::DispatchDOMEvent(
    static_cast<EventTarget*>(this), nullptr, aEvt, nullptr, &status);
  *aRetVal = (status != nsEventStatus_eConsumeNoDefault);
  return rv;
}

nsresult
nsWindowRoot::DispatchDOMEvent(WidgetEvent* aEvent,
                               nsIDOMEvent* aDOMEvent,
                               nsPresContext* aPresContext,
                               nsEventStatus* aEventStatus)
{
  return EventDispatcher::DispatchDOMEvent(static_cast<EventTarget*>(this),
                                           aEvent, aDOMEvent,
                                           aPresContext, aEventStatus);
}

NS_IMETHODIMP
nsWindowRoot::AddEventListener(const nsAString& aType,
                               nsIDOMEventListener *aListener,
                               bool aUseCapture, bool aWantsUntrusted,
                               uint8_t aOptionalArgc)
{
  NS_ASSERTION(!aWantsUntrusted || aOptionalArgc > 1,
               "Won't check if this is chrome, you want to set "
               "aWantsUntrusted to false or make the aWantsUntrusted "
               "explicit by making optional_argc non-zero.");

  EventListenerManager* elm = GetOrCreateListenerManager();
  NS_ENSURE_STATE(elm);
  elm->AddEventListener(aType, aListener, aUseCapture, aWantsUntrusted);
  return NS_OK;
}

void
nsWindowRoot::AddEventListener(const nsAString& aType,
                                EventListener* aListener,
                                bool aUseCapture,
                                const Nullable<bool>& aWantsUntrusted,
                                ErrorResult& aRv)
{
  bool wantsUntrusted = !aWantsUntrusted.IsNull() && aWantsUntrusted.Value();
  EventListenerManager* elm = GetOrCreateListenerManager();
  if (!elm) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }
  elm->AddEventListener(aType, aListener, aUseCapture, wantsUntrusted);
}


NS_IMETHODIMP
nsWindowRoot::AddSystemEventListener(const nsAString& aType,
                                     nsIDOMEventListener *aListener,
                                     bool aUseCapture,
                                     bool aWantsUntrusted,
                                     uint8_t aOptionalArgc)
{
  NS_ASSERTION(!aWantsUntrusted || aOptionalArgc > 1,
               "Won't check if this is chrome, you want to set "
               "aWantsUntrusted to false or make the aWantsUntrusted "
               "explicit by making optional_argc non-zero.");

  return NS_AddSystemEventListener(this, aType, aListener, aUseCapture,
                                   aWantsUntrusted);
}

EventListenerManager*
nsWindowRoot::GetOrCreateListenerManager()
{
  if (!mListenerManager) {
    mListenerManager =
      new EventListenerManager(static_cast<EventTarget*>(this));
  }

  return mListenerManager;
}

EventListenerManager*
nsWindowRoot::GetExistingListenerManager() const
{
  return mListenerManager;
}

nsIScriptContext*
nsWindowRoot::GetContextForEventHandlers(nsresult* aRv)
{
  *aRv = NS_OK;
  return nullptr;
}

nsresult
nsWindowRoot::PreHandleEvent(EventChainPreVisitor& aVisitor)
{
  aVisitor.mCanHandle = true;
  aVisitor.mForceContentDispatch = true; //FIXME! Bug 329119
  // To keep mWindow alive
  aVisitor.mItemData = static_cast<nsISupports *>(mWindow);
  aVisitor.mParentTarget = mParent;
  return NS_OK;
}

nsresult
nsWindowRoot::PostHandleEvent(EventChainPostVisitor& aVisitor)
{
  return NS_OK;
}

nsIDOMWindow*
nsWindowRoot::GetOwnerGlobalForBindings()
{
  return GetWindow();
}

nsIGlobalObject*
nsWindowRoot::GetOwnerGlobal() const
{
  nsCOMPtr<nsIGlobalObject> global =
    do_QueryInterface(mWindow->GetCurrentInnerWindow());
  // We're still holding a ref to it, so returning the raw pointer is ok...
  return global;
}

nsPIDOMWindow*
nsWindowRoot::GetWindow()
{
  return mWindow;
}

nsresult
nsWindowRoot::GetControllers(nsIControllers** aResult)
{
  *aResult = nullptr;

  // XXX: we should fix this so there's a generic interface that
  // describes controllers, so this code would have no special
  // knowledge of what object might have controllers.

  nsCOMPtr<nsPIDOMWindow> focusedWindow;
  nsIContent* focusedContent =
    nsFocusManager::GetFocusedDescendant(mWindow, true, getter_AddRefs(focusedWindow));
  if (focusedContent) {
#ifdef MOZ_XUL
    nsCOMPtr<nsIDOMXULElement> xulElement(do_QueryInterface(focusedContent));
    if (xulElement)
      return xulElement->GetControllers(aResult);
#endif

    nsCOMPtr<nsIDOMHTMLTextAreaElement> htmlTextArea =
      do_QueryInterface(focusedContent);
    if (htmlTextArea)
      return htmlTextArea->GetControllers(aResult);

    nsCOMPtr<nsIDOMHTMLInputElement> htmlInputElement =
      do_QueryInterface(focusedContent);
    if (htmlInputElement)
      return htmlInputElement->GetControllers(aResult);

    if (focusedContent->IsEditable() && focusedWindow)
      return focusedWindow->GetControllers(aResult);
  }
  else {
    nsCOMPtr<nsIDOMWindow> domWindow = do_QueryInterface(focusedWindow);
    if (domWindow)
      return domWindow->GetControllers(aResult);
  }

  return NS_OK;
}

nsresult
nsWindowRoot::GetControllerForCommand(const char * aCommand,
                                      nsIController** _retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = nullptr;

  {
    nsCOMPtr<nsIControllers> controllers;
    GetControllers(getter_AddRefs(controllers));
    if (controllers) {
      nsCOMPtr<nsIController> controller;
      controllers->GetControllerForCommand(aCommand, getter_AddRefs(controller));
      if (controller) {
        controller.forget(_retval);
        return NS_OK;
      }
    }
  }

  nsCOMPtr<nsPIDOMWindow> focusedWindow;
  nsFocusManager::GetFocusedDescendant(mWindow, true, getter_AddRefs(focusedWindow));
  while (focusedWindow) {
    nsCOMPtr<nsIControllers> controllers;
    focusedWindow->GetControllers(getter_AddRefs(controllers));
    if (controllers) {
      nsCOMPtr<nsIController> controller;
      controllers->GetControllerForCommand(aCommand,
                                           getter_AddRefs(controller));
      if (controller) {
        controller.forget(_retval);
        return NS_OK;
      }
    }

    // XXXndeakin P3 is this casting safe?
    nsGlobalWindow *win = static_cast<nsGlobalWindow*>(focusedWindow.get());
    focusedWindow = win->GetPrivateParent();
  }
  
  return NS_OK;
}

void
nsWindowRoot::GetEnabledDisabledCommandsForControllers(nsIControllers* aControllers,
                                                       nsTHashtable<nsCharPtrHashKey>& aCommandsHandled,
                                                       nsTArray<nsCString>& aEnabledCommands,
                                                       nsTArray<nsCString>& aDisabledCommands)
{
  uint32_t controllerCount;
  aControllers->GetControllerCount(&controllerCount);
  for (uint32_t c = 0; c < controllerCount; c++) {
    nsCOMPtr<nsIController> controller;
    aControllers->GetControllerAt(c, getter_AddRefs(controller));

    nsCOMPtr<nsICommandController> commandController(do_QueryInterface(controller));
    if (commandController) {
      uint32_t commandsCount;
      char** commands;
      if (NS_SUCCEEDED(commandController->GetSupportedCommands(&commandsCount, &commands))) {
        for (uint32_t e = 0; e < commandsCount; e++) {
          // Use a hash to determine which commands have already been handled by
          // earlier controllers, as the earlier controller's result should get
          // priority.
          if (!aCommandsHandled.Contains(commands[e])) {
            aCommandsHandled.PutEntry(commands[e]);

            bool enabled = false;
            controller->IsCommandEnabled(commands[e], &enabled);

            const nsDependentCSubstring commandStr(commands[e], strlen(commands[e]));
            if (enabled) {
              aEnabledCommands.AppendElement(commandStr);
            } else {
              aDisabledCommands.AppendElement(commandStr);
            }
          }
        }

        NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(commandsCount, commands);
      }
    }
  }
}

void
nsWindowRoot::GetEnabledDisabledCommands(nsTArray<nsCString>& aEnabledCommands,
                                         nsTArray<nsCString>& aDisabledCommands)
{
  nsTHashtable<nsCharPtrHashKey> commandsHandled;

  nsCOMPtr<nsIControllers> controllers;
  GetControllers(getter_AddRefs(controllers));
  if (controllers) {
    GetEnabledDisabledCommandsForControllers(controllers, commandsHandled,
                                             aEnabledCommands, aDisabledCommands);
  }

  nsCOMPtr<nsPIDOMWindow> focusedWindow;
  nsFocusManager::GetFocusedDescendant(mWindow, true, getter_AddRefs(focusedWindow));
  while (focusedWindow) {
    focusedWindow->GetControllers(getter_AddRefs(controllers));
    if (controllers) {
      GetEnabledDisabledCommandsForControllers(controllers, commandsHandled,
                                               aEnabledCommands, aDisabledCommands);
    }

    nsGlobalWindow* win = static_cast<nsGlobalWindow*>(focusedWindow.get());
    focusedWindow = win->GetPrivateParent();
  }
}

nsIDOMNode*
nsWindowRoot::GetPopupNode()
{
  return mPopupNode;
}

void
nsWindowRoot::SetPopupNode(nsIDOMNode* aNode)
{
  mPopupNode = aNode;
}

nsIGlobalObject*
nsWindowRoot::GetParentObject()
{
  return xpc::NativeGlobal(xpc::PrivilegedJunkScope());
}

JSObject*
nsWindowRoot::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return mozilla::dom::WindowRootBinding::Wrap(aCx, this, aGivenProto);
}

void
nsWindowRoot::AddBrowser(mozilla::dom::TabParent* aBrowser)
{
  nsWeakPtr weakBrowser = do_GetWeakReference(static_cast<nsITabParent*>(aBrowser));
  mWeakBrowsers.PutEntry(weakBrowser);
}

void
nsWindowRoot::RemoveBrowser(mozilla::dom::TabParent* aBrowser)
{
  nsWeakPtr weakBrowser = do_GetWeakReference(static_cast<nsITabParent*>(aBrowser));
  mWeakBrowsers.RemoveEntry(weakBrowser);
}

static PLDHashOperator
WeakBrowserEnumFunc(nsRefPtrHashKey<nsIWeakReference>* aKey, void* aArg)
{
  nsTArray<nsRefPtr<TabParent>>* tabParents =
    static_cast<nsTArray<nsRefPtr<TabParent>>*>(aArg);
  nsCOMPtr<nsITabParent> tabParent(do_QueryReferent((*aKey).GetKey()));
  TabParent* tab = TabParent::GetFrom(tabParent);
  if (tab) {
    tabParents->AppendElement(tab);
  }
  return PL_DHASH_NEXT;
}

void
nsWindowRoot::EnumerateBrowsers(BrowserEnumerator aEnumFunc, void* aArg)
{
  // Collect strong references to all browsers in a separate array in
  // case aEnumFunc alters mWeakBrowsers.
  nsTArray<nsRefPtr<TabParent>> tabParents;
  mWeakBrowsers.EnumerateEntries(WeakBrowserEnumFunc, &tabParents);

  for (uint32_t i = 0; i < tabParents.Length(); ++i) {
    aEnumFunc(tabParents[i], aArg);
  }
}

///////////////////////////////////////////////////////////////////////////////////

already_AddRefed<EventTarget>
NS_NewWindowRoot(nsPIDOMWindow* aWindow)
{
  nsCOMPtr<EventTarget> result = new nsWindowRoot(aWindow);
  return result.forget();
}
