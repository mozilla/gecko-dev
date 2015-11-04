/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FileSystemProviderCloseFileEvent_h
#define mozilla_dom_FileSystemProviderCloseFileEvent_h

#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/FileSystemProviderEvent.h"
#include "mozilla/dom/FileSystemProviderCloseFileEventBinding.h"
#include "nsWrapperCache.h"

class nsIVirtualFileSystemCloseFileRequestOption;

namespace mozilla {
namespace dom {

class CloseFileRequestedOptions final : public FileSystemProviderRequestedOptions
                                      , public nsIVirtualFileSystemCloseFileRequestOption
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(CloseFileRequestedOptions,
                                           FileSystemProviderRequestedOptions)
  NS_FORWARD_NSIVIRTUALFILESYSTEMREQUESTOPTION(FileSystemProviderRequestedOptions::)
  NS_DECL_NSIVIRTUALFILESYSTEMCLOSEFILEREQUESTOPTION

  explicit CloseFileRequestedOptions() = default;

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  uint32_t OpenRequestId() const
  {
    return mOpenRequestId;
  }

private:
  virtual ~CloseFileRequestedOptions() = default;

  uint32_t mOpenRequestId;
};

class FileSystemProviderCloseFileEvent final : public FileSystemProviderEvent
{
public:
  FileSystemProviderCloseFileEvent(EventTarget* aOwner,
                                   nsIVirtualFileSystemRequestManager* aManager);

  virtual JSObject* WrapObjectInternal(JSContext* aCx,
                                       JS::Handle<JSObject*> aGivenProto) override;

  CloseFileRequestedOptions* Options() const;

  virtual nsresult InitFileSystemProviderEvent(uint32_t aRequestId,
                                               nsIVirtualFileSystemRequestOption* aOption) override;

  void SuccessCallback();

private:
  ~FileSystemProviderCloseFileEvent() = default;

};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_FileSystemProviderCloseFileEvent_h
