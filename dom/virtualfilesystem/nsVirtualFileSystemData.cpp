/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/FileSystemProviderGetMetadataEventBinding.h"
#include "nsVirtualFileSystemData.h"

namespace mozilla {
namespace dom {
namespace virtualfilesystem {

NS_IMPL_ISUPPORTS(nsVirtualFileSystemUnmountOptions, nsIVirtualFileSystemUnmountOptions)

NS_IMETHODIMP
nsVirtualFileSystemUnmountOptions::GetRequestId(uint32_t* aRequestId)
{
  if (NS_WARN_IF(!aRequestId)) {
    return NS_ERROR_INVALID_ARG;
  }

  *aRequestId = mRequestId;
  return NS_OK;
}
NS_IMETHODIMP
nsVirtualFileSystemUnmountOptions::SetRequestId(uint32_t aRequestId)
{
  mRequestId = aRequestId;
  return NS_OK;
}

NS_IMETHODIMP
nsVirtualFileSystemUnmountOptions::GetFileSystemId(nsAString& aFileSystemId)
{
  aFileSystemId = mFileSystemId;
  return NS_OK;
}

NS_IMETHODIMP
nsVirtualFileSystemUnmountOptions::SetFileSystemId(const nsAString& aFileSystemId)
{
  mFileSystemId = aFileSystemId;
  return NS_OK;
}

NS_IMPL_ISUPPORTS_INHERITED(nsVirtualFileSystemMountOptions,
                            nsVirtualFileSystemUnmountOptions,
                            nsIVirtualFileSystemMountOptions)

NS_IMETHODIMP
nsVirtualFileSystemMountOptions::GetDisplayName(nsAString& aDisplayName)
{
  aDisplayName = mDisplayName;
  return NS_OK;
}

NS_IMETHODIMP
nsVirtualFileSystemMountOptions::SetDisplayName(const nsAString& aDisplayName)
{
  mDisplayName = aDisplayName;
  return NS_OK;
}

NS_IMETHODIMP
nsVirtualFileSystemMountOptions::GetWritable(bool* aWritable)
{
  if (NS_WARN_IF(!aWritable)) {
    return NS_ERROR_INVALID_ARG;
  }

  *aWritable = mWritable;
  return NS_OK;
}

NS_IMETHODIMP
nsVirtualFileSystemMountOptions::SetWritable(bool aWritable)
{
  mWritable = aWritable;
  return NS_OK;
}

NS_IMETHODIMP
nsVirtualFileSystemMountOptions::GetOpenedFilesLimit(uint32_t* aOpenedFilesLimit)
{
  if (NS_WARN_IF(!aOpenedFilesLimit)) {
    return NS_ERROR_INVALID_ARG;
  }

  *aOpenedFilesLimit = mOpenedFilesLimit;
  return NS_OK;
}
NS_IMETHODIMP
nsVirtualFileSystemMountOptions::SetOpenedFilesLimit(uint32_t aOpenedFilesLimit)
{
  mOpenedFilesLimit = aOpenedFilesLimit;
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsEntryMetadata, nsIEntryMetadata)

NS_IMETHODIMP
nsEntryMetadata::GetIsDirectory(bool* aIsDirectory)
{
  if (NS_WARN_IF(!aIsDirectory)) {
    return NS_ERROR_INVALID_ARG;
  }

  *aIsDirectory = mIsDirectory;
  return NS_OK;
}

NS_IMETHODIMP
nsEntryMetadata::SetIsDirectory(bool aIsDirectory)
{
  mIsDirectory = aIsDirectory;
  return NS_OK;
}

NS_IMETHODIMP
nsEntryMetadata::GetName(nsAString& aName)
{
  aName = mName;
  return NS_OK;
}

NS_IMETHODIMP
nsEntryMetadata::SetName(const nsAString& aName)
{
  mName = aName;
  return NS_OK;
}

NS_IMETHODIMP
nsEntryMetadata::GetSize(uint64_t* aSize)
{
  if (NS_WARN_IF(!aSize)) {
    return NS_ERROR_INVALID_ARG;
  }

  *aSize = mSize;
  return NS_OK;
}

NS_IMETHODIMP
nsEntryMetadata::SetSize(uint64_t aSize)
{
  mSize = aSize;
  return NS_OK;
}

NS_IMETHODIMP
nsEntryMetadata::GetModificationTime(DOMTimeStamp* aModificationTime)
{
  if (NS_WARN_IF(!aModificationTime)) {
    return NS_ERROR_INVALID_ARG;
  }

  *aModificationTime = mModificationTime;
  return NS_OK;
}

NS_IMETHODIMP
nsEntryMetadata::SetModificationTime(DOMTimeStamp aModificationTime)
{
  mModificationTime = aModificationTime;
  return NS_OK;
}

NS_IMETHODIMP
nsEntryMetadata::GetMimeType(nsAString& aMimeType)
{
  aMimeType = mMimeType;
  return NS_OK;
}

NS_IMETHODIMP
nsEntryMetadata::SetMimeType(const nsAString& aMimeType)
{
  mMimeType = aMimeType;
  return NS_OK;
}

/* static */ already_AddRefed<nsIEntryMetadata>
nsEntryMetadata::FromEntryMetadata(const EntryMetadata& aData)
{
  nsCOMPtr<nsIEntryMetadata> data = new nsEntryMetadata();
  data->SetIsDirectory(aData.mIsDirectory);
  data->SetName(aData.mName);
  data->SetSize(aData.mSize);
  data->SetModificationTime(aData.mModificationTime);
  if (aData.mMimeType.WasPassed() && !aData.mMimeType.Value().IsEmpty()) {
    data->SetMimeType(aData.mMimeType.Value());
  }
  return data.forget();
}

} // namespace virtualfilesystem
} // namespace dom
} // namespace mozilla
