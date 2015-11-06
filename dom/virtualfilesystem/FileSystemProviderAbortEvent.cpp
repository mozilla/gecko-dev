/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/FileSystemProviderAbortEvent.h"
#include "nsIVirtualFileSystemRequestOption.h"
#include "nsIVirtualFileSystemRequestManager.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(AbortRequestedOptions)

NS_IMPL_ADDREF_INHERITED(AbortRequestedOptions, FileSystemProviderRequestedOptions)
NS_IMPL_RELEASE_INHERITED(AbortRequestedOptions, FileSystemProviderRequestedOptions)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(AbortRequestedOptions,
                                                  FileSystemProviderRequestedOptions)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(AbortRequestedOptions,
                                                FileSystemProviderRequestedOptions)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(AbortRequestedOptions)
NS_INTERFACE_MAP_ENTRY(nsIVirtualFileSystemAbortRequestOption)
NS_INTERFACE_MAP_END_INHERITING(FileSystemProviderRequestedOptions)

JSObject*
AbortRequestedOptions::WrapObject(JSContext* aCx,
                                  JS::Handle<JSObject*> aGivenProto)
{
  return AbortRequestedOptionsBinding::Wrap(aCx, this, aGivenProto);
}

NS_IMETHODIMP
AbortRequestedOptions::GetOperationRequestId(uint32_t *aOperationRequestId)
{
  NS_ENSURE_ARG_POINTER(aOperationRequestId);

  *aOperationRequestId = mOperationRequestId;
  return NS_OK;
}

NS_IMETHODIMP
AbortRequestedOptions::SetOperationRequestId(uint32_t aOperationRequestId)
{
  mOperationRequestId = aOperationRequestId;
  return NS_OK;
}

FileSystemProviderAbortEvent::FileSystemProviderAbortEvent(EventTarget* aOwner,
                                                           nsIVirtualFileSystemRequestManager* aManager)
  : FileSystemProviderEvent(aOwner, aManager)
{

}

JSObject*
FileSystemProviderAbortEvent::WrapObjectInternal(JSContext* aCx,
                                                 JS::Handle<JSObject*> aGivenProto)
{
  return FileSystemProviderAbortEventBinding::Wrap(aCx, this, aGivenProto);
}

AbortRequestedOptions*
FileSystemProviderAbortEvent::Options() const
{
  MOZ_ASSERT(mOptions);

  return static_cast<AbortRequestedOptions*>(mOptions.get());
}

nsresult
FileSystemProviderAbortEvent::InitFileSystemProviderEvent(
  uint32_t aRequestId,
  nsIVirtualFileSystemRequestOption* aOption)
{
  nsCOMPtr<nsIVirtualFileSystemAbortRequestOption> option = do_QueryInterface(aOption);
  if (!option) {
    MOZ_ASSERT(false);
    return NS_ERROR_INVALID_ARG;
  }

  InitFileSystemProviderEventInternal(NS_LITERAL_STRING("abortrequested"),
                                      aRequestId,
                                      option);
  return NS_OK;
}

void
FileSystemProviderAbortEvent::SuccessCallback()
{
  FileSystemProviderEvent::OnSuccess(nullptr, false);
}

} // namespace dom
} // namespace mozilla
