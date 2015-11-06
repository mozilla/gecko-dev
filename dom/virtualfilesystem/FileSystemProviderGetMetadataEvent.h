/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FileSystemProviderGetMetadataEvent_h
#define mozilla_dom_FileSystemProviderGetMetadataEvent_h

#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/FileSystemProviderEvent.h"
#include "mozilla/dom/FileSystemProviderGetMetadataEventBinding.h"
#include "nsWrapperCache.h"

class nsIVirtualFileSystemGetMetadataRequestOption;

namespace mozilla {
namespace dom {

class GetMetadataRequestedOptions final : public FileSystemProviderRequestedOptions
                                        , public nsIVirtualFileSystemGetMetadataRequestOption
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(GetMetadataRequestedOptions,
                                           FileSystemProviderRequestedOptions)
  NS_FORWARD_NSIVIRTUALFILESYSTEMREQUESTOPTION(FileSystemProviderRequestedOptions::)
  NS_DECL_NSIVIRTUALFILESYSTEMGETMETADATAREQUESTOPTION

  explicit GetMetadataRequestedOptions() = default;

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  void GetEntryPath(nsAString& aPath) const
  {
    aPath = mEntryPath;
  }

private:
  virtual ~GetMetadataRequestedOptions() = default;

  nsString mEntryPath;
};

class FileSystemProviderGetMetadataEvent final : public FileSystemProviderEvent
{
public:
  FileSystemProviderGetMetadataEvent(EventTarget* aOwner,
                                     nsIVirtualFileSystemRequestManager* aManager);

  virtual JSObject* WrapObjectInternal(JSContext* aCx,
                                       JS::Handle<JSObject*> aGivenProto) override;

  GetMetadataRequestedOptions* Options() const;

  virtual nsresult InitFileSystemProviderEvent(uint32_t aRequestId,
                                               nsIVirtualFileSystemRequestOption* aOption) override;

  void SuccessCallback(const EntryMetadata& aData);

private:
  ~FileSystemProviderGetMetadataEvent() = default;

};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_FileSystemProviderGetMetadataEvent_h
