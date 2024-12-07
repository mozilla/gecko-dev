/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OriginOperations.h"

#include <algorithm>
#include <cstdint>
#include <utility>

#include "ErrorList.h"
#include "FileUtils.h"
#include "GroupInfo.h"
#include "MainThreadUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/Atomics.h"
#include "mozilla/Maybe.h"
#include "mozilla/NotNull.h"
#include "mozilla/ProfilerLabels.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Result.h"
#include "mozilla/ResultExtensions.h"
#include "mozilla/dom/Nullable.h"
#include "mozilla/dom/quota/CommonMetadata.h"
#include "mozilla/dom/quota/Client.h"
#include "mozilla/dom/quota/Constants.h"
#include "mozilla/dom/quota/DirectoryLock.h"
#include "mozilla/dom/quota/DirectoryLockInlines.h"
#include "mozilla/dom/quota/OriginDirectoryLock.h"
#include "mozilla/dom/quota/PersistenceType.h"
#include "mozilla/dom/quota/PrincipalUtils.h"
#include "mozilla/dom/quota/PQuota.h"
#include "mozilla/dom/quota/PQuotaRequest.h"
#include "mozilla/dom/quota/PQuotaUsageRequest.h"
#include "mozilla/dom/quota/OriginScope.h"
#include "mozilla/dom/quota/PersistenceScope.h"
#include "mozilla/dom/quota/QuotaCommon.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "mozilla/dom/quota/QuotaManagerImpl.h"
#include "mozilla/dom/quota/ResultExtensions.h"
#include "mozilla/dom/quota/StreamUtils.h"
#include "mozilla/dom/quota/UniversalDirectoryLock.h"
#include "mozilla/dom/quota/UsageInfo.h"
#include "mozilla/fallible.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/ipc/PBackgroundSharedTypes.h"
#include "NormalOriginOperationBase.h"
#include "nsCOMPtr.h"
#include "nsTHashMap.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsHashKeys.h"
#include "nsIBinaryOutputStream.h"
#include "nsIFile.h"
#include "nsIObjectOutputStream.h"
#include "nsIOutputStream.h"
#include "nsLiteralString.h"
#include "nsPrintfCString.h"
#include "nsString.h"
#include "nsTArray.h"
#include "OriginInfo.h"
#include "OriginOperationBase.h"
#include "QuotaRequestBase.h"
#include "ResolvableNormalOriginOp.h"
#include "prthread.h"
#include "prtime.h"

namespace mozilla::dom::quota {

using namespace mozilla::ipc;

template <class Base>
class OpenStorageDirectoryHelper : public Base {
 protected:
  OpenStorageDirectoryHelper(MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
                             const char* aName)
      : Base(std::move(aQuotaManager), aName) {}

  RefPtr<BoolPromise> OpenStorageDirectory(
      const PersistenceScope& aPersistenceScope,
      const OriginScope& aOriginScope,
      const Nullable<Client::Type>& aClientType, bool aExclusive,
      bool aInitializeOrigins = false,
      DirectoryLockCategory aCategory = DirectoryLockCategory::None);

  RefPtr<UniversalDirectoryLock> mDirectoryLock;
};

class FinalizeOriginEvictionOp : public OriginOperationBase {
  nsTArray<RefPtr<OriginDirectoryLock>> mLocks;

 public:
  FinalizeOriginEvictionOp(MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
                           nsTArray<RefPtr<OriginDirectoryLock>>&& aLocks)
      : OriginOperationBase(std::move(aQuotaManager),
                            "dom::quota::FinalizeOriginEvictionOp"),
        mLocks(std::move(aLocks)) {
    AssertIsOnOwningThread();
  }

  NS_INLINE_DECL_REFCOUNTING(FinalizeOriginEvictionOp, override)

 private:
  ~FinalizeOriginEvictionOp() = default;

  virtual RefPtr<BoolPromise> Open() override;

  virtual nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  virtual void UnblockOpen() override;
};

class SaveOriginAccessTimeOp
    : public OpenStorageDirectoryHelper<NormalOriginOperationBase> {
  const OriginMetadata mOriginMetadata;
  int64_t mTimestamp;

 public:
  SaveOriginAccessTimeOp(MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
                         const OriginMetadata& aOriginMetadata,
                         int64_t aTimestamp)
      : OpenStorageDirectoryHelper(std::move(aQuotaManager),
                                   "dom::quota::SaveOriginAccessTimeOp"),
        mOriginMetadata(aOriginMetadata),
        mTimestamp(aTimestamp) {
    AssertIsOnOwningThread();
  }

  NS_INLINE_DECL_REFCOUNTING(SaveOriginAccessTimeOp, override)

 private:
  ~SaveOriginAccessTimeOp() = default;

  RefPtr<BoolPromise> OpenDirectory() override;

  virtual nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  virtual void SendResults() override;

  void CloseDirectory() override;
};

class ClearPrivateRepositoryOp
    : public OpenStorageDirectoryHelper<ResolvableNormalOriginOp<bool>> {
 public:
  explicit ClearPrivateRepositoryOp(
      MovingNotNull<RefPtr<QuotaManager>> aQuotaManager)
      : OpenStorageDirectoryHelper(std::move(aQuotaManager),
                                   "dom::quota::ClearPrivateRepositoryOp") {
    AssertIsOnOwningThread();
  }

 private:
  ~ClearPrivateRepositoryOp() = default;

  RefPtr<BoolPromise> OpenDirectory() override;

  nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  bool UnwrapResolveValue() override { return true; }

  void CloseDirectory() override;
};

class ShutdownStorageOp : public ResolvableNormalOriginOp<bool> {
  RefPtr<UniversalDirectoryLock> mDirectoryLock;

 public:
  explicit ShutdownStorageOp(MovingNotNull<RefPtr<QuotaManager>> aQuotaManager)
      : ResolvableNormalOriginOp(std::move(aQuotaManager),
                                 "dom::quota::ShutdownStorageOp") {
    AssertIsOnOwningThread();
  }

 private:
  ~ShutdownStorageOp() = default;

#ifdef DEBUG
  nsresult DirectoryOpen() override;
#endif

  RefPtr<BoolPromise> OpenDirectory() override;

  nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  bool UnwrapResolveValue() override { return true; }

  void CloseDirectory() override;
};

class CancelableHelper {
 protected:
  virtual const Atomic<bool>& GetIsCanceledFlag() = 0;
};

// A mix-in class to simplify operations that need to process every origin in
// one or more repositories. Sub-classes should call TraverseRepository in their
// DoDirectoryWork and implement a ProcessOrigin method for their per-origin
// logic.
class TraverseRepositoryHelper : public CancelableHelper {
 public:
  TraverseRepositoryHelper() = default;

 protected:
  virtual ~TraverseRepositoryHelper() = default;

  // If ProcessOrigin returns an error, TraverseRepository will immediately
  // terminate and return the received error code to its caller.
  nsresult TraverseRepository(QuotaManager& aQuotaManager,
                              PersistenceType aPersistenceType);

 private:
  virtual nsresult ProcessOrigin(QuotaManager& aQuotaManager,
                                 nsIFile& aOriginDir, const bool aPersistent,
                                 const PersistenceType aPersistenceType) = 0;
};

class OriginUsageHelper : public CancelableHelper {
 protected:
  mozilla::Result<UsageInfo, nsresult> GetUsageForOrigin(
      QuotaManager& aQuotaManager, PersistenceType aPersistenceType,
      const OriginMetadata& aOriginMetadata);

 private:
  mozilla::Result<UsageInfo, nsresult> GetUsageForOriginEntries(
      QuotaManager& aQuotaManager, PersistenceType aPersistenceType,
      const OriginMetadata& aOriginMetadata, nsIFile& aDirectory,
      bool aInitialized);
};

class GetUsageOp final
    : public OpenStorageDirectoryHelper<
          ResolvableNormalOriginOp<OriginUsageMetadataArray, true>>,
      public TraverseRepositoryHelper,
      public OriginUsageHelper {
  OriginUsageMetadataArray mOriginUsages;
  nsTHashMap<nsCStringHashKey, uint32_t> mOriginUsagesIndex;

  bool mGetAll;

 public:
  GetUsageOp(MovingNotNull<RefPtr<QuotaManager>> aQuotaManager, bool aGetAll);

 private:
  ~GetUsageOp() = default;

  void ProcessOriginInternal(QuotaManager* aQuotaManager,
                             const PersistenceType aPersistenceType,
                             const nsACString& aOrigin,
                             const int64_t aTimestamp, const bool aPersisted,
                             const uint64_t aUsage);

  RefPtr<BoolPromise> OpenDirectory() override;

  const Atomic<bool>& GetIsCanceledFlag() override;

  nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  nsresult ProcessOrigin(QuotaManager& aQuotaManager, nsIFile& aOriginDir,
                         const bool aPersistent,
                         const PersistenceType aPersistenceType) override;

  OriginUsageMetadataArray UnwrapResolveValue() override;

  void CloseDirectory() override;
};

class GetOriginUsageOp final
    : public OpenStorageDirectoryHelper<ResolvableNormalOriginOp<UsageInfo>>,
      public OriginUsageHelper {
  const PrincipalInfo mPrincipalInfo;
  PrincipalMetadata mPrincipalMetadata;
  UsageInfo mUsageInfo;

 public:
  GetOriginUsageOp(MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
                   const PrincipalInfo& aPrincipalInfo);

 private:
  ~GetOriginUsageOp() = default;

  nsresult DoInit(QuotaManager& aQuotaManager) override;

  RefPtr<BoolPromise> OpenDirectory() override;

  virtual nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  const Atomic<bool>& GetIsCanceledFlag() override;

  UsageInfo UnwrapResolveValue() override;

  void CloseDirectory() override;
};

class StorageNameOp final : public QuotaRequestBase {
  nsString mName;

 public:
  explicit StorageNameOp(MovingNotNull<RefPtr<QuotaManager>> aQuotaManager);

 private:
  ~StorageNameOp() = default;

  RefPtr<BoolPromise> OpenDirectory() override;

  nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  void GetResponse(RequestResponse& aResponse) override;

  void CloseDirectory() override;
};

class InitializedRequestBase : public ResolvableNormalOriginOp<bool> {
 protected:
  bool mInitialized;

  InitializedRequestBase(MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
                         const char* aName);

 private:
  RefPtr<BoolPromise> OpenDirectory() override;

  void CloseDirectory() override;
};

class StorageInitializedOp final : public InitializedRequestBase {
 public:
  explicit StorageInitializedOp(
      MovingNotNull<RefPtr<QuotaManager>> aQuotaManager)
      : InitializedRequestBase(std::move(aQuotaManager),
                               "dom::quota::StorageInitializedOp") {}

 private:
  ~StorageInitializedOp() = default;

  nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  bool UnwrapResolveValue() override;
};

class PersistentStorageInitializedOp final : public InitializedRequestBase {
 public:
  explicit PersistentStorageInitializedOp(
      MovingNotNull<RefPtr<QuotaManager>> aQuotaManager)
      : InitializedRequestBase(std::move(aQuotaManager),
                               "dom::quota::PersistentStorageInitializedOp") {}

 private:
  ~PersistentStorageInitializedOp() = default;

  nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  bool UnwrapResolveValue() override;
};

class TemporaryStorageInitializedOp final : public InitializedRequestBase {
 public:
  explicit TemporaryStorageInitializedOp(
      MovingNotNull<RefPtr<QuotaManager>> aQuotaManager)
      : InitializedRequestBase(std::move(aQuotaManager),
                               "dom::quota::TemporaryStorageInitializedOp") {}

 private:
  ~TemporaryStorageInitializedOp() = default;

  nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  bool UnwrapResolveValue() override;
};

class TemporaryGroupInitializedOp final
    : public ResolvableNormalOriginOp<bool> {
  const PrincipalInfo mPrincipalInfo;
  PrincipalMetadata mPrincipalMetadata;
  bool mInitialized;

 public:
  explicit TemporaryGroupInitializedOp(
      MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
      const PrincipalInfo& aPrincipalInfo);

 private:
  ~TemporaryGroupInitializedOp() = default;

  nsresult DoInit(QuotaManager& aQuotaManager) override;

  RefPtr<BoolPromise> OpenDirectory() override;

  nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  bool UnwrapResolveValue() override;

  void CloseDirectory() override;
};

class InitializedOriginRequestBase : public ResolvableNormalOriginOp<bool> {
 protected:
  const PrincipalMetadata mPrincipalMetadata;
  bool mInitialized;

  InitializedOriginRequestBase(
      MovingNotNull<RefPtr<QuotaManager>> aQuotaManager, const char* aName,
      const PrincipalMetadata& aPrincipalMetadata);

 private:
  RefPtr<BoolPromise> OpenDirectory() override;

  void CloseDirectory() override;
};

class PersistentOriginInitializedOp final
    : public InitializedOriginRequestBase {
 public:
  explicit PersistentOriginInitializedOp(
      MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
      const OriginMetadata& aOriginMetadata);

 private:
  ~PersistentOriginInitializedOp() = default;

  nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  bool UnwrapResolveValue() override;
};

class TemporaryOriginInitializedOp final : public InitializedOriginRequestBase {
  const PersistenceType mPersistenceType;

 public:
  explicit TemporaryOriginInitializedOp(
      MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
      const OriginMetadata& aOriginMetadata);

 private:
  ~TemporaryOriginInitializedOp() = default;

  nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  bool UnwrapResolveValue() override;
};

class InitOp final : public ResolvableNormalOriginOp<bool> {
  RefPtr<UniversalDirectoryLock> mDirectoryLock;

 public:
  InitOp(MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
         RefPtr<UniversalDirectoryLock> aDirectoryLock);

 private:
  ~InitOp() = default;

  RefPtr<BoolPromise> OpenDirectory() override;

  nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  bool UnwrapResolveValue() override;

  void CloseDirectory() override;
};

class InitializePersistentStorageOp final
    : public ResolvableNormalOriginOp<bool> {
  RefPtr<UniversalDirectoryLock> mDirectoryLock;

 public:
  InitializePersistentStorageOp(
      MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
      RefPtr<UniversalDirectoryLock> aDirectoryLock);

 private:
  ~InitializePersistentStorageOp() = default;

  RefPtr<BoolPromise> OpenDirectory() override;

  nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  bool UnwrapResolveValue() override;

  void CloseDirectory() override;
};

class InitTemporaryStorageOp final
    : public ResolvableNormalOriginOp<MaybePrincipalMetadataArray, true> {
  MaybePrincipalMetadataArray mAllTemporaryGroups;
  RefPtr<UniversalDirectoryLock> mDirectoryLock;

 public:
  InitTemporaryStorageOp(MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
                         RefPtr<UniversalDirectoryLock> aDirectoryLock);

 private:
  ~InitTemporaryStorageOp() = default;

  RefPtr<BoolPromise> OpenDirectory() override;

  nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  MaybePrincipalMetadataArray UnwrapResolveValue() override;

  void CloseDirectory() override;
};

class InitializeTemporaryGroupOp final : public ResolvableNormalOriginOp<bool> {
  const PrincipalMetadata mPrincipalMetadata;
  RefPtr<UniversalDirectoryLock> mDirectoryLock;

 public:
  InitializeTemporaryGroupOp(MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
                             const PrincipalMetadata& aPrincipalMetadata,
                             RefPtr<UniversalDirectoryLock> aDirectoryLock);

 private:
  ~InitializeTemporaryGroupOp() = default;

  RefPtr<BoolPromise> OpenDirectory() override;

  nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  bool UnwrapResolveValue() override;

  void CloseDirectory() override;
};

class InitializeOriginRequestBase : public ResolvableNormalOriginOp<bool> {
 protected:
  const PrincipalMetadata mPrincipalMetadata;
  RefPtr<UniversalDirectoryLock> mDirectoryLock;
  bool mCreated;

