/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/BrowsingContext.h"

#include "ipc/IPCMessageUtils.h"

#include "mozilla/dom/CanonicalBrowsingContext.h"
#include "mozilla/dom/BrowsingContextGroup.h"
#include "mozilla/dom/BrowsingContextBinding.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/Location.h"
#include "mozilla/dom/LocationBinding.h"
#include "mozilla/dom/PopupBlocker.h"
#include "mozilla/dom/StructuredCloneTags.h"
#include "mozilla/dom/UserActivationIPCUtils.h"
#include "mozilla/dom/WindowBinding.h"
#include "mozilla/dom/WindowGlobalChild.h"
#include "mozilla/dom/WindowGlobalParent.h"
#include "mozilla/dom/WindowProxyHolder.h"
#include "mozilla/Assertions.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Components.h"
#include "mozilla/HashTable.h"
#include "mozilla/Logging.h"
#include "mozilla/StaticPtr.h"
#include "nsIURIFixup.h"

#include "nsDocShell.h"
#include "nsGlobalWindowOuter.h"
#include "nsContentUtils.h"
#include "nsScriptError.h"
#include "nsThreadUtils.h"
#include "xpcprivate.h"

#include "AutoplayPolicy.h"

extern mozilla::LazyLogModule gAutoplayPermissionLog;

