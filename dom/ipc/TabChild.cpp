/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"

#include "TabChild.h"

#include "gfxPrefs.h"
#ifdef ACCESSIBILITY
#include "mozilla/a11y/DocAccessibleChild.h"
#endif
#include "Layers.h"
#include "ContentChild.h"
#include "TabParent.h"
#include "js/JSON.h"
#include "mozilla/Preferences.h"
#include "mozilla/BrowserElementParent.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/EventListenerManager.h"
#include "mozilla/dom/DataTransfer.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/indexedDB/PIndexedDBPermissionRequestChild.h"
#include "mozilla/dom/MessageManagerBinding.h"
#include "mozilla/dom/MouseEventBinding.h"
#include "mozilla/dom/PaymentRequestChild.h"
#include "mozilla/gfx/CrossProcessPaint.h"
#include "mozilla/IMEStateManager.h"
#include "mozilla/ipc/URIUtils.h"
#include "mozilla/layers/APZChild.h"
#include "mozilla/layers/APZCCallbackHelper.h"
#include "mozilla/layers/APZCTreeManagerChild.h"
#include "mozilla/layers/APZEventState.h"
#include "mozilla/layers/ContentProcessController.h"
#include "mozilla/layers/CompositorBridgeChild.h"
#include "mozilla/layers/DoubleTapToZoom.h"
#include "mozilla/layers/IAPZCTreeManager.h"
#include "mozilla/layers/ImageBridgeChild.h"
#include "mozilla/layers/InputAPZContext.h"
#include "mozilla/layers/LayerTransactionChild.h"
#include "mozilla/layers/ShadowLayers.h"
#include "mozilla/layers/WebRenderLayerManager.h"
#include "mozilla/plugins/PPluginWidgetChild.h"
#include "mozilla/recordreplay/ParentIPC.h"
#include "mozilla/LookAndFeel.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/Move.h"
#include "mozilla/PresShell.h"
#include "mozilla/ProcessHangMonitor.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/TextEvents.h"
#include "mozilla/TouchEvents.h"
#include "mozilla/Unused.h"
#include "nsContentUtils.h"
#include "nsCSSFrameConstructor.h"
#include "nsDocShell.h"
#include "nsEmbedCID.h"
#include "nsGlobalWindow.h"
#include <algorithm>
#include "nsExceptionHandler.h"
#include "nsFilePickerProxy.h"
#include "mozilla/dom/Element.h"
#include "nsGlobalWindow.h"
#include "nsIBaseWindow.h"
#include "nsIBrowserDOMWindow.h"
#include "nsIDocumentInlines.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIDOMChromeWindow.h"
#include "nsIDOMWindow.h"
#include "nsIDOMWindowUtils.h"
#include "nsFocusManager.h"
#include "EventStateManager.h"
#include "nsIDocShell.h"
#include "nsIFrame.h"
#include "nsIURI.h"
#include "nsIURIFixup.h"
#include "nsCDefaultURIFixup.h"
#include "nsIWebBrowser.h"
#include "nsIWebProgress.h"
#include "nsIXULRuntime.h"
#include "nsPIDOMWindow.h"
#include "nsPIWindowRoot.h"
#include "nsPointerHashKeys.h"
#include "nsLayoutUtils.h"
#include "nsPrintfCString.h"
#include "nsTHashtable.h"
#include "nsThreadManager.h"
#include "nsThreadUtils.h"
#include "nsViewManager.h"
#include "nsIWeakReferenceUtils.h"
#include "nsWindowWatcher.h"
#include "PermissionMessageUtils.h"
#include "PuppetWidget.h"
#include "StructuredCloneData.h"
#include "nsViewportInfo.h"
#include "nsILoadContext.h"
#include "ipc/nsGUIEventIPC.h"
#include "mozilla/gfx/Matrix.h"
#include "UnitTransforms.h"
#include "ClientLayerManager.h"
#include "nsColorPickerProxy.h"
#include "nsContentPermissionHelper.h"
#include "nsNetUtil.h"
#include "nsIPermissionManager.h"
#include "nsIURILoader.h"
#include "nsIScriptError.h"
#include "mozilla/EventForwards.h"
#include "nsDeviceContext.h"
#include "nsSandboxFlags.h"
#include "FrameLayerBuilder.h"
#include "VRManagerChild.h"
#include "nsCommandParams.h"
#include "nsISHistory.h"
#include "nsQueryObject.h"
#include "nsIHttpChannel.h"
#include "mozilla/dom/DocGroup.h"
#include "nsString.h"
#include "nsISupportsPrimitives.h"
#include "mozilla/Telemetry.h"
#include "nsDocShellLoadState.h"
#include "nsWebBrowser.h"

#ifdef XP_WIN
#include "mozilla/plugins/PluginWidgetChild.h"
#endif

#ifdef NS_PRINTING
#include "nsIPrintSession.h"
#include "nsIPrintSettings.h"
#include "nsIPrintSettingsService.h"
#include "nsIWebBrowserPrint.h"
#endif

#define BROWSER_ELEMENT_CHILD_SCRIPT \
    NS_LITERAL_STRING("chrome://global/content/BrowserElementChild.js")

#define TABC_LOG(...)
// #define TABC_LOG(...) printf_stderr("TABC: " __VA_ARGS__)

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::dom::ipc;
using namespace mozilla::ipc;
using namespace mozilla::layers;
using namespace mozilla::layout;
using namespace mozilla::docshell;
using namespace mozilla::widget;
using namespace mozilla::jsipc;
using mozilla::layers::GeckoContentController;

NS_IMPL_ISUPPORTS(ContentListener, nsIDOMEventListener)

static const char BEFORE_FIRST_PAINT[] = "before-first-paint";

nsTHashtable<nsPtrHashKey<TabChild>>* TabChild::sVisibleTabs;

typedef nsDataHashtable<nsUint64HashKey, TabChild*> TabChildMap;
static TabChildMap* sTabChildren;
StaticMutex sTabChildrenMutex;

TabChildBase::TabChildBase()
  : mTabChildMessageManager(nullptr)
{
}

TabChildBase::~TabChildBase()
{
  mAnonymousGlobalScopes.Clear();
}

NS_IMPL_CYCLE_COLLECTION_CLASS(TabChildBase)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(TabChildBase)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mTabChildMessageManager)
  tmp->nsMessageManagerScriptExecutor::Unlink();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mWebBrowserChrome)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(TabChildBase)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mTabChildMessageManager)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mWebBrowserChrome)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(TabChildBase)
  tmp->nsMessageManagerScriptExecutor::Trace(aCallbacks, aClosure);
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(TabChildBase)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(TabChildBase)
NS_IMPL_CYCLE_COLLECTING_RELEASE(TabChildBase)

already_AddRefed<nsIDocument>
TabChildBase::GetDocument() const
{
  nsCOMPtr<nsIDocument> doc;
  WebNavigation()->GetDocument(getter_AddRefs(doc));
  return doc.forget();
}

already_AddRefed<nsIPresShell>
TabChildBase::GetPresShell() const
{
  nsCOMPtr<nsIPresShell> result;
  if (nsCOMPtr<nsIDocument> doc = GetDocument()) {
    result = doc->GetShell();
  }
  return result.forget();
}

void
TabChildBase::DispatchMessageManagerMessage(const nsAString& aMessageName,
                                            const nsAString& aJSONData)
{
    AutoSafeJSContext cx;
    JS::Rooted<JS::Value> json(cx, JS::NullValue());
    dom::ipc::StructuredCloneData data;
    if (JS_ParseJSON(cx,
                      static_cast<const char16_t*>(aJSONData.BeginReading()),
                      aJSONData.Length(),
                      &json)) {
        ErrorResult rv;
        data.Write(cx, json, rv);
        if (NS_WARN_IF(rv.Failed())) {
            rv.SuppressException();
            return;
        }
    }

    RefPtr<TabChildMessageManager> kungFuDeathGrip(mTabChildMessageManager);
    RefPtr<nsFrameMessageManager> mm = kungFuDeathGrip->GetMessageManager();
    mm->ReceiveMessage(static_cast<EventTarget*>(kungFuDeathGrip), nullptr,
                       aMessageName, false, &data, nullptr, nullptr, nullptr,
                       IgnoreErrors());
}

bool
TabChildBase::UpdateFrameHandler(const RepaintRequest& aRequest)
{
  MOZ_ASSERT(aRequest.GetScrollId() != ScrollableLayerGuid::NULL_SCROLL_ID);

  if (aRequest.IsRootContent()) {
    if (nsCOMPtr<nsIPresShell> shell = GetPresShell()) {
      // Guard against stale updates (updates meant for a pres shell which
      // has since been torn down and destroyed).
      if (aRequest.GetPresShellId() == shell->GetPresShellId()) {
        ProcessUpdateFrame(aRequest);
        return true;
      }
    }
  } else {
    // aRequest.mIsRoot is false, so we are trying to update a subframe.
    // This requires special handling.
    APZCCallbackHelper::UpdateSubFrame(aRequest);
    return true;
  }
  return true;
}

void
TabChildBase::ProcessUpdateFrame(const RepaintRequest& aRequest)
{
  if (!mTabChildMessageManager) {
      return;
  }

  APZCCallbackHelper::UpdateRootFrame(aRequest);
}

NS_IMETHODIMP
ContentListener::HandleEvent(Event* aEvent)
{
  RemoteDOMEvent remoteEvent;
  remoteEvent.mEvent = aEvent;
  NS_ENSURE_STATE(remoteEvent.mEvent);
  mTabChild->SendEvent(remoteEvent);
  return NS_OK;
}

class TabChild::DelayedDeleteRunnable final
  : public Runnable
  , public nsIRunnablePriority
{
    RefPtr<TabChild> mTabChild;

    // In order to ensure that this runnable runs after everything that could
    // possibly touch this tab, we send it through the event queue twice. The
    // first time it runs at normal priority and the second time it runs at
    // input priority. This ensures that it runs after all events that were in
    // either queue at the time it was first dispatched. mReadyToDelete starts
    // out false (when it runs at normal priority) and is then set to true.
    bool mReadyToDelete = false;

public:
    explicit DelayedDeleteRunnable(TabChild* aTabChild)
      : Runnable("TabChild::DelayedDeleteRunnable")
      , mTabChild(aTabChild)
    {
        MOZ_ASSERT(NS_IsMainThread());
        MOZ_ASSERT(aTabChild);
    }

    NS_DECL_ISUPPORTS_INHERITED

private:
    ~DelayedDeleteRunnable()
    {
        MOZ_ASSERT(NS_IsMainThread());
        MOZ_ASSERT(!mTabChild);
    }

    NS_IMETHOD GetPriority(uint32_t* aPriority) override
    {
      *aPriority = mReadyToDelete
                 ? nsIRunnablePriority::PRIORITY_INPUT
                 : nsIRunnablePriority::PRIORITY_NORMAL;
      return NS_OK;
    }

    NS_IMETHOD
    Run() override
    {
        MOZ_ASSERT(NS_IsMainThread());
        MOZ_ASSERT(mTabChild);

        if (!mReadyToDelete) {
          // This time run this runnable at input priority.
          mReadyToDelete = true;
          MOZ_ALWAYS_SUCCEEDS(NS_DispatchToCurrentThread(this));
          return NS_OK;
        }

        // Check in case ActorDestroy was called after RecvDestroy message.
        // Middleman processes with their own recording child process avoid
        // sending a delete message, so that the parent process does not
        // receive two deletes for the same actor.
        if (mTabChild->IPCOpen() && !recordreplay::parent::IsMiddlemanWithRecordingChild()) {
          Unused << PBrowserChild::Send__delete__(mTabChild);
        }

        mTabChild = nullptr;
        return NS_OK;
    }
};

NS_IMPL_ISUPPORTS_INHERITED(TabChild::DelayedDeleteRunnable,
                            Runnable,
                            nsIRunnablePriority)

namespace {
std::map<TabId, RefPtr<TabChild>>&
NestedTabChildMap()
{
  MOZ_ASSERT(NS_IsMainThread());
  static std::map<TabId, RefPtr<TabChild>> sNestedTabChildMap;
  return sNestedTabChildMap;
}
} // namespace

already_AddRefed<TabChild>
TabChild::FindTabChild(const TabId& aTabId)
{
  auto iter = NestedTabChildMap().find(aTabId);
  if (iter == NestedTabChildMap().end()) {
    return nullptr;
  }
  RefPtr<TabChild> tabChild = iter->second;
  return tabChild.forget();
}

/*static*/ already_AddRefed<TabChild>
TabChild::Create(nsIContentChild* aManager,
                 const TabId& aTabId,
                 const TabId& aSameTabGroupAs,
                 const TabContext &aContext,
                 uint32_t aChromeFlags)
{
  RefPtr<TabChild> groupChild = FindTabChild(aSameTabGroupAs);
  dom::TabGroup* group = groupChild ? groupChild->TabGroup() : nullptr;
  RefPtr<TabChild> iframe = new TabChild(aManager, aTabId, group,
                                         aContext, aChromeFlags);
  return iframe.forget();
}

TabChild::TabChild(nsIContentChild* aManager,
                   const TabId& aTabId,
                   dom::TabGroup* aTabGroup,
                   const TabContext& aContext,
                   uint32_t aChromeFlags)
  : TabContext(aContext)
  , mTabGroup(aTabGroup)
  , mManager(aManager)
  , mChromeFlags(aChromeFlags)
  , mMaxTouchPoints(0)
  , mLayersId{0}
  , mBeforeUnloadListeners(0)
  , mDidFakeShow(false)
  , mNotified(false)
  , mTriedBrowserInit(false)
  , mOrientation(hal::eScreenOrientation_PortraitPrimary)
  , mIgnoreKeyPressEvent(false)
  , mHasValidInnerSize(false)
  , mDestroyed(false)
  , mUniqueId(aTabId)
  , mHasSiblings(false)
  , mIsTransparent(false)
  , mIPCOpen(false)
  , mParentIsActive(false)
  , mDidSetRealShowInfo(false)
  , mDidLoadURLInit(false)
  , mAwaitingLA(false)
  , mSkipKeyPress(false)
  , mLayersObserverEpoch{1}
#if defined(XP_WIN) && defined(ACCESSIBILITY)
  , mNativeWindowHandle(0)
#endif
#if defined(ACCESSIBILITY)
  , mTopLevelDocAccessibleChild(nullptr)
#endif
  , mPendingDocShellIsActive(false)
  , mPendingDocShellReceivedMessage(false)
  , mPendingRenderLayers(false)
  , mPendingRenderLayersReceivedMessage(false)
  , mPendingLayersObserverEpoch{0}
  , mPendingDocShellBlockers(0)
  , mWidgetNativeData(0)
{
  mozilla::HoldJSObjects(this);

  nsWeakPtr weakPtrThis(do_GetWeakReference(static_cast<nsITabChild*>(this)));  // for capture by the lambda
  mSetAllowedTouchBehaviorCallback = [weakPtrThis](uint64_t aInputBlockId,
                                                   const nsTArray<TouchBehaviorFlags>& aFlags)
  {
    if (nsCOMPtr<nsITabChild> tabChild = do_QueryReferent(weakPtrThis)) {
      static_cast<TabChild*>(tabChild.get())->SetAllowedTouchBehavior(aInputBlockId, aFlags);
    }
  };

  // preloaded TabChild should not be added to child map
  if (mUniqueId) {
    MOZ_ASSERT(NestedTabChildMap().find(mUniqueId) == NestedTabChildMap().end());
    NestedTabChildMap()[mUniqueId] = this;
  }
  mCoalesceMouseMoveEvents =
    Preferences::GetBool("dom.event.coalesce_mouse_move");
  if (mCoalesceMouseMoveEvents) {
    mCoalescedMouseEventFlusher = new CoalescedMouseMoveFlusher(this);
  }
}

const CompositorOptions&
TabChild::GetCompositorOptions() const
{
  // If you're calling this before mCompositorOptions is set, well.. don't.
  MOZ_ASSERT(mCompositorOptions);
  return mCompositorOptions.ref();
}

bool
TabChild::AsyncPanZoomEnabled() const
{
  // This might get called by the TouchEvent::PrefEnabled code before we have
  // mCompositorOptions populated (bug 1370089). In that case we just assume
  // APZ is enabled because we're in a content process (because TabChild) and
  // APZ is probably going to be enabled here since e10s is enabled.
  return mCompositorOptions ? mCompositorOptions->UseAPZ() : true;
}

