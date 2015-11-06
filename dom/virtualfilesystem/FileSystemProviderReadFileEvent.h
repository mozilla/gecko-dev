/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FileSystemProviderReadFileEvent_h
#define mozilla_dom_FileSystemProviderReadFileEvent_h

#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/FileSystemProviderEvent.h"
#include "mozilla/dom/FileSystemProviderReadFileEventBinding.h"
#include "mozilla/dom/TypedArray.h"
#include "nsWrapperCache.h"

class nsIVirtualFileSystemReadFileRequestOption;

namespace mozilla {
namespace dom {

class ReadFileRequestedOptions final : public FileSystemProviderRequestedOptions
                                     , public nsIVirtualFileSystemReadFileRequestOption
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(ReadFileRequestedOptions,
                                           FileSystemProviderRequestedOptions)
  NS_FORWARD_NSIVIRTUALFILESYSTEMREQUESTOPTION(FileSystemProviderRequestedOptions::)
  NS_DECL_NSIVIRTUALFILESYSTEMREADFILEREQUESTOPTION

  explicit ReadFileRequestedOptions() = default;

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  uint32_t OpenRequestId() const
  {
    return mOpenRequestId;
  }

  uint64_t Offset() const
  {
    return mOffset;
  }

  uint64_t Length() const
  {
    return mLength;
  }

private:
  virtual ~ReadFileRequestedOptions() = default;

  uint32_t mOpenRequestId;
  uint64_t mOffset;
  uint64_t mLength;
};

class FileSystemProviderReadFileEvent final : public FileSystemProviderEvent
{
public:
  FileSystemProviderReadFileEvent(EventTarget* aOwner,
                                  nsIVirtualFileSystemRequestManager* aManager);

  virtual JSObject* WrapObjectInternal(JSContext* aCx,
                                       JS::Handle<JSObject*> aGivenProto) override;

  ReadFileRequestedOptions* Options() const;

  virtual nsresult InitFileSystemProviderEvent(uint32_t aRequestId,
                                               nsIVirtualFileSystemRequestOption* aOption) override;

  void SuccessCallback(const ArrayBuffer& aData, bool aHasMore);

private:
  virtual ~FileSystemProviderReadFileEvent() = default;

};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_FileSystemProviderReadFileEvent_h