  InitializeOriginRequestBase(MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
                              const char* aName,
                              const PrincipalMetadata& aPrincipalMetadata,
                              RefPtr<UniversalDirectoryLock> aDirectoryLock);

 private:
  RefPtr<BoolPromise> OpenDirectory() override;

  void CloseDirectory() override;
};

class InitializePersistentOriginOp final : public InitializeOriginRequestBase {
 public:
  InitializePersistentOriginOp(
      MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
      const OriginMetadata& aOriginMetadata,
      RefPtr<UniversalDirectoryLock> aDirectoryLock);

 private:
  ~InitializePersistentOriginOp() = default;

  nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  bool UnwrapResolveValue() override;
};

class InitializeTemporaryOriginOp final : public InitializeOriginRequestBase {
  const PersistenceType mPersistenceType;
  const bool mCreateIfNonExistent;

 public:
  InitializeTemporaryOriginOp(MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
                              const OriginMetadata& aOriginMetadata,
                              bool aCreateIfNonExistent,
                              RefPtr<UniversalDirectoryLock> aDirectoryLock);

 private:
  ~InitializeTemporaryOriginOp() = default;

  nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  bool UnwrapResolveValue() override;
};

class InitializeClientBase : public ResolvableNormalOriginOp<bool> {
 protected:
  const PrincipalInfo mPrincipalInfo;
  ClientMetadata mClientMetadata;
  RefPtr<UniversalDirectoryLock> mDirectoryLock;
  const PersistenceType mPersistenceType;
  const Client::Type mClientType;
  bool mCreated;

  InitializeClientBase(MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
                       const char* aName, PersistenceType aPersistenceType,
                       const PrincipalInfo& aPrincipalInfo,
                       Client::Type aClientType);

  nsresult DoInit(QuotaManager& aQuotaManager) override;

 private:
  RefPtr<BoolPromise> OpenDirectory() override;

  void CloseDirectory() override;
};

class InitializePersistentClientOp : public InitializeClientBase {
 public:
  InitializePersistentClientOp(
      MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
      const PrincipalInfo& aPrincipalInfo, Client::Type aClientType);

 private:
  nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  bool UnwrapResolveValue() override;
};

class InitializeTemporaryClientOp : public InitializeClientBase {
 public:
  InitializeTemporaryClientOp(MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
                              PersistenceType aPersistenceType,
                              const PrincipalInfo& aPrincipalInfo,
                              Client::Type aClientType);

 private:
  nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  bool UnwrapResolveValue() override;
};

class GetFullOriginMetadataOp
    : public OpenStorageDirectoryHelper<QuotaRequestBase> {
  const GetFullOriginMetadataParams mParams;
  // XXX Consider wrapping with LazyInitializedOnce
  OriginMetadata mOriginMetadata;
  Maybe<FullOriginMetadata> mMaybeFullOriginMetadata;

 public:
  GetFullOriginMetadataOp(MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
                          const GetFullOriginMetadataParams& aParams);

 private:
  nsresult DoInit(QuotaManager& aQuotaManager) override;

  RefPtr<BoolPromise> OpenDirectory() override;

  nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  void GetResponse(RequestResponse& aResponse) override;

  void CloseDirectory() override;
};

class GetCachedOriginUsageOp
    : public OpenStorageDirectoryHelper<ResolvableNormalOriginOp<uint64_t>> {
  const PrincipalInfo mPrincipalInfo;
  PrincipalMetadata mPrincipalMetadata;
  uint64_t mUsage;

 public:
  GetCachedOriginUsageOp(MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
                         const PrincipalInfo& aPrincipalInfo);

 private:
  ~GetCachedOriginUsageOp() = default;

  nsresult DoInit(QuotaManager& aQuotaManager) override;

  RefPtr<BoolPromise> OpenDirectory() override;

  nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  uint64_t UnwrapResolveValue() override;

  void CloseDirectory() override;
};

class ListCachedOriginsOp final
    : public OpenStorageDirectoryHelper<
          ResolvableNormalOriginOp<CStringArray, /* IsExclusive */ true>> {
  nsTArray<nsCString> mOrigins;

 public:
  explicit ListCachedOriginsOp(
      MovingNotNull<RefPtr<QuotaManager>> aQuotaManager);

 private:
  ~ListCachedOriginsOp() = default;

  RefPtr<BoolPromise> OpenDirectory() override;

  nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  CStringArray UnwrapResolveValue() override;

  void CloseDirectory() override;
};

class ClearStorageOp final
    : public OpenStorageDirectoryHelper<ResolvableNormalOriginOp<bool>> {
 public:
  explicit ClearStorageOp(MovingNotNull<RefPtr<QuotaManager>> aQuotaManager);

 private:
  ~ClearStorageOp() = default;

  void DeleteFiles(QuotaManager& aQuotaManager);

  void DeleteStorageFile(QuotaManager& aQuotaManager);

  RefPtr<BoolPromise> OpenDirectory() override;

  virtual nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  bool UnwrapResolveValue() override;

  void CloseDirectory() override;
};

class ClearRequestBase
    : public OpenStorageDirectoryHelper<
          ResolvableNormalOriginOp<OriginMetadataArray, true>> {
  Atomic<uint64_t> mIterations;

 protected:
  OriginMetadataArray mOriginMetadataArray;

  ClearRequestBase(MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
                   const char* aName)
      : OpenStorageDirectoryHelper(std::move(aQuotaManager), aName),
        mIterations(0) {
    AssertIsOnOwningThread();
  }

  void DeleteFiles(QuotaManager& aQuotaManager,
                   const OriginMetadata& aOriginMetadata);

  void DeleteFiles(QuotaManager& aQuotaManager,
                   PersistenceType aPersistenceType,
                   const OriginScope& aOriginScope);

 private:
  template <typename FileCollector>
  void DeleteFilesInternal(QuotaManager& aQuotaManager,
                           PersistenceType aPersistenceType,
                           const OriginScope& aOriginScope,
                           const FileCollector& aFileCollector);

  void DoStringify(nsACString& aData) override {
    aData.Append("ClearRequestBase "_ns +
                 //
                 kStringifyStartInstance +
                 //
                 "Iterations:"_ns +
                 IntToCString(static_cast<uint64_t>(mIterations)) +
                 //
                 kStringifyEndInstance);
  }
};

class ClearOriginOp final : public ClearRequestBase {
  const PrincipalInfo mPrincipalInfo;
  PrincipalMetadata mPrincipalMetadata;
  const PersistenceScope mPersistenceScope;

 public:
  ClearOriginOp(MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
                const mozilla::Maybe<PersistenceType>& aPersistenceType,
                const PrincipalInfo& aPrincipalInfo);

 private:
  ~ClearOriginOp() = default;

  nsresult DoInit(QuotaManager& aQuotaManager) override;

  RefPtr<BoolPromise> OpenDirectory() override;

  nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  OriginMetadataArray UnwrapResolveValue() override;

  void CloseDirectory() override;
};

class ClearClientOp final
    : public OpenStorageDirectoryHelper<ResolvableNormalOriginOp<bool>> {
  const PrincipalInfo mPrincipalInfo;
  PrincipalMetadata mPrincipalMetadata;
  const PersistenceScope mPersistenceScope;
  const Client::Type mClientType;

 public:
  ClearClientOp(MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
                mozilla::Maybe<PersistenceType> aPersistenceType,
                const PrincipalInfo& aPrincipalInfo,
                const Client::Type aClientType);

 private:
  ~ClearClientOp() = default;

  nsresult DoInit(QuotaManager& aQuotaManager) override;

  RefPtr<BoolPromise> OpenDirectory() override;

  void DeleteFiles(const ClientMetadata& aClientMetadata);

  nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  bool UnwrapResolveValue() override;

  void CloseDirectory() override;
};

class ClearStoragesForOriginPrefixOp final
    : public OpenStorageDirectoryHelper<ClearRequestBase> {
  const PrincipalInfo mPrincipalInfo;
  PrincipalMetadata mPrincipalMetadata;
  const PersistenceScope mPersistenceScope;

 public:
  ClearStoragesForOriginPrefixOp(
      MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
      const Maybe<PersistenceType>& aPersistenceType,
      const PrincipalInfo& aPrincipalInfo);

 private:
  ~ClearStoragesForOriginPrefixOp() = default;

  nsresult DoInit(QuotaManager& aQuotaManager) override;

  RefPtr<BoolPromise> OpenDirectory() override;

  nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  OriginMetadataArray UnwrapResolveValue() override;

  void CloseDirectory() override;
};

class ClearDataOp final : public ClearRequestBase {
  const OriginAttributesPattern mPattern;

 public:
  ClearDataOp(MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
              const OriginAttributesPattern& aPattern);

 private:
  ~ClearDataOp() = default;

  RefPtr<BoolPromise> OpenDirectory() override;

  nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  OriginMetadataArray UnwrapResolveValue() override;

  void CloseDirectory() override;
};

class ShutdownOriginOp final
    : public ResolvableNormalOriginOp<OriginMetadataArray, true> {
  const PrincipalInfo mPrincipalInfo;
  PrincipalMetadata mPrincipalMetadata;
  OriginMetadataArray mOriginMetadataArray;
  RefPtr<UniversalDirectoryLock> mDirectoryLock;
  const PersistenceScope mPersistenceScope;

 public:
  ShutdownOriginOp(MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
                   mozilla::Maybe<PersistenceType> aPersistenceType,
                   const PrincipalInfo& aPrincipalInfo);

 private:
  ~ShutdownOriginOp() = default;

  nsresult DoInit(QuotaManager& aQuotaManager) override;

  RefPtr<BoolPromise> OpenDirectory() override;

  void CollectOriginMetadata(const OriginMetadata& aOriginMetadata);

  nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  OriginMetadataArray UnwrapResolveValue() override;

  void CloseDirectory() override;
};

class ShutdownClientOp final : public ResolvableNormalOriginOp<bool> {
  const PrincipalInfo mPrincipalInfo;
  PrincipalMetadata mPrincipalMetadata;
  RefPtr<UniversalDirectoryLock> mDirectoryLock;
  const PersistenceScope mPersistenceScope;
  const Client::Type mClientType;

 public:
  ShutdownClientOp(MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
                   mozilla::Maybe<PersistenceType> aPersistenceType,
                   const PrincipalInfo& aPrincipalInfo,
                   const Client::Type aClientType);

 private:
  ~ShutdownClientOp() = default;

  nsresult DoInit(QuotaManager& aQuotaManager) override;

  RefPtr<BoolPromise> OpenDirectory() override;

  nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  bool UnwrapResolveValue() override;

  void CloseDirectory() override;
};

class PersistRequestBase : public OpenStorageDirectoryHelper<QuotaRequestBase> {
  const PrincipalInfo mPrincipalInfo;

 protected:
  PrincipalMetadata mPrincipalMetadata;

 protected:
  PersistRequestBase(MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
                     const PrincipalInfo& aPrincipalInfo);

  nsresult DoInit(QuotaManager& aQuotaManager) override;

 private:
  RefPtr<BoolPromise> OpenDirectory() override;

  void CloseDirectory() override;
};

class PersistedOp final : public PersistRequestBase {
  bool mPersisted;

 public:
  PersistedOp(MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
              const RequestParams& aParams);

 private:
  ~PersistedOp() = default;

  nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  void GetResponse(RequestResponse& aResponse) override;
};

class PersistOp final : public PersistRequestBase {
 public:
  PersistOp(MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
            const RequestParams& aParams);

 private:
  ~PersistOp() = default;

  nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  void GetResponse(RequestResponse& aResponse) override;
};

class EstimateOp final : public OpenStorageDirectoryHelper<QuotaRequestBase> {
  const EstimateParams mParams;
  OriginMetadata mOriginMetadata;
  std::pair<uint64_t, uint64_t> mUsageAndLimit;

 public:
  EstimateOp(MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
             const EstimateParams& aParams);

 private:
  ~EstimateOp() = default;

  nsresult DoInit(QuotaManager& aQuotaManager) override;

  RefPtr<BoolPromise> OpenDirectory() override;

  virtual nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  void GetResponse(RequestResponse& aResponse) override;

  void CloseDirectory() override;
};

class ListOriginsOp final : public OpenStorageDirectoryHelper<QuotaRequestBase>,
                            public TraverseRepositoryHelper {
  // XXX Bug 1521541 will make each origin has it's own state.
  nsTArray<nsCString> mOrigins;

 public:
  explicit ListOriginsOp(MovingNotNull<RefPtr<QuotaManager>> aQuotaManager);

 private:
  ~ListOriginsOp() = default;

  RefPtr<BoolPromise> OpenDirectory() override;

  nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  const Atomic<bool>& GetIsCanceledFlag() override;

  nsresult ProcessOrigin(QuotaManager& aQuotaManager, nsIFile& aOriginDir,
                         const bool aPersistent,
                         const PersistenceType aPersistenceType) override;

  void GetResponse(RequestResponse& aResponse) override;

  void CloseDirectory() override;
};

RefPtr<OriginOperationBase> CreateFinalizeOriginEvictionOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
    nsTArray<RefPtr<OriginDirectoryLock>>&& aLocks) {
  return MakeRefPtr<FinalizeOriginEvictionOp>(std::move(aQuotaManager),
                                              std::move(aLocks));
}

RefPtr<NormalOriginOperationBase> CreateSaveOriginAccessTimeOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
    const OriginMetadata& aOriginMetadata, int64_t aTimestamp) {
  return MakeRefPtr<SaveOriginAccessTimeOp>(std::move(aQuotaManager),
                                            aOriginMetadata, aTimestamp);
}

RefPtr<ResolvableNormalOriginOp<bool>> CreateClearPrivateRepositoryOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager) {
  return MakeRefPtr<ClearPrivateRepositoryOp>(std::move(aQuotaManager));
}

RefPtr<ResolvableNormalOriginOp<bool>> CreateShutdownStorageOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager) {
  return MakeRefPtr<ShutdownStorageOp>(std::move(aQuotaManager));
}

RefPtr<ResolvableNormalOriginOp<OriginUsageMetadataArray, true>>
CreateGetUsageOp(MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
                 bool aGetAll) {
  return MakeRefPtr<GetUsageOp>(std::move(aQuotaManager), aGetAll);
}

RefPtr<ResolvableNormalOriginOp<UsageInfo>> CreateGetOriginUsageOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
    const mozilla::ipc::PrincipalInfo& aPrincipalInfo) {
  return MakeRefPtr<GetOriginUsageOp>(std::move(aQuotaManager), aPrincipalInfo);
}

RefPtr<QuotaRequestBase> CreateStorageNameOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager) {
  return MakeRefPtr<StorageNameOp>(std::move(aQuotaManager));
}

RefPtr<ResolvableNormalOriginOp<bool>> CreateStorageInitializedOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager) {
  return MakeRefPtr<StorageInitializedOp>(std::move(aQuotaManager));
}

RefPtr<ResolvableNormalOriginOp<bool>> CreatePersistentStorageInitializedOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager) {
  return MakeRefPtr<PersistentStorageInitializedOp>(std::move(aQuotaManager));
}

RefPtr<ResolvableNormalOriginOp<bool>> CreateTemporaryStorageInitializedOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager) {
  return MakeRefPtr<TemporaryStorageInitializedOp>(std::move(aQuotaManager));
}

RefPtr<ResolvableNormalOriginOp<bool>> CreateTemporaryGroupInitializedOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
    const mozilla::ipc::PrincipalInfo& aPrincipalInfo) {
  return MakeRefPtr<TemporaryGroupInitializedOp>(std::move(aQuotaManager),
                                                 aPrincipalInfo);
}

RefPtr<ResolvableNormalOriginOp<bool>> CreatePersistentOriginInitializedOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
    const OriginMetadata& aOriginMetadata) {
  return MakeRefPtr<PersistentOriginInitializedOp>(std::move(aQuotaManager),
                                                   aOriginMetadata);
}

