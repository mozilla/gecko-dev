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
#include "nsIFrameLoader.h"
#include "nsPoint.h"
#include "nsSize.h"
#include "nsIURI.h"
#include "nsAutoPtr.h"
#include "nsFrameMessageManager.h"
#include "mozilla/dom/Element.h"
#include "mozilla/Attributes.h"
#include "nsStubMutationObserver.h"
#include "Units.h"

class nsIURI;
class nsSubDocumentFrame;
class nsView;
class nsIInProcessContentFrameMessageManager;
class AutoResetInShow;
class nsITabParent;
class nsIDocShellTreeItem;
class nsIDocShellTreeOwner;
class mozIApplication;

namespace mozilla {
namespace dom {
class ContentParent;
class PBrowserParent;
class TabParent;
struct StructuredCloneData;
}

namespace layout {
class RenderFrameParent;
}
}

#if defined(MOZ_WIDGET_GTK)
typedef struct _GtkWidget GtkWidget;
#endif

class nsFrameLoader final : public nsIFrameLoader,
                            public nsStubMutationObserver,
                            public mozilla::dom::ipc::MessageManagerCallback
{
  friend class AutoResetInShow;
  typedef mozilla::dom::PBrowserParent PBrowserParent;
  typedef mozilla::dom::TabParent TabParent;
  typedef mozilla::layout::RenderFrameParent RenderFrameParent;

public:
  static nsFrameLoader* Create(mozilla::dom::Element* aOwner,
                               bool aNetworkCreated);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsFrameLoader, nsIFrameLoader)
  NS_DECL_NSIFRAMELOADER
  NS_DECL_NSIMUTATIONOBSERVER_ATTRIBUTECHANGED
  nsresult CheckForRecursiveLoad(nsIURI* aURI);
  nsresult ReallyStartLoading();
  void StartDestroy();
  void DestroyDocShell();
  void DestroyComplete();
  nsIDocShell* GetExistingDocShell() { return mDocShell; }
  mozilla::dom::EventTarget* GetTabChildGlobalAsEventTarget();
  nsresult CreateStaticClone(nsIFrameLoader* aDest);

  /**
   * MessageManagerCallback methods that we override.
   */
  virtual bool DoLoadMessageManagerScript(const nsAString& aURL,
                                          bool aRunInGlobalScope) override;
  virtual bool DoSendAsyncMessage(JSContext* aCx,
                                  const nsAString& aMessage,
                                  const mozilla::dom::StructuredCloneData& aData,
                                  JS::Handle<JSObject *> aCpows,
                                  nsIPrincipal* aPrincipal) override;
  virtual bool CheckPermission(const nsAString& aPermission) override;
  virtual bool CheckManifestURL(const nsAString& aManifestURL) override;
  virtual bool CheckAppHasPermission(const nsAString& aPermission) override;

  /**
   * Called from the layout frame associated with this frame loader;
   * this notifies us to hook up with the widget and view.
   */
  bool Show(int32_t marginWidth, int32_t marginHeight,
              int32_t scrollbarPrefX, int32_t scrollbarPrefY,
              nsSubDocumentFrame* frame);

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

  nsresult CloneForStatic(nsIFrameLoader* aOriginal);

  // The guts of an nsIFrameLoaderOwner::SwapFrameLoader implementation.  A
  // frame loader owner needs to call this, and pass in the two references to
  // nsRefPtrs for frame loaders that need to be swapped.
  nsresult SwapWithOtherLoader(nsFrameLoader* aOther,
                               nsRefPtr<nsFrameLoader>& aFirstToSwap,
                               nsRefPtr<nsFrameLoader>& aSecondToSwap);

  nsresult SwapWithOtherRemoteLoader(nsFrameLoader* aOther,
                                     nsRefPtr<nsFrameLoader>& aFirstToSwap,
                                     nsRefPtr<nsFrameLoader>& aSecondToSwap);

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
  RenderFrameParent* GetCurrentRenderFrame() const;

  nsFrameMessageManager* GetFrameMessageManager() { return mMessageManager; }

  mozilla::dom::Element* GetOwnerContent() { return mOwnerContent; }
  bool ShouldClipSubdocument() { return mClipSubdocument; }

  bool ShouldClampScrollPosition() { return mClampScrollPosition; }

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
   * Stashes a detached view on the frame loader. We do this when we're
   * destroying the nsSubDocumentFrame. If the nsSubdocumentFrame is
   * being reframed we'll restore the detached view when it's recreated,
   * otherwise we'll discard the old presentation and set the detached
   * subdoc view to null. aContainerDoc is the document containing the
   * the subdoc frame. This enables us to detect when the containing
   * document has changed during reframe, so we can discard the presentation 
   * in that case.
   */
  void SetDetachedSubdocView(nsView* aDetachedView,
                             nsIDocument* aContainerDoc);

  /**
   * Retrieves the detached view and the document containing the view,
   * as set by SetDetachedSubdocView().
   */
  nsView* GetDetachedSubdocView(nsIDocument** aContainerDoc) const;

  /**
   * Applies a new set of sandbox flags. These are merged with the sandbox
   * flags from our owning content's owning document with a logical OR, this
   * ensures that we can only add restrictions and never remove them.
   */
  void ApplySandboxFlags(uint32_t sandboxFlags);

  void GetURL(nsString& aURL);

  void ActivateUpdateHitRegion();
  void DeactivateUpdateHitRegion();

  // Properly retrieves documentSize of any subdocument type.
  nsresult GetWindowDimensions(nsIntRect& aRect);

  // public because a callback needs these.
  nsRefPtr<nsFrameMessageManager> mMessageManager;
  nsCOMPtr<nsIInProcessContentFrameMessageManager> mChildMessageManager;

