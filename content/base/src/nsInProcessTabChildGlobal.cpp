/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 8; -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsInProcessTabChildGlobal.h"
#include "nsContentUtils.h"
#include "nsIScriptSecurityManager.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsEventDispatcher.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIJSRuntimeService.h"
#include "nsComponentManagerUtils.h"
#include "nsNetUtil.h"
#include "nsScriptLoader.h"
#include "nsFrameLoader.h"
#include "xpcpublic.h"
#include "nsIMozBrowserFrame.h"
#include "nsDOMClassInfoID.h"
#include "mozilla/dom/StructuredCloneUtils.h"
#include "js/StructuredClone.h"

using mozilla::dom::StructuredCloneData;
using mozilla::dom::StructuredCloneClosure;

bool
nsInProcessTabChildGlobal::DoSendBlockingMessage(JSContext* aCx,
                                                 const nsAString& aMessage,
                                                 const mozilla::dom::StructuredCloneData& aData,
                                                 JS::Handle<JSObject *> aCpows,
                                                 nsIPrincipal* aPrincipal,
                                                 InfallibleTArray<nsString>* aJSONRetVal,
                                                 bool aIsSync)
{
  nsTArray<nsCOMPtr<nsIRunnable> > asyncMessages;
  asyncMessages.SwapElements(mASyncMessages);
  uint32_t len = asyncMessages.Length();
  for (uint32_t i = 0; i < len; ++i) {
    nsCOMPtr<nsIRunnable> async = asyncMessages[i];
    async->Run();
  }
  if (mChromeMessageManager) {
    SameProcessCpowHolder cpows(js::GetRuntime(aCx), aCpows);
    nsRefPtr<nsFrameMessageManager> mm = mChromeMessageManager;
    mm->ReceiveMessage(mOwner, aMessage, true, &aData, &cpows, aPrincipal,
                       aJSONRetVal);
  }
  return true;
}

class nsAsyncMessageToParent : public nsRunnable
{
public:
  nsAsyncMessageToParent(JSContext* aCx,
                         nsInProcessTabChildGlobal* aTabChild,
                         const nsAString& aMessage,
                         const StructuredCloneData& aData,
                         JS::Handle<JSObject *> aCpows,
                         nsIPrincipal* aPrincipal)
    : mRuntime(js::GetRuntime(aCx)),
      mTabChild(aTabChild),
      mMessage(aMessage),
      mCpows(aCpows),
      mPrincipal(aPrincipal),
      mRun(false)
  {
    if (aData.mDataLength && !mData.copy(aData.mData, aData.mDataLength)) {
      NS_RUNTIMEABORT("OOM");
    }
    if (mCpows && !js_AddObjectRoot(mRuntime, &mCpows)) {
      NS_RUNTIMEABORT("OOM");
    }
    mClosure = aData.mClosure;
  }

  ~nsAsyncMessageToParent()
  {
    if (mCpows) {
        JS_RemoveObjectRootRT(mRuntime, &mCpows);
    }
  }

  NS_IMETHOD Run()
  {
    if (mRun) {
      return NS_OK;
    }

    mRun = true;
    mTabChild->mASyncMessages.RemoveElement(this);
    if (mTabChild->mChromeMessageManager) {
      StructuredCloneData data;
      data.mData = mData.data();
      data.mDataLength = mData.nbytes();
      data.mClosure = mClosure;

      SameProcessCpowHolder cpows(mRuntime, JS::Handle<JSObject *>::fromMarkedLocation(&mCpows));

      nsRefPtr<nsFrameMessageManager> mm = mTabChild->mChromeMessageManager;
      mm->ReceiveMessage(mTabChild->mOwner, mMessage, false, &data, &cpows,
                         mPrincipal, nullptr);
    }
    return NS_OK;
  }
  JSRuntime* mRuntime;
  nsRefPtr<nsInProcessTabChildGlobal> mTabChild;
  nsString mMessage;
  JSAutoStructuredCloneBuffer mData;
  StructuredCloneClosure mClosure;
  JSObject* mCpows;
  nsCOMPtr<nsIPrincipal> mPrincipal;
  // True if this runnable has already been called. This can happen if DoSendSyncMessage
  // is called while waiting for an asynchronous message send.
  bool mRun;
};

