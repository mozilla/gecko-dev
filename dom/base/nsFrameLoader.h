/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Class for managing loading of a subframe (creation of the docshell,
 * handling of loads in it, recursion-checking).
 */

#ifndef nsFrameLoader_h_
#define nsFrameLoader_h_

#include "nsIDocShell.h"
#include "nsStringFwd.h"
#include "nsIFrameLoaderOwner.h"
#include "nsPoint.h"
#include "nsSize.h"
#include "nsWrapperCache.h"
#include "nsIURI.h"
#include "nsFrameMessageManager.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/ParentSHistory.h"
#include "mozilla/Attributes.h"
#include "nsStubMutationObserver.h"
#include "Units.h"
#include "nsIFrame.h"
#include "nsPluginTags.h"

class nsIURI;
class nsSubDocumentFrame;
class nsView;
class AutoResetInShow;
class AutoResetInFrameSwap;
class nsITabParent;
class nsIDocShellTreeItem;
class nsIDocShellTreeOwner;
class nsILoadContext;
class nsIMessageSender;
class nsIPrintSettings;
class nsIWebBrowserPersistDocumentReceiver;
class nsIWebProgressListener;

namespace mozilla {

class OriginAttributes;

namespace dom {
class ChromeMessageSender;
class ContentParent;
class InProcessTabChildMessageManager;
class MessageSender;
class PBrowserParent;
class ProcessMessageManager;
class Promise;
class TabParent;
class MutableTabContext;

namespace ipc {
class StructuredCloneData;
} // namespace ipc

} // namespace dom

namespace layout {
class RenderFrame;
} // namespace layout
} // namespace mozilla

#if defined(MOZ_WIDGET_GTK)
typedef struct _GtkWidget GtkWidget;
#endif

// IID for nsFrameLoader, because some places want to QI to it.
#define NS_FRAMELOADER_IID                                      \
  { 0x297fd0ea, 0x1b4a, 0x4c9a,                                 \
      { 0xa4, 0x04, 0xe5, 0x8b, 0xe8, 0x95, 0x10, 0x50 } }

