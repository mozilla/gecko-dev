/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workernavigator_h__
#define mozilla_dom_workernavigator_h__

#include "Workers.h"
#include "RuntimeService.h"
#include "nsString.h"
#include "nsWrapperCache.h"
#include "mozilla/dom/StorageManager.h"

namespace mozilla {
namespace dom {
class Promise;
class StorageManager;

class WorkerNavigator final : public nsWrapperCache
{
  typedef struct workers::RuntimeService::NavigatorProperties NavigatorProperties;

  NavigatorProperties mProperties;
  RefPtr<StorageManager> mStorageManager;
  bool mOnline;

  WorkerNavigator(const NavigatorProperties& aProperties,
                  bool aOnline)
    : mProperties(aProperties)
    , mOnline(aOnline)
  {
    MOZ_COUNT_CTOR(WorkerNavigator);
  }

  ~WorkerNavigator()
  {
    MOZ_COUNT_DTOR(WorkerNavigator);
  }

public:

  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(WorkerNavigator)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(WorkerNavigator)

  static already_AddRefed<WorkerNavigator>
  Create(bool aOnLine);

  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  nsISupports* GetParentObject() const {
    return nullptr;
  }

  void GetAppCodeName(nsString& aAppCodeName) const
  {
    aAppCodeName.AssignLiteral("Mozilla");
  }
  void GetAppName(nsString& aAppName) const;

  void GetAppVersion(nsString& aAppVersion) const;

  void GetPlatform(nsString& aPlatform) const;

  void GetProduct(nsString& aProduct) const
  {
    aProduct.AssignLiteral("Gecko");
  }

  bool TaintEnabled() const
  {
    return false;
  }

  void GetLanguage(nsString& aLanguage) const
  {
    if (mProperties.mLanguages.Length() >= 1) {
      aLanguage.Assign(mProperties.mLanguages[0]);
    } else {
      aLanguage.Truncate();
    }
  }

  void GetLanguages(nsTArray<nsString>& aLanguages) const
  {
    aLanguages = mProperties.mLanguages;
  }

  void GetUserAgent(nsString& aUserAgent, ErrorResult& aRv) const;

  bool OnLine() const
  {
    return mOnline;
  }

  // Worker thread only!
  void SetOnLine(bool aOnline)
  {
    mOnline = aOnline;
  }

  void SetLanguages(const nsTArray<nsString>& aLanguages);

  uint64_t HardwareConcurrency() const;

  StorageManager* Storage();
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_workernavigator_h__
