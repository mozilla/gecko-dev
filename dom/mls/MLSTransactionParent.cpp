/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MLSTransactionParent.h"
#include "MLSTransactionMessage.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "mozilla/security/mls/mls_gk_ffi_generated.h"
#include "MLSLogging.h"
#include "mozilla/Base64.h"

#include "nsIFile.h"
#include "nsIPrincipal.h"
#include "nsString.h"
#include "nsCOMPtr.h"

using mozilla::dom::quota::QuotaManager;

namespace mozilla::dom {

/* static */ nsresult MLSTransactionParent::CreateDirectoryIfNotExists(
    nsIFile* aDir) {
  nsresult rv = aDir->Create(nsIFile::DIRECTORY_TYPE, 0755);
  if (rv == NS_ERROR_FILE_ALREADY_EXISTS) {
    // Evaluate if the file is a directory
    bool isDirectory = false;
    rv = aDir->IsDirectory(&isDirectory);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    // Check if the file is actually a directory
    if (!isDirectory) {
      return NS_ERROR_FILE_NOT_DIRECTORY;
    }
    return NS_OK;
  }
  return rv;
}

/* static */ nsresult MLSTransactionParent::ConstructDatabasePrefixPath(
    nsCOMPtr<nsIFile>& aFile) {
  MOZ_LOG(gMlsLog, mozilla::LogLevel::Debug,
          ("MLSTransactionParent::ConstructDatabasePath()"));

  // Get the base path from the quota manager
  QuotaManager* quotaManager = QuotaManager::Get();
  if (NS_WARN_IF(!quotaManager)) {
    return NS_ERROR_FAILURE;
  }

  // Create an nsIFile object from the path
  nsresult rv =
      NS_NewLocalFile(quotaManager->GetBasePath(), getter_AddRefs(aFile));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Append the hardcoded "mls" directory name to the path
  rv = aFile->AppendNative("mls"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

/* static */ nsresult MLSTransactionParent::ConstructDatabaseFullPath(
    nsCOMPtr<nsIFile>& aFile, nsIPrincipal* aPrincipal,
    nsCString& aDatabasePath) {
  MOZ_LOG(gMlsLog, mozilla::LogLevel::Debug,
          ("MLSTransactionParent::ConstructDatabaseFullPath()"));

  // Get StorageOriginKey
  nsAutoCString originKey;
  nsresult rv = aPrincipal->GetStorageOriginKey(originKey);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Get OriginSuffix
  nsAutoCString originAttrSuffix;
  rv = aPrincipal->GetOriginSuffix(originAttrSuffix);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Set the base path and origin
  nsAutoCString origin = originKey + originAttrSuffix;

  // Encode the origin with its suffix
  nsAutoCString encodedOrigin;
  rv = mozilla::Base64Encode(origin, encodedOrigin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_LOG(gMlsLog, mozilla::LogLevel::Debug,
          ("MLSTransactionParent::ConstructDatabasePath() - origin: %s",
           origin.get()));

  MOZ_LOG(gMlsLog, mozilla::LogLevel::Debug,
          ("MLSTransactionParent::ConstructDatabasePath() - encodedOrigin: "
           "%s",
           encodedOrigin.get()));

  // Append the origin to the path
  rv = aFile->AppendNative(encodedOrigin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Get the updated path back into the nsCString
  nsAutoString databasePathUTF16;
  rv = aFile->GetPath(databasePathUTF16);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  aDatabasePath = NS_ConvertUTF16toUTF8(databasePathUTF16);
  MOZ_LOG(gMlsLog, mozilla::LogLevel::Debug,
          ("MLSTransactionParent::ConstructDatabasePath() - databasePath: %s",
           aDatabasePath.get()));

  return NS_OK;
}

void MLSTransactionParent::ActorDestroy(ActorDestroyReason) {
  MOZ_LOG(gMlsLog, mozilla::LogLevel::Debug,
          ("MLSTransactionParent::ActorDestroy()"));
}

mozilla::ipc::IPCResult MLSTransactionParent::RecvRequestStateDelete(
    RequestStateDeleteResolver&& aResolver) {
  MOZ_LOG(gMlsLog, mozilla::LogLevel::Debug,
          ("MLSTransactionParent::RecvRequestStateDelete()"));

  // Call to the MLS rust code
  nsresult rv = security::mls::mls_state_delete(&mDatabasePath);

  aResolver(NS_SUCCEEDED(rv));
  return IPC_OK();
}

mozilla::ipc::IPCResult MLSTransactionParent::RecvRequestGroupStateDelete(
    const nsTArray<uint8_t>& aGroupIdentifier,
    const nsTArray<uint8_t>& aIdentifier,
    RequestGroupStateDeleteResolver&& aResolver) {
  MOZ_LOG(gMlsLog, mozilla::LogLevel::Debug,
          ("MLSTransactionParent::RecvRequestGroupStateDelete()"));

  // Call to the MLS rust code
  security::mls::GkGroupIdEpoch groupIdEpoch;
  nsresult rv = security::mls::mls_state_delete_group(
      &mDatabasePath, aGroupIdentifier.Elements(), aGroupIdentifier.Length(),
      aIdentifier.Elements(), aIdentifier.Length(), &groupIdEpoch);

  // Return Nothing if failed
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aResolver(Nothing());
    return IPC_OK();
  }

  // Return the result if success
  aResolver(Some(std::move(groupIdEpoch)));
  return IPC_OK();
}

mozilla::ipc::IPCResult
MLSTransactionParent::RecvRequestGenerateIdentityKeypair(
    RequestGenerateIdentityKeypairResolver&& aResolver) {
  MOZ_LOG(gMlsLog, mozilla::LogLevel::Debug,
          ("MLSTransactionParent::RecvRequestGenerateIdentityKeypair()"));

  // Call to the MLS rust code
  nsTArray<uint8_t> signatureIdentifier;
  nsresult rv = security::mls::mls_generate_signature_keypair(
      &mDatabasePath, &signatureIdentifier);

  // Return Nothing if failed
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aResolver(Nothing());
    return IPC_OK();
  }

  // Return the result if success
  aResolver(Some(RawBytes{std::move(signatureIdentifier)}));
  return IPC_OK();
}

mozilla::ipc::IPCResult
MLSTransactionParent::RecvRequestGenerateCredentialBasic(
    const nsTArray<uint8_t>& aCredContent,
    RequestGenerateCredentialBasicResolver&& aResolver) {
  MOZ_LOG(gMlsLog, mozilla::LogLevel::Debug,
          ("MLSTransactionParent::RecvRequestGenerateCredentialBasic()"));

  // Call to the MLS rust code
  nsTArray<uint8_t> credential;
  nsresult rv = security::mls::mls_generate_credential_basic(
      aCredContent.Elements(), aCredContent.Length(), &credential);

  // Return Nothing if failed
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aResolver(Nothing());
    return IPC_OK();
  }

  // Return the result if success
  aResolver(Some(RawBytes{std::move(credential)}));

  return IPC_OK();
}

mozilla::ipc::IPCResult MLSTransactionParent::RecvRequestGenerateKeyPackage(
    const nsTArray<uint8_t>& aIdentifier, const nsTArray<uint8_t>& aCredential,
    RequestGenerateKeyPackageResolver&& aResolver) {
  MOZ_LOG(gMlsLog, mozilla::LogLevel::Debug,
          ("MLSTransactionParent::RecvRequestGenerateKeyPackage()"));

  // Call to the MLS rust code
  nsTArray<uint8_t> keyPackage;
  nsresult rv = security::mls::mls_generate_keypackage(
      &mDatabasePath, aIdentifier.Elements(), aIdentifier.Length(),
      aCredential.Elements(), aCredential.Length(), &keyPackage);

  // Return Nothing if failed
  if (NS_FAILED(rv)) {
    aResolver(Nothing());
    return IPC_OK();
  }

  // Return the result if success
  aResolver(Some(RawBytes{std::move(keyPackage)}));

  return IPC_OK();
}

mozilla::ipc::IPCResult MLSTransactionParent::RecvRequestGroupCreate(
    const nsTArray<uint8_t>& aIdentifier, const nsTArray<uint8_t>& aCredential,
    const nsTArray<uint8_t>& aInOptGroupIdentifier,
    RequestGroupCreateResolver&& aResolver) {
  MOZ_LOG(gMlsLog, mozilla::LogLevel::Debug,
          ("MLSTransactionParent::RecvRequestGroupCreate()"));

  // Call to the MLS rust code
  security::mls::GkGroupIdEpoch groupIdEpoch;
  nsresult rv = security::mls::mls_group_create(
      &mDatabasePath, aIdentifier.Elements(), aIdentifier.Length(),
      aCredential.Elements(), aCredential.Length(),
      aInOptGroupIdentifier.Elements(), aInOptGroupIdentifier.Length(),
      &groupIdEpoch);

  // Return Nothing if failed
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aResolver(Nothing());
    return IPC_OK();
  }

  // Return the result if success
  aResolver(Some(std::move(groupIdEpoch)));

  return IPC_OK();
}

mozilla::ipc::IPCResult MLSTransactionParent::RecvRequestGroupJoin(
    const nsTArray<uint8_t>& aIdentifier, const nsTArray<uint8_t>& aWelcome,
    RequestGroupJoinResolver&& aResolver) {
  MOZ_LOG(gMlsLog, mozilla::LogLevel::Debug,
          ("MLSTransactionParent::RecvRequestGroupJoin()"));

  // Call to the MLS rust code
  security::mls::GkGroupIdEpoch groupIdEpoch;
  nsresult rv = security::mls::mls_group_join(
      &mDatabasePath, aIdentifier.Elements(), aIdentifier.Length(),
      aWelcome.Elements(), aWelcome.Length(), &groupIdEpoch);

  // Return Nothing if failed
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aResolver(Nothing());
    return IPC_OK();
  }