RefPtr<ResolvableNormalOriginOp<bool>> CreateTemporaryOriginInitializedOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
    const OriginMetadata& aOriginMetadata) {
  return MakeRefPtr<TemporaryOriginInitializedOp>(std::move(aQuotaManager),
                                                  aOriginMetadata);
}

RefPtr<ResolvableNormalOriginOp<bool>> CreateInitOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
    RefPtr<UniversalDirectoryLock> aDirectoryLock) {
  return MakeRefPtr<InitOp>(std::move(aQuotaManager),
                            std::move(aDirectoryLock));
}

RefPtr<ResolvableNormalOriginOp<bool>> CreateInitializePersistentStorageOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
    RefPtr<UniversalDirectoryLock> aDirectoryLock) {
  return MakeRefPtr<InitializePersistentStorageOp>(std::move(aQuotaManager),
                                                   std::move(aDirectoryLock));
}

RefPtr<ResolvableNormalOriginOp<MaybePrincipalMetadataArray, true>>
CreateInitTemporaryStorageOp(MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
                             RefPtr<UniversalDirectoryLock> aDirectoryLock) {
  return MakeRefPtr<InitTemporaryStorageOp>(std::move(aQuotaManager),
                                            std::move(aDirectoryLock));
}

RefPtr<ResolvableNormalOriginOp<bool>> CreateInitializeTemporaryGroupOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
    const PrincipalMetadata& aPrincipalMetadata,
    RefPtr<UniversalDirectoryLock> aDirectoryLock) {
  return MakeRefPtr<InitializeTemporaryGroupOp>(
      std::move(aQuotaManager), aPrincipalMetadata, std::move(aDirectoryLock));
}

RefPtr<ResolvableNormalOriginOp<bool>> CreateInitializePersistentOriginOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
    const OriginMetadata& aOriginMetadata,
    RefPtr<UniversalDirectoryLock> aDirectoryLock) {
  return MakeRefPtr<InitializePersistentOriginOp>(
      std::move(aQuotaManager), aOriginMetadata, std::move(aDirectoryLock));
}

RefPtr<ResolvableNormalOriginOp<bool>> CreateInitializeTemporaryOriginOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
    const OriginMetadata& aOriginMetadata, bool aCreateIfNonExistent,
    RefPtr<UniversalDirectoryLock> aDirectoryLock) {
  return MakeRefPtr<InitializeTemporaryOriginOp>(
      std::move(aQuotaManager), aOriginMetadata, aCreateIfNonExistent,
      std::move(aDirectoryLock));
}

RefPtr<ResolvableNormalOriginOp<bool>> CreateInitializePersistentClientOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
    const mozilla::ipc::PrincipalInfo& aPrincipalInfo,
    const Client::Type aClientType) {
  return MakeRefPtr<InitializePersistentClientOp>(std::move(aQuotaManager),
                                                  aPrincipalInfo, aClientType);
}

RefPtr<ResolvableNormalOriginOp<bool>> CreateInitializeTemporaryClientOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
    const PersistenceType aPersistenceType, const PrincipalInfo& aPrincipalInfo,
    const Client::Type aClientType) {
  return MakeRefPtr<InitializeTemporaryClientOp>(
      std::move(aQuotaManager), aPersistenceType, aPrincipalInfo, aClientType);
}

RefPtr<QuotaRequestBase> CreateGetFullOriginMetadataOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
    const GetFullOriginMetadataParams& aParams) {
  return MakeRefPtr<GetFullOriginMetadataOp>(std::move(aQuotaManager), aParams);
}

RefPtr<ResolvableNormalOriginOp<uint64_t>> CreateGetCachedOriginUsageOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
    const mozilla::ipc::PrincipalInfo& aPrincipalInfo) {
  return MakeRefPtr<GetCachedOriginUsageOp>(std::move(aQuotaManager),
                                            aPrincipalInfo);
}

RefPtr<ResolvableNormalOriginOp<CStringArray, true>> CreateListCachedOriginsOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager) {
  return MakeRefPtr<ListCachedOriginsOp>(std::move(aQuotaManager));
}

RefPtr<ResolvableNormalOriginOp<bool>> CreateClearStorageOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager) {
  return MakeRefPtr<ClearStorageOp>(std::move(aQuotaManager));
}

RefPtr<ResolvableNormalOriginOp<OriginMetadataArray, true>> CreateClearOriginOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
    const Maybe<PersistenceType>& aPersistenceType,
    const PrincipalInfo& aPrincipalInfo) {
  return MakeRefPtr<ClearOriginOp>(std::move(aQuotaManager), aPersistenceType,
                                   aPrincipalInfo);
}

RefPtr<ResolvableNormalOriginOp<bool>> CreateClearClientOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
    Maybe<PersistenceType> aPersistenceType,
    const PrincipalInfo& aPrincipalInfo, Client::Type aClientType) {
  return MakeRefPtr<ClearClientOp>(std::move(aQuotaManager), aPersistenceType,
                                   aPrincipalInfo, aClientType);
}

RefPtr<ResolvableNormalOriginOp<OriginMetadataArray, true>>
CreateClearStoragesForOriginPrefixOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
    const Maybe<PersistenceType>& aPersistenceType,
    const PrincipalInfo& aPrincipalInfo) {
  return MakeRefPtr<ClearStoragesForOriginPrefixOp>(
      std::move(aQuotaManager), aPersistenceType, aPrincipalInfo);
}

RefPtr<ResolvableNormalOriginOp<OriginMetadataArray, true>> CreateClearDataOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
    const OriginAttributesPattern& aPattern) {
  return MakeRefPtr<ClearDataOp>(std::move(aQuotaManager), aPattern);
}

RefPtr<ResolvableNormalOriginOp<OriginMetadataArray, true>>
CreateShutdownOriginOp(MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
                       Maybe<PersistenceType> aPersistenceType,
                       const mozilla::ipc::PrincipalInfo& aPrincipalInfo) {
  return MakeRefPtr<ShutdownOriginOp>(std::move(aQuotaManager),
                                      aPersistenceType, aPrincipalInfo);
}

RefPtr<ResolvableNormalOriginOp<bool>> CreateShutdownClientOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
    Maybe<PersistenceType> aPersistenceType,
    const PrincipalInfo& aPrincipalInfo, Client::Type aClientType) {
  return MakeRefPtr<ShutdownClientOp>(
      std::move(aQuotaManager), aPersistenceType, aPrincipalInfo, aClientType);
}

RefPtr<QuotaRequestBase> CreatePersistedOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
    const RequestParams& aParams) {
  return MakeRefPtr<PersistedOp>(std::move(aQuotaManager), aParams);
}

RefPtr<QuotaRequestBase> CreatePersistOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
    const RequestParams& aParams) {
  return MakeRefPtr<PersistOp>(std::move(aQuotaManager), aParams);
}

RefPtr<QuotaRequestBase> CreateEstimateOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
    const EstimateParams& aParams) {
  return MakeRefPtr<EstimateOp>(std::move(aQuotaManager), aParams);
}

RefPtr<QuotaRequestBase> CreateListOriginsOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager) {
  return MakeRefPtr<ListOriginsOp>(std::move(aQuotaManager));
}

template <class Base>
RefPtr<BoolPromise> OpenStorageDirectoryHelper<Base>::OpenStorageDirectory(
    const PersistenceScope& aPersistenceScope, const OriginScope& aOriginScope,
    const Nullable<Client::Type>& aClientType, bool aExclusive,
    bool aInitializeOrigins, const DirectoryLockCategory aCategory) {
  return Base::mQuotaManager
      ->OpenStorageDirectory(aPersistenceScope, aOriginScope, aClientType,
                             aExclusive, aInitializeOrigins, aCategory)
      ->Then(GetCurrentSerialEventTarget(), __func__,
             [self = RefPtr(this)](
                 UniversalDirectoryLockPromise::ResolveOrRejectValue&& aValue) {
               if (aValue.IsReject()) {
                 return BoolPromise::CreateAndReject(aValue.RejectValue(),
                                                     __func__);
               }

               self->mDirectoryLock = std::move(aValue.ResolveValue());

               return BoolPromise::CreateAndResolve(true, __func__);
             });
}

RefPtr<BoolPromise> FinalizeOriginEvictionOp::Open() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(!mLocks.IsEmpty());

  return BoolPromise::CreateAndResolve(true, __func__);
}

nsresult FinalizeOriginEvictionOp::DoDirectoryWork(
    QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();

  AUTO_PROFILER_LABEL("FinalizeOriginEvictionOp::DoDirectoryWork", OTHER);

  for (const auto& lock : mLocks) {
    aQuotaManager.OriginClearCompleted(lock->OriginMetadata(),
                                       Nullable<Client::Type>());
  }

  return NS_OK;
}

void FinalizeOriginEvictionOp::UnblockOpen() {
  AssertIsOnOwningThread();

  nsTArray<OriginMetadata> origins;

  std::transform(mLocks.cbegin(), mLocks.cend(), MakeBackInserter(origins),
                 [](const auto& lock) { return lock->OriginMetadata(); });

  MOZ_ALWAYS_SUCCEEDS(NS_DispatchToCurrentThread(NS_NewRunnableFunction(
      "dom::quota::FinalizeOriginEvictionOp::UnblockOpen",
      [quotaManager = mQuotaManager, origins = std::move(origins)]() {
        quotaManager->NoteUninitializedOrigins(origins);
      })));

  for (const auto& lock : mLocks) {
    lock->Drop();
  }
  mLocks.Clear();
}

RefPtr<BoolPromise> SaveOriginAccessTimeOp::OpenDirectory() {
  AssertIsOnOwningThread();

  return OpenStorageDirectory(
      PersistenceScope::CreateFromValue(mOriginMetadata.mPersistenceType),
      OriginScope::FromOrigin(mOriginMetadata), Nullable<Client::Type>(),
      /* aExclusive */ false);
}

nsresult SaveOriginAccessTimeOp::DoDirectoryWork(QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();
  aQuotaManager.AssertStorageIsInitializedInternal();

  AUTO_PROFILER_LABEL("SaveOriginAccessTimeOp::DoDirectoryWork", OTHER);

  QM_TRY(MOZ_TO_RESULT(!QuotaManager::IsShuttingDown()), NS_ERROR_ABORT);

  QM_TRY_INSPECT(const auto& file,
                 aQuotaManager.GetOriginDirectory(mOriginMetadata));

  // The origin directory might not exist
  // anymore, because it was deleted by a clear operation.
  QM_TRY_INSPECT(const bool& exists, MOZ_TO_RESULT_INVOKE_MEMBER(file, Exists));

  if (exists) {
    QM_TRY(MOZ_TO_RESULT(file->Append(nsLiteralString(METADATA_V2_FILE_NAME))));

    QM_TRY_INSPECT(const auto& stream,
                   GetBinaryOutputStream(*file, FileFlag::Update));
    MOZ_ASSERT(stream);

    QM_TRY(MOZ_TO_RESULT(stream->Write64(mTimestamp)));
  }

  return NS_OK;
}

void SaveOriginAccessTimeOp::SendResults() {}

void SaveOriginAccessTimeOp::CloseDirectory() {
  AssertIsOnOwningThread();

  SafeDropDirectoryLock(mDirectoryLock);
}

RefPtr<BoolPromise> ClearPrivateRepositoryOp::OpenDirectory() {
  AssertIsOnOwningThread();

  return OpenStorageDirectory(
      PersistenceScope::CreateFromValue(PERSISTENCE_TYPE_PRIVATE),
      OriginScope::FromNull(), Nullable<Client::Type>(),
      /* aExclusive */ true, /* aInitializeOrigins */ false,
      DirectoryLockCategory::UninitOrigins);
}

nsresult ClearPrivateRepositoryOp::DoDirectoryWork(
    QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();
  aQuotaManager.AssertStorageIsInitializedInternal();

  AUTO_PROFILER_LABEL("ClearPrivateRepositoryOp::DoDirectoryWork", OTHER);

  QM_TRY_INSPECT(
      const auto& directory,
      QM_NewLocalFile(aQuotaManager.GetStoragePath(PERSISTENCE_TYPE_PRIVATE)));

  nsresult rv = directory->Remove(true);
  if (rv != NS_ERROR_FILE_NOT_FOUND && NS_FAILED(rv)) {
    // This should never fail if we've closed all storage connections
    // correctly...
    MOZ_ASSERT(false, "Failed to remove directory!");
  }

  aQuotaManager.RemoveQuotaForRepository(PERSISTENCE_TYPE_PRIVATE);

  aQuotaManager.RepositoryClearCompleted(PERSISTENCE_TYPE_PRIVATE);

  return NS_OK;
}

void ClearPrivateRepositoryOp::CloseDirectory() {
  AssertIsOnOwningThread();

  SafeDropDirectoryLock(mDirectoryLock);
}

RefPtr<BoolPromise> ShutdownStorageOp::OpenDirectory() {
  AssertIsOnOwningThread();

  // Clear directory lock tables (which also saves origin access time) before
  // acquiring the exclusive lock below. Otherwise, saving of origin access
  // time would be scheduled after storage shutdown and that would initialize
  // storage again in the end.
  mQuotaManager->ClearDirectoryLockTables();

  mDirectoryLock = mQuotaManager->CreateDirectoryLockInternal(
      PersistenceScope::CreateFromNull(), OriginScope::FromNull(),
      Nullable<Client::Type>(),
      /* aExclusive */ true, DirectoryLockCategory::UninitStorage);

  return mDirectoryLock->Acquire();
}

#ifdef DEBUG
nsresult ShutdownStorageOp::DirectoryOpen() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mDirectoryLock);
  mDirectoryLock->AssertIsAcquiredExclusively();

  return NormalOriginOperationBase::DirectoryOpen();
}
#endif

nsresult ShutdownStorageOp::DoDirectoryWork(QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();

  AUTO_PROFILER_LABEL("ShutdownStorageOp::DoDirectoryWork", OTHER);

  aQuotaManager.MaybeRecordQuotaManagerShutdownStep(
      "ShutdownStorageOp::DoDirectoryWork -> ShutdownStorageInternal."_ns);

  aQuotaManager.ShutdownStorageInternal();

  return NS_OK;
}

void ShutdownStorageOp::CloseDirectory() {
  AssertIsOnOwningThread();

  DropDirectoryLockIfNotDropped(mDirectoryLock);
}

nsresult TraverseRepositoryHelper::TraverseRepository(
    QuotaManager& aQuotaManager, PersistenceType aPersistenceType) {
  AssertIsOnIOThread();

  QM_TRY_INSPECT(
      const auto& directory,
      QM_NewLocalFile(aQuotaManager.GetStoragePath(aPersistenceType)));

  QM_TRY_INSPECT(const bool& exists,
                 MOZ_TO_RESULT_INVOKE_MEMBER(directory, Exists));

  if (!exists) {
    return NS_OK;
  }

  QM_TRY(CollectEachFileAtomicCancelable(
      *directory, GetIsCanceledFlag(),
      [this, aPersistenceType, &aQuotaManager,
       persistent = aPersistenceType == PERSISTENCE_TYPE_PERSISTENT](
          const nsCOMPtr<nsIFile>& originDir) -> Result<Ok, nsresult> {
        QM_TRY_INSPECT(const auto& dirEntryKind, GetDirEntryKind(*originDir));

        switch (dirEntryKind) {
          case nsIFileKind::ExistsAsDirectory:
            QM_TRY(MOZ_TO_RESULT(ProcessOrigin(aQuotaManager, *originDir,
                                               persistent, aPersistenceType)));
            break;

          case nsIFileKind::ExistsAsFile: {
            QM_TRY_INSPECT(const auto& leafName,
                           MOZ_TO_RESULT_INVOKE_MEMBER_TYPED(
                               nsAutoString, originDir, GetLeafName));

            // Unknown files during getting usages are allowed. Just warn if we
            // find them.
            if (!IsOSMetadata(leafName)) {
              UNKNOWN_FILE_WARNING(leafName);
            }

            break;
          }

          case nsIFileKind::DoesNotExist:
            // Ignore files that got removed externally while iterating.
            break;
        }

        return Ok{};
      }));

  return NS_OK;
}

