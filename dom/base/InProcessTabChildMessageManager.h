/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsInProcessTabChildGlobal_h
#define nsInProcessTabChildGlobal_h

#include "mozilla/Attributes.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/RefPtr.h"
#include "mozilla/dom/ContentFrameMessageManager.h"
#include "nsCOMPtr.h"
#include "nsFrameMessageManager.h"
#include "nsIScriptContext.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsIScriptContext.h"
#include "nsIClassInfo.h"
#include "nsIDocShell.h"
#include "nsCOMArray.h"
#include "nsIRunnable.h"
#include "nsWeakReference.h"

namespace mozilla {
class EventChainPreVisitor;

namespace dom {

/**
 * This class implements a ContentFrameMessageManager for use by frame loaders
 * in the parent process. It is bound to a DocShell rather than a TabChild, and
 * does not use any IPC infrastructure for its message passing.
 */

class InProcessTabChildMessageManager final
    : public ContentFrameMessageManager,
      public nsMessageManagerScriptExecutor,
      public nsIInProcessContentFrameMessageManager,
      public nsSupportsWeakReference,
      public mozilla::dom::ipc::MessageManagerCallback {
  typedef mozilla::dom::ipc::StructuredCloneData StructuredCloneData;

 private:
  InProcessTabChildMessageManager(nsIDocShell* aShell, nsIContent* aOwner,
                                  nsFrameMessageManager* aChrome);

 public:
  static already_AddRefed<InProcessTabChildMessageManager> Create(
      nsIDocShell* aShell, nsIContent* aOwner, nsFrameMessageManager* aChrome) {
    RefPtr<InProcessTabChildMessageManager> mm =
        new InProcessTabChildMessageManager(aShell, aOwner, aChrome);

    NS_ENSURE_TRUE(mm->Init(), nullptr);

    return mm.forget();
  }

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(
      InProcessTabChildMessageManager, DOMEventTargetHelper)

  void MarkForCC();

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  virtual already_AddRefed<nsPIDOMWindowOuter> GetContent(
      ErrorResult& aError) override;
  virtual already_AddRefed<nsIDocShell> GetDocShell(
      ErrorResult& aError) override {
    nsCOMPtr<nsIDocShell> docShell(mDocShell);
    return docShell.forget();
  }
  virtual already_AddRefed<nsIEventTarget> GetTabEventTarget() override;
  virtual uint64_t ChromeOuterWindowID() override;

  NS_FORWARD_SAFE_NSIMESSAGESENDER(mMessageManager)

  NS_DECL_NSIINPROCESSCONTENTFRAMEMESSAGEMANAGER

  void CacheFrameLoader(nsFrameLoader* aFrameLoader);

  /**
   * MessageManagerCallback methods that we override.
   */
  virtual bool DoSendBlockingMessage(JSContext* aCx, const nsAString& aMessage,
                                     StructuredCloneData& aData,
                                     JS::Handle<JSObject*> aCpows,
                                     nsIPrincipal* aPrincipal,
                                     nsTArray<StructuredCloneData>* aRetVal,
                                     bool aIsSync) override;
  virtual nsresult DoSendAsyncMessage(JSContext* aCx, const nsAString& aMessage,
                                      StructuredCloneData& aData,
                                      JS::Handle<JSObject*> aCpows,
                                      nsIPrincipal* aPrincipal) override;

  void GetEventTargetParent(EventChainPreVisitor& aVisitor) override;

  void LoadFrameScript(const nsAString& aURL, bool aRunInGlobalScope);
  void FireUnloadEvent();
  void DisconnectEventListeners();
  void Disconnect();
  void SendMessageToParent(const nsString& aMessage, bool aSync,
                           const nsString& aJSON,
                           nsTArray<nsString>* aJSONRetVal);
  nsFrameMessageManager* GetInnerManager() {
    return static_cast<nsFrameMessageManager*>(mMessageManager.get());
  }

  void SetOwner(nsIContent* aOwner) { mOwner = aOwner; }
  nsFrameMessageManager* GetChromeMessageManager() {
    return mChromeMessageManager;
  }
  void SetChromeMessageManager(nsFrameMessageManager* aParent) {
    mChromeMessageManager = aParent;
  }

  already_AddRefed<nsFrameLoader> GetFrameLoader();

 protected:
  virtual ~InProcessTabChildMessageManager();

  nsCOMPtr<nsIDocShell> mDocShell;
  bool mLoadingScript;

  // Is this the message manager for an in-process <iframe mozbrowser>? This
  // affects where events get sent, so it affects GetEventTargetParent.
  bool mIsBrowserFrame;
  bool mPreventEventsEscaping;

  // We keep a strong reference to the frameloader after we've started
  // teardown. This allows us to dispatch message manager messages during this
  // time.
  RefPtr<nsFrameLoader> mFrameLoader;

 public:
  nsIContent* mOwner;
  nsFrameMessageManager* mChromeMessageManager;
};

}  // namespace dom
}  // namespace mozilla

#endif
