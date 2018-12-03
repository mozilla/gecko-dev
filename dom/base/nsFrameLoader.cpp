/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Class for managing loading of a subframe (creation of the docshell,
 * handling of loads in it, recursion-checking).
 */

#include "base/basictypes.h"

#include "prenv.h"

#include "nsDocShell.h"
#include "nsIDOMMozBrowserFrame.h"
#include "nsIDOMWindow.h"
#include "nsIPresShell.h"
#include "nsIContentInlines.h"
#include "nsIContentViewer.h"
#include "nsIDocument.h"
#include "nsPIDOMWindow.h"
#include "nsIWebNavigation.h"
#include "nsIWebProgress.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeOwner.h"
#include "nsDocShellLoadState.h"
#include "nsIBaseWindow.h"
#include "nsIBrowser.h"
#include "nsContentUtils.h"
#include "nsIXPConnect.h"
#include "nsUnicharUtils.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptSecurityManager.h"
#include "nsIScrollable.h"
#include "nsFrameLoader.h"
#include "nsIFrame.h"
#include "nsIScrollableFrame.h"
#include "nsSubDocumentFrame.h"
#include "nsError.h"
#include "nsISHistory.h"
#include "nsIXULWindow.h"
#include "nsIMozBrowserFrame.h"
#include "nsISHistory.h"
#include "nsIScriptError.h"
#include "nsGlobalWindow.h"
#include "nsHTMLDocument.h"
#include "nsPIWindowRoot.h"
#include "nsLayoutUtils.h"
#include "nsMappedAttributes.h"
#include "nsView.h"
#include "nsBaseWidget.h"
#include "nsQueryObject.h"

#include "nsIURI.h"
#include "nsIURL.h"
#include "nsNetUtil.h"

#include "nsGkAtoms.h"
#include "nsNameSpaceManager.h"

#include "nsThreadUtils.h"

#include "nsIDOMChromeWindow.h"
#include "InProcessTabChildMessageManager.h"

#include "Layers.h"
#include "ClientLayerManager.h"

#include "ContentParent.h"
#include "TabParent.h"
#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/GuardObjects.h"
#include "mozilla/HTMLEditor.h"
#include "mozilla/NullPrincipal.h"
#include "mozilla/Preferences.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/ChromeMessageSender.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/FrameLoaderBinding.h"
#include "mozilla/gfx/CrossProcessPaint.h"
#include "mozilla/jsipc/CrossProcessObjectWrappers.h"
#include "mozilla/layout/RenderFrame.h"
#include "mozilla/ServoCSSParser.h"
#include "mozilla/ServoStyleSet.h"
#include "nsGenericHTMLFrameElement.h"
#include "GeckoProfiler.h"

#include "jsapi.h"
#include "mozilla/dom/HTMLIFrameElement.h"
#include "nsSandboxFlags.h"
#include "mozilla/layers/CompositorBridgeChild.h"
#include "mozilla/dom/CustomEvent.h"

#include "mozilla/dom/ipc/StructuredCloneData.h"
#include "mozilla/WebBrowserPersistLocalDocument.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PromiseNativeHandler.h"
#include "mozilla/dom/ParentSHistory.h"
#include "mozilla/dom/ChildSHistory.h"

#include "mozilla/dom/HTMLBodyElement.h"

#include "mozilla/ContentPrincipal.h"

#ifdef XP_WIN
#include "mozilla/plugins/PPluginWidgetParent.h"
#include "../plugins/ipc/PluginWidgetParent.h"
#endif

#ifdef MOZ_XUL
#include "nsXULPopupManager.h"
#endif

#ifdef NS_PRINTING
#include "mozilla/embedding/printingui/PrintingParent.h"
#include "nsIWebBrowserPrint.h"
#endif

using namespace mozilla;
using namespace mozilla::hal;
using namespace mozilla::dom;
using namespace mozilla::dom::ipc;
using namespace mozilla::layers;
using namespace mozilla::layout;
typedef ScrollableLayerGuid::ViewID ViewID;

// Bug 136580: Limit to the number of nested content frames that can have the
//             same URL. This is to stop content that is recursively loading
//             itself.  Note that "#foo" on the end of URL doesn't affect
//             whether it's considered identical, but "?foo" or ";foo" are
//             considered and compared.
// Limit this to 2, like chromium does.
#define MAX_SAME_URL_CONTENT_FRAMES 2

// Bug 8065: Limit content frame depth to some reasonable level. This
// does not count chrome frames when determining depth, nor does it
// prevent chrome recursion.  Number is fairly arbitrary, but meant to
// keep number of shells to a reasonable number on accidental recursion with a
// small (but not 1) branching factor.  With large branching factors the number
// of shells can rapidly become huge and run us out of memory.  To solve that,
// we'd need to re-institute a fixed version of bug 98158.
#define MAX_DEPTH_CONTENT_FRAMES 10

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(nsFrameLoader, mDocShell, mMessageManager,
                                      mChildMessageManager, mOpener,
                                      mParentSHistory)
NS_IMPL_CYCLE_COLLECTING_ADDREF(nsFrameLoader)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsFrameLoader)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsFrameLoader)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY_CONCRETE(nsFrameLoader)
  NS_INTERFACE_MAP_ENTRY(nsIMutationObserver)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

nsFrameLoader::nsFrameLoader(Element* aOwner, nsPIDOMWindowOuter* aOpener,
                             bool aNetworkCreated, int32_t aJSPluginID)
    : mOwnerContent(aOwner),
      mDetachedSubdocFrame(nullptr),
      mOpener(aOpener),
      mRemoteBrowser(nullptr),
      mChildID(0),
      mJSPluginID(aJSPluginID),
      mDepthTooGreat(false),
      mIsTopLevelContent(false),
      mDestroyCalled(false),
      mNeedsAsyncDestroy(false),
      mInSwap(false),
      mInShow(false),
      mHideCalled(false),
      mNetworkCreated(aNetworkCreated),
      mLoadingOriginalSrc(false),
      mRemoteBrowserShown(false),
      mRemoteFrame(false),
      mClampScrollPosition(true),
      mObservingOwnerContent(false) {
  mRemoteFrame = ShouldUseRemoteProcess();
  MOZ_ASSERT(!mRemoteFrame || !aOpener,
             "Cannot pass aOpener for a remote frame!");
}

nsFrameLoader::~nsFrameLoader() {
  if (mMessageManager) {
    mMessageManager->Disconnect();
  }
  MOZ_RELEASE_ASSERT(mDestroyCalled);
}

nsFrameLoader* nsFrameLoader::Create(Element* aOwner,
                                     nsPIDOMWindowOuter* aOpener,
                                     bool aNetworkCreated,
                                     int32_t aJSPluginId) {
  NS_ENSURE_TRUE(aOwner, nullptr);
  nsIDocument* doc = aOwner->OwnerDoc();

  // We never create nsFrameLoaders for elements in resource documents.
  //
  // We never create nsFrameLoaders for elements in data documents, unless the
  // document is a static document.
  // Static documents are an exception because any sub-documents need an
  // nsFrameLoader to keep the relevant docShell alive, even though the
  // nsFrameLoader isn't used to load anything (the sub-document is created by
  // the static clone process).
  //
  // We never create nsFrameLoaders for elements that are not
  // in-composed-document, unless the element belongs to a static document.
  // Static documents are an exception because this method is called at a point
  // in the static clone process before aOwner has been inserted into its
  // document.  For other types of documents this wouldn't be a problem since
  // we'd create the nsFrameLoader as necessary after aOwner is inserted into a
  // document, but the mechanisms that take care of that don't apply for static
  // documents so we need to create the nsFrameLoader now. (This isn't wasteful
  // since for a static document we know aOwner will end up in a document and
  // the nsFrameLoader will be used for its docShell.)
  //
  NS_ENSURE_TRUE(!doc->IsResourceDoc() &&
                     ((!doc->IsLoadedAsData() && aOwner->IsInComposedDoc()) ||
                      doc->IsStaticDocument()),
                 nullptr);

  return new nsFrameLoader(aOwner, aOpener, aNetworkCreated, aJSPluginId);
}

void nsFrameLoader::LoadFrame(bool aOriginalSrc) {
  if (NS_WARN_IF(!mOwnerContent)) {
    return;
  }

  nsAutoString src;
  nsCOMPtr<nsIPrincipal> principal;

  bool isSrcdoc = mOwnerContent->IsHTMLElement(nsGkAtoms::iframe) &&
                  mOwnerContent->HasAttr(kNameSpaceID_None, nsGkAtoms::srcdoc);
  if (isSrcdoc) {
    src.AssignLiteral("about:srcdoc");
    principal = mOwnerContent->NodePrincipal();
  } else {
    GetURL(src, getter_AddRefs(principal));

    src.Trim(" \t\n\r");

    if (src.IsEmpty()) {
      // If the frame is a XUL element and has the attribute 'nodefaultsrc=true'
      // then we will not use 'about:blank' as fallback but return early without
      // starting a load if no 'src' attribute is given (or it's empty).
      if (mOwnerContent->IsXULElement() &&
          mOwnerContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::nodefaultsrc,
                                     nsGkAtoms::_true, eCaseMatters)) {
        return;
      }
      src.AssignLiteral("about:blank");
      principal = mOwnerContent->NodePrincipal();
    }
  }

  nsIDocument* doc = mOwnerContent->OwnerDoc();
  if (doc->IsStaticDocument()) {
    return;
  }

  if (doc->IsLoadedAsInteractiveData()) {
    // XBL bindings doc shouldn't load sub-documents.
    return;
  }

  nsCOMPtr<nsIURI> base_uri = mOwnerContent->GetBaseURI();
  auto encoding = doc->GetDocumentCharacterSet();

  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), src, encoding, base_uri);

  // If the URI was malformed, try to recover by loading about:blank.
  if (rv == NS_ERROR_MALFORMED_URI) {
    rv = NS_NewURI(getter_AddRefs(uri), NS_LITERAL_STRING("about:blank"),
                   encoding, base_uri);
  }

  if (NS_SUCCEEDED(rv)) {
    rv = LoadURI(uri, principal, aOriginalSrc);
  }

  if (NS_FAILED(rv)) {
    FireErrorEvent();
  }
}

void nsFrameLoader::FireErrorEvent() {
  if (!mOwnerContent) {
    return;
  }
  RefPtr<AsyncEventDispatcher> loadBlockingAsyncDispatcher =
      new LoadBlockingAsyncEventDispatcher(
          mOwnerContent, NS_LITERAL_STRING("error"), CanBubble::eNo,
          ChromeOnlyDispatch::eNo);
  loadBlockingAsyncDispatcher->PostDOMEvent();
}