Result<UsageInfo, nsresult> OriginUsageHelper::GetUsageForOrigin(
    QuotaManager& aQuotaManager, PersistenceType aPersistenceType,
    const OriginMetadata& aOriginMetadata) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aOriginMetadata.mPersistenceType == aPersistenceType);

  QM_TRY_INSPECT(const auto& directory,
                 aQuotaManager.GetOriginDirectory(aOriginMetadata));

  QM_TRY_INSPECT(const bool& exists,
                 MOZ_TO_RESULT_INVOKE_MEMBER(directory, Exists));

  if (!exists || GetIsCanceledFlag()) {
    return UsageInfo();
  }

  // If the directory exists then enumerate all the files inside, adding up
  // the sizes to get the final usage statistic.
  bool initialized;

  if (aPersistenceType == PERSISTENCE_TYPE_PERSISTENT) {
    initialized = aQuotaManager.IsPersistentOriginInitializedInternal(
        aOriginMetadata.mOrigin);
  } else {
    initialized = aQuotaManager.IsTemporaryStorageInitializedInternal();
  }

  return GetUsageForOriginEntries(aQuotaManager, aPersistenceType,
                                  aOriginMetadata, *directory, initialized);
}

Result<UsageInfo, nsresult> OriginUsageHelper::GetUsageForOriginEntries(
    QuotaManager& aQuotaManager, PersistenceType aPersistenceType,
    const OriginMetadata& aOriginMetadata, nsIFile& aDirectory,
    const bool aInitialized) {
  AssertIsOnIOThread();

  QM_TRY_RETURN((ReduceEachFileAtomicCancelable(
      aDirectory, GetIsCanceledFlag(), UsageInfo{},
      [&](UsageInfo oldUsageInfo, const nsCOMPtr<nsIFile>& file)
          -> mozilla::Result<UsageInfo, nsresult> {
        QM_TRY_INSPECT(
            const auto& leafName,
            MOZ_TO_RESULT_INVOKE_MEMBER_TYPED(nsAutoString, file, GetLeafName));

        QM_TRY_INSPECT(const auto& dirEntryKind, GetDirEntryKind(*file));

        switch (dirEntryKind) {
          case nsIFileKind::ExistsAsDirectory: {
            Client::Type clientType;
            const bool ok =
                Client::TypeFromText(leafName, clientType, fallible);
            if (!ok) {
              // Unknown directories during getting usage for an origin (even
              // for an uninitialized origin) are now allowed. Just warn if we
              // find them.
              UNKNOWN_FILE_WARNING(leafName);
              break;
            }

            Client* const client = aQuotaManager.GetClient(clientType);
            MOZ_ASSERT(client);

            QM_TRY_INSPECT(const auto& usageInfo,
                           aInitialized ? client->GetUsageForOrigin(
                                              aPersistenceType, aOriginMetadata,
                                              GetIsCanceledFlag())
                                        : client->InitOrigin(
                                              aPersistenceType, aOriginMetadata,
                                              GetIsCanceledFlag()));
            return oldUsageInfo + usageInfo;
          }

          case nsIFileKind::ExistsAsFile:
            // We are maintaining existing behavior for unknown files here (just
            // continuing).
            // This can possibly be used by developers to add temporary backups
            // into origin directories without losing get usage functionality.
            if (IsTempMetadata(leafName)) {
              if (!aInitialized) {
                QM_TRY(MOZ_TO_RESULT(file->Remove(/* recursive */ false)));
              }

              break;
            }

            if (IsOriginMetadata(leafName) || IsOSMetadata(leafName) ||
                IsDotFile(leafName)) {
              break;
            }

            // Unknown files during getting usage for an origin (even for an
            // uninitialized origin) are now allowed. Just warn if we find them.
            UNKNOWN_FILE_WARNING(leafName);
            break;

          case nsIFileKind::DoesNotExist:
            // Ignore files that got removed externally while iterating.
            break;
        }

        return oldUsageInfo;
      })));
}

GetUsageOp::GetUsageOp(MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
                       bool aGetAll)
    : OpenStorageDirectoryHelper(std::move(aQuotaManager),
                                 "dom::quota::GetUsageOp"),
      mGetAll(aGetAll) {
  AssertIsOnOwningThread();
}

void GetUsageOp::ProcessOriginInternal(QuotaManager* aQuotaManager,
                                       const PersistenceType aPersistenceType,
                                       const nsACString& aOrigin,
                                       const int64_t aTimestamp,
                                       const bool aPersisted,
                                       const uint64_t aUsage) {
  if (!mGetAll && aQuotaManager->IsOriginInternal(aOrigin)) {
    return;
  }

  // We can't store pointers to OriginUsage objects in the hashtable
  // since AppendElement() reallocates its internal array buffer as number
  // of elements grows.
  const auto& originUsage =
      mOriginUsagesIndex.WithEntryHandle(aOrigin, [&](auto&& entry) {
        if (entry) {
          return WrapNotNullUnchecked(&mOriginUsages[entry.Data()]);
        }

        entry.Insert(mOriginUsages.Length());

        OriginUsageMetadata metadata;
        metadata.mOrigin = aOrigin;
        metadata.mPersistenceType = PERSISTENCE_TYPE_DEFAULT;
        metadata.mPersisted = false;
        metadata.mLastAccessTime = 0;
        metadata.mUsage = 0;

        return mOriginUsages.EmplaceBack(std::move(metadata));
      });

  if (aPersistenceType == PERSISTENCE_TYPE_DEFAULT) {
    originUsage->mPersisted = aPersisted;
  }

  originUsage->mUsage = originUsage->mUsage + aUsage;

  originUsage->mLastAccessTime =
      std::max<int64_t>(originUsage->mLastAccessTime, aTimestamp);
}

const Atomic<bool>& GetUsageOp::GetIsCanceledFlag() {
  AssertIsOnIOThread();

  return Canceled();
}

// XXX Remove aPersistent
// XXX Remove aPersistenceType once GetUsageForOrigin uses the persistence
// type from OriginMetadata
nsresult GetUsageOp::ProcessOrigin(QuotaManager& aQuotaManager,
                                   nsIFile& aOriginDir, const bool aPersistent,
                                   const PersistenceType aPersistenceType) {
  AssertIsOnIOThread();

  QM_TRY_UNWRAP(auto maybeMetadata,
                QM_OR_ELSE_WARN_IF(
                    // Expression
                    aQuotaManager.LoadFullOriginMetadataWithRestore(&aOriginDir)
                        .map([](auto metadata) -> Maybe<FullOriginMetadata> {
                          return Some(std::move(metadata));
                        }),
                    // Predicate.
                    IsSpecificError<NS_ERROR_MALFORMED_URI>,
                    // Fallback.
                    ErrToDefaultOk<Maybe<FullOriginMetadata>>));

  if (!maybeMetadata) {
    // Unknown directories during getting usage are allowed. Just warn if we
    // find them.
    QM_TRY_INSPECT(const auto& leafName,
                   MOZ_TO_RESULT_INVOKE_MEMBER_TYPED(nsAutoString, aOriginDir,
                                                     GetLeafName));

    UNKNOWN_FILE_WARNING(leafName);
    return NS_OK;
  }

  auto metadata = maybeMetadata.extract();

  QM_TRY_INSPECT(const auto& usageInfo,
                 GetUsageForOrigin(aQuotaManager, aPersistenceType, metadata));

  ProcessOriginInternal(&aQuotaManager, aPersistenceType, metadata.mOrigin,
                        metadata.mLastAccessTime, metadata.mPersisted,
                        usageInfo.TotalUsage().valueOr(0));

  return NS_OK;
}

RefPtr<BoolPromise> GetUsageOp::OpenDirectory() {
  AssertIsOnOwningThread();

  return OpenStorageDirectory(PersistenceScope::CreateFromNull(),
                              OriginScope::FromNull(), Nullable<Client::Type>(),
                              /* aExclusive */ false);
}

nsresult GetUsageOp::DoDirectoryWork(QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();
  aQuotaManager.AssertStorageIsInitializedInternal();

  AUTO_PROFILER_LABEL("GetUsageOp::DoDirectoryWork", OTHER);

  nsresult rv;

  for (const PersistenceType type : kAllPersistenceTypes) {
    rv = TraverseRepository(aQuotaManager, type);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  // TraverseRepository above only consulted the filesystem. We also need to
  // consider origins which may have pending quota usage, such as buffered
  // LocalStorage writes for an origin which didn't previously have any
  // LocalStorage data.

  aQuotaManager.CollectPendingOriginsForListing(
      [this, &aQuotaManager](const auto& originInfo) {
        ProcessOriginInternal(
            &aQuotaManager, originInfo->GetGroupInfo()->GetPersistenceType(),
            originInfo->Origin(), originInfo->LockedAccessTime(),
            originInfo->LockedPersisted(), originInfo->LockedUsage());
      });

  return NS_OK;
}

OriginUsageMetadataArray GetUsageOp::UnwrapResolveValue() {
  AssertIsOnOwningThread();

  return std::move(mOriginUsages);
}

void GetUsageOp::CloseDirectory() {
  AssertIsOnOwningThread();

  SafeDropDirectoryLock(mDirectoryLock);
}

GetOriginUsageOp::GetOriginUsageOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
    const PrincipalInfo& aPrincipalInfo)
    : OpenStorageDirectoryHelper(std::move(aQuotaManager),
                                 "dom::quota::GetOriginUsageOp"),
      mPrincipalInfo(aPrincipalInfo) {
  AssertIsOnOwningThread();
}

nsresult GetOriginUsageOp::DoInit(QuotaManager& aQuotaManager) {
  AssertIsOnOwningThread();

  QM_TRY_UNWRAP(mPrincipalMetadata, GetInfoFromValidatedPrincipalInfo(
                                        aQuotaManager, mPrincipalInfo));

  mPrincipalMetadata.AssertInvariants();

  return NS_OK;
}

RefPtr<BoolPromise> GetOriginUsageOp::OpenDirectory() {
  AssertIsOnOwningThread();

  return OpenStorageDirectory(PersistenceScope::CreateFromNull(),
                              OriginScope::FromOrigin(mPrincipalMetadata),
                              Nullable<Client::Type>(),
                              /* aExclusive */ false);
}

const Atomic<bool>& GetOriginUsageOp::GetIsCanceledFlag() {
  AssertIsOnIOThread();

  return Canceled();
}

nsresult GetOriginUsageOp::DoDirectoryWork(QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();
  aQuotaManager.AssertStorageIsInitializedInternal();
  MOZ_ASSERT(mUsageInfo.TotalUsage().isNothing());

  AUTO_PROFILER_LABEL("GetOriginUsageOp::DoDirectoryWork", OTHER);

  // Add all the persistent/temporary/default/private storage files we care
  // about.
  for (const PersistenceType type : kAllPersistenceTypes) {
    const OriginMetadata originMetadata = {mPrincipalMetadata, type};

    auto usageInfoOrErr =
        GetUsageForOrigin(aQuotaManager, type, originMetadata);
    if (NS_WARN_IF(usageInfoOrErr.isErr())) {
      return usageInfoOrErr.unwrapErr();
    }

    mUsageInfo += usageInfoOrErr.unwrap();
  }

  return NS_OK;
}

UsageInfo GetOriginUsageOp::UnwrapResolveValue() {
  AssertIsOnOwningThread();

  return mUsageInfo;
}

void GetOriginUsageOp::CloseDirectory() {
  AssertIsOnOwningThread();

  SafeDropDirectoryLock(mDirectoryLock);
}

StorageNameOp::StorageNameOp(MovingNotNull<RefPtr<QuotaManager>> aQuotaManager)
    : QuotaRequestBase(std::move(aQuotaManager), "dom::quota::StorageNameOp") {
  AssertIsOnOwningThread();
}

RefPtr<BoolPromise> StorageNameOp::OpenDirectory() {
  AssertIsOnOwningThread();

  return BoolPromise::CreateAndResolve(true, __func__);
}

nsresult StorageNameOp::DoDirectoryWork(QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();

  AUTO_PROFILER_LABEL("StorageNameOp::DoDirectoryWork", OTHER);

  mName = aQuotaManager.GetStorageName();

  return NS_OK;
}

void StorageNameOp::GetResponse(RequestResponse& aResponse) {
  AssertIsOnOwningThread();

  StorageNameResponse storageNameResponse;

  storageNameResponse.name() = mName;

  aResponse = storageNameResponse;
}

void StorageNameOp::CloseDirectory() { AssertIsOnOwningThread(); }

InitializedRequestBase::InitializedRequestBase(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager, const char* aName)
    : ResolvableNormalOriginOp(std::move(aQuotaManager), aName),
      mInitialized(false) {
  AssertIsOnOwningThread();
}

RefPtr<BoolPromise> InitializedRequestBase::OpenDirectory() {
  AssertIsOnOwningThread();

  return BoolPromise::CreateAndResolve(true, __func__);
}

void InitializedRequestBase::CloseDirectory() { AssertIsOnOwningThread(); }

nsresult StorageInitializedOp::DoDirectoryWork(QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();

  AUTO_PROFILER_LABEL("StorageInitializedOp::DoDirectoryWork", OTHER);

  mInitialized = aQuotaManager.IsStorageInitializedInternal();

  return NS_OK;
}

bool StorageInitializedOp::UnwrapResolveValue() {
  AssertIsOnOwningThread();

  return mInitialized;
}

nsresult PersistentStorageInitializedOp::DoDirectoryWork(
    QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();

  AUTO_PROFILER_LABEL("PersistentStorageInitializedOp::DoDirectoryWork", OTHER);

  mInitialized = aQuotaManager.IsPersistentStorageInitializedInternal();

  return NS_OK;
}

bool PersistentStorageInitializedOp::UnwrapResolveValue() {
  AssertIsOnOwningThread();

  return mInitialized;
}

nsresult TemporaryStorageInitializedOp::DoDirectoryWork(
    QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();

  AUTO_PROFILER_LABEL("TemporaryStorageInitializedOp::DoDirectoryWork", OTHER);

  mInitialized = aQuotaManager.IsTemporaryStorageInitializedInternal();

  return NS_OK;
}

bool TemporaryStorageInitializedOp::UnwrapResolveValue() {
  AssertIsOnOwningThread();

  return mInitialized;
}

TemporaryGroupInitializedOp::TemporaryGroupInitializedOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
    const PrincipalInfo& aPrincipalInfo)
    : ResolvableNormalOriginOp(std::move(aQuotaManager),
                               "dom::quota::TemporaryGroupInitializedOp"),
      mPrincipalInfo(aPrincipalInfo),
      mInitialized(false) {
  AssertIsOnOwningThread();
}

nsresult TemporaryGroupInitializedOp::DoInit(QuotaManager& aQuotaManager) {
  AssertIsOnOwningThread();

  QM_TRY_UNWRAP(mPrincipalMetadata, GetInfoFromValidatedPrincipalInfo(
                                        aQuotaManager, mPrincipalInfo));

  mPrincipalMetadata.AssertInvariants();

  return NS_OK;
}

RefPtr<BoolPromise> TemporaryGroupInitializedOp::OpenDirectory() {
  AssertIsOnOwningThread();

  return BoolPromise::CreateAndResolve(true, __func__);
}

nsresult TemporaryGroupInitializedOp::DoDirectoryWork(
    QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();

  AUTO_PROFILER_LABEL("TemporaryGroupInitializedOp::DoDirectoryWork", OTHER);

  mInitialized =
      aQuotaManager.IsTemporaryGroupInitializedInternal(mPrincipalMetadata);

  return NS_OK;
}

bool TemporaryGroupInitializedOp::UnwrapResolveValue() {
  AssertIsOnOwningThread();

  return mInitialized;
}

