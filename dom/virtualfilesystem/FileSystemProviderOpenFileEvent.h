/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FileSystemProviderOpenFileEvent_h
#define mozilla_dom_FileSystemProviderOpenFileEvent_h

#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/FileSystemProviderBinding.h"
#include "mozilla/dom/FileSystemProviderEvent.h"
#include "mozilla/dom/FileSystemProviderOpenFileEventBinding.h"
#include "nsWrapperCache.h"

class nsIVirtualFileSystemOpenFileRequestOption;

namespace mozilla {
namespace dom {

class OpenFileRequestedOptions final : public FileSystemProviderRequestedOptions
                                     , public nsIVirtualFileSystemOpenFileRequestOption
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(OpenFileRequestedOptions,
                                           FileSystemProviderRequestedOptions)
  NS_FORWARD_NSIVIRTUALFILESYSTEMREQUESTOPTION(FileSystemProviderRequestedOptions::)
  NS_DECL_NSIVIRTUALFILESYSTEMOPENFILEREQUESTOPTION

  explicit OpenFileRequestedOptions() = default;

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  void GetFilePath(nsAString& aPath) const
  {
    aPath = mFilePath;
  }

  OpenFileMode Mode() const
  {
    return mMode;
  }

private:
  ~OpenFileRequestedOptions() {}

  nsString mFilePath;
  OpenFileMode mMode;
};

class FileSystemProviderOpenFileEvent final : public FileSystemProviderEvent
{
public:
  FileSystemProviderOpenFileEvent(EventTarget* aOwner,
                                  nsIVirtualFileSystemRequestManager* aManager);

  virtual JSObject* WrapObjectInternal(JSContext* aCx,
                                       JS::Handle<JSObject*> aGivenProto) override;

  OpenFileRequestedOptions* Options() const;

  virtual nsresult InitFileSystemProviderEvent(uint32_t aRequestId,
                                               nsIVirtualFileSystemRequestOption* aOption) override;

  void SuccessCallback();

private:
  virtual ~FileSystemProviderOpenFileEvent() = default;

};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_FileSystemProviderOpenFileEvent_h