NS_IMETHODIMP
TabChild::Observe(nsISupports *aSubject,
                  const char *aTopic,
                  const char16_t *aData)
{
  if (!strcmp(aTopic, BEFORE_FIRST_PAINT)) {
    if (AsyncPanZoomEnabled()) {
      nsCOMPtr<nsIDocument> subject(do_QueryInterface(aSubject));
      nsCOMPtr<nsIDocument> doc(GetDocument());

      if (SameCOMIdentity(subject, doc)) {
        nsCOMPtr<nsIPresShell> shell(doc->GetShell());
        if (shell) {
          shell->SetIsFirstPaint(true);
        }

        APZCCallbackHelper::InitializeRootDisplayport(shell);
      }
    }
  }

  return NS_OK;
}

void
TabChild::ContentReceivedInputBlock(const ScrollableLayerGuid& aGuid,
                                    uint64_t aInputBlockId,
                                    bool aPreventDefault) const
{
  if (mApzcTreeManager) {
    mApzcTreeManager->ContentReceivedInputBlock(aInputBlockId, aPreventDefault);
  }
}

void
TabChild::SetTargetAPZC(uint64_t aInputBlockId,
                        const nsTArray<ScrollableLayerGuid>& aTargets) const
{
  if (mApzcTreeManager) {
    mApzcTreeManager->SetTargetAPZC(aInputBlockId, aTargets);
  }
}

void
TabChild::SetAllowedTouchBehavior(uint64_t aInputBlockId,
                                  const nsTArray<TouchBehaviorFlags>& aTargets) const
{
  if (mApzcTreeManager) {
    mApzcTreeManager->SetAllowedTouchBehavior(aInputBlockId, aTargets);
  }
}

bool
TabChild::DoUpdateZoomConstraints(const uint32_t& aPresShellId,
                                  const ViewID& aViewId,
                                  const Maybe<ZoomConstraints>& aConstraints)
{
  if (!mApzcTreeManager || mDestroyed) {
    return false;
  }

  ScrollableLayerGuid guid = ScrollableLayerGuid(mLayersId, aPresShellId, aViewId);

  mApzcTreeManager->UpdateZoomConstraints(guid, aConstraints);
  return true;
}

nsresult
TabChild::Init()
{
  if (!mTabGroup) {
    mTabGroup = TabGroup::GetFromActor(this);
  }

  // Directly create our web browser object and store it, so we can start
  // eliminating QIs.
  mWebBrowser = new nsWebBrowser(nsIDocShellTreeItem::typeContentWrapper);
  nsIWebBrowser* webBrowser = mWebBrowser;

  webBrowser->SetContainerWindow(this);
  webBrowser->SetOriginAttributes(OriginAttributesRef());
  mWebNav = do_QueryInterface(webBrowser);
  NS_ASSERTION(mWebNav, "nsWebBrowser doesn't implement nsIWebNavigation?");

  nsCOMPtr<nsIBaseWindow> baseWindow = do_QueryInterface(WebNavigation());
  if (!baseWindow) {
    NS_ERROR("mWebNav doesn't QI to nsIBaseWindow");
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIWidget> widget = nsIWidget::CreatePuppetWidget(this);
  mPuppetWidget = static_cast<PuppetWidget*>(widget.get());
  if (!mPuppetWidget) {
    NS_ERROR("couldn't create fake widget");
    return NS_ERROR_FAILURE;
  }
  mPuppetWidget->InfallibleCreate(
    nullptr, 0,              // no parents
    LayoutDeviceIntRect(0, 0, 0, 0),
    nullptr                  // HandleWidgetEvent
  );

  baseWindow->InitWindow(0, mPuppetWidget, 0, 0, 0, 0);
  baseWindow->Create();

  // Set the tab context attributes then pass to docShell
  NotifyTabContextUpdated(false);

  // IPC uses a WebBrowser object for which DNS prefetching is turned off
  // by default. But here we really want it, so enable it explicitly
  mWebBrowser->SetAllowDNSPrefetch(true);

  nsCOMPtr<nsIDocShell> docShell = do_GetInterface(WebNavigation());
  MOZ_ASSERT(docShell);

  docShell->SetAffectPrivateSessionLifetime(
      mChromeFlags & nsIWebBrowserChrome::CHROME_PRIVATE_LIFETIME);
  nsCOMPtr<nsILoadContext> loadContext = do_GetInterface(WebNavigation());
  MOZ_ASSERT(loadContext);
  loadContext->SetPrivateBrowsing(OriginAttributesRef().mPrivateBrowsingId > 0);
  loadContext->SetRemoteTabs(
      mChromeFlags & nsIWebBrowserChrome::CHROME_REMOTE_WINDOW);

  // Few lines before, baseWindow->Create() will end up creating a new
  // window root in nsGlobalWindow::SetDocShell.
  // Then this chrome event handler, will be inherited to inner windows.
  // We want to also set it to the docshell so that inner windows
  // and any code that has access to the docshell
  // can all listen to the same chrome event handler.
  // XXX: ideally, we would set a chrome event handler earlier,
  // and all windows, even the root one, will use the docshell one.
  nsCOMPtr<nsPIDOMWindowOuter> window = do_GetInterface(WebNavigation());
  NS_ENSURE_TRUE(window, NS_ERROR_FAILURE);
  nsCOMPtr<EventTarget> chromeHandler = window->GetChromeEventHandler();
  docShell->SetChromeEventHandler(chromeHandler);

  if (window->GetCurrentInnerWindow()) {
    window->SetKeyboardIndicators(ShowAccelerators(), ShowFocusRings());
  } else {
    // Skip ShouldShowFocusRing check if no inner window is available
    window->SetInitialKeyboardIndicators(ShowAccelerators(), ShowFocusRings());
  }

  nsContentUtils::SetScrollbarsVisibility(window->GetDocShell(),
    !!(mChromeFlags & nsIWebBrowserChrome::CHROME_SCROLLBARS));

  nsWeakPtr weakPtrThis = do_GetWeakReference(static_cast<nsITabChild*>(this));  // for capture by the lambda
  ContentReceivedInputBlockCallback callback(
      [weakPtrThis](const ScrollableLayerGuid& aGuid,
                    uint64_t aInputBlockId,
                    bool aPreventDefault)
      {
        if (nsCOMPtr<nsITabChild> tabChild = do_QueryReferent(weakPtrThis)) {
          static_cast<TabChild*>(tabChild.get())->ContentReceivedInputBlock(aGuid, aInputBlockId, aPreventDefault);
        }
      });
  mAPZEventState = new APZEventState(mPuppetWidget, std::move(callback));

  mIPCOpen = true;

  // Recording/replaying processes use their own compositor.
  if (recordreplay::IsRecordingOrReplaying()) {
    mPuppetWidget->CreateCompositor();
  }

  return NS_OK;
}

void
TabChild::NotifyTabContextUpdated(bool aIsPreallocated)
{
  nsCOMPtr<nsIDocShell> docShell = do_GetInterface(WebNavigation());
  MOZ_ASSERT(docShell);

  if (!docShell) {
    return;
  }

  UpdateFrameType();

  if (aIsPreallocated)  {
    nsDocShell::Cast(docShell)->SetOriginAttributes(OriginAttributesRef());
  }

  // Set SANDBOXED_AUXILIARY_NAVIGATION flag if this is a receiver page.
  if (!PresentationURL().IsEmpty()) {
    docShell->SetSandboxFlags(SANDBOXED_AUXILIARY_NAVIGATION);
  }
}

void
TabChild::UpdateFrameType()
{
  nsCOMPtr<nsIDocShell> docShell = do_GetInterface(WebNavigation());
  MOZ_ASSERT(docShell);

  // TODO: Bug 1252794 - remove frameType from nsIDocShell.idl
  docShell->SetFrameType(IsMozBrowserElement() ? nsIDocShell::FRAME_TYPE_BROWSER :
                           nsIDocShell::FRAME_TYPE_REGULAR);
}

NS_IMPL_CYCLE_COLLECTION_CLASS(TabChild)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(TabChild, TabChildBase)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mWebNav)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(TabChild, TabChildBase)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mWebNav)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(TabChild, TabChildBase)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(TabChild)
  NS_INTERFACE_MAP_ENTRY(nsIWebBrowserChrome)
  NS_INTERFACE_MAP_ENTRY(nsIWebBrowserChrome2)
  NS_INTERFACE_MAP_ENTRY(nsIEmbeddingSiteWindow)
  NS_INTERFACE_MAP_ENTRY(nsIWebBrowserChromeFocus)
  NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
  NS_INTERFACE_MAP_ENTRY(nsIWindowProvider)
  NS_INTERFACE_MAP_ENTRY(nsITabChild)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsITooltipListener)
NS_INTERFACE_MAP_END_INHERITING(TabChildBase)

NS_IMPL_ADDREF_INHERITED(TabChild, TabChildBase);
NS_IMPL_RELEASE_INHERITED(TabChild, TabChildBase);

NS_IMETHODIMP
TabChild::SetStatus(uint32_t aStatusType, const char16_t* aStatus)
{
  return SetStatusWithContext(aStatusType,
      aStatus ? static_cast<const nsString &>(nsDependentString(aStatus))
              : EmptyString(),
      nullptr);
}

NS_IMETHODIMP
TabChild::GetChromeFlags(uint32_t* aChromeFlags)
{
  *aChromeFlags = mChromeFlags;
  return NS_OK;
}

