/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FileSystemProviderAbortEvent_h
#define mozilla_dom_FileSystemProviderAbortEvent_h

#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/FileSystemProviderEvent.h"
#include "mozilla/dom/FileSystemProviderAbortEventBinding.h"
#include "nsWrapperCache.h"

class nsIVirtualFileSystemAbortRequestOption;

namespace mozilla {
namespace dom {

class AbortRequestedOptions final : public FileSystemProviderRequestedOptions
                                  , public nsIVirtualFileSystemAbortRequestOption
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(AbortRequestedOptions,
                                           FileSystemProviderRequestedOptions)
  NS_FORWARD_NSIVIRTUALFILESYSTEMREQUESTOPTION(FileSystemProviderRequestedOptions::)
  NS_DECL_NSIVIRTUALFILESYSTEMABORTREQUESTOPTION

  explicit AbortRequestedOptions() = default;

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  uint32_t OperationRequestId() const
  {
    return mOperationRequestId;
  }

private:
  virtual ~AbortRequestedOptions() = default;

  uint32_t mOperationRequestId;
};

class FileSystemProviderAbortEvent final : public FileSystemProviderEvent
{
public:
  FileSystemProviderAbortEvent(EventTarget* aOwner,
                               nsIVirtualFileSystemRequestManager* aManager);

  virtual JSObject* WrapObjectInternal(JSContext* aCx,
                                       JS::Handle<JSObject*> aGivenProto) override;

  AbortRequestedOptions* Options() const;

  virtual nsresult InitFileSystemProviderEvent(uint32_t aRequestId,
                                               nsIVirtualFileSystemRequestOption* aOption) override;

  void SuccessCallback();

private:
  ~FileSystemProviderAbortEvent() = default;

};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_FileSystemProviderAbortEvent_h
