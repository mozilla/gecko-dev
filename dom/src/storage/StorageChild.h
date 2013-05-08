/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 8 -*- */
/* vim: set sw=4 ts=8 et tw=80 ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_StorageChild_h
#define mozilla_dom_StorageChild_h

#include "mozilla/dom/PStorageChild.h"
#include "nsDOMStorage.h"
#include "nsCycleCollectionParticipant.h"

namespace mozilla {
namespace dom {

class StorageChild : public PStorageChild
                   , public DOMStorageBase
                   , public nsSupportsWeakReference
{
public:
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(StorageChild, nsIPrivacyTransitionObserver)
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_NSIPRIVACYTRANSITIONOBSERVER
  
  StorageChild(nsDOMStorage* aOwner);
  StorageChild(nsDOMStorage* aOwner, StorageChild& aOther);

  virtual void InitAsSessionStorage(nsIPrincipal* aPrincipal, bool aPrivate);
  virtual void InitAsLocalStorage(nsIPrincipal* aPrincipal, bool aPrivate);

  virtual bool CacheStoragePermissions();
  
  virtual nsTArray<nsCString>* GetKeys(bool aCallerSecure);
  virtual nsresult GetLength(bool aCallerSecure, uint32_t* aLength);
  virtual nsresult GetKey(bool aCallerSecure, uint32_t aIndex, 
                          nsACString& aKey);
  virtual nsIDOMStorageItem* GetValue(bool aCallerSecure,
                                      const nsACString& aKey, nsresult* rv);
  virtual nsresult SetValue(bool aCallerSecure, const nsACString& aKey,
                            const nsACString& aData, nsACString& aOldValue);
  virtual nsresult RemoveValue(bool aCallerSecure, const nsACString& aKey,
                               nsACString& aOldValue);
  virtual nsresult Clear(bool aCallerSecure, int32_t* aOldCount);

  virtual nsresult GetDBValue(const nsACString& aKey,
                              nsACString& aValue,
                              bool* aSecure);
  virtual nsresult SetDBValue(const nsACString& aKey,
                              const nsACString& aValue,
                              bool aSecure);
  virtual nsresult SetSecure(const nsACString& aKey, bool aSecure);

  virtual nsresult CloneFrom(bool aCallerSecure, DOMStorageBase* aThat);

  void AddIPDLReference();
  void ReleaseIPDLReference();

  virtual void MarkOwnerDead();

private:
  void InitRemote();

  // Unimplemented
  StorageChild(const StorageChild&);

  nsCOMPtr<nsIDOMStorageObsolete> mStorage;
  bool mIPCOpen;
};

}
}

#endif
