/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsWindowRoot_h__
#define nsWindowRoot_h__

class nsPIDOMWindow;
class nsIDOMEvent;
class nsIGlobalObject;

#include "mozilla/Attributes.h"
#include "mozilla/EventListenerManager.h"
#include "nsIDOMEventTarget.h"
#include "nsPIWindowRoot.h"
#include "nsCycleCollectionParticipant.h"
#include "nsAutoPtr.h"
#include "nsTHashtable.h"
#include "nsHashKeys.h"

class nsWindowRoot final : public nsPIWindowRoot
{
public:
  explicit nsWindowRoot(nsPIDOMWindow* aWindow);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_NSIDOMEVENTTARGET

  virtual mozilla::EventListenerManager*
    GetExistingListenerManager() const override;
  virtual mozilla::EventListenerManager*
    GetOrCreateListenerManager() override;

  using mozilla::dom::EventTarget::RemoveEventListener;
  virtual void AddEventListener(const nsAString& aType,
                                mozilla::dom::EventListener* aListener,
                                bool aUseCapture,
                                const mozilla::dom::Nullable<bool>& aWantsUntrusted,
                                mozilla::ErrorResult& aRv) override;

  // nsPIWindowRoot

  virtual nsPIDOMWindow* GetWindow() override;

  virtual nsresult GetControllers(nsIControllers** aResult) override;
  virtual nsresult GetControllerForCommand(const char * aCommand,
                                           nsIController** _retval) override;

  virtual void GetEnabledDisabledCommands(nsTArray<nsCString>& aEnabledCommands,
                                          nsTArray<nsCString>& aDisabledCommands) override;

  virtual nsIDOMNode* GetPopupNode() override;
  virtual void SetPopupNode(nsIDOMNode* aNode) override;

  virtual void SetParentTarget(mozilla::dom::EventTarget* aTarget) override
  {
    mParent = aTarget;
  }
  virtual mozilla::dom::EventTarget* GetParentTarget() override { return mParent; }
  virtual nsIDOMWindow* GetOwnerGlobalForBindings() override;
  virtual nsIGlobalObject* GetOwnerGlobal() const override;

  nsIGlobalObject* GetParentObject();

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_AMBIGUOUS(nsWindowRoot,
                                                         nsIDOMEventTarget)

  virtual void AddBrowser(mozilla::dom::TabParent* aBrowser) override;
  virtual void RemoveBrowser(mozilla::dom::TabParent* aBrowser) override;
  virtual void EnumerateBrowsers(BrowserEnumerator aEnumFunc, void *aArg) override;

protected:
  virtual ~nsWindowRoot();

  void GetEnabledDisabledCommandsForControllers(nsIControllers* aControllers,
                                                nsTHashtable<nsCharPtrHashKey>& aCommandsHandled,
                                                nsTArray<nsCString>& aEnabledCommands,
                                                nsTArray<nsCString>& aDisabledCommands);

  // Members
  nsCOMPtr<nsPIDOMWindow> mWindow;
  // We own the manager, which owns event listeners attached to us.
  nsRefPtr<mozilla::EventListenerManager> mListenerManager; // [Strong]
  nsCOMPtr<nsIDOMNode> mPopupNode; // [OWNER]

  nsCOMPtr<mozilla::dom::EventTarget> mParent;

  // The TabParents that are currently registered with this top-level window.
  typedef nsTHashtable<nsRefPtrHashKey<nsIWeakReference>> WeakBrowserTable;
  WeakBrowserTable mWeakBrowsers;
};

extern already_AddRefed<mozilla::dom::EventTarget>
NS_NewWindowRoot(nsPIDOMWindow* aWindow);

#endif