nsresult nsFrameLoader::LoadURI(nsIURI* aURI,
                                nsIPrincipal* aTriggeringPrincipal,
                                bool aOriginalSrc) {
  if (!aURI) return NS_ERROR_INVALID_POINTER;
  NS_ENSURE_STATE(!mDestroyCalled && mOwnerContent);
  MOZ_ASSERT(
      aTriggeringPrincipal,
      "Must have an explicit triggeringPrincipal to nsFrameLoader::LoadURI.");

  mLoadingOriginalSrc = aOriginalSrc;

  nsCOMPtr<nsIDocument> doc = mOwnerContent->OwnerDoc();

  nsresult rv;
  // If IsForJSPlugin() returns true then we want to allow the load. We're just
  // loading the source for the implementation of the JS plugin from a URI
  // that's under our control. We will already have done the security checks for
  // loading the plugin content itself in the object/embed loading code.
  if (!IsForJSPlugin()) {
    rv = CheckURILoad(aURI, aTriggeringPrincipal);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  mURIToLoad = aURI;
  mTriggeringPrincipal = aTriggeringPrincipal;
  rv = doc->InitializeFrameLoader(this);
  if (NS_FAILED(rv)) {
    mURIToLoad = nullptr;
    mTriggeringPrincipal = nullptr;
  }
  return rv;
}

nsresult nsFrameLoader::ReallyStartLoading() {
  nsresult rv = ReallyStartLoadingInternal();
  if (NS_FAILED(rv)) {
    FireErrorEvent();
  }

  return rv;
}

nsresult nsFrameLoader::ReallyStartLoadingInternal() {
  NS_ENSURE_STATE(mURIToLoad && mOwnerContent &&
                  mOwnerContent->IsInComposedDoc());

  AUTO_PROFILER_LABEL("nsFrameLoader::ReallyStartLoadingInternal", OTHER);

  if (IsRemoteFrame()) {
    if (!mRemoteBrowser && !TryRemoteBrowser()) {
      NS_WARNING("Couldn't create child process for iframe.");
      return NS_ERROR_FAILURE;
    }

    // FIXME get error codes from child
    mRemoteBrowser->LoadURL(mURIToLoad);

    if (!mRemoteBrowserShown) {
      // This can fail if it's too early to show the frame, we will retry later.
      Unused << ShowRemoteFrame(ScreenIntSize(0, 0));
    }

    return NS_OK;
  }

  nsresult rv = MaybeCreateDocShell();
  if (NS_FAILED(rv)) {
    return rv;
  }
  NS_ASSERTION(mDocShell,
               "MaybeCreateDocShell succeeded with a null mDocShell");

  // Just to be safe, recheck uri.
  rv = CheckURILoad(mURIToLoad, mTriggeringPrincipal);
  NS_ENSURE_SUCCESS(rv, rv);

  RefPtr<nsDocShellLoadState> loadState = new nsDocShellLoadState();

  loadState->SetOriginalFrameSrc(mLoadingOriginalSrc);
  mLoadingOriginalSrc = false;

  // If this frame is sandboxed with respect to origin we will set it up with
  // a null principal later in nsDocShell::DoURILoad.
  // We do it there to correctly sandbox content that was loaded into
  // the frame via other methods than the src attribute.
  // We'll use our principal, not that of the document loaded inside us.  This
  // is very important; needed to prevent XSS attacks on documents loaded in
  // subframes!
  if (mTriggeringPrincipal) {
    loadState->SetTriggeringPrincipal(mTriggeringPrincipal);
  } else {
    loadState->SetTriggeringPrincipal(mOwnerContent->NodePrincipal());
  }

  nsCOMPtr<nsIURI> referrer;

  nsAutoString srcdoc;
  bool isSrcdoc =
      mOwnerContent->IsHTMLElement(nsGkAtoms::iframe) &&
      mOwnerContent->GetAttr(kNameSpaceID_None, nsGkAtoms::srcdoc, srcdoc);

  if (isSrcdoc) {
    nsAutoString referrerStr;
    mOwnerContent->OwnerDoc()->GetReferrer(referrerStr);
    rv = NS_NewURI(getter_AddRefs(referrer), referrerStr);

    loadState->SetSrcdocData(srcdoc);
    nsCOMPtr<nsIURI> baseURI = mOwnerContent->GetBaseURI();
    loadState->SetBaseURI(baseURI);
  } else {
    rv = mOwnerContent->NodePrincipal()->GetURI(getter_AddRefs(referrer));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Use referrer as long as it is not an NullPrincipalURI.
  // We could add a method such as GetReferrerURI to principals to make this
  // cleaner, but given that we need to start using Source Browsing Context for
  // referrer (see Bug 960639) this may be wasted effort at this stage.
  if (referrer) {
    bool isNullPrincipalScheme;
    rv = referrer->SchemeIs(NS_NULLPRINCIPAL_SCHEME, &isNullPrincipalScheme);
    if (NS_SUCCEEDED(rv) && !isNullPrincipalScheme) {
      loadState->SetReferrer(referrer);
    }
  }

  // get referrer policy for this iframe:
  // first load document wide policy, then
  // load iframe referrer attribute if enabled in preferences
  // per element referrer overrules document wide referrer if enabled
  net::ReferrerPolicy referrerPolicy =
      mOwnerContent->OwnerDoc()->GetReferrerPolicy();
  HTMLIFrameElement* iframe = HTMLIFrameElement::FromNode(mOwnerContent);
  if (iframe) {
    net::ReferrerPolicy iframeReferrerPolicy =
        iframe->GetReferrerPolicyAsEnum();
    if (iframeReferrerPolicy != net::RP_Unset) {
      referrerPolicy = iframeReferrerPolicy;
    }
  }
  loadState->SetReferrerPolicy(referrerPolicy);

  // Default flags:
  int32_t flags = nsIWebNavigation::LOAD_FLAGS_NONE;

  // Flags for browser frame:
  if (OwnerIsMozBrowserFrame()) {
    flags = nsIWebNavigation::LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP |
            nsIWebNavigation::LOAD_FLAGS_DISALLOW_INHERIT_PRINCIPAL;
  }

  loadState->SetIsFromProcessingFrameAttributes();

  // Kick off the load...
  bool tmpState = mNeedsAsyncDestroy;
  mNeedsAsyncDestroy = true;
  loadState->SetURI(mURIToLoad);
  loadState->SetLoadFlags(flags);
  loadState->SetFirstParty(false);
  rv = mDocShell->LoadURI(loadState);
  mNeedsAsyncDestroy = tmpState;
  mURIToLoad = nullptr;
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult nsFrameLoader::CheckURILoad(nsIURI* aURI,
                                     nsIPrincipal* aTriggeringPrincipal) {
  // Check for security.  The fun part is trying to figure out what principals
  // to use.  The way I figure it, if we're doing a LoadFrame() accidentally
  // (eg someone created a frame/iframe node, we're being parsed, XUL iframes
  // are being reframed, etc.) then we definitely want to use the node
  // principal of mOwnerContent for security checks.  If, on the other hand,
  // someone's setting the src on our owner content, or created it via script,
  // or whatever, then they can clearly access it... and we should still use
  // the principal of mOwnerContent.  I don't think that leads to privilege
  // escalation, and it's reasonably guaranteed to not lead to XSS issues
  // (since caller can already access mOwnerContent in this case).  So just use
  // the principal of mOwnerContent no matter what.  If script wants to run
  // things with its own permissions, which differ from those of mOwnerContent
  // (which means the script is privileged in some way) it should set
  // window.location instead.
  nsIScriptSecurityManager* secMan = nsContentUtils::GetSecurityManager();

  // Get our principal
  nsIPrincipal* principal =
      (aTriggeringPrincipal ? aTriggeringPrincipal
                            : mOwnerContent->NodePrincipal());

  // Check if we are allowed to load absURL
  nsresult rv = secMan->CheckLoadURIWithPrincipal(
      principal, aURI, nsIScriptSecurityManager::STANDARD);
  if (NS_FAILED(rv)) {
    return rv;  // We're not
  }

  // Bail out if this is an infinite recursion scenario
  if (IsRemoteFrame()) {
    return NS_OK;
  }
  return CheckForRecursiveLoad(aURI);
}

nsIDocShell* nsFrameLoader::GetDocShell(ErrorResult& aRv) {
  if (IsRemoteFrame()) {
    return nullptr;
  }

  // If we have an owner, make sure we have a docshell and return
  // that. If not, we're most likely in the middle of being torn down,
  // then we just return null.
  if (mOwnerContent) {
    nsresult rv = MaybeCreateDocShell();
    if (NS_FAILED(rv)) {
      aRv.Throw(rv);
      return nullptr;
    }
    NS_ASSERTION(mDocShell,
                 "MaybeCreateDocShell succeeded, but null mDocShell");
  }

  return mDocShell;
}

static void SetTreeOwnerAndChromeEventHandlerOnDocshellTree(
    nsIDocShellTreeItem* aItem, nsIDocShellTreeOwner* aOwner,
    EventTarget* aHandler) {
  MOZ_ASSERT(aItem, "Must have item");

  aItem->SetTreeOwner(aOwner);

  int32_t childCount = 0;
  aItem->GetChildCount(&childCount);
  for (int32_t i = 0; i < childCount; ++i) {
    nsCOMPtr<nsIDocShellTreeItem> item;
    aItem->GetChildAt(i, getter_AddRefs(item));
    if (aHandler) {
      nsCOMPtr<nsIDocShell> shell(do_QueryInterface(item));
      shell->SetChromeEventHandler(aHandler);
    }
    SetTreeOwnerAndChromeEventHandlerOnDocshellTree(item, aOwner, aHandler);
  }
}

#if defined(MOZ_DIAGNOSTIC_ASSERT_ENABLED)
static bool CheckDocShellType(mozilla::dom::Element* aOwnerContent,
                              nsIDocShellTreeItem* aDocShell, nsAtom* aAtom) {
  bool isContent = aOwnerContent->AttrValueIs(kNameSpaceID_None, aAtom,
                                              nsGkAtoms::content, eIgnoreCase);

  if (!isContent) {
    nsCOMPtr<nsIMozBrowserFrame> mozbrowser =
        aOwnerContent->GetAsMozBrowserFrame();
    if (mozbrowser) {
      mozbrowser->GetMozbrowser(&isContent);
    }
  }

  if (isContent) {
    return aDocShell->ItemType() == nsIDocShellTreeItem::typeContent;
  }

  nsCOMPtr<nsIDocShellTreeItem> parent;
  aDocShell->GetParent(getter_AddRefs(parent));

  return parent && parent->ItemType() == aDocShell->ItemType();
}
#endif  // defined(MOZ_DIAGNOSTIC_ASSERT_ENABLED)

/**
 * Hook up a given TreeItem to its tree owner. aItem's type must have already
 * been set, and it should already be part of the DocShellTree.
 * @param aItem the treeitem we're working with
 * @param aTreeOwner the relevant treeowner; might be null
 */
void nsFrameLoader::AddTreeItemToTreeOwner(nsIDocShellTreeItem* aItem,
                                           nsIDocShellTreeOwner* aOwner) {
  MOZ_ASSERT(aItem, "Must have docshell treeitem");
  MOZ_ASSERT(mOwnerContent, "Must have owning content");

  MOZ_DIAGNOSTIC_ASSERT(
      CheckDocShellType(mOwnerContent, aItem, TypeAttrName()),
      "Correct ItemType should be set when creating BrowsingContext");

  if (mIsTopLevelContent) {
    bool is_primary = mOwnerContent->AttrValueIs(
        kNameSpaceID_None, nsGkAtoms::primary, nsGkAtoms::_true, eIgnoreCase);
    if (aOwner) {
      mOwnerContent->AddMutationObserver(this);
      mObservingOwnerContent = true;
      aOwner->ContentShellAdded(aItem, is_primary);
    }
  }
}

static bool AllDescendantsOfType(nsIDocShellTreeItem* aParentItem,
                                 int32_t aType) {
  int32_t childCount = 0;
  aParentItem->GetChildCount(&childCount);

  for (int32_t i = 0; i < childCount; ++i) {
    nsCOMPtr<nsIDocShellTreeItem> kid;
    aParentItem->GetChildAt(i, getter_AddRefs(kid));

    if (kid->ItemType() != aType || !AllDescendantsOfType(kid, aType)) {
      return false;
    }
  }

  return true;
}

/**
 * A class that automatically sets mInShow to false when it goes
 * out of scope.
 */
class MOZ_RAII AutoResetInShow {
 private:
  nsFrameLoader* mFrameLoader;
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
 public:
  explicit AutoResetInShow(
      nsFrameLoader* aFrameLoader MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : mFrameLoader(aFrameLoader) {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
  }
  ~AutoResetInShow() { mFrameLoader->mInShow = false; }
};

static bool ParentWindowIsActive(nsIDocument* aDoc) {
  nsCOMPtr<nsPIWindowRoot> root = nsContentUtils::GetWindowRoot(aDoc);
  if (root) {
    nsPIDOMWindowOuter* rootWin = root->GetWindow();
    return rootWin && rootWin->IsActive();
  }
  return false;
}

void nsFrameLoader::MaybeShowFrame() {
  nsIFrame* frame = GetPrimaryFrameOfOwningContent();
  if (frame) {
    nsSubDocumentFrame* subDocFrame = do_QueryFrame(frame);
    if (subDocFrame) {
      subDocFrame->MaybeShowViewer();
    }
  }
}

bool nsFrameLoader::Show(int32_t marginWidth, int32_t marginHeight,
                         int32_t scrollbarPrefX, int32_t scrollbarPrefY,
                         nsSubDocumentFrame* frame) {
  if (mInShow) {
    return false;
  }
  // Reset mInShow if we exit early.
  AutoResetInShow resetInShow(this);
  mInShow = true;

  ScreenIntSize size = frame->GetSubdocumentSize();
  if (IsRemoteFrame()) {
    return ShowRemoteFrame(size, frame);
  }

  nsresult rv = MaybeCreateDocShell();
  if (NS_FAILED(rv)) {
    return false;
  }
  NS_ASSERTION(mDocShell, "MaybeCreateDocShell succeeded, but null mDocShell");
  if (!mDocShell) {
    return false;
  }

  mDocShell->SetMarginWidth(marginWidth);
  mDocShell->SetMarginHeight(marginHeight);

  nsCOMPtr<nsIScrollable> sc = do_QueryInterface(mDocShell);
  if (sc) {
    sc->SetDefaultScrollbarPreferences(nsIScrollable::ScrollOrientation_X,
                                       scrollbarPrefX);
    sc->SetDefaultScrollbarPreferences(nsIScrollable::ScrollOrientation_Y,
                                       scrollbarPrefY);
  }

  nsCOMPtr<nsIPresShell> presShell = mDocShell->GetPresShell();
  if (presShell) {
    // Ensure root scroll frame is reflowed in case scroll preferences or
    // margins have changed
    nsIFrame* rootScrollFrame = presShell->GetRootScrollFrame();
    if (rootScrollFrame) {
      presShell->FrameNeedsReflow(rootScrollFrame, nsIPresShell::eResize,
                                  NS_FRAME_IS_DIRTY);
    }
    return true;
  }

  nsView* view = frame->EnsureInnerView();
  if (!view) return false;

  nsCOMPtr<nsIBaseWindow> baseWindow = do_QueryInterface(mDocShell);
  NS_ASSERTION(baseWindow, "Found a nsIDocShell that isn't a nsIBaseWindow.");
  baseWindow->InitWindow(nullptr, view->GetWidget(), 0, 0, size.width,
                         size.height);
  // This is kinda whacky, this "Create()" call doesn't really
  // create anything, one starts to wonder why this was named
  // "Create"...
  baseWindow->Create();
  baseWindow->SetVisibility(true);
  NS_ENSURE_TRUE(mDocShell, false);

  // Trigger editor re-initialization if midas is turned on in the
  // sub-document. This shouldn't be necessary, but given the way our
  // editor works, it is. See
  // https://bugzilla.mozilla.org/show_bug.cgi?id=284245
  presShell = mDocShell->GetPresShell();
  if (presShell) {
    nsIDocument* doc = presShell->GetDocument();
    nsHTMLDocument* htmlDoc =
        doc && doc->IsHTMLOrXHTML() ? doc->AsHTMLDocument() : nullptr;

    if (htmlDoc) {
      nsAutoString designMode;
      htmlDoc->GetDesignMode(designMode);

      if (designMode.EqualsLiteral("on")) {
        // Hold on to the editor object to let the document reattach to the
        // same editor object, instead of creating a new one.
        RefPtr<HTMLEditor> htmlEditor = mDocShell->GetHTMLEditor();
        Unused << htmlEditor;
        htmlDoc->SetDesignMode(NS_LITERAL_STRING("off"), Nothing(),
                               IgnoreErrors());

        htmlDoc->SetDesignMode(NS_LITERAL_STRING("on"), Nothing(),
                               IgnoreErrors());
      } else {
        // Re-initialize the presentation for contenteditable documents
        bool editable = false, hasEditingSession = false;
        mDocShell->GetEditable(&editable);
        mDocShell->GetHasEditingSession(&hasEditingSession);
        RefPtr<HTMLEditor> htmlEditor = mDocShell->GetHTMLEditor();
        if (editable && hasEditingSession && htmlEditor) {
          htmlEditor->PostCreate();
        }
      }
    }
  }

  mInShow = false;
  if (mHideCalled) {
    mHideCalled = false;
    Hide();
    return false;
  }
  return true;
}

void nsFrameLoader::MarginsChanged(uint32_t aMarginWidth,
                                   uint32_t aMarginHeight) {
  // We assume that the margins are always zero for remote frames.
  if (IsRemoteFrame()) return;

  // If there's no docshell, we're probably not up and running yet.
  // nsFrameLoader::Show() will take care of setting the right
  // margins.
  if (!mDocShell) return;

  // Set the margins
  mDocShell->SetMarginWidth(aMarginWidth);
  mDocShell->SetMarginHeight(aMarginHeight);

  // There's a cached property declaration block
  // that needs to be updated
  if (nsIDocument* doc = mDocShell->GetDocument()) {
    for (nsINode* cur = doc; cur; cur = cur->GetNextNode()) {
      if (cur->IsHTMLElement(nsGkAtoms::body)) {
        static_cast<HTMLBodyElement*>(cur)->ClearMappedServoStyle();
      }
    }
  }

  // Trigger a restyle if there's a prescontext
  // FIXME: This could do something much less expensive.
  RefPtr<nsPresContext> presContext = mDocShell->GetPresContext();
  if (presContext)
    // rebuild, because now the same nsMappedAttributes* will produce
    // a different style
    presContext->RebuildAllStyleData(nsChangeHint(0), eRestyle_Subtree);
}

bool nsFrameLoader::ShowRemoteFrame(const ScreenIntSize& size,
                                    nsSubDocumentFrame* aFrame) {
  AUTO_PROFILER_LABEL("nsFrameLoader::ShowRemoteFrame", OTHER);
  NS_ASSERTION(IsRemoteFrame(),
               "ShowRemote only makes sense on remote frames.");

  if (!mRemoteBrowser && !TryRemoteBrowser()) {
    NS_ERROR("Couldn't create child process.");
    return false;
  }

  // FIXME/bug 589337: Show()/Hide() is pretty expensive for
  // cross-process layers; need to figure out what behavior we really
  // want here.  For now, hack.
  if (!mRemoteBrowserShown) {
    if (!mOwnerContent || !mOwnerContent->GetComposedDoc()) {
      return false;
    }

    // We never want to host remote frameloaders in simple popups, like menus.
    nsIWidget* widget = nsContentUtils::WidgetForContent(mOwnerContent);
    if (!widget || static_cast<nsBaseWidget*>(widget)->IsSmallPopup()) {
      return false;
    }

    RenderFrame* rf = GetCurrentRenderFrame();
    if (!rf) {
      return false;
    }

    if (!rf->AttachLayerManager()) {
      // This is just not going to work.
      return false;
    }

    mRemoteBrowser->Show(size, ParentWindowIsActive(mOwnerContent->OwnerDoc()));
    mRemoteBrowserShown = true;

    nsCOMPtr<nsIObserverService> os = services::GetObserverService();
    if (os) {
      os->NotifyObservers(ToSupports(this), "remote-browser-shown", nullptr);
    }
  } else {
    nsIntRect dimensions;
    NS_ENSURE_SUCCESS(GetWindowDimensions(dimensions), false);

    // Don't show remote iframe if we are waiting for the completion of reflow.
    if (!aFrame || !(aFrame->GetStateBits() & NS_FRAME_FIRST_REFLOW)) {
      mRemoteBrowser->UpdateDimensions(dimensions, size);
    }
  }

  return true;
}

void nsFrameLoader::Hide() {
  if (mHideCalled) {
    return;
  }
  if (mInShow) {
    mHideCalled = true;
    return;
  }

  if (!mDocShell) return;

  nsCOMPtr<nsIContentViewer> contentViewer;
  mDocShell->GetContentViewer(getter_AddRefs(contentViewer));
  if (contentViewer) contentViewer->SetSticky(false);

  nsCOMPtr<nsIBaseWindow> baseWin = do_QueryInterface(mDocShell);
  NS_ASSERTION(baseWin,
               "Found an nsIDocShell which doesn't implement nsIBaseWindow.");
  baseWin->SetVisibility(false);
  baseWin->SetParentWidget(nullptr);
}

void nsFrameLoader::ForceLayoutIfNecessary() {
  nsIFrame* frame = GetPrimaryFrameOfOwningContent();
  if (!frame) {
    return;
  }

  nsPresContext* presContext = frame->PresContext();
  if (!presContext) {
    return;
  }

  // Only force the layout flush if the frameloader hasn't ever been
  // run through layout.
  if (frame->GetStateBits() & NS_FRAME_FIRST_REFLOW) {
    if (nsCOMPtr<nsIPresShell> shell = presContext->GetPresShell()) {
      shell->FlushPendingNotifications(FlushType::Layout);
    }
  }
}

nsresult nsFrameLoader::SwapWithOtherRemoteLoader(
    nsFrameLoader* aOther, nsIFrameLoaderOwner* aThisOwner,
    nsIFrameLoaderOwner* aOtherOwner) {
  MOZ_ASSERT(NS_IsMainThread());

#ifdef DEBUG
  RefPtr<nsFrameLoader> first = aThisOwner->GetFrameLoader();
  RefPtr<nsFrameLoader> second = aOtherOwner->GetFrameLoader();
  MOZ_ASSERT(first == this, "aThisOwner must own this");
  MOZ_ASSERT(second == aOther, "aOtherOwner must own aOther");
#endif

  Element* ourContent = mOwnerContent;
  Element* otherContent = aOther->mOwnerContent;

  if (!ourContent || !otherContent) {
    // Can't handle this
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  // Make sure there are no same-origin issues
  bool equal;
  nsresult rv = ourContent->NodePrincipal()->Equals(
      otherContent->NodePrincipal(), &equal);
  if (NS_FAILED(rv) || !equal) {
    // Security problems loom.  Just bail on it all
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  nsIDocument* ourDoc = ourContent->GetComposedDoc();
  nsIDocument* otherDoc = otherContent->GetComposedDoc();
  if (!ourDoc || !otherDoc) {
    // Again, how odd, given that we had docshells
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  nsIPresShell* ourShell = ourDoc->GetShell();
  nsIPresShell* otherShell = otherDoc->GetShell();
  if (!ourShell || !otherShell) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  if (!mRemoteBrowser || !aOther->mRemoteBrowser) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  if (mRemoteBrowser->IsIsolatedMozBrowserElement() !=
      aOther->mRemoteBrowser->IsIsolatedMozBrowserElement()) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  // When we swap docShells, maybe we have to deal with a new page created just
  // for this operation. In this case, the browser code should already have set
  // the correct userContextId attribute value in the owning element, but our
  // docShell, that has been created way before) doesn't know that that
  // happened.
  // This is the reason why now we must retrieve the correct value from the
  // usercontextid attribute before comparing our originAttributes with the
  // other one.
  OriginAttributes ourOriginAttributes = mRemoteBrowser->OriginAttributesRef();
  rv = PopulateUserContextIdFromAttribute(ourOriginAttributes);
  NS_ENSURE_SUCCESS(rv, rv);

  OriginAttributes otherOriginAttributes =
      aOther->mRemoteBrowser->OriginAttributesRef();
  rv = aOther->PopulateUserContextIdFromAttribute(otherOriginAttributes);
  NS_ENSURE_SUCCESS(rv, rv);

  if (ourOriginAttributes != otherOriginAttributes) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  bool ourHasHistory =
      mIsTopLevelContent && ourContent->IsXULElement(nsGkAtoms::browser) &&
      !ourContent->HasAttr(kNameSpaceID_None, nsGkAtoms::disablehistory);
  bool otherHasHistory =
      aOther->mIsTopLevelContent &&
      otherContent->IsXULElement(nsGkAtoms::browser) &&
      !otherContent->HasAttr(kNameSpaceID_None, nsGkAtoms::disablehistory);
  if (ourHasHistory != otherHasHistory) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  if (mInSwap || aOther->mInSwap) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  mInSwap = aOther->mInSwap = true;

  nsIFrame* ourFrame = ourContent->GetPrimaryFrame();
  nsIFrame* otherFrame = otherContent->GetPrimaryFrame();
  if (!ourFrame || !otherFrame) {
    mInSwap = aOther->mInSwap = false;
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  nsSubDocumentFrame* ourFrameFrame = do_QueryFrame(ourFrame);
  if (!ourFrameFrame) {
    mInSwap = aOther->mInSwap = false;
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  rv = ourFrameFrame->BeginSwapDocShells(otherFrame);
  if (NS_FAILED(rv)) {
    mInSwap = aOther->mInSwap = false;
    return rv;
  }

  nsCOMPtr<nsIBrowserDOMWindow> otherBrowserDOMWindow =
      aOther->mRemoteBrowser->GetBrowserDOMWindow();
  nsCOMPtr<nsIBrowserDOMWindow> browserDOMWindow =
      mRemoteBrowser->GetBrowserDOMWindow();

  if (!!otherBrowserDOMWindow != !!browserDOMWindow) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  // Destroy browser frame scripts for content leaving a frame with browser API
  if (OwnerIsMozBrowserFrame() && !aOther->OwnerIsMozBrowserFrame()) {
    DestroyBrowserFrameScripts();
  }
  if (!OwnerIsMozBrowserFrame() && aOther->OwnerIsMozBrowserFrame()) {
    aOther->DestroyBrowserFrameScripts();
  }

  aOther->mRemoteBrowser->SetBrowserDOMWindow(browserDOMWindow);
  mRemoteBrowser->SetBrowserDOMWindow(otherBrowserDOMWindow);

#ifdef XP_WIN
  // Native plugin windows used by this remote content need to be reparented.
  if (nsPIDOMWindowOuter* newWin = ourDoc->GetWindow()) {
    RefPtr<nsIWidget> newParent =
        nsGlobalWindowOuter::Cast(newWin)->GetMainWidget();
    const ManagedContainer<mozilla::plugins::PPluginWidgetParent>& plugins =
        aOther->mRemoteBrowser->ManagedPPluginWidgetParent();
    for (auto iter = plugins.ConstIter(); !iter.Done(); iter.Next()) {
      static_cast<mozilla::plugins::PluginWidgetParent*>(iter.Get()->GetKey())
          ->SetParent(newParent);
    }
  }
#endif  // XP_WIN

  MaybeUpdatePrimaryTabParent(eTabParentRemoved);
  aOther->MaybeUpdatePrimaryTabParent(eTabParentRemoved);

  SetOwnerContent(otherContent);
  aOther->SetOwnerContent(ourContent);

  mRemoteBrowser->SetOwnerElement(otherContent);
  aOther->mRemoteBrowser->SetOwnerElement(ourContent);

  // Update window activation state for the swapped owner content.
  Unused << mRemoteBrowser->SendParentActivated(
      ParentWindowIsActive(otherContent->OwnerDoc()));
  Unused << aOther->mRemoteBrowser->SendParentActivated(
      ParentWindowIsActive(ourContent->OwnerDoc()));

  MaybeUpdatePrimaryTabParent(eTabParentChanged);
  aOther->MaybeUpdatePrimaryTabParent(eTabParentChanged);

  RefPtr<nsFrameMessageManager> ourMessageManager = mMessageManager;
  RefPtr<nsFrameMessageManager> otherMessageManager = aOther->mMessageManager;
  // Swap and setup things in parent message managers.
  if (ourMessageManager) {
    ourMessageManager->SetCallback(aOther);
  }
  if (otherMessageManager) {
    otherMessageManager->SetCallback(this);
  }
  mMessageManager.swap(aOther->mMessageManager);

  // Perform the actual swap of the internal refptrs. We keep a strong reference
  // to ourselves to make sure we don't die while we overwrite our reference to
  // ourself.
  RefPtr<nsFrameLoader> kungFuDeathGrip(this);
  aThisOwner->InternalSetFrameLoader(aOther);
  aOtherOwner->InternalSetFrameLoader(kungFuDeathGrip);

  ourFrameFrame->EndSwapDocShells(otherFrame);

  ourShell->BackingScaleFactorChanged();
  otherShell->BackingScaleFactorChanged();

  // Initialize browser API if needed now that owner content has changed.
  InitializeBrowserAPI();
  aOther->InitializeBrowserAPI();

  mInSwap = aOther->mInSwap = false;

  // Send an updated tab context since owner content type may have changed.
  MutableTabContext ourContext;
  rv = GetNewTabContext(&ourContext);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  MutableTabContext otherContext;
  rv = aOther->GetNewTabContext(&otherContext);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Swap the remoteType property as the frameloaders are being swapped
  nsAutoString ourRemoteType;
  if (!ourContent->GetAttr(kNameSpaceID_None, nsGkAtoms::RemoteType,
                           ourRemoteType)) {
    ourRemoteType.AssignLiteral(DEFAULT_REMOTE_TYPE);
  }
  nsAutoString otherRemoteType;
  if (!otherContent->GetAttr(kNameSpaceID_None, nsGkAtoms::RemoteType,
                             otherRemoteType)) {
    otherRemoteType.AssignLiteral(DEFAULT_REMOTE_TYPE);
  }
  ourContent->SetAttr(kNameSpaceID_None, nsGkAtoms::RemoteType, otherRemoteType,
                      false);
  otherContent->SetAttr(kNameSpaceID_None, nsGkAtoms::RemoteType, ourRemoteType,
                        false);

  Unused << mRemoteBrowser->SendSwappedWithOtherRemoteLoader(
      ourContext.AsIPCTabContext());
  Unused << aOther->mRemoteBrowser->SendSwappedWithOtherRemoteLoader(
      otherContext.AsIPCTabContext());
  return NS_OK;
}

class MOZ_RAII AutoResetInFrameSwap final {
 public:
  AutoResetInFrameSwap(
      nsFrameLoader* aThisFrameLoader, nsFrameLoader* aOtherFrameLoader,
      nsDocShell* aThisDocShell, nsDocShell* aOtherDocShell,
      EventTarget* aThisEventTarget,
      EventTarget* aOtherEventTarget MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : mThisFrameLoader(aThisFrameLoader),
        mOtherFrameLoader(aOtherFrameLoader),
        mThisDocShell(aThisDocShell),
        mOtherDocShell(aOtherDocShell),
        mThisEventTarget(aThisEventTarget),
        mOtherEventTarget(aOtherEventTarget) {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;

    mThisFrameLoader->mInSwap = true;
    mOtherFrameLoader->mInSwap = true;
    mThisDocShell->SetInFrameSwap(true);
    mOtherDocShell->SetInFrameSwap(true);

    // Fire pageshow events on still-loading pages, and then fire pagehide
    // events.  Note that we do NOT fire these in the normal way, but just fire
    // them on the chrome event handlers.
    nsContentUtils::FirePageShowEvent(mThisDocShell, mThisEventTarget, false);
    nsContentUtils::FirePageShowEvent(mOtherDocShell, mOtherEventTarget, false);
    nsContentUtils::FirePageHideEvent(mThisDocShell, mThisEventTarget);
    nsContentUtils::FirePageHideEvent(mOtherDocShell, mOtherEventTarget);
  }

  ~AutoResetInFrameSwap() {
    nsContentUtils::FirePageShowEvent(mThisDocShell, mThisEventTarget, true);
    nsContentUtils::FirePageShowEvent(mOtherDocShell, mOtherEventTarget, true);

    mThisFrameLoader->mInSwap = false;
    mOtherFrameLoader->mInSwap = false;
    mThisDocShell->SetInFrameSwap(false);
    mOtherDocShell->SetInFrameSwap(false);
  }

 private:
  RefPtr<nsFrameLoader> mThisFrameLoader;
  RefPtr<nsFrameLoader> mOtherFrameLoader;
  RefPtr<nsDocShell> mThisDocShell;
  RefPtr<nsDocShell> mOtherDocShell;
  nsCOMPtr<EventTarget> mThisEventTarget;
  nsCOMPtr<EventTarget> mOtherEventTarget;
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

nsresult nsFrameLoader::SwapWithOtherLoader(nsFrameLoader* aOther,
                                            nsIFrameLoaderOwner* aThisOwner,
                                            nsIFrameLoaderOwner* aOtherOwner) {
#ifdef DEBUG
  RefPtr<nsFrameLoader> first = aThisOwner->GetFrameLoader();
  RefPtr<nsFrameLoader> second = aOtherOwner->GetFrameLoader();
  MOZ_ASSERT(first == this, "aThisOwner must own this");
  MOZ_ASSERT(second == aOther, "aOtherOwner must own aOther");
#endif

  NS_ENSURE_STATE(!mInShow && !aOther->mInShow);

  if (IsRemoteFrame() != aOther->IsRemoteFrame()) {
    NS_WARNING(
        "Swapping remote and non-remote frames is not currently supported");
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  Element* ourContent = mOwnerContent;
  Element* otherContent = aOther->mOwnerContent;

  if (!ourContent || !otherContent) {
    // Can't handle this
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  bool ourHasSrcdoc = ourContent->IsHTMLElement(nsGkAtoms::iframe) &&
                      ourContent->HasAttr(kNameSpaceID_None, nsGkAtoms::srcdoc);
  bool otherHasSrcdoc =
      otherContent->IsHTMLElement(nsGkAtoms::iframe) &&
      otherContent->HasAttr(kNameSpaceID_None, nsGkAtoms::srcdoc);
  if (ourHasSrcdoc || otherHasSrcdoc) {
    // Ignore this case entirely for now, since we support XUL <-> HTML swapping
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  bool ourFullscreenAllowed =
      ourContent->IsXULElement() ||
      (OwnerIsMozBrowserFrame() &&
       (ourContent->HasAttr(kNameSpaceID_None, nsGkAtoms::allowfullscreen) ||
        ourContent->HasAttr(kNameSpaceID_None, nsGkAtoms::mozallowfullscreen)));
  bool otherFullscreenAllowed =
      otherContent->IsXULElement() ||
      (aOther->OwnerIsMozBrowserFrame() &&
       (otherContent->HasAttr(kNameSpaceID_None, nsGkAtoms::allowfullscreen) ||
        otherContent->HasAttr(kNameSpaceID_None,
                              nsGkAtoms::mozallowfullscreen)));
  if (ourFullscreenAllowed != otherFullscreenAllowed) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  bool ourPaymentRequestAllowed =
      ourContent->HasAttr(kNameSpaceID_None, nsGkAtoms::allowpaymentrequest);
  bool otherPaymentRequestAllowed =
      otherContent->HasAttr(kNameSpaceID_None, nsGkAtoms::allowpaymentrequest);
  if (ourPaymentRequestAllowed != otherPaymentRequestAllowed) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  // Divert to a separate path for the remaining steps in the remote case
  if (IsRemoteFrame()) {
    MOZ_ASSERT(aOther->IsRemoteFrame());
    return SwapWithOtherRemoteLoader(aOther, aThisOwner, aOtherOwner);
  }

  // Make sure there are no same-origin issues
  bool equal;
  nsresult rv = ourContent->NodePrincipal()->Equals(
      otherContent->NodePrincipal(), &equal);
  if (NS_FAILED(rv) || !equal) {
    // Security problems loom.  Just bail on it all
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  RefPtr<nsDocShell> ourDocshell =
      static_cast<nsDocShell*>(GetExistingDocShell());
  RefPtr<nsDocShell> otherDocshell =
      static_cast<nsDocShell*>(aOther->GetExistingDocShell());
  if (!ourDocshell || !otherDocshell) {
    // How odd
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  // To avoid having to mess with session history, avoid swapping
  // frameloaders that don't correspond to root same-type docshells,
  // unless both roots have session history disabled.
  nsCOMPtr<nsIDocShellTreeItem> ourRootTreeItem, otherRootTreeItem;
  ourDocshell->GetSameTypeRootTreeItem(getter_AddRefs(ourRootTreeItem));
  otherDocshell->GetSameTypeRootTreeItem(getter_AddRefs(otherRootTreeItem));
  nsCOMPtr<nsIWebNavigation> ourRootWebnav = do_QueryInterface(ourRootTreeItem);
  nsCOMPtr<nsIWebNavigation> otherRootWebnav =
      do_QueryInterface(otherRootTreeItem);

  if (!ourRootWebnav || !otherRootWebnav) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  RefPtr<ChildSHistory> ourHistory = ourRootWebnav->GetSessionHistory();
  RefPtr<ChildSHistory> otherHistory = otherRootWebnav->GetSessionHistory();

  if ((ourRootTreeItem != ourDocshell || otherRootTreeItem != otherDocshell) &&
      (ourHistory || otherHistory)) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  // Also make sure that the two docshells are the same type. Otherwise
  // swapping is certainly not safe. If this needs to be changed then
  // the code below needs to be audited as it assumes identical types.
  int32_t ourType = ourDocshell->ItemType();
  int32_t otherType = otherDocshell->ItemType();
  if (ourType != otherType) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  // One more twist here.  Setting up the right treeowners in a heterogeneous
  // tree is a bit of a pain.  So make sure that if ourType is not
  // nsIDocShellTreeItem::typeContent then all of our descendants are the same
  // type as us.
  if (ourType != nsIDocShellTreeItem::typeContent &&
      (!AllDescendantsOfType(ourDocshell, ourType) ||
       !AllDescendantsOfType(otherDocshell, otherType))) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  // Save off the tree owners, frame elements, chrome event handlers, and
  // docshell and document parents before doing anything else.
  nsCOMPtr<nsIDocShellTreeOwner> ourOwner, otherOwner;
  ourDocshell->GetTreeOwner(getter_AddRefs(ourOwner));
  otherDocshell->GetTreeOwner(getter_AddRefs(otherOwner));
  // Note: it's OK to have null treeowners.

  nsCOMPtr<nsIDocShellTreeItem> ourParentItem, otherParentItem;
  ourDocshell->GetParent(getter_AddRefs(ourParentItem));
  otherDocshell->GetParent(getter_AddRefs(otherParentItem));
  if (!ourParentItem || !otherParentItem) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  // Make sure our parents are the same type too
  int32_t ourParentType = ourParentItem->ItemType();
  int32_t otherParentType = otherParentItem->ItemType();
  if (ourParentType != otherParentType) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  nsCOMPtr<nsPIDOMWindowOuter> ourWindow = ourDocshell->GetWindow();
  nsCOMPtr<nsPIDOMWindowOuter> otherWindow = otherDocshell->GetWindow();

  nsCOMPtr<Element> ourFrameElement = ourWindow->GetFrameElementInternal();
  nsCOMPtr<Element> otherFrameElement = otherWindow->GetFrameElementInternal();

  nsCOMPtr<EventTarget> ourChromeEventHandler =
      ourWindow->GetChromeEventHandler();
  nsCOMPtr<EventTarget> otherChromeEventHandler =
      otherWindow->GetChromeEventHandler();

  nsCOMPtr<EventTarget> ourEventTarget = ourWindow->GetParentTarget();
  nsCOMPtr<EventTarget> otherEventTarget = otherWindow->GetParentTarget();

  NS_ASSERTION(SameCOMIdentity(ourFrameElement, ourContent) &&
                   SameCOMIdentity(otherFrameElement, otherContent) &&
                   SameCOMIdentity(ourChromeEventHandler, ourContent) &&
                   SameCOMIdentity(otherChromeEventHandler, otherContent),
               "How did that happen, exactly?");

  nsCOMPtr<nsIDocument> ourChildDocument = ourWindow->GetExtantDoc();
  nsCOMPtr<nsIDocument> otherChildDocument = otherWindow->GetExtantDoc();
  if (!ourChildDocument || !otherChildDocument) {
    // This shouldn't be happening
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  nsCOMPtr<nsIDocument> ourParentDocument =
      ourChildDocument->GetParentDocument();
  nsCOMPtr<nsIDocument> otherParentDocument =
      otherChildDocument->GetParentDocument();

  // Make sure to swap docshells between the two frames.
  nsIDocument* ourDoc = ourContent->GetComposedDoc();
  nsIDocument* otherDoc = otherContent->GetComposedDoc();
  if (!ourDoc || !otherDoc) {
    // Again, how odd, given that we had docshells
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  NS_ASSERTION(ourDoc == ourParentDocument, "Unexpected parent document");
  NS_ASSERTION(otherDoc == otherParentDocument, "Unexpected parent document");

  nsIPresShell* ourShell = ourDoc->GetShell();
  nsIPresShell* otherShell = otherDoc->GetShell();
  if (!ourShell || !otherShell) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  if (ourDocshell->GetIsIsolatedMozBrowserElement() !=
      otherDocshell->GetIsIsolatedMozBrowserElement()) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  // When we swap docShells, maybe we have to deal with a new page created just
  // for this operation. In this case, the browser code should already have set
  // the correct userContextId attribute value in the owning element, but our
  // docShell, that has been created way before) doesn't know that that
  // happened.
  // This is the reason why now we must retrieve the correct value from the
  // usercontextid attribute before comparing our originAttributes with the
  // other one.
  OriginAttributes ourOriginAttributes = ourDocshell->GetOriginAttributes();
  rv = PopulateUserContextIdFromAttribute(ourOriginAttributes);
  NS_ENSURE_SUCCESS(rv, rv);

  OriginAttributes otherOriginAttributes = otherDocshell->GetOriginAttributes();
  rv = aOther->PopulateUserContextIdFromAttribute(otherOriginAttributes);
  NS_ENSURE_SUCCESS(rv, rv);

  if (ourOriginAttributes != otherOriginAttributes) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  if (mInSwap || aOther->mInSwap) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  AutoResetInFrameSwap autoFrameSwap(this, aOther, ourDocshell, otherDocshell,
                                     ourEventTarget, otherEventTarget);

  nsIFrame* ourFrame = ourContent->GetPrimaryFrame();
  nsIFrame* otherFrame = otherContent->GetPrimaryFrame();
  if (!ourFrame || !otherFrame) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  nsSubDocumentFrame* ourFrameFrame = do_QueryFrame(ourFrame);
  if (!ourFrameFrame) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  // OK.  First begin to swap the docshells in the two nsIFrames
  rv = ourFrameFrame->BeginSwapDocShells(otherFrame);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Destroy browser frame scripts for content leaving a frame with browser API
  if (OwnerIsMozBrowserFrame() && !aOther->OwnerIsMozBrowserFrame()) {
    DestroyBrowserFrameScripts();
  }
  if (!OwnerIsMozBrowserFrame() && aOther->OwnerIsMozBrowserFrame()) {
    aOther->DestroyBrowserFrameScripts();
  }

  // Now move the docshells to the right docshell trees.  Note that this
  // resets their treeowners to null.
  ourParentItem->RemoveChild(ourDocshell);
  otherParentItem->RemoveChild(otherDocshell);
  if (ourType == nsIDocShellTreeItem::typeContent) {
    ourOwner->ContentShellRemoved(ourDocshell);
    otherOwner->ContentShellRemoved(otherDocshell);
  }

  ourParentItem->AddChild(otherDocshell);
  otherParentItem->AddChild(ourDocshell);

  // Restore the correct chrome event handlers.
  ourDocshell->SetChromeEventHandler(otherChromeEventHandler);
  otherDocshell->SetChromeEventHandler(ourChromeEventHandler);
  // Restore the correct treeowners
  // (and also chrome event handlers for content frames only).
  SetTreeOwnerAndChromeEventHandlerOnDocshellTree(
      ourDocshell, otherOwner,
      ourType == nsIDocShellTreeItem::typeContent
          ? otherChromeEventHandler.get()
          : nullptr);
  SetTreeOwnerAndChromeEventHandlerOnDocshellTree(
      otherDocshell, ourOwner,
      ourType == nsIDocShellTreeItem::typeContent ? ourChromeEventHandler.get()
                                                  : nullptr);

  // Switch the owner content before we start calling AddTreeItemToTreeOwner.
  // Note that we rely on this to deal with setting mObservingOwnerContent to
  // false and calling RemoveMutationObserver as needed.
  SetOwnerContent(otherContent);
  aOther->SetOwnerContent(ourContent);

  AddTreeItemToTreeOwner(ourDocshell, otherOwner);
  aOther->AddTreeItemToTreeOwner(otherDocshell, ourOwner);

  // SetSubDocumentFor nulls out parent documents on the old child doc if a
  // new non-null document is passed in, so just go ahead and remove both
  // kids before reinserting in the parent subdoc maps, to avoid
  // complications.
  ourParentDocument->SetSubDocumentFor(ourContent, nullptr);
  otherParentDocument->SetSubDocumentFor(otherContent, nullptr);
  ourParentDocument->SetSubDocumentFor(ourContent, otherChildDocument);
  otherParentDocument->SetSubDocumentFor(otherContent, ourChildDocument);

  ourWindow->SetFrameElementInternal(otherFrameElement);
  otherWindow->SetFrameElementInternal(ourFrameElement);

  RefPtr<nsFrameMessageManager> ourMessageManager = mMessageManager;
  RefPtr<nsFrameMessageManager> otherMessageManager = aOther->mMessageManager;
  // Swap pointers in child message managers.
  if (mChildMessageManager) {
    InProcessTabChildMessageManager* tabChild = mChildMessageManager;
    tabChild->SetOwner(otherContent);
    tabChild->SetChromeMessageManager(otherMessageManager);
  }
  if (aOther->mChildMessageManager) {
    InProcessTabChildMessageManager* otherTabChild =
        aOther->mChildMessageManager;
    otherTabChild->SetOwner(ourContent);
    otherTabChild->SetChromeMessageManager(ourMessageManager);
  }
  // Swap and setup things in parent message managers.
  if (mMessageManager) {
    mMessageManager->SetCallback(aOther);
  }
  if (aOther->mMessageManager) {
    aOther->mMessageManager->SetCallback(this);
  }
  mMessageManager.swap(aOther->mMessageManager);

  // Perform the actual swap of the internal refptrs. We keep a strong reference
  // to ourselves to make sure we don't die while we overwrite our reference to
  // ourself.
  RefPtr<nsFrameLoader> kungFuDeathGrip(this);
  aThisOwner->InternalSetFrameLoader(aOther);
  aOtherOwner->InternalSetFrameLoader(kungFuDeathGrip);

  // Drop any cached content viewers in the two session histories.
  if (ourHistory) {
    ourHistory->EvictLocalContentViewers();
  }
  if (otherHistory) {
    otherHistory->EvictLocalContentViewers();
  }

  NS_ASSERTION(ourFrame == ourContent->GetPrimaryFrame() &&
                   otherFrame == otherContent->GetPrimaryFrame(),
               "changed primary frame");

  ourFrameFrame->EndSwapDocShells(otherFrame);

  // If the content being swapped came from windows on two screens with
  // incompatible backing resolution (e.g. dragging a tab between windows on
  // hi-dpi and low-dpi screens), it will have style data that is based on
  // the wrong appUnitsPerDevPixel value. So we tell the PresShells that their
  // backing scale factor may have changed. (Bug 822266)
  ourShell->BackingScaleFactorChanged();
  otherShell->BackingScaleFactorChanged();

  // Initialize browser API if needed now that owner content has changed
  InitializeBrowserAPI();
  aOther->InitializeBrowserAPI();

  return NS_OK;
}

void nsFrameLoader::Destroy() { StartDestroy(); }

class nsFrameLoaderDestroyRunnable : public Runnable {
  enum DestroyPhase {
    // See the implementation of Run for an explanation of these phases.
    eDestroyDocShell,
    eWaitForUnloadMessage,
    eDestroyComplete
  };

  RefPtr<nsFrameLoader> mFrameLoader;
  DestroyPhase mPhase;

 public:
  explicit nsFrameLoaderDestroyRunnable(nsFrameLoader* aFrameLoader)
      : mozilla::Runnable("nsFrameLoaderDestroyRunnable"),
        mFrameLoader(aFrameLoader),
        mPhase(eDestroyDocShell) {}

  NS_IMETHOD Run() override;
};

void nsFrameLoader::StartDestroy() {
  // nsFrameLoader::StartDestroy is called just before the frameloader is
  // detached from the <browser> element. Destruction continues in phases via
  // the nsFrameLoaderDestroyRunnable.

  if (mDestroyCalled) {
    return;
  }
  mDestroyCalled = true;

  // After this point, we return an error when trying to send a message using
  // the message manager on the frame.
  if (mMessageManager) {
    mMessageManager->Close();
  }

  // Retain references to the <browser> element and the frameloader in case we
  // receive any messages from the message manager on the frame. These
  // references are dropped in DestroyComplete.
  if (mChildMessageManager || mRemoteBrowser) {
    mOwnerContentStrong = mOwnerContent;
    if (mRemoteBrowser) {
      mRemoteBrowser->CacheFrameLoader(this);
    }
    if (mChildMessageManager) {
      mChildMessageManager->CacheFrameLoader(this);
    }
  }

  // If the TabParent has installed any event listeners on the window, this is
  // its last chance to remove them while we're still in the document.
  if (mRemoteBrowser) {
    mRemoteBrowser->RemoveWindowListeners();
  }

  nsCOMPtr<nsIDocument> doc;
  bool dynamicSubframeRemoval = false;
  if (mOwnerContent) {
    doc = mOwnerContent->OwnerDoc();
    dynamicSubframeRemoval = !mIsTopLevelContent && !doc->InUnlinkOrDeletion();
    doc->SetSubDocumentFor(mOwnerContent, nullptr);
    MaybeUpdatePrimaryTabParent(eTabParentRemoved);
    SetOwnerContent(nullptr);
  }

  // Seems like this is a dynamic frame removal.
  if (dynamicSubframeRemoval) {
    if (mDocShell) {
      mDocShell->RemoveFromSessionHistory();
    }
  }

  // Let the tree owner know we're gone.
  if (mIsTopLevelContent) {
    if (mDocShell) {
      nsCOMPtr<nsIDocShellTreeItem> parentItem;
      mDocShell->GetParent(getter_AddRefs(parentItem));
      nsCOMPtr<nsIDocShellTreeOwner> owner = do_GetInterface(parentItem);
      if (owner) {
        owner->ContentShellRemoved(mDocShell);
      }
    }
  }

  // Let our window know that we are gone
  if (mDocShell) {
    nsCOMPtr<nsPIDOMWindowOuter> win_private(mDocShell->GetWindow());
    if (win_private) {
      win_private->SetFrameElementInternal(nullptr);
    }
  }

  nsCOMPtr<nsIRunnable> destroyRunnable =
      new nsFrameLoaderDestroyRunnable(this);
  if (mNeedsAsyncDestroy || !doc ||
      NS_FAILED(doc->FinalizeFrameLoader(this, destroyRunnable))) {
    NS_DispatchToCurrentThread(destroyRunnable);
  }
}

nsresult nsFrameLoaderDestroyRunnable::Run() {
  switch (mPhase) {
    case eDestroyDocShell:
      mFrameLoader->DestroyDocShell();

      // In the out-of-process case, TabParent will eventually call
      // DestroyComplete once it receives a __delete__ message from the child.
      // In the in-process case, we dispatch a series of runnables to ensure
      // that DestroyComplete gets called at the right time. The frame loader is
      // kept alive by mFrameLoader during this time.
      if (mFrameLoader->mChildMessageManager) {
        // When the docshell is destroyed, NotifyWindowIDDestroyed is called to
        // asynchronously notify {outer,inner}-window-destroyed via a runnable.
        // We don't want DestroyComplete to run until after those runnables have
        // run. Since we're enqueueing ourselves after the window-destroyed
        // runnables are enqueued, we're guaranteed to run after.
        mPhase = eWaitForUnloadMessage;
        NS_DispatchToCurrentThread(this);
      }
      break;

    case eWaitForUnloadMessage:
      // The *-window-destroyed observers have finished running at this
      // point. However, it's possible that a *-window-destroyed observer might
      // have sent a message using the message manager. These messages might not
      // have been processed yet. So we enqueue ourselves again to ensure that
      // DestroyComplete runs after all messages sent by *-window-destroyed
      // observers have been processed.
      mPhase = eDestroyComplete;
      NS_DispatchToCurrentThread(this);
      break;

    case eDestroyComplete:
      // Now that all messages sent by unload listeners and window destroyed
      // observers have been processed, we disconnect the message manager and
      // finish destruction.
      mFrameLoader->DestroyComplete();
      break;
  }

  return NS_OK;
}

void nsFrameLoader::DestroyDocShell() {
  // This code runs after the frameloader has been detached from the <browser>
  // element. We postpone this work because we may not be allowed to run
  // script at that time.

  // Ask the TabChild to fire the frame script "unload" event, destroy its
  // docshell, and finally destroy the PBrowser actor. This eventually leads to
  // nsFrameLoader::DestroyComplete being called.
  if (mRemoteBrowser) {
    mRemoteBrowser->Destroy();
  }

  // Fire the "unload" event if we're in-process.
  if (mChildMessageManager) {
    mChildMessageManager->FireUnloadEvent();
  }

  // Destroy the docshell.
  nsCOMPtr<nsIBaseWindow> base_win(do_QueryInterface(mDocShell));
  if (base_win) {
    base_win->Destroy();
  }
  mDocShell = nullptr;

  if (mChildMessageManager) {
    // Stop handling events in the in-process frame script.
    mChildMessageManager->DisconnectEventListeners();
  }
}

void nsFrameLoader::DestroyComplete() {
  // We get here, as part of StartDestroy, after the docshell has been destroyed
  // and all message manager messages sent during docshell destruction have been
  // dispatched.  We also get here if the child process crashes. In the latter
  // case, StartDestroy might not have been called.

  // Drop the strong references created in StartDestroy.
  if (mChildMessageManager || mRemoteBrowser) {
    mOwnerContentStrong = nullptr;
    if (mRemoteBrowser) {
      mRemoteBrowser->CacheFrameLoader(nullptr);
    }
    if (mChildMessageManager) {
      mChildMessageManager->CacheFrameLoader(nullptr);
    }
  }

  // Call TabParent::Destroy if we haven't already (in case of a crash).
  if (mRemoteBrowser) {
    mRemoteBrowser->SetOwnerElement(nullptr);
    mRemoteBrowser->Destroy();
    mRemoteBrowser = nullptr;
  }

  if (mMessageManager) {
    mMessageManager->Disconnect();
  }

  if (mChildMessageManager) {
    mChildMessageManager->Disconnect();
  }

  mMessageManager = nullptr;
  mChildMessageManager = nullptr;
}

void nsFrameLoader::SetOwnerContent(Element* aContent) {
  if (mObservingOwnerContent) {
    mObservingOwnerContent = false;
    mOwnerContent->RemoveMutationObserver(this);
  }
  mOwnerContent = aContent;

  AutoJSAPI jsapi;
  jsapi.Init();

  JS::RootedObject wrapper(jsapi.cx(), GetWrapper());
  if (wrapper) {
    JSAutoRealm ar(jsapi.cx(), wrapper);
    IgnoredErrorResult rv;
    ReparentWrapper(jsapi.cx(), wrapper, rv);
    Unused << NS_WARN_IF(rv.Failed());
  }

  if (RenderFrame* rfp = GetCurrentRenderFrame()) {
    rfp->OwnerContentChanged(aContent);
  }
}

bool nsFrameLoader::OwnerIsMozBrowserFrame() {
  nsCOMPtr<nsIMozBrowserFrame> browserFrame = do_QueryInterface(mOwnerContent);
  return browserFrame ? browserFrame->GetReallyIsBrowser() : false;
}

bool nsFrameLoader::OwnerIsIsolatedMozBrowserFrame() {
  nsCOMPtr<nsIMozBrowserFrame> browserFrame = do_QueryInterface(mOwnerContent);
  if (!browserFrame) {
    return false;
  }

  if (!OwnerIsMozBrowserFrame()) {
    return false;
  }

  bool isolated = browserFrame->GetIsolated();
  if (isolated) {
    return true;
  }

  return false;
}

bool nsFrameLoader::ShouldUseRemoteProcess() {
  if (IsForJSPlugin()) {
    return true;
  }

  if (PR_GetEnv("MOZ_DISABLE_OOP_TABS") ||
      Preferences::GetBool("dom.ipc.tabs.disabled", false)) {
    return false;
  }

  // Don't try to launch nested children if we don't have OMTC.
  // They won't render!
  if (XRE_IsContentProcess() &&
      !CompositorBridgeChild::ChildProcessHasCompositorBridge()) {
    return false;
  }

  if (XRE_IsContentProcess() &&
      !(PR_GetEnv("MOZ_NESTED_OOP_TABS") ||
        Preferences::GetBool("dom.ipc.tabs.nested.enabled", false))) {
    return false;
  }

  // If we're an <iframe mozbrowser> and we don't have a "remote" attribute,
  // fall back to the default.
  if (OwnerIsMozBrowserFrame() &&
      !mOwnerContent->HasAttr(kNameSpaceID_None, nsGkAtoms::remote)) {
    return Preferences::GetBool("dom.ipc.browser_frames.oop_by_default", false);
  }

  // Otherwise, we're remote if we have "remote=true" and we're either a
  // browser frame or a XUL element.
  return (OwnerIsMozBrowserFrame() ||
          mOwnerContent->GetNameSpaceID() == kNameSpaceID_XUL) &&
         mOwnerContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::remote,
                                    nsGkAtoms::_true, eCaseMatters);
}

bool nsFrameLoader::IsRemoteFrame() {
  if (mRemoteFrame) {
    MOZ_ASSERT(!mDocShell, "Found a remote frame with a DocShell");
    return true;
  }
  return false;
}

static already_AddRefed<BrowsingContext> CreateBrowsingContext(
    BrowsingContext* aParentContext, BrowsingContext* aOpenerContext,
    const nsAString& aName, bool aIsContent) {
  // If we're content but our parent isn't, we're going to want to start a new
  // browsing context tree.
  if (aIsContent && !aParentContext->IsContent()) {
    aParentContext = nullptr;
  }

  BrowsingContext::Type type = aIsContent ? BrowsingContext::Type::Content
                                          : BrowsingContext::Type::Chrome;

  return BrowsingContext::Create(aParentContext, aOpenerContext, aName, type);
}

nsresult nsFrameLoader::MaybeCreateDocShell() {
  if (mDocShell) {
    return NS_OK;
  }
  if (IsRemoteFrame()) {
    return NS_OK;
  }
  NS_ENSURE_STATE(!mDestroyCalled);

  // Get our parent docshell off the document of mOwnerContent
  // XXXbz this is such a total hack.... We really need to have a
  // better setup for doing this.
  nsIDocument* doc = mOwnerContent->OwnerDoc();

  MOZ_RELEASE_ASSERT(!doc->IsResourceDoc(), "We shouldn't even exist");

  // Check if the document still has a window since it is possible for an
  // iframe to be inserted and cause the creation of the docshell in a
  // partially unloaded document (see Bug 1305237 comment 127).
  if (!doc->IsStaticDocument() &&
      (!doc->GetWindow() || !mOwnerContent->IsInComposedDoc())) {
    return NS_ERROR_UNEXPECTED;
  }

  if (!doc->IsActive()) {
    // Don't allow subframe loads in non-active documents.
    // (See bug 610571 comment 5.)
    return NS_ERROR_NOT_AVAILABLE;
  }

  // Determine our parent nsDocShell
  RefPtr<nsDocShell> parentDocShell = nsDocShell::Cast(doc->GetDocShell());
  if (NS_WARN_IF(!parentDocShell)) {
    return NS_ERROR_UNEXPECTED;
  }

  RefPtr<BrowsingContext> parentBC = parentDocShell->GetBrowsingContext();
  MOZ_ASSERT(parentBC, "docShell must have BrowsingContext");

  // Determine the frame name for the new browsing context.
  nsAutoString frameName;

  int32_t namespaceID = mOwnerContent->GetNameSpaceID();
  if (namespaceID == kNameSpaceID_XHTML && !mOwnerContent->IsInHTMLDocument()) {
    mOwnerContent->GetAttr(kNameSpaceID_None, nsGkAtoms::id, frameName);
  } else {
    mOwnerContent->GetAttr(kNameSpaceID_None, nsGkAtoms::name, frameName);
    // XXX if no NAME then use ID, after a transition period this will be
    // changed so that XUL only uses ID too (bug 254284).
    if (frameName.IsEmpty() && namespaceID == kNameSpaceID_XUL) {
      mOwnerContent->GetAttr(kNameSpaceID_None, nsGkAtoms::id, frameName);
    }
  }

  // Check if our new context is chrome or content
  bool isContent = parentBC->IsContent() ||
                   mOwnerContent->AttrValueIs(kNameSpaceID_None, TypeAttrName(),
                                              nsGkAtoms::content, eIgnoreCase);

  // Force mozbrowser frames to always be content, even if the mozbrowser
  // interfaces are disabled.
  nsCOMPtr<nsIMozBrowserFrame> mozbrowser =
      mOwnerContent->GetAsMozBrowserFrame();
  if (!isContent && mozbrowser) {
    mozbrowser->GetMozbrowser(&isContent);
  }

  RefPtr<BrowsingContext> openerBC =
      mOpener ? mOpener->GetBrowsingContext() : nullptr;
  RefPtr<BrowsingContext> browsingContext =
      CreateBrowsingContext(parentBC, openerBC, frameName, isContent);

  mDocShell = nsDocShell::Create(browsingContext);
  NS_ENSURE_TRUE(mDocShell, NS_ERROR_FAILURE);

  mIsTopLevelContent = isContent && !parentBC->IsContent();
  if (!mNetworkCreated && !mIsTopLevelContent) {
    mDocShell->SetCreatedDynamically(true);
  }

  if (mIsTopLevelContent) {
    // Manually add ourselves to our parent's docshell, as BrowsingContext won't
    // have done this for us.
    //
    // XXX(nika): Consider removing the DocShellTree in the future, for
    // consistency between local and remote frames..
    parentDocShell->AddChild(mDocShell);
  }

  // Now that we are part of the DocShellTree, attach our DocShell to our
  // parent's TreeOwner.
  nsCOMPtr<nsIDocShellTreeOwner> parentTreeOwner;
  parentDocShell->GetTreeOwner(getter_AddRefs(parentTreeOwner));
  AddTreeItemToTreeOwner(mDocShell, parentTreeOwner);

  // Make sure all nsDocShells have links back to the content element in the
  // nearest enclosing chrome shell.
  RefPtr<EventTarget> chromeEventHandler;
  if (parentBC->IsContent()) {
    // Our parent shell is a content shell. Get the chrome event handler from it
    // and use that for our shell as well.
    parentDocShell->GetChromeEventHandler(getter_AddRefs(chromeEventHandler));
  } else {
    // Our parent shell is a chrome shell. It is therefore our nearest enclosing
    // chrome shell.
    chromeEventHandler = mOwnerContent;
  }

  mDocShell->SetChromeEventHandler(chromeEventHandler);

  // This is nasty, this code (the mDocShell->GetWindow() below)
  // *must* come *after* the above call to
  // mDocShell->SetChromeEventHandler() for the global window to get
  // the right chrome event handler.

  // Tell the window about the frame that hosts it.
  nsCOMPtr<nsPIDOMWindowOuter> newWindow = mDocShell->GetWindow();
  if (NS_WARN_IF(!newWindow)) {
    // Do not call Destroy() here. See bug 472312.
    NS_WARNING("Something wrong when creating the docshell for a frameloader!");
    return NS_ERROR_FAILURE;
  }

  newWindow->SetFrameElementInternal(mOwnerContent);

  // Set the opener window if we have one provided here XXX(nika): We
  // should tell our BrowsingContext this as we create it.
  // TODO(farre): Remove this when nsGlobalWindowOuter::GetOpenerWindowOuter
  // starts using BrowsingContext::GetOpener.
  if (mOpener) {
    newWindow->SetOpenerWindow(mOpener, true);
    mOpener = nullptr;
  }

  // Allow scripts to close the docshell if specified.
  if (mOwnerContent->IsXULElement(nsGkAtoms::browser) &&
      mOwnerContent->AttrValueIs(kNameSpaceID_None,
                                 nsGkAtoms::allowscriptstoclose,
                                 nsGkAtoms::_true, eCaseMatters)) {
    nsGlobalWindowOuter::Cast(newWindow)->AllowScriptsToClose();
  }

  // This is kinda whacky, this call doesn't really create anything,
  // but it must be called to make sure things are properly
  // initialized.
  nsCOMPtr<nsIBaseWindow> baseWin = do_QueryInterface(mDocShell);
  if (NS_FAILED(baseWin->Create())) {
    // Do not call Destroy() here. See bug 472312.
    NS_WARNING("Something wrong when creating the docshell for a frameloader!");
    return NS_ERROR_FAILURE;
  }

  // If we are an in-process browser, we want to set up our session history. We
  // do this by creating both the child SHistory (which is in the nsDocShell),
  // and creating the corresponding in-process ParentSHistory.
  if (mIsTopLevelContent && mOwnerContent->IsXULElement(nsGkAtoms::browser) &&
      !mOwnerContent->HasAttr(kNameSpaceID_None, nsGkAtoms::disablehistory)) {
    // XXX(nika): Set this up more explicitly?
    nsresult rv = mDocShell->InitSessionHistory();
    NS_ENSURE_SUCCESS(rv, rv);
    mParentSHistory = new ParentSHistory(this);
  }

  OriginAttributes attrs;
  if (parentDocShell->ItemType() == mDocShell->ItemType()) {
    attrs = parentDocShell->GetOriginAttributes();
  }

  // Inherit origin attributes from parent document if
  // 1. It's in a content docshell.
  // 2. its nodePrincipal is not a SystemPrincipal.
  // 3. It's not a mozbrowser frame.
  //
  // For example, firstPartyDomain is computed from top-level document, it
  // doesn't exist in the top-level docshell.
  if (parentBC->IsContent() &&
      !nsContentUtils::IsSystemPrincipal(doc->NodePrincipal()) &&
      !OwnerIsMozBrowserFrame()) {
    OriginAttributes oa = doc->NodePrincipal()->OriginAttributesRef();

    // Assert on the firstPartyDomain from top-level docshell should be empty
    MOZ_ASSERT_IF(mIsTopLevelContent, attrs.mFirstPartyDomain.IsEmpty());

    // So far we want to make sure Inherit doesn't override any other origin
    // attribute than firstPartyDomain.
    MOZ_ASSERT(attrs.mAppId == oa.mAppId,
               "docshell and document should have the same appId attribute.");
    MOZ_ASSERT(
        attrs.mUserContextId == oa.mUserContextId,
        "docshell and document should have the same userContextId attribute.");
    MOZ_ASSERT(attrs.mInIsolatedMozBrowser == oa.mInIsolatedMozBrowser,
               "docshell and document should have the same "
               "inIsolatedMozBrowser attribute.");
    MOZ_ASSERT(attrs.mPrivateBrowsingId == oa.mPrivateBrowsingId,
               "docshell and document should have the same privateBrowsingId "
               "attribute.");

    attrs = oa;
  }

  if (OwnerIsMozBrowserFrame()) {
    attrs.mAppId = nsIScriptSecurityManager::NO_APP_ID;
    attrs.mInIsolatedMozBrowser = OwnerIsIsolatedMozBrowserFrame();
    mDocShell->SetFrameType(nsIDocShell::FRAME_TYPE_BROWSER);
  }

  // Apply sandbox flags even if our owner is not an iframe, as this copies
  // flags from our owning content's owning document.
  // Note: ApplySandboxFlags should be called after mDocShell->SetFrameType
  // because we need to get the correct presentation URL in ApplySandboxFlags.
  uint32_t sandboxFlags = 0;
  HTMLIFrameElement* iframe = HTMLIFrameElement::FromNode(mOwnerContent);
  if (iframe) {
    sandboxFlags = iframe->GetSandboxFlags();
  }
  ApplySandboxFlags(sandboxFlags);

  // Grab the userContextId from owner
  nsresult rv = PopulateUserContextIdFromAttribute(attrs);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool isPrivate = false;
  rv = parentDocShell->GetUsePrivateBrowsing(&isPrivate);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  attrs.SyncAttributesWithPrivateBrowsing(isPrivate);

  if (OwnerIsMozBrowserFrame()) {
    // For inproc frames, set the docshell properties.
    nsAutoString name;
    if (mOwnerContent->GetAttr(kNameSpaceID_None, nsGkAtoms::name, name)) {
      mDocShell->SetName(name);
    }
    mDocShell->SetFullscreenAllowed(
        mOwnerContent->HasAttr(kNameSpaceID_None, nsGkAtoms::allowfullscreen) ||
        mOwnerContent->HasAttr(kNameSpaceID_None,
                               nsGkAtoms::mozallowfullscreen));
    bool isPrivate = mOwnerContent->HasAttr(kNameSpaceID_None,
                                            nsGkAtoms::mozprivatebrowsing);
    if (isPrivate) {
      if (mDocShell->GetHasLoadedNonBlankURI()) {
        nsContentUtils::ReportToConsoleNonLocalized(
            NS_LITERAL_STRING("We should not switch to Private Browsing after "
                              "loading a document."),
            nsIScriptError::warningFlag,
            NS_LITERAL_CSTRING("mozprivatebrowsing"), nullptr);
      } else {
        // This handles the case where a frames private browsing is set by
        // chrome flags and not inherited by its parent.
        attrs.SyncAttributesWithPrivateBrowsing(isPrivate);
      }
    }
  }

  nsDocShell::Cast(mDocShell)->SetOriginAttributes(attrs);

  // Typically there will be a window, however for some cases such as printing
  // the document is cloned with a docshell that has no window.  We check
  // that the window exists to ensure we don't try to gather ancestors for
  // those cases.
  nsCOMPtr<nsPIDOMWindowOuter> win = doc->GetWindow();
  if (!mDocShell->GetIsMozBrowser() &&
      parentDocShell->ItemType() == mDocShell->ItemType() &&
      !doc->IsStaticDocument() && win) {
    // Propagate through the ancestor principals.
    nsTArray<nsCOMPtr<nsIPrincipal>> ancestorPrincipals;
    // Make a copy, so we can modify it.
    ancestorPrincipals = doc->AncestorPrincipals();
    ancestorPrincipals.InsertElementAt(0, doc->NodePrincipal());
    nsDocShell::Cast(mDocShell)->SetAncestorPrincipals(
        std::move(ancestorPrincipals));

    // Repeat for outer window IDs.
    nsTArray<uint64_t> ancestorOuterWindowIDs;
    ancestorOuterWindowIDs = doc->AncestorOuterWindowIDs();
    ancestorOuterWindowIDs.InsertElementAt(0, win->WindowID());
    nsDocShell::Cast(mDocShell)->SetAncestorOuterWindowIDs(
        std::move(ancestorOuterWindowIDs));
  }

  ReallyLoadFrameScripts();
  InitializeBrowserAPI();

  nsCOMPtr<nsIObserverService> os = services::GetObserverService();
  if (os) {
    os->NotifyObservers(ToSupports(this), "inprocess-browser-shown", nullptr);
  }

  return NS_OK;
}

void nsFrameLoader::GetURL(nsString& aURI,
                           nsIPrincipal** aTriggeringPrincipal) {
  aURI.Truncate();

  if (mOwnerContent->IsHTMLElement(nsGkAtoms::object)) {
    mOwnerContent->GetAttr(kNameSpaceID_None, nsGkAtoms::data, aURI);
    nsCOMPtr<nsIPrincipal> prin = mOwnerContent->NodePrincipal();
    prin.forget(aTriggeringPrincipal);
  } else {
    mOwnerContent->GetAttr(kNameSpaceID_None, nsGkAtoms::src, aURI);
    if (RefPtr<nsGenericHTMLFrameElement> frame =
            do_QueryObject(mOwnerContent)) {
      nsCOMPtr<nsIPrincipal> prin = frame->GetSrcTriggeringPrincipal();
      prin.forget(aTriggeringPrincipal);
    } else {
      nsCOMPtr<nsIPrincipal> prin = mOwnerContent->NodePrincipal();
      prin.forget(aTriggeringPrincipal);
    }
  }
}

nsresult nsFrameLoader::CheckForRecursiveLoad(nsIURI* aURI) {
  nsresult rv;

  MOZ_ASSERT(!IsRemoteFrame(),
             "Shouldn't call CheckForRecursiveLoad on remote frames.");

  mDepthTooGreat = false;
  rv = MaybeCreateDocShell();
  if (NS_FAILED(rv)) {
    return rv;
  }
  NS_ASSERTION(mDocShell, "MaybeCreateDocShell succeeded, but null mDocShell");
  if (!mDocShell) {
    return NS_ERROR_FAILURE;
  }

  // Check that we're still in the docshell tree.
  nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
  mDocShell->GetTreeOwner(getter_AddRefs(treeOwner));
  NS_WARNING_ASSERTION(treeOwner,
                       "Trying to load a new url to a docshell without owner!");
  NS_ENSURE_STATE(treeOwner);

  if (mDocShell->ItemType() != nsIDocShellTreeItem::typeContent) {
    // No need to do recursion-protection here XXXbz why not??  Do we really
    // trust people not to screw up with non-content docshells?
    return NS_OK;
  }

  // Bug 8065: Don't exceed some maximum depth in content frames
  // (MAX_DEPTH_CONTENT_FRAMES)
  nsCOMPtr<nsIDocShellTreeItem> parentAsItem;
  mDocShell->GetSameTypeParent(getter_AddRefs(parentAsItem));
  int32_t depth = 0;
  while (parentAsItem) {
    ++depth;

    if (depth >= MAX_DEPTH_CONTENT_FRAMES) {
      mDepthTooGreat = true;
      NS_WARNING("Too many nested content frames so giving up");

      return NS_ERROR_UNEXPECTED;  // Too deep, give up!  (silently?)
    }

    nsCOMPtr<nsIDocShellTreeItem> temp;
    temp.swap(parentAsItem);
    temp->GetSameTypeParent(getter_AddRefs(parentAsItem));
  }

  // Bug 136580: Check for recursive frame loading excluding about:srcdoc URIs.
  // srcdoc URIs require their contents to be specified inline, so it isn't
  // possible for undesirable recursion to occur without the aid of a
  // non-srcdoc URI,  which this method will block normally.
  // Besides, URI is not enough to guarantee uniqueness of srcdoc documents.
  nsAutoCString buffer;
  rv = aURI->GetScheme(buffer);
  if (NS_SUCCEEDED(rv) && buffer.EqualsLiteral("about")) {
    rv = aURI->GetPathQueryRef(buffer);
    if (NS_SUCCEEDED(rv) && buffer.EqualsLiteral("srcdoc")) {
      // Duplicates allowed up to depth limits
      return NS_OK;
    }
  }
  int32_t matchCount = 0;
  mDocShell->GetSameTypeParent(getter_AddRefs(parentAsItem));
  while (parentAsItem) {
    // Check the parent URI with the URI we're loading
    nsCOMPtr<nsIWebNavigation> parentAsNav(do_QueryInterface(parentAsItem));
    if (parentAsNav) {
      // Does the URI match the one we're about to load?
      nsCOMPtr<nsIURI> parentURI;
      parentAsNav->GetCurrentURI(getter_AddRefs(parentURI));
      if (parentURI) {
        // Bug 98158/193011: We need to ignore data after the #
        bool equal;
        rv = aURI->EqualsExceptRef(parentURI, &equal);
        NS_ENSURE_SUCCESS(rv, rv);

        if (equal) {
          matchCount++;
          if (matchCount >= MAX_SAME_URL_CONTENT_FRAMES) {
            NS_WARNING(
                "Too many nested content frames have the same url (recursion?) "
                "so giving up");
            return NS_ERROR_UNEXPECTED;
          }
        }
      }
    }
    nsCOMPtr<nsIDocShellTreeItem> temp;
    temp.swap(parentAsItem);
    temp->GetSameTypeParent(getter_AddRefs(parentAsItem));
  }

  return NS_OK;
}

nsresult nsFrameLoader::GetWindowDimensions(nsIntRect& aRect) {
  // Need to get outer window position here
  nsIDocument* doc = mOwnerContent->GetComposedDoc();
  if (!doc) {
    return NS_ERROR_FAILURE;
  }

  MOZ_RELEASE_ASSERT(!doc->IsResourceDoc(), "We shouldn't even exist");

  nsCOMPtr<nsPIDOMWindowOuter> win = doc->GetWindow();
  if (!win) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIDocShellTreeItem> parentAsItem(win->GetDocShell());
  if (!parentAsItem) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIDocShellTreeOwner> parentOwner;
  if (NS_FAILED(parentAsItem->GetTreeOwner(getter_AddRefs(parentOwner))) ||
      !parentOwner) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin(do_GetInterface(parentOwner));
  treeOwnerAsWin->GetPosition(&aRect.x, &aRect.y);
  treeOwnerAsWin->GetSize(&aRect.width, &aRect.height);
  return NS_OK;
}

nsresult nsFrameLoader::UpdatePositionAndSize(nsSubDocumentFrame* aIFrame) {
  if (IsRemoteFrame()) {
    if (mRemoteBrowser) {
      ScreenIntSize size = aIFrame->GetSubdocumentSize();
      // If we were not able to show remote frame before, we should probably
      // retry now to send correct showInfo.
      if (!mRemoteBrowserShown) {
        ShowRemoteFrame(size, aIFrame);
      }
      nsIntRect dimensions;
      NS_ENSURE_SUCCESS(GetWindowDimensions(dimensions), NS_ERROR_FAILURE);
      mLazySize = size;
      mRemoteBrowser->UpdateDimensions(dimensions, size);
    }
    return NS_OK;
  }
  UpdateBaseWindowPositionAndSize(aIFrame);
  return NS_OK;
}

void nsFrameLoader::UpdateBaseWindowPositionAndSize(
    nsSubDocumentFrame* aIFrame) {
  nsCOMPtr<nsIBaseWindow> baseWindow =
      do_QueryInterface(GetDocShell(IgnoreErrors()));

  // resize the sub document
  if (baseWindow) {
    int32_t x = 0;
    int32_t y = 0;

    AutoWeakFrame weakFrame(aIFrame);

    baseWindow->GetPosition(&x, &y);

    if (!weakFrame.IsAlive()) {
      // GetPosition() killed us
      return;
    }

    ScreenIntSize size = aIFrame->GetSubdocumentSize();
    mLazySize = size;

    baseWindow->SetPositionAndSize(x, y, size.width, size.height,
                                   nsIBaseWindow::eDelayResize);
  }
}

uint32_t nsFrameLoader::LazyWidth() const {
  uint32_t lazyWidth = mLazySize.width;

  nsIFrame* frame = GetPrimaryFrameOfOwningContent();
  if (frame) {
    lazyWidth = frame->PresContext()->DevPixelsToIntCSSPixels(lazyWidth);
  }

  return lazyWidth;
}

uint32_t nsFrameLoader::LazyHeight() const {
  uint32_t lazyHeight = mLazySize.height;

  nsIFrame* frame = GetPrimaryFrameOfOwningContent();
  if (frame) {
    lazyHeight = frame->PresContext()->DevPixelsToIntCSSPixels(lazyHeight);
  }

  return lazyHeight;
}

void nsFrameLoader::SetClampScrollPosition(bool aClamp) {
  mClampScrollPosition = aClamp;

  // When turning clamping on, make sure the current position is clamped.
  if (aClamp) {
    nsIFrame* frame = GetPrimaryFrameOfOwningContent();
    nsSubDocumentFrame* subdocFrame = do_QueryFrame(frame);
    if (subdocFrame) {
      nsIFrame* subdocRootFrame = subdocFrame->GetSubdocumentRootFrame();
      if (subdocRootFrame) {
        nsIScrollableFrame* subdocRootScrollFrame =
            subdocRootFrame->PresShell()->GetRootScrollFrameAsScrollable();
        if (subdocRootScrollFrame) {
          subdocRootScrollFrame->ScrollTo(
              subdocRootScrollFrame->GetScrollPosition(),
              nsIScrollableFrame::INSTANT);
        }
      }
    }
  }
}

static Tuple<ContentParent*, TabParent*> GetContentParent(Element* aBrowser) {
  using ReturnTuple = Tuple<ContentParent*, TabParent*>;

  nsCOMPtr<nsIBrowser> browser = do_QueryInterface(aBrowser);
  if (!browser) {
    return ReturnTuple(nullptr, nullptr);
  }

  RefPtr<nsFrameLoader> otherLoader;
  browser->GetSameProcessAsFrameLoader(getter_AddRefs(otherLoader));
  if (!otherLoader) {
    return ReturnTuple(nullptr, nullptr);
  }

  TabParent* tabParent = TabParent::GetFrom(otherLoader);
  if (tabParent && tabParent->Manager() &&
      tabParent->Manager()->IsContentParent()) {
    return MakeTuple(tabParent->Manager()->AsContentParent(), tabParent);
  }

  return ReturnTuple(nullptr, nullptr);
}

bool nsFrameLoader::TryRemoteBrowser() {
  NS_ASSERTION(!mRemoteBrowser,
               "TryRemoteBrowser called with a remote browser already?");

  if (!mOwnerContent) {
    return false;
  }

  // XXXsmaug Per spec (2014/08/21) frameloader should not work in case the
  //         element isn't in document, only in shadow dom, but that will change
  //         https://www.w3.org/Bugs/Public/show_bug.cgi?id=26365#c0
  nsIDocument* doc = mOwnerContent->GetComposedDoc();
  if (!doc) {
    return false;
  }

  MOZ_RELEASE_ASSERT(!doc->IsResourceDoc(), "We shouldn't even exist");

  if (!doc->IsActive()) {
    // Don't allow subframe loads in non-active documents.
    // (See bug 610571 comment 5.)
    return false;
  }

  nsCOMPtr<nsPIDOMWindowOuter> parentWin = doc->GetWindow();
  if (!parentWin) {
    return false;
  }

  nsCOMPtr<nsIDocShell> parentDocShell = parentWin->GetDocShell();
  if (!parentDocShell) {
    return false;
  }

  TabParent* openingTab = TabParent::GetFrom(parentDocShell->GetOpener());
  RefPtr<ContentParent> openerContentParent;
  RefPtr<TabParent> sameTabGroupAs;

  if (openingTab && openingTab->Manager() &&
      openingTab->Manager()->IsContentParent()) {
    openerContentParent = openingTab->Manager()->AsContentParent();
  }

  // <iframe mozbrowser> gets to skip these checks.
  // iframes for JS plugins also get to skip these checks. We control the URL
  // that gets loaded, but the load is triggered from the document containing
  // the plugin.
  if (!OwnerIsMozBrowserFrame() && !IsForJSPlugin()) {
    if (parentDocShell->ItemType() != nsIDocShellTreeItem::typeChrome) {
      // Allow about:addon an exception to this rule so it can load remote
      // extension options pages.
      //
      // Note that the new frame's message manager will not be a child of the
      // chrome window message manager, and, the values of window.top and
      // window.parent will be different than they would be for a non-remote
      // frame.
      nsCOMPtr<nsIWebNavigation> parentWebNav;
      nsCOMPtr<nsIURI> aboutAddons;
      nsCOMPtr<nsIURI> parentURI;
      bool equals;
      if (!((parentWebNav = do_GetInterface(parentDocShell)) &&
            NS_SUCCEEDED(
                NS_NewURI(getter_AddRefs(aboutAddons), "about:addons")) &&
            NS_SUCCEEDED(
                parentWebNav->GetCurrentURI(getter_AddRefs(parentURI))) &&
            NS_SUCCEEDED(parentURI->EqualsExceptRef(aboutAddons, &equals)) &&
            equals)) {
        return false;
      }
    }

    if (!mOwnerContent->IsXULElement()) {
      return false;
    }

    if (!mOwnerContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::type,
                                    nsGkAtoms::content, eIgnoreCase)) {
      return false;
    }

    // Try to get the related content parent from our browser element.
    Tie(openerContentParent, sameTabGroupAs) = GetContentParent(mOwnerContent);
  }

  uint32_t chromeFlags = 0;
  nsCOMPtr<nsIDocShellTreeOwner> parentOwner;
  if (NS_FAILED(parentDocShell->GetTreeOwner(getter_AddRefs(parentOwner))) ||
      !parentOwner) {
    return false;
  }
  nsCOMPtr<nsIXULWindow> window(do_GetInterface(parentOwner));
  if (window && NS_FAILED(window->GetChromeFlags(&chromeFlags))) {
    return false;
  }

  AUTO_PROFILER_LABEL("nsFrameLoader::TryRemoteBrowser:Create", OTHER);

  MutableTabContext context;
  nsresult rv = GetNewTabContext(&context);
  NS_ENSURE_SUCCESS(rv, false);

  uint64_t nextTabParentId = 0;
  if (mOwnerContent) {
    nsAutoString nextTabParentIdAttr;
    mOwnerContent->GetAttr(kNameSpaceID_None, nsGkAtoms::nextTabParentId,
                           nextTabParentIdAttr);
    nextTabParentId =
        strtoull(NS_ConvertUTF16toUTF8(nextTabParentIdAttr).get(), nullptr, 10);

    // We may be in a window that was just opened, so try the
    // nsIBrowserDOMWindow API as a backup.
    if (!nextTabParentId && window) {
      Unused << window->GetNextTabParentId(&nextTabParentId);
    }
  }

  nsCOMPtr<Element> ownerElement = mOwnerContent;
  mRemoteBrowser =
      ContentParent::CreateBrowser(context, ownerElement, openerContentParent,
                                   sameTabGroupAs, nextTabParentId);
  if (!mRemoteBrowser) {
    return false;
  }
  // Now that mRemoteBrowser is set, we can initialize the RenderFrame
  mRemoteBrowser->InitRendering();

  MaybeUpdatePrimaryTabParent(eTabParentChanged);

  mChildID = mRemoteBrowser->Manager()->ChildID();

  nsCOMPtr<nsIDocShellTreeItem> rootItem;
  parentDocShell->GetRootTreeItem(getter_AddRefs(rootItem));
  nsCOMPtr<nsPIDOMWindowOuter> rootWin = rootItem->GetWindow();
  nsCOMPtr<nsIDOMChromeWindow> rootChromeWin = do_QueryInterface(rootWin);

  if (rootChromeWin) {
    nsCOMPtr<nsIBrowserDOMWindow> browserDOMWin;
    rootChromeWin->GetBrowserDOMWindow(getter_AddRefs(browserDOMWin));
    mRemoteBrowser->SetBrowserDOMWindow(browserDOMWin);
  }

  // Set up a parent SHistory
  if (XRE_IsParentProcess()) {
    // XXX(nika): Once we get out of process iframes we won't want to
    // unconditionally set this up. What do we do for iframes in a chrome loaded
    // document for example?
    mParentSHistory = new ParentSHistory(this);
  }

  // For xul:browsers, update some settings based on attributes:
  if (mOwnerContent->IsXULElement()) {
    // Send down the name of the browser through mRemoteBrowser if it is set.
    nsAutoString frameName;
    mOwnerContent->GetAttr(kNameSpaceID_None, nsGkAtoms::name, frameName);
    if (nsContentUtils::IsOverridingWindowName(frameName)) {
      Unused << mRemoteBrowser->SendSetWindowName(frameName);
    }
    // Allow scripts to close the window if the browser specified so:
    if (mOwnerContent->AttrValueIs(kNameSpaceID_None,
                                   nsGkAtoms::allowscriptstoclose,
                                   nsGkAtoms::_true, eCaseMatters)) {
      Unused << mRemoteBrowser->SendAllowScriptsToClose();
    }
  }

  ReallyLoadFrameScripts();
  InitializeBrowserAPI();

  return true;
}

mozilla::dom::PBrowserParent* nsFrameLoader::GetRemoteBrowser() const {
  return mRemoteBrowser;
}

RenderFrame* nsFrameLoader::GetCurrentRenderFrame() const {
  if (mRemoteBrowser) {
    return mRemoteBrowser->GetRenderFrame();
  }
  return nullptr;
}

void nsFrameLoader::ActivateRemoteFrame(ErrorResult& aRv) {
  if (!mRemoteBrowser) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  mRemoteBrowser->Activate();
}

void nsFrameLoader::DeactivateRemoteFrame(ErrorResult& aRv) {
  if (!mRemoteBrowser) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  mRemoteBrowser->Deactivate();
}

void nsFrameLoader::SendCrossProcessMouseEvent(const nsAString& aType, float aX,
                                               float aY, int32_t aButton,
                                               int32_t aClickCount,
                                               int32_t aModifiers,
                                               bool aIgnoreRootScrollFrame,
                                               ErrorResult& aRv) {
  if (!mRemoteBrowser) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  mRemoteBrowser->SendMouseEvent(aType, aX, aY, aButton, aClickCount,
                                 aModifiers, aIgnoreRootScrollFrame);
}

void nsFrameLoader::ActivateFrameEvent(const nsAString& aType, bool aCapture,
                                       ErrorResult& aRv) {
  if (!mRemoteBrowser) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  bool ok = mRemoteBrowser->SendActivateFrameEvent(nsString(aType), aCapture);
  if (!ok) {
    aRv.Throw(NS_ERROR_NOT_AVAILABLE);
  }
}

nsresult nsFrameLoader::CreateStaticClone(nsFrameLoader* aDest) {
  aDest->MaybeCreateDocShell();
  NS_ENSURE_STATE(aDest->mDocShell);

  nsCOMPtr<nsIDocument> kungFuDeathGrip = aDest->mDocShell->GetDocument();
  Unused << kungFuDeathGrip;

  nsCOMPtr<nsIContentViewer> viewer;
  aDest->mDocShell->GetContentViewer(getter_AddRefs(viewer));
  NS_ENSURE_STATE(viewer);

  nsIDocShell* origDocShell = GetDocShell(IgnoreErrors());
  NS_ENSURE_STATE(origDocShell);

  nsCOMPtr<nsIDocument> doc = origDocShell->GetDocument();
  NS_ENSURE_STATE(doc);

  nsCOMPtr<nsIDocument> clonedDoc = doc->CreateStaticClone(aDest->mDocShell);

  viewer->SetDocument(clonedDoc);
  return NS_OK;
}

bool nsFrameLoader::DoLoadMessageManagerScript(const nsAString& aURL,
                                               bool aRunInGlobalScope) {
  auto* tabParent = TabParent::GetFrom(GetRemoteBrowser());
  if (tabParent) {
    return tabParent->SendLoadRemoteScript(nsString(aURL), aRunInGlobalScope);
  }
  RefPtr<InProcessTabChildMessageManager> tabChild =
      GetTabChildMessageManager();
  if (tabChild) {
    tabChild->LoadFrameScript(aURL, aRunInGlobalScope);
  }
  return true;
}

class nsAsyncMessageToChild : public nsSameProcessAsyncMessageBase,
                              public Runnable {
 public:
  nsAsyncMessageToChild(JS::RootingContext* aRootingCx,
                        JS::Handle<JSObject*> aCpows,
                        nsFrameLoader* aFrameLoader)
      : nsSameProcessAsyncMessageBase(aRootingCx, aCpows),
        mozilla::Runnable("nsAsyncMessageToChild"),
        mFrameLoader(aFrameLoader) {}

  NS_IMETHOD Run() override {
    InProcessTabChildMessageManager* tabChild =
        mFrameLoader->mChildMessageManager;
    // Since bug 1126089, messages can arrive even when the docShell is
    // destroyed. Here we make sure that those messages are not delivered.
    if (tabChild && tabChild->GetInnerManager() &&
        mFrameLoader->GetExistingDocShell()) {
      JS::Rooted<JSObject*> kungFuDeathGrip(dom::RootingCx(),
                                            tabChild->GetWrapper());
      ReceiveMessage(static_cast<EventTarget*>(tabChild), mFrameLoader,
                     tabChild->GetInnerManager());
    }
    return NS_OK;
  }
  RefPtr<nsFrameLoader> mFrameLoader;
};

nsresult nsFrameLoader::DoSendAsyncMessage(JSContext* aCx,
                                           const nsAString& aMessage,
                                           StructuredCloneData& aData,
                                           JS::Handle<JSObject*> aCpows,
                                           nsIPrincipal* aPrincipal) {
  TabParent* tabParent = mRemoteBrowser;
  if (tabParent) {
    ClonedMessageData data;
    nsIContentParent* cp = tabParent->Manager();
    if (!BuildClonedMessageDataForParent(cp, aData, data)) {
      MOZ_CRASH();
      return NS_ERROR_DOM_DATA_CLONE_ERR;
    }
    InfallibleTArray<mozilla::jsipc::CpowEntry> cpows;
    jsipc::CPOWManager* mgr = cp->GetCPOWManager();
    if (aCpows && (!mgr || !mgr->Wrap(aCx, aCpows, &cpows))) {
      return NS_ERROR_UNEXPECTED;
    }
    if (tabParent->SendAsyncMessage(nsString(aMessage), cpows,
                                    IPC::Principal(aPrincipal), data)) {
      return NS_OK;
    } else {
      return NS_ERROR_UNEXPECTED;
    }
  }

  if (mChildMessageManager) {
    JS::RootingContext* rcx = JS::RootingContext::get(aCx);
    RefPtr<nsAsyncMessageToChild> ev =
        new nsAsyncMessageToChild(rcx, aCpows, this);
    nsresult rv = ev->Init(aMessage, aData, aPrincipal);
    if (NS_FAILED(rv)) {
      return rv;
    }
    rv = NS_DispatchToCurrentThread(ev);
    if (NS_FAILED(rv)) {
      return rv;
    }
    return rv;
  }

  // We don't have any targets to send our asynchronous message to.
  return NS_ERROR_UNEXPECTED;
}

already_AddRefed<MessageSender> nsFrameLoader::GetMessageManager() {
  EnsureMessageManager();
  return do_AddRef(mMessageManager);
}

nsresult nsFrameLoader::EnsureMessageManager() {
  NS_ENSURE_STATE(mOwnerContent);

  if (mMessageManager) {
    return NS_OK;
  }

  if (!mIsTopLevelContent && !OwnerIsMozBrowserFrame() && !IsRemoteFrame() &&
      !(mOwnerContent->IsXULElement() &&
        mOwnerContent->AttrValueIs(kNameSpaceID_None,
                                   nsGkAtoms::forcemessagemanager,
                                   nsGkAtoms::_true, eCaseMatters))) {
    return NS_OK;
  }

  RefPtr<nsGlobalWindowOuter> window =
      nsGlobalWindowOuter::Cast(GetOwnerDoc()->GetWindow());
  RefPtr<ChromeMessageBroadcaster> parentManager;

  if (window && window->IsChromeWindow()) {
    nsAutoString messagemanagergroup;
    if (mOwnerContent->IsXULElement() &&
        mOwnerContent->GetAttr(kNameSpaceID_None,
                               nsGkAtoms::messagemanagergroup,
                               messagemanagergroup)) {
      parentManager = window->GetGroupMessageManager(messagemanagergroup);
    }

    if (!parentManager) {
      parentManager = window->GetMessageManager();
    }
  } else {
    parentManager = nsFrameMessageManager::GetGlobalMessageManager();
  }

  mMessageManager = new ChromeMessageSender(parentManager);
  if (!IsRemoteFrame()) {
    nsresult rv = MaybeCreateDocShell();
    if (NS_FAILED(rv)) {
      return rv;
    }
    NS_ASSERTION(mDocShell,
                 "MaybeCreateDocShell succeeded, but null mDocShell");
    if (!mDocShell) {
      return NS_ERROR_FAILURE;
    }
    mChildMessageManager = InProcessTabChildMessageManager::Create(
        mDocShell, mOwnerContent, mMessageManager);
    NS_ENSURE_TRUE(mChildMessageManager, NS_ERROR_UNEXPECTED);
  }
  return NS_OK;
}

nsresult nsFrameLoader::ReallyLoadFrameScripts() {
  nsresult rv = EnsureMessageManager();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  if (mMessageManager) {
    mMessageManager->InitWithCallback(this);
  }
  return NS_OK;
}

already_AddRefed<Element> nsFrameLoader::GetOwnerElement() {
  return do_AddRef(mOwnerContent);
}

void nsFrameLoader::SetRemoteBrowser(nsITabParent* aTabParent) {
  MOZ_ASSERT(!mRemoteBrowser);
  mRemoteFrame = true;
  mRemoteBrowser = TabParent::GetFrom(aTabParent);
  mChildID = mRemoteBrowser ? mRemoteBrowser->Manager()->ChildID() : 0;
  MaybeUpdatePrimaryTabParent(eTabParentChanged);
  ReallyLoadFrameScripts();
  InitializeBrowserAPI();
  mRemoteBrowser->InitRendering();
  ShowRemoteFrame(ScreenIntSize(0, 0));
}

void nsFrameLoader::SetDetachedSubdocFrame(nsIFrame* aDetachedFrame,
                                           nsIDocument* aContainerDoc) {
  mDetachedSubdocFrame = aDetachedFrame;
  mContainerDocWhileDetached = aContainerDoc;
}

nsIFrame* nsFrameLoader::GetDetachedSubdocFrame(
    nsIDocument** aContainerDoc) const {
  NS_IF_ADDREF(*aContainerDoc = mContainerDocWhileDetached);
  return mDetachedSubdocFrame.GetFrame();
}

void nsFrameLoader::ApplySandboxFlags(uint32_t sandboxFlags) {
  if (mDocShell) {
    uint32_t parentSandboxFlags = mOwnerContent->OwnerDoc()->GetSandboxFlags();

    // The child can only add restrictions, never remove them.
    sandboxFlags |= parentSandboxFlags;

    // If this frame is a receiving browsing context, we should add
    // sandboxed auxiliary navigation flag to sandboxFlags. See
    // https://w3c.github.io/presentation-api/#creating-a-receiving-browsing-context
    nsAutoString presentationURL;
    nsContentUtils::GetPresentationURL(mDocShell, presentationURL);
    if (!presentationURL.IsEmpty()) {
      sandboxFlags |= SANDBOXED_AUXILIARY_NAVIGATION;
    }
    mDocShell->SetSandboxFlags(sandboxFlags);
  }
}

/* virtual */ void nsFrameLoader::AttributeChanged(
    mozilla::dom::Element* aElement, int32_t aNameSpaceID, nsAtom* aAttribute,
    int32_t aModType, const nsAttrValue* aOldValue) {
  MOZ_ASSERT(mObservingOwnerContent);

  if (aNameSpaceID != kNameSpaceID_None ||
      (aAttribute != TypeAttrName() && aAttribute != nsGkAtoms::primary)) {
    return;
  }

  if (aElement != mOwnerContent) {
    return;
  }

  // Note: This logic duplicates a lot of logic in
  // MaybeCreateDocshell.  We should fix that.

  // Notify our enclosing chrome that our type has changed.  We only do this
  // if our parent is chrome, since in all other cases we're random content
  // subframes and the treeowner shouldn't worry about us.
  if (!mDocShell) {
    MaybeUpdatePrimaryTabParent(eTabParentChanged);
    return;
  }

  nsCOMPtr<nsIDocShellTreeItem> parentItem;
  mDocShell->GetParent(getter_AddRefs(parentItem));
  if (!parentItem) {
    return;
  }

  if (parentItem->ItemType() != nsIDocShellTreeItem::typeChrome) {
    return;
  }

  nsCOMPtr<nsIDocShellTreeOwner> parentTreeOwner;
  parentItem->GetTreeOwner(getter_AddRefs(parentTreeOwner));
  if (!parentTreeOwner) {
    return;
  }

  bool is_primary = aElement->AttrValueIs(kNameSpaceID_None, nsGkAtoms::primary,
                                          nsGkAtoms::_true, eIgnoreCase);

#ifdef MOZ_XUL
  // when a content panel is no longer primary, hide any open popups it may have
  if (!is_primary) {
    nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
    if (pm) pm->HidePopupsInDocShell(mDocShell);
  }
#endif

  parentTreeOwner->ContentShellRemoved(mDocShell);
  if (aElement->AttrValueIs(kNameSpaceID_None, TypeAttrName(),
                            nsGkAtoms::content, eIgnoreCase)) {
    parentTreeOwner->ContentShellAdded(mDocShell, is_primary);
  }
}

/**
 * Send the RequestNotifyAfterRemotePaint message to the current Tab.
 */
void nsFrameLoader::RequestNotifyAfterRemotePaint() {
  // If remote browsing (e10s), handle this with the TabParent.
  if (mRemoteBrowser) {
    Unused << mRemoteBrowser->SendRequestNotifyAfterRemotePaint();
  }
}

void nsFrameLoader::RequestUpdatePosition(ErrorResult& aRv) {
  if (auto* tabParent = TabParent::GetFrom(GetRemoteBrowser())) {
    nsresult rv = tabParent->UpdatePosition();

    if (NS_FAILED(rv)) {
      aRv.Throw(rv);
    }
  }
}

void nsFrameLoader::Print(uint64_t aOuterWindowID,
                          nsIPrintSettings* aPrintSettings,
                          nsIWebProgressListener* aProgressListener,
                          ErrorResult& aRv) {
#if defined(NS_PRINTING)
  if (mRemoteBrowser) {
    RefPtr<embedding::PrintingParent> printingParent =
        mRemoteBrowser->Manager()->AsContentParent()->GetPrintingParent();

    embedding::PrintData printData;
    nsresult rv = printingParent->SerializeAndEnsureRemotePrintJob(
        aPrintSettings, aProgressListener, nullptr, &printData);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      aRv.Throw(rv);
      return;
    }

    bool success = mRemoteBrowser->SendPrint(aOuterWindowID, printData);
    if (!success) {
      aRv.Throw(NS_ERROR_FAILURE);
    }
    return;
  }

  nsGlobalWindowOuter* outerWindow =
      nsGlobalWindowOuter::GetOuterWindowWithId(aOuterWindowID);
  if (NS_WARN_IF(!outerWindow)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  nsCOMPtr<nsIWebBrowserPrint> webBrowserPrint =
      do_GetInterface(outerWindow->AsOuter());
  if (NS_WARN_IF(!webBrowserPrint)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  nsresult rv = webBrowserPrint->Print(aPrintSettings, aProgressListener);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return;
  }
#endif
}

already_AddRefed<mozilla::dom::Promise> nsFrameLoader::DrawSnapshot(
    double aX, double aY, double aW, double aH, double aScale,
    const nsAString& aBackgroundColor, mozilla::ErrorResult& aRv) {
  RefPtr<nsIGlobalObject> global = GetOwnerContent()->GetOwnerGlobal();
  RefPtr<Promise> promise = Promise::Create(global, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  RefPtr<nsIDocument> document = GetOwnerContent()->GetOwnerDocument();
  if (NS_WARN_IF(!document)) {
    aRv = NS_ERROR_FAILURE;
    return nullptr;
  }
  nsIPresShell* presShell = document->GetShell();
  if (NS_WARN_IF(!presShell)) {
    aRv = NS_ERROR_FAILURE;
    return nullptr;
  }

  nscolor color;
  css::Loader* loader = document->CSSLoader();
  ServoStyleSet* set = presShell->StyleSet();
  if (NS_WARN_IF(!ServoCSSParser::ComputeColor(
          set, NS_RGB(0, 0, 0), aBackgroundColor, &color, nullptr, loader))) {
    aRv = NS_ERROR_FAILURE;
    return nullptr;
  }

  gfx::IntRect rect = gfx::IntRect::RoundOut(gfx::Rect(aX, aY, aW, aH));

  if (IsRemoteFrame()) {
    gfx::CrossProcessPaint::StartRemote(mRemoteBrowser->GetTabId(), rect,
                                        aScale, color, promise);
  } else {
    gfx::CrossProcessPaint::StartLocal(mDocShell, rect, aScale, color, promise);
  }

  return promise.forget();
}

already_AddRefed<nsITabParent> nsFrameLoader::GetTabParent() {
  return do_AddRef(mRemoteBrowser);
}

already_AddRefed<nsILoadContext> nsFrameLoader::LoadContext() {
  nsCOMPtr<nsILoadContext> loadContext;
  if (IsRemoteFrame() && (mRemoteBrowser || TryRemoteBrowser())) {
    loadContext = mRemoteBrowser->GetLoadContext();
  } else {
    loadContext = do_GetInterface(GetDocShell(IgnoreErrors()));
  }
  return loadContext.forget();
}

void nsFrameLoader::InitializeBrowserAPI() {
  if (!OwnerIsMozBrowserFrame()) {
    return;
  }
  if (!IsRemoteFrame()) {
    nsresult rv = EnsureMessageManager();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }
    if (mMessageManager) {
      mMessageManager->LoadFrameScript(
          NS_LITERAL_STRING("chrome://global/content/BrowserElementChild.js"),
          /* allowDelayedLoad = */ true,
          /* aRunInGlobalScope */ true, IgnoreErrors());
    }
  }
  nsCOMPtr<nsIMozBrowserFrame> browserFrame = do_QueryInterface(mOwnerContent);
  if (browserFrame) {
    browserFrame->InitializeBrowserAPI();
  }
}

void nsFrameLoader::DestroyBrowserFrameScripts() {
  if (!OwnerIsMozBrowserFrame()) {
    return;
  }
  nsCOMPtr<nsIMozBrowserFrame> browserFrame = do_QueryInterface(mOwnerContent);
  if (browserFrame) {
    browserFrame->DestroyBrowserFrameScripts();
  }
}

void nsFrameLoader::StartPersistence(
    uint64_t aOuterWindowID, nsIWebBrowserPersistDocumentReceiver* aRecv,
    ErrorResult& aRv) {
  MOZ_ASSERT(aRecv);

  if (mRemoteBrowser) {
    mRemoteBrowser->StartPersistence(aOuterWindowID, aRecv, aRv);
    return;
  }

  nsCOMPtr<nsIDocument> rootDoc =
      mDocShell ? mDocShell->GetDocument() : nullptr;
  nsCOMPtr<nsIDocument> foundDoc;
  if (aOuterWindowID) {
    foundDoc = nsContentUtils::GetSubdocumentWithOuterWindowId(rootDoc,
                                                               aOuterWindowID);
  } else {
    foundDoc = rootDoc;
  }

  if (!foundDoc) {
    aRecv->OnError(NS_ERROR_NO_CONTENT);
  } else {
    nsCOMPtr<nsIWebBrowserPersistDocument> pdoc =
        new mozilla::WebBrowserPersistLocalDocument(foundDoc);
    aRecv->OnDocumentReady(pdoc);
  }
}

void nsFrameLoader::MaybeUpdatePrimaryTabParent(TabParentChange aChange) {
  if (mRemoteBrowser && mOwnerContent) {
    nsCOMPtr<nsIDocShell> docShell = mOwnerContent->OwnerDoc()->GetDocShell();
    if (!docShell) {
      return;
    }

    int32_t parentType = docShell->ItemType();
    if (parentType != nsIDocShellTreeItem::typeChrome) {
      return;
    }

    nsCOMPtr<nsIDocShellTreeOwner> parentTreeOwner;
    docShell->GetTreeOwner(getter_AddRefs(parentTreeOwner));
    if (!parentTreeOwner) {
      return;
    }

    if (!mObservingOwnerContent) {
      mOwnerContent->AddMutationObserver(this);
      mObservingOwnerContent = true;
    }

    parentTreeOwner->TabParentRemoved(mRemoteBrowser);
    if (aChange == eTabParentChanged) {
      bool isPrimary = mOwnerContent->AttrValueIs(
          kNameSpaceID_None, nsGkAtoms::primary, nsGkAtoms::_true, eIgnoreCase);
      parentTreeOwner->TabParentAdded(mRemoteBrowser, isPrimary);
    }
  }
}

nsresult nsFrameLoader::GetNewTabContext(MutableTabContext* aTabContext,
                                         nsIURI* aURI) {
  if (IsForJSPlugin()) {
    return aTabContext->SetTabContextForJSPluginFrame(mJSPluginID)
               ? NS_OK
               : NS_ERROR_FAILURE;
  }

  OriginAttributes attrs;
  attrs.mInIsolatedMozBrowser = OwnerIsIsolatedMozBrowserFrame();
  nsresult rv;

  attrs.mAppId = nsIScriptSecurityManager::NO_APP_ID;

  // set the userContextId on the attrs before we pass them into
  // the tab context
  rv = PopulateUserContextIdFromAttribute(attrs);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString presentationURLStr;
  mOwnerContent->GetAttr(kNameSpaceID_None, nsGkAtoms::mozpresentation,
                         presentationURLStr);

  nsCOMPtr<nsIDocShell> docShell = mOwnerContent->OwnerDoc()->GetDocShell();
  nsCOMPtr<nsILoadContext> parentContext = do_QueryInterface(docShell);
  NS_ENSURE_STATE(parentContext);

  bool isPrivate = parentContext->UsePrivateBrowsing();
  attrs.SyncAttributesWithPrivateBrowsing(isPrivate);

  UIStateChangeType showAccelerators = UIStateChangeType_NoChange;
  UIStateChangeType showFocusRings = UIStateChangeType_NoChange;
  uint64_t chromeOuterWindowID = 0;

  nsIDocument* doc = mOwnerContent->OwnerDoc();
  if (doc) {
    nsCOMPtr<nsPIWindowRoot> root = nsContentUtils::GetWindowRoot(doc);
    if (root) {
      showAccelerators = root->ShowAccelerators() ? UIStateChangeType_Set
                                                  : UIStateChangeType_Clear;
      showFocusRings = root->ShowFocusRings() ? UIStateChangeType_Set
                                              : UIStateChangeType_Clear;

      nsPIDOMWindowOuter* outerWin = root->GetWindow();
      if (outerWin) {
        chromeOuterWindowID = outerWin->WindowID();
      }
    }
  }

  bool tabContextUpdated = aTabContext->SetTabContext(
      OwnerIsMozBrowserFrame(), chromeOuterWindowID, showAccelerators,
      showFocusRings, attrs, presentationURLStr);
  NS_ENSURE_STATE(tabContextUpdated);

  return NS_OK;
}

nsresult nsFrameLoader::PopulateUserContextIdFromAttribute(
    OriginAttributes& aAttr) {
  if (aAttr.mUserContextId ==
      nsIScriptSecurityManager::DEFAULT_USER_CONTEXT_ID) {
    // Grab the userContextId from owner if XUL or mozbrowser frame
    nsAutoString userContextIdStr;
    int32_t namespaceID = mOwnerContent->GetNameSpaceID();
    if ((namespaceID == kNameSpaceID_XUL || OwnerIsMozBrowserFrame()) &&
        mOwnerContent->GetAttr(kNameSpaceID_None, nsGkAtoms::usercontextid,
                               userContextIdStr) &&
        !userContextIdStr.IsEmpty()) {
      nsresult rv;
      aAttr.mUserContextId = userContextIdStr.ToInteger(&rv);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  return NS_OK;
}

ProcessMessageManager* nsFrameLoader::GetProcessMessageManager() const {
  return mRemoteBrowser ? mRemoteBrowser->Manager()->GetMessageManager()
                        : nullptr;
};

JSObject* nsFrameLoader::WrapObject(JSContext* cx,
                                    JS::Handle<JSObject*> aGivenProto) {
  JS::RootedObject result(cx);
  FrameLoader_Binding::Wrap(cx, this, this, aGivenProto, &result);
  return result;
}