  // Return the result if success
  aResolver(Some(std::move(groupIdEpoch)));

  return IPC_OK();
}

mozilla::ipc::IPCResult MLSTransactionParent::RecvRequestGroupAdd(
    const nsTArray<uint8_t>& aGroupIdentifier,
    const nsTArray<uint8_t>& aIdentifier, const nsTArray<uint8_t>& aKeyPackage,
    RequestGroupAddResolver&& aResolver) {
  MOZ_LOG(gMlsLog, mozilla::LogLevel::Debug,
          ("MLSTransactionParent::RecvRequestGroupAdd()"));

  // Call to the MLS rust code
  security::mls::GkMlsCommitOutput commitOutput;
  nsresult rv = security::mls::mls_group_add(
      &mDatabasePath, aGroupIdentifier.Elements(), aGroupIdentifier.Length(),
      aIdentifier.Elements(), aIdentifier.Length(), aKeyPackage.Elements(),
      aKeyPackage.Length(), &commitOutput);

  // Return Nothing if failed
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aResolver(Nothing());
    return IPC_OK();
  }

  // Return the result if success
  aResolver(Some(std::move(commitOutput)));

  return IPC_OK();
}

mozilla::ipc::IPCResult MLSTransactionParent::RecvRequestGroupProposeAdd(
    const nsTArray<uint8_t>& aGroupIdentifier,
    const nsTArray<uint8_t>& aIdentifier, const nsTArray<uint8_t>& aKeyPackage,
    RequestGroupProposeAddResolver&& aResolver) {
  MOZ_LOG(gMlsLog, mozilla::LogLevel::Debug,
          ("MLSTransactionParent::RecvRequestGroupProposeAdd()"));

  // Call to the MLS rust code
  nsTArray<uint8_t> proposal;
  nsresult rv = security::mls::mls_group_propose_add(
      &mDatabasePath, aGroupIdentifier.Elements(), aGroupIdentifier.Length(),
      aIdentifier.Elements(), aIdentifier.Length(), aKeyPackage.Elements(),
      aKeyPackage.Length(), &proposal);

  // Return Nothing if failed
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aResolver(Nothing());
    return IPC_OK();
  }

  // Return the result if success
  aResolver(Some(RawBytes{std::move(proposal)}));

  return IPC_OK();
}

