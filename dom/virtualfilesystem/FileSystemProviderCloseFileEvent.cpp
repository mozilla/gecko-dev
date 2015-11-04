/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/FileSystemProviderCloseFileEvent.h"
#include "nsIVirtualFileSystemRequestOption.h"
#include "nsIVirtualFileSystemRequestManager.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(CloseFileRequestedOptions)

NS_IMPL_ADDREF_INHERITED(CloseFileRequestedOptions, FileSystemProviderRequestedOptions)
NS_IMPL_RELEASE_INHERITED(CloseFileRequestedOptions, FileSystemProviderRequestedOptions)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(CloseFileRequestedOptions,
                                                  FileSystemProviderRequestedOptions)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(CloseFileRequestedOptions,
                                                FileSystemProviderRequestedOptions)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(CloseFileRequestedOptions)
NS_INTERFACE_MAP_ENTRY(nsIVirtualFileSystemCloseFileRequestOption)
NS_INTERFACE_MAP_END_INHERITING(FileSystemProviderRequestedOptions)

JSObject*
CloseFileRequestedOptions::WrapObject(JSContext* aCx,
                                  JS::Handle<JSObject*> aGivenProto)
{
  return CloseFileRequestedOptionsBinding::Wrap(aCx, this, aGivenProto);
}

NS_IMETHODIMP
CloseFileRequestedOptions::GetOpenRequestId(uint32_t* aOpenRequestId)
{
  NS_ENSURE_ARG_POINTER(aOpenRequestId);

  *aOpenRequestId = mOpenRequestId;
  return NS_OK;
}

NS_IMETHODIMP
CloseFileRequestedOptions
::SetOpenRequestId(uint32_t aOpenRequestId)
{
   mOpenRequestId = aOpenRequestId;
  return NS_OK;
}

FileSystemProviderCloseFileEvent::FileSystemProviderCloseFileEvent(
  EventTarget* aOwner,
  nsIVirtualFileSystemRequestManager* aManager)
  : FileSystemProviderEvent(aOwner, aManager)
{
}

JSObject*
FileSystemProviderCloseFileEvent::WrapObjectInternal(JSContext* aCx,
                                                     JS::Handle<JSObject*> aGivenProto)
{
  return FileSystemProviderCloseFileEventBinding::Wrap(aCx, this, aGivenProto);
}

CloseFileRequestedOptions*
FileSystemProviderCloseFileEvent::Options() const
{
  MOZ_ASSERT(mOptions);

  return static_cast<CloseFileRequestedOptions*>(mOptions.get());
}

nsresult
FileSystemProviderCloseFileEvent::InitFileSystemProviderEvent(
  uint32_t aRequestId,
  nsIVirtualFileSystemRequestOption* aOption)
{
  nsCOMPtr<nsIVirtualFileSystemCloseFileRequestOption> option = do_QueryInterface(aOption);
  if (!option) {
    MOZ_ASSERT(false);
    return NS_ERROR_INVALID_ARG;
  }

  InitFileSystemProviderEventInternal(NS_LITERAL_STRING("closefilerequested"),
                                      aRequestId,
                                      option);
  return NS_OK;
}

void
FileSystemProviderCloseFileEvent::SuccessCallback()
{
  FileSystemProviderEvent::OnSuccess(nullptr, false);
}

} // namespace dom
} // namespace mozilla