bool
nsInProcessTabChildGlobal::DoSendAsyncMessage(JSContext* aCx,
                                              const nsAString& aMessage,
                                              const StructuredCloneData& aData,
                                              JS::Handle<JSObject *> aCpows,
                                              nsIPrincipal* aPrincipal)
{
  nsCOMPtr<nsIRunnable> ev =
    new nsAsyncMessageToParent(aCx, this, aMessage, aData, aCpows, aPrincipal);
  mASyncMessages.AppendElement(ev);
  NS_DispatchToCurrentThread(ev);
  return true;
}

nsInProcessTabChildGlobal::nsInProcessTabChildGlobal(nsIDocShell* aShell,
                                                     nsIContent* aOwner,
                                                     nsFrameMessageManager* aChrome)
: mDocShell(aShell), mInitialized(false), mLoadingScript(false),
  mDelayedDisconnect(false), mOwner(aOwner), mChromeMessageManager(aChrome)
{

  // If owner corresponds to an <iframe mozbrowser> or <iframe mozapp>, we'll
  // have to tweak our PreHandleEvent implementation.
  nsCOMPtr<nsIMozBrowserFrame> browserFrame = do_QueryInterface(mOwner);
  if (browserFrame) {
    mIsBrowserOrAppFrame = browserFrame->GetReallyIsBrowserOrApp();
  }
  else {
    mIsBrowserOrAppFrame = false;
  }
}

nsInProcessTabChildGlobal::~nsInProcessTabChildGlobal()
{
}

/* [notxpcom] boolean markForCC (); */
// This method isn't automatically forwarded safely because it's notxpcom, so
// the IDL binding doesn't know what value to return.
NS_IMETHODIMP_(bool)
nsInProcessTabChildGlobal::MarkForCC()
{
  return mMessageManager ? mMessageManager->MarkForCC() : false;
}

nsresult
nsInProcessTabChildGlobal::Init()
{
#ifdef DEBUG
  nsresult rv =
#endif
  InitTabChildGlobal();
  NS_WARN_IF_FALSE(NS_SUCCEEDED(rv),
                   "Couldn't initialize nsInProcessTabChildGlobal");
  mMessageManager = new nsFrameMessageManager(this,
                                              nullptr,
                                              mozilla::dom::ipc::MM_CHILD);
  return NS_OK;
}