mozilla::ipc::IPCResult MLSTransactionParent::RecvRequestGroupRemove(
    const nsTArray<uint8_t>& aGroupIdentifier,
    const nsTArray<uint8_t>& aIdentifier,
    const nsTArray<uint8_t>& aRemIdentifier,
    RequestGroupRemoveResolver&& aResolver) {
  MOZ_LOG(gMlsLog, mozilla::LogLevel::Debug,
          ("MLSTransactionParent::RecvRequestGroupRemove()"));

  // Call to the MLS rust code
  security::mls::GkMlsCommitOutput commitOutput;
  nsresult rv = security::mls::mls_group_remove(
      &mDatabasePath, aGroupIdentifier.Elements(), aGroupIdentifier.Length(),
      aIdentifier.Elements(), aIdentifier.Length(), aRemIdentifier.Elements(),
      aRemIdentifier.Length(), &commitOutput);

  // Return Nothing if failed
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aResolver(Nothing());
    return IPC_OK();
  }

  // Return the result if success
  aResolver(Some(std::move(commitOutput)));

  return IPC_OK();
}

mozilla::ipc::IPCResult MLSTransactionParent::RecvRequestGroupProposeRemove(
    const nsTArray<uint8_t>& aGroupIdentifier,
    const nsTArray<uint8_t>& aIdentifier,
    const nsTArray<uint8_t>& aRemIdentifier,
    RequestGroupProposeRemoveResolver&& aResolver) {
  MOZ_LOG(gMlsLog, mozilla::LogLevel::Debug,
          ("MLSTransactionParent::RecvRequestGroupProposeRemove()"));

  nsTArray<uint8_t> proposal;
  nsresult rv = security::mls::mls_group_propose_remove(
      &mDatabasePath, aGroupIdentifier.Elements(), aGroupIdentifier.Length(),
      aIdentifier.Elements(), aIdentifier.Length(), aRemIdentifier.Elements(),
      aRemIdentifier.Length(), &proposal);

  // Return Nothing if failed
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aResolver(Nothing());
    return IPC_OK();
  }

  // Return the result if success
  aResolver(Some(RawBytes{std::move(proposal)}));

  return IPC_OK();
}

