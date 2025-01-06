/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsClipboardProxy.h"

#if defined(ACCESSIBILITY) && defined(XP_WIN)
#  include "mozilla/a11y/Compatibility.h"
#endif
#include "mozilla/ClipboardContentAnalysisChild.h"
#include "mozilla/ClipboardReadRequestChild.h"
#include "mozilla/ClipboardWriteRequestChild.h"
#include "mozilla/Components.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/net/CookieJarSettings.h"
#include "mozilla/Maybe.h"
#include "mozilla/dom/WindowGlobalChild.h"
#include "mozilla/Unused.h"
#include "mozilla/SpinEventLoopUntil.h"
#include "nsArrayUtils.h"
#include "nsBaseClipboard.h"
#include "nsIContentAnalysis.h"
#include "nsISupportsPrimitives.h"
#include "nsCOMPtr.h"
#include "nsComponentManagerUtils.h"
#include "nsXULAppAPI.h"
#include "nsContentUtils.h"
#include "PermissionMessageUtils.h"

using mozilla::ipc::ResponseRejectReason;
using namespace mozilla;
using namespace mozilla::dom;

NS_IMPL_ISUPPORTS(nsClipboardProxy, nsIClipboard, nsIClipboardProxy)

nsClipboardProxy::nsClipboardProxy() : mClipboardCaps(false, false, false) {}

NS_IMETHODIMP
nsClipboardProxy::SetData(nsITransferable* aTransferable,
                          nsIClipboardOwner* anOwner,
                          nsIClipboard::ClipboardType aWhichClipboard,
                          mozilla::dom::WindowContext* aWindowContext) {
#if defined(ACCESSIBILITY) && defined(XP_WIN)
  a11y::Compatibility::SuppressA11yForClipboardCopy();
#endif

  ContentChild* child = ContentChild::GetSingleton();
  IPCTransferable ipcTransferable;
  nsContentUtils::TransferableToIPCTransferable(aTransferable, &ipcTransferable,
                                                false, nullptr);
  child->SendSetClipboard(std::move(ipcTransferable), aWhichClipboard,
                          aWindowContext);
  return NS_OK;
}