NS_IMETHODIMP
TabChild::SetChromeFlags(uint32_t aChromeFlags)
{
  NS_WARNING("trying to SetChromeFlags from content process?");

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TabChild::RemoteSizeShellTo(int32_t aWidth, int32_t aHeight,
                            int32_t aShellItemWidth, int32_t aShellItemHeight)
{
  nsCOMPtr<nsIDocShell> ourDocShell = do_GetInterface(WebNavigation());
  nsCOMPtr<nsIBaseWindow> docShellAsWin(do_QueryInterface(ourDocShell));
  NS_ENSURE_STATE(docShellAsWin);

  int32_t width, height;
  docShellAsWin->GetSize(&width, &height);

  uint32_t flags = 0;
  if (width == aWidth) {
    flags |= nsIEmbeddingSiteWindow::DIM_FLAGS_IGNORE_CX;
  }

  if (height == aHeight) {
    flags |= nsIEmbeddingSiteWindow::DIM_FLAGS_IGNORE_CY;
  }

  bool sent = SendSizeShellTo(flags, aWidth, aHeight, aShellItemWidth, aShellItemHeight);

  return sent ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
TabChild::RemoteDropLinks(uint32_t aLinksCount,
                          nsIDroppedLinkItem** aLinks)
{
  nsTArray<nsString> linksArray;
  nsresult rv = NS_OK;
  for (uint32_t i = 0; i < aLinksCount; i++) {
    nsString tmp;
    rv = aLinks[i]->GetUrl(tmp);
    if (NS_FAILED(rv)) {
      return rv;
    }
    linksArray.AppendElement(tmp);

    rv = aLinks[i]->GetName(tmp);
    if (NS_FAILED(rv)) {
      return rv;
    }
    linksArray.AppendElement(tmp);

    rv = aLinks[i]->GetType(tmp);
    if (NS_FAILED(rv)) {
      return rv;
    }
    linksArray.AppendElement(tmp);
  }
  bool sent = SendDropLinks(linksArray);

  return sent ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
TabChild::ShowAsModal()
{
  NS_WARNING("TabChild::ShowAsModal not supported in TabChild");

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TabChild::IsWindowModal(bool* aRetVal)
{
  *aRetVal = false;
  return NS_OK;
}

NS_IMETHODIMP
TabChild::SetStatusWithContext(uint32_t aStatusType,
                               const nsAString& aStatusText,
                               nsISupports* aStatusContext)
{
  // We can only send the status after the ipc machinery is set up
  if (IPCOpen()) {
    SendSetStatus(aStatusType, nsString(aStatusText));
  }
  return NS_OK;
}

NS_IMETHODIMP
TabChild::SetDimensions(uint32_t aFlags, int32_t aX, int32_t aY,
                        int32_t aCx, int32_t aCy)
{
  // The parent is in charge of the dimension changes. If JS code wants to
  // change the dimensions (moveTo, screenX, etc.) we send a message to the
  // parent about the new requested dimension, the parent does the resize/move
  // then send a message to the child to update itself. For APIs like screenX
  // this function is called with the current value for the non-changed values.
  // In a series of calls like window.screenX = 10; window.screenY = 10; for
  // the second call, since screenX is not yet updated we might accidentally
  // reset back screenX to it's old value. To avoid this if a parameter did not
  // change we want the parent to ignore its value.
  int32_t x, y, cx, cy;
  GetDimensions(aFlags, &x, &y, &cx, &cy);

  if (x == aX) {
    aFlags |= nsIEmbeddingSiteWindow::DIM_FLAGS_IGNORE_X;
  }

  if (y == aY) {
    aFlags |= nsIEmbeddingSiteWindow::DIM_FLAGS_IGNORE_Y;
  }

  if (cx == aCx) {
    aFlags |= nsIEmbeddingSiteWindow::DIM_FLAGS_IGNORE_CX;
  }

  if (cy == aCy) {
    aFlags |= nsIEmbeddingSiteWindow::DIM_FLAGS_IGNORE_CY;
  }

  Unused << SendSetDimensions(aFlags, aX, aY, aCx, aCy);

  return NS_OK;
}

NS_IMETHODIMP
TabChild::GetDimensions(uint32_t aFlags, int32_t* aX,
                             int32_t* aY, int32_t* aCx, int32_t* aCy)
{
  ScreenIntRect rect = GetOuterRect();
  if (aX) {
    *aX = rect.x;
  }
  if (aY) {
    *aY = rect.y;
  }
  if (aCx) {
    *aCx = rect.width;
  }
  if (aCy) {
    *aCy = rect.height;
  }

  return NS_OK;
}

NS_IMETHODIMP
TabChild::SetFocus()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TabChild::GetVisibility(bool* aVisibility)
{
  *aVisibility = true;
  return NS_OK;
}

NS_IMETHODIMP
TabChild::SetVisibility(bool aVisibility)
{
  // should the platform support this? Bug 666365
  return NS_OK;
}

NS_IMETHODIMP
TabChild::GetTitle(nsAString& aTitle)
{
  NS_WARNING("TabChild::GetTitle not supported in TabChild");

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TabChild::SetTitle(const nsAString& aTitle)
{
  // JavaScript sends the "DOMTitleChanged" event to the parent
  // via the message manager.
  return NS_OK;
}

NS_IMETHODIMP
TabChild::GetSiteWindow(void** aSiteWindow)
{
  NS_WARNING("TabChild::GetSiteWindow not supported in TabChild");

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TabChild::Blur()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TabChild::FocusNextElement(bool aForDocumentNavigation)
{
  SendMoveFocus(true, aForDocumentNavigation);
  return NS_OK;
}

NS_IMETHODIMP
TabChild::FocusPrevElement(bool aForDocumentNavigation)
{
  SendMoveFocus(false, aForDocumentNavigation);
  return NS_OK;
}

NS_IMETHODIMP
TabChild::GetInterface(const nsIID & aIID, void **aSink)
{
    if (aIID.Equals(NS_GET_IID(nsIWebBrowserChrome3))) {
      NS_IF_ADDREF(((nsISupports *) (*aSink = mWebBrowserChrome)));
      return NS_OK;
    }

    // XXXbz should we restrict the set of interfaces we hand out here?
    // See bug 537429
    return QueryInterface(aIID, aSink);
}

NS_IMETHODIMP
TabChild::ProvideWindow(mozIDOMWindowProxy* aParent,
                        uint32_t aChromeFlags,
                        bool aCalledFromJS,
                        bool aPositionSpecified, bool aSizeSpecified,
                        nsIURI* aURI, const nsAString& aName,
                        const nsACString& aFeatures, bool aForceNoOpener,
                        nsDocShellLoadState* aLoadState, bool* aWindowIsNew,
                        mozIDOMWindowProxy** aReturn)
{
    *aReturn = nullptr;

    // If aParent is inside an <iframe mozbrowser> and this isn't a request to
    // open a modal-type window, we're going to create a new <iframe mozbrowser>
    // and return its window here.
    nsCOMPtr<nsIDocShell> docshell = do_GetInterface(aParent);
    bool iframeMoz = (docshell && docshell->GetIsInMozBrowser() &&
                      !(aChromeFlags & (nsIWebBrowserChrome::CHROME_MODAL |
                                        nsIWebBrowserChrome::CHROME_OPENAS_DIALOG |
                                        nsIWebBrowserChrome::CHROME_OPENAS_CHROME)));

    if (!iframeMoz) {
      int32_t openLocation =
        nsWindowWatcher::GetWindowOpenLocation(nsPIDOMWindowOuter::From(aParent),
                                               aChromeFlags, aCalledFromJS,
                                               aPositionSpecified, aSizeSpecified);

      // If it turns out we're opening in the current browser, just hand over the
      // current browser's docshell.
      if (openLocation == nsIBrowserDOMWindow::OPEN_CURRENTWINDOW) {
        nsCOMPtr<nsIWebBrowser> browser = do_GetInterface(WebNavigation());
        *aWindowIsNew = false;
        return browser->GetContentDOMWindow(aReturn);
      }
    }

    // Note that ProvideWindowCommon may return NS_ERROR_ABORT if the
    // open window call was canceled.  It's important that we pass this error
    // code back to our caller.
    ContentChild* cc = ContentChild::GetSingleton();
    return cc->ProvideWindowCommon(this,
                                   aParent,
                                   iframeMoz,
                                   aChromeFlags,
                                   aCalledFromJS,
                                   aPositionSpecified,
                                   aSizeSpecified,
                                   aURI,
                                   aName,
                                   aFeatures,
                                   aForceNoOpener,
                                   aLoadState,
                                   aWindowIsNew,
                                   aReturn);
}

void
TabChild::DestroyWindow()
{
    if (mCoalescedMouseEventFlusher) {
      mCoalescedMouseEventFlusher->RemoveObserver();
      mCoalescedMouseEventFlusher = nullptr;
    }

    // In case we don't have chance to process all entries, clean all data in
    // the queue.
    while (mToBeDispatchedMouseData.GetSize() > 0) {
      UniquePtr<CoalescedMouseData> data(
        static_cast<CoalescedMouseData*>(mToBeDispatchedMouseData.PopFront()));
      data.reset();
    }

    nsCOMPtr<nsIBaseWindow> baseWindow = do_QueryInterface(WebNavigation());
    if (baseWindow)
        baseWindow->Destroy();

    if (mPuppetWidget) {
        mPuppetWidget->Destroy();
    }

    mLayersConnected = Nothing();

    if (mLayersId.IsValid()) {
      StaticMutexAutoLock lock(sTabChildrenMutex);

      MOZ_ASSERT(sTabChildren);
      sTabChildren->Remove(uint64_t(mLayersId));
      if (!sTabChildren->Count()) {
        delete sTabChildren;
        sTabChildren = nullptr;
      }
      mLayersId = layers::LayersId{0};
    }
}

void
TabChild::ActorDestroy(ActorDestroyReason why)
{
  mIPCOpen = false;

  DestroyWindow();

  if (mTabChildMessageManager) {
    // We should have a message manager if the global is alive, but it
    // seems sometimes we don't.  Assert in aurora/nightly, but don't
    // crash in release builds.
    MOZ_DIAGNOSTIC_ASSERT(mTabChildMessageManager->GetMessageManager());
    if (mTabChildMessageManager->GetMessageManager()) {
      // The messageManager relays messages via the TabChild which
      // no longer exists.
      mTabChildMessageManager->DisconnectMessageManager();
    }
  }

  CompositorBridgeChild* compositorChild = CompositorBridgeChild::Get();
  if (compositorChild) {
    compositorChild->CancelNotifyAfterRemotePaint(this);
  }

  if (GetTabId() != 0) {
    NestedTabChildMap().erase(GetTabId());
  }
}

TabChild::~TabChild()
{
  if (sVisibleTabs) {
    sVisibleTabs->RemoveEntry(this);
    if (sVisibleTabs->IsEmpty()) {
      delete sVisibleTabs;
      sVisibleTabs = nullptr;
    }
  }

  DestroyWindow();

  nsCOMPtr<nsIWebBrowser> webBrowser = do_QueryInterface(WebNavigation());
  if (webBrowser) {
    webBrowser->SetContainerWindow(nullptr);
  }

  mozilla::DropJSObjects(this);
}

mozilla::ipc::IPCResult
TabChild::RecvLoadURL(const nsCString& aURI,
                      const ShowInfo& aInfo)
{
  if (!mDidLoadURLInit) {
    mDidLoadURLInit = true;
    if (!InitTabChildMessageManager()) {
      return IPC_FAIL_NO_REASON(this);
    }

    ApplyShowInfo(aInfo);
  }

  nsresult rv =
    WebNavigation()->LoadURI(NS_ConvertUTF8toUTF16(aURI),
                             nsIWebNavigation::LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP |
                             nsIWebNavigation::LOAD_FLAGS_DISALLOW_INHERIT_PRINCIPAL,
                             nullptr, nullptr, nullptr, nsContentUtils::GetSystemPrincipal());
  if (NS_FAILED(rv)) {
      NS_WARNING("WebNavigation()->LoadURI failed. Eating exception, what else can I do?");
  }

  CrashReporter::AnnotateCrashReport(CrashReporter::Annotation::URL, aURI);

  return IPC_OK();
}

void
TabChild::DoFakeShow(const ShowInfo& aShowInfo)
{
  RecvShow(ScreenIntSize(0, 0), aShowInfo, mParentIsActive, nsSizeMode_Normal);
  mDidFakeShow = true;
}

void
TabChild::ApplyShowInfo(const ShowInfo& aInfo)
{
  // Even if we already set real show info, the dpi / rounding & scale may still
  // be invalid (if TabParent wasn't able to get widget it would just send 0).
  // So better to always set up-to-date values here.
  if (aInfo.dpi() > 0) {
    mPuppetWidget->UpdateBackingScaleCache(aInfo.dpi(),
                                           aInfo.widgetRounding(),
                                           aInfo.defaultScale());
  }

  if (mDidSetRealShowInfo) {
    return;
  }

  if (!aInfo.fakeShowInfo()) {
    // Once we've got one ShowInfo from parent, no need to update the values
    // anymore.
    mDidSetRealShowInfo = true;
  }

  nsCOMPtr<nsIDocShell> docShell = do_GetInterface(WebNavigation());
  if (docShell) {
    nsCOMPtr<nsIDocShellTreeItem> item = do_GetInterface(docShell);
    if (IsMozBrowser()) {
      // B2G allows window.name to be set by changing the name attribute on the
      // <iframe mozbrowser> element. window.open calls cause this attribute to
      // be set to the correct value. A normal <xul:browser> element has no such
      // attribute. The data we get here comes from reading the attribute, so we
      // shouldn't trust it for <xul:browser> elements.
      item->SetName(aInfo.name());
    }
    docShell->SetFullscreenAllowed(aInfo.fullscreenAllowed());
    if (aInfo.isPrivate()) {
      nsCOMPtr<nsILoadContext> context = do_GetInterface(docShell);
      // No need to re-set private browsing mode.
      if (!context->UsePrivateBrowsing()) {
        if (docShell->GetHasLoadedNonBlankURI()) {
          nsContentUtils::ReportToConsoleNonLocalized(
            NS_LITERAL_STRING("We should not switch to Private Browsing after loading a document."),
            nsIScriptError::warningFlag,
            NS_LITERAL_CSTRING("mozprivatebrowsing"),
            nullptr);
        } else {
          OriginAttributes attrs(nsDocShell::Cast(docShell)->GetOriginAttributes());
          attrs.SyncAttributesWithPrivateBrowsing(true);
          nsDocShell::Cast(docShell)->SetOriginAttributes(attrs);
        }
      }
    }
  }
  mIsTransparent = aInfo.isTransparent();
}

mozilla::ipc::IPCResult
TabChild::RecvShow(const ScreenIntSize& aSize,
                   const ShowInfo& aInfo,
                   const bool& aParentIsActive,
                   const nsSizeMode& aSizeMode)
{
  bool res = true;

  mPuppetWidget->SetSizeMode(aSizeMode);
  if (!mDidFakeShow) {
    nsCOMPtr<nsIBaseWindow> baseWindow = do_QueryInterface(WebNavigation());
    if (!baseWindow) {
        NS_ERROR("WebNavigation() doesn't QI to nsIBaseWindow");
        return IPC_FAIL_NO_REASON(this);
    }

    baseWindow->SetVisibility(true);
    res = InitTabChildMessageManager();
  }

  ApplyShowInfo(aInfo);
  RecvParentActivated(aParentIsActive);

  if (!res) {
    return IPC_FAIL_NO_REASON(this);
  }

  // We have now done enough initialization for the record/replay system to
  // create checkpoints. Create a checkpoint now, in case this process never
  // paints later on (the usual place where checkpoints occur).
  if (recordreplay::IsRecordingOrReplaying()) {
    recordreplay::child::CreateCheckpoint();
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult
TabChild::RecvInitRendering(const TextureFactoryIdentifier& aTextureFactoryIdentifier,
                            const layers::LayersId& aLayersId,
                            const CompositorOptions& aCompositorOptions,
                            const bool& aLayersConnected)
{
  mLayersConnected = Some(aLayersConnected);
  InitRenderingState(aTextureFactoryIdentifier, aLayersId, aCompositorOptions);
  return IPC_OK();
}

mozilla::ipc::IPCResult
TabChild::RecvUpdateDimensions(const DimensionInfo& aDimensionInfo)
{
    // When recording/replaying we need to make sure the dimensions are up to
    // date on the compositor used in this process.
    if (mLayersConnected.isNothing() && !recordreplay::IsRecordingOrReplaying()) {
        return IPC_OK();
    }

    mUnscaledOuterRect = aDimensionInfo.rect();
    mClientOffset = aDimensionInfo.clientOffset();
    mChromeOffset = aDimensionInfo.chromeOffset();

    mOrientation = aDimensionInfo.orientation();
    SetUnscaledInnerSize(aDimensionInfo.size());
    if (!mHasValidInnerSize &&
        aDimensionInfo.size().width != 0 &&
        aDimensionInfo.size().height != 0) {
      mHasValidInnerSize = true;
    }

    ScreenIntSize screenSize = GetInnerSize();
    ScreenIntRect screenRect = GetOuterRect();

    // Set the size on the document viewer before we update the widget and
    // trigger a reflow. Otherwise the MobileViewportManager reads the stale
    // size from the content viewer when it computes a new CSS viewport.
    nsCOMPtr<nsIBaseWindow> baseWin = do_QueryInterface(WebNavigation());
    baseWin->SetPositionAndSize(0, 0, screenSize.width, screenSize.height,
                                nsIBaseWindow::eRepaint);

    mPuppetWidget->Resize(screenRect.x + mClientOffset.x + mChromeOffset.x,
                          screenRect.y + mClientOffset.y + mChromeOffset.y,
                          screenSize.width, screenSize.height, true);

    return IPC_OK();
}

mozilla::ipc::IPCResult
TabChild::RecvSizeModeChanged(const nsSizeMode& aSizeMode)
{
  mPuppetWidget->SetSizeMode(aSizeMode);
  if (!mPuppetWidget->IsVisible()) {
    return IPC_OK();
  }
  nsCOMPtr<nsIDocument> document(GetDocument());
  nsPresContext* presContext = document->GetPresContext();
  if (presContext) {
    presContext->SizeModeChanged(aSizeMode);
  }
  return IPC_OK();
}

bool
TabChild::UpdateFrame(const RepaintRequest& aRequest)
{
  return TabChildBase::UpdateFrameHandler(aRequest);
}

mozilla::ipc::IPCResult
TabChild::RecvSuppressDisplayport(const bool& aEnabled)
{
  if (nsCOMPtr<nsIPresShell> shell = GetPresShell()) {
    shell->SuppressDisplayport(aEnabled);
  }
  return IPC_OK();
}

void
TabChild::HandleDoubleTap(const CSSPoint& aPoint, const Modifiers& aModifiers,
                          const ScrollableLayerGuid& aGuid)
{
  TABC_LOG("Handling double tap at %s with %p %p\n",
    Stringify(aPoint).c_str(),
    mTabChildMessageManager ? mTabChildMessageManager->GetWrapper() : nullptr,
    mTabChildMessageManager.get());

  if (!mTabChildMessageManager) {
    return;
  }

  // Note: there is nothing to do with the modifiers here, as we are not
  // synthesizing any sort of mouse event.
  nsCOMPtr<nsIDocument> document = GetDocument();
  CSSRect zoomToRect = CalculateRectToZoomTo(document, aPoint);
  // The double-tap can be dispatched by any scroll frame (so |aGuid| could be
  // the guid of any scroll frame), but the zoom-to-rect operation must be
  // performed by the root content scroll frame, so query its identifiers
  // for the SendZoomToRect() call rather than using the ones from |aGuid|.
  uint32_t presShellId;
  ViewID viewId;
  if (APZCCallbackHelper::GetOrCreateScrollIdentifiers(
      document->GetDocumentElement(), &presShellId, &viewId) && mApzcTreeManager) {
    ScrollableLayerGuid guid(mLayersId, presShellId, viewId);

    mApzcTreeManager->ZoomToRect(guid, zoomToRect, DEFAULT_BEHAVIOR);
  }
}

mozilla::ipc::IPCResult
TabChild::RecvHandleTap(const GeckoContentController::TapType& aType,
                        const LayoutDevicePoint& aPoint,
                        const Modifiers& aModifiers,
                        const ScrollableLayerGuid& aGuid,
                        const uint64_t& aInputBlockId)
{
  nsCOMPtr<nsIPresShell> presShell = GetPresShell();
  if (!presShell) {
    return IPC_OK();
  }
  if (!presShell->GetPresContext()) {
    return IPC_OK();
  }
  CSSToLayoutDeviceScale scale(presShell->GetPresContext()->CSSToDevPixelScale());
  CSSPoint point = APZCCallbackHelper::ApplyCallbackTransform(aPoint / scale, aGuid);

  switch (aType) {
  case GeckoContentController::TapType::eSingleTap:
    if (mTabChildMessageManager) {
      mAPZEventState->ProcessSingleTap(point, scale, aModifiers, aGuid, 1);
    }
    break;
  case GeckoContentController::TapType::eDoubleTap:
    HandleDoubleTap(point, aModifiers, aGuid);
    break;
  case GeckoContentController::TapType::eSecondTap:
    if (mTabChildMessageManager) {
      mAPZEventState->ProcessSingleTap(point, scale, aModifiers, aGuid, 2);
    }
    break;
  case GeckoContentController::TapType::eLongTap:
    if (mTabChildMessageManager) {
      mAPZEventState->ProcessLongTap(presShell, point, scale, aModifiers, aGuid,
          aInputBlockId);
    }
    break;
  case GeckoContentController::TapType::eLongTapUp:
    if (mTabChildMessageManager) {
      mAPZEventState->ProcessLongTapUp(presShell, point, scale, aModifiers);
    }
    break;
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
TabChild::RecvNormalPriorityHandleTap(
  const GeckoContentController::TapType& aType,
  const LayoutDevicePoint& aPoint,
  const Modifiers& aModifiers,
  const ScrollableLayerGuid& aGuid,
  const uint64_t& aInputBlockId)
{
  return RecvHandleTap(aType, aPoint, aModifiers, aGuid, aInputBlockId);
}

bool
TabChild::NotifyAPZStateChange(const ViewID& aViewId,
                               const layers::GeckoContentController::APZStateChange& aChange,
                               const int& aArg)
{
  mAPZEventState->ProcessAPZStateChange(aViewId, aChange, aArg);
  if (aChange == layers::GeckoContentController::APZStateChange::eTransformEnd) {
    // This is used by tests to determine when the APZ is done doing whatever
    // it's doing. XXX generify this as needed when writing additional tests.
    nsCOMPtr<nsIObserverService> observerService = mozilla::services::GetObserverService();
    observerService->NotifyObservers(nullptr, "APZ:TransformEnd", nullptr);
  }
  return true;
}

void
TabChild::StartScrollbarDrag(const layers::AsyncDragMetrics& aDragMetrics)
{
  ScrollableLayerGuid guid(mLayersId, aDragMetrics.mPresShellId,
                           aDragMetrics.mViewId);

  if (mApzcTreeManager) {
    mApzcTreeManager->StartScrollbarDrag(guid, aDragMetrics);
  }
}

void
TabChild::ZoomToRect(const uint32_t& aPresShellId,
                     const ScrollableLayerGuid::ViewID& aViewId,
                     const CSSRect& aRect,
                     const uint32_t& aFlags)
{
  ScrollableLayerGuid guid(mLayersId, aPresShellId, aViewId);

  if (mApzcTreeManager) {
    mApzcTreeManager->ZoomToRect(guid, aRect, aFlags);
  }
}

mozilla::ipc::IPCResult
TabChild::RecvActivate()
{
  MOZ_ASSERT(mWebBrowser);
  // Ensure that the PresShell exists, otherwise focusing
  // is definitely not going to work. GetPresShell should
  // create a PresShell if one doesn't exist yet.
  nsCOMPtr<nsIPresShell> presShell = GetPresShell();
  MOZ_ASSERT(presShell);

  mWebBrowser->FocusActivate();
  return IPC_OK();
}

mozilla::ipc::IPCResult
TabChild::RecvDeactivate()
{
  MOZ_ASSERT(mWebBrowser);
  mWebBrowser->FocusDeactivate();
  return IPC_OK();
}

mozilla::ipc::IPCResult
TabChild::RecvParentActivated(const bool& aActivated)
{
  mParentIsActive = aActivated;

  nsFocusManager* fm = nsFocusManager::GetFocusManager();
  NS_ENSURE_TRUE(fm, IPC_OK());

  nsCOMPtr<nsPIDOMWindowOuter> window = do_GetInterface(WebNavigation());
  fm->ParentActivated(window, aActivated);
  return IPC_OK();
}

mozilla::ipc::IPCResult
TabChild::RecvSetKeyboardIndicators(const UIStateChangeType& aShowAccelerators,
                                    const UIStateChangeType& aShowFocusRings)
{
  nsCOMPtr<nsPIDOMWindowOuter> window = do_GetInterface(WebNavigation());
  NS_ENSURE_TRUE(window, IPC_OK());

  window->SetKeyboardIndicators(aShowAccelerators, aShowFocusRings);
  return IPC_OK();
}

mozilla::ipc::IPCResult
TabChild::RecvStopIMEStateManagement()
{
  IMEStateManager::StopIMEStateManagement();
  return IPC_OK();
}

mozilla::ipc::IPCResult
TabChild::RecvMouseEvent(const nsString& aType,
                         const float&    aX,
                         const float&    aY,
                         const int32_t&  aButton,
                         const int32_t&  aClickCount,
                         const int32_t&  aModifiers,
                         const bool&     aIgnoreRootScrollFrame)
{
  APZCCallbackHelper::DispatchMouseEvent(GetPresShell(), aType,
                                         CSSPoint(aX, aY), aButton, aClickCount,
                                         aModifiers, aIgnoreRootScrollFrame,
                                         MouseEvent_Binding::MOZ_SOURCE_UNKNOWN,
                                         0 /* Use the default value here. */);
  return IPC_OK();
}

void
TabChild::ProcessPendingCoalescedMouseDataAndDispatchEvents()
{
  if (!mCoalesceMouseMoveEvents || !mCoalescedMouseEventFlusher) {
    // We don't enable mouse coalescing or we are destroying TabChild.
    return;
  }

  // We may reentry the event loop and push more data to
  // mToBeDispatchedMouseData while dispatching an event.

  // We may have some pending coalesced data while dispatch an event and reentry
  // the event loop. In that case we don't have chance to consume the remainding
  // pending data until we get new mouse events. Get some helps from
  // mCoalescedMouseEventFlusher to trigger it.
  mCoalescedMouseEventFlusher->StartObserver();

  while (mToBeDispatchedMouseData.GetSize() > 0) {
    UniquePtr<CoalescedMouseData> data(
      static_cast<CoalescedMouseData*>(mToBeDispatchedMouseData.PopFront()));

    UniquePtr<WidgetMouseEvent> event = data->TakeCoalescedEvent();
    if (event) {
      // Dispatch the pending events. Using HandleRealMouseButtonEvent
      // to bypass the coalesce handling in RecvRealMouseMoveEvent. Can't use
      // RecvRealMouseButtonEvent because we may also put some mouse events
      // other than mousemove.
      HandleRealMouseButtonEvent(*event,
                                 data->GetScrollableLayerGuid(),
                                 data->GetInputBlockId());
    }
  }
  // mCoalescedMouseEventFlusher may be destroyed when reentrying the event
  // loop.
  if (mCoalescedMouseEventFlusher) {
    mCoalescedMouseEventFlusher->RemoveObserver();
  }
}

void
TabChild::FlushAllCoalescedMouseData()
{
  MOZ_ASSERT(mCoalesceMouseMoveEvents);

  // Move all entries from mCoalescedMouseData to mToBeDispatchedMouseData.
  for (auto iter = mCoalescedMouseData.Iter(); !iter.Done(); iter.Next()) {
    CoalescedMouseData* data = iter.UserData();
    if (!data || data->IsEmpty()) {
      continue;
    }
    UniquePtr<CoalescedMouseData> dispatchData =
      MakeUnique<CoalescedMouseData>();

    dispatchData->RetrieveDataFrom(*data);
    mToBeDispatchedMouseData.Push(dispatchData.release());
  }
  mCoalescedMouseData.Clear();
}

mozilla::ipc::IPCResult
TabChild::RecvRealMouseMoveEvent(const WidgetMouseEvent& aEvent,
                                 const ScrollableLayerGuid& aGuid,
                                 const uint64_t& aInputBlockId)
{
  if (mCoalesceMouseMoveEvents && mCoalescedMouseEventFlusher) {
    CoalescedMouseData* data = mCoalescedMouseData.LookupOrAdd(aEvent.pointerId);
    MOZ_ASSERT(data);
    if (data->CanCoalesce(aEvent, aGuid, aInputBlockId)) {
      data->Coalesce(aEvent, aGuid, aInputBlockId);
      mCoalescedMouseEventFlusher->StartObserver();
      return IPC_OK();
    }
    // Can't coalesce current mousemove event. Put the coalesced mousemove data
    // with the same pointer id to mToBeDispatchedMouseData, coalesce the
    // current one, and process all pending data in mToBeDispatchedMouseData.
    UniquePtr<CoalescedMouseData> dispatchData =
      MakeUnique<CoalescedMouseData>();

    dispatchData->RetrieveDataFrom(*data);
    mToBeDispatchedMouseData.Push(dispatchData.release());

    // Put new data to replace the old one in the hash table.
    CoalescedMouseData* newData = new CoalescedMouseData();
    mCoalescedMouseData.Put(aEvent.pointerId, newData);
    newData->Coalesce(aEvent, aGuid, aInputBlockId);

    // Dispatch all pending mouse events.
    ProcessPendingCoalescedMouseDataAndDispatchEvents();
    mCoalescedMouseEventFlusher->StartObserver();
  } else if (!RecvRealMouseButtonEvent(aEvent, aGuid, aInputBlockId)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
TabChild::RecvNormalPriorityRealMouseMoveEvent(const WidgetMouseEvent& aEvent,
                                               const ScrollableLayerGuid& aGuid,
                                               const uint64_t& aInputBlockId)
{
  return RecvRealMouseMoveEvent(aEvent, aGuid, aInputBlockId);
}

mozilla::ipc::IPCResult
TabChild::RecvSynthMouseMoveEvent(const WidgetMouseEvent& aEvent,
                                  const ScrollableLayerGuid& aGuid,
                                  const uint64_t& aInputBlockId)
{
  if (!RecvRealMouseButtonEvent(aEvent, aGuid, aInputBlockId)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
TabChild::RecvNormalPrioritySynthMouseMoveEvent(const WidgetMouseEvent& aEvent,
                                                const ScrollableLayerGuid& aGuid,
                                                const uint64_t& aInputBlockId)
{
  return RecvSynthMouseMoveEvent(aEvent, aGuid, aInputBlockId);
}

mozilla::ipc::IPCResult
TabChild::RecvRealMouseButtonEvent(const WidgetMouseEvent& aEvent,
                                   const ScrollableLayerGuid& aGuid,
                                   const uint64_t& aInputBlockId)
{
  if (mCoalesceMouseMoveEvents && mCoalescedMouseEventFlusher &&
      aEvent.mMessage != eMouseMove) {
    // When receiving a mouse event other than mousemove, we have to dispatch
    // all coalesced events before it. However, we can't dispatch all pending
    // coalesced events directly because we may reentry the event loop while
    // dispatching. To make sure we won't dispatch disorder events, we move all
    // coalesced mousemove events and current event to a deque to dispatch them.
    // When reentrying the event loop and dispatching more events, we put new
    // events in the end of the nsQueue and dispatch events from the beginning.
    FlushAllCoalescedMouseData();

    UniquePtr<CoalescedMouseData> dispatchData =
      MakeUnique<CoalescedMouseData>();

    dispatchData->Coalesce(aEvent, aGuid, aInputBlockId);
    mToBeDispatchedMouseData.Push(dispatchData.release());

    ProcessPendingCoalescedMouseDataAndDispatchEvents();
    return IPC_OK();
  }
  HandleRealMouseButtonEvent(aEvent, aGuid, aInputBlockId);
  return IPC_OK();
}

void
TabChild::HandleRealMouseButtonEvent(const WidgetMouseEvent& aEvent,
                                     const ScrollableLayerGuid& aGuid,
                                     const uint64_t& aInputBlockId)
{
  // Mouse events like eMouseEnterIntoWidget, that are created in the parent
  // process EventStateManager code, have an input block id which they get from
  // the InputAPZContext in the parent process stack. However, they did not
  // actually go through the APZ code and so their mHandledByAPZ flag is false.
  // Since thos events didn't go through APZ, we don't need to send
  // notifications for them.
  UniquePtr<DisplayportSetListener> postLayerization;
  if (aInputBlockId && aEvent.mFlags.mHandledByAPZ) {
    nsCOMPtr<nsIDocument> document(GetDocument());
    postLayerization = APZCCallbackHelper::SendSetTargetAPZCNotification(
        mPuppetWidget, document, aEvent, aGuid, aInputBlockId);
  }

  InputAPZContext context(aGuid, aInputBlockId, nsEventStatus_eIgnore, postLayerization != nullptr);

  WidgetMouseEvent localEvent(aEvent);
  localEvent.mWidget = mPuppetWidget;
  APZCCallbackHelper::ApplyCallbackTransform(localEvent, aGuid,
      mPuppetWidget->GetDefaultScale());
  DispatchWidgetEventViaAPZ(localEvent);

  if (aInputBlockId && aEvent.mFlags.mHandledByAPZ) {
    mAPZEventState->ProcessMouseEvent(aEvent, aGuid, aInputBlockId);
  }

  // Do this after the DispatchWidgetEventViaAPZ call above, so that if the
  // mouse event triggered a post-refresh AsyncDragMetrics message to be sent
  // to APZ (from scrollbar dragging in nsSliderFrame), then that will reach
  // APZ before the SetTargetAPZC message. This ensures the drag input block
  // gets the drag metrics before handling the input events.
  if (postLayerization && postLayerization->Register()) {
    Unused << postLayerization.release();
  }
}

mozilla::ipc::IPCResult
TabChild::RecvNormalPriorityRealMouseButtonEvent(
  const WidgetMouseEvent& aEvent,
  const ScrollableLayerGuid& aGuid,
  const uint64_t& aInputBlockId)
{
  return RecvRealMouseButtonEvent(aEvent, aGuid, aInputBlockId);
}

// In case handling repeated mouse wheel takes much time, we skip firing current
// wheel event if it may be coalesced to the next one.
bool
TabChild::MaybeCoalesceWheelEvent(const WidgetWheelEvent& aEvent,
                                  const ScrollableLayerGuid& aGuid,
                                  const uint64_t& aInputBlockId,
                                  bool* aIsNextWheelEvent)
{
  MOZ_ASSERT(aIsNextWheelEvent);
  if (aEvent.mMessage == eWheel) {
    GetIPCChannel()->PeekMessages(
        [aIsNextWheelEvent](const IPC::Message& aMsg) -> bool {
          if (aMsg.type() == mozilla::dom::PBrowser::Msg_MouseWheelEvent__ID) {
            *aIsNextWheelEvent = true;
          }
          return false; // Stop peeking.
        });
    // We only coalesce the current event when
    // 1. It's eWheel (we don't coalesce eOperationStart and eWheelOperationEnd)
    // 2. It's not the first wheel event.
    // 3. It's not the last wheel event.
    // 4. It's dispatched before the last wheel event was processed +
    //    the processing time of the last event.
    //    This way pages spending lots of time in wheel listeners get wheel
    //    events coalesced more aggressively.
    // 5. It has same attributes as the coalesced wheel event which is not yet
    //    fired.
    if (!mLastWheelProcessedTimeFromParent.IsNull() &&
        *aIsNextWheelEvent &&
        aEvent.mTimeStamp < (mLastWheelProcessedTimeFromParent +
                             mLastWheelProcessingDuration) &&
        (mCoalescedWheelData.IsEmpty() ||
         mCoalescedWheelData.CanCoalesce(aEvent, aGuid, aInputBlockId))) {
      mCoalescedWheelData.Coalesce(aEvent, aGuid, aInputBlockId);
      return true;
    }
  }
  return false;
}

nsEventStatus
TabChild::DispatchWidgetEventViaAPZ(WidgetGUIEvent& aEvent)
{
  aEvent.ResetWaitingReplyFromRemoteProcessState();
  return APZCCallbackHelper::DispatchWidgetEvent(aEvent);
}

void
TabChild::MaybeDispatchCoalescedWheelEvent()
{
  if (mCoalescedWheelData.IsEmpty()) {
    return;
  }
  UniquePtr<WidgetWheelEvent> wheelEvent =
    mCoalescedWheelData.TakeCoalescedEvent();
  MOZ_ASSERT(wheelEvent);
  DispatchWheelEvent(*wheelEvent,
                     mCoalescedWheelData.GetScrollableLayerGuid(),
                     mCoalescedWheelData.GetInputBlockId());
}

void
TabChild::DispatchWheelEvent(const WidgetWheelEvent& aEvent,
                                  const ScrollableLayerGuid& aGuid,
                                  const uint64_t& aInputBlockId)
{
  WidgetWheelEvent localEvent(aEvent);
  if (aInputBlockId && aEvent.mFlags.mHandledByAPZ) {
    nsCOMPtr<nsIDocument> document(GetDocument());
    UniquePtr<DisplayportSetListener> postLayerization =
        APZCCallbackHelper::SendSetTargetAPZCNotification(
            mPuppetWidget, document, aEvent, aGuid, aInputBlockId);
    if (postLayerization && postLayerization->Register()) {
      Unused << postLayerization.release();
    }
  }

  localEvent.mWidget = mPuppetWidget;
  APZCCallbackHelper::ApplyCallbackTransform(localEvent, aGuid,
                                             mPuppetWidget->GetDefaultScale());
  DispatchWidgetEventViaAPZ(localEvent);

  if (localEvent.mCanTriggerSwipe) {
    SendRespondStartSwipeEvent(aInputBlockId, localEvent.TriggersSwipe());
  }

  if (aInputBlockId && aEvent.mFlags.mHandledByAPZ) {
    mAPZEventState->ProcessWheelEvent(localEvent, aGuid, aInputBlockId);
  }
}

mozilla::ipc::IPCResult
TabChild::RecvMouseWheelEvent(const WidgetWheelEvent& aEvent,
                              const ScrollableLayerGuid& aGuid,
                              const uint64_t& aInputBlockId)
{
  bool isNextWheelEvent = false;
  if (MaybeCoalesceWheelEvent(aEvent, aGuid, aInputBlockId,
                              &isNextWheelEvent)) {
    return IPC_OK();
  }
  if (isNextWheelEvent) {
    // Update mLastWheelProcessedTimeFromParent so that we can compare the end
    // time of the current event with the dispatched time of the next event.
    mLastWheelProcessedTimeFromParent = aEvent.mTimeStamp;
    mozilla::TimeStamp beforeDispatchingTime = TimeStamp::Now();
    MaybeDispatchCoalescedWheelEvent();
    DispatchWheelEvent(aEvent, aGuid, aInputBlockId);
    mLastWheelProcessingDuration = (TimeStamp::Now() - beforeDispatchingTime);
    mLastWheelProcessedTimeFromParent += mLastWheelProcessingDuration;
  } else {
    // This is the last wheel event. Set mLastWheelProcessedTimeFromParent to
    // null moment to avoid coalesce the next incoming wheel event.
    mLastWheelProcessedTimeFromParent = TimeStamp();
    MaybeDispatchCoalescedWheelEvent();
    DispatchWheelEvent(aEvent, aGuid, aInputBlockId);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
TabChild::RecvNormalPriorityMouseWheelEvent(const WidgetWheelEvent& aEvent,
                                            const ScrollableLayerGuid& aGuid,
                                            const uint64_t& aInputBlockId)
{
  return RecvMouseWheelEvent(aEvent, aGuid, aInputBlockId);
}

mozilla::ipc::IPCResult
TabChild::RecvRealTouchEvent(const WidgetTouchEvent& aEvent,
                             const ScrollableLayerGuid& aGuid,
                             const uint64_t& aInputBlockId,
                             const nsEventStatus& aApzResponse)
{
  TABC_LOG("Receiving touch event of type %d\n", aEvent.mMessage);

  WidgetTouchEvent localEvent(aEvent);
  localEvent.mWidget = mPuppetWidget;

  APZCCallbackHelper::ApplyCallbackTransform(localEvent, aGuid,
                                             mPuppetWidget->GetDefaultScale());

  if (localEvent.mMessage == eTouchStart && AsyncPanZoomEnabled()) {
    nsCOMPtr<nsIDocument> document = GetDocument();
    if (gfxPrefs::TouchActionEnabled()) {
      APZCCallbackHelper::SendSetAllowedTouchBehaviorNotification(
        mPuppetWidget, document, localEvent, aInputBlockId,
        mSetAllowedTouchBehaviorCallback);
    }
    UniquePtr<DisplayportSetListener> postLayerization =
        APZCCallbackHelper::SendSetTargetAPZCNotification(
            mPuppetWidget, document, localEvent, aGuid, aInputBlockId);
    if (postLayerization && postLayerization->Register()) {
      Unused << postLayerization.release();
    }
  }

  // Dispatch event to content (potentially a long-running operation)
  nsEventStatus status = DispatchWidgetEventViaAPZ(localEvent);

  if (!AsyncPanZoomEnabled()) {
    // We shouldn't have any e10s platforms that have touch events enabled
    // without APZ.
    MOZ_ASSERT(false);
    return IPC_OK();
  }

  mAPZEventState->ProcessTouchEvent(localEvent, aGuid, aInputBlockId,
                                    aApzResponse, status);
  return IPC_OK();
}

mozilla::ipc::IPCResult
TabChild::RecvNormalPriorityRealTouchEvent(const WidgetTouchEvent& aEvent,
                                           const ScrollableLayerGuid& aGuid,
                                           const uint64_t& aInputBlockId,
                                           const nsEventStatus& aApzResponse)
{
  return RecvRealTouchEvent(aEvent, aGuid, aInputBlockId, aApzResponse);
}

mozilla::ipc::IPCResult
TabChild::RecvRealTouchMoveEvent(const WidgetTouchEvent& aEvent,
                                 const ScrollableLayerGuid& aGuid,
                                 const uint64_t& aInputBlockId,
                                 const nsEventStatus& aApzResponse)
{
  if (!RecvRealTouchEvent(aEvent, aGuid, aInputBlockId, aApzResponse)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
TabChild::RecvNormalPriorityRealTouchMoveEvent(
  const WidgetTouchEvent& aEvent,
  const ScrollableLayerGuid& aGuid,
  const uint64_t& aInputBlockId,
  const nsEventStatus& aApzResponse)
{
  return RecvRealTouchMoveEvent(aEvent, aGuid, aInputBlockId, aApzResponse);
}

mozilla::ipc::IPCResult
TabChild::RecvRealDragEvent(const WidgetDragEvent& aEvent,
                            const uint32_t& aDragAction,
                            const uint32_t& aDropEffect,
                            const nsCString& aPrincipalURISpec)
{
  WidgetDragEvent localEvent(aEvent);
  localEvent.mWidget = mPuppetWidget;

  nsCOMPtr<nsIDragSession> dragSession = nsContentUtils::GetDragSession();
  if (dragSession) {
    dragSession->SetDragAction(aDragAction);
    dragSession->SetTriggeringPrincipalURISpec(aPrincipalURISpec);
    RefPtr<DataTransfer> initialDataTransfer = dragSession->GetDataTransfer();
    if (initialDataTransfer) {
      initialDataTransfer->SetDropEffectInt(aDropEffect);
    }
  }

  if (aEvent.mMessage == eDrop) {
    bool canDrop = true;
    if (!dragSession || NS_FAILED(dragSession->GetCanDrop(&canDrop)) ||
        !canDrop) {
      localEvent.mMessage = eDragExit;
    }
  } else if (aEvent.mMessage == eDragOver) {
    nsCOMPtr<nsIDragService> dragService =
      do_GetService("@mozilla.org/widget/dragservice;1");
    if (dragService) {
      // This will dispatch 'drag' event at the source if the
      // drag transaction started in this process.
      dragService->FireDragEventAtSource(eDrag, aEvent.mModifiers);
    }
  }

  DispatchWidgetEventViaAPZ(localEvent);
  return IPC_OK();
}

mozilla::ipc::IPCResult
TabChild::RecvPluginEvent(const WidgetPluginEvent& aEvent)
{
  WidgetPluginEvent localEvent(aEvent);
  localEvent.mWidget = mPuppetWidget;
  nsEventStatus status = DispatchWidgetEventViaAPZ(localEvent);
  if (status != nsEventStatus_eConsumeNoDefault) {
    // If not consumed, we should call default action
    SendDefaultProcOfPluginEvent(aEvent);
  }
  return IPC_OK();
}

void
TabChild::RequestEditCommands(nsIWidget::NativeKeyBindingsType aType,
                              const WidgetKeyboardEvent& aEvent,
                              nsTArray<CommandInt>& aCommands)
{
  MOZ_ASSERT(aCommands.IsEmpty());

  if (NS_WARN_IF(aEvent.IsEditCommandsInitialized(aType))) {
    aCommands = aEvent.EditCommandsConstRef(aType);
    return;
  }

  switch (aType) {
    case nsIWidget::NativeKeyBindingsForSingleLineEditor:
    case nsIWidget::NativeKeyBindingsForMultiLineEditor:
    case nsIWidget::NativeKeyBindingsForRichTextEditor:
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Invalid native key bindings type");
  }

  // Don't send aEvent to the parent process directly because it'll be marked
  // as posted to remote process.
  WidgetKeyboardEvent localEvent(aEvent);
  SendRequestNativeKeyBindings(aType, localEvent, &aCommands);
}

mozilla::ipc::IPCResult
TabChild::RecvNativeSynthesisResponse(const uint64_t& aObserverId,
                                      const nsCString& aResponse)
{
  mozilla::widget::AutoObserverNotifier::NotifySavedObserver(aObserverId,
                                                             aResponse.get());
  return IPC_OK();
}

// In case handling repeated keys takes much time, we skip firing new ones.
bool
TabChild::SkipRepeatedKeyEvent(const WidgetKeyboardEvent& aEvent)
{
  if (mRepeatedKeyEventTime.IsNull() ||
      !aEvent.CanSkipInRemoteProcess() ||
      (aEvent.mMessage != eKeyDown && aEvent.mMessage != eKeyPress)) {
    mRepeatedKeyEventTime = TimeStamp();
    mSkipKeyPress = false;
    return false;
  }

  if ((aEvent.mMessage == eKeyDown &&
       (mRepeatedKeyEventTime > aEvent.mTimeStamp)) ||
      (mSkipKeyPress && (aEvent.mMessage == eKeyPress))) {
    // If we skip a keydown event, also the following keypress events should be
    // skipped.
    mSkipKeyPress |= aEvent.mMessage == eKeyDown;
    return true;
  }

  if (aEvent.mMessage == eKeyDown) {
    // If keydown wasn't skipped, nor should the possible following keypress.
    mRepeatedKeyEventTime = TimeStamp();
    mSkipKeyPress = false;
  }
  return false;
}

void
TabChild::UpdateRepeatedKeyEventEndTime(const WidgetKeyboardEvent& aEvent)
{
  if (aEvent.mIsRepeat &&
      (aEvent.mMessage == eKeyDown || aEvent.mMessage == eKeyPress)) {
    mRepeatedKeyEventTime = TimeStamp::Now();
  }
}

mozilla::ipc::IPCResult
TabChild::RecvRealKeyEvent(const WidgetKeyboardEvent& aEvent)
{
  if (SkipRepeatedKeyEvent(aEvent)) {
    return IPC_OK();
  }

  MOZ_ASSERT(aEvent.mMessage != eKeyPress ||
             aEvent.AreAllEditCommandsInitialized(),
    "eKeyPress event should have native key binding information");

  // If content code called preventDefault() on a keydown event, then we don't
  // want to process any following keypress events.
  if (aEvent.mMessage == eKeyPress && mIgnoreKeyPressEvent) {
    return IPC_OK();
  }

  WidgetKeyboardEvent localEvent(aEvent);
  localEvent.mWidget = mPuppetWidget;
  localEvent.mUniqueId = aEvent.mUniqueId;
  nsEventStatus status = DispatchWidgetEventViaAPZ(localEvent);

  // Update the end time of the possible repeated event so that we can skip
  // some incoming events in case event handling took long time.
  UpdateRepeatedKeyEventEndTime(localEvent);

  if (aEvent.mMessage == eKeyDown) {
    mIgnoreKeyPressEvent = status == nsEventStatus_eConsumeNoDefault;
  }

  if (localEvent.mFlags.mIsSuppressedOrDelayed) {
    localEvent.PreventDefault();
  }

  // If a response is desired from the content process, resend the key event.
  if (aEvent.WantReplyFromContentProcess()) {
    // If the event's default isn't prevented but the status is no default,
    // That means that the event was consumed by EventStateManager or something
    // which is not a usual event handler.  In such case, prevent its default
    // as a default handler.  For example, when an eKeyPress event matches
    // with a content accesskey, and it's executed, peventDefault() of the
    // event won't be called but the status is set to "no default".  Then,
    // the event shouldn't be handled by nsMenuBarListener in the main process.
    if (!localEvent.DefaultPrevented() &&
        status == nsEventStatus_eConsumeNoDefault) {
      localEvent.PreventDefault();
    }
    // This is an ugly hack, mNoRemoteProcessDispatch is set to true when the
    // event's PreventDefault() or StopScrollProcessForwarding() is called.
    // And then, it'll be checked by ParamTraits<mozilla::WidgetEvent>::Write()
    // whether the event is being sent to remote process unexpectedly.
    // However, unfortunately, it cannot check the destination.  Therefore,
    // we need to clear the flag explicitly here because ParamTraits should
    // keep checking the flag for avoiding regression.
    localEvent.mFlags.mNoRemoteProcessDispatch = false;
    SendReplyKeyEvent(localEvent);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult
TabChild::RecvNormalPriorityRealKeyEvent(const WidgetKeyboardEvent& aEvent)
{
  return RecvRealKeyEvent(aEvent);
}

mozilla::ipc::IPCResult
TabChild::RecvCompositionEvent(const WidgetCompositionEvent& aEvent)
{
  WidgetCompositionEvent localEvent(aEvent);
  localEvent.mWidget = mPuppetWidget;
  DispatchWidgetEventViaAPZ(localEvent);
  Unused << SendOnEventNeedingAckHandled(aEvent.mMessage);
  return IPC_OK();
}

mozilla::ipc::IPCResult
TabChild::RecvNormalPriorityCompositionEvent(
            const WidgetCompositionEvent& aEvent)
{
  return RecvCompositionEvent(aEvent);
}

mozilla::ipc::IPCResult
TabChild::RecvSelectionEvent(const WidgetSelectionEvent& aEvent)
{
  WidgetSelectionEvent localEvent(aEvent);
  localEvent.mWidget = mPuppetWidget;
  DispatchWidgetEventViaAPZ(localEvent);
  Unused << SendOnEventNeedingAckHandled(aEvent.mMessage);
  return IPC_OK();
}

mozilla::ipc::IPCResult
TabChild::RecvNormalPrioritySelectionEvent(const WidgetSelectionEvent& aEvent)
{
  return RecvSelectionEvent(aEvent);
}

mozilla::ipc::IPCResult
TabChild::RecvPasteTransferable(const IPCDataTransfer& aDataTransfer,
                                const bool& aIsPrivateData,
                                const IPC::Principal& aRequestingPrincipal,
                                const uint32_t& aContentPolicyType)
{
  nsresult rv;
  nsCOMPtr<nsITransferable> trans =
    do_CreateInstance("@mozilla.org/widget/transferable;1", &rv);
  NS_ENSURE_SUCCESS(rv, IPC_OK());
  trans->Init(nullptr);

  rv = nsContentUtils::IPCTransferableToTransferable(aDataTransfer,
                                                     aIsPrivateData,
                                                     aRequestingPrincipal,
                                                     aContentPolicyType,
                                                     trans, nullptr, this);
  NS_ENSURE_SUCCESS(rv, IPC_OK());

  nsCOMPtr<nsIDocShell> ourDocShell = do_GetInterface(WebNavigation());
  if (NS_WARN_IF(!ourDocShell)) {
    return IPC_OK();
  }

  RefPtr<nsCommandParams> params = new nsCommandParams();
  rv = params->SetISupports("transferable", trans);
  NS_ENSURE_SUCCESS(rv, IPC_OK());

  ourDocShell->DoCommandWithParams("cmd_pasteTransferable", params);
  return IPC_OK();
}


a11y::PDocAccessibleChild*
TabChild::AllocPDocAccessibleChild(PDocAccessibleChild*, const uint64_t&,
                                   const uint32_t&, const IAccessibleHolder&)
{
  MOZ_ASSERT(false, "should never call this!");
  return nullptr;
}

bool
TabChild::DeallocPDocAccessibleChild(a11y::PDocAccessibleChild* aChild)
{
#ifdef ACCESSIBILITY
  delete static_cast<mozilla::a11y::DocAccessibleChild*>(aChild);
#endif
  return true;
}

PColorPickerChild*
TabChild::AllocPColorPickerChild(const nsString&, const nsString&)
{
  MOZ_CRASH("unused");
  return nullptr;
}

bool
TabChild::DeallocPColorPickerChild(PColorPickerChild* aColorPicker)
{
  nsColorPickerProxy* picker = static_cast<nsColorPickerProxy*>(aColorPicker);
  NS_RELEASE(picker);
  return true;
}

PFilePickerChild*
TabChild::AllocPFilePickerChild(const nsString&, const int16_t&)
{
  MOZ_CRASH("unused");
  return nullptr;
}

bool
TabChild::DeallocPFilePickerChild(PFilePickerChild* actor)
{
  nsFilePickerProxy* filePicker = static_cast<nsFilePickerProxy*>(actor);
  NS_RELEASE(filePicker);
  return true;
}

auto
TabChild::AllocPIndexedDBPermissionRequestChild(const Principal& aPrincipal)
  -> PIndexedDBPermissionRequestChild*
{
  MOZ_CRASH("PIndexedDBPermissionRequestChild actors should always be created "
            "manually!");
}

bool
TabChild::DeallocPIndexedDBPermissionRequestChild(
                                       PIndexedDBPermissionRequestChild* aActor)
{
  MOZ_ASSERT(aActor);
  delete aActor;
  return true;
}

mozilla::ipc::IPCResult
TabChild::RecvActivateFrameEvent(const nsString& aType, const bool& capture)
{
  nsCOMPtr<nsPIDOMWindowOuter> window = do_GetInterface(WebNavigation());
  NS_ENSURE_TRUE(window, IPC_OK());
  nsCOMPtr<EventTarget> chromeHandler = window->GetChromeEventHandler();
  NS_ENSURE_TRUE(chromeHandler, IPC_OK());
  RefPtr<ContentListener> listener = new ContentListener(this);
  chromeHandler->AddEventListener(aType, listener, capture);
  return IPC_OK();
}

// Return whether a remote script should be loaded in middleman processes in
// addition to any child recording process they have.
static bool
LoadScriptInMiddleman(const nsString& aURL)
{
  return // Middleman processes run devtools server side scripts.
         (StringBeginsWith(aURL, NS_LITERAL_STRING("resource://devtools/")) &&
          recordreplay::parent::DebuggerRunsInMiddleman())
         // This script includes event listeners needed to propagate document
         // title changes.
      || aURL.EqualsLiteral("chrome://global/content/browser-child.js")
         // This script is needed to respond to session store requests from the
         // UI process.
      || aURL.EqualsLiteral("chrome://browser/content/content-sessionStore.js");
}

mozilla::ipc::IPCResult
TabChild::RecvLoadRemoteScript(const nsString& aURL, const bool& aRunInGlobalScope)
{
  if (!InitTabChildMessageManager())
    // This can happen if we're half-destroyed.  It's not a fatal
    // error.
    return IPC_OK();

  JS::Rooted<JSObject*> mm(RootingCx(), mTabChildMessageManager->GetOrCreateWrapper());
  if (!mm) {
    // This can happen if we're half-destroyed.  It's not a fatal error.
    return IPC_OK();
  }

  // Make sure we only load whitelisted scripts in middleman processes.
  if (recordreplay::IsMiddleman() && !LoadScriptInMiddleman(aURL)) {
    return IPC_OK();
  }

  LoadScriptInternal(mm, aURL, !aRunInGlobalScope);
  return IPC_OK();
}

mozilla::ipc::IPCResult
TabChild::RecvAsyncMessage(const nsString& aMessage,
                           InfallibleTArray<CpowEntry>&& aCpows,
                           const IPC::Principal& aPrincipal,
                           const ClonedMessageData& aData)
{
  AUTO_PROFILER_LABEL_DYNAMIC_LOSSY_NSSTRING(
    "TabChild::RecvAsyncMessage", OTHER, aMessage);

  CrossProcessCpowHolder cpows(Manager(), aCpows);
  if (!mTabChildMessageManager) {
    return IPC_OK();
  }

  RefPtr<nsFrameMessageManager> mm = mTabChildMessageManager->GetMessageManager();

  // We should have a message manager if the global is alive, but it
  // seems sometimes we don't.  Assert in aurora/nightly, but don't
  // crash in release builds.
  MOZ_DIAGNOSTIC_ASSERT(mm);
  if (!mm) {
    return IPC_OK();
  }

  JS::Rooted<JSObject*> kungFuDeathGrip(dom::RootingCx(), mTabChildMessageManager->GetWrapper());
  StructuredCloneData data;
  UnpackClonedMessageDataForChild(aData, data);
  mm->ReceiveMessage(static_cast<EventTarget*>(mTabChildMessageManager), nullptr,
                     aMessage, false, &data, &cpows, aPrincipal, nullptr,
                     IgnoreErrors());
  return IPC_OK();
}

mozilla::ipc::IPCResult
TabChild::RecvSwappedWithOtherRemoteLoader(const IPCTabContext& aContext)
{
  nsCOMPtr<nsIDocShell> ourDocShell = do_GetInterface(WebNavigation());
  if (NS_WARN_IF(!ourDocShell)) {
    return IPC_OK();
  }

  nsCOMPtr<nsPIDOMWindowOuter> ourWindow = ourDocShell->GetWindow();
  if (NS_WARN_IF(!ourWindow)) {
    return IPC_OK();
  }

  RefPtr<nsDocShell> docShell = static_cast<nsDocShell*>(ourDocShell.get());

  nsCOMPtr<EventTarget> ourEventTarget = nsGlobalWindowOuter::Cast(ourWindow);

  docShell->SetInFrameSwap(true);

  nsContentUtils::FirePageShowEvent(ourDocShell, ourEventTarget, false, true);
  nsContentUtils::FirePageHideEvent(ourDocShell, ourEventTarget, true);

  // Owner content type may have changed, so store the possibly updated context
  // and notify others.
  MaybeInvalidTabContext maybeContext(aContext);
  if (!maybeContext.IsValid()) {
    NS_ERROR(nsPrintfCString("Received an invalid TabContext from "
                             "the parent process. (%s)",
                             maybeContext.GetInvalidReason()).get());
    MOZ_CRASH("Invalid TabContext received from the parent process.");
  }

  if (!UpdateTabContextAfterSwap(maybeContext.GetTabContext())) {
    MOZ_CRASH("Update to TabContext after swap was denied.");
  }

  // Since mIsMozBrowserElement may change in UpdateTabContextAfterSwap, so we
  // call UpdateFrameType here to make sure the frameType on the docshell is
  // correct.
  UpdateFrameType();

  // Ignore previous value of mTriedBrowserInit since owner content has changed.
  mTriedBrowserInit = true;
  // Initialize the child side of the browser element machinery, if appropriate.
  if (IsMozBrowser()) {
    RecvLoadRemoteScript(BROWSER_ELEMENT_CHILD_SCRIPT, true);
  }

  nsContentUtils::FirePageShowEvent(ourDocShell, ourEventTarget, true, true);

  docShell->SetInFrameSwap(false);

  return IPC_OK();
}

mozilla::ipc::IPCResult
TabChild::RecvHandleAccessKey(const WidgetKeyboardEvent& aEvent,
                              nsTArray<uint32_t>&& aCharCodes)
{
  nsCOMPtr<nsIDocument> document(GetDocument());
  RefPtr<nsPresContext> pc = document->GetPresContext();
  if (pc) {
    if (!pc->EventStateManager()->
               HandleAccessKey(&(const_cast<WidgetKeyboardEvent&>(aEvent)),
                               pc, aCharCodes)) {
      // If no accesskey was found, inform the parent so that accesskeys on
      // menus can be handled.
      WidgetKeyboardEvent localEvent(aEvent);
      localEvent.mWidget = mPuppetWidget;
      SendAccessKeyNotHandled(localEvent);
    }
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult
TabChild::RecvSetUseGlobalHistory(const bool& aUse)
{
  nsCOMPtr<nsIDocShell> docShell = do_GetInterface(WebNavigation());
  MOZ_ASSERT(docShell);

  nsresult rv = docShell->SetUseGlobalHistory(aUse);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to set UseGlobalHistory on TabChild docShell");
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult
TabChild::RecvPrint(const uint64_t& aOuterWindowID, const PrintData& aPrintData)
{
#ifdef NS_PRINTING
  nsGlobalWindowOuter* outerWindow =
    nsGlobalWindowOuter::GetOuterWindowWithId(aOuterWindowID);
  if (NS_WARN_IF(!outerWindow)) {
    return IPC_OK();
  }

  nsCOMPtr<nsIWebBrowserPrint> webBrowserPrint =
    do_GetInterface(outerWindow->AsOuter());
  if (NS_WARN_IF(!webBrowserPrint)) {
    return IPC_OK();
  }

  nsCOMPtr<nsIPrintSettingsService> printSettingsSvc =
    do_GetService("@mozilla.org/gfx/printsettings-service;1");
  if (NS_WARN_IF(!printSettingsSvc)) {
    return IPC_OK();
  }

  nsCOMPtr<nsIPrintSettings> printSettings;
  nsresult rv =
    printSettingsSvc->GetNewPrintSettings(getter_AddRefs(printSettings));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return IPC_OK();
  }

  nsCOMPtr<nsIPrintSession>  printSession =
    do_CreateInstance("@mozilla.org/gfx/printsession;1", &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return IPC_OK();
  }

  printSettings->SetPrintSession(printSession);
  printSettingsSvc->DeserializeToPrintSettings(aPrintData, printSettings);
  rv = webBrowserPrint->Print(printSettings, nullptr);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return IPC_OK();
  }

#endif
  return IPC_OK();
}

mozilla::ipc::IPCResult
TabChild::RecvUpdateNativeWindowHandle(const uintptr_t& aNewHandle)
{
#if defined(XP_WIN) && defined(ACCESSIBILITY)
  mNativeWindowHandle = aNewHandle;
  return IPC_OK();
#else
  return IPC_FAIL_NO_REASON(this);
#endif
}

mozilla::ipc::IPCResult
TabChild::RecvDestroy()
{
  MOZ_ASSERT(mDestroyed == false);
  mDestroyed = true;

  nsTArray<PContentPermissionRequestChild*> childArray =
      nsContentPermissionUtils::GetContentPermissionRequestChildById(GetTabId());

  // Need to close undeleted ContentPermissionRequestChilds before tab is closed.
  for (auto& permissionRequestChild : childArray) {
      auto child = static_cast<RemotePermissionRequest*>(permissionRequestChild);
      child->Destroy();
  }

  if (mTabChildMessageManager) {
    // Message handlers are called from the event loop, so it better be safe to
    // run script.
    MOZ_ASSERT(nsContentUtils::IsSafeToRunScript());
    mTabChildMessageManager->DispatchTrustedEvent(NS_LITERAL_STRING("unload"));
  }

  nsCOMPtr<nsIObserverService> observerService =
    mozilla::services::GetObserverService();

  observerService->RemoveObserver(this, BEFORE_FIRST_PAINT);

  // XXX what other code in ~TabChild() should we be running here?
  DestroyWindow();

  // Bounce through the event loop once to allow any delayed teardown runnables
  // that were just generated to have a chance to run.
  nsCOMPtr<nsIRunnable> deleteRunnable = new DelayedDeleteRunnable(this);
  MOZ_ALWAYS_SUCCEEDS(NS_DispatchToCurrentThread(deleteRunnable));

  return IPC_OK();
}

void
TabChild::AddPendingDocShellBlocker()
{
  mPendingDocShellBlockers++;
}

void
TabChild::RemovePendingDocShellBlocker()
{
  mPendingDocShellBlockers--;
  if (!mPendingDocShellBlockers && mPendingDocShellReceivedMessage) {
    mPendingDocShellReceivedMessage = false;
    InternalSetDocShellIsActive(mPendingDocShellIsActive);
  }
  if (!mPendingDocShellBlockers && mPendingRenderLayersReceivedMessage) {
    mPendingRenderLayersReceivedMessage = false;
    RecvRenderLayers(mPendingRenderLayers,
                     false /* aForceRepaint */,
                     mPendingLayersObserverEpoch);
  }
}

void
TabChild::InternalSetDocShellIsActive(bool aIsActive)
{
  nsCOMPtr<nsIDocShell> docShell = do_GetInterface(WebNavigation());

  if (docShell) {
    docShell->SetIsActive(aIsActive);
  }
}

mozilla::ipc::IPCResult
TabChild::RecvSetDocShellIsActive(const bool& aIsActive)
{
  // If we're currently waiting for window opening to complete, we need to hold
  // off on setting the docshell active. We queue up the values we're receiving
  // in the mWindowOpenDocShellActiveStatus.
  if (mPendingDocShellBlockers > 0) {
    mPendingDocShellReceivedMessage = true;
    mPendingDocShellIsActive = aIsActive;
    return IPC_OK();
  }

  InternalSetDocShellIsActive(aIsActive);
  return IPC_OK();
}

mozilla::ipc::IPCResult
TabChild::RecvRenderLayers(const bool& aEnabled, const bool& aForceRepaint, const layers::LayersObserverEpoch& aEpoch)
{
  if (mPendingDocShellBlockers > 0) {
    mPendingRenderLayersReceivedMessage = true;
    mPendingRenderLayers = aEnabled;
    mPendingLayersObserverEpoch = aEpoch;
    return IPC_OK();
  }

  // Since requests to change the rendering state come in from both the hang
  // monitor channel and the PContent channel, we have an ordering problem. This
  // code ensures that we respect the order in which the requests were made and
  // ignore stale requests.
  if (mLayersObserverEpoch >= aEpoch) {
    return IPC_OK();
  }
  mLayersObserverEpoch = aEpoch;

  auto clearPaintWhileInterruptingJS = MakeScopeExit([&] {
    // We might force a paint, or we might already have painted and this is a
    // no-op. In either case, once we exit this scope, we need to alert the
    // ProcessHangMonitor that we've finished responding to what might have
    // been a request to force paint. This is so that the BackgroundHangMonitor
    // for force painting can be made to wait again.
    if (aEnabled) {
      ProcessHangMonitor::ClearPaintWhileInterruptingJS(mLayersObserverEpoch);
    }
  });

  if (aEnabled) {
    ProcessHangMonitor::MaybeStartPaintWhileInterruptingJS();
  }

  if (mCompositorOptions) {
    MOZ_ASSERT(mPuppetWidget);
    RefPtr<LayerManager> lm = mPuppetWidget->GetLayerManager();
    MOZ_ASSERT(lm);

    // We send the current layer observer epoch to the compositor so that
    // TabParent knows whether a layer update notification corresponds to the
    // latest RecvRenderLayers request that was made.
    lm->SetLayersObserverEpoch(mLayersObserverEpoch);
  }

  if (aEnabled) {
    if (!aForceRepaint && IsVisible()) {
      // This request is a no-op. In this case, we still want a MozLayerTreeReady
      // notification to fire in the parent (so that it knows that the child has
      // updated its epoch). PaintWhileInterruptingJSNoOp does that.
      if (IPCOpen()) {
        Unused << SendPaintWhileInterruptingJSNoOp(mLayersObserverEpoch);
        return IPC_OK();
      }
    }

    if (!sVisibleTabs) {
      sVisibleTabs = new nsTHashtable<nsPtrHashKey<TabChild>>();
    }
    sVisibleTabs->PutEntry(this);

    MakeVisible();

    nsCOMPtr<nsIDocShell> docShell = do_GetInterface(WebNavigation());
    if (!docShell) {
      return IPC_OK();
    }

    // We don't use TabChildBase::GetPresShell() here because that would create
    // a content viewer if one doesn't exist yet. Creating a content viewer can
    // cause JS to run, which we want to avoid. nsIDocShell::GetPresShell
    // returns null if no content viewer exists yet.
    if (nsCOMPtr<nsIPresShell> presShell = docShell->GetPresShell()) {
      presShell->SetIsActive(true);

      if (nsIFrame* root = presShell->GetRootFrame()) {
        FrameLayerBuilder::InvalidateAllLayersForFrame(
          nsLayoutUtils::GetDisplayRootFrame(root));
        root->SchedulePaint();
      }

      Telemetry::AutoTimer<Telemetry::TABCHILD_PAINT_TIME> timer;
      // If we need to repaint, let's do that right away. No sense waiting until
      // we get back to the event loop again. We suppress the display port so that
      // we only paint what's visible. This ensures that the tab we're switching
      // to paints as quickly as possible.
      presShell->SuppressDisplayport(true);
      if (nsContentUtils::IsSafeToRunScript()) {
        WebWidget()->PaintNowIfNeeded();
      } else {
        RefPtr<nsViewManager> vm = presShell->GetViewManager();
        if (nsView* view = vm->GetRootView()) {
          presShell->Paint(view, view->GetBounds(),
                           nsIPresShell::PAINT_LAYERS);
        }
      }
      presShell->SuppressDisplayport(false);
    }
  } else {
    if (sVisibleTabs) {
      sVisibleTabs->RemoveEntry(this);
      // We don't delete sVisibleTabs here when it's empty since that
      // could cause a lot of churn. Instead, we wait until ~TabChild.
    }

    MakeHidden();
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult
TabChild::RecvRequestRootPaint(const IntRect& aRect, const float& aScale, const nscolor& aBackgroundColor, RequestRootPaintResolver&& aResolve)
{
  nsCOMPtr<nsIDocShell> docShell = do_GetInterface(WebNavigation());
  if (!docShell) {
    return IPC_OK();
  }

  aResolve(gfx::PaintFragment::Record(docShell, aRect, aScale, aBackgroundColor));
  return IPC_OK();
}

mozilla::ipc::IPCResult
TabChild::RecvRequestSubPaint(const float& aScale, const nscolor& aBackgroundColor, RequestSubPaintResolver&& aResolve)
{
  nsCOMPtr<nsIDocShell> docShell = do_GetInterface(WebNavigation());
  if (!docShell) {
    return IPC_OK();
  }

  gfx::IntRect rect = gfx::RoundedIn(gfx::Rect(0.0f, 0.0f, mUnscaledInnerSize.width, mUnscaledInnerSize.height));
  aResolve(gfx::PaintFragment::Record(docShell, rect, aScale, aBackgroundColor));
  return IPC_OK();
}

mozilla::ipc::IPCResult
TabChild::RecvNavigateByKey(const bool& aForward, const bool& aForDocumentNavigation)
{
  nsIFocusManager* fm = nsFocusManager::GetFocusManager();
  if (fm) {
    RefPtr<Element> result;
    nsCOMPtr<nsPIDOMWindowOuter> window = do_GetInterface(WebNavigation());

    // Move to the first or last document.
    uint32_t type = aForward ?
      (aForDocumentNavigation ? static_cast<uint32_t>(nsIFocusManager::MOVEFOCUS_FIRSTDOC) :
                                static_cast<uint32_t>(nsIFocusManager::MOVEFOCUS_ROOT)) :
      (aForDocumentNavigation ? static_cast<uint32_t>(nsIFocusManager::MOVEFOCUS_LASTDOC) :
                                static_cast<uint32_t>(nsIFocusManager::MOVEFOCUS_LAST));
    fm->MoveFocus(window, nullptr, type,
                  nsIFocusManager::FLAG_BYKEY, getter_AddRefs(result));

    // No valid root element was found, so move to the first focusable element.
    if (!result && aForward && !aForDocumentNavigation) {
      fm->MoveFocus(window, nullptr, nsIFocusManager::MOVEFOCUS_FIRST,
                  nsIFocusManager::FLAG_BYKEY, getter_AddRefs(result));
    }

    SendRequestFocus(false);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult
TabChild::RecvHandledWindowedPluginKeyEvent(
            const NativeEventData& aKeyEventData,
            const bool& aIsConsumed)
{
  if (NS_WARN_IF(!mPuppetWidget)) {
    return IPC_OK();
  }
  mPuppetWidget->HandledWindowedPluginKeyEvent(aKeyEventData, aIsConsumed);
  return IPC_OK();
}

bool
TabChild::InitTabChildMessageManager()
{
  if (!mTabChildMessageManager) {
    nsCOMPtr<nsPIDOMWindowOuter> window = do_GetInterface(WebNavigation());
    NS_ENSURE_TRUE(window, false);
    nsCOMPtr<EventTarget> chromeHandler = window->GetChromeEventHandler();
    NS_ENSURE_TRUE(chromeHandler, false);

    RefPtr<TabChildMessageManager> scope = mTabChildMessageManager = new TabChildMessageManager(this);

    MOZ_ALWAYS_TRUE(nsMessageManagerScriptExecutor::Init());

    nsCOMPtr<nsPIWindowRoot> root = do_QueryInterface(chromeHandler);
    if (NS_WARN_IF(!root)) {
        mTabChildMessageManager = nullptr;
        return false;
    }
    root->SetParentTarget(scope);
  }

  if (!mTriedBrowserInit) {
    mTriedBrowserInit = true;
    // Initialize the child side of the browser element machinery,
    // if appropriate.
    if (IsMozBrowser()) {
      RecvLoadRemoteScript(BROWSER_ELEMENT_CHILD_SCRIPT, true);
    }
  }

  return true;
}

void
TabChild::InitRenderingState(const TextureFactoryIdentifier& aTextureFactoryIdentifier,
                             const layers::LayersId& aLayersId,
                             const CompositorOptions& aCompositorOptions)
{
    mPuppetWidget->InitIMEState();

    MOZ_ASSERT(aLayersId.IsValid());
    mTextureFactoryIdentifier = aTextureFactoryIdentifier;

    // Pushing layers transactions directly to a separate
    // compositor context.
    PCompositorBridgeChild* compositorChild = CompositorBridgeChild::Get();
    if (!compositorChild) {
      mLayersConnected = Some(false);
      NS_WARNING("failed to get CompositorBridgeChild instance");
      return;
    }

    mCompositorOptions = Some(aCompositorOptions);

    if (aLayersId.IsValid()) {
      StaticMutexAutoLock lock(sTabChildrenMutex);

      if (!sTabChildren) {
        sTabChildren = new TabChildMap;
      }
      MOZ_ASSERT(!sTabChildren->Get(uint64_t(aLayersId)));
      sTabChildren->Put(uint64_t(aLayersId), this);
      mLayersId = aLayersId;
    }

    MOZ_ASSERT(!mPuppetWidget->HasLayerManager());
    bool success = false;
    if (mLayersConnected == Some(true)) {
      success = CreateRemoteLayerManager(compositorChild);
    }

    if (success) {
      MOZ_ASSERT(mLayersConnected == Some(true));
      // Succeeded to create "remote" layer manager
      ImageBridgeChild::IdentifyCompositorTextureHost(mTextureFactoryIdentifier);
      gfx::VRManagerChild::IdentifyTextureHost(mTextureFactoryIdentifier);
      InitAPZState();
      RefPtr<LayerManager> lm = mPuppetWidget->GetLayerManager();
      MOZ_ASSERT(lm);
      lm->SetLayersObserverEpoch(mLayersObserverEpoch);
    } else {
      NS_WARNING("Fallback to BasicLayerManager");
      mLayersConnected = Some(false);
    }

    nsCOMPtr<nsIObserverService> observerService =
        mozilla::services::GetObserverService();

    if (observerService) {
        observerService->AddObserver(this,
                                     BEFORE_FIRST_PAINT,
                                     false);
    }
}

bool
TabChild::CreateRemoteLayerManager(mozilla::layers::PCompositorBridgeChild* aCompositorChild)
{
  MOZ_ASSERT(aCompositorChild);

  bool success = false;
  if (mCompositorOptions->UseWebRender()) {
    success = mPuppetWidget->CreateRemoteLayerManager([&] (LayerManager* aLayerManager) -> bool {
      MOZ_ASSERT(aLayerManager->AsWebRenderLayerManager());
      return aLayerManager->AsWebRenderLayerManager()->Initialize(aCompositorChild,
                                                                  wr::AsPipelineId(mLayersId),
                                                                  &mTextureFactoryIdentifier);
    });
  } else {
    nsTArray<LayersBackend> ignored;
    PLayerTransactionChild* shadowManager = aCompositorChild->SendPLayerTransactionConstructor(ignored, GetLayersId());
    if (shadowManager &&
        shadowManager->SendGetTextureFactoryIdentifier(&mTextureFactoryIdentifier) &&
        mTextureFactoryIdentifier.mParentBackend != LayersBackend::LAYERS_NONE)
    {
      success = true;
    }
    if (!success) {
      // Since no LayerManager is associated with the tab's widget, we will never
      // have an opportunity to destroy the PLayerTransaction on the next device
      // or compositor reset. Therefore, we make sure to forcefully close it here.
      // Failure to do so will cause the next layer tree to fail to attach due
      // since the compositor requires the old layer tree to be disassociated.
      if (shadowManager) {
        static_cast<LayerTransactionChild*>(shadowManager)->Destroy();
        shadowManager = nullptr;
      }
      NS_WARNING("failed to allocate layer transaction");
    } else {
      success = mPuppetWidget->CreateRemoteLayerManager([&] (LayerManager* aLayerManager) -> bool {
        ShadowLayerForwarder* lf = aLayerManager->AsShadowForwarder();
        lf->SetShadowManager(shadowManager);
        lf->IdentifyTextureHost(mTextureFactoryIdentifier);
        return true;
      });
    }
  }
  return success;
}

void
TabChild::InitAPZState()
{
  if (!mCompositorOptions->UseAPZ()) {
    return;
  }
  auto cbc = CompositorBridgeChild::Get();

  // Initialize the ApzcTreeManager. This takes multiple casts because of ugly multiple inheritance.
  PAPZCTreeManagerChild* baseProtocol = cbc->SendPAPZCTreeManagerConstructor(mLayersId);
  APZCTreeManagerChild* derivedProtocol = static_cast<APZCTreeManagerChild*>(baseProtocol);

  mApzcTreeManager = RefPtr<IAPZCTreeManager>(derivedProtocol);

  // Initialize the GeckoContentController for this tab. We don't hold a reference because we don't need it.
  // The ContentProcessController will hold a reference to the tab, and will be destroyed by the compositor or ipdl
  // during destruction.
  RefPtr<GeckoContentController> contentController = new ContentProcessController(this);
  APZChild* apzChild = new APZChild(contentController);
  cbc->SetEventTargetForActor(
    apzChild, TabGroup()->EventTargetFor(TaskCategory::Other));
  MOZ_ASSERT(apzChild->GetActorEventTarget());
  cbc->SendPAPZConstructor(apzChild, mLayersId);
}

void
TabChild::NotifyPainted()
{
    if (!mNotified) {
        // Recording/replaying processes have a compositor but not a remote frame.
        if (!recordreplay::IsRecordingOrReplaying()) {
            SendNotifyCompositorTransaction();
        }
        mNotified = true;
    }
}

void
TabChild::MakeVisible()
{
  if (IsVisible()) {
    return;
  }

  if (mPuppetWidget) {
    mPuppetWidget->Show(true);
  }
}

void
TabChild::MakeHidden()
{
  if (!IsVisible()) {
    return;
  }

  // Due to the nested event loop in ContentChild::ProvideWindowCommon,
  // it's possible to be told to become hidden before we're finished
  // setting up a layer manager. We should skip clearing cached layers
  // in that case, since doing so might accidentally put is into
  // BasicLayers mode.
  if (mPuppetWidget && mPuppetWidget->HasLayerManager()) {
    ClearCachedResources();
  }

  nsCOMPtr<nsIDocShell> docShell = do_GetInterface(WebNavigation());
  if (docShell) {
    // Hide all plugins in this tab. We don't use TabChildBase::GetPresShell()
    // here because that would create a content viewer if one doesn't exist yet.
    // Creating a content viewer can cause JS to run, which we want to avoid.
    // nsIDocShell::GetPresShell returns null if no content viewer exists yet.
    if (nsCOMPtr<nsIPresShell> presShell = docShell->GetPresShell()) {
      if (nsPresContext* presContext = presShell->GetPresContext()) {
        nsRootPresContext* rootPresContext = presContext->GetRootPresContext();
        nsIFrame* rootFrame = presShell->GetRootFrame();
        rootPresContext->ComputePluginGeometryUpdates(rootFrame, nullptr, nullptr);
        rootPresContext->ApplyPluginGeometryUpdates();
      }
      presShell->SetIsActive(false);
    }
  }

  if (mPuppetWidget) {
    mPuppetWidget->Show(false);
  }
}

bool
TabChild::IsVisible()
{
  return mPuppetWidget && mPuppetWidget->IsVisible();
}

NS_IMETHODIMP
TabChild::GetMessageManager(ContentFrameMessageManager** aResult)
{
  RefPtr<ContentFrameMessageManager> mm(mTabChildMessageManager);
  mm.forget(aResult);
  return *aResult ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
TabChild::GetWebBrowserChrome(nsIWebBrowserChrome3** aWebBrowserChrome)
{
  NS_IF_ADDREF(*aWebBrowserChrome = mWebBrowserChrome);
  return NS_OK;
}

NS_IMETHODIMP
TabChild::SetWebBrowserChrome(nsIWebBrowserChrome3* aWebBrowserChrome)
{
  mWebBrowserChrome = aWebBrowserChrome;
  return NS_OK;
}

void
TabChild::SendRequestFocus(bool aCanFocus)
{
  PBrowserChild::SendRequestFocus(aCanFocus);
}

void
TabChild::EnableDisableCommands(const nsAString& aAction,
                                nsTArray<nsCString>& aEnabledCommands,
                                nsTArray<nsCString>& aDisabledCommands)
{
  PBrowserChild::SendEnableDisableCommands(PromiseFlatString(aAction),
                                           aEnabledCommands, aDisabledCommands);
}

NS_IMETHODIMP
TabChild::GetTabId(uint64_t* aId)
{
  *aId = GetTabId();
  return NS_OK;
}

void
TabChild::SetTabId(const TabId& aTabId)
{
  MOZ_ASSERT(mUniqueId == 0);

  mUniqueId = aTabId;
  NestedTabChildMap()[mUniqueId] = this;
}

bool
TabChild::DoSendBlockingMessage(JSContext* aCx,
                                const nsAString& aMessage,
                                StructuredCloneData& aData,
                                JS::Handle<JSObject *> aCpows,
                                nsIPrincipal* aPrincipal,
                                nsTArray<StructuredCloneData>* aRetVal,
                                bool aIsSync)
{
  ClonedMessageData data;
  if (!BuildClonedMessageDataForChild(Manager(), aData, data)) {
    return false;
  }
  InfallibleTArray<CpowEntry> cpows;
  if (aCpows) {
    jsipc::CPOWManager* mgr = Manager()->GetCPOWManager();
    if (!mgr || !mgr->Wrap(aCx, aCpows, &cpows)) {
      return false;
    }
  }
  if (aIsSync) {
    return SendSyncMessage(PromiseFlatString(aMessage), data, cpows,
                           Principal(aPrincipal), aRetVal);
  }

  return SendRpcMessage(PromiseFlatString(aMessage), data, cpows,
                        Principal(aPrincipal), aRetVal);
}

nsresult
TabChild::DoSendAsyncMessage(JSContext* aCx,
                             const nsAString& aMessage,
                             StructuredCloneData& aData,
                             JS::Handle<JSObject *> aCpows,
                             nsIPrincipal* aPrincipal)
{
  ClonedMessageData data;
  if (!BuildClonedMessageDataForChild(Manager(), aData, data)) {
    return NS_ERROR_DOM_DATA_CLONE_ERR;
  }
  InfallibleTArray<CpowEntry> cpows;
  if (aCpows) {
    jsipc::CPOWManager* mgr = Manager()->GetCPOWManager();
    if (!mgr || !mgr->Wrap(aCx, aCpows, &cpows)) {
      return NS_ERROR_UNEXPECTED;
    }
  }
  if (!SendAsyncMessage(PromiseFlatString(aMessage), cpows,
                        Principal(aPrincipal), data)) {
    return NS_ERROR_UNEXPECTED;
  }
  return NS_OK;
}

/* static */ nsTArray<RefPtr<TabChild>>
TabChild::GetAll()
{
  StaticMutexAutoLock lock(sTabChildrenMutex);

  nsTArray<RefPtr<TabChild>> list;
  if (!sTabChildren) {
    return list;
  }

  for (auto iter = sTabChildren->Iter(); !iter.Done(); iter.Next()) {
    list.AppendElement(iter.Data());
  }

  return list;
}

TabChild*
TabChild::GetFrom(nsIPresShell* aPresShell)
{
  nsIDocument* doc = aPresShell->GetDocument();
  if (!doc) {
      return nullptr;
  }
  nsCOMPtr<nsIDocShell> docShell(doc->GetDocShell());
  return GetFrom(docShell);
}

TabChild*
TabChild::GetFrom(layers::LayersId aLayersId)
{
  StaticMutexAutoLock lock(sTabChildrenMutex);
  if (!sTabChildren) {
    return nullptr;
  }
  return sTabChildren->Get(uint64_t(aLayersId));
}

void
TabChild::DidComposite(mozilla::layers::TransactionId aTransactionId,
                       const TimeStamp& aCompositeStart,
                       const TimeStamp& aCompositeEnd)
{
  MOZ_ASSERT(mPuppetWidget);
  RefPtr<LayerManager> lm = mPuppetWidget->GetLayerManager();
  MOZ_ASSERT(lm);

  lm->DidComposite(aTransactionId, aCompositeStart, aCompositeEnd);
}

void
TabChild::DidRequestComposite(const TimeStamp& aCompositeReqStart,
                              const TimeStamp& aCompositeReqEnd)
{
  nsCOMPtr<nsIDocShell> docShellComPtr = do_GetInterface(WebNavigation());
  if (!docShellComPtr) {
    return;
  }

  nsDocShell* docShell = static_cast<nsDocShell*>(docShellComPtr.get());
  RefPtr<TimelineConsumers> timelines = TimelineConsumers::Get();

  if (timelines && timelines->HasConsumer(docShell)) {
    // Since we're assuming that it's impossible for content JS to directly
    // trigger a synchronous paint, we can avoid capturing a stack trace here,
    // which means we won't run into JS engine reentrancy issues like bug
    // 1310014.
    timelines->AddMarkerForDocShell(docShell,
      "CompositeForwardTransaction", aCompositeReqStart,
      MarkerTracingType::START, MarkerStackRequest::NO_STACK);
    timelines->AddMarkerForDocShell(docShell,
      "CompositeForwardTransaction", aCompositeReqEnd,
      MarkerTracingType::END, MarkerStackRequest::NO_STACK);
  }
}

void
TabChild::ClearCachedResources()
{
  MOZ_ASSERT(mPuppetWidget);
  RefPtr<LayerManager> lm = mPuppetWidget->GetLayerManager();
  MOZ_ASSERT(lm);

  lm->ClearCachedResources();
}

void
TabChild::InvalidateLayers()
{
  MOZ_ASSERT(mPuppetWidget);
  RefPtr<LayerManager> lm = mPuppetWidget->GetLayerManager();
  MOZ_ASSERT(lm);

  FrameLayerBuilder::InvalidateAllLayers(lm);
}

void
TabChild::SchedulePaint()
{
  nsCOMPtr<nsIDocShell> docShell = do_GetInterface(WebNavigation());
  if (!docShell) {
    return;
  }

  // We don't use TabChildBase::GetPresShell() here because that would create
  // a content viewer if one doesn't exist yet. Creating a content viewer can
  // cause JS to run, which we want to avoid. nsIDocShell::GetPresShell
  // returns null if no content viewer exists yet.
  if (nsCOMPtr<nsIPresShell> presShell = docShell->GetPresShell()) {
    if (nsIFrame* root = presShell->GetRootFrame()) {
      root->SchedulePaint();
    }
  }
}

void
TabChild::ReinitRendering()
{
  MOZ_ASSERT(mLayersId.IsValid());

  // Before we establish a new PLayerTransaction, we must connect our layer tree
  // id, CompositorBridge, and the widget compositor all together again.
  // Normally this happens in TabParent before TabChild is given rendering
  // information.
  //
  // In this case, we will send a sync message to our TabParent, which in turn
  // will send a sync message to the Compositor of the widget owning this tab.
  // This guarantees the correct association is in place before our
  // PLayerTransaction constructor message arrives on the cross-process
  // compositor bridge.
  CompositorOptions options;
  SendEnsureLayersConnected(&options);
  mCompositorOptions = Some(options);

  bool success = false;
  RefPtr<CompositorBridgeChild> cb = CompositorBridgeChild::Get();

  if (cb) {
    success = CreateRemoteLayerManager(cb);
  }

  if (!success) {
    NS_WARNING("failed to recreate layer manager");
    return;
  }

  mLayersConnected = Some(true);
  ImageBridgeChild::IdentifyCompositorTextureHost(mTextureFactoryIdentifier);
  gfx::VRManagerChild::IdentifyTextureHost(mTextureFactoryIdentifier);

  InitAPZState();
  RefPtr<LayerManager> lm = mPuppetWidget->GetLayerManager();
  MOZ_ASSERT(lm);
  lm->SetLayersObserverEpoch(mLayersObserverEpoch);

  nsCOMPtr<nsIDocument> doc(GetDocument());
  doc->NotifyLayerManagerRecreated();
}

void
TabChild::ReinitRenderingForDeviceReset()
{
  InvalidateLayers();

  RefPtr<LayerManager> lm = mPuppetWidget->GetLayerManager();
  if (WebRenderLayerManager* wlm = lm->AsWebRenderLayerManager()) {
    wlm->DoDestroy(/* aIsSync */ true);
  } else if (ClientLayerManager* clm = lm->AsClientLayerManager()) {
    if (ShadowLayerForwarder* fwd = clm->AsShadowForwarder()) {
      // Force the LayerTransactionChild to synchronously shutdown. It is
      // okay to do this early, we'll simply stop sending messages. This
      // step is necessary since otherwise the compositor will think we
      // are trying to attach two layer trees to the same ID.
      fwd->SynchronouslyShutdown();
    }
  } else {
    if (mLayersConnected.isNothing()) {
      return;
    }
  }

  // Proceed with destroying and recreating the layer manager.
  ReinitRendering();
}

NS_IMETHODIMP
TabChild::OnShowTooltip(int32_t aXCoords, int32_t aYCoords, const char16_t *aTipText,
                        const char16_t *aTipDir)
{
    nsString str(aTipText);
    nsString dir(aTipDir);
    SendShowTooltip(aXCoords, aYCoords, str, dir);
    return NS_OK;
}

NS_IMETHODIMP
TabChild::OnHideTooltip()
{
    SendHideTooltip();
    return NS_OK;
}

mozilla::ipc::IPCResult
TabChild::RecvRequestNotifyAfterRemotePaint()
{
  // Get the CompositorBridgeChild instance for this content thread.
  CompositorBridgeChild* compositor = CompositorBridgeChild::Get();

  // Tell the CompositorBridgeChild that, when it gets a RemotePaintIsReady
  // message that it should forward it us so that we can bounce it to our
  // TabParent.
  compositor->RequestNotifyAfterRemotePaint(this);
  return IPC_OK();
}

mozilla::ipc::IPCResult
TabChild::RecvUIResolutionChanged(const float& aDpi,
                                  const int32_t& aRounding,
                                  const double& aScale)
{
  ScreenIntSize oldScreenSize = GetInnerSize();
  if (aDpi > 0) {
    mPuppetWidget->UpdateBackingScaleCache(aDpi, aRounding, aScale);
  }
  nsCOMPtr<nsIDocument> document(GetDocument());
  RefPtr<nsPresContext> presContext = document->GetPresContext();
  if (presContext) {
    presContext->UIResolutionChangedSync();
  }

  ScreenIntSize screenSize = GetInnerSize();
  if (mHasValidInnerSize && oldScreenSize != screenSize) {
    ScreenIntRect screenRect = GetOuterRect();
    mPuppetWidget->Resize(screenRect.x + mClientOffset.x + mChromeOffset.x,
                          screenRect.y + mClientOffset.y + mChromeOffset.y,
                          screenSize.width, screenSize.height, true);

    nsCOMPtr<nsIBaseWindow> baseWin = do_QueryInterface(WebNavigation());
    baseWin->SetPositionAndSize(0, 0, screenSize.width, screenSize.height,
                                nsIBaseWindow::eRepaint);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult
TabChild::RecvThemeChanged(nsTArray<LookAndFeelInt>&& aLookAndFeelIntCache)
{
  LookAndFeel::SetIntCache(aLookAndFeelIntCache);
  nsCOMPtr<nsIDocument> document(GetDocument());
  RefPtr<nsPresContext> presContext = document->GetPresContext();
  if (presContext) {
    presContext->ThemeChanged();
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
TabChild::RecvAwaitLargeAlloc()
{
  mAwaitingLA = true;
  return IPC_OK();
}

bool
TabChild::IsAwaitingLargeAlloc()
{
  return mAwaitingLA;
}

bool
TabChild::StopAwaitingLargeAlloc()
{
  bool awaiting = mAwaitingLA;
  mAwaitingLA = false;
  return awaiting;
}

mozilla::ipc::IPCResult
TabChild::RecvSetWindowName(const nsString& aName)
{
  nsCOMPtr<nsIDocShellTreeItem> item = do_QueryInterface(WebNavigation());
  if (item) {
    item->SetName(aName);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
TabChild::RecvAllowScriptsToClose()
{
  nsCOMPtr<nsPIDOMWindowOuter> window = do_GetInterface(WebNavigation());
  if (window) {
    nsGlobalWindowOuter::Cast(window)->AllowScriptsToClose();
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
TabChild::RecvSetOriginAttributes(const OriginAttributes& aOriginAttributes)
{
  nsCOMPtr<nsIDocShell> docShell = do_GetInterface(WebNavigation());
  nsDocShell::Cast(docShell)->SetOriginAttributes(aOriginAttributes);

  return IPC_OK();
}

mozilla::ipc::IPCResult
TabChild::RecvSetWidgetNativeData(const WindowsHandle& aWidgetNativeData)
{
  mWidgetNativeData = aWidgetNativeData;
  return IPC_OK();
}

mozilla::ipc::IPCResult
TabChild::RecvGetContentBlockingLog(GetContentBlockingLogResolver&& aResolve)
{
  bool success = false;
  nsAutoString result;

  if (nsCOMPtr<nsIDocument> doc = GetDocument()) {
    result = doc->GetContentBlockingLog()->Stringify();
    success = true;
  }

  aResolve(Tuple<const nsString&, const bool&>(result, success));
  return IPC_OK();
}

mozilla::plugins::PPluginWidgetChild*
TabChild::AllocPPluginWidgetChild()
{
#ifdef XP_WIN
  return new mozilla::plugins::PluginWidgetChild();
#else
  MOZ_ASSERT_UNREACHABLE("AllocPPluginWidgetChild only supports Windows");
  return nullptr;
#endif
}

bool
TabChild::DeallocPPluginWidgetChild(mozilla::plugins::PPluginWidgetChild* aActor)
{
  delete aActor;
  return true;
}

#ifdef XP_WIN
nsresult
TabChild::CreatePluginWidget(nsIWidget* aParent, nsIWidget** aOut)
{
  *aOut = nullptr;
  mozilla::plugins::PluginWidgetChild* child =
    static_cast<mozilla::plugins::PluginWidgetChild*>(SendPPluginWidgetConstructor());
  if (!child) {
    NS_ERROR("couldn't create PluginWidgetChild");
    return NS_ERROR_UNEXPECTED;
  }
  nsCOMPtr<nsIWidget> pluginWidget = nsIWidget::CreatePluginProxyWidget(this, child);
  if (!pluginWidget) {
    NS_ERROR("couldn't create PluginWidgetProxy");
    return NS_ERROR_UNEXPECTED;
  }

  nsWidgetInitData initData;
  initData.mWindowType = eWindowType_plugin_ipc_content;
  initData.mUnicode = false;
  initData.clipChildren = true;
  initData.clipSiblings = true;
  nsresult rv = pluginWidget->Create(aParent, nullptr,
                                     LayoutDeviceIntRect(0, 0, 0, 0),
                                     &initData);
  if (NS_FAILED(rv)) {
    NS_WARNING("Creating native plugin widget on the chrome side failed.");
  }
  pluginWidget.forget(aOut);
  return rv;
}
#endif // XP_WIN

PPaymentRequestChild*
TabChild::AllocPPaymentRequestChild()
{
  MOZ_CRASH("We should never be manually allocating PPaymentRequestChild actors");
  return nullptr;
}

bool
TabChild::DeallocPPaymentRequestChild(PPaymentRequestChild* actor)
{
  delete actor;
  return true;
}

ScreenIntSize
TabChild::GetInnerSize()
{
  LayoutDeviceIntSize innerSize =
    RoundedToInt(mUnscaledInnerSize * mPuppetWidget->GetDefaultScale());
  return ViewAs<ScreenPixel>(innerSize, PixelCastJustification::LayoutDeviceIsScreenForTabDims);
};

ScreenIntRect
TabChild::GetOuterRect()
{
  LayoutDeviceIntRect outerRect =
    RoundedToInt(mUnscaledOuterRect * mPuppetWidget->GetDefaultScale());
  return ViewAs<ScreenPixel>(outerRect, PixelCastJustification::LayoutDeviceIsScreenForTabDims);
}

void
TabChild::PaintWhileInterruptingJS(const layers::LayersObserverEpoch& aEpoch,
                                   bool aForceRepaint)
{
  if (!IPCOpen() || !mPuppetWidget || !mPuppetWidget->HasLayerManager()) {
    // Don't bother doing anything now. Better to wait until we receive the
    // message on the PContent channel.
    return;
  }

  nsAutoScriptBlocker scriptBlocker;
  RecvRenderLayers(true /* aEnabled */, aForceRepaint, aEpoch);
}

void
TabChild::BeforeUnloadAdded()
{
  // Don't bother notifying the parent if we don't have an IPC link open.
  if (mBeforeUnloadListeners == 0 && IPCOpen()) {
    SendSetHasBeforeUnload(true);
  }

  mBeforeUnloadListeners++;
  MOZ_ASSERT(mBeforeUnloadListeners >= 0);
}

void
TabChild::BeforeUnloadRemoved()
{
  mBeforeUnloadListeners--;
  MOZ_ASSERT(mBeforeUnloadListeners >= 0);

  // Don't bother notifying the parent if we don't have an IPC link open.
  if (mBeforeUnloadListeners == 0 && IPCOpen()) {
    SendSetHasBeforeUnload(false);
  }
}

mozilla::dom::TabGroup*
TabChild::TabGroup()
{
  return mTabGroup;
}

nsresult
TabChild::GetHasSiblings(bool* aHasSiblings)
{
  *aHasSiblings = mHasSiblings;
  return NS_OK;
}

nsresult
TabChild::SetHasSiblings(bool aHasSiblings)
{
  mHasSiblings = aHasSiblings;
  return NS_OK;
}

TabChildMessageManager::TabChildMessageManager(TabChild* aTabChild)
: ContentFrameMessageManager(new nsFrameMessageManager(aTabChild)),
  mTabChild(aTabChild)
{
}

TabChildMessageManager::~TabChildMessageManager()
{
}

NS_IMPL_CYCLE_COLLECTION_CLASS(TabChildMessageManager)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(TabChildMessageManager,
                                                DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mMessageManager);
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mTabChild);
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(TabChildMessageManager,
                                                  DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mMessageManager)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mTabChild)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(TabChildMessageManager)
  NS_INTERFACE_MAP_ENTRY(nsIMessageSender)
  NS_INTERFACE_MAP_ENTRY(ContentFrameMessageManager)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(TabChildMessageManager, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(TabChildMessageManager, DOMEventTargetHelper)

JSObject*
TabChildMessageManager::WrapObject(JSContext* aCx,
                                   JS::Handle<JSObject*> aGivenProto)
{
  return ContentFrameMessageManager_Binding::Wrap(aCx, this, aGivenProto);
}

void
TabChildMessageManager::MarkForCC()
{
  if (mTabChild) {
    mTabChild->MarkScopesForCC();
  }
  EventListenerManager* elm = GetExistingListenerManager();
  if (elm) {
    elm->MarkForCC();
  }
  MessageManagerGlobal::MarkForCC();
}

already_AddRefed<nsPIDOMWindowOuter>
TabChildMessageManager::GetContent(ErrorResult& aError)
{
  if (!mTabChild) {
    aError.Throw(NS_ERROR_NULL_POINTER);
    return nullptr;
  }
  nsCOMPtr<nsPIDOMWindowOuter> window =
    do_GetInterface(mTabChild->WebNavigation());
  return window.forget();
}

already_AddRefed<nsIDocShell>
TabChildMessageManager::GetDocShell(ErrorResult& aError)
{
  if (!mTabChild) {
    aError.Throw(NS_ERROR_NULL_POINTER);
    return nullptr;
  }
  nsCOMPtr<nsIDocShell> window = do_GetInterface(mTabChild->WebNavigation());
  return window.forget();
}

already_AddRefed<nsIEventTarget>
TabChildMessageManager::GetTabEventTarget()
{
  nsCOMPtr<nsIEventTarget> target = EventTargetFor(TaskCategory::Other);
  return target.forget();
}

uint64_t
TabChildMessageManager::ChromeOuterWindowID()
{
  if (!mTabChild) {
    return 0;
  }
  return mTabChild->ChromeOuterWindowID();
}

nsresult
TabChildMessageManager::Dispatch(TaskCategory aCategory,
                         already_AddRefed<nsIRunnable>&& aRunnable)
{
  if (mTabChild && mTabChild->TabGroup()) {
    return mTabChild->TabGroup()->Dispatch(aCategory, std::move(aRunnable));
  }
  return DispatcherTrait::Dispatch(aCategory, std::move(aRunnable));
}

nsISerialEventTarget*
TabChildMessageManager::EventTargetFor(TaskCategory aCategory) const
{
  if (mTabChild && mTabChild->TabGroup()) {
    return mTabChild->TabGroup()->EventTargetFor(aCategory);
  }
  return DispatcherTrait::EventTargetFor(aCategory);
}

AbstractThread*
TabChildMessageManager::AbstractMainThreadFor(TaskCategory aCategory)
{
  if (mTabChild && mTabChild->TabGroup()) {
    return mTabChild->TabGroup()->AbstractMainThreadFor(aCategory);
  }
  return DispatcherTrait::AbstractMainThreadFor(aCategory);
}
