/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/FileSystemProviderOpenFileEvent.h"
#include "nsIVirtualFileSystemRequestOption.h"
#include "nsIVirtualFileSystemRequestManager.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(OpenFileRequestedOptions)

NS_IMPL_ADDREF_INHERITED(OpenFileRequestedOptions, FileSystemProviderRequestedOptions)
NS_IMPL_RELEASE_INHERITED(OpenFileRequestedOptions, FileSystemProviderRequestedOptions)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(OpenFileRequestedOptions,
                                                  FileSystemProviderRequestedOptions)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(OpenFileRequestedOptions,
                                                FileSystemProviderRequestedOptions)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(OpenFileRequestedOptions)
NS_INTERFACE_MAP_ENTRY(nsIVirtualFileSystemOpenFileRequestOption)
NS_INTERFACE_MAP_END_INHERITING(FileSystemProviderRequestedOptions)

JSObject*
OpenFileRequestedOptions::WrapObject(JSContext* aCx,
                                     JS::Handle<JSObject*> aGivenProto)
{
  return OpenFileRequestedOptionsBinding::Wrap(aCx, this, aGivenProto);
}

NS_IMETHODIMP
OpenFileRequestedOptions::GetFilePath(nsAString& aFilePath)
{
  aFilePath = mFilePath;
  return NS_OK;
}

NS_IMETHODIMP
OpenFileRequestedOptions::SetFilePath(const nsAString & aFilePath)
{
  mFilePath = aFilePath;
  return NS_OK;
}

NS_IMETHODIMP
OpenFileRequestedOptions::GetMode(uint32_t* aMode)
{
  if (NS_WARN_IF(!aMode)) {
    return NS_ERROR_INVALID_ARG;
  }

  *aMode = static_cast<uint32_t>(mMode);
  return NS_OK;
}

NS_IMETHODIMP
OpenFileRequestedOptions::SetMode(uint32_t aMode)
{
  mMode = static_cast<OpenFileMode>(aMode);
  return NS_OK;
}

FileSystemProviderOpenFileEvent::FileSystemProviderOpenFileEvent(
  EventTarget* aOwner,
  nsIVirtualFileSystemRequestManager* aManager)
  : FileSystemProviderEvent(aOwner, aManager)
{

}

JSObject*
FileSystemProviderOpenFileEvent::WrapObjectInternal(JSContext* aCx,
                                                 JS::Handle<JSObject*> aGivenProto)
{
  return FileSystemProviderOpenFileEventBinding::Wrap(aCx, this, aGivenProto);
}

OpenFileRequestedOptions*
FileSystemProviderOpenFileEvent::Options() const
{
  MOZ_ASSERT(mOptions);

  return static_cast<OpenFileRequestedOptions*>(mOptions.get());
}

nsresult
FileSystemProviderOpenFileEvent::InitFileSystemProviderEvent(
  uint32_t aRequestId,
  nsIVirtualFileSystemRequestOption* aOption)
{
  nsCOMPtr<nsIVirtualFileSystemOpenFileRequestOption> option = do_QueryInterface(aOption);
  if (!option) {
    MOZ_ASSERT(false);
    return NS_ERROR_INVALID_ARG;
  }

  InitFileSystemProviderEventInternal(NS_LITERAL_STRING("openfilerequested"),
                                      aRequestId,
                                      option);
  return NS_OK;
}

void
FileSystemProviderOpenFileEvent::SuccessCallback()
{
  FileSystemProviderEvent::OnSuccess(nullptr, false);
}

} // namespace dom
} // namespace mozilla
