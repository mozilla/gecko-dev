/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_URLSearchParams_h
#define mozilla_dom_URLSearchParams_h

#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/ErrorResult.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"
#include "nsClassHashtable.h"
#include "nsHashKeys.h"
#include "nsISupports.h"

namespace mozilla {
namespace dom {

class URLSearchParamsObserver : public nsISupports
{
public:
  virtual ~URLSearchParamsObserver() {}

  virtual void URLSearchParamsUpdated() = 0;
};

class URLSearchParams MOZ_FINAL : public nsISupports,
                                  public nsWrapperCache
{
  ~URLSearchParams();

public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(URLSearchParams)

  URLSearchParams();

  // WebIDL methods
  nsISupports* GetParentObject() const
  {
    return nullptr;
  }

  virtual JSObject*
  WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  static already_AddRefed<URLSearchParams>
  Constructor(const GlobalObject& aGlobal, const nsAString& aInit,
              ErrorResult& aRv);

  static already_AddRefed<URLSearchParams>
  Constructor(const GlobalObject& aGlobal, URLSearchParams& aInit,
              ErrorResult& aRv);

  void ParseInput(const nsACString& aInput,
                  URLSearchParamsObserver* aObserver);

  void AddObserver(URLSearchParamsObserver* aObserver);
  void RemoveObserver(URLSearchParamsObserver* aObserver);

  void Serialize(nsAString& aValue) const;

  void Get(const nsAString& aName, nsString& aRetval);

  void GetAll(const nsAString& aName, nsTArray<nsString >& aRetval);

  void Set(const nsAString& aName, const nsAString& aValue);

  void Append(const nsAString& aName, const nsAString& aValue);

  bool Has(const nsAString& aName);

  void Delete(const nsAString& aName);

  void Stringify(nsString& aRetval)
  {
    Serialize(aRetval);
  }

private:
  void AppendInternal(const nsAString& aName, const nsAString& aValue);

  void DeleteAll();

  void DecodeString(const nsACString& aInput, nsACString& aOutput);

  void NotifyObservers(URLSearchParamsObserver* aExceptObserver);

  static PLDHashOperator
  CopyEnumerator(const nsAString& aName, nsTArray<nsString>* aArray,
                 void *userData);

  static PLDHashOperator
  SerializeEnumerator(const nsAString& aName, nsTArray<nsString>* aArray,
                      void *userData);

  nsClassHashtable<nsStringHashKey, nsTArray<nsString>> mSearchParams;

  nsTArray<nsRefPtr<URLSearchParamsObserver>> mObservers;
};

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_URLSearchParams_h */