void TemporaryGroupInitializedOp::CloseDirectory() { AssertIsOnOwningThread(); }

InitializedOriginRequestBase::InitializedOriginRequestBase(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager, const char* aName,
    const PrincipalMetadata& aPrincipalMetadata)
    : ResolvableNormalOriginOp(std::move(aQuotaManager), aName),
      mPrincipalMetadata(aPrincipalMetadata),
      mInitialized(false) {
  AssertIsOnOwningThread();
}

RefPtr<BoolPromise> InitializedOriginRequestBase::OpenDirectory() {
  AssertIsOnOwningThread();

  return BoolPromise::CreateAndResolve(true, __func__);
}

void InitializedOriginRequestBase::CloseDirectory() {
  AssertIsOnOwningThread();
}

PersistentOriginInitializedOp::PersistentOriginInitializedOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
    const OriginMetadata& aOriginMetadata)
    : InitializedOriginRequestBase(std::move(aQuotaManager),
                                   "dom::quota::PersistentOriginInitializedOp",
                                   aOriginMetadata) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aOriginMetadata.mPersistenceType == PERSISTENCE_TYPE_PERSISTENT);
}

nsresult PersistentOriginInitializedOp::DoDirectoryWork(
    QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();

  AUTO_PROFILER_LABEL("PersistentOriginInitializedOp::DoDirectoryWork", OTHER);

  mInitialized = aQuotaManager.IsPersistentOriginInitializedInternal(
      OriginMetadata{mPrincipalMetadata, PERSISTENCE_TYPE_PERSISTENT});

  return NS_OK;
}

bool PersistentOriginInitializedOp::UnwrapResolveValue() {
  AssertIsOnOwningThread();

  return mInitialized;
}

TemporaryOriginInitializedOp::TemporaryOriginInitializedOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
    const OriginMetadata& aOriginMetadata)
    : InitializedOriginRequestBase(std::move(aQuotaManager),
                                   "dom::quota::TemporaryOriginInitializedOp",
                                   aOriginMetadata),
      mPersistenceType(aOriginMetadata.mPersistenceType) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aOriginMetadata.mPersistenceType != PERSISTENCE_TYPE_PERSISTENT);
}

nsresult TemporaryOriginInitializedOp::DoDirectoryWork(
    QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();

  AUTO_PROFILER_LABEL("TemporaryOriginInitializedOp::DoDirectoryWork", OTHER);

  mInitialized = aQuotaManager.IsTemporaryOriginInitializedInternal(
      OriginMetadata{mPrincipalMetadata, mPersistenceType});

  return NS_OK;
}

bool TemporaryOriginInitializedOp::UnwrapResolveValue() {
  AssertIsOnOwningThread();

  return mInitialized;
}

InitOp::InitOp(MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
               RefPtr<UniversalDirectoryLock> aDirectoryLock)
    : ResolvableNormalOriginOp(std::move(aQuotaManager), "dom::quota::InitOp"),
      mDirectoryLock(std::move(aDirectoryLock)) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mDirectoryLock);
}

RefPtr<BoolPromise> InitOp::OpenDirectory() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mDirectoryLock);

  return BoolPromise::CreateAndResolve(true, __func__);
}

nsresult InitOp::DoDirectoryWork(QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();

  AUTO_PROFILER_LABEL("InitOp::DoDirectoryWork", OTHER);

  QM_TRY(MOZ_TO_RESULT(aQuotaManager.EnsureStorageIsInitializedInternal()));

  return NS_OK;
}

bool InitOp::UnwrapResolveValue() { return true; }

void InitOp::CloseDirectory() {
  AssertIsOnOwningThread();

  DropDirectoryLock(mDirectoryLock);
}

InitializePersistentStorageOp::InitializePersistentStorageOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
    RefPtr<UniversalDirectoryLock> aDirectoryLock)
    : ResolvableNormalOriginOp(std::move(aQuotaManager),
                               "dom::quota::InitializePersistentStorageOp"),
      mDirectoryLock(std::move(aDirectoryLock)) {
  AssertIsOnOwningThread();
}

RefPtr<BoolPromise> InitializePersistentStorageOp::OpenDirectory() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mDirectoryLock);

  return BoolPromise::CreateAndResolve(true, __func__);
}

nsresult InitializePersistentStorageOp::DoDirectoryWork(
    QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();

  AUTO_PROFILER_LABEL("InitializePersistentStorageOp::DoDirectoryWork", OTHER);

  QM_TRY(OkIf(aQuotaManager.IsStorageInitializedInternal()),
         NS_ERROR_NOT_INITIALIZED);

  QM_TRY(MOZ_TO_RESULT(
      aQuotaManager.EnsurePersistentStorageIsInitializedInternal()));

  return NS_OK;
}

bool InitializePersistentStorageOp::UnwrapResolveValue() {
  AssertIsOnOwningThread();

  return true;
}

void InitializePersistentStorageOp::CloseDirectory() {
  AssertIsOnOwningThread();

  DropDirectoryLock(mDirectoryLock);
}

InitTemporaryStorageOp::InitTemporaryStorageOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
    RefPtr<UniversalDirectoryLock> aDirectoryLock)
    : ResolvableNormalOriginOp(std::move(aQuotaManager),
                               "dom::quota::InitTemporaryStorageOp"),
      mDirectoryLock(std::move(aDirectoryLock)) {
  AssertIsOnOwningThread();
}

RefPtr<BoolPromise> InitTemporaryStorageOp::OpenDirectory() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mDirectoryLock);

  return BoolPromise::CreateAndResolve(true, __func__);
}

nsresult InitTemporaryStorageOp::DoDirectoryWork(QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();

  AUTO_PROFILER_LABEL("InitTemporaryStorageOp::DoDirectoryWork", OTHER);

  QM_TRY(OkIf(aQuotaManager.IsStorageInitializedInternal()),
         NS_ERROR_NOT_INITIALIZED);

  const bool wasInitialized =
      aQuotaManager.IsTemporaryStorageInitializedInternal();

  if (!wasInitialized) {
    QM_TRY(MOZ_TO_RESULT(
        aQuotaManager.EnsureTemporaryStorageIsInitializedInternal()));

    mAllTemporaryGroups = Some(aQuotaManager.GetAllTemporaryGroups());
  }

  return NS_OK;
}

MaybePrincipalMetadataArray InitTemporaryStorageOp::UnwrapResolveValue() {
  AssertIsOnOwningThread();

  return std::move(mAllTemporaryGroups);
}

void InitTemporaryStorageOp::CloseDirectory() {
  AssertIsOnOwningThread();

  DropDirectoryLock(mDirectoryLock);
}

InitializeTemporaryGroupOp::InitializeTemporaryGroupOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
    const PrincipalMetadata& aPrincipalMetadata,
    RefPtr<UniversalDirectoryLock> aDirectoryLock)
    : ResolvableNormalOriginOp(std::move(aQuotaManager),
                               "dom::quota::InitializeTemporaryGroupOp"),
      mPrincipalMetadata(aPrincipalMetadata),
      mDirectoryLock(std::move(aDirectoryLock)) {
  AssertIsOnOwningThread();
}

RefPtr<BoolPromise> InitializeTemporaryGroupOp::OpenDirectory() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mDirectoryLock);

  return BoolPromise::CreateAndResolve(true, __func__);
}

nsresult InitializeTemporaryGroupOp::DoDirectoryWork(
    QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();

  AUTO_PROFILER_LABEL("InitializeTemporaryGroupOp::DoDirectoryWork", OTHER);

  QM_TRY(OkIf(aQuotaManager.IsStorageInitializedInternal()),
         NS_ERROR_NOT_INITIALIZED);

  QM_TRY(OkIf(aQuotaManager.IsTemporaryStorageInitializedInternal()),
         NS_ERROR_NOT_INITIALIZED);

  QM_TRY(aQuotaManager.EnsureTemporaryGroupIsInitializedInternal(
      mPrincipalMetadata));

  return NS_OK;
}

bool InitializeTemporaryGroupOp::UnwrapResolveValue() {
  AssertIsOnOwningThread();

  return true;
}

void InitializeTemporaryGroupOp::CloseDirectory() {
  AssertIsOnOwningThread();

  DropDirectoryLock(mDirectoryLock);
}

InitializeOriginRequestBase::InitializeOriginRequestBase(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager, const char* aName,
    const PrincipalMetadata& aPrincipalMetadata,
    RefPtr<UniversalDirectoryLock> aDirectoryLock)
    : ResolvableNormalOriginOp(std::move(aQuotaManager), aName),
      mPrincipalMetadata(aPrincipalMetadata),
      mDirectoryLock(std::move(aDirectoryLock)),
      mCreated(false) {
  AssertIsOnOwningThread();
}

RefPtr<BoolPromise> InitializeOriginRequestBase::OpenDirectory() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mDirectoryLock);

  return BoolPromise::CreateAndResolve(true, __func__);
}

void InitializeOriginRequestBase::CloseDirectory() {
  AssertIsOnOwningThread();

  DropDirectoryLockIfNotDropped(mDirectoryLock);
}

InitializePersistentOriginOp::InitializePersistentOriginOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
    const OriginMetadata& aOriginMetadata,
    RefPtr<UniversalDirectoryLock> aDirectoryLock)
    : InitializeOriginRequestBase(std::move(aQuotaManager),
                                  "dom::quota::InitializePersistentOriginOp",
                                  aOriginMetadata, std::move(aDirectoryLock)) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aOriginMetadata.mPersistenceType == PERSISTENCE_TYPE_PERSISTENT);
}

nsresult InitializePersistentOriginOp::DoDirectoryWork(
    QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();

  AUTO_PROFILER_LABEL("InitializePersistentOriginOp::DoDirectoryWork", OTHER);

  QM_TRY(OkIf(aQuotaManager.IsStorageInitializedInternal()),
         NS_ERROR_NOT_INITIALIZED);

  QM_TRY_UNWRAP(
      mCreated,
      (aQuotaManager
           .EnsurePersistentOriginIsInitializedInternal(
               OriginMetadata{mPrincipalMetadata, PERSISTENCE_TYPE_PERSISTENT})
           .map([](const auto& res) { return res.second; })));

  return NS_OK;
}

bool InitializePersistentOriginOp::UnwrapResolveValue() {
  AssertIsOnOwningThread();

  return mCreated;
}

InitializeTemporaryOriginOp::InitializeTemporaryOriginOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
    const OriginMetadata& aOriginMetadata, bool aCreateIfNonExistent,
    RefPtr<UniversalDirectoryLock> aDirectoryLock)
    : InitializeOriginRequestBase(std::move(aQuotaManager),
                                  "dom::quota::InitializeTemporaryOriginOp",
                                  aOriginMetadata, std::move(aDirectoryLock)),
      mPersistenceType(aOriginMetadata.mPersistenceType),
      mCreateIfNonExistent(aCreateIfNonExistent) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aOriginMetadata.mPersistenceType != PERSISTENCE_TYPE_PERSISTENT);
}

nsresult InitializeTemporaryOriginOp::DoDirectoryWork(
    QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();

  AUTO_PROFILER_LABEL("InitializeTemporaryOriginOp::DoDirectoryWork", OTHER);

  QM_TRY(OkIf(aQuotaManager.IsStorageInitializedInternal()),
         NS_ERROR_NOT_INITIALIZED);

  QM_TRY(OkIf(aQuotaManager.IsTemporaryStorageInitializedInternal()),
         NS_ERROR_NOT_INITIALIZED);

  QM_TRY_UNWRAP(mCreated,
                (aQuotaManager
                     .EnsureTemporaryOriginIsInitializedInternal(
                         OriginMetadata{mPrincipalMetadata, mPersistenceType},
                         mCreateIfNonExistent)
                     .map([](const auto& res) { return res.second; })));

  return NS_OK;
}

bool InitializeTemporaryOriginOp::UnwrapResolveValue() {
  AssertIsOnOwningThread();

  return mCreated;
}

InitializeClientBase::InitializeClientBase(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager, const char* aName,
    const PersistenceType aPersistenceType, const PrincipalInfo& aPrincipalInfo,
    Client::Type aClientType)
    : ResolvableNormalOriginOp(std::move(aQuotaManager), aName),
      mPrincipalInfo(aPrincipalInfo),
      mPersistenceType(aPersistenceType),
      mClientType(aClientType),
      mCreated(false) {
  AssertIsOnOwningThread();
}

nsresult InitializeClientBase::DoInit(QuotaManager& aQuotaManager) {
  AssertIsOnOwningThread();

  QM_TRY_UNWRAP(
      PrincipalMetadata principalMetadata,
      GetInfoFromValidatedPrincipalInfo(aQuotaManager, mPrincipalInfo));

  principalMetadata.AssertInvariants();

  mClientMetadata = {
      OriginMetadata{std::move(principalMetadata), mPersistenceType},
      mClientType};

  return NS_OK;
}

RefPtr<BoolPromise> InitializeClientBase::OpenDirectory() {
  AssertIsOnOwningThread();

  mDirectoryLock = mQuotaManager->CreateDirectoryLockInternal(
      PersistenceScope::CreateFromValue(mPersistenceType),
      OriginScope::FromOrigin(mClientMetadata),
      Nullable(mClientMetadata.mClientType), /* aExclusive */ false);

  return mDirectoryLock->Acquire();
}

void InitializeClientBase::CloseDirectory() {
  AssertIsOnOwningThread();

  DropDirectoryLockIfNotDropped(mDirectoryLock);
}

InitializePersistentClientOp::InitializePersistentClientOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
    const PrincipalInfo& aPrincipalInfo, Client::Type aClientType)
    : InitializeClientBase(
          std::move(aQuotaManager), "dom::quota::InitializePersistentClientOp",
          PERSISTENCE_TYPE_PERSISTENT, aPrincipalInfo, aClientType) {
  AssertIsOnOwningThread();
}

nsresult InitializePersistentClientOp::DoDirectoryWork(
    QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();

  AUTO_PROFILER_LABEL("InitializePersistentClientOp::DoDirectoryWork", OTHER);

  QM_TRY(MOZ_TO_RESULT(aQuotaManager.IsStorageInitializedInternal()),
         NS_ERROR_FAILURE);

  QM_TRY(MOZ_TO_RESULT(aQuotaManager.IsPersistentOriginInitializedInternal(
             mClientMetadata.mOrigin)),
         NS_ERROR_FAILURE);

  QM_TRY_UNWRAP(
      mCreated,
      (aQuotaManager.EnsurePersistentClientIsInitialized(mClientMetadata)
           .map([](const auto& res) { return res.second; })));

  return NS_OK;
}

bool InitializePersistentClientOp::UnwrapResolveValue() {
  AssertIsOnOwningThread();

  return mCreated;
}

InitializeTemporaryClientOp::InitializeTemporaryClientOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
    PersistenceType aPersistenceType, const PrincipalInfo& aPrincipalInfo,
    Client::Type aClientType)
    : InitializeClientBase(std::move(aQuotaManager),
                           "dom::quota::InitializeTemporaryClientOp",
                           aPersistenceType, aPrincipalInfo, aClientType) {
  AssertIsOnOwningThread();
}

nsresult InitializeTemporaryClientOp::DoDirectoryWork(
    QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();

  AUTO_PROFILER_LABEL("InitializeTemporaryClientOp::DoDirectoryWork", OTHER);

  QM_TRY(MOZ_TO_RESULT(aQuotaManager.IsStorageInitializedInternal()),
         NS_ERROR_FAILURE);

  QM_TRY(MOZ_TO_RESULT(aQuotaManager.IsTemporaryStorageInitializedInternal()),
         NS_ERROR_FAILURE);

  QM_TRY(MOZ_TO_RESULT(aQuotaManager.IsTemporaryOriginInitializedInternal(
             mClientMetadata)),
         NS_ERROR_FAILURE);

  QM_TRY_UNWRAP(
      mCreated,
      (aQuotaManager.EnsureTemporaryClientIsInitialized(mClientMetadata)
           .map([](const auto& res) { return res.second; })));

  return NS_OK;
}

