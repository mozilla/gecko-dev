/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GMPStorageParent_h_
#define GMPStorageParent_h_

#include "mozilla/gmp/PGMPStorageParent.h"
#include "gmp-storage.h"
#include "mozilla/UniquePtr.h"

namespace mozilla {
namespace gmp {

class GMPParent;

class GMPStorage {
public:
  virtual ~GMPStorage() {}

  virtual GMPErr Open(const nsCString& aRecordName) = 0;
  virtual bool IsOpen(const nsCString& aRecordName) = 0;
  virtual GMPErr Read(const nsCString& aRecordName,
                      nsTArray<uint8_t>& aOutBytes) = 0;
  virtual GMPErr Write(const nsCString& aRecordName,
                       const nsTArray<uint8_t>& aBytes) = 0;
  virtual GMPErr GetRecordNames(nsTArray<nsCString>& aOutRecordNames) = 0;
  virtual void Close(const nsCString& aRecordName) = 0;
};

class GMPStorageParent : public PGMPStorageParent {
public:
  NS_INLINE_DECL_REFCOUNTING(GMPStorageParent)
  GMPStorageParent(const nsCString& aNodeId,
                   GMPParent* aPlugin);

  nsresult Init();
  void Shutdown();

protected:
  virtual bool RecvOpen(const nsCString& aRecordName) override;
  virtual bool RecvRead(const nsCString& aRecordName) override;
  virtual bool RecvWrite(const nsCString& aRecordName,
                         InfallibleTArray<uint8_t>&& aBytes) override;
  virtual bool RecvGetRecordNames() override;
  virtual bool RecvClose(const nsCString& aRecordName) override;
  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

private:
  ~GMPStorageParent() {}

  UniquePtr<GMPStorage> mStorage;

  const nsCString mNodeId;
  nsRefPtr<GMPParent> mPlugin;
  bool mShutdown;
};

} // namespace gmp
} // namespace mozilla

#endif // GMPStorageParent_h_
