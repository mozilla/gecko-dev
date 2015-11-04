/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/FileSystemProviderReadFileEvent.h"
#include "nsVirtualFileSystemRequestValue.h"
#include "nsIVirtualFileSystemRequestOption.h"
#include "nsIVirtualFileSystemRequestManager.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(ReadFileRequestedOptions)

NS_IMPL_ADDREF_INHERITED(ReadFileRequestedOptions, FileSystemProviderRequestedOptions)
NS_IMPL_RELEASE_INHERITED(ReadFileRequestedOptions, FileSystemProviderRequestedOptions)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(ReadFileRequestedOptions,
                                                  FileSystemProviderRequestedOptions)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(ReadFileRequestedOptions,
                                                FileSystemProviderRequestedOptions)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(ReadFileRequestedOptions)
NS_INTERFACE_MAP_ENTRY(nsIVirtualFileSystemReadFileRequestOption)
NS_INTERFACE_MAP_END_INHERITING(FileSystemProviderRequestedOptions)

JSObject*
ReadFileRequestedOptions::WrapObject(JSContext* aCx,
                                     JS::Handle<JSObject*> aGivenProto)
{
  return ReadFileRequestedOptionsBinding::Wrap(aCx, this, aGivenProto);
}

NS_IMETHODIMP
ReadFileRequestedOptions::GetOpenRequestId(uint32_t* aOpenRequestId)
{
  if (NS_WARN_IF(!aOpenRequestId)) {
    return NS_ERROR_INVALID_ARG;
  }

  *aOpenRequestId = mOpenRequestId;
  return NS_OK;
}

NS_IMETHODIMP
ReadFileRequestedOptions::SetOpenRequestId(uint32_t aOpenRequestId)
{
  mOpenRequestId = aOpenRequestId;
  return NS_OK;
}

NS_IMETHODIMP
ReadFileRequestedOptions::GetOffset(uint64_t* aOffset)
{
  if (NS_WARN_IF(!aOffset)) {
    return NS_ERROR_INVALID_ARG;
  }

  *aOffset = mOffset;
  return NS_OK;
}

NS_IMETHODIMP
ReadFileRequestedOptions::SetOffset(uint64_t aOffset)
{
   mOffset = aOffset;
  return NS_OK;
}

NS_IMETHODIMP
ReadFileRequestedOptions::GetLength(uint64_t* aLength)
{
  if (NS_WARN_IF(!aLength)) {
    return NS_ERROR_INVALID_ARG;
  }

  *aLength = mLength;
  return NS_OK;
}

NS_IMETHODIMP
ReadFileRequestedOptions::SetLength(uint64_t aLength)
{
  mLength = aLength;
  return NS_OK;
}

FileSystemProviderReadFileEvent::FileSystemProviderReadFileEvent(
  EventTarget* aOwner,
  nsIVirtualFileSystemRequestManager* aManager)
  : FileSystemProviderEvent(aOwner, aManager)
{

}

JSObject*
FileSystemProviderReadFileEvent::WrapObjectInternal(JSContext* aCx,
                                                    JS::Handle<JSObject*> aGivenProto)
{
  return FileSystemProviderReadFileEventBinding::Wrap(aCx, this, aGivenProto);
}

ReadFileRequestedOptions*
FileSystemProviderReadFileEvent::Options() const
{
  MOZ_ASSERT(mOptions);

  return static_cast<ReadFileRequestedOptions*>(mOptions.get());
}

nsresult
FileSystemProviderReadFileEvent::InitFileSystemProviderEvent(
  uint32_t aRequestId,
  nsIVirtualFileSystemRequestOption* aOption)
{
  nsCOMPtr<nsIVirtualFileSystemReadFileRequestOption> option = do_QueryInterface(aOption);
  if (!option) {
    MOZ_ASSERT(false);
    return NS_ERROR_INVALID_ARG;
  }

  InitFileSystemProviderEventInternal(NS_LITERAL_STRING("readfilerequested"),
                                      aRequestId,
                                      option);
  return NS_OK;
}

void
FileSystemProviderReadFileEvent::SuccessCallback(const ArrayBuffer& aData, bool aHasMore)
{
  nsCOMPtr<nsIVirtualFileSystemReadFileRequestValue> value =
    virtualfilesystem::nsVirtualFileSystemReadFileRequestValue::CreateFromArrayBuffer(aData);

  FileSystemProviderEvent::OnSuccess(value, aHasMore);
}

} // namespace dom
} // namespace mozilla
