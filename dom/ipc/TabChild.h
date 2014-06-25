/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 2; -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TabChild_h
#define mozilla_dom_TabChild_h

#include "mozilla/dom/PBrowserChild.h"
#include "nsIWebNavigation.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsIWebBrowserChrome2.h"
#include "nsIEmbeddingSiteWindow.h"
#include "nsIWebBrowserChromeFocus.h"
#include "nsIDOMEventListener.h"
#include "nsIInterfaceRequestor.h"
#include "nsIWindowProvider.h"
#include "nsIDOMWindow.h"
#include "nsIDocShell.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsFrameMessageManager.h"
#include "nsIWebProgressListener.h"
#include "nsIPresShell.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsWeakReference.h"
#include "nsITabChild.h"
#include "nsITooltipListener.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/TabContext.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/EventForwards.h"
#include "mozilla/layers/CompositorTypes.h"

class nsICachedFileDescriptorListener;
class nsIDOMWindowUtils;

namespace mozilla {
namespace layout {
class RenderFrameChild;
}

namespace layers {
class ActiveElementManager;
}

namespace widget {
struct AutoCacheNativeKeyCommands;
}

namespace dom {

class TabChild;
class ClonedMessageData;
class TabChildBase;

class TabChildGlobal : public DOMEventTargetHelper,
                       public nsIContentFrameMessageManager,
                       public nsIScriptObjectPrincipal,
                       public nsIGlobalObject
{
public:
  TabChildGlobal(TabChildBase* aTabChild);
  void Init();
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(TabChildGlobal, DOMEventTargetHelper)
  NS_FORWARD_SAFE_NSIMESSAGELISTENERMANAGER(mMessageManager)
  NS_FORWARD_SAFE_NSIMESSAGESENDER(mMessageManager)
  NS_IMETHOD SendSyncMessage(const nsAString& aMessageName,
                             JS::Handle<JS::Value> aObject,
                             JS::Handle<JS::Value> aRemote,
                             nsIPrincipal* aPrincipal,
                             JSContext* aCx,
                             uint8_t aArgc,
                             JS::MutableHandle<JS::Value> aRetval)
  {
    return mMessageManager
      ? mMessageManager->SendSyncMessage(aMessageName, aObject, aRemote,
                                         aPrincipal, aCx, aArgc, aRetval)
      : NS_ERROR_NULL_POINTER;
  }
  NS_IMETHOD SendRpcMessage(const nsAString& aMessageName,
                            JS::Handle<JS::Value> aObject,
                            JS::Handle<JS::Value> aRemote,
                            nsIPrincipal* aPrincipal,
                            JSContext* aCx,
                            uint8_t aArgc,
                            JS::MutableHandle<JS::Value> aRetval)
  {
    return mMessageManager
      ? mMessageManager->SendRpcMessage(aMessageName, aObject, aRemote,
                                        aPrincipal, aCx, aArgc, aRetval)
      : NS_ERROR_NULL_POINTER;
  }
  NS_IMETHOD GetContent(nsIDOMWindow** aContent) MOZ_OVERRIDE;
  NS_IMETHOD GetDocShell(nsIDocShell** aDocShell) MOZ_OVERRIDE;
  NS_IMETHOD Dump(const nsAString& aStr) MOZ_OVERRIDE
  {
    return mMessageManager ? mMessageManager->Dump(aStr) : NS_OK;
  }
  NS_IMETHOD PrivateNoteIntentionalCrash() MOZ_OVERRIDE;
  NS_IMETHOD Btoa(const nsAString& aBinaryData,
                  nsAString& aAsciiBase64String) MOZ_OVERRIDE;
  NS_IMETHOD Atob(const nsAString& aAsciiString,
                  nsAString& aBinaryData) MOZ_OVERRIDE;

  NS_IMETHOD AddEventListener(const nsAString& aType,
                              nsIDOMEventListener* aListener,
                              bool aUseCapture)
  {
    // By default add listeners only for trusted events!
    return DOMEventTargetHelper::AddEventListener(aType, aListener,
                                                  aUseCapture, false, 2);
  }
  using DOMEventTargetHelper::AddEventListener;
  NS_IMETHOD AddEventListener(const nsAString& aType,
                              nsIDOMEventListener* aListener,
                              bool aUseCapture, bool aWantsUntrusted,
                              uint8_t optional_argc) MOZ_OVERRIDE
  {
    return DOMEventTargetHelper::AddEventListener(aType, aListener,
                                                  aUseCapture,
                                                  aWantsUntrusted,
                                                  optional_argc);
  }

  nsresult
  PreHandleEvent(EventChainPreVisitor& aVisitor)
  {
    aVisitor.mForceContentDispatch = true;
    return NS_OK;
  }

  virtual JSContext* GetJSContextForEventHandlers() MOZ_OVERRIDE;
  virtual nsIPrincipal* GetPrincipal() MOZ_OVERRIDE;
  virtual JSObject* GetGlobalJSObject() MOZ_OVERRIDE;

  nsCOMPtr<nsIContentFrameMessageManager> mMessageManager;
  nsRefPtr<TabChildBase> mTabChild;
};

class ContentListener MOZ_FINAL : public nsIDOMEventListener
{
public:
  ContentListener(TabChild* aTabChild) : mTabChild(aTabChild) {}
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMEVENTLISTENER
protected:
  ~ContentListener() {}
  TabChild* mTabChild;
};

// This is base clase which helps to share Viewport and touch related functionality
// between b2g/android FF/embedlite clients implementation.
// It make sense to place in this class all helper functions, and functionality which could be shared between
// Cross-process/Cross-thread implmentations.
class TabChildBase : public nsISupports,
                     public nsFrameScriptExecutor,
                     public ipc::MessageManagerCallback
{
public:
    TabChildBase();
    NS_DECL_CYCLE_COLLECTING_ISUPPORTS
    NS_DECL_CYCLE_COLLECTION_CLASS(TabChildBase)

    virtual nsIWebNavigation* WebNavigation() = 0;
    virtual nsIWidget* WebWidget() = 0;
    nsIPrincipal* GetPrincipal() { return mPrincipal; }
    bool IsAsyncPanZoomEnabled();
    // Recalculates the display state, including the CSS
    // viewport. This should be called whenever we believe the
    // viewport data on a document may have changed. If it didn't
    // change, this function doesn't do anything.  However, it should
    // not be called all the time as it is fairly expensive.
    bool HandlePossibleViewportChange();
    virtual bool DoUpdateZoomConstraints(const uint32_t& aPresShellId,
                                         const mozilla::layers::FrameMetrics::ViewID& aViewId,
                                         const bool& aIsRoot,
                                         const mozilla::layers::ZoomConstraints& aConstraints) = 0;

    nsEventStatus DispatchSynthesizedMouseEvent(uint32_t aMsg, uint64_t aTime,
                                                const LayoutDevicePoint& aRefPoint,
                                                nsIWidget* aWidget);

protected:
    ~TabChildBase() {}
    CSSSize GetPageSize(nsCOMPtr<nsIDocument> aDocument, const CSSSize& aViewport);

    // Get the DOMWindowUtils for the top-level window in this tab.
    already_AddRefed<nsIDOMWindowUtils> GetDOMWindowUtils();
    // Get the Document for the top-level window in this tab.
    already_AddRefed<nsIDocument> GetDocument();

    // Wrapper for nsIDOMWindowUtils.setCSSViewport(). This updates some state
    // variables local to this class before setting it.
    void SetCSSViewport(const CSSSize& aSize);

    // Wraps up a JSON object as a structured clone and sends it to the browser
    // chrome script.
    //
    // XXX/bug 780335: Do the work the browser chrome script does in C++ instead
    // so we don't need things like this.
    void DispatchMessageManagerMessage(const nsAString& aMessageName,
                                       const nsAString& aJSONData);

    nsEventStatus DispatchWidgetEvent(WidgetGUIEvent& event);

    void InitializeRootMetrics();

    mozilla::layers::FrameMetrics ProcessUpdateFrame(const mozilla::layers::FrameMetrics& aFrameMetrics);

    bool UpdateFrameHandler(const mozilla::layers::FrameMetrics& aFrameMetrics);

protected:
    CSSSize mOldViewportSize;
    bool mContentDocumentIsDisplayed;
    nsRefPtr<TabChildGlobal> mTabChildGlobal;
    ScreenIntSize mInnerSize;
    mozilla::layers::FrameMetrics mLastRootMetrics;
    mozilla::layout::ScrollingBehavior mScrolling;
};

class TabChild : public TabChildBase,
                 public PBrowserChild,
                 public nsIWebBrowserChrome2,
                 public nsIEmbeddingSiteWindow,
                 public nsIWebBrowserChromeFocus,
                 public nsIInterfaceRequestor,
                 public nsIWindowProvider,
                 public nsIDOMEventListener,
                 public nsIWebProgressListener,
                 public nsSupportsWeakReference,
                 public nsITabChild,
                 public nsIObserver,
                 public TabContext,
                 public nsITooltipListener
{
    typedef mozilla::dom::ClonedMessageData ClonedMessageData;
    typedef mozilla::layout::RenderFrameChild RenderFrameChild;
    typedef mozilla::layout::ScrollingBehavior ScrollingBehavior;
    typedef mozilla::layers::ActiveElementManager ActiveElementManager;

public:
    static std::map<uint64_t, nsRefPtr<TabChild> >& NestedTabChildMap();

public:
    /** 
     * This is expected to be called off the critical path to content
     * startup.  This is an opportunity to load things that are slow
     * on the critical path.
     */
    static void PreloadSlowThings();

    /** Return a TabChild with the given attributes. */
    static already_AddRefed<TabChild>
    Create(nsIContentChild* aManager, const TabContext& aContext, uint32_t aChromeFlags);

    virtual ~TabChild();

    bool IsRootContentDocument();

    const uint64_t Id() const {
        return mUniqueId;
    }

    static uint64_t
    GetTabChildId(TabChild* aTabChild)
    {
        MOZ_ASSERT(NS_IsMainThread());
        if (aTabChild->Id() != 0) {
            return aTabChild->Id();
        }
        static uint64_t sId = 0;
        sId++;
        aTabChild->mUniqueId = sId;
        NestedTabChildMap()[sId] = aTabChild;
        return sId;
    }

    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_NSIWEBBROWSERCHROME
    NS_DECL_NSIWEBBROWSERCHROME2
    NS_DECL_NSIEMBEDDINGSITEWINDOW
    NS_DECL_NSIWEBBROWSERCHROMEFOCUS
    NS_DECL_NSIINTERFACEREQUESTOR
    NS_DECL_NSIWINDOWPROVIDER
    NS_DECL_NSIDOMEVENTLISTENER
    NS_DECL_NSIWEBPROGRESSLISTENER
    NS_DECL_NSITABCHILD
    NS_DECL_NSIOBSERVER
    NS_DECL_NSITOOLTIPLISTENER

    /**
     * MessageManagerCallback methods that we override.
     */
    virtual bool DoSendBlockingMessage(JSContext* aCx,
                                       const nsAString& aMessage,
                                       const mozilla::dom::StructuredCloneData& aData,
                                       JS::Handle<JSObject *> aCpows,
                                       nsIPrincipal* aPrincipal,
                                       InfallibleTArray<nsString>* aJSONRetVal,
                                       bool aIsSync) MOZ_OVERRIDE;
    virtual bool DoSendAsyncMessage(JSContext* aCx,
                                    const nsAString& aMessage,
                                    const mozilla::dom::StructuredCloneData& aData,
                                    JS::Handle<JSObject *> aCpows,
                                    nsIPrincipal* aPrincipal) MOZ_OVERRIDE;
    virtual bool DoUpdateZoomConstraints(const uint32_t& aPresShellId,
                                         const ViewID& aViewId,
                                         const bool& aIsRoot,
                                         const ZoomConstraints& aConstraints) MOZ_OVERRIDE;
    virtual bool RecvLoadURL(const nsCString& uri) MOZ_OVERRIDE;
    virtual bool RecvCacheFileDescriptor(const nsString& aPath,
                                         const FileDescriptor& aFileDescriptor)
                                         MOZ_OVERRIDE;
    virtual bool RecvShow(const nsIntSize& size) MOZ_OVERRIDE;
    virtual bool RecvUpdateDimensions(const nsRect& rect,
                                      const nsIntSize& size,
                                      const ScreenOrientation& orientation) MOZ_OVERRIDE;
    virtual bool RecvUpdateFrame(const mozilla::layers::FrameMetrics& aFrameMetrics) MOZ_OVERRIDE;
    virtual bool RecvAcknowledgeScrollUpdate(const ViewID& aScrollId,
                                             const uint32_t& aScrollGeneration) MOZ_OVERRIDE;
    virtual bool RecvHandleDoubleTap(const CSSPoint& aPoint,
                                     const mozilla::layers::ScrollableLayerGuid& aGuid) MOZ_OVERRIDE;
    virtual bool RecvHandleSingleTap(const CSSPoint& aPoint,
                                     const mozilla::layers::ScrollableLayerGuid& aGuid) MOZ_OVERRIDE;
    virtual bool RecvHandleLongTap(const CSSPoint& aPoint,
                                   const mozilla::layers::ScrollableLayerGuid& aGuid) MOZ_OVERRIDE;
    virtual bool RecvHandleLongTapUp(const CSSPoint& aPoint,
                                     const mozilla::layers::ScrollableLayerGuid& aGuid) MOZ_OVERRIDE;
    virtual bool RecvNotifyAPZStateChange(const ViewID& aViewId,
                                          const APZStateChange& aChange,
                                          const int& aArg) MOZ_OVERRIDE;
    virtual bool RecvActivate() MOZ_OVERRIDE;
    virtual bool RecvDeactivate() MOZ_OVERRIDE;
    virtual bool RecvMouseEvent(const nsString& aType,
                                const float&    aX,
                                const float&    aY,
                                const int32_t&  aButton,
                                const int32_t&  aClickCount,
                                const int32_t&  aModifiers,
                                const bool&     aIgnoreRootScrollFrame) MOZ_OVERRIDE;
    virtual bool RecvRealMouseEvent(const mozilla::WidgetMouseEvent& event) MOZ_OVERRIDE;
    virtual bool RecvRealKeyEvent(const mozilla::WidgetKeyboardEvent& event,
                                  const MaybeNativeKeyBinding& aBindings) MOZ_OVERRIDE;
    virtual bool RecvMouseWheelEvent(const mozilla::WidgetWheelEvent& event) MOZ_OVERRIDE;
    virtual bool RecvRealTouchEvent(const WidgetTouchEvent& aEvent,
                                    const ScrollableLayerGuid& aGuid) MOZ_OVERRIDE;
    virtual bool RecvRealTouchMoveEvent(const WidgetTouchEvent& aEvent,
                                        const ScrollableLayerGuid& aGuid) MOZ_OVERRIDE;
    virtual bool RecvKeyEvent(const nsString& aType,
                              const int32_t&  aKeyCode,
                              const int32_t&  aCharCode,
                              const int32_t&  aModifiers,
                              const bool&     aPreventDefault) MOZ_OVERRIDE;
    virtual bool RecvCompositionEvent(const mozilla::WidgetCompositionEvent& event) MOZ_OVERRIDE;
    virtual bool RecvTextEvent(const mozilla::WidgetTextEvent& event) MOZ_OVERRIDE;
    virtual bool RecvSelectionEvent(const mozilla::WidgetSelectionEvent& event) MOZ_OVERRIDE;
    virtual bool RecvActivateFrameEvent(const nsString& aType, const bool& capture) MOZ_OVERRIDE;
    virtual bool RecvLoadRemoteScript(const nsString& aURL,
                                      const bool& aRunInGlobalScope) MOZ_OVERRIDE;
    virtual bool RecvAsyncMessage(const nsString& aMessage,
                                  const ClonedMessageData& aData,
                                  const InfallibleTArray<CpowEntry>& aCpows,
                                  const IPC::Principal& aPrincipal) MOZ_OVERRIDE;

    virtual PDocumentRendererChild*
    AllocPDocumentRendererChild(const nsRect& documentRect, const gfx::Matrix& transform,
                                const nsString& bgcolor,
                                const uint32_t& renderFlags, const bool& flushLayout,
                                const nsIntSize& renderSize) MOZ_OVERRIDE;
    virtual bool DeallocPDocumentRendererChild(PDocumentRendererChild* actor) MOZ_OVERRIDE;
    virtual bool RecvPDocumentRendererConstructor(PDocumentRendererChild* actor,
                                                  const nsRect& documentRect,
                                                  const gfx::Matrix& transform,
                                                  const nsString& bgcolor,
                                                  const uint32_t& renderFlags,
                                                  const bool& flushLayout,
                                                  const nsIntSize& renderSize) MOZ_OVERRIDE;

    virtual PColorPickerChild*
    AllocPColorPickerChild(const nsString& title, const nsString& initialColor) MOZ_OVERRIDE;
    virtual bool DeallocPColorPickerChild(PColorPickerChild* actor) MOZ_OVERRIDE;

#ifdef DEBUG
    virtual PContentPermissionRequestChild*
    SendPContentPermissionRequestConstructor(PContentPermissionRequestChild* aActor,
                                             const InfallibleTArray<PermissionRequest>& aRequests,
                                             const IPC::Principal& aPrincipal);
#endif /* DEBUG */

    virtual PContentPermissionRequestChild*
    AllocPContentPermissionRequestChild(const InfallibleTArray<PermissionRequest>& aRequests,
                                        const IPC::Principal& aPrincipal) MOZ_OVERRIDE;
    virtual bool
    DeallocPContentPermissionRequestChild(PContentPermissionRequestChild* actor) MOZ_OVERRIDE;

    virtual PFilePickerChild*
    AllocPFilePickerChild(const nsString& aTitle, const int16_t& aMode) MOZ_OVERRIDE;
    virtual bool
    DeallocPFilePickerChild(PFilePickerChild* actor) MOZ_OVERRIDE;

    virtual POfflineCacheUpdateChild* AllocPOfflineCacheUpdateChild(
            const URIParams& manifestURI,
            const URIParams& documentURI,
            const bool& stickDocument) MOZ_OVERRIDE;
    virtual bool
    DeallocPOfflineCacheUpdateChild(POfflineCacheUpdateChild* offlineCacheUpdate) MOZ_OVERRIDE;

    virtual nsIWebNavigation* WebNavigation() MOZ_OVERRIDE { return mWebNav; }
    virtual nsIWidget* WebWidget() MOZ_OVERRIDE { return mWidget; }

    /** Return the DPI of the widget this TabChild draws to. */
    void GetDPI(float* aDPI);
    void GetDefaultScale(double *aScale);

    ScreenOrientation GetOrientation() { return mOrientation; }

    void SetBackgroundColor(const nscolor& aColor);

    void NotifyPainted();

    void RequestNativeKeyBindings(mozilla::widget::AutoCacheNativeKeyCommands* aAutoCache,
                                  WidgetKeyboardEvent* aEvent);

    /** Return a boolean indicating if the page has called preventDefault on
     *  the event.
     */
    bool DispatchMouseEvent(const nsString&       aType,
                            const CSSPoint&       aPoint,
                            const int32_t&        aButton,
                            const int32_t&        aClickCount,
                            const int32_t&        aModifiers,
                            const bool&           aIgnoreRootScrollFrame,
                            const unsigned short& aInputSourceArg);

    /**
     * Signal to this TabChild that it should be made visible:
     * activated widget, retained layer tree, etc.  (Respectively,
     * made not visible.)
     */
    void MakeVisible();
    void MakeHidden();

    // Returns true if the file descriptor was found in the cache, false
    // otherwise.
    bool GetCachedFileDescriptor(const nsAString& aPath,
                                 nsICachedFileDescriptorListener* aCallback);

    void CancelCachedFileDescriptorCallback(
                                    const nsAString& aPath,
                                    nsICachedFileDescriptorListener* aCallback);

    nsIContentChild* Manager() { return mManager; }

    bool GetUpdateHitRegion() { return mUpdateHitRegion; }

    void UpdateHitRegion(const nsRegion& aRegion);

    static inline TabChild*
    GetFrom(nsIDocShell* aDocShell)
    {
      nsCOMPtr<nsITabChild> tc = do_GetInterface(aDocShell);
      return static_cast<TabChild*>(tc.get());
    }

    static TabChild* GetFrom(nsIPresShell* aPresShell);
    static TabChild* GetFrom(uint64_t aLayersId);

    void DidComposite(uint64_t aTransactionId);

    static inline TabChild*
    GetFrom(nsIDOMWindow* aWindow)
    {
      nsCOMPtr<nsIWebNavigation> webNav = do_GetInterface(aWindow);
      nsCOMPtr<nsIDocShell> docShell = do_QueryInterface(webNav);
      return GetFrom(docShell);
    }

    virtual bool RecvUIResolutionChanged() MOZ_OVERRIDE;

protected:
    virtual PRenderFrameChild* AllocPRenderFrameChild(ScrollingBehavior* aScrolling,
                                                      TextureFactoryIdentifier* aTextureFactoryIdentifier,
                                                      uint64_t* aLayersId,
                                                      bool* aSuccess) MOZ_OVERRIDE;
    virtual bool DeallocPRenderFrameChild(PRenderFrameChild* aFrame) MOZ_OVERRIDE;
    virtual bool RecvDestroy() MOZ_OVERRIDE;
    virtual bool RecvSetUpdateHitRegion(const bool& aEnabled) MOZ_OVERRIDE;
    virtual bool RecvSetIsDocShellActive(const bool& aIsActive) MOZ_OVERRIDE;

    virtual PIndexedDBChild* AllocPIndexedDBChild(const nsCString& aGroup,
                                                  const nsCString& aASCIIOrigin,
                                                  bool* /* aAllowed */) MOZ_OVERRIDE;

    virtual bool DeallocPIndexedDBChild(PIndexedDBChild* aActor) MOZ_OVERRIDE;

private:
    /**
     * Create a new TabChild object.
     *
     * |aOwnOrContainingAppId| is the app-id of our frame or of the closest app
     * frame in the hierarchy which contains us.
     *
     * |aIsBrowserElement| indicates whether we're a browser (but not an app).
     */
    TabChild(nsIContentChild* aManager, const TabContext& aContext, uint32_t aChromeFlags);

    nsresult Init();


    // Notify others that our TabContext has been updated.  (At the moment, this
    // sets the appropriate app-id and is-browser flags on our docshell.)
    //
    // You should call this after calling TabContext::SetTabContext().  We also
    // call this during Init().
    void NotifyTabContextUpdated();

    bool UseDirectCompositor();

    void ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE;

    enum FrameScriptLoading { DONT_LOAD_SCRIPTS, DEFAULT_LOAD_SCRIPTS };
    bool InitTabChildGlobal(FrameScriptLoading aScriptLoading = DEFAULT_LOAD_SCRIPTS);
    bool InitRenderingState();
    void DestroyWindow();
    void SetProcessNameToAppName();

    // Call RecvShow(nsIntSize(0, 0)) and block future calls to RecvShow().
    void DoFakeShow();

    // These methods are used for tracking synthetic mouse events
    // dispatched for compatibility.  On each touch event, we
    // UpdateTapState().  If we've detected that the current gesture
    // isn't a tap, then we CancelTapTracking().  In the meantime, we
    // may detect a context-menu event, and if so we
    // FireContextMenuEvent().
    void FireContextMenuEvent();
    void CancelTapTracking();
    void UpdateTapState(const WidgetTouchEvent& aEvent, nsEventStatus aStatus);

    nsresult
    BrowserFrameProvideWindow(nsIDOMWindow* aOpener,
                              nsIURI* aURI,
                              const nsAString& aName,
                              const nsACString& aFeatures,
                              bool* aWindowIsNew,
                              nsIDOMWindow** aReturn);

    bool HasValidInnerSize();

    class CachedFileDescriptorInfo;
    class CachedFileDescriptorCallbackRunnable;

    TextureFactoryIdentifier mTextureFactoryIdentifier;
    nsCOMPtr<nsIWebNavigation> mWebNav;
    nsCOMPtr<nsIWidget> mWidget;
    nsCOMPtr<nsIURI> mLastURI;
    RenderFrameChild* mRemoteFrame;
    nsRefPtr<nsIContentChild> mManager;
    uint32_t mChromeFlags;
    uint64_t mLayersId;
    nsIntRect mOuterRect;
    // When we're tracking a possible tap gesture, this is the "down"
    // point of the touchstart.
    LayoutDevicePoint mGestureDownPoint;
    // The touch identifier of the active gesture.
    int32_t mActivePointerId;
    // A timer task that fires if the tap-hold timeout is exceeded by
    // the touch we're tracking.  That is, if touchend or a touchmove
    // that exceeds the gesture threshold doesn't happen.
    CancelableTask* mTapHoldTimer;
    // Whether we have already received a FileDescriptor for the app package.
    bool mAppPackageFileDescriptorRecved;
    // At present only 1 of these is really expected.
    nsAutoTArray<nsAutoPtr<CachedFileDescriptorInfo>, 1>
        mCachedFileDescriptorInfos;
    nscolor mLastBackgroundColor;
    bool mDidFakeShow;
    bool mNotified;
    bool mTriedBrowserInit;
    ScreenOrientation mOrientation;
    bool mUpdateHitRegion;
    bool mContextMenuHandled;
    bool mLongTapEventHandled;
    bool mWaitingTouchListeners;
    void FireSingleTapEvent(LayoutDevicePoint aPoint);

    bool mIgnoreKeyPressEvent;
    nsRefPtr<ActiveElementManager> mActiveElementManager;
    bool mHasValidInnerSize;
    uint64_t mUniqueId;

    DISALLOW_EVIL_CONSTRUCTORS(TabChild);
};

}
}

#endif // mozilla_dom_TabChild_h