bool InitializeTemporaryClientOp::UnwrapResolveValue() {
  AssertIsOnOwningThread();

  return mCreated;
}

GetFullOriginMetadataOp::GetFullOriginMetadataOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
    const GetFullOriginMetadataParams& aParams)
    : OpenStorageDirectoryHelper(std::move(aQuotaManager),
                                 "dom::quota::GetFullOriginMetadataOp"),
      mParams(aParams) {
  AssertIsOnOwningThread();
}

nsresult GetFullOriginMetadataOp::DoInit(QuotaManager& aQuotaManager) {
  AssertIsOnOwningThread();

  QM_TRY_UNWRAP(PrincipalMetadata principalMetadata,
                GetInfoFromValidatedPrincipalInfo(aQuotaManager,
                                                  mParams.principalInfo()));

  principalMetadata.AssertInvariants();

  mOriginMetadata = {std::move(principalMetadata), mParams.persistenceType()};

  return NS_OK;
}

RefPtr<BoolPromise> GetFullOriginMetadataOp::OpenDirectory() {
  AssertIsOnOwningThread();

  return OpenStorageDirectory(
      PersistenceScope::CreateFromValue(mOriginMetadata.mPersistenceType),
      OriginScope::FromOrigin(mOriginMetadata), Nullable<Client::Type>(),
      /* aExclusive */ false,
      /* aInitializeOrigins */ true);
}

nsresult GetFullOriginMetadataOp::DoDirectoryWork(QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();
  aQuotaManager.AssertStorageIsInitializedInternal();

  AUTO_PROFILER_LABEL("GetFullOriginMetadataOp::DoDirectoryWork", OTHER);

  // Get metadata cached in memory (the method doesn't have to stat any
  // files).
  mMaybeFullOriginMetadata =
      aQuotaManager.GetFullOriginMetadata(mOriginMetadata);

  return NS_OK;
}

void GetFullOriginMetadataOp::GetResponse(RequestResponse& aResponse) {
  AssertIsOnOwningThread();

  aResponse = GetFullOriginMetadataResponse();
  aResponse.get_GetFullOriginMetadataResponse().maybeFullOriginMetadata() =
      std::move(mMaybeFullOriginMetadata);
}

void GetFullOriginMetadataOp::CloseDirectory() {
  AssertIsOnOwningThread();

  SafeDropDirectoryLock(mDirectoryLock);
}

GetCachedOriginUsageOp::GetCachedOriginUsageOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
    const PrincipalInfo& aPrincipalInfo)
    : OpenStorageDirectoryHelper(std::move(aQuotaManager),
                                 "dom::quota::GetCachedOriginUsageOp"),
      mPrincipalInfo(aPrincipalInfo),
      mUsage(0) {
  AssertIsOnOwningThread();
}

nsresult GetCachedOriginUsageOp::DoInit(QuotaManager& aQuotaManager) {
  AssertIsOnOwningThread();

  QM_TRY_UNWRAP(mPrincipalMetadata, GetInfoFromValidatedPrincipalInfo(
                                        aQuotaManager, mPrincipalInfo));

  mPrincipalMetadata.AssertInvariants();

  return NS_OK;
}

RefPtr<BoolPromise> GetCachedOriginUsageOp::OpenDirectory() {
  AssertIsOnOwningThread();

  return OpenStorageDirectory(
      PersistenceScope::CreateFromSet(PERSISTENCE_TYPE_TEMPORARY,
                                      PERSISTENCE_TYPE_DEFAULT,
                                      PERSISTENCE_TYPE_PRIVATE),
      OriginScope::FromOrigin(mPrincipalMetadata), Nullable<Client::Type>(),
      /* aExclusive */ false);
}

nsresult GetCachedOriginUsageOp::DoDirectoryWork(QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();
  MOZ_ASSERT(mUsage == 0);

  AUTO_PROFILER_LABEL("GetCachedOriginUsageOp::DoDirectoryWork", OTHER);

  // If temporary storage hasn't been initialized yet, there's no cached usage
  // to report.
  if (!aQuotaManager.IsTemporaryStorageInitializedInternal()) {
    return NS_OK;
  }

  // Get cached usage (the method doesn't have to stat any files).
  mUsage = aQuotaManager.GetOriginUsage(mPrincipalMetadata);

  return NS_OK;
}

uint64_t GetCachedOriginUsageOp::UnwrapResolveValue() {
  AssertIsOnOwningThread();

  return mUsage;
}

void GetCachedOriginUsageOp::CloseDirectory() {
  AssertIsOnOwningThread();

  SafeDropDirectoryLock(mDirectoryLock);
}

ListCachedOriginsOp::ListCachedOriginsOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager)
    : OpenStorageDirectoryHelper(std::move(aQuotaManager),
                                 "dom::quota::ListCachedOriginsOp") {
  AssertIsOnOwningThread();
}

RefPtr<BoolPromise> ListCachedOriginsOp::OpenDirectory() {
  AssertIsOnOwningThread();

  return OpenStorageDirectory(PersistenceScope::CreateFromNull(),
                              OriginScope::FromNull(), Nullable<Client::Type>(),
                              /* aExclusive */ false);
}

nsresult ListCachedOriginsOp::DoDirectoryWork(QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();
  MOZ_ASSERT(mOrigins.Length() == 0);

  AUTO_PROFILER_LABEL("ListCachedOriginsOp::DoDirectoryWork", OTHER);

  // If temporary storage hasn't been initialized yet, there are no cached
  // origins to report.
  if (!aQuotaManager.IsTemporaryStorageInitializedInternal()) {
    return NS_OK;
  }

  // Get cached origins (the method doesn't have to stat any files).
  OriginMetadataArray originMetadataArray =
      aQuotaManager.GetAllTemporaryOrigins();

  std::transform(originMetadataArray.cbegin(), originMetadataArray.cend(),
                 MakeBackInserter(mOrigins), [](const auto& originMetadata) {
                   return originMetadata.mOrigin;
                 });

  return NS_OK;
}

CStringArray ListCachedOriginsOp::UnwrapResolveValue() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(!ResolveValueConsumed());

  return std::move(mOrigins);
}

void ListCachedOriginsOp::CloseDirectory() {
  AssertIsOnOwningThread();

  SafeDropDirectoryLock(mDirectoryLock);
}

ClearStorageOp::ClearStorageOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager)
    : OpenStorageDirectoryHelper(std::move(aQuotaManager),
                                 "dom::quota::ClearStorageOp") {
  AssertIsOnOwningThread();
}

void ClearStorageOp::DeleteFiles(QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();

  nsresult rv = aQuotaManager.AboutToClearOrigins(
      PersistenceScope::CreateFromNull(), OriginScope::FromNull(),
      Nullable<Client::Type>());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  auto directoryOrErr = QM_NewLocalFile(aQuotaManager.GetStoragePath());
  if (NS_WARN_IF(directoryOrErr.isErr())) {
    return;
  }

  nsCOMPtr<nsIFile> directory = directoryOrErr.unwrap();

  rv = directory->Remove(true);
  if (rv != NS_ERROR_FILE_NOT_FOUND && NS_FAILED(rv)) {
    // This should never fail if we've closed all storage connections
    // correctly...
    MOZ_ASSERT(false, "Failed to remove storage directory!");
  }
}

void ClearStorageOp::DeleteStorageFile(QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();

  QM_TRY_INSPECT(const auto& storageFile,
                 QM_NewLocalFile(aQuotaManager.GetBasePath()), QM_VOID);

  QM_TRY(MOZ_TO_RESULT(storageFile->Append(aQuotaManager.GetStorageName() +
                                           kSQLiteSuffix)),
         QM_VOID);

  const nsresult rv = storageFile->Remove(true);
  if (rv != NS_ERROR_FILE_NOT_FOUND && NS_FAILED(rv)) {
    // This should never fail if we've closed the storage connection
    // correctly...
    MOZ_ASSERT(false, "Failed to remove storage file!");
  }
}

RefPtr<BoolPromise> ClearStorageOp::OpenDirectory() {
  AssertIsOnOwningThread();

  // Clear directory lock tables (which also saves origin access time) before
  // acquiring the exclusive lock below. Otherwise, saving of origin access
  // time would be scheduled after storage clearing and that would initialize
  // storage again in the end.
  mQuotaManager->ClearDirectoryLockTables();

  return OpenStorageDirectory(PersistenceScope::CreateFromNull(),
                              OriginScope::FromNull(), Nullable<Client::Type>(),
                              /* aExclusive */ true,
                              /* aInitializeOrigins */ false,
                              DirectoryLockCategory::UninitStorage);
}

nsresult ClearStorageOp::DoDirectoryWork(QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();
  aQuotaManager.AssertStorageIsInitializedInternal();

  AUTO_PROFILER_LABEL("ClearStorageOp::DoDirectoryWork", OTHER);

  DeleteFiles(aQuotaManager);

  aQuotaManager.RemoveQuota();

  aQuotaManager.ShutdownStorageInternal();

  DeleteStorageFile(aQuotaManager);

  return NS_OK;
}

bool ClearStorageOp::UnwrapResolveValue() {
  AssertIsOnOwningThread();

  return true;
}

void ClearStorageOp::CloseDirectory() {
  AssertIsOnOwningThread();

  SafeDropDirectoryLock(mDirectoryLock);
}

void ClearRequestBase::DeleteFiles(QuotaManager& aQuotaManager,
                                   const OriginMetadata& aOriginMetadata) {
  AssertIsOnIOThread();

  DeleteFilesInternal(
      aQuotaManager, aOriginMetadata.mPersistenceType,
      OriginScope::FromOrigin(aOriginMetadata),
      [&aQuotaManager, &aOriginMetadata](
          const std::function<Result<Ok, nsresult>(nsCOMPtr<nsIFile>)>& aBody)
          -> Result<Ok, nsresult> {
        QM_TRY_UNWRAP(auto directory,
                      aQuotaManager.GetOriginDirectory(aOriginMetadata));

        // We're not checking if the origin directory actualy exists because
        // it can be a pending origin (OriginInfo does exist but the origin
        // directory hasn't been created yet).

        QM_TRY_RETURN(aBody(std::move(directory)));
      });
}

void ClearRequestBase::DeleteFiles(QuotaManager& aQuotaManager,
                                   PersistenceType aPersistenceType,
                                   const OriginScope& aOriginScope) {
  AssertIsOnIOThread();

  DeleteFilesInternal(
      aQuotaManager, aPersistenceType, aOriginScope,
      [&aQuotaManager, &aPersistenceType](
          const std::function<Result<Ok, nsresult>(nsCOMPtr<nsIFile>)>& aBody)
          -> Result<Ok, nsresult> {
        QM_TRY_INSPECT(
            const auto& directory,
            QM_NewLocalFile(aQuotaManager.GetStoragePath(aPersistenceType)));

        QM_TRY_INSPECT(const bool& exists,
                       MOZ_TO_RESULT_INVOKE_MEMBER(directory, Exists));

        if (!exists) {
          return Ok{};
        }

        QM_TRY(CollectEachFile(*directory, aBody));

        // CollectEachFile above only consulted the file-system to get a list of
        // known origins, but we also need to include origins that have pending
        // quota usage.

        nsTArray<OriginMetadata> originMetadataArray;
        aQuotaManager.CollectPendingOriginsForListing(
            [aPersistenceType, &originMetadataArray](const auto& originInfo) {
              if (originInfo->GetGroupInfo()->GetPersistenceType() !=
                  aPersistenceType) {
                return;
              }
              originMetadataArray.AppendElement(
                  originInfo->FlattenToOriginMetadata());
            });

        if (originMetadataArray.IsEmpty()) {
          return Ok{};
        }

        nsTArray<nsCOMPtr<nsIFile>> originDirectories;
        QM_TRY(TransformAbortOnErr(
            originMetadataArray, MakeBackInserter(originDirectories),
            [&aQuotaManager](const auto& originMetadata)
                -> Result<nsCOMPtr<nsIFile>, nsresult> {
              QM_TRY_UNWRAP(auto originDirectory,
                            aQuotaManager.GetOriginDirectory(originMetadata));
              return originDirectory;
            }));

        QM_TRY_RETURN(CollectEachInRange(originDirectories, aBody));
      });
}

template <typename FileCollector>
void ClearRequestBase::DeleteFilesInternal(
    QuotaManager& aQuotaManager, PersistenceType aPersistenceType,
    const OriginScope& aOriginScope, const FileCollector& aFileCollector) {
  AssertIsOnIOThread();

  QM_TRY(MOZ_TO_RESULT(aQuotaManager.AboutToClearOrigins(
             PersistenceScope::CreateFromValue(aPersistenceType), aOriginScope,
             Nullable<Client::Type>())),
         QM_VOID);

  nsTArray<nsCOMPtr<nsIFile>> directoriesForRemovalRetry;

  aQuotaManager.MaybeRecordQuotaManagerShutdownStep(
      "ClearRequestBase: Starting deleting files"_ns);

  QM_TRY(
      aFileCollector([&originScope = aOriginScope, aPersistenceType,
                      &aQuotaManager, &directoriesForRemovalRetry,
                      this](nsCOMPtr<nsIFile> file)
                         -> mozilla::Result<Ok, nsresult> {
        QM_TRY_INSPECT(const auto& dirEntryKind, GetDirEntryKind(*file));

        QM_TRY_INSPECT(
            const auto& leafName,
            MOZ_TO_RESULT_INVOKE_MEMBER_TYPED(nsAutoString, file, GetLeafName));

        switch (dirEntryKind) {
          case nsIFileKind::ExistsAsDirectory: {
            QM_TRY_UNWRAP(auto maybeMetadata,
                          QM_OR_ELSE_WARN_IF(
                              // Expression
                              aQuotaManager.GetOriginMetadata(file).map(
                                  [](auto metadata) -> Maybe<OriginMetadata> {
                                    return Some(std::move(metadata));
                                  }),
                              // Predicate.
                              IsSpecificError<NS_ERROR_MALFORMED_URI>,
                              // Fallback.
                              ErrToDefaultOk<Maybe<OriginMetadata>>));

            if (!maybeMetadata) {
              // Unknown directories during clearing are allowed. Just
              // warn if we find them.
              UNKNOWN_FILE_WARNING(leafName);
              break;
            }

            auto metadata = maybeMetadata.extract();

            MOZ_ASSERT(metadata.mPersistenceType == aPersistenceType);

            // Skip the origin directory if it doesn't match the pattern.
            if (!originScope.Matches(OriginScope::FromOrigin(metadata))) {
              break;
            }

            // We can't guarantee that this will always succeed on
            // Windows...
            QM_WARNONLY_TRY(
                aQuotaManager.RemoveOriginDirectory(*file), [&](const auto&) {
                  directoriesForRemovalRetry.AppendElement(std::move(file));
                });

            mOriginMetadataArray.AppendElement(metadata);

            const bool initialized =
                aPersistenceType == PERSISTENCE_TYPE_PERSISTENT
                    ? aQuotaManager.IsPersistentOriginInitializedInternal(
                          metadata.mOrigin)
                    : aQuotaManager.IsTemporaryStorageInitializedInternal();

            // If it hasn't been initialized, we don't need to update the
            // quota and notify the removing client, but we do need to remove
            // it from quota info cache.
            if (!initialized) {
              aQuotaManager.RemoveOriginFromCache(metadata);
              break;
            }

            if (aPersistenceType != PERSISTENCE_TYPE_PERSISTENT) {
              aQuotaManager.RemoveQuotaForOrigin(aPersistenceType, metadata);
            }

            aQuotaManager.OriginClearCompleted(metadata,
                                               Nullable<Client::Type>());

            break;
          }

          case nsIFileKind::ExistsAsFile: {
            // Unknown files during clearing are allowed. Just warn if we
            // find them.
            if (!IsOSMetadata(leafName)) {
              UNKNOWN_FILE_WARNING(leafName);
            }

            break;
          }

          case nsIFileKind::DoesNotExist: {
            if (aPersistenceType == PERSISTENCE_TYPE_PERSISTENT) {
              break;
            }

            QM_TRY_UNWRAP(auto metadata, aQuotaManager.GetOriginMetadata(file));

            MOZ_ASSERT(metadata.mPersistenceType == aPersistenceType);

            // Skip the origin directory if it doesn't match the pattern.
            if (!originScope.Matches(OriginScope::FromOrigin(metadata))) {
              break;
            }

            if (!aQuotaManager.IsPendingOrigin(metadata)) {
              break;
            }

            mOriginMetadataArray.AppendElement(metadata);

            aQuotaManager.RemoveQuotaForOrigin(aPersistenceType, metadata);

            aQuotaManager.OriginClearCompleted(metadata,
                                               Nullable<Client::Type>());

            break;
          }
        }

        mIterations++;

        return Ok{};
      }),
      QM_VOID);

  // Retry removing any directories that failed to be removed earlier now.
  //
  // XXX This will still block this operation. We might instead dispatch a
  // runnable to our own thread for each retry round with a timer. We must
  // ensure that the directory lock is upheld until we complete or give up
  // though.
  for (uint32_t index = 0; index < 10; index++) {
    aQuotaManager.MaybeRecordQuotaManagerShutdownStepWith([index]() {
      return nsPrintfCString(
          "ClearRequestBase: Starting repeated directory removal #%d", index);
    });

    for (auto&& file : std::exchange(directoriesForRemovalRetry,
                                     nsTArray<nsCOMPtr<nsIFile>>{})) {
      QM_WARNONLY_TRY(
          aQuotaManager.RemoveOriginDirectory(*file),
          ([&directoriesForRemovalRetry, &file](const auto&) {
            directoriesForRemovalRetry.AppendElement(std::move(file));
          }));
    }

    aQuotaManager.MaybeRecordQuotaManagerShutdownStepWith([index]() {
      return nsPrintfCString(
          "ClearRequestBase: Completed repeated directory removal #%d", index);
    });

    if (directoriesForRemovalRetry.IsEmpty()) {
      break;
    }

    aQuotaManager.MaybeRecordQuotaManagerShutdownStepWith([index]() {
      return nsPrintfCString("ClearRequestBase: Before sleep #%d", index);
    });

    PR_Sleep(PR_MillisecondsToInterval(200));

    aQuotaManager.MaybeRecordQuotaManagerShutdownStepWith([index]() {
      return nsPrintfCString("ClearRequestBase: After sleep #%d", index);
    });
  }

  QM_WARNONLY_TRY(OkIf(directoriesForRemovalRetry.IsEmpty()));

  aQuotaManager.MaybeRecordQuotaManagerShutdownStep(
      "ClearRequestBase: Completed deleting files"_ns);
}

