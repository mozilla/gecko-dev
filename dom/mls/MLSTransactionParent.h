/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MLSTransactionParent_h
#define mozilla_dom_MLSTransactionParent_h

#include "mozilla/dom/PMLSTransaction.h"
#include "mozilla/dom/PMLSTransactionParent.h"
#include "nsIPrincipal.h"

namespace mozilla::dom {

class MLSTransactionParent final : public PMLSTransactionParent {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MLSTransactionParent, override);

  explicit MLSTransactionParent(const nsACString& aDatabasePath)
      : mDatabasePath(aDatabasePath) {};

  static nsresult CreateDirectoryIfNotExists(nsIFile* aDir);

  static nsresult ConstructDatabasePrefixPath(nsCOMPtr<nsIFile>& aFile);

  static nsresult ConstructDatabaseFullPath(nsCOMPtr<nsIFile>& aFile,
                                            nsIPrincipal* aPrincipal,
                                            nsCString& aDatabasePath);

  void ActorDestroy(ActorDestroyReason) override;

  mozilla::ipc::IPCResult RecvRequestStateDelete(
      RequestStateDeleteResolver&& aResolver);

  mozilla::ipc::IPCResult RecvRequestGroupStateDelete(
      const nsTArray<uint8_t>& aGroupIdentifier,
      const nsTArray<uint8_t>& aIdentifier,
      RequestGroupStateDeleteResolver&& aResolver);

  mozilla::ipc::IPCResult RecvRequestGenerateIdentityKeypair(
      RequestGenerateIdentityKeypairResolver&& aResolver);

  mozilla::ipc::IPCResult RecvRequestGenerateCredentialBasic(
      const nsTArray<uint8_t>& aCredContent,
      RequestGenerateCredentialBasicResolver&& aResolver);

  mozilla::ipc::IPCResult RecvRequestGenerateKeyPackage(
      const nsTArray<uint8_t>& aIdentifier,
      const nsTArray<uint8_t>& aCredential,
      RequestGenerateKeyPackageResolver&& aResolver);

  mozilla::ipc::IPCResult RecvRequestGroupCreate(
      const nsTArray<uint8_t>& aIdentifier,
      const nsTArray<uint8_t>& aCredential,
      const nsTArray<uint8_t>& aInOptGroupIdentifier,
      RequestGroupCreateResolver&& aResolver);

  mozilla::ipc::IPCResult RecvRequestGroupJoin(
      const nsTArray<uint8_t>& aIdentifier, const nsTArray<uint8_t>& aWelcome,
      RequestGroupJoinResolver&& aResolver);

  mozilla::ipc::IPCResult RecvRequestGroupAdd(
      const nsTArray<uint8_t>& aGroupIdentifier,
      const nsTArray<uint8_t>& aIdentifier,
      const nsTArray<uint8_t>& aKeyPackage,
      RequestGroupAddResolver&& aResolver);

  mozilla::ipc::IPCResult RecvRequestGroupProposeAdd(
      const nsTArray<uint8_t>& aGroupIdentifier,
      const nsTArray<uint8_t>& aIdentifier,
      const nsTArray<uint8_t>& aKeyPackage,
      RequestGroupProposeAddResolver&& aResolver);

  mozilla::ipc::IPCResult RecvRequestGroupRemove(
      const nsTArray<uint8_t>& aGroupIdentifier,
      const nsTArray<uint8_t>& aIdentifier,
      const nsTArray<uint8_t>& aRemIdentifier,
      RequestGroupRemoveResolver&& aResolver);

  mozilla::ipc::IPCResult RecvRequestGroupProposeRemove(
      const nsTArray<uint8_t>& aGroupIdentifier,
      const nsTArray<uint8_t>& aIdentifier,
      const nsTArray<uint8_t>& aRemIdentifier,
      RequestGroupProposeRemoveResolver&& aResolver);

  mozilla::ipc::IPCResult RecvRequestGroupClose(
      const nsTArray<uint8_t>& aGroupIdentifier,
      const nsTArray<uint8_t>& aIdentifier,
      RequestGroupCloseResolver&& aResolver);

  mozilla::ipc::IPCResult RecvRequestGroupDetails(
      const nsTArray<uint8_t>& aGroupIdentifier,
      const nsTArray<uint8_t>& aIdentifier,
      RequestGroupDetailsResolver&& aResolver);

  mozilla::ipc::IPCResult RecvRequestReceive(
      const nsTArray<uint8_t>& aClientIdentifier,
      const nsTArray<uint8_t>& aMessage, RequestReceiveResolver&& aResolver);

  mozilla::ipc::IPCResult RecvRequestApplyPendingCommit(
      const nsTArray<uint8_t>& aGroupIdentifier,
      const nsTArray<uint8_t>& aClientIdentifier,
      RequestApplyPendingCommitResolver&& aResolver);

  mozilla::ipc::IPCResult RecvRequestSend(
      const nsTArray<uint8_t>& aGroupIdentifier,
      const nsTArray<uint8_t>& aIdentifier, const nsTArray<uint8_t>& aMessage,
      RequestSendResolver&& aResolver);

  mozilla::ipc::IPCResult RecvRequestExportSecret(
      const nsTArray<uint8_t>& aGroupIdentifier,
      const nsTArray<uint8_t>& aIdentifier, const nsTArray<uint8_t>& aLabel,
      const nsTArray<uint8_t>& aContext, uint64_t aLen,
      RequestExportSecretResolver&& aResolver);

  mozilla::ipc::IPCResult RecvRequestGetGroupIdentifier(
      const nsTArray<uint8_t>& aMessage,
      RequestGetGroupIdentifierResolver&& aResolver);

 protected:
  ~MLSTransactionParent() = default;
  nsCString mDatabasePath;
};

}  // namespace mozilla::dom

#endif