NS_IMPL_CYCLE_COLLECTION_INHERITED_2(nsInProcessTabChildGlobal,
                                     nsDOMEventTargetHelper,
                                     mMessageManager,
                                     mGlobal)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(nsInProcessTabChildGlobal)
  NS_INTERFACE_MAP_ENTRY(nsIMessageListenerManager)
  NS_INTERFACE_MAP_ENTRY(nsIMessageSender)
  NS_INTERFACE_MAP_ENTRY(nsISyncMessageSender)
  NS_INTERFACE_MAP_ENTRY(nsIContentFrameMessageManager)
  NS_INTERFACE_MAP_ENTRY(nsIInProcessContentFrameMessageManager)
  NS_INTERFACE_MAP_ENTRY(nsIScriptObjectPrincipal)
  NS_INTERFACE_MAP_ENTRY(nsIGlobalObject)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(ContentFrameMessageManager)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(nsInProcessTabChildGlobal, nsDOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(nsInProcessTabChildGlobal, nsDOMEventTargetHelper)

NS_IMETHODIMP
nsInProcessTabChildGlobal::GetContent(nsIDOMWindow** aContent)
{
  *aContent = nullptr;
  nsCOMPtr<nsIDOMWindow> window = do_GetInterface(mDocShell);
  window.swap(*aContent);
  return NS_OK;
}

NS_IMETHODIMP
nsInProcessTabChildGlobal::GetDocShell(nsIDocShell** aDocShell)
{
  NS_IF_ADDREF(*aDocShell = mDocShell);
  return NS_OK;
}

NS_IMETHODIMP
nsInProcessTabChildGlobal::Btoa(const nsAString& aBinaryData,
                            nsAString& aAsciiBase64String)
{
  return nsContentUtils::Btoa(aBinaryData, aAsciiBase64String);
}

NS_IMETHODIMP
nsInProcessTabChildGlobal::Atob(const nsAString& aAsciiString,
                            nsAString& aBinaryData)
{
  return nsContentUtils::Atob(aAsciiString, aBinaryData);
}


NS_IMETHODIMP
nsInProcessTabChildGlobal::PrivateNoteIntentionalCrash()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

void
nsInProcessTabChildGlobal::Disconnect()
{
  // Let the frame scripts know the child is being closed. We do any other
  // cleanup after the event has been fired. See DelayedDisconnect
  nsContentUtils::AddScriptRunner(
     NS_NewRunnableMethod(this, &nsInProcessTabChildGlobal::DelayedDisconnect)
  );
}

void
nsInProcessTabChildGlobal::DelayedDisconnect()
{
  // Don't let the event escape
  mOwner = nullptr;

  // Fire the "unload" event
  nsDOMEventTargetHelper::DispatchTrustedEvent(NS_LITERAL_STRING("unload"));

  // Continue with the Disconnect cleanup
  nsCOMPtr<nsPIDOMWindow> win = do_GetInterface(mDocShell);
  if (win) {
    MOZ_ASSERT(win->IsOuterWindow());
    win->SetChromeEventHandler(win->GetChromeEventHandler());
  }
  mDocShell = nullptr;
  mChromeMessageManager = nullptr;
  if (mMessageManager) {
    static_cast<nsFrameMessageManager*>(mMessageManager.get())->Disconnect();
    mMessageManager = nullptr;
  }
  if (mListenerManager) {
    mListenerManager->Disconnect();
  }

  if (!mLoadingScript) {
    ReleaseWrapper(static_cast<EventTarget*>(this));
    mGlobal = nullptr;
  } else {
    mDelayedDisconnect = true;
  }
}

NS_IMETHODIMP_(nsIContent *)
nsInProcessTabChildGlobal::GetOwnerContent()
{
  return mOwner;
}

nsresult
nsInProcessTabChildGlobal::PreHandleEvent(nsEventChainPreVisitor& aVisitor)
{
  aVisitor.mCanHandle = true;

  if (mIsBrowserOrAppFrame &&
      (!mOwner || !nsContentUtils::IsInChromeDocshell(mOwner->OwnerDoc()))) {
    if (mOwner) {
      nsPIDOMWindow* innerWindow = mOwner->OwnerDoc()->GetInnerWindow();
      if (innerWindow) {
        aVisitor.mParentTarget = innerWindow->GetParentTarget();
      }
    }
  } else {
    aVisitor.mParentTarget = mOwner;
  }

#ifdef DEBUG
  if (mOwner) {
    nsCOMPtr<nsIFrameLoaderOwner> owner = do_QueryInterface(mOwner);
    nsRefPtr<nsFrameLoader> fl = owner->GetFrameLoader();
    if (fl) {
      NS_ASSERTION(this == fl->GetTabChildGlobalAsEventTarget(),
                   "Wrong event target!");
      NS_ASSERTION(fl->mMessageManager == mChromeMessageManager,
                   "Wrong message manager!");
    }
  }
#endif

  return NS_OK;
}

nsresult
nsInProcessTabChildGlobal::InitTabChildGlobal()
{
  nsAutoCString id;
  id.AssignLiteral("inProcessTabChildGlobal");
  nsIURI* uri = mOwner->OwnerDoc()->GetDocumentURI();
  if (uri) {
    nsAutoCString u;
    uri->GetSpec(u);
    id.AppendLiteral("?ownedBy=");
    id.Append(u);
  }
  nsISupports* scopeSupports = NS_ISUPPORTS_CAST(EventTarget*, this);
  NS_ENSURE_STATE(InitTabChildGlobalInternal(scopeSupports, id));
  return NS_OK;
}

class nsAsyncScriptLoad : public nsRunnable
{
public:
    nsAsyncScriptLoad(nsInProcessTabChildGlobal* aTabChild, const nsAString& aURL,
                      bool aRunInGlobalScope)
      : mTabChild(aTabChild), mURL(aURL), mRunInGlobalScope(aRunInGlobalScope) {}

  NS_IMETHOD Run()
  {
    mTabChild->LoadFrameScript(mURL, mRunInGlobalScope);
    return NS_OK;
  }
  nsRefPtr<nsInProcessTabChildGlobal> mTabChild;
  nsString mURL;
  bool mRunInGlobalScope;
};

void
nsInProcessTabChildGlobal::LoadFrameScript(const nsAString& aURL, bool aRunInGlobalScope)
{
  if (!nsContentUtils::IsSafeToRunScript()) {
    nsContentUtils::AddScriptRunner(new nsAsyncScriptLoad(this, aURL, aRunInGlobalScope));
    return;
  }
  if (!mInitialized) {
    mInitialized = true;
    Init();
  }
  bool tmp = mLoadingScript;
  mLoadingScript = true;
  LoadFrameScriptInternal(aURL, aRunInGlobalScope);
  mLoadingScript = tmp;
  if (!mLoadingScript && mDelayedDisconnect) {
    mDelayedDisconnect = false;
    Disconnect();
  }
}
