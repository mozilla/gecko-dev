/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileSystemManagerParentFactory.h"

#include "mozilla/OriginAttributes.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/dom/FileSystemDataManager.h"
#include "mozilla/dom/FileSystemLog.h"
#include "mozilla/dom/FileSystemManagerParent.h"
#include "mozilla/dom/FileSystemTypes.h"
#include "mozilla/dom/quota/PrincipalUtils.h"
#include "mozilla/dom/quota/QuotaCommon.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "mozilla/dom/quota/ResultExtensions.h"
#include "mozilla/ipc/Endpoint.h"
#include "mozilla/ipc/PBackgroundParent.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsString.h"

namespace mozilla::dom {
mozilla::ipc::IPCResult CreateFileSystemManagerParent(
    RefPtr<mozilla::ipc::PBackgroundParent> aBackgroundActor,
    const mozilla::ipc::PrincipalInfo& aPrincipalInfo,
    mozilla::ipc::Endpoint<PFileSystemManagerParent>&& aParentEndpoint,
    std::function<void(const nsresult&)>&& aResolver) {
  using CreateActorPromise =
      MozPromise<RefPtr<FileSystemManagerParent>, nsresult, true>;

  QM_TRY(OkIf(StaticPrefs::dom_fs_enabled()), IPC_OK(),
         [aResolver](const auto&) { aResolver(NS_ERROR_DOM_NOT_ALLOWED_ERR); });

  QM_TRY(OkIf(aParentEndpoint.IsValid()), IPC_OK(),
         [aResolver](const auto&) { aResolver(NS_ERROR_INVALID_ARG); });

  // This blocks Null and Expanded principals
  QM_TRY(OkIf(quota::IsPrincipalInfoValid(aPrincipalInfo)), IPC_OK(),
         [aResolver](const auto&) { aResolver(NS_ERROR_DOM_SECURITY_ERR); });

  QM_TRY(quota::QuotaManager::EnsureCreated(), IPC_OK(),
         [aResolver](const auto rv) { aResolver(rv); });

  auto* const quotaManager = quota::QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  QM_TRY_UNWRAP(
      auto principalMetadata,
      quota::GetInfoFromValidatedPrincipalInfo(*quotaManager, aPrincipalInfo),
      IPC_OK(), [aResolver](const auto rv) { aResolver(rv); });

  quota::OriginMetadata originMetadata(std::move(principalMetadata),
                                       quota::PERSISTENCE_TYPE_DEFAULT);

  // Block use for now in PrivateBrowsing
  QM_TRY(OkIf(!OriginAttributes::IsPrivateBrowsing(originMetadata.mOrigin)),
         IPC_OK(),
         [aResolver](const auto&) { aResolver(NS_ERROR_DOM_NOT_ALLOWED_ERR); });

  LOG(("CreateFileSystemManagerParent, origin: %s",
       originMetadata.mOrigin.get()));

  RefPtr<mozilla::ipc::PBackgroundParent> backgroundActor =
      std::move(aBackgroundActor);

  // This creates the file system data manager, which has to be done on
  // PBackground
  fs::data::FileSystemDataManager::GetOrCreateFileSystemDataManager(
      originMetadata)
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [origin = originMetadata.mOrigin,
           parentEndpoint = std::move(aParentEndpoint), backgroundActor,
           aResolver](const fs::Registered<fs::data::FileSystemDataManager>&
                          dataManager) mutable {
            QM_TRY_UNWRAP(fs::EntryId rootId, fs::data::GetRootHandle(origin),
                          QM_VOID,
                          ([backgroundActor, aResolver](const auto& aRv) {
                            if (!backgroundActor->CanSend()) {
                              return;
                            }

                            aResolver(ToNSResult(aRv));
                          }));

            InvokeAsync(
                dataManager->MutableIOTaskQueuePtr(), __func__,
                [dataManager = dataManager, rootId,
                 parentEndpoint = std::move(parentEndpoint)]() mutable {
                  RefPtr<FileSystemManagerParent> parent =
                      new FileSystemManagerParent(dataManager.inspect(),
                                                  rootId);

                  auto autoProxyDestroyFileSystemDataManagerHandle =
                      MakeScopeExit([&dataManager] {
                        nsCOMPtr<nsISerialEventTarget> target =
                            dataManager->MutableBackgroundTargetPtr();

                        MOZ_ALWAYS_SUCCEEDS(target->Dispatch(
                            NS_NewRunnableFunction(
                                "DestroyFileSystemDataManagerHandle",
                                [dataManager = std::move(dataManager)]() {}),
                            NS_DISPATCH_NORMAL));
                      });

                  LOG(("Binding parent endpoint"));
                  if (!parentEndpoint.Bind(parent)) {
                    return CreateActorPromise::CreateAndReject(NS_ERROR_FAILURE,
                                                               __func__);
                  }

                  return CreateActorPromise::CreateAndResolve(std::move(parent),
                                                              __func__);
                })
                ->Then(GetCurrentSerialEventTarget(), __func__,
                       [dataManager = dataManager](
                           CreateActorPromise::ResolveOrRejectValue&& aValue) {
                         if (aValue.IsReject()) {
                           return BoolPromise::CreateAndReject(
                               aValue.RejectValue(), __func__);
                         }

                         RefPtr<FileSystemManagerParent> parent =
                             std::move(aValue.ResolveValue());

                         if (!parent->IsAlive()) {
                           return BoolPromise::CreateAndReject(NS_ERROR_ABORT,
                                                               __func__);
                         }

                         dataManager->RegisterActor(WrapNotNull(parent));

                         return BoolPromise::CreateAndResolve(true, __func__);
                       })
                ->Then(dataManager->MutableIOTaskQueuePtr(), __func__,
                       [](const BoolPromise::ResolveOrRejectValue& aValue) {
                         // Hopping to the I/O task queue is needed to avoid
                         // a potential race triggered by
                         // FileSystemManagerParent::SendCloseAll called by
                         // FileSystemManagerParent::RequestAllowToClose called
                         // by FileSystemDataManager::RegisterActor when the
                         // directory lock has been invalidated in the
                         // meantime. The race would cause that the child side
                         // could sometimes use the child actor for sending
                         // messages and sometimes not. This extra hop
                         // guarantees that the created child actor will always
                         // refuse to send messages.
                         return BoolPromise::CreateAndResolveOrReject(aValue,
                                                                      __func__);
                       })
                ->Then(GetCurrentSerialEventTarget(), __func__,
                       [backgroundActor, aResolver](
                           const BoolPromise::ResolveOrRejectValue& aValue) {
                         if (!backgroundActor->CanSend()) {
                           return;
                         }

                         if (aValue.IsReject()) {
                           aResolver(aValue.RejectValue());
                         } else {
                           aResolver(NS_OK);
                         }
                       });
          },
          [backgroundActor, aResolver](nsresult aRejectValue) {
            if (!backgroundActor->CanSend()) {
              return;
            }

            aResolver(aRejectValue);
          });

  return IPC_OK();
}

}  // namespace mozilla::dom
