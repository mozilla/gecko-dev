/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CanonicalBrowsingContext.h"

#include "mozilla/dom/BrowsingContextGroup.h"
#include "mozilla/dom/WindowGlobalParent.h"
#include "mozilla/dom/ContentProcessManager.h"
#include "mozilla/ipc/ProtocolUtils.h"
#include "mozilla/NullPrincipal.h"

using namespace mozilla::ipc;

extern mozilla::LazyLogModule gAutoplayPermissionLog;

#define AUTOPLAY_LOG(msg, ...) \
  MOZ_LOG(gAutoplayPermissionLog, LogLevel::Debug, (msg, ##__VA_ARGS__))

namespace mozilla {
namespace dom {

extern mozilla::LazyLogModule gUserInteractionPRLog;

#define USER_ACTIVATION_LOG(msg, ...) \
  MOZ_LOG(gUserInteractionPRLog, LogLevel::Debug, (msg, ##__VA_ARGS__))

CanonicalBrowsingContext::CanonicalBrowsingContext(BrowsingContext* aParent,
                                                   BrowsingContextGroup* aGroup,
                                                   uint64_t aBrowsingContextId,
                                                   uint64_t aProcessId,
                                                   BrowsingContext::Type aType)
    : BrowsingContext(aParent, aGroup, aBrowsingContextId, aType),
      mProcessId(aProcessId) {
  // You are only ever allowed to create CanonicalBrowsingContexts in the
  // parent process.
  MOZ_RELEASE_ASSERT(XRE_IsParentProcess());
}

/* static */
already_AddRefed<CanonicalBrowsingContext> CanonicalBrowsingContext::Get(
    uint64_t aId) {
  MOZ_RELEASE_ASSERT(XRE_IsParentProcess());
  return BrowsingContext::Get(aId).downcast<CanonicalBrowsingContext>();
}

/* static */
CanonicalBrowsingContext* CanonicalBrowsingContext::Cast(
    BrowsingContext* aContext) {
  MOZ_RELEASE_ASSERT(XRE_IsParentProcess());
  return static_cast<CanonicalBrowsingContext*>(aContext);
}

/* static */
const CanonicalBrowsingContext* CanonicalBrowsingContext::Cast(
    const BrowsingContext* aContext) {
  MOZ_RELEASE_ASSERT(XRE_IsParentProcess());
  return static_cast<const CanonicalBrowsingContext*>(aContext);
}

ContentParent* CanonicalBrowsingContext::GetContentParent() const {
  if (mProcessId == 0) {
    return nullptr;
  }

  ContentProcessManager* cpm = ContentProcessManager::GetSingleton();
  return cpm->GetContentProcessById(ContentParentId(mProcessId));
}

void CanonicalBrowsingContext::GetCurrentRemoteType(nsAString& aRemoteType,
                                                    ErrorResult& aRv) const {
  // If we're in the parent process, dump out the void string.
  if (mProcessId == 0) {
    aRemoteType.Assign(VoidString());
    return;
  }

  ContentParent* cp = GetContentParent();
  if (!cp) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  aRemoteType.Assign(cp->GetRemoteType());
}

void CanonicalBrowsingContext::SetOwnerProcessId(uint64_t aProcessId) {
  MOZ_LOG(GetLog(), LogLevel::Debug,
          ("SetOwnerProcessId for 0x%08" PRIx64 " (0x%08" PRIx64
           " -> 0x%08" PRIx64 ")",
           Id(), mProcessId, aProcessId));

  mProcessId = aProcessId;
}

void CanonicalBrowsingContext::SetInFlightProcessId(uint64_t aProcessId) {
  // We can't handle more than one in-flight process change at a time.
  MOZ_ASSERT_IF(aProcessId, mInFlightProcessId == 0);

  mInFlightProcessId = aProcessId;
}

void CanonicalBrowsingContext::GetWindowGlobals(
    nsTArray<RefPtr<WindowGlobalParent>>& aWindows) {
  aWindows.SetCapacity(mWindowGlobals.Count());
  for (auto iter = mWindowGlobals.Iter(); !iter.Done(); iter.Next()) {
    aWindows.AppendElement(iter.Get()->GetKey());
  }
}

void CanonicalBrowsingContext::RegisterWindowGlobal(
    WindowGlobalParent* aGlobal) {
  MOZ_ASSERT(!mWindowGlobals.Contains(aGlobal), "Global already registered!");
  mWindowGlobals.PutEntry(aGlobal);
}

void CanonicalBrowsingContext::UnregisterWindowGlobal(
    WindowGlobalParent* aGlobal) {
  MOZ_ASSERT(mWindowGlobals.Contains(aGlobal), "Global not registered!");
  mWindowGlobals.RemoveEntry(aGlobal);

  // Our current window global should be in our mWindowGlobals set. If it's not
  // anymore, clear that reference.
  if (aGlobal == mCurrentWindowGlobal) {
    mCurrentWindowGlobal = nullptr;
  }
}

void CanonicalBrowsingContext::SetCurrentWindowGlobal(
    WindowGlobalParent* aGlobal) {
  MOZ_ASSERT(mWindowGlobals.Contains(aGlobal), "Global not registered!");

  // TODO: This should probably assert that the processes match.
  mCurrentWindowGlobal = aGlobal;
}

already_AddRefed<WindowGlobalParent>
CanonicalBrowsingContext::GetEmbedderWindowGlobal() const {
  uint64_t windowId = GetEmbedderInnerWindowId();
  if (windowId == 0) {
    return nullptr;
  }

  return WindowGlobalParent::GetByInnerWindowId(windowId);
}

JSObject* CanonicalBrowsingContext::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return CanonicalBrowsingContext_Binding::Wrap(aCx, this, aGivenProto);
}

void CanonicalBrowsingContext::Traverse(
    nsCycleCollectionTraversalCallback& cb) {
  CanonicalBrowsingContext* tmp = this;
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mWindowGlobals, mCurrentWindowGlobal);
}

void CanonicalBrowsingContext::Unlink() {
  CanonicalBrowsingContext* tmp = this;
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mWindowGlobals, mCurrentWindowGlobal);
}

void CanonicalBrowsingContext::NotifyStartDelayedAutoplayMedia() {
  if (!mCurrentWindowGlobal) {
    return;
  }

  // As this function would only be called when user click the play icon on the
  // tab bar. That's clear user intent to play, so gesture activate the browsing
  // context so that the block-autoplay logic allows the media to autoplay.
  NotifyUserGestureActivation();
  AUTOPLAY_LOG("NotifyStartDelayedAutoplayMedia for chrome bc 0x%08" PRIx64,
               Id());
  StartDelayedAutoplayMediaComponents();
  // Notfiy all content browsing contexts which are related with the canonical
  // browsing content tree to start delayed autoplay media.

  Group()->EachParent([&](ContentParent* aParent) {
    Unused << aParent->SendStartDelayedAutoplayMediaComponents(this);
  });
}

void CanonicalBrowsingContext::NotifyMediaMutedChanged(bool aMuted) {
  MOZ_ASSERT(!GetParent(),
             "Notify media mute change on non top-level context!");
  SetMuted(aMuted);
}

void CanonicalBrowsingContext::UpdateMediaAction(MediaControlActions aAction) {
  nsPIDOMWindowOuter* window = GetDOMWindow();
  if (window) {
    window->UpdateMediaAction(aAction);
  }
  Group()->EachParent([&](ContentParent* aParent) {
    Unused << aParent->SendUpdateMediaAction(this, aAction);
  });
}

namespace {

using NewOrUsedPromise = MozPromise<RefPtr<ContentParent>, nsresult, false>;

// NOTE: This method is currently a dummy, and always actually spawns sync. It
// mostly exists so I can test out the async API right now.
RefPtr<NewOrUsedPromise> GetNewOrUsedBrowserProcessAsync(
    const nsAString& aRemoteType) {
  RefPtr<ContentParent> contentParent =
      ContentParent::GetNewOrUsedBrowserProcess(
          nullptr, aRemoteType, hal::PROCESS_PRIORITY_FOREGROUND, nullptr,
          false);
  if (!contentParent) {
    return NewOrUsedPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }
  return NewOrUsedPromise::CreateAndResolve(contentParent, __func__);
}

}  // anonymous namespace

void CanonicalBrowsingContext::PendingRemotenessChange::Complete(
    ContentParent* aContentParent) {
  if (!mPromise) {
    return;
  }

  RefPtr<CanonicalBrowsingContext> target(mTarget);
  RefPtr<WindowGlobalParent> embedderWindow = target->GetEmbedderWindowGlobal();
  if (NS_WARN_IF(!embedderWindow) || NS_WARN_IF(!embedderWindow->CanSend())) {
    Cancel(NS_ERROR_FAILURE);
    return;
  }

  RefPtr<BrowserParent> embedderBrowser = embedderWindow->GetBrowserParent();
  if (NS_WARN_IF(!embedderBrowser)) {
    Cancel(NS_ERROR_FAILURE);
    return;
  }

  // Pull load flags from our embedder browser.
  nsCOMPtr<nsILoadContext> loadContext = embedderBrowser->GetLoadContext();
  MOZ_DIAGNOSTIC_ASSERT(
      loadContext->UseRemoteTabs() && loadContext->UseRemoteSubframes(),
      "Not supported without fission");

  // NOTE: These are the only flags we actually care about
  uint32_t chromeFlags = nsIWebBrowserChrome::CHROME_REMOTE_WINDOW |
                         nsIWebBrowserChrome::CHROME_FISSION_WINDOW;
  if (loadContext->UsePrivateBrowsing()) {
    chromeFlags |= nsIWebBrowserChrome::CHROME_PRIVATE_WINDOW;
  }

  TabId tabId(nsContentUtils::GenerateTabId());
  RefPtr<BrowserBridgeParent> bridge = new BrowserBridgeParent();
  ManagedEndpoint<PBrowserBridgeChild> endpoint =
      embedderBrowser->OpenPBrowserBridgeEndpoint(bridge);
  if (NS_WARN_IF(!endpoint.IsValid())) {
    Cancel(NS_ERROR_UNEXPECTED);
    return;
  }

  RefPtr<WindowGlobalParent> oldWindow = target->mCurrentWindowGlobal;
  RefPtr<BrowserParent> oldBrowser =
      oldWindow ? oldWindow->GetBrowserParent() : nullptr;
  bool wasRemote = oldWindow && oldWindow->IsProcessRoot();

  // Update which process is considered the current owner
  uint64_t inFlightProcessId = target->OwnerProcessId();
  target->SetInFlightProcessId(inFlightProcessId);
  target->SetOwnerProcessId(aContentParent->ChildID());

  auto resetInFlightId = [target, inFlightProcessId] {
    if (target->GetInFlightProcessId() == inFlightProcessId) {
      target->SetInFlightProcessId(0);
    } else {
      MOZ_DIAGNOSTIC_ASSERT(false, "Unexpected InFlightProcessId");
    }
  };

  // If we were in a remote frame, trigger unloading of the remote window. When
  // the original remote window acknowledges, we can clear the in-flight ID.
  if (wasRemote) {
    MOZ_DIAGNOSTIC_ASSERT(oldBrowser);
    MOZ_DIAGNOSTIC_ASSERT(oldBrowser != embedderBrowser);
    MOZ_DIAGNOSTIC_ASSERT(oldBrowser->GetBrowserBridgeParent());

    oldBrowser->SendSkipBrowsingContextDetach(
        [resetInFlightId](bool aSuccess) { resetInFlightId(); },
        [resetInFlightId](mozilla::ipc::ResponseRejectReason aReason) {
          resetInFlightId();
        });
    oldBrowser->Destroy();
  }

  // Tell the embedder process a remoteness change is in-process. When this is
  // acknowledged, reset the in-flight ID if it used to be an in-process load.
  embedderWindow->SendMakeFrameRemote(
      target, std::move(endpoint), tabId,
      [wasRemote, resetInFlightId](bool aSuccess) {
        if (!wasRemote) {
          resetInFlightId();
        }
      },
      [wasRemote, resetInFlightId](mozilla::ipc::ResponseRejectReason aReason) {
        if (!wasRemote) {
          resetInFlightId();
        }
      });

  // FIXME: We should get the correct principal for the to-be-created window so
  // we can avoid creating unnecessary extra windows in the new process.
  nsCOMPtr<nsIPrincipal> initialPrincipal =
      NullPrincipal::CreateWithInheritedAttributes(
          embedderBrowser->OriginAttributesRef(),
          /* isFirstParty */ false);
  WindowGlobalInit windowInit =
      WindowGlobalActor::AboutBlankInitializer(target, initialPrincipal);

  // Actually create the new BrowserParent actor and finish initialization of
  // our new BrowserBridgeParent.
  nsresult rv = bridge->InitWithProcess(aContentParent, EmptyString(),
                                        windowInit, chromeFlags, tabId);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    Cancel(rv);
    return;
  }

  RefPtr<BrowserParent> newBrowser = bridge->GetBrowserParent();
  newBrowser->ResumeLoad(mPendingSwitchId);

  // We did it! The process switch is complete.
  mPromise->Resolve(newBrowser, __func__);
  Clear();
}

void CanonicalBrowsingContext::PendingRemotenessChange::Cancel(nsresult aRv) {
  if (!mPromise) {
    return;
  }

  mPromise->Reject(aRv, __func__);
  Clear();
}

void CanonicalBrowsingContext::PendingRemotenessChange::Clear() {
  // Make sure we don't die while we're doing cleanup.
  RefPtr<PendingRemotenessChange> kungFuDeathGrip(this);
  if (mTarget) {
    MOZ_DIAGNOSTIC_ASSERT(mTarget->mPendingRemotenessChange == this);
    mTarget->mPendingRemotenessChange = nullptr;
  }

  mPromise = nullptr;
  mTarget = nullptr;
}

CanonicalBrowsingContext::PendingRemotenessChange::~PendingRemotenessChange() {
  MOZ_ASSERT(!mPromise && !mTarget,
             "should've already been Cancel() or Complete()-ed");
}

RefPtr<CanonicalBrowsingContext::RemotenessPromise>
CanonicalBrowsingContext::ChangeFrameRemoteness(const nsAString& aRemoteType,
                                                uint64_t aPendingSwitchId) {
  // Ensure our embedder hasn't been destroyed already.
  RefPtr<WindowGlobalParent> embedderWindowGlobal = GetEmbedderWindowGlobal();
  if (!embedderWindowGlobal) {
    NS_WARNING("Non-embedded BrowsingContext");
    return RemotenessPromise::CreateAndReject(NS_ERROR_UNEXPECTED, __func__);
  }

  if (!embedderWindowGlobal->CanSend()) {
    NS_WARNING("Embedder already been destroyed.");
    return RemotenessPromise::CreateAndReject(NS_ERROR_NOT_AVAILABLE, __func__);
  }

  RefPtr<ContentParent> oldContent = GetContentParent();
  if (!oldContent || aRemoteType.IsEmpty()) {
    NS_WARNING("Cannot switch to or from non-remote frame");
    return RemotenessPromise::CreateAndReject(NS_ERROR_NOT_IMPLEMENTED,
                                              __func__);
  }

  if (aRemoteType.Equals(oldContent->GetRemoteType())) {
    NS_WARNING("Already in the correct process");
    return RemotenessPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }

  // Cancel ongoing remoteness changes.
  if (mPendingRemotenessChange) {
    mPendingRemotenessChange->Cancel(NS_ERROR_ABORT);
    MOZ_ASSERT(!mPendingRemotenessChange, "Should have cleared");
  }

  RefPtr<BrowserParent> embedderBrowser =
      embedderWindowGlobal->GetBrowserParent();
  MOZ_ASSERT(embedderBrowser);

  // Switching to local. No new process, so perform switch sync.
  if (aRemoteType.Equals(embedderBrowser->Manager()->GetRemoteType())) {
    if (mCurrentWindowGlobal) {
      MOZ_DIAGNOSTIC_ASSERT(mCurrentWindowGlobal->IsProcessRoot());
      RefPtr<BrowserParent> oldBrowser =
          mCurrentWindowGlobal->GetBrowserParent();

      RefPtr<CanonicalBrowsingContext> target(this);
      SetInFlightProcessId(OwnerProcessId());
      oldBrowser->SendSkipBrowsingContextDetach(
          [target](bool aSuccess) { target->SetInFlightProcessId(0); },
          [target](mozilla::ipc::ResponseRejectReason aReason) {
            target->SetInFlightProcessId(0);
          });
      oldBrowser->Destroy();
    }

    SetOwnerProcessId(embedderBrowser->Manager()->ChildID());
    Unused << embedderWindowGlobal->SendMakeFrameLocal(this, aPendingSwitchId);
    return RemotenessPromise::CreateAndResolve(embedderBrowser, __func__);
  }

  // Switching to remote. Wait for new process to launch before switch.
  auto promise = MakeRefPtr<RemotenessPromise::Private>(__func__);
  RefPtr<PendingRemotenessChange> change =
      new PendingRemotenessChange(this, promise, aPendingSwitchId);
  mPendingRemotenessChange = change;

  GetNewOrUsedBrowserProcessAsync(aRemoteType)
      ->Then(
          GetMainThreadSerialEventTarget(), __func__,
          [change](ContentParent* aContentParent) {
            change->Complete(aContentParent);
          },
          [change](nsresult aRv) { change->Cancel(aRv); });
  return promise.forget();
}

already_AddRefed<Promise> CanonicalBrowsingContext::ChangeFrameRemoteness(
    const nsAString& aRemoteType, uint64_t aPendingSwitchId, ErrorResult& aRv) {
  nsIGlobalObject* global = xpc::NativeGlobal(xpc::PrivilegedJunkScope());

  RefPtr<Promise> promise = Promise::Create(global, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  ChangeFrameRemoteness(aRemoteType, aPendingSwitchId)
      ->Then(
          GetMainThreadSerialEventTarget(), __func__,
          [promise](BrowserParent* aBrowserParent) {
            promise->MaybeResolve(aBrowserParent->Manager()->ChildID());
          },
          [promise](nsresult aRv) { promise->MaybeReject(aRv); });
  return promise.forget();
}

}  // namespace dom
}  // namespace mozilla