#define AUTOPLAY_LOG(msg, ...) \
  MOZ_LOG(gAutoplayPermissionLog, LogLevel::Debug, (msg, ##__VA_ARGS__))

namespace mozilla {
namespace dom {

extern mozilla::LazyLogModule gUserInteractionPRLog;

#define USER_ACTIVATION_LOG(msg, ...) \
  MOZ_LOG(gUserInteractionPRLog, LogLevel::Debug, (msg, ##__VA_ARGS__))

static LazyLogModule gBrowsingContextLog("BrowsingContext");

typedef nsDataHashtable<nsUint64HashKey, BrowsingContext*> BrowsingContextMap;

static StaticAutoPtr<BrowsingContextMap> sBrowsingContexts;

static void Register(BrowsingContext* aBrowsingContext) {
  sBrowsingContexts->Put(aBrowsingContext->Id(), aBrowsingContext);

  aBrowsingContext->Group()->Register(aBrowsingContext);
}

BrowsingContext* BrowsingContext::Top() {
  BrowsingContext* bc = this;
  while (bc->mParent) {
    bc = bc->mParent;
  }
  return bc;
}

/* static */
void BrowsingContext::Init() {
  if (!sBrowsingContexts) {
    sBrowsingContexts = new BrowsingContextMap();
    ClearOnShutdown(&sBrowsingContexts);
  }
}

/* static */
LogModule* BrowsingContext::GetLog() { return gBrowsingContextLog; }

/* static */
already_AddRefed<BrowsingContext> BrowsingContext::Get(uint64_t aId) {
  return do_AddRef(sBrowsingContexts->Get(aId));
}

/* static */
already_AddRefed<BrowsingContext> BrowsingContext::GetFromWindow(
    WindowProxyHolder& aProxy) {
  return do_AddRef(aProxy.get());
}

CanonicalBrowsingContext* BrowsingContext::Canonical() {
  return CanonicalBrowsingContext::Cast(this);
}

/* static */
already_AddRefed<BrowsingContext> BrowsingContext::Create(
    BrowsingContext* aParent, BrowsingContext* aOpener, const nsAString& aName,
    Type aType) {
  MOZ_DIAGNOSTIC_ASSERT(!aParent || aParent->mType == aType);

  MOZ_DIAGNOSTIC_ASSERT(aType != Type::Chrome || XRE_IsParentProcess());

  uint64_t id = nsContentUtils::GenerateBrowsingContextId();

  MOZ_LOG(GetLog(), LogLevel::Debug,
          ("Creating 0x%08" PRIx64 " in %s", id,
           XRE_IsParentProcess() ? "Parent" : "Child"));

  // Determine which BrowsingContextGroup this context should be created in.
  RefPtr<BrowsingContextGroup> group =
      BrowsingContextGroup::Select(aParent, aOpener);

  RefPtr<BrowsingContext> context;
  if (XRE_IsParentProcess()) {
    context = new CanonicalBrowsingContext(aParent, group, id,
                                           /* aProcessId */ 0, aType);
  } else {
    context = new BrowsingContext(aParent, group, id, aType);
  }

  // The name and opener fields need to be explicitly initialized. Don't bother
  // using transactions to set them, as we haven't been attached yet.
  context->mName = aName;
  if (aOpener) {
    MOZ_DIAGNOSTIC_ASSERT(aOpener->Group() == context->Group());
    MOZ_DIAGNOSTIC_ASSERT(aOpener->mType == context->mType);
    context->mOpenerId = aOpener->Id();
    context->mHadOriginalOpener = true;
  }
  context->mEmbedderPolicy = nsILoadInfo::EMBEDDER_POLICY_NULL;

  BrowsingContext* inherit = aParent ? aParent : aOpener;
  if (inherit) {
    context->mOpenerPolicy = inherit->Top()->mOpenerPolicy;
    // CORPP 3.1.3 https://mikewest.github.io/corpp/#integration-html
    context->mEmbedderPolicy = inherit->mEmbedderPolicy;
  }

  Register(context);

  // Attach the browsing context to the tree.
  context->Attach();

  return context.forget();
}

/* static */
already_AddRefed<BrowsingContext> BrowsingContext::CreateFromIPC(
    BrowsingContext::IPCInitializer&& aInit, BrowsingContextGroup* aGroup,
    ContentParent* aOriginProcess) {
  MOZ_DIAGNOSTIC_ASSERT(aOriginProcess || XRE_IsContentProcess());
  MOZ_DIAGNOSTIC_ASSERT(aGroup);

  uint64_t originId = 0;
  if (aOriginProcess) {
    originId = aOriginProcess->ChildID();
    aGroup->EnsureSubscribed(aOriginProcess);
  }

  MOZ_LOG(GetLog(), LogLevel::Debug,
          ("Creating 0x%08" PRIx64 " from IPC (origin=0x%08" PRIx64 ")",
           aInit.mId, originId));

  RefPtr<BrowsingContext> parent = aInit.GetParent();

  RefPtr<BrowsingContext> context;
  if (XRE_IsParentProcess()) {
    context = new CanonicalBrowsingContext(parent, aGroup, aInit.mId, originId,
                                           Type::Content);
  } else {
    context = new BrowsingContext(parent, aGroup, aInit.mId, Type::Content);
  }

  Register(context);

  // Initialize all of our fields from IPC. We don't have to worry about
  // mOpenerId, as we won't try to dereference it immediately.
#define MOZ_BC_FIELD(name, ...) context->m##name = aInit.m##name;
#include "mozilla/dom/BrowsingContextFieldList.h"

  // Caller handles attaching us to the tree.

  return context.forget();
}

BrowsingContext::BrowsingContext(BrowsingContext* aParent,
                                 BrowsingContextGroup* aGroup,
                                 uint64_t aBrowsingContextId, Type aType)
    : mType(aType),
      mBrowsingContextId(aBrowsingContextId),
      mGroup(aGroup),
      mParent(aParent),
      mIsInProcess(false),
      mIsDiscarded(false),
      mDanglingRemoteOuterProxies(false) {
  MOZ_RELEASE_ASSERT(!mParent || mParent->Group() == mGroup);
  MOZ_RELEASE_ASSERT(mBrowsingContextId != 0);
  MOZ_RELEASE_ASSERT(mGroup);
}

void BrowsingContext::SetDocShell(nsIDocShell* aDocShell) {
  // XXX(nika): We should communicate that we are now an active BrowsingContext
  // process to the parent & do other validation here.
  MOZ_RELEASE_ASSERT(aDocShell->GetBrowsingContext() == this);
  mDocShell = aDocShell;
  mDanglingRemoteOuterProxies = !mIsInProcess;
  mIsInProcess = true;
}

// This class implements a callback that will return the remote window proxy for
// mBrowsingContext in that compartment, if it has one. It also removes the
// proxy from the map, because the object will be transplanted into another kind
// of object.
class MOZ_STACK_CLASS CompartmentRemoteProxyTransplantCallback
    : public js::CompartmentTransplantCallback {
 public:
  explicit CompartmentRemoteProxyTransplantCallback(
      BrowsingContext* aBrowsingContext)
      : mBrowsingContext(aBrowsingContext) {}

  virtual JSObject* getObjectToTransplant(
      JS::Compartment* compartment) override {
    auto* priv = xpc::CompartmentPrivate::Get(compartment);
    if (!priv) {
      return nullptr;
    }

    auto& map = priv->GetRemoteProxyMap();
    auto result = map.lookup(mBrowsingContext);
    if (!result) {
      return nullptr;
    }
    JSObject* resultObject = result->value();
    map.remove(result);

    return resultObject;
  }

 private:
  BrowsingContext* mBrowsingContext;
};

void BrowsingContext::CleanUpDanglingRemoteOuterWindowProxies(
    JSContext* aCx, JS::MutableHandle<JSObject*> aOuter) {
  if (!mDanglingRemoteOuterProxies) {
    return;
  }
  mDanglingRemoteOuterProxies = false;

  CompartmentRemoteProxyTransplantCallback cb(this);
  js::RemapRemoteWindowProxies(aCx, &cb, aOuter);
}

void BrowsingContext::SetEmbedderElement(Element* aEmbedder) {
  // Notify the parent process of the embedding status. We don't need to do
  // this when clearing our embedder, as we're being destroyed either way.
  if (aEmbedder) {
    // If our embedder element is being mutated to a different embedder, and we
    // have a parent edge, bad things might be happening!
    //
    // XXX: This is a workaround to some parent edges not being immutable in the
    // parent process. It can be fixed once bug 1539979 has been fixed.
    if (mParent && mEmbedderElement && mEmbedderElement != aEmbedder) {
      NS_WARNING("Non root content frameLoader swap! This will crash soon!");

      MOZ_DIAGNOSTIC_ASSERT(mType == Type::Chrome, "must be chrome");
      MOZ_DIAGNOSTIC_ASSERT(XRE_IsParentProcess(), "must be in parent");
      MOZ_DIAGNOSTIC_ASSERT(!mGroup->IsContextCached(this),
                            "cannot be in bfcache");

      RefPtr<BrowsingContext> kungFuDeathGrip(this);
      RefPtr<BrowsingContext> newParent(
          aEmbedder->OwnerDoc()->GetBrowsingContext());
      mParent->mChildren.RemoveElement(this);
      if (newParent) {
        newParent->mChildren.AppendElement(this);
      }
      mParent = newParent;
    }

    if (nsCOMPtr<nsPIDOMWindowInner> inner =
            do_QueryInterface(aEmbedder->GetOwnerGlobal())) {
      SetEmbedderInnerWindowId(inner->WindowID());
    }
  }

  mEmbedderElement = aEmbedder;
}

void BrowsingContext::Attach(bool aFromIPC) {
  MOZ_LOG(GetLog(), LogLevel::Debug,
          ("%s: Connecting 0x%08" PRIx64 " to 0x%08" PRIx64,
           XRE_IsParentProcess() ? "Parent" : "Child", Id(),
           mParent ? mParent->Id() : 0));

  MOZ_DIAGNOSTIC_ASSERT(mGroup);
  MOZ_DIAGNOSTIC_ASSERT(!mGroup->IsContextCached(this));
  MOZ_DIAGNOSTIC_ASSERT(!mIsDiscarded);

  auto* children = mParent ? &mParent->mChildren : &mGroup->Toplevels();
  MOZ_DIAGNOSTIC_ASSERT(!children->Contains(this));

  children->AppendElement(this);

  if (mIsPopupSpam) {
    PopupBlocker::RegisterOpenPopupSpam();
  }

  if (!aFromIPC) {
    // Send attach to our parent if we need to.
    if (XRE_IsContentProcess()) {
      ContentChild::GetSingleton()->SendAttachBrowsingContext(
          GetIPCInitializer());
    } else if (IsContent()) {
      MOZ_DIAGNOSTIC_ASSERT(XRE_IsParentProcess());
      mGroup->EachParent([&](ContentParent* aParent) {
        Unused << aParent->SendAttachBrowsingContext(GetIPCInitializer());
      });
    }
  }
}

void BrowsingContext::Detach(bool aFromIPC) {
  MOZ_LOG(GetLog(), LogLevel::Debug,
          ("%s: Detaching 0x%08" PRIx64 " from 0x%08" PRIx64,
           XRE_IsParentProcess() ? "Parent" : "Child", Id(),
           mParent ? mParent->Id() : 0));

  // Unlinking might remove our group before Detach gets called.
  if (NS_WARN_IF(!mGroup)) {
    return;
  }

  RefPtr<BrowsingContext> self(this);

  if (!mGroup->EvictCachedContext(this)) {
    Children* children = nullptr;
    if (mParent) {
      children = &mParent->mChildren;
    } else {
      children = &mGroup->Toplevels();
    }

    children->RemoveElement(this);
  }

  if (!mChildren.IsEmpty()) {
    mGroup->CacheContexts(mChildren);
    mChildren.Clear();
  }

  mGroup->Unregister(this);
  mIsDiscarded = true;

  // NOTE: Doesn't use SetClosed, as it will be set in all processes
  // automatically by calls to Detach()
  mClosed = true;

  if (mIsPopupSpam) {
    PopupBlocker::UnregisterOpenPopupSpam();
    // NOTE: Doesn't use SetIsPopupSpam, as it will be set all processes
    // automatically.
    mIsPopupSpam = false;
  }

  if (!aFromIPC && XRE_IsContentProcess()) {
    auto cc = ContentChild::GetSingleton();
    MOZ_DIAGNOSTIC_ASSERT(cc);
    // Tell our parent that the BrowsingContext has been detached. A strong
    // reference to this is held until the promise is resolved to ensure it
    // doesn't die before the parent receives the message.
    auto resolve = [self](bool) {};
    auto reject = [self](mozilla::ipc::ResponseRejectReason) {};
    cc->SendDetachBrowsingContext(Id(), resolve, reject);
  }
}

void BrowsingContext::PrepareForProcessChange() {
  MOZ_LOG(GetLog(), LogLevel::Debug,
          ("%s: Preparing 0x%08" PRIx64 " for a process change",
           XRE_IsParentProcess() ? "Parent" : "Child", Id()));

  MOZ_ASSERT(mIsInProcess, "Must currently be an in-process frame");
  MOZ_ASSERT(!mIsDiscarded, "We're already closed?");

  mIsInProcess = false;
  mUserGestureStart = TimeStamp();

  // NOTE: For now, clear our nsDocShell reference, as we're primarily in a
  // different process now. This may need to change in the future with
  // Cross-Process BFCache.
  mDocShell = nullptr;

  if (!mWindowProxy) {
    return;
  }

  // We have to go through mWindowProxy rather than calling GetDOMWindow() on
  // mDocShell because the mDocshell reference gets cleared immediately after
  // the window is closed.
  nsGlobalWindowOuter::PrepareForProcessChange(mWindowProxy);
  MOZ_ASSERT(!mWindowProxy);
}

void BrowsingContext::CacheChildren(bool aFromIPC) {
  MOZ_LOG(GetLog(), LogLevel::Debug,
          ("%s: Caching children of 0x%08" PRIx64 "",
           XRE_IsParentProcess() ? "Parent" : "Child", Id()));

  mGroup->CacheContexts(mChildren);
  mChildren.Clear();

  if (!aFromIPC && XRE_IsContentProcess()) {
    auto cc = ContentChild::GetSingleton();
    MOZ_DIAGNOSTIC_ASSERT(cc);
    cc->SendCacheBrowsingContextChildren(this);
  }
}

void BrowsingContext::RestoreChildren(Children&& aChildren, bool aFromIPC) {
  MOZ_LOG(GetLog(), LogLevel::Debug,
          ("%s: Restoring children of 0x%08" PRIx64 "",
           XRE_IsParentProcess() ? "Parent" : "Child", Id()));

  for (BrowsingContext* child : aChildren) {
    MOZ_DIAGNOSTIC_ASSERT(child->GetParent() == this);
    Unused << mGroup->EvictCachedContext(child);
  }

  mChildren.AppendElements(aChildren);

  if (!aFromIPC && XRE_IsContentProcess()) {
    auto cc = ContentChild::GetSingleton();
    MOZ_DIAGNOSTIC_ASSERT(cc);
    cc->SendRestoreBrowsingContextChildren(this, aChildren);
  }
}

bool BrowsingContext::IsCached() { return mGroup->IsContextCached(this); }

bool BrowsingContext::IsTargetable() {
  return !mClosed && !mIsDiscarded && !IsCached();
}

bool BrowsingContext::HasOpener() const {
  return sBrowsingContexts->Contains(mOpenerId);
}

void BrowsingContext::GetChildren(Children& aChildren) {
  MOZ_ALWAYS_TRUE(aChildren.AppendElements(mChildren));
}

// FindWithName follows the rules for choosing a browsing context,
// with the exception of sandboxing for iframes. The implementation
// for arbitrarily choosing between two browsing contexts with the
// same name is as follows:
//
// 1) The start browsing context, i.e. 'this'
// 2) Descendants in insertion order
// 3) The parent
// 4) Siblings and their children, both in insertion order
// 5) After this we iteratively follow the parent chain, repeating 3
//    and 4 until
// 6) If there is no parent, consider all other top level browsing
//    contexts and their children, both in insertion order
//
// See
// https://html.spec.whatwg.org/multipage/browsers.html#the-rules-for-choosing-a-browsing-context-given-a-browsing-context-name
BrowsingContext* BrowsingContext::FindWithName(
    const nsAString& aName, BrowsingContext& aRequestingContext) {
  BrowsingContext* found = nullptr;
  if (aName.IsEmpty()) {
    // You can't find a browsing context with an empty name.
    found = nullptr;
  } else if (aName.LowerCaseEqualsLiteral("_blank")) {
    // Just return null. Caller must handle creating a new window with
    // a blank name.
    found = nullptr;
  } else if (IsSpecialName(aName)) {
    found = FindWithSpecialName(aName, aRequestingContext);
  } else if (BrowsingContext* child =
                 FindWithNameInSubtree(aName, aRequestingContext)) {
    found = child;
  } else {
    BrowsingContext* current = this;

    do {
      Children* siblings;
      BrowsingContext* parent = current->mParent;

      if (!parent) {
        // We've reached the root of the tree, consider browsing
        // contexts in the same browsing context group.
        siblings = &mGroup->Toplevels();
      } else if (parent->NameEquals(aName) &&
                 aRequestingContext.CanAccess(parent) &&
                 parent->IsTargetable()) {
        found = parent;
        break;
      } else {
        siblings = &parent->mChildren;
      }

      for (BrowsingContext* sibling : *siblings) {
        if (sibling == current) {
          continue;
        }

        if (BrowsingContext* relative =
                sibling->FindWithNameInSubtree(aName, aRequestingContext)) {
          found = relative;
          // Breaks the outer loop
          parent = nullptr;
          break;
        }
      }

      current = parent;
    } while (current);
  }

  // Helpers should perform access control checks, which means that we
  // only need to assert that we can access found.
  MOZ_DIAGNOSTIC_ASSERT(!found || aRequestingContext.CanAccess(found));

  return found;
}

BrowsingContext* BrowsingContext::FindChildWithName(
    const nsAString& aName, BrowsingContext& aRequestingContext) {
  if (aName.IsEmpty()) {
    // You can't find a browsing context with the empty name.
    return nullptr;
  }

  for (BrowsingContext* child : mChildren) {
    if (child->NameEquals(aName) && aRequestingContext.CanAccess(child) &&
        child->IsTargetable()) {
      return child;
    }
  }

  return nullptr;
}

/* static */
bool BrowsingContext::IsSpecialName(const nsAString& aName) {
  return (aName.LowerCaseEqualsLiteral("_self") ||
          aName.LowerCaseEqualsLiteral("_parent") ||
          aName.LowerCaseEqualsLiteral("_top") ||
          aName.LowerCaseEqualsLiteral("_blank"));
}

BrowsingContext* BrowsingContext::FindWithSpecialName(
    const nsAString& aName, BrowsingContext& aRequestingContext) {
  // TODO(farre): Neither BrowsingContext nor nsDocShell checks if the
  // browsing context pointed to by a special name is active. Should
  // it be? See Bug 1527913.
  if (aName.LowerCaseEqualsLiteral("_self")) {
    return this;
  }

  if (aName.LowerCaseEqualsLiteral("_parent")) {
    if (mParent) {
      return aRequestingContext.CanAccess(mParent) ? mParent.get() : nullptr;
    }
    return this;
  }

  if (aName.LowerCaseEqualsLiteral("_top")) {
    BrowsingContext* top = Top();

    return aRequestingContext.CanAccess(top) ? top : nullptr;
  }

  return nullptr;
}

BrowsingContext* BrowsingContext::FindWithNameInSubtree(
    const nsAString& aName, BrowsingContext& aRequestingContext) {
  MOZ_DIAGNOSTIC_ASSERT(!aName.IsEmpty());

  if (NameEquals(aName) && aRequestingContext.CanAccess(this) &&
      IsTargetable()) {
    return this;
  }

  for (BrowsingContext* child : mChildren) {
    if (BrowsingContext* found =
            child->FindWithNameInSubtree(aName, aRequestingContext)) {
      return found;
    }
  }

  return nullptr;
}

// For historical context, see:
//
// Bug 13871:   Prevent frameset spoofing
// Bug 103638:  Targets with same name in different windows open in wrong
//              window with javascript
// Bug 408052:  Adopt "ancestor" frame navigation policy
// Bug 1570207: Refactor logic to rely on BrowsingContextGroups to enforce
//              origin attribute isolation.
bool BrowsingContext::CanAccess(BrowsingContext* aTarget,
                                bool aConsiderOpener) {
  MOZ_ASSERT(
      mDocShell,
      "CanAccess() may only be called in the process of the accessing window");
  MOZ_ASSERT(aTarget, "Must have a target");

  MOZ_DIAGNOSTIC_ASSERT(
      Group() == aTarget->Group(),
      "A BrowsingContext should never see a context from a different group");

  // A frame can navigate itself and its own root.
  if (aTarget == this || aTarget == Top()) {
    return true;
  }

  // A frame can navigate any frame with a same-origin ancestor.
  for (BrowsingContext* bc = aTarget; bc; bc = bc->GetParent()) {
    if (bc->mDocShell && nsDocShell::ValidateOrigin(mDocShell, bc->mDocShell)) {
      return true;
    }
  }

  // If the target is a top-level document, a frame can navigate it if it can
  // navigate its opener.
  if (aConsiderOpener && !aTarget->GetParent()) {
    if (RefPtr<BrowsingContext> opener = aTarget->GetOpener()) {
      return CanAccess(opener, false);
    }
  }

  return false;
}

BrowsingContext::~BrowsingContext() {
  MOZ_DIAGNOSTIC_ASSERT(!mParent || !mParent->mChildren.Contains(this));
  MOZ_DIAGNOSTIC_ASSERT(!mGroup || !mGroup->Toplevels().Contains(this));
  MOZ_DIAGNOSTIC_ASSERT(!mGroup || !mGroup->IsContextCached(this));

  if (sBrowsingContexts) {
    sBrowsingContexts->Remove(Id());
  }
}

nsISupports* BrowsingContext::GetParentObject() const {
  return xpc::NativeGlobal(xpc::PrivilegedJunkScope());
}

JSObject* BrowsingContext::WrapObject(JSContext* aCx,
                                      JS::Handle<JSObject*> aGivenProto) {
  return BrowsingContext_Binding::Wrap(aCx, this, aGivenProto);
}

bool BrowsingContext::WriteStructuredClone(JSContext* aCx,
                                           JSStructuredCloneWriter* aWriter,
                                           StructuredCloneHolder* aHolder) {
  return (JS_WriteUint32Pair(aWriter, SCTAG_DOM_BROWSING_CONTEXT, 0) &&
          JS_WriteUint32Pair(aWriter, uint32_t(Id()), uint32_t(Id() >> 32)));
}

/* static */
JSObject* BrowsingContext::ReadStructuredClone(JSContext* aCx,
                                               JSStructuredCloneReader* aReader,
                                               StructuredCloneHolder* aHolder) {
  uint32_t idLow = 0;
  uint32_t idHigh = 0;
  if (!JS_ReadUint32Pair(aReader, &idLow, &idHigh)) {
    return nullptr;
  }
  uint64_t id = uint64_t(idHigh) << 32 | idLow;

  // Note: Do this check after reading our ID data. Returning null will abort
  // the decode operation anyway, but we should at least be as safe as possible.
  if (NS_WARN_IF(!NS_IsMainThread())) {
    MOZ_DIAGNOSTIC_ASSERT(false,
                          "We shouldn't be trying to decode a BrowsingContext "
                          "on a background thread.");
    return nullptr;
  }

  JS::RootedValue val(aCx, JS::NullValue());
  // We'll get rooting hazard errors from the RefPtr destructor if it isn't
  // destroyed before we try to return a raw JSObject*, so create it in its own
  // scope.
  if (RefPtr<BrowsingContext> context = Get(id)) {
    if (!GetOrCreateDOMReflector(aCx, context, &val) || !val.isObject()) {
      return nullptr;
    }
  }
  return val.toObjectOrNull();
}

void BrowsingContext::NotifyUserGestureActivation() {
  SetUserActivationState(UserActivation::State::FullActivated);
}

void BrowsingContext::NotifyResetUserGestureActivation() {
  SetUserActivationState(UserActivation::State::None);
}

bool BrowsingContext::HasBeenUserGestureActivated() {
  return mUserActivationState != UserActivation::State::None;
}

bool BrowsingContext::HasValidTransientUserGestureActivation() {
  MOZ_ASSERT(mIsInProcess);

  if (mUserActivationState != UserActivation::State::FullActivated) {
    MOZ_ASSERT(mUserGestureStart.IsNull(),
               "mUserGestureStart should be null if the document hasn't ever "
               "been activated by user gesture");
    return false;
  }

  MOZ_ASSERT(!mUserGestureStart.IsNull(),
             "mUserGestureStart shouldn't be null if the document has ever "
             "been activated by user gesture");
  TimeDuration timeout = TimeDuration::FromMilliseconds(
      StaticPrefs::dom_user_activation_transient_timeout());

  return timeout <= TimeDuration() ||
         (TimeStamp::Now() - mUserGestureStart) <= timeout;
}

bool BrowsingContext::ConsumeTransientUserGestureActivation() {
  MOZ_ASSERT(mIsInProcess);

  if (!HasValidTransientUserGestureActivation()) {
    return false;
  }

  BrowsingContext* top = Top();
  top->PreOrderWalk([&](BrowsingContext* aContext) {
    if (aContext->GetUserActivationState() ==
        UserActivation::State::FullActivated) {
      aContext->SetUserActivationState(UserActivation::State::HasBeenActivated);
    }
  });

  return true;
}

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(BrowsingContext)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_CLASS(BrowsingContext)

NS_IMPL_CYCLE_COLLECTING_ADDREF(BrowsingContext)
NS_IMPL_CYCLE_COLLECTING_RELEASE(BrowsingContext)

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(BrowsingContext)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(BrowsingContext)
  if (sBrowsingContexts) {
    sBrowsingContexts->Remove(tmp->Id());
  }

  if (tmp->mIsPopupSpam) {
    PopupBlocker::UnregisterOpenPopupSpam();
    // NOTE: Doesn't use SetIsPopupSpam, as it will be set all processes
    // automatically.
    tmp->mIsPopupSpam = false;
  }

  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDocShell, mChildren, mParent, mGroup,
                                  mEmbedderElement)
  if (XRE_IsParentProcess()) {
    CanonicalBrowsingContext::Cast(tmp)->Unlink();
  }
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(BrowsingContext)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDocShell, mChildren, mParent, mGroup,
                                    mEmbedderElement)
  if (XRE_IsParentProcess()) {
    CanonicalBrowsingContext::Cast(tmp)->Traverse(cb);
  }
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

class RemoteLocationProxy
    : public RemoteObjectProxy<BrowsingContext::LocationProxy,
                               Location_Binding::sCrossOriginAttributes,
                               Location_Binding::sCrossOriginMethods> {
 public:
  typedef RemoteObjectProxy Base;

  constexpr RemoteLocationProxy()
      : RemoteObjectProxy(prototypes::id::Location) {}

  void NoteChildren(JSObject* aProxy,
                    nsCycleCollectionTraversalCallback& aCb) const override {
    auto location =
        static_cast<BrowsingContext::LocationProxy*>(GetNative(aProxy));
    CycleCollectionNoteChild(aCb, location->GetBrowsingContext(),
                             "js::GetObjectPrivate(obj)->GetBrowsingContext()");
  }
};

static const RemoteLocationProxy sSingleton;

// Give RemoteLocationProxy 2 reserved slots, like the other wrappers,
// so JSObject::swap can swap it with CrossCompartmentWrappers without requiring
// malloc.
template <>
const JSClass RemoteLocationProxy::Base::sClass =
    PROXY_CLASS_DEF("Proxy", JSCLASS_HAS_RESERVED_SLOTS(2));

void BrowsingContext::Location(JSContext* aCx,
                               JS::MutableHandle<JSObject*> aLocation,
                               ErrorResult& aError) {
  aError.MightThrowJSException();
  sSingleton.GetProxyObject(aCx, &mLocation, /* aTransplantTo = */ nullptr,
                            aLocation);
  if (!aLocation) {
    aError.StealExceptionFromJSContext(aCx);
  }
}

void BrowsingContext::LoadURI(const nsAString& aURI,
                              const LoadURIOptions& aOptions,
                              ErrorResult& aError) {
  nsCOMPtr<nsIURIFixup> uriFixup = components::URIFixup::Service();

  nsCOMPtr<nsISupports> consumer = mDocShell.get();
  if (!consumer) {
    consumer = mEmbedderElement;
  }
  if (!consumer) {
    aError.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  RefPtr<nsDocShellLoadState> loadState;
  nsresult rv = nsDocShellLoadState::CreateFromLoadURIOptions(
      consumer, uriFixup, aURI, aOptions, getter_AddRefs(loadState));

  if (rv == NS_ERROR_MALFORMED_URI) {
    DisplayLoadError(aURI);
    return;
  }

  if (NS_FAILED(rv)) {
    aError.Throw(rv);
    return;
  }

  LoadURI(nullptr, loadState, true);
}

nsresult BrowsingContext::LoadURI(BrowsingContext* aAccessor,
                                  nsDocShellLoadState* aLoadState,
                                  bool aSetNavigating) {
  // Per spec, most load attempts are silently ignored when a BrowsingContext is
  // null (which in our code corresponds to discarded), so we simply fail
  // silently in those cases. Regardless, we cannot trigger loads in/from
  // discarded BrowsingContexts via IPC, so we need to abort in any case.
  if (IsDiscarded() || (aAccessor && aAccessor->IsDiscarded())) {
    return NS_OK;
  }

  if (mDocShell) {
    return mDocShell->LoadURI(aLoadState, aSetNavigating);
  }

  if (!aAccessor && XRE_IsParentProcess()) {
    Unused << Canonical()->GetCurrentWindowGlobal()->SendLoadURIInChild(
        aLoadState, aSetNavigating);
  } else {
    MOZ_DIAGNOSTIC_ASSERT(aAccessor);
    MOZ_DIAGNOSTIC_ASSERT(aAccessor->Group() == Group());

    if (!aAccessor->CanAccess(this)) {
      return NS_ERROR_DOM_PROP_ACCESS_DENIED;
    }

    nsCOMPtr<nsPIDOMWindowOuter> win(aAccessor->GetDOMWindow());
    MOZ_DIAGNOSTIC_ASSERT(win);
    if (WindowGlobalChild* wgc =
            win->GetCurrentInnerWindow()->GetWindowGlobalChild()) {
      wgc->SendLoadURI(this, aLoadState, aSetNavigating);
    }
  }
  return NS_OK;
}

void BrowsingContext::DisplayLoadError(const nsAString& aURI) {
  MOZ_LOG(GetLog(), LogLevel::Debug, ("DisplayLoadError"));
  MOZ_DIAGNOSTIC_ASSERT(!IsDiscarded());
  MOZ_DIAGNOSTIC_ASSERT(mDocShell || XRE_IsParentProcess());

  if (mDocShell) {
    bool didDisplayLoadError = false;
    mDocShell->DisplayLoadError(NS_ERROR_MALFORMED_URI, nullptr,
                                PromiseFlatString(aURI).get(), nullptr,
                                &didDisplayLoadError);
  } else {
    Unused << Canonical()->GetCurrentWindowGlobal()->SendDisplayLoadError(
        PromiseFlatString(aURI));
  }
}

WindowProxyHolder BrowsingContext::Window() {
  return WindowProxyHolder(Self());
}

WindowProxyHolder BrowsingContext::GetFrames(ErrorResult& aError) {
  return Window();
}

void BrowsingContext::Close(CallerType aCallerType, ErrorResult& aError) {
  // FIXME We need to set mClosed, but only once we're sending the
  //       DOMWindowClose event (which happens in the process where the
  //       document for this browsing context is loaded).
  //       See https://bugzilla.mozilla.org/show_bug.cgi?id=1516343.
  if (ContentChild* cc = ContentChild::GetSingleton()) {
    cc->SendWindowClose(this, aCallerType == CallerType::System);
  } else if (ContentParent* cp = Canonical()->GetContentParent()) {
    Unused << cp->SendWindowClose(this, aCallerType == CallerType::System);
  }
}

void BrowsingContext::Focus(ErrorResult& aError) {
  if (ContentChild* cc = ContentChild::GetSingleton()) {
    cc->SendWindowFocus(this);
  } else if (ContentParent* cp = Canonical()->GetContentParent()) {
    Unused << cp->SendWindowFocus(this);
  }
}

void BrowsingContext::Blur(ErrorResult& aError) {
  if (ContentChild* cc = ContentChild::GetSingleton()) {
    cc->SendWindowBlur(this);
  } else if (ContentParent* cp = Canonical()->GetContentParent()) {
    Unused << cp->SendWindowBlur(this);
  }
}

Nullable<WindowProxyHolder> BrowsingContext::GetWindow() {
  if (XRE_IsParentProcess() && !IsInProcess()) {
    return nullptr;
  }
  return WindowProxyHolder(this);
}

Nullable<WindowProxyHolder> BrowsingContext::GetTop(ErrorResult& aError) {
  if (mIsDiscarded) {
    return nullptr;
  }

  // We never return null or throw an error, but the implementation in
  // nsGlobalWindow does and we need to use the same signature.
  return WindowProxyHolder(Top());
}

void BrowsingContext::GetOpener(JSContext* aCx,
                                JS::MutableHandle<JS::Value> aOpener,
                                ErrorResult& aError) const {
  RefPtr<BrowsingContext> opener = GetOpener();
  if (!opener) {
    aOpener.setNull();
    return;
  }

  if (!ToJSValue(aCx, WindowProxyHolder(opener), aOpener)) {
    aError.NoteJSContextException(aCx);
  }
}

Nullable<WindowProxyHolder> BrowsingContext::GetParent(ErrorResult& aError) {
  if (mIsDiscarded) {
    return nullptr;
  }

  // We never throw an error, but the implementation in nsGlobalWindow does and
  // we need to use the same signature.
  if (!mParent) {
    return WindowProxyHolder(this);
  }
  return WindowProxyHolder(mParent.get());
}

void BrowsingContext::PostMessageMoz(JSContext* aCx,
                                     JS::Handle<JS::Value> aMessage,
                                     const nsAString& aTargetOrigin,
                                     const Sequence<JSObject*>& aTransfer,
                                     nsIPrincipal& aSubjectPrincipal,
                                     ErrorResult& aError) {
  if (mIsDiscarded) {
    return;
  }

  RefPtr<BrowsingContext> sourceBc;
  PostMessageData data;
  data.targetOrigin() = aTargetOrigin;
  data.subjectPrincipal() = &aSubjectPrincipal;
  RefPtr<nsGlobalWindowInner> callerInnerWindow;
  // We don't need to get the caller's agentClusterId since that is used for
  // checking whehter it's okay to sharing memory (and it's not allowed to share
  // memory cross processes)
  if (!nsGlobalWindowOuter::GatherPostMessageData(
          aCx, aTargetOrigin, getter_AddRefs(sourceBc), data.origin(),
          getter_AddRefs(data.targetOriginURI()),
          getter_AddRefs(data.callerPrincipal()),
          getter_AddRefs(callerInnerWindow),
          getter_AddRefs(data.callerDocumentURI()),
          /* aCallerAgentClusterId */ nullptr, aError)) {
    return;
  }
  data.source() = sourceBc;
  data.isFromPrivateWindow() =
      callerInnerWindow &&
      nsScriptErrorBase::ComputeIsFromPrivateWindow(callerInnerWindow);

  JS::Rooted<JS::Value> transferArray(aCx);
  aError = nsContentUtils::CreateJSValueFromSequenceOfObject(aCx, aTransfer,
                                                             &transferArray);
  if (NS_WARN_IF(aError.Failed())) {
    return;
  }

  ipc::StructuredCloneData message;
  message.Write(aCx, aMessage, transferArray, aError);
  if (NS_WARN_IF(aError.Failed())) {
    return;
  }

  ClonedMessageData messageData;
  if (ContentChild* cc = ContentChild::GetSingleton()) {
    if (!message.BuildClonedMessageDataForChild(cc, messageData)) {
      aError.Throw(NS_ERROR_FAILURE);
      return;
    }

    cc->SendWindowPostMessage(this, messageData, data);
  } else if (ContentParent* cp = Canonical()->GetContentParent()) {
    if (!message.BuildClonedMessageDataForParent(cp, messageData)) {
      aError.Throw(NS_ERROR_FAILURE);
      return;
    }

    Unused << cp->SendWindowPostMessage(this, messageData, data);
  }
}

void BrowsingContext::PostMessageMoz(JSContext* aCx,
                                     JS::Handle<JS::Value> aMessage,
                                     const WindowPostMessageOptions& aOptions,
                                     nsIPrincipal& aSubjectPrincipal,
                                     ErrorResult& aError) {
  PostMessageMoz(aCx, aMessage, aOptions.mTargetOrigin, aOptions.mTransfer,
                 aSubjectPrincipal, aError);
}

nsresult BrowsingContext::Transaction::Commit(
    BrowsingContext* aBrowsingContext) {
  if (NS_WARN_IF(aBrowsingContext->IsDiscarded())) {
    return NS_ERROR_FAILURE;
  }

  if (!Validate(aBrowsingContext, nullptr)) {
    MOZ_CRASH("Cannot commit invalid BrowsingContext transaction");
  }

  if (XRE_IsContentProcess()) {
    ContentChild* cc = ContentChild::GetSingleton();

    // Increment the field epoch for fields affected by this transaction. We
    // only need to do this in content.
    uint64_t epoch = cc->NextBrowsingContextFieldEpoch();
#define MOZ_BC_FIELD(name, ...)             \
  if (m##name) {                            \
    aBrowsingContext->mEpochs.name = epoch; \
  }
#include "mozilla/dom/BrowsingContextFieldList.h"

    cc->SendCommitBrowsingContextTransaction(aBrowsingContext, *this, epoch);
  } else {
    MOZ_DIAGNOSTIC_ASSERT(XRE_IsParentProcess());

    aBrowsingContext->Group()->EachParent([&](ContentParent* aParent) {
      Unused << aParent->SendCommitBrowsingContextTransaction(
          aBrowsingContext, *this, aParent->GetBrowsingContextFieldEpoch());
    });
  }

  Apply(aBrowsingContext);
  return NS_OK;
}

bool BrowsingContext::Transaction::Validate(BrowsingContext* aBrowsingContext,
                                            ContentParent* aSource) {
#define MOZ_BC_FIELD(name, ...)                                        \
  if (m##name && !aBrowsingContext->MaySet##name(*m##name, aSource)) { \
    NS_WARNING("Invalid attempt to set BC field " #name);              \
    return false;                                                      \
  }
#include "mozilla/dom/BrowsingContextFieldList.h"

  mValidated = true;
  return true;
}

bool BrowsingContext::Transaction::ValidateEpochs(
    BrowsingContext* aBrowsingContext, uint64_t aEpoch) {
  MOZ_DIAGNOSTIC_ASSERT(XRE_IsContentProcess(),
                        "Should only be called in content process");

  // Clear fields which are obsoleted by the epoch.
#define MOZ_BC_FIELD(name, ...)                             \
  if (m##name && aBrowsingContext->mEpochs.name > aEpoch) { \
    m##name.reset();                                        \
  }
#include "mozilla/dom/BrowsingContextFieldList.h"

  // NOTE: We don't call MaySet in a content process for messages sent from over
  // IPC. The message has already been validated in both the original sending
  // process (to get good errors), and in the parent process (to enforce trust).
  mValidated = true;
  return true;
}

void BrowsingContext::Transaction::Apply(BrowsingContext* aBrowsingContext) {
  MOZ_RELEASE_ASSERT(mValidated,
                     "Must validate BrowsingContext Transaction before Apply");

#define MOZ_BC_FIELD(name, ...)                      \
  if (m##name) {                                     \
    aBrowsingContext->m##name = std::move(*m##name); \
    aBrowsingContext->DidSet##name();                \
    m##name.reset();                                 \
  }
#include "mozilla/dom/BrowsingContextFieldList.h"
}

BrowsingContext::IPCInitializer BrowsingContext::GetIPCInitializer() {
  MOZ_DIAGNOSTIC_ASSERT(mType == Type::Content);

  IPCInitializer init;
  init.mId = Id();
  init.mParentId = mParent ? mParent->Id() : 0;
  init.mCached = IsCached();

#define MOZ_BC_FIELD(name, type) init.m##name = m##name;
#include "mozilla/dom/BrowsingContextFieldList.h"
  return init;
}

already_AddRefed<BrowsingContext> BrowsingContext::IPCInitializer::GetParent() {
  RefPtr<BrowsingContext> parent;
  if (mParentId != 0) {
    parent = BrowsingContext::Get(mParentId);
    MOZ_RELEASE_ASSERT(parent);
  }
  return parent.forget();
}

already_AddRefed<BrowsingContext> BrowsingContext::IPCInitializer::GetOpener() {
  RefPtr<BrowsingContext> opener;
  if (mOpenerId != 0) {
    opener = BrowsingContext::Get(mOpenerId);
    MOZ_RELEASE_ASSERT(opener);
  }
  return opener.forget();
}

void BrowsingContext::StartDelayedAutoplayMediaComponents() {
  if (!mDocShell) {
    return;
  }
  AUTOPLAY_LOG("%s : StartDelayedAutoplayMediaComponents for bc 0x%08" PRIx64,
               XRE_IsParentProcess() ? "Parent" : "Child", Id());
  mDocShell->StartDelayedAutoplayMediaComponents();
}

void BrowsingContext::DidSetUserActivationState() {
  MOZ_ASSERT_IF(!mIsInProcess, mUserGestureStart.IsNull());
  USER_ACTIVATION_LOG("Set user gesture activation %" PRIu8
                      " for %s browsing context 0x%08" PRIx64,
                      static_cast<uint8_t>(mUserActivationState),
                      XRE_IsParentProcess() ? "Parent" : "Child", Id());
  if (mIsInProcess) {
    USER_ACTIVATION_LOG(
        "Set user gesture start time for %s browsing context 0x%08" PRIx64,
        XRE_IsParentProcess() ? "Parent" : "Child", Id());
    mUserGestureStart =
        (mUserActivationState == UserActivation::State::FullActivated)
            ? TimeStamp::Now()
            : TimeStamp();
  }
}

void BrowsingContext::DidSetMuted() {
  MOZ_ASSERT(!mParent, "Set muted flag on non top-level context!");
  USER_ACTIVATION_LOG("Set audio muted %d for %s browsing context 0x%08" PRIx64,
                      mMuted, XRE_IsParentProcess() ? "Parent" : "Child", Id());
  PreOrderWalk([&](BrowsingContext* aContext) {
    nsPIDOMWindowOuter* win = aContext->GetDOMWindow();
    if (win) {
      win->RefreshMediaElementsVolume();
    }
  });
}

bool BrowsingContext::MaySetEmbedderInnerWindowId(const uint64_t& aValue,
                                                  ContentParent* aSource) {
  // Generally allow clearing this. We may want to be more precise about this
  // check in the future.
  if (aValue == 0) {
    return true;
  }

  // If we don't have a specified source, we're the setting process. The window
  // which we're setting this to must be in-process.
  RefPtr<BrowsingContext> impliedParent;
  if (!aSource) {
    nsGlobalWindowInner* innerWindow =
        nsGlobalWindowInner::GetInnerWindowWithId(aValue);
    if (NS_WARN_IF(!innerWindow)) {
      return false;
    }

    impliedParent = innerWindow->GetBrowsingContext();
  }

  // If in the parent process, double-check ownership and WindowGlobalParent as
  // well.
  if (XRE_IsParentProcess()) {
    RefPtr<WindowGlobalParent> wgp =
        WindowGlobalParent::GetByInnerWindowId(aValue);
    if (NS_WARN_IF(!wgp)) {
      return false;
    }

    // Deduce the implied parent from the WindowGlobalParent actor.
    if (impliedParent) {
      MOZ_ASSERT(impliedParent == wgp->BrowsingContext());
    }
    impliedParent = wgp->BrowsingContext();

    // Double-check ownership if we aren't the setter.
    if (aSource &&
        !impliedParent->Canonical()->IsOwnedByProcess(aSource->ChildID()) &&
        aSource->ChildID() !=
            impliedParent->Canonical()->GetInFlightProcessId()) {
      return false;
    }
  }

  // If we would have an invalid implied parent, something has gone wrong.
  MOZ_ASSERT(impliedParent);
  if (NS_WARN_IF(mParent && mParent != impliedParent)) {
    return false;
  }

  return true;
}

bool BrowsingContext::MaySetIsPopupSpam(const bool& aValue,
                                        ContentParent* aSource) {
  // Ensure that we only mark a browsing context as popup spam once and never
  // unmark it.
  return aValue && !mIsPopupSpam;
}

void BrowsingContext::DidSetIsPopupSpam() {
  if (mIsPopupSpam) {
    PopupBlocker::RegisterOpenPopupSpam();
  }
}

}  // namespace dom

namespace ipc {

void IPDLParamTraits<dom::BrowsingContext*>::Write(
    IPC::Message* aMsg, IProtocol* aActor, dom::BrowsingContext* aParam) {
  uint64_t id = aParam ? aParam->Id() : 0;
  WriteIPDLParam(aMsg, aActor, id);
  if (!aParam) {
    return;
  }

  // Make sure that the other side will still have our BrowsingContext around
  // when it tries to perform deserialization.
  if (aActor->GetIPCChannel()->IsCrossProcess()) {
    // If we're sending the message between processes, we only know the other
    // side will still have a copy if we've not been discarded yet. As
    // serialization cannot fail softly, fail loudly by crashing.
    MOZ_RELEASE_ASSERT(
        !aParam->IsDiscarded(),
        "Cannot send discarded BrowsingContext between processes!");
  } else {
    // If we're in-process, we can take an extra reference to ensure it lives
    // long enough to make it to the other side. This reference is freed in
    // `::Read()`.
    aParam->AddRef();
  }
}

bool IPDLParamTraits<dom::BrowsingContext*>::Read(
    const IPC::Message* aMsg, PickleIterator* aIter, IProtocol* aActor,
    RefPtr<dom::BrowsingContext>* aResult) {
  uint64_t id = 0;
  if (!ReadIPDLParam(aMsg, aIter, aActor, &id)) {
    return false;
  }

  if (id == 0) {
    *aResult = nullptr;
    return true;
  }

  RefPtr<dom::BrowsingContext> browsingContext = dom::BrowsingContext::Get(id);
  if (!browsingContext) {
    // NOTE: We could fail softly by returning `false` if the `BrowsingContext`
    // isn't present, but doing so will cause a crash anyway. Let's improve
    // diagnostics by reliably crashing here.
    //
    // If we can recover from failures to deserialize in the future, this crash
    // should be removed or modified.
    MOZ_CRASH("Attempt to deserialize absent BrowsingContext");
    *aResult = nullptr;
    return false;
  }

  if (!aActor->GetIPCChannel()->IsCrossProcess()) {
    // Release the reference taken in `::Write()` for in-process actors.
    browsingContext.get()->Release();
  }

  *aResult = browsingContext.forget();
  return true;
}

void IPDLParamTraits<dom::BrowsingContext::Transaction>::Write(
    IPC::Message* aMessage, IProtocol* aActor,
    const dom::BrowsingContext::Transaction& aTransaction) {
  MOZ_RELEASE_ASSERT(
      aTransaction.mValidated,
      "Must validate BrowsingContext Transaction before sending");

#define MOZ_BC_FIELD(name, ...) \
  WriteIPDLParam(aMessage, aActor, aTransaction.m##name);
#include "mozilla/dom/BrowsingContextFieldList.h"
}

bool IPDLParamTraits<dom::BrowsingContext::Transaction>::Read(
    const IPC::Message* aMessage, PickleIterator* aIterator, IProtocol* aActor,
    dom::BrowsingContext::Transaction* aTransaction) {
  aTransaction->mValidated = false;

#define MOZ_BC_FIELD(name, ...)                                              \
  if (!ReadIPDLParam(aMessage, aIterator, aActor, &aTransaction->m##name)) { \
    return false;                                                            \
  }
#include "mozilla/dom/BrowsingContextFieldList.h"

  return true;
}

void IPDLParamTraits<dom::BrowsingContext::IPCInitializer>::Write(
    IPC::Message* aMessage, IProtocol* aActor,
    const dom::BrowsingContext::IPCInitializer& aInit) {
  // Write actor ID parameters.
  WriteIPDLParam(aMessage, aActor, aInit.mId);
  WriteIPDLParam(aMessage, aActor, aInit.mParentId);

  WriteIPDLParam(aMessage, aActor, aInit.mCached);

  // Write other synchronized fields.
#define MOZ_BC_FIELD(name, ...) WriteIPDLParam(aMessage, aActor, aInit.m##name);
#include "mozilla/dom/BrowsingContextFieldList.h"
}

bool IPDLParamTraits<dom::BrowsingContext::IPCInitializer>::Read(
    const IPC::Message* aMessage, PickleIterator* aIterator, IProtocol* aActor,
    dom::BrowsingContext::IPCInitializer* aInit) {
  // Read actor ID parameters.
  if (!ReadIPDLParam(aMessage, aIterator, aActor, &aInit->mId) ||
      !ReadIPDLParam(aMessage, aIterator, aActor, &aInit->mParentId)) {
    return false;
  }

  if (!ReadIPDLParam(aMessage, aIterator, aActor, &aInit->mCached)) {
    return false;
  }

  // Read other synchronized fields.
#define MOZ_BC_FIELD(name, ...)                                       \
  if (!ReadIPDLParam(aMessage, aIterator, aActor, &aInit->m##name)) { \
    return false;                                                     \
  }
#include "mozilla/dom/BrowsingContextFieldList.h"

  return true;
}

}  // namespace ipc
}  // namespace mozilla
