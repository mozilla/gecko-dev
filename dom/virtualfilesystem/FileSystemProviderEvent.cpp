/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/FileSystemProviderEvent.h"
#include "nsIVirtualFileSystemRequestManager.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(FileSystemProviderRequestedOptions, mParent)

NS_IMPL_CYCLE_COLLECTING_ADDREF(FileSystemProviderRequestedOptions)
NS_IMPL_CYCLE_COLLECTING_RELEASE(FileSystemProviderRequestedOptions)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(FileSystemProviderRequestedOptions)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsIVirtualFileSystemRequestOption)
NS_INTERFACE_MAP_END

JSObject*
FileSystemProviderRequestedOptions::WrapObject(JSContext* aCx,
                                               JS::Handle<JSObject*> aGivenProto)
{
  return FileSystemProviderRequestedOptionsBinding::Wrap(aCx, this, aGivenProto);
}

NS_IMETHODIMP
FileSystemProviderRequestedOptions::GetFileSystemId(nsAString& aFileSystemId)
{
  aFileSystemId = mFileSystemId;
  return NS_OK;
}

NS_IMETHODIMP
FileSystemProviderRequestedOptions::SetFileSystemId(const nsAString& aFileSystemId)
{
  mFileSystemId = aFileSystemId;
  return NS_OK;
}

NS_IMPL_CYCLE_COLLECTION_CLASS(FileSystemProviderEvent)

NS_IMPL_ADDREF_INHERITED(FileSystemProviderEvent, Event)
NS_IMPL_RELEASE_INHERITED(FileSystemProviderEvent, Event)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(FileSystemProviderEvent, Event)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mRequestManager)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mOptions)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(FileSystemProviderEvent, Event)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(FileSystemProviderEvent, Event)
NS_IMPL_CYCLE_COLLECTION_UNLINK(mRequestManager)
NS_IMPL_CYCLE_COLLECTION_UNLINK(mOptions)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(FileSystemProviderEvent)
NS_INTERFACE_MAP_END_INHERITING(Event)

FileSystemProviderEvent::FileSystemProviderEvent(EventTarget* aOwner,
                                                 nsIVirtualFileSystemRequestManager* aManager)
  : Event(aOwner, nullptr, nullptr)
  , mRequestManager(aManager)
  , mOptions(nullptr)
{
}

FileSystemProviderEvent::~FileSystemProviderEvent()
{

}

JSObject*
FileSystemProviderEvent::WrapObjectInternal(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return FileSystemProviderEventBinding::Wrap(aCx, this, aGivenProto);
}

void
FileSystemProviderEvent::OnSuccess(nsIVirtualFileSystemRequestValue* aValue, bool aHasMore)
{
  if (!mOptions) {
    return;
  }

  mRequestManager->FufillRequest(mOptions->RequestId(), aValue, aHasMore);
}

void
FileSystemProviderEvent::ErrorCallback(const FileSystemProviderError& aError)
{
  if (!mOptions) {
    return;
  }

  mRequestManager->RejectRequest(mOptions->RequestId(), static_cast<uint32_t>(aError));
}

void
FileSystemProviderEvent::InitFileSystemProviderEventInternal(const nsAString& aType,
                                                             uint32_t aRequestId,
                                                             nsIVirtualFileSystemRequestOption* aOption)
{
  Event::InitEvent(aType, false, false);

  mOptions = static_cast<FileSystemProviderRequestedOptions*>(aOption);
  mOptions->SetParentObject(mOwner);
  mOptions->SetRequestId(aRequestId);
}

} // namespace dom
} // namespace mozilla