class nsFrameLoader final : public nsStubMutationObserver,
                            public mozilla::dom::ipc::MessageManagerCallback,
                            public nsWrapperCache
{
  friend class AutoResetInShow;
  friend class AutoResetInFrameSwap;
  typedef mozilla::dom::PBrowserParent PBrowserParent;
  typedef mozilla::dom::TabParent TabParent;
  typedef mozilla::layout::RenderFrame RenderFrame;

public:
  static nsFrameLoader* Create(mozilla::dom::Element* aOwner,
                               nsPIDOMWindowOuter* aOpener,
                               bool aNetworkCreated,
                               int32_t aJSPluginID = nsFakePluginTag::NOT_JSPLUGIN);

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_FRAMELOADER_IID)

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(nsFrameLoader)

  NS_DECL_NSIMUTATIONOBSERVER_ATTRIBUTECHANGED
  nsresult CheckForRecursiveLoad(nsIURI* aURI);
  nsresult ReallyStartLoading();
  void StartDestroy();
  void DestroyDocShell();
  void DestroyComplete();
  nsIDocShell* GetExistingDocShell() { return mDocShell; }
  mozilla::dom::InProcessTabChildMessageManager* GetTabChildMessageManager() const
  {
    return mChildMessageManager;
  }
  nsresult CreateStaticClone(nsFrameLoader* aDest);
  nsresult UpdatePositionAndSize(nsSubDocumentFrame *aIFrame);

  // WebIDL methods

  nsIDocShell* GetDocShell(mozilla::ErrorResult& aRv);

  already_AddRefed<nsITabParent> GetTabParent();

  already_AddRefed<nsILoadContext> LoadContext();

  /**
   * Start loading the frame. This method figures out what to load
   * from the owner content in the frame loader.
   */
  void LoadFrame(bool aOriginalSrc);

  /**
   * Loads the specified URI in this frame. Behaves identically to loadFrame,
   * except that this method allows specifying the URI to load.
   *
   * @param aURI The URI to load.
   * @param aTriggeringPrincipal The triggering principal for the load. May be
   *        null, in which case the node principal of the owner content will be
   *        used.
   */
  nsresult LoadURI(nsIURI* aURI, nsIPrincipal* aTriggeringPrincipal,
                   bool aOriginalSrc);

  /**
   * Destroy the frame loader and everything inside it. This will
   * clear the weak owner content reference.
   */
  void Destroy();

  void ActivateRemoteFrame(mozilla::ErrorResult& aRv);

  void DeactivateRemoteFrame(mozilla::ErrorResult& aRv);

  void SendCrossProcessMouseEvent(const nsAString& aType,
                                  float aX,
                                  float aY,
                                  int32_t aButton,
                                  int32_t aClickCount,
                                  int32_t aModifiers,
                                  bool aIgnoreRootScrollFrame,
                                  mozilla::ErrorResult& aRv);

  void ActivateFrameEvent(const nsAString& aType,
                          bool aCapture,
                          mozilla::ErrorResult& aRv);

  void RequestNotifyAfterRemotePaint();

  void RequestUpdatePosition(mozilla::ErrorResult& aRv);

  void Print(uint64_t aOuterWindowID,
             nsIPrintSettings* aPrintSettings,
             nsIWebProgressListener* aProgressListener,
             mozilla::ErrorResult& aRv);

  already_AddRefed<mozilla::dom::Promise>
  DrawSnapshot(double aX,
               double aY,
               double aW,
               double aH,
               double aScale,
               const nsAString& aBackgroundColor,
               mozilla::ErrorResult& aRv);

  void StartPersistence(uint64_t aOuterWindowID,
                        nsIWebBrowserPersistDocumentReceiver* aRecv,
                        mozilla::ErrorResult& aRv);

  // WebIDL getters

  already_AddRefed<mozilla::dom::MessageSender> GetMessageManager();

  already_AddRefed<Element> GetOwnerElement();

  uint32_t LazyWidth() const;

  uint32_t LazyHeight() const;

  uint64_t ChildID() const { return mChildID; }

  bool ClampScrollPosition() const { return mClampScrollPosition; }
  void SetClampScrollPosition(bool aClamp);

  bool DepthTooGreat() const { return mDepthTooGreat; }

  bool IsDead() const { return mDestroyCalled; }

  /**
   * Is this a frame loader for a bona fide <iframe mozbrowser>?
   * <xul:browser> is not a mozbrowser, so this is false for that case.
   */
  bool OwnerIsMozBrowserFrame();

  nsIContent* GetParentObject() const { return mOwnerContent; }


  /**
   * MessageManagerCallback methods that we override.
   */
  virtual bool DoLoadMessageManagerScript(const nsAString& aURL,
                                          bool aRunInGlobalScope) override;
  virtual nsresult DoSendAsyncMessage(JSContext* aCx,
                                      const nsAString& aMessage,
                                      mozilla::dom::ipc::StructuredCloneData& aData,
                                      JS::Handle<JSObject *> aCpows,
                                      nsIPrincipal* aPrincipal) override;

  /**
   * Called from the layout frame associated with this frame loader;
   * this notifies us to hook up with the widget and view.
   */
  bool Show(int32_t marginWidth, int32_t marginHeight,
              int32_t scrollbarPrefX, int32_t scrollbarPrefY,
              nsSubDocumentFrame* frame);

  void MaybeShowFrame();

  /**
   * Called when the margin properties of the containing frame are changed.
   */
  void MarginsChanged(uint32_t aMarginWidth, uint32_t aMarginHeight);

  /**
   * Called from the layout frame associated with this frame loader, when
   * the frame is being torn down; this notifies us that out widget and view
   * are going away and we should unhook from them.
   */
  void Hide();

  // Used when content is causing a FrameLoader to be created, and
  // needs to try forcing layout to flush in order to get accurate
  // dimensions for the content area.
  void ForceLayoutIfNecessary();

  // The guts of an nsIFrameLoaderOwner::SwapFrameLoader implementation.  A
  // frame loader owner needs to call this, and pass in the two references to
  // nsRefPtrs for frame loaders that need to be swapped.
  nsresult SwapWithOtherLoader(nsFrameLoader* aOther,
                               nsIFrameLoaderOwner* aThisOwner,
                               nsIFrameLoaderOwner* aOtherOwner);

  nsresult SwapWithOtherRemoteLoader(nsFrameLoader* aOther,
                                     nsIFrameLoaderOwner* aThisOwner,
                                     nsIFrameLoaderOwner* aOtherOwner);

  /**
   * Return the primary frame for our owning content, or null if it
   * can't be found.
   */
  nsIFrame* GetPrimaryFrameOfOwningContent() const
  {
    return mOwnerContent ? mOwnerContent->GetPrimaryFrame() : nullptr;
  }

  /**
   * Return the document that owns this, or null if we don't have
   * an owner.
   */
  nsIDocument* GetOwnerDoc() const
  { return mOwnerContent ? mOwnerContent->OwnerDoc() : nullptr; }

  PBrowserParent* GetRemoteBrowser() const;

  /**
   * The "current" render frame is the one on which the most recent
   * remote layer-tree transaction was executed.  If no content has
   * been drawn yet, or the remote browser doesn't have any drawn
   * content for whatever reason, return nullptr.  The returned render
   * frame has an associated shadow layer tree.
   *
   * Note that the returned render frame might not be a frame
   * constructed for this->GetURL().  This can happen, e.g., if the
   * <browser> was just navigated to a new URL, but hasn't painted the
   * new page yet.  A render frame for the previous page may be
   * returned.  (In-process <browser> behaves similarly, and this
   * behavior seems desirable.)
   */
  RenderFrame* GetCurrentRenderFrame() const;

  mozilla::dom::ChromeMessageSender* GetFrameMessageManager() { return mMessageManager; }

  mozilla::dom::Element* GetOwnerContent() { return mOwnerContent; }

  bool ShouldClampScrollPosition() { return mClampScrollPosition; }

  mozilla::dom::ParentSHistory* GetParentSHistory() { return mParentSHistory; }

  /**
   * Tell this FrameLoader to use a particular remote browser.
   *
   * This will assert if mRemoteBrowser is non-null.  In practice,
   * this means you can't have successfully run TryRemoteBrowser() on
   * this object, which means you can't have called ShowRemoteFrame()
   * or ReallyStartLoading().
   */
  void SetRemoteBrowser(nsITabParent* aTabParent);

  /**
   * Stashes a detached nsIFrame on the frame loader. We do this when we're
   * destroying the nsSubDocumentFrame. If the nsSubdocumentFrame is
   * being reframed we'll restore the detached nsIFrame when it's recreated,
   * otherwise we'll discard the old presentation and set the detached
   * subdoc nsIFrame to null. aContainerDoc is the document containing the
   * the subdoc frame. This enables us to detect when the containing
   * document has changed during reframe, so we can discard the presentation
   * in that case.
   */
  void SetDetachedSubdocFrame(nsIFrame* aDetachedFrame,
                              nsIDocument* aContainerDoc);

  /**
   * Retrieves the detached nsIFrame and the document containing the nsIFrame,
   * as set by SetDetachedSubdocFrame().
   */
  nsIFrame* GetDetachedSubdocFrame(nsIDocument** aContainerDoc) const;

  /**
   * Applies a new set of sandbox flags. These are merged with the sandbox
   * flags from our owning content's owning document with a logical OR, this
   * ensures that we can only add restrictions and never remove them.
   */
  void ApplySandboxFlags(uint32_t sandboxFlags);

  void GetURL(nsString& aURL, nsIPrincipal** aTriggeringPrincipal);

  // Properly retrieves documentSize of any subdocument type.
  nsresult GetWindowDimensions(nsIntRect& aRect);

  virtual mozilla::dom::ProcessMessageManager* GetProcessMessageManager() const override;

  // public because a callback needs these.
  RefPtr<mozilla::dom::ChromeMessageSender> mMessageManager;
  RefPtr<mozilla::dom::InProcessTabChildMessageManager> mChildMessageManager;

  virtual JSObject* WrapObject(JSContext* cx, JS::Handle<JSObject*> aGivenProto) override;