private:
  nsFrameLoader(mozilla::dom::Element* aOwner, bool aNetworkCreated);
  ~nsFrameLoader();

  void SetOwnerContent(mozilla::dom::Element* aContent);

  bool ShouldUseRemoteProcess();

  /**
   * Is this a frameloader for a bona fide <iframe mozbrowser> or
   * <iframe mozapp>?  (I.e., does the frame return true for
   * nsIMozBrowserFrame::GetReallyIsBrowserOrApp()?)
   */
  bool OwnerIsBrowserOrAppFrame();

  /**
   * Is this a frameloader for a bona fide <iframe mozwidget>?  (I.e., does the
   * frame return true for nsIMozBrowserFrame::GetReallyIsWidget()?)
   */
  bool OwnerIsWidget();

  /**
   * Is this a frameloader for a bona fide <iframe mozapp>?  (I.e., does the
   * frame return true for nsIMozBrowserFrame::GetReallyIsApp()?)
   */
  bool OwnerIsAppFrame();

  /**
   * Is this a frame loader for a bona fide <iframe mozbrowser>?
   */
  bool OwnerIsBrowserFrame();

  /**
   * Get our owning element's app manifest URL, or return the empty string if
   * our owning element doesn't have an app manifest URL.
   */
  void GetOwnerAppManifestURL(nsAString& aOut);

  /**
   * Get the app for our frame.  This is the app whose manifest is returned by
   * GetOwnerAppManifestURL.
   */
  already_AddRefed<mozIApplication> GetOwnApp();

  /**
   * Get the app which contains this frame.  This is the app associated with
   * the frame element's principal.
   */
  already_AddRefed<mozIApplication> GetContainingApp();

  /**
   * If we are an IPC frame, set mRemoteFrame. Otherwise, create and
   * initialize mDocShell.
   */
  nsresult MaybeCreateDocShell();
  nsresult EnsureMessageManager();

  // Updates the subdocument position and size. This gets called only
  // when we have our own in-process DocShell.
  void UpdateBaseWindowPositionAndSize(nsSubDocumentFrame *aIFrame);
  nsresult CheckURILoad(nsIURI* aURI);
  void FireErrorEvent();
  nsresult ReallyStartLoadingInternal();

  // Return true if remote browser created; nothing else to do
  bool TryRemoteBrowser();

  // Tell the remote browser that it's now "virtually visible"
  bool ShowRemoteFrame(const mozilla::ScreenIntSize& size,
                       nsSubDocumentFrame *aFrame = nullptr);

  bool AddTreeItemToTreeOwner(nsIDocShellTreeItem* aItem,
                              nsIDocShellTreeOwner* aOwner,
                              int32_t aParentType,
                              nsIDocShell* aParentNode);

  nsIAtom* TypeAttrName() const {
    return mOwnerContent->IsXULElement()
             ? nsGkAtoms::type : nsGkAtoms::mozframetype;
  }

  // Update the permission manager's app-id refcount based on mOwnerContent's
  // own-or-containing-app.
  void ResetPermissionManagerStatus();

  void InitializeBrowserAPI();

  nsCOMPtr<nsIDocShell> mDocShell;
  nsCOMPtr<nsIURI> mURIToLoad;
  mozilla::dom::Element* mOwnerContent; // WEAK

  // After the frameloader has been removed from the DOM but before all of the
  // messages from the frame have been received, we keep a strong reference to
  // our <browser> element.
  nsRefPtr<mozilla::dom::Element> mOwnerContentStrong;

  // Note: this variable must be modified only by ResetPermissionManagerStatus()
  uint32_t mAppIdSentToPermissionManager;

  // Stores the root view of the subdocument while the subdocument is being
  // reframed. Used to restore the presentation after reframing.
  nsView* mDetachedSubdocViews;
  // Stores the containing document of the frame corresponding to this
  // frame loader. This is reference is kept valid while the subframe's
  // presentation is detached and stored in mDetachedSubdocViews. This
  // enables us to detect whether the frame has moved documents during
  // a reframe, so that we know not to restore the presentation.
  nsCOMPtr<nsIDocument> mContainerDocWhileDetached;

  TabParent* mRemoteBrowser;
  uint64_t mChildID;

  // See nsIFrameLoader.idl. EVENT_MODE_NORMAL_DISPATCH automatically
  // forwards some input events to out-of-process content.
  uint32_t mEventMode;

  bool mIsPrerendered : 1;
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

  bool mRemoteBrowserShown : 1;
  bool mRemoteFrame : 1;
  bool mClipSubdocument : 1;
  bool mClampScrollPosition : 1;
  bool mObservingOwnerContent : 1;

  // Backs nsIFrameLoader::{Get,Set}Visible.  Visibility state here relates to
  // whether this frameloader's <iframe mozbrowser> is setVisible(true)'ed, and
  // doesn't necessarily correlate with docshell/document visibility.
  bool mVisible : 1;
};

#endif