ClearOriginOp::ClearOriginOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
    const mozilla::Maybe<PersistenceType>& aPersistenceType,
    const PrincipalInfo& aPrincipalInfo)
    : ClearRequestBase(std::move(aQuotaManager), "dom::quota::ClearOriginOp"),
      mPrincipalInfo(aPrincipalInfo),
      mPersistenceScope(aPersistenceType ? PersistenceScope::CreateFromValue(
                                               *aPersistenceType)
                                         : PersistenceScope::CreateFromNull()) {
  AssertIsOnOwningThread();
}

nsresult ClearOriginOp::DoInit(QuotaManager& aQuotaManager) {
  AssertIsOnOwningThread();

  QM_TRY_UNWRAP(mPrincipalMetadata, GetInfoFromValidatedPrincipalInfo(
                                        aQuotaManager, mPrincipalInfo));

  mPrincipalMetadata.AssertInvariants();

  return NS_OK;
}

RefPtr<BoolPromise> ClearOriginOp::OpenDirectory() {
  AssertIsOnOwningThread();

  return OpenStorageDirectory(
      mPersistenceScope, OriginScope::FromOrigin(mPrincipalMetadata),
      Nullable<Client::Type>(), /* aExclusive */ true,
      /* aInitializeOrigins */ false, DirectoryLockCategory::UninitOrigins);
}

nsresult ClearOriginOp::DoDirectoryWork(QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();
  aQuotaManager.AssertStorageIsInitializedInternal();

  AUTO_PROFILER_LABEL("ClearRequestBase::DoDirectoryWork", OTHER);

  if (mPersistenceScope.IsNull()) {
    for (const PersistenceType type : kAllPersistenceTypes) {
      DeleteFiles(aQuotaManager, OriginMetadata(mPrincipalMetadata, type));
    }
  } else {
    MOZ_ASSERT(mPersistenceScope.IsValue());

    DeleteFiles(aQuotaManager, OriginMetadata(mPrincipalMetadata,
                                              mPersistenceScope.GetValue()));
  }

  return NS_OK;
}

OriginMetadataArray ClearOriginOp::UnwrapResolveValue() {
  AssertIsOnOwningThread();

  return std::move(mOriginMetadataArray);
}

void ClearOriginOp::CloseDirectory() {
  AssertIsOnOwningThread();

  SafeDropDirectoryLock(mDirectoryLock);
}

ClearClientOp::ClearClientOp(MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
                             mozilla::Maybe<PersistenceType> aPersistenceType,
                             const PrincipalInfo& aPrincipalInfo,
                             Client::Type aClientType)
    : OpenStorageDirectoryHelper(std::move(aQuotaManager),
                                 "dom::quota::ClearClientOp"),
      mPrincipalInfo(aPrincipalInfo),
      mPersistenceScope(aPersistenceType ? PersistenceScope::CreateFromValue(
                                               *aPersistenceType)
                                         : PersistenceScope::CreateFromNull()),
      mClientType(aClientType) {
  AssertIsOnOwningThread();
}

nsresult ClearClientOp::DoInit(QuotaManager& aQuotaManager) {
  AssertIsOnOwningThread();

  QM_TRY_UNWRAP(mPrincipalMetadata, GetInfoFromValidatedPrincipalInfo(
                                        aQuotaManager, mPrincipalInfo));

  mPrincipalMetadata.AssertInvariants();

  return NS_OK;
}

RefPtr<BoolPromise> ClearClientOp::OpenDirectory() {
  AssertIsOnOwningThread();

  return OpenStorageDirectory(mPersistenceScope,
                              OriginScope::FromOrigin(mPrincipalMetadata),
                              Nullable(mClientType), /* aExclusive */ true);
}

void ClearClientOp::DeleteFiles(const ClientMetadata& aClientMetadata) {
  AssertIsOnIOThread();

  QM_TRY(
      MOZ_TO_RESULT(mQuotaManager->AboutToClearOrigins(
          PersistenceScope::CreateFromValue(aClientMetadata.mPersistenceType),
          OriginScope::FromOrigin(aClientMetadata),
          Nullable(aClientMetadata.mClientType))),
      QM_VOID);

  QM_TRY_INSPECT(const auto& directory,
                 mQuotaManager->GetOriginDirectory(aClientMetadata), QM_VOID);

  QM_TRY(MOZ_TO_RESULT(directory->Append(
             Client::TypeToString(aClientMetadata.mClientType))),
         QM_VOID);

  QM_TRY_INSPECT(const bool& exists,
                 MOZ_TO_RESULT_INVOKE_MEMBER(directory, Exists), QM_VOID);
  if (!exists) {
    return;
  }

  QM_TRY(MOZ_TO_RESULT(directory->Remove(true)), QM_VOID);

  const bool initialized =
      aClientMetadata.mPersistenceType == PERSISTENCE_TYPE_PERSISTENT
          ? mQuotaManager->IsPersistentOriginInitializedInternal(
                aClientMetadata.mOrigin)
          : mQuotaManager->IsTemporaryStorageInitializedInternal();

  if (!initialized) {
    return;
  }

  if (aClientMetadata.mPersistenceType != PERSISTENCE_TYPE_PERSISTENT) {
    mQuotaManager->ResetUsageForClient(aClientMetadata);
  }

  mQuotaManager->OriginClearCompleted(aClientMetadata,
                                      Nullable(aClientMetadata.mClientType));
}

nsresult ClearClientOp::DoDirectoryWork(QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();
  aQuotaManager.AssertStorageIsInitializedInternal();

  AUTO_PROFILER_LABEL("ClearClientOp::DoDirectoryWork", OTHER);

  if (mPersistenceScope.IsNull()) {
    for (const PersistenceType type : kAllPersistenceTypes) {
      DeleteFiles(ClientMetadata(OriginMetadata(mPrincipalMetadata, type),
                                 mClientType));
    }
  } else {
    MOZ_ASSERT(mPersistenceScope.IsValue());

    DeleteFiles(ClientMetadata(
        OriginMetadata(mPrincipalMetadata, mPersistenceScope.GetValue()),
        mClientType));
  }

  return NS_OK;
}

bool ClearClientOp::UnwrapResolveValue() {
  AssertIsOnOwningThread();

  return true;
}

void ClearClientOp::CloseDirectory() {
  AssertIsOnOwningThread();

  SafeDropDirectoryLock(mDirectoryLock);
}

ClearStoragesForOriginPrefixOp::ClearStoragesForOriginPrefixOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
    const Maybe<PersistenceType>& aPersistenceType,
    const PrincipalInfo& aPrincipalInfo)
    : OpenStorageDirectoryHelper(std::move(aQuotaManager),
                                 "dom::quota::ClearStoragesForOriginPrefixOp"),
      mPrincipalInfo(aPrincipalInfo),
      mPersistenceScope(aPersistenceType ? PersistenceScope::CreateFromValue(
                                               *aPersistenceType)
                                         : PersistenceScope::CreateFromNull()) {
  AssertIsOnOwningThread();
}

nsresult ClearStoragesForOriginPrefixOp::DoInit(QuotaManager& aQuotaManager) {
  AssertIsOnOwningThread();

  QM_TRY_UNWRAP(mPrincipalMetadata, GetInfoFromValidatedPrincipalInfo(
                                        aQuotaManager, mPrincipalInfo));

  mPrincipalMetadata.AssertInvariants();

  return NS_OK;
}

RefPtr<BoolPromise> ClearStoragesForOriginPrefixOp::OpenDirectory() {
  AssertIsOnOwningThread();

  return OpenStorageDirectory(
      mPersistenceScope, OriginScope::FromPrefix(mPrincipalMetadata),
      Nullable<Client::Type>(), /* aExclusive */ true,
      /* aInitializeOrigins */ false, DirectoryLockCategory::UninitOrigins);
}

nsresult ClearStoragesForOriginPrefixOp::DoDirectoryWork(
    QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();

  AUTO_PROFILER_LABEL("ClearStoragesForOriginPrefixOp::DoDirectoryWork", OTHER);

  if (mPersistenceScope.IsNull()) {
    for (const PersistenceType type : kAllPersistenceTypes) {
      DeleteFiles(aQuotaManager, type,
                  OriginScope::FromPrefix(mPrincipalMetadata));
    }
  } else {
    MOZ_ASSERT(mPersistenceScope.IsValue());

    DeleteFiles(aQuotaManager, mPersistenceScope.GetValue(),
                OriginScope::FromPrefix(mPrincipalMetadata));
  }

  return NS_OK;
}

OriginMetadataArray ClearStoragesForOriginPrefixOp::UnwrapResolveValue() {
  AssertIsOnOwningThread();

  return std::move(mOriginMetadataArray);
}

void ClearStoragesForOriginPrefixOp::CloseDirectory() {
  AssertIsOnOwningThread();

  SafeDropDirectoryLock(mDirectoryLock);
}

ClearDataOp::ClearDataOp(MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
                         const OriginAttributesPattern& aPattern)
    : ClearRequestBase(std::move(aQuotaManager), "dom::quota::ClearDataOp"),
      mPattern(aPattern) {}

RefPtr<BoolPromise> ClearDataOp::OpenDirectory() {
  AssertIsOnOwningThread();

  return OpenStorageDirectory(
      PersistenceScope::CreateFromNull(), OriginScope::FromPattern(mPattern),
      Nullable<Client::Type>(), /* aExclusive */ true,
      /* aInitializeOrigins */ false, DirectoryLockCategory::UninitOrigins);
}

nsresult ClearDataOp::DoDirectoryWork(QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();

  AUTO_PROFILER_LABEL("ClearRequestBase::DoDirectoryWork", OTHER);

  for (const PersistenceType type : kAllPersistenceTypes) {
    DeleteFiles(aQuotaManager, type, OriginScope::FromPattern(mPattern));
  }

  return NS_OK;
}

OriginMetadataArray ClearDataOp::UnwrapResolveValue() {
  AssertIsOnOwningThread();

  return std::move(mOriginMetadataArray);
}

void ClearDataOp::CloseDirectory() {
  AssertIsOnOwningThread();

  SafeDropDirectoryLock(mDirectoryLock);
}

ShutdownOriginOp::ShutdownOriginOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
    mozilla::Maybe<PersistenceType> aPersistenceType,
    const PrincipalInfo& aPrincipalInfo)
    : ResolvableNormalOriginOp(std::move(aQuotaManager),
                               "dom::quota::ShutdownOriginOp"),
      mPrincipalInfo(aPrincipalInfo),
      mPersistenceScope(aPersistenceType ? PersistenceScope::CreateFromValue(
                                               *aPersistenceType)
                                         : PersistenceScope::CreateFromNull()) {
  AssertIsOnOwningThread();
}

nsresult ShutdownOriginOp::DoInit(QuotaManager& aQuotaManager) {
  AssertIsOnOwningThread();

  QM_TRY_UNWRAP(mPrincipalMetadata, GetInfoFromValidatedPrincipalInfo(
                                        aQuotaManager, mPrincipalInfo));

  mPrincipalMetadata.AssertInvariants();

  return NS_OK;
}

RefPtr<BoolPromise> ShutdownOriginOp::OpenDirectory() {
  AssertIsOnOwningThread();

  mDirectoryLock = mQuotaManager->CreateDirectoryLockInternal(
      mPersistenceScope, OriginScope::FromOrigin(mPrincipalMetadata),
      Nullable<Client::Type>(), /* aExclusive */ true,
      DirectoryLockCategory::UninitOrigins);

  return mDirectoryLock->Acquire();
}

void ShutdownOriginOp::CollectOriginMetadata(
    const OriginMetadata& aOriginMetadata) {
  AssertIsOnIOThread();

  QM_TRY_INSPECT(const auto& directory,
                 mQuotaManager->GetOriginDirectory(aOriginMetadata), QM_VOID);

  QM_TRY_INSPECT(const bool& exists,
                 MOZ_TO_RESULT_INVOKE_MEMBER(directory, Exists), QM_VOID);
  if (!exists) {
    if (aOriginMetadata.mPersistenceType != PERSISTENCE_TYPE_PERSISTENT &&
        mQuotaManager->IsPendingOrigin(aOriginMetadata)) {
      mOriginMetadataArray.AppendElement(aOriginMetadata);
    }

    return;
  }

  mOriginMetadataArray.AppendElement(aOriginMetadata);
}

nsresult ShutdownOriginOp::DoDirectoryWork(QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();

  AUTO_PROFILER_LABEL("ShutdownOriginOp::DoDirectoryWork", OTHER);

  if (mPersistenceScope.IsNull()) {
    for (const PersistenceType type : kAllPersistenceTypes) {
      CollectOriginMetadata(OriginMetadata(mPrincipalMetadata, type));
    }
  } else {
    MOZ_ASSERT(mPersistenceScope.IsValue());

    CollectOriginMetadata(
        OriginMetadata(mPrincipalMetadata, mPersistenceScope.GetValue()));
  }

  return NS_OK;
}

OriginMetadataArray ShutdownOriginOp::UnwrapResolveValue() {
  AssertIsOnOwningThread();

  return std::move(mOriginMetadataArray);
}