mozilla::ipc::IPCResult MLSTransactionParent::RecvRequestGroupClose(
    const nsTArray<uint8_t>& aGroupIdentifier,
    const nsTArray<uint8_t>& aIdentifier,
    RequestGroupCloseResolver&& aResolver) {
  MOZ_LOG(gMlsLog, mozilla::LogLevel::Debug,
          ("MLSTransactionParent::RecvRequestGroupClose()"));

  // Call to the MLS rust code
  security::mls::GkMlsCommitOutput commitOutput;
  nsresult rv = security::mls::mls_group_close(
      &mDatabasePath, aGroupIdentifier.Elements(), aGroupIdentifier.Length(),
      aIdentifier.Elements(), aIdentifier.Length(), &commitOutput);

  // Return Nothing if failed
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aResolver(Nothing());
    return IPC_OK();
  }

  // Return the result if success
  aResolver(Some(std::move(commitOutput)));

  return IPC_OK();
}

mozilla::ipc::IPCResult MLSTransactionParent::RecvRequestGroupDetails(
    const nsTArray<uint8_t>& aGroupIdentifier,
    const nsTArray<uint8_t>& aIdentifier,
    RequestGroupDetailsResolver&& aResolver) {
  MOZ_LOG(gMlsLog, mozilla::LogLevel::Debug,
          ("MLSTransactionParent::RecvRequestGroupDetails()"));

  // Call to the MLS rust code
  security::mls::GkGroupMembers members;
  nsresult rv = security::mls::mls_group_members(
      &mDatabasePath, aGroupIdentifier.Elements(), aGroupIdentifier.Length(),
      aIdentifier.Elements(), aIdentifier.Length(), &members);

  // Return Nothing if failed
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aResolver(Nothing());
    return IPC_OK();
  }

  // Return the result if success
  aResolver(Some(std::move(members)));

  return IPC_OK();
}

mozilla::ipc::IPCResult MLSTransactionParent::RecvRequestReceive(
    const nsTArray<uint8_t>& aClientIdentifier,
    const nsTArray<uint8_t>& aMessage, RequestReceiveResolver&& aResolver) {
  MOZ_LOG(gMlsLog, mozilla::LogLevel::Debug,
          ("MLSTransactionParent::RecvRequestReceive()"));

  // Call to the MLS rust code
  GkReceived received;
  nsTArray<uint8_t> group_id_bytes;

  nsresult rv = security::mls::mls_receive(
      &mDatabasePath, aClientIdentifier.Elements(), aClientIdentifier.Length(),
      aMessage.Elements(), aMessage.Length(), &group_id_bytes, &received);

  // Return Nothing if failed
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aResolver(GkReceived());
    return IPC_OK();
  }

  // Return the result if success
  aResolver(received);

  return IPC_OK();
}

