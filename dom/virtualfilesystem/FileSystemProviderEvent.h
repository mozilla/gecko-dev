/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FileSystemProviderEvent_h
#define mozilla_dom_FileSystemProviderEvent_h

#include "nsIVirtualFileSystemRequestOption.h"
#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/FileSystemProviderEventBinding.h"

class nsIVirtualFileSystemRequestManager;
class nsIVirtualFileSystemRequestOption;
class nsIVirtualFileSystemRequestValue;

namespace mozilla {
namespace dom {

class FileSystemProviderRequestedOptions : public nsIVirtualFileSystemRequestOption
                                         , public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(FileSystemProviderRequestedOptions)
  NS_DECL_NSIVIRTUALFILESYSTEMREQUESTOPTION

  explicit FileSystemProviderRequestedOptions() = default;

  nsISupports* GetParentObject() const
  {
    return mParent;
  }

  void SetParentObject(nsISupports* aParent)
  {
    mParent = aParent;
  }

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  uint32_t RequestId() const
  {
    return mRequestId;
  }

  void SetRequestId(uint32_t aRequestId)
  {
    mRequestId = aRequestId;
  }

protected:
  virtual ~FileSystemProviderRequestedOptions() = default;

  nsCOMPtr<nsISupports> mParent;
  nsString mFileSystemId;
  uint32_t mRequestId;
};

class FileSystemProviderEvent : public Event
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(FileSystemProviderEvent,
                                                         Event)

  explicit FileSystemProviderEvent(EventTarget* aOwner,
                                   nsIVirtualFileSystemRequestManager* aManager);

  virtual JSObject* WrapObjectInternal(JSContext* aCx,
                                       JS::Handle<JSObject*> aGivenProto) override;

  virtual nsresult InitFileSystemProviderEvent(uint32_t aRequestId,
                                               nsIVirtualFileSystemRequestOption* aOption) = 0;

  virtual void OnSuccess(nsIVirtualFileSystemRequestValue* aValue, bool aHasMore);
  void ErrorCallback(const FileSystemProviderError& aError);

protected:
  virtual ~FileSystemProviderEvent();
  void InitFileSystemProviderEventInternal(const nsAString& aType,
                                           uint32_t aRequestId,
                                           nsIVirtualFileSystemRequestOption* aOption);

  nsCOMPtr<nsIVirtualFileSystemRequestManager> mRequestManager;
  RefPtr<FileSystemProviderRequestedOptions> mOptions;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_FileSystemProviderEvent_h