private:
  nsFrameLoader(mozilla::dom::Element* aOwner,
                nsPIDOMWindowOuter* aOpener,
                bool aNetworkCreated,
                int32_t aJSPluginID);
  ~nsFrameLoader();

  void SetOwnerContent(mozilla::dom::Element* aContent);

  bool ShouldUseRemoteProcess();

  /**
   * Return true if the frame is a remote frame. Return false otherwise
   */
  bool IsRemoteFrame();

  bool IsForJSPlugin()
  {
    return mJSPluginID != nsFakePluginTag::NOT_JSPLUGIN;
  }

  /**
   * Is this a frame loader for an isolated <iframe mozbrowser>?
   *
   * By default, mozbrowser frames are isolated.  Isolation can be disabled by
   * setting the frame's noisolation attribute.  Disabling isolation is
   * only allowed if the containing document is chrome.
   */
  bool OwnerIsIsolatedMozBrowserFrame();

  /**
   * Get our owning element's app manifest URL, or return the empty string if
   * our owning element doesn't have an app manifest URL.
   */
  void GetOwnerAppManifestURL(nsAString& aOut);

  /**
   * If we are an IPC frame, set mRemoteFrame. Otherwise, create and
   * initialize mDocShell.
   */
  nsresult MaybeCreateDocShell();
  nsresult EnsureMessageManager();
  nsresult ReallyLoadFrameScripts();

  // Updates the subdocument position and size. This gets called only
  // when we have our own in-process DocShell.
  void UpdateBaseWindowPositionAndSize(nsSubDocumentFrame *aIFrame);

  /**
   * Checks whether a load of the given URI should be allowed, and returns an
   * error result if it should not.
   *
   * @param aURI The URI to check.
   * @param aTriggeringPrincipal The triggering principal for the load. May be
   *        null, in which case the node principal of the owner content is used.
   */
  nsresult CheckURILoad(nsIURI* aURI, nsIPrincipal* aTriggeringPrincipal);
  void FireErrorEvent();
  nsresult ReallyStartLoadingInternal();

  // Return true if remote browser created; nothing else to do
  bool TryRemoteBrowser();

  // Tell the remote browser that it's now "virtually visible"
  bool ShowRemoteFrame(const mozilla::ScreenIntSize& size,
                       nsSubDocumentFrame *aFrame = nullptr);

  void AddTreeItemToTreeOwner(nsIDocShellTreeItem* aItem,
                              nsIDocShellTreeOwner* aOwner);

  nsAtom* TypeAttrName() const {
    return mOwnerContent->IsXULElement()
             ? nsGkAtoms::type : nsGkAtoms::mozframetype;
  }

  void InitializeBrowserAPI();
  void DestroyBrowserFrameScripts();

  nsresult GetNewTabContext(mozilla::dom::MutableTabContext* aTabContext,
                            nsIURI* aURI = nullptr);

  enum TabParentChange {
    eTabParentRemoved,
    eTabParentChanged
  };
  void MaybeUpdatePrimaryTabParent(TabParentChange aChange);

  nsresult
  PopulateUserContextIdFromAttribute(mozilla::OriginAttributes& aAttr);

  nsCOMPtr<nsIDocShell> mDocShell;
  nsCOMPtr<nsIURI> mURIToLoad;
  nsCOMPtr<nsIPrincipal> mTriggeringPrincipal;
  mozilla::dom::Element* mOwnerContent; // WEAK

  // After the frameloader has been removed from the DOM but before all of the
  // messages from the frame have been received, we keep a strong reference to
  // our <browser> element.
  RefPtr<mozilla::dom::Element> mOwnerContentStrong;

  // Stores the root frame of the subdocument while the subdocument is being
  // reframed. Used to restore the presentation after reframing.
  WeakFrame mDetachedSubdocFrame;
  // Stores the containing document of the frame corresponding to this
  // frame loader. This is reference is kept valid while the subframe's
  // presentation is detached and stored in mDetachedSubdocFrame. This
  // enables us to detect whether the frame has moved documents during
  // a reframe, so that we know not to restore the presentation.
  nsCOMPtr<nsIDocument> mContainerDocWhileDetached;

  // An opener window which should be used when the docshell is created.
  nsCOMPtr<nsPIDOMWindowOuter> mOpener;

  TabParent* mRemoteBrowser;
  uint64_t mChildID;

  int32_t mJSPluginID;

  // Holds the last known size of the frame.
  mozilla::ScreenIntSize mLazySize;

  RefPtr<mozilla::dom::ParentSHistory> mParentSHistory;

  bool mDepthTooGreat : 1;
  bool mIsTopLevelContent : 1;
  bool mDestroyCalled : 1;
  bool mNeedsAsyncDestroy : 1;
  bool mInSwap : 1;
  bool mInShow : 1;
  bool mHideCalled : 1;
  // True when the object is created for an element which the parser has
  // created using NS_FROM_PARSER_NETWORK flag. If the element is modified,
  // it may lose the flag.
  bool mNetworkCreated : 1;

  // True if a pending load corresponds to the original src (or srcdoc)
  // attribute of the frame element.
  bool mLoadingOriginalSrc : 1;

  bool mRemoteBrowserShown : 1;
  bool mRemoteFrame : 1;
  bool mClampScrollPosition : 1;
  bool mObservingOwnerContent : 1;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsFrameLoader, NS_FRAMELOADER_IID)

inline nsISupports*
ToSupports(nsFrameLoader* aFrameLoader)
{
  return aFrameLoader;
}

#endif
