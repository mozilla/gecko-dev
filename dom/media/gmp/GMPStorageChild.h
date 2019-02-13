/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GMPStorageChild_h_
#define GMPStorageChild_h_

#include "mozilla/gmp/PGMPStorageChild.h"
#include "gmp-storage.h"
#include "nsTHashtable.h"
#include "nsRefPtrHashtable.h"
#include "gmp-platform.h"

#include <queue>

namespace mozilla {
namespace gmp {

class GMPChild;
class GMPStorageChild;

class GMPRecordImpl : public GMPRecord
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(GMPRecordImpl)

  GMPRecordImpl(GMPStorageChild* aOwner,
                const nsCString& aName,
                GMPRecordClient* aClient);

  // GMPRecord.
  virtual GMPErr Open() override;
  virtual GMPErr Read() override;
  virtual GMPErr Write(const uint8_t* aData,
                       uint32_t aDataSize) override;
  virtual GMPErr Close() override;

  const nsCString& Name() const { return mName; }

  void OpenComplete(GMPErr aStatus);
  void ReadComplete(GMPErr aStatus, const uint8_t* aBytes, uint32_t aLength);
  void WriteComplete(GMPErr aStatus);

private:
  ~GMPRecordImpl() {}
  const nsCString mName;
  GMPRecordClient* const mClient;
  GMPStorageChild* const mOwner;
};

class GMPStorageChild : public PGMPStorageChild
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(GMPStorageChild)

  explicit GMPStorageChild(GMPChild* aPlugin);

  GMPErr CreateRecord(const nsCString& aRecordName,
                      GMPRecord** aOutRecord,
                      GMPRecordClient* aClient);

  GMPErr Open(GMPRecordImpl* aRecord);

  GMPErr Read(GMPRecordImpl* aRecord);

  GMPErr Write(GMPRecordImpl* aRecord,
               const uint8_t* aData,
               uint32_t aDataSize);

  GMPErr Close(const nsCString& aRecordName);

  GMPErr EnumerateRecords(RecvGMPRecordIteratorPtr aRecvIteratorFunc,
                          void* aUserArg);

private:
  bool HasRecord(const nsCString& aRecordName);
  already_AddRefed<GMPRecordImpl> GetRecord(const nsCString& aRecordName);

protected:
  ~GMPStorageChild() {}

  // PGMPStorageChild
  virtual bool RecvOpenComplete(const nsCString& aRecordName,
                                const GMPErr& aStatus) override;
  virtual bool RecvReadComplete(const nsCString& aRecordName,
                                const GMPErr& aStatus,
                                InfallibleTArray<uint8_t>&& aBytes) override;
  virtual bool RecvWriteComplete(const nsCString& aRecordName,
                                 const GMPErr& aStatus) override;
  virtual bool RecvRecordNames(InfallibleTArray<nsCString>&& aRecordNames,
                               const GMPErr& aStatus) override;
  virtual bool RecvShutdown() override;

private:
  Monitor mMonitor;
  nsRefPtrHashtable<nsCStringHashKey, GMPRecordImpl> mRecords;
  GMPChild* mPlugin;

  struct RecordIteratorContext {
    explicit RecordIteratorContext(RecvGMPRecordIteratorPtr aFunc,
                                   void* aUserArg)
      : mFunc(aFunc)
      , mUserArg(aUserArg)
    {}
    RecordIteratorContext() {}
    RecvGMPRecordIteratorPtr mFunc;
    void* mUserArg;
  };

  std::queue<RecordIteratorContext> mPendingRecordIterators;
  bool mShutdown;
};

} // namespace gmp
} // namespace mozilla

#endif // GMPStorageChild_h_