NS_IMETHODIMP nsClipboardProxy::AsyncSetData(
    nsIClipboard::ClipboardType aWhichClipboard,
    mozilla::dom::WindowContext* aSettingWindowContext,
    nsIAsyncClipboardRequestCallback* aCallback,
    nsIAsyncSetClipboardData** _retval) {
  RefPtr<ClipboardWriteRequestChild> request =
      MakeRefPtr<ClipboardWriteRequestChild>(aCallback);
  ContentChild::GetSingleton()->SendPClipboardWriteRequestConstructor(
      request, aWhichClipboard, aSettingWindowContext);
  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
nsClipboardProxy::GetData(nsITransferable* aTransferable,
                          nsIClipboard::ClipboardType aWhichClipboard,
                          mozilla::dom::WindowContext* aWindowContext) {
  MOZ_DIAGNOSTIC_ASSERT(aWindowContext && aWindowContext->IsInProcess(),
                        "content clipboard reads must be associated with an "
                        "in-process WindowContext");
  if (aWindowContext->IsDiscarded()) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  nsTArray<nsCString> types;
  aTransferable->FlavorsTransferableCanImport(types);

  IPCTransferableDataOrError transferableOrError;
  if (MOZ_UNLIKELY(nsIContentAnalysis::MightBeActive())) {
    RefPtr<ClipboardContentAnalysisChild> contentAnalysis =
        ClipboardContentAnalysisChild::GetOrCreate();
    if (!contentAnalysis) {
      return NS_ERROR_FAILURE;
    }
    if (!contentAnalysis->SendGetClipboard(types, aWhichClipboard,
                                           aWindowContext->InnerWindowId(),
                                           &transferableOrError)) {
      return NS_ERROR_FAILURE;
    }
  } else {
    if (!ContentChild::GetSingleton()->SendGetClipboard(
            types, aWhichClipboard, aWindowContext, &transferableOrError)) {
      return NS_ERROR_FAILURE;
    };
  }

  if (transferableOrError.type() == IPCTransferableDataOrError::Tnsresult) {
    MOZ_ASSERT(NS_FAILED(transferableOrError.get_nsresult()));
    return transferableOrError.get_nsresult();
  }

  return nsContentUtils::IPCTransferableDataToTransferable(
      transferableOrError.get_IPCTransferableData(), false /* aAddDataFlavor */,
      aTransferable, false /* aFilterUnknownFlavors */);
}

namespace {

class ClipboardDataSnapshotProxy final : public nsIClipboardDataSnapshot {
 public:
  explicit ClipboardDataSnapshotProxy(ClipboardReadRequestChild* aActor)
      : mActor(aActor) {
    MOZ_ASSERT(mActor);
  }

  NS_DECL_ISUPPORTS
  NS_DECL_NSICLIPBOARDDATASNAPSHOT

 private:
  virtual ~ClipboardDataSnapshotProxy() {
    MOZ_ASSERT(mActor);
    if (mActor->CanSend()) {
      PClipboardReadRequestChild::Send__delete__(mActor);
    }
  };

  RefPtr<ClipboardReadRequestChild> mActor;
};

NS_IMPL_ISUPPORTS(ClipboardDataSnapshotProxy, nsIClipboardDataSnapshot)

NS_IMETHODIMP ClipboardDataSnapshotProxy::GetValid(bool* aOutResult) {
  MOZ_ASSERT(mActor);
  *aOutResult = mActor->CanSend();
  return NS_OK;
}

NS_IMETHODIMP ClipboardDataSnapshotProxy::GetFlavorList(
    nsTArray<nsCString>& aFlavorList) {
  MOZ_ASSERT(mActor);
  aFlavorList.AppendElements(mActor->FlavorList());
  return NS_OK;
}

NS_IMETHODIMP ClipboardDataSnapshotProxy::GetData(
    nsITransferable* aTransferable,
    nsIAsyncClipboardRequestCallback* aCallback) {
  if (!aTransferable || !aCallback) {
    return NS_ERROR_INVALID_ARG;
  }

  // Get a list of flavors this transferable can import
  nsTArray<nsCString> flavors;
  nsresult rv = aTransferable->FlavorsTransferableCanImport(flavors);
  if (NS_FAILED(rv)) {
    return rv;
  }

  MOZ_ASSERT(mActor);
  // If the requested flavor is not in the list, throw an error.
  for (const auto& flavor : flavors) {
    if (!mActor->FlavorList().Contains(flavor)) {
      return NS_ERROR_FAILURE;
    }
  }

  if (!mActor->CanSend()) {
    return aCallback->OnComplete(NS_ERROR_NOT_AVAILABLE);
  }

  mActor->SendGetData(flavors)->Then(
      GetMainThreadSerialEventTarget(), __func__,
      /* resolve */
      [self = RefPtr{this}, callback = nsCOMPtr{aCallback},
       transferable = nsCOMPtr{aTransferable}](
          const IPCTransferableDataOrError& aIpcTransferableDataOrError) {
        if (aIpcTransferableDataOrError.type() ==
            IPCTransferableDataOrError::Tnsresult) {
          MOZ_ASSERT(NS_FAILED(aIpcTransferableDataOrError.get_nsresult()));
          callback->OnComplete(aIpcTransferableDataOrError.get_nsresult());
          return;
        }

        nsresult rv = nsContentUtils::IPCTransferableDataToTransferable(
            aIpcTransferableDataOrError.get_IPCTransferableData(),
            false /* aAddDataFlavor */, transferable,
            false /* aFilterUnknownFlavors */);
        if (NS_FAILED(rv)) {
          callback->OnComplete(rv);
          return;
        }

        callback->OnComplete(NS_OK);
      },
      /* reject */
      [callback = nsCOMPtr{aCallback}](ResponseRejectReason aReason) {
        callback->OnComplete(ResponseRejectReason::ActorDestroyed == aReason
                                 ? NS_ERROR_NOT_AVAILABLE
                                 : NS_ERROR_FAILURE);
      });

  return NS_OK;
}

NS_IMETHODIMP ClipboardDataSnapshotProxy::GetDataSync(
    nsITransferable* aTransferable) {
  if (!aTransferable) {
    return NS_ERROR_INVALID_ARG;
  }

  // Get a list of flavors this transferable can import
  nsTArray<nsCString> flavors;
  nsresult rv = aTransferable->FlavorsTransferableCanImport(flavors);
  if (NS_FAILED(rv)) {
    return rv;
  }

  MOZ_ASSERT(mActor);
  // If the requested flavor is not in the list, throw an error.
  for (const auto& flavor : flavors) {
    if (!mActor->FlavorList().Contains(flavor)) {
      return NS_ERROR_FAILURE;
    }
  }

  if (!mActor->CanSend()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  IPCTransferableDataOrError ipcTransferableDataOrError;
  bool success = mActor->SendGetDataSync(flavors, &ipcTransferableDataOrError);
  if (!success) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  if (ipcTransferableDataOrError.type() ==
      IPCTransferableDataOrError::Tnsresult) {
    MOZ_ASSERT(NS_FAILED(ipcTransferableDataOrError.get_nsresult()));
    return ipcTransferableDataOrError.get_nsresult();
  }
  rv = nsContentUtils::IPCTransferableDataToTransferable(
      ipcTransferableDataOrError.get_IPCTransferableData(),
      false /* aAddDataFlavor */, aTransferable,
      false /* aFilterUnknownFlavors */);
  if (NS_FAILED(rv)) {
    return rv;
  }

  return NS_OK;
}

static Result<RefPtr<ClipboardDataSnapshotProxy>, nsresult>
CreateClipboardDataSnapshotProxy(
    ClipboardReadRequestOrError&& aClipboardReadRequestOrError) {
  if (aClipboardReadRequestOrError.type() ==
      ClipboardReadRequestOrError::Tnsresult) {
    MOZ_ASSERT(NS_FAILED(aClipboardReadRequestOrError.get_nsresult()));
    return Err(aClipboardReadRequestOrError.get_nsresult());
  }

  ClipboardReadRequest& request =
      aClipboardReadRequestOrError.get_ClipboardReadRequest();
  auto requestChild = MakeRefPtr<ClipboardReadRequestChild>(
      std::move(request.availableTypes()));
  if (NS_WARN_IF(
          !ContentChild::GetSingleton()->BindPClipboardReadRequestEndpoint(
              std::move(request.childEndpoint()), requestChild))) {
    return Err(NS_ERROR_FAILURE);
  }

  return MakeRefPtr<ClipboardDataSnapshotProxy>(requestChild);
}

}  // namespace

NS_IMETHODIMP nsClipboardProxy::GetDataSnapshot(
    const nsTArray<nsCString>& aFlavorList,
    nsIClipboard::ClipboardType aWhichClipboard,
    mozilla::dom::WindowContext* aRequestingWindowContext,
    nsIPrincipal* aRequestingPrincipal,
    nsIClipboardGetDataSnapshotCallback* aCallback) {
  if (!aCallback || !aRequestingPrincipal || aFlavorList.IsEmpty()) {
    return NS_ERROR_INVALID_ARG;
  }

  if (!nsIClipboard::IsClipboardTypeSupported(aWhichClipboard)) {
    MOZ_CLIPBOARD_LOG("%s: clipboard %d is not supported.", __FUNCTION__,
                      aWhichClipboard);
    return NS_ERROR_FAILURE;
  }

  ContentChild::GetSingleton()
      ->SendGetClipboardDataSnapshot(aFlavorList, aWhichClipboard,
                                     aRequestingWindowContext,
                                     WrapNotNull(aRequestingPrincipal))
      ->Then(
          GetMainThreadSerialEventTarget(), __func__,
          /* resolve */
          [callback = nsCOMPtr{aCallback}](
              ClipboardReadRequestOrError&& aClipboardReadRequestOrError) {
            auto result = CreateClipboardDataSnapshotProxy(
                std::move(aClipboardReadRequestOrError));
            if (result.isErr()) {
              callback->OnError(result.unwrapErr());
              return;
            }

            callback->OnSuccess(result.inspect());
          },
          /* reject */
          [callback = nsCOMPtr{aCallback}](ResponseRejectReason aReason) {
            callback->OnError(NS_ERROR_FAILURE);
          });
  return NS_OK;
}

NS_IMETHODIMP nsClipboardProxy::GetDataSnapshotSync(
    const nsTArray<nsCString>& aFlavorList,
    nsIClipboard::ClipboardType aWhichClipboard,
    mozilla::dom::WindowContext* aRequestingWindowContext,
    nsIClipboardDataSnapshot** _retval) {
  *_retval = nullptr;

  if (aFlavorList.IsEmpty()) {
    return NS_ERROR_INVALID_ARG;
  }

  if (!nsIClipboard::IsClipboardTypeSupported(aWhichClipboard)) {
    MOZ_CLIPBOARD_LOG("%s: clipboard %d is not supported.", __FUNCTION__,
                      aWhichClipboard);
    return NS_ERROR_FAILURE;
  }
  if (MOZ_UNLIKELY(nsIContentAnalysis::MightBeActive())) {
    // If Content Analysis is active we want to fetch all the clipboard data
    // up front since we need to analyze it anyway.
    RefPtr<ClipboardContentAnalysisChild> contentAnalysis =
        ClipboardContentAnalysisChild::GetOrCreate();
    IPCTransferableDataOrError ipcTransferableDataOrError;
    bool result = contentAnalysis->SendGetAllClipboardDataSync(
        aFlavorList, aWhichClipboard, aRequestingWindowContext->InnerWindowId(),
        &ipcTransferableDataOrError);
    if (!result) {
      return NS_ERROR_FAILURE;
    }
    if (ipcTransferableDataOrError.type() ==
        IPCTransferableDataOrError::Tnsresult) {
      return ipcTransferableDataOrError.get_nsresult();
    }
    nsresult rv;
    nsCOMPtr<nsITransferable> trans =
        do_CreateInstance("@mozilla.org/widget/transferable;1", &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    trans->Init(nullptr);
    rv = nsContentUtils::IPCTransferableDataToTransferable(
        ipcTransferableDataOrError.get_IPCTransferableData(),
        true /* aAddDataFlavor */, trans, false /* aFilterUnknownFlavors */);
    NS_ENSURE_SUCCESS(rv, rv);
    auto snapshot =
        mozilla::MakeRefPtr<nsBaseClipboard::ClipboardPopulatedDataSnapshot>(
            trans);
    snapshot.forget(_retval);
    return NS_OK;
  }
  ClipboardReadRequestOrError requestOrError;
  ContentChild* contentChild = ContentChild::GetSingleton();
  contentChild->SendGetClipboardDataSnapshotSync(
      aFlavorList, aWhichClipboard, aRequestingWindowContext, &requestOrError);
  auto result = CreateClipboardDataSnapshotProxy(std::move(requestOrError));
  if (result.isErr()) {
    return result.unwrapErr();
  }

  result.unwrap().forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
nsClipboardProxy::EmptyClipboard(nsIClipboard::ClipboardType aWhichClipboard) {
  ContentChild::GetSingleton()->SendEmptyClipboard(aWhichClipboard);
  return NS_OK;
}

NS_IMETHODIMP
nsClipboardProxy::HasDataMatchingFlavors(
    const nsTArray<nsCString>& aFlavorList,
    nsIClipboard::ClipboardType aWhichClipboard, bool* aHasType) {
  *aHasType = false;

  ContentChild::GetSingleton()->SendClipboardHasType(aFlavorList,
                                                     aWhichClipboard, aHasType);

  return NS_OK;
}

NS_IMETHODIMP
nsClipboardProxy::IsClipboardTypeSupported(
    nsIClipboard::ClipboardType aWhichClipboard, bool* aIsSupported) {
  switch (aWhichClipboard) {
    case kGlobalClipboard:
      // We always support the global clipboard.
      *aIsSupported = true;
      return NS_OK;
    case kSelectionClipboard:
      *aIsSupported = mClipboardCaps.supportsSelectionClipboard();
      return NS_OK;
    case kFindClipboard:
      *aIsSupported = mClipboardCaps.supportsFindClipboard();
      return NS_OK;
    case kSelectionCache:
      *aIsSupported = mClipboardCaps.supportsSelectionCache();
      return NS_OK;
  }

  *aIsSupported = false;
  return NS_OK;
}

void nsClipboardProxy::SetCapabilities(
    const ClipboardCapabilities& aClipboardCaps) {
  mClipboardCaps = aClipboardCaps;
}