void ShutdownOriginOp::CloseDirectory() {
  AssertIsOnOwningThread();

  DropDirectoryLockIfNotDropped(mDirectoryLock);
}

ShutdownClientOp::ShutdownClientOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
    mozilla::Maybe<PersistenceType> aPersistenceType,
    const PrincipalInfo& aPrincipalInfo, Client::Type aClientType)
    : ResolvableNormalOriginOp(std::move(aQuotaManager),
                               "dom::quota::ShutdownClientOp"),
      mPrincipalInfo(aPrincipalInfo),
      mPersistenceScope(aPersistenceType ? PersistenceScope::CreateFromValue(
                                               *aPersistenceType)
                                         : PersistenceScope::CreateFromNull()),
      mClientType(aClientType) {
  AssertIsOnOwningThread();
}

nsresult ShutdownClientOp::DoInit(QuotaManager& aQuotaManager) {
  AssertIsOnOwningThread();

  QM_TRY_UNWRAP(mPrincipalMetadata, GetInfoFromValidatedPrincipalInfo(
                                        aQuotaManager, mPrincipalInfo));

  mPrincipalMetadata.AssertInvariants();

  return NS_OK;
}

RefPtr<BoolPromise> ShutdownClientOp::OpenDirectory() {
  AssertIsOnOwningThread();

  mDirectoryLock = mQuotaManager->CreateDirectoryLockInternal(
      mPersistenceScope, OriginScope::FromOrigin(mPrincipalMetadata),
      Nullable(mClientType), /* aExclusive */ true);

  return mDirectoryLock->Acquire();
}

nsresult ShutdownClientOp::DoDirectoryWork(QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();

  AUTO_PROFILER_LABEL("ShutdownClientOp::DoDirectoryWork", OTHER);

  // All the work is handled by NormalOriginOperationBase parent class. In
  // this particular case, we just needed to acquire an exclusive directory
  // lock and that's it.

  return NS_OK;
}

bool ShutdownClientOp::UnwrapResolveValue() {
  AssertIsOnOwningThread();

  return true;
}

void ShutdownClientOp::CloseDirectory() {
  AssertIsOnOwningThread();

  DropDirectoryLockIfNotDropped(mDirectoryLock);
}

PersistRequestBase::PersistRequestBase(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
    const PrincipalInfo& aPrincipalInfo)
    : OpenStorageDirectoryHelper(std::move(aQuotaManager),
                                 "dom::quota::PersistRequestBase"),
      mPrincipalInfo(aPrincipalInfo) {
  AssertIsOnOwningThread();
}

nsresult PersistRequestBase::DoInit(QuotaManager& aQuotaManager) {
  AssertIsOnOwningThread();

  // Figure out which origin we're dealing with.
  QM_TRY_UNWRAP(mPrincipalMetadata, GetInfoFromValidatedPrincipalInfo(
                                        aQuotaManager, mPrincipalInfo));

  mPrincipalMetadata.AssertInvariants();

  return NS_OK;
}

RefPtr<BoolPromise> PersistRequestBase::OpenDirectory() {
  AssertIsOnOwningThread();

  return OpenStorageDirectory(
      PersistenceScope::CreateFromValue(PERSISTENCE_TYPE_DEFAULT),
      OriginScope::FromOrigin(mPrincipalMetadata), Nullable<Client::Type>(),
      /* aExclusive */ false);
}

void PersistRequestBase::CloseDirectory() {
  AssertIsOnOwningThread();

  SafeDropDirectoryLock(mDirectoryLock);
}

PersistedOp::PersistedOp(MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
                         const RequestParams& aParams)
    : PersistRequestBase(std::move(aQuotaManager),
                         aParams.get_PersistedParams().principalInfo()),
      mPersisted(false) {
  MOZ_ASSERT(aParams.type() == RequestParams::TPersistedParams);
}

nsresult PersistedOp::DoDirectoryWork(QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();
  aQuotaManager.AssertStorageIsInitializedInternal();

  AUTO_PROFILER_LABEL("PersistedOp::DoDirectoryWork", OTHER);

  const OriginMetadata originMetadata = {mPrincipalMetadata,
                                         PERSISTENCE_TYPE_DEFAULT};

  Nullable<bool> persisted = aQuotaManager.OriginPersisted(originMetadata);

  if (!persisted.IsNull()) {
    mPersisted = persisted.Value();
    return NS_OK;
  }

  // If we get here, it means the origin hasn't been initialized yet.
  // Try to get the persisted flag from directory metadata on disk.

  QM_TRY_INSPECT(const auto& directory,
                 aQuotaManager.GetOriginDirectory(originMetadata));

  QM_TRY_INSPECT(const bool& exists,
                 MOZ_TO_RESULT_INVOKE_MEMBER(directory, Exists));

  if (exists) {
    // Get the metadata. We only use the persisted flag.
    QM_TRY_INSPECT(const auto& metadata,
                   aQuotaManager.LoadFullOriginMetadataWithRestore(directory));

    mPersisted = metadata.mPersisted;
  } else {
    // The directory has not been created yet.
    mPersisted = false;
  }

  return NS_OK;
}

void PersistedOp::GetResponse(RequestResponse& aResponse) {
  AssertIsOnOwningThread();

  PersistedResponse persistedResponse;
  persistedResponse.persisted() = mPersisted;

  aResponse = persistedResponse;
}

PersistOp::PersistOp(MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
                     const RequestParams& aParams)
    : PersistRequestBase(std::move(aQuotaManager),
                         aParams.get_PersistParams().principalInfo()) {
  MOZ_ASSERT(aParams.type() == RequestParams::TPersistParams);
}

nsresult PersistOp::DoDirectoryWork(QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();
  aQuotaManager.AssertStorageIsInitializedInternal();

  const OriginMetadata originMetadata = {mPrincipalMetadata,
                                         PERSISTENCE_TYPE_DEFAULT};

  AUTO_PROFILER_LABEL("PersistOp::DoDirectoryWork", OTHER);

  // Update directory metadata on disk first. Then, create/update the
  // originInfo if needed.

  QM_TRY_INSPECT(const auto& directory,
                 aQuotaManager.GetOriginDirectory(originMetadata));

  QM_TRY_INSPECT(const bool& created,
                 aQuotaManager.EnsureOriginDirectory(*directory));

  if (created) {
    // A new origin directory has been created.

    // XXX The code below could be converted to a function which returns the
    //     timestamp.
    int64_t timestamp;

    // Update OriginInfo too if temporary origin was already initialized.
    if (aQuotaManager.IsTemporaryStorageInitializedInternal()) {
      if (aQuotaManager.IsTemporaryOriginInitializedInternal(originMetadata)) {
        // We have a temporary origin which has been initialized without
        // ensuring respective origin directory. So OriginInfo already exists
        // and it needs to be updated because the origin directory has been
        // just created.

        timestamp = aQuotaManager.WithOriginInfo(
            originMetadata, [](const auto& originInfo) {
              const int64_t timestamp = originInfo->LockedAccessTime();

              originInfo->LockedDirectoryCreated();

              return timestamp;
            });
      } else {
        timestamp = PR_Now();
      }

      FullOriginMetadata fullOriginMetadata =
          FullOriginMetadata{originMetadata, /* aPersisted */ true, timestamp};

      // Usually, infallible operations are placed after fallible ones.
      // However, since we lack atomic support for creating the origin
      // directory along with its metadata, we need to add the origin to cached
      // origins right after directory creation.
      aQuotaManager.AddTemporaryOrigin(fullOriginMetadata);
    } else {
      timestamp = PR_Now();
    }

    QM_TRY(MOZ_TO_RESULT(QuotaManager::CreateDirectoryMetadata2(
        *directory, timestamp, /* aPersisted */ true, originMetadata)));

    // Update or create OriginInfo too if temporary storage was already
    // initialized.
    if (aQuotaManager.IsTemporaryStorageInitializedInternal()) {
      if (aQuotaManager.IsTemporaryOriginInitializedInternal(originMetadata)) {
        // In this case, we have a temporary origin which has been initialized
        // without ensuring respective origin directory. So OriginInfo already
        // exists and it needs to be updated because the origin directory has
        // been just created.

        aQuotaManager.PersistOrigin(originMetadata);
      } else {
        // In this case, we have a temporary origin which hasn't been
        // initialized yet. So OriginInfo needs to be created because the
        // origin directory has been just created.

        FullOriginMetadata fullOriginMetadata = FullOriginMetadata{
            originMetadata, /* aPersisted */ true, timestamp};

        aQuotaManager.InitQuotaForOrigin(fullOriginMetadata, ClientUsageArray(),
                                         /* aUsageBytes */ 0);
      }
    }
  } else {
    QM_TRY_INSPECT(
        const bool& persisted,
        ([&aQuotaManager, &originMetadata,
          &directory]() -> mozilla::Result<bool, nsresult> {
          Nullable<bool> persisted =
              aQuotaManager.OriginPersisted(originMetadata);

          if (!persisted.IsNull()) {
            return persisted.Value();
          }

          // Get the metadata (restore the metadata file if necessary). We only
          // use the persisted flag.
          QM_TRY_INSPECT(
              const auto& metadata,
              aQuotaManager.LoadFullOriginMetadataWithRestore(directory));

          return metadata.mPersisted;
        }()));

    if (!persisted) {
      QM_TRY_INSPECT(const auto& file,
                     CloneFileAndAppend(
                         *directory, nsLiteralString(METADATA_V2_FILE_NAME)));

      QM_TRY_INSPECT(const auto& stream,
                     GetBinaryOutputStream(*file, FileFlag::Update));

      MOZ_ASSERT(stream);

      // Update origin access time while we are here.
      QM_TRY(MOZ_TO_RESULT(stream->Write64(PR_Now())));

      // Set the persisted flag to true.
      QM_TRY(MOZ_TO_RESULT(stream->WriteBoolean(true)));

      QM_TRY(MOZ_TO_RESULT(stream->Close()));

      // Directory metadata has been successfully updated.
      // Update OriginInfo too if temporary storage was already initialized.
      if (aQuotaManager.IsTemporaryStorageInitializedInternal()) {
        aQuotaManager.PersistOrigin(originMetadata);
      }
    }
  }

  return NS_OK;
}

void PersistOp::GetResponse(RequestResponse& aResponse) {
  AssertIsOnOwningThread();

  aResponse = PersistResponse();
}

EstimateOp::EstimateOp(MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
                       const EstimateParams& aParams)
    : OpenStorageDirectoryHelper(std::move(aQuotaManager),
                                 "dom::quota::EstimateOp"),
      mParams(aParams) {
  AssertIsOnOwningThread();
}

nsresult EstimateOp::DoInit(QuotaManager& aQuotaManager) {
  AssertIsOnOwningThread();

  QM_TRY_UNWRAP(PrincipalMetadata principalMetadata,
                GetInfoFromValidatedPrincipalInfo(aQuotaManager,
                                                  mParams.principalInfo()));

  principalMetadata.AssertInvariants();

  mOriginMetadata = {std::move(principalMetadata), PERSISTENCE_TYPE_DEFAULT};

  return NS_OK;
}

RefPtr<BoolPromise> EstimateOp::OpenDirectory() {
  AssertIsOnOwningThread();

  return OpenStorageDirectory(
      PersistenceScope::CreateFromSet(PERSISTENCE_TYPE_TEMPORARY,
                                      PERSISTENCE_TYPE_DEFAULT,
                                      PERSISTENCE_TYPE_PRIVATE),
      OriginScope::FromGroup(mOriginMetadata.mGroup), Nullable<Client::Type>(),
      /* aExclusive */ false,
      /* aInitializeOrigins */ true);
}

nsresult EstimateOp::DoDirectoryWork(QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();
  aQuotaManager.AssertStorageIsInitializedInternal();

  AUTO_PROFILER_LABEL("EstimateOp::DoDirectoryWork", OTHER);

  // Get cached usage (the method doesn't have to stat any files).
  mUsageAndLimit = aQuotaManager.GetUsageAndLimitForEstimate(mOriginMetadata);

  return NS_OK;
}

void EstimateOp::GetResponse(RequestResponse& aResponse) {
  AssertIsOnOwningThread();

  EstimateResponse estimateResponse;

  estimateResponse.usage() = mUsageAndLimit.first;
  estimateResponse.limit() = mUsageAndLimit.second;

  aResponse = estimateResponse;
}

void EstimateOp::CloseDirectory() {
  AssertIsOnOwningThread();

  SafeDropDirectoryLock(mDirectoryLock);
}

ListOriginsOp::ListOriginsOp(MovingNotNull<RefPtr<QuotaManager>> aQuotaManager)
    : OpenStorageDirectoryHelper(std::move(aQuotaManager),
                                 "dom::quota::ListOriginsOp") {
  AssertIsOnOwningThread();
}

RefPtr<BoolPromise> ListOriginsOp::OpenDirectory() {
  AssertIsOnOwningThread();

  return OpenStorageDirectory(PersistenceScope::CreateFromNull(),
                              OriginScope::FromNull(), Nullable<Client::Type>(),
                              /* aExclusive */ false);
}

nsresult ListOriginsOp::DoDirectoryWork(QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();
  aQuotaManager.AssertStorageIsInitializedInternal();

  AUTO_PROFILER_LABEL("ListOriginsOp::DoDirectoryWork", OTHER);

  for (const PersistenceType type : kAllPersistenceTypes) {
    QM_TRY(MOZ_TO_RESULT(TraverseRepository(aQuotaManager, type)));
  }

  // TraverseRepository above only consulted the file-system to get a list of
  // known origins, but we also need to include origins that have pending
  // quota usage.

  aQuotaManager.CollectPendingOriginsForListing([this](const auto& originInfo) {
    mOrigins.AppendElement(originInfo->Origin());
  });

  return NS_OK;
}

const Atomic<bool>& ListOriginsOp::GetIsCanceledFlag() {
  AssertIsOnIOThread();

  return Canceled();
}

nsresult ListOriginsOp::ProcessOrigin(QuotaManager& aQuotaManager,
                                      nsIFile& aOriginDir,
                                      const bool aPersistent,
                                      const PersistenceType aPersistenceType) {
  AssertIsOnIOThread();

  QM_TRY_UNWRAP(auto maybeMetadata,
                QM_OR_ELSE_WARN_IF(
                    // Expression
                    aQuotaManager.GetOriginMetadata(&aOriginDir)
                        .map([](auto metadata) -> Maybe<OriginMetadata> {
                          return Some(std::move(metadata));
                        }),
                    // Predicate.
                    IsSpecificError<NS_ERROR_MALFORMED_URI>,
                    // Fallback.
                    ErrToDefaultOk<Maybe<OriginMetadata>>));

  if (!maybeMetadata) {
    // Unknown directories during listing are allowed. Just warn if we find
    // them.
    QM_TRY_INSPECT(const auto& leafName,
                   MOZ_TO_RESULT_INVOKE_MEMBER_TYPED(nsAutoString, aOriginDir,
                                                     GetLeafName));

    UNKNOWN_FILE_WARNING(leafName);
    return NS_OK;
  }

  auto metadata = maybeMetadata.extract();

  if (aQuotaManager.IsOriginInternal(metadata.mOrigin)) {
    return NS_OK;
  }

  mOrigins.AppendElement(std::move(metadata.mOrigin));

  return NS_OK;
}

void ListOriginsOp::GetResponse(RequestResponse& aResponse) {
  AssertIsOnOwningThread();

  aResponse = ListOriginsResponse();
  if (mOrigins.IsEmpty()) {
    return;
  }

  nsTArray<nsCString>& origins = aResponse.get_ListOriginsResponse().origins();
  mOrigins.SwapElements(origins);
}

void ListOriginsOp::CloseDirectory() {
  AssertIsOnOwningThread();

  SafeDropDirectoryLock(mDirectoryLock);
}

}  // namespace mozilla::dom::quota