mozilla::ipc::IPCResult MLSTransactionParent::RecvRequestApplyPendingCommit(
    const nsTArray<uint8_t>& aGroupIdentifier,
    const nsTArray<uint8_t>& aClientIdentifier,
    RequestApplyPendingCommitResolver&& aResolver) {
  MOZ_LOG(gMlsLog, mozilla::LogLevel::Debug,
          ("MLSTransactionParent::RecvRequestApplyPendingCommit()"));

  // Call to the MLS rust code
  GkReceived received;
  nsresult rv = security::mls::mls_receive_ack(
      &mDatabasePath, aGroupIdentifier.Elements(), aGroupIdentifier.Length(),
      aClientIdentifier.Elements(), aClientIdentifier.Length(), &received);

  // Return Nothing if failed
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aResolver(GkReceived());
    return IPC_OK();
  }

  // Return the result if success
  aResolver(received);

  return IPC_OK();
}

mozilla::ipc::IPCResult MLSTransactionParent::RecvRequestSend(
    const nsTArray<uint8_t>& aGroupIdentifier,
    const nsTArray<uint8_t>& aIdentifier, const nsTArray<uint8_t>& aMessage,
    RequestSendResolver&& aResolver) {
  MOZ_LOG(gMlsLog, mozilla::LogLevel::Debug,
          ("MLSTransactionParent::RecvRequestSend()"));

  // Call to the MLS rust code
  nsTArray<uint8_t> outputMessage;
  nsresult rv = security::mls::mls_send(
      &mDatabasePath, aGroupIdentifier.Elements(), aGroupIdentifier.Length(),
      aIdentifier.Elements(), aIdentifier.Length(), aMessage.Elements(),
      aMessage.Length(), &outputMessage);

  // Return Nothing if failed
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aResolver(Nothing());
    return IPC_OK();
  }

  // Return the result if success
  aResolver(Some(RawBytes{std::move(outputMessage)}));

  return IPC_OK();
}

mozilla::ipc::IPCResult MLSTransactionParent::RecvRequestExportSecret(
    const nsTArray<uint8_t>& aGroupIdentifier,
    const nsTArray<uint8_t>& aIdentifier, const nsTArray<uint8_t>& aLabel,
    const nsTArray<uint8_t>& aContext, uint64_t aLen,
    RequestExportSecretResolver&& aResolver) {
  MOZ_LOG(gMlsLog, mozilla::LogLevel::Debug,
          ("MLSTransactionParent::RecvRequestExportSecret()"));

  // Call to the MLS rust code
  security::mls::GkExporterOutput exporterOutput;
  nsresult rv = security::mls::mls_derive_exporter(
      &mDatabasePath, aGroupIdentifier.Elements(), aGroupIdentifier.Length(),
      aIdentifier.Elements(), aIdentifier.Length(), aLabel.Elements(),
      aLabel.Length(), aContext.Elements(), aContext.Length(), aLen,
      &exporterOutput);

  // Return Nothing if failed
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aResolver(Nothing());
    return IPC_OK();
  }

  // Return the result if success
  aResolver(Some(std::move(exporterOutput)));

  return IPC_OK();
}

mozilla::ipc::IPCResult MLSTransactionParent::RecvRequestGetGroupIdentifier(
    const nsTArray<uint8_t>& aMessage,
    RequestGetGroupIdentifierResolver&& aResolver) {
  MOZ_LOG(gMlsLog, mozilla::LogLevel::Debug,
          ("MLSTransactionParent::RecvRequestGetGroupIdentifier()"));

  nsTArray<uint8_t> groupId;
  nsresult rv = security::mls::mls_get_group_id(aMessage.Elements(),
                                                aMessage.Length(), &groupId);

  // Return Nothing if failed
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aResolver(Nothing());
    return IPC_OK();
  }

  // Return the result if success
  aResolver(Some(RawBytes{std::move(groupId)}));

  return IPC_OK();
}

}  // namespace mozilla::dom
