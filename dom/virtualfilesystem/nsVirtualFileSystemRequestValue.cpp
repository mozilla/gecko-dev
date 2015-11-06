/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/FileSystemProviderGetMetadataEventBinding.h"
#include "mozilla/dom/TypedArray.h"
#include "nsArrayUtils.h"
#include "nsVirtualFileSystemData.h"
#include "nsVirtualFileSystemRequestValue.h"
#include "nsComponentManagerUtils.h"
#include "nsIMutableArray.h"
#include "nsTArray.h"

namespace mozilla {
namespace dom {
namespace virtualfilesystem {

NS_IMPL_ISUPPORTS(nsVirtualFileSystemGetMetadataRequestValue,
                  nsIVirtualFileSystemGetMetadataRequestValue)

/* static */ already_AddRefed<nsIVirtualFileSystemGetMetadataRequestValue>
nsVirtualFileSystemGetMetadataRequestValue::CreateFromEntryMetadata(
  const EntryMetadata& aData)
{
  nsCOMPtr<nsIVirtualFileSystemGetMetadataRequestValue> requestValue =
    new nsVirtualFileSystemGetMetadataRequestValue();
  nsCOMPtr<nsIEntryMetadata> metadata =
    nsEntryMetadata::FromEntryMetadata(aData);

  requestValue->SetMetadata(metadata);
  return requestValue.forget();
}

NS_IMETHODIMP
nsVirtualFileSystemGetMetadataRequestValue::SetMetadata(nsIEntryMetadata *aMetadata)
{
  mMetadata = aMetadata;
  return NS_OK;
}

NS_IMETHODIMP
nsVirtualFileSystemGetMetadataRequestValue::GetMetadata(nsIEntryMetadata** aMetadata)
{
  if (NS_WARN_IF(!aMetadata)) {
    return NS_ERROR_INVALID_POINTER;
  }

  nsCOMPtr<nsIEntryMetadata> data = mMetadata;
  data.forget(aMetadata);
  return NS_OK;
}

NS_IMETHODIMP
nsVirtualFileSystemGetMetadataRequestValue::Concat(nsIVirtualFileSystemRequestValue* aValue)
{
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsVirtualFileSystemReadDirectoryRequestValue,
                  nsIVirtualFileSystemReadDirectoryRequestValue)

/* static */ already_AddRefed<nsIVirtualFileSystemReadDirectoryRequestValue>
nsVirtualFileSystemReadDirectoryRequestValue::CreateFromEntryMetadataArray(
  const nsTArray<nsCOMPtr<nsIEntryMetadata>>& aArray)
{
  nsCOMPtr<nsIVirtualFileSystemReadDirectoryRequestValue> requestValue =
    new nsVirtualFileSystemReadDirectoryRequestValue();

  nsresult rv;
  nsCOMPtr<nsIMutableArray> entries = do_CreateInstance(NS_ARRAY_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }

  for (uint32_t i = 0; i < aArray.Length(); ++i) {
    rv = entries->AppendElement(aArray[i], false);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return nullptr;
    }
  }

  requestValue->SetEntries(entries);
  return requestValue.forget();
}

NS_IMETHODIMP
nsVirtualFileSystemReadDirectoryRequestValue::SetEntries(nsIArray* aEntries)
{
  mEntries = aEntries;
  return NS_OK;
}

NS_IMETHODIMP
nsVirtualFileSystemReadDirectoryRequestValue::GetEntries(nsIArray** aEntries)
{
  if (NS_WARN_IF(!aEntries)) {
    return NS_ERROR_INVALID_POINTER;
  }

  nsresult rv;
  nsCOMPtr<nsIMutableArray> entries = do_CreateInstance(NS_ARRAY_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  uint32_t length = 0;
  mEntries->GetLength(&length);
  for (uint32_t i = 0; i < length; i++) {
    nsCOMPtr<nsIEntryMetadata> metadata = do_QueryElementAt(mEntries, i);
    if (metadata) {
      entries->AppendElement(metadata, false);
    }
  }

  entries.forget(aEntries);
  return NS_OK;
}

NS_IMETHODIMP
nsVirtualFileSystemReadDirectoryRequestValue::Concat(nsIVirtualFileSystemRequestValue* aValue)
{
  nsCOMPtr<nsIVirtualFileSystemReadDirectoryRequestValue> value = do_QueryInterface(aValue);
  if (NS_WARN_IF(!value)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsresult rv;
  nsCOMPtr<nsIArray> orgEntries;
  rv = GetEntries(getter_AddRefs(orgEntries));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIArray> entries;
  rv = value->GetEntries(getter_AddRefs(entries));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIMutableArray> mergedEntries = do_CreateInstance(NS_ARRAY_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  AppendElementsInArray(mergedEntries, orgEntries);
  AppendElementsInArray(mergedEntries, entries);

  SetEntries(mergedEntries);
  return NS_OK;
}

void
nsVirtualFileSystemReadDirectoryRequestValue::AppendElementsInArray(
  nsIMutableArray* aMergedArray, nsIArray* aToBeMergedArray)
{
  uint32_t length = 0;
  aToBeMergedArray->GetLength(&length);
  for (uint32_t i = 0; i < length; i++) {
    nsCOMPtr<nsIEntryMetadata> metadata = do_QueryElementAt(aToBeMergedArray, i);
    if (metadata) {
      aMergedArray->AppendElement(metadata, false);
    }
  }
}

NS_IMPL_ISUPPORTS(nsVirtualFileSystemReadFileRequestValue,
                  nsIVirtualFileSystemReadFileRequestValue)

/* static */ already_AddRefed<nsIVirtualFileSystemReadFileRequestValue>
nsVirtualFileSystemReadFileRequestValue::CreateFromArrayBuffer(
  const ArrayBuffer& aBuffer)
{
  nsCOMPtr<nsIVirtualFileSystemReadFileRequestValue> requestValue =
    new nsVirtualFileSystemReadFileRequestValue();
  nsCString data = nsCString(reinterpret_cast<const char*>(aBuffer.Data()),
                             aBuffer.Length());

  requestValue->SetData(data);
  return requestValue.forget();
}

NS_IMETHODIMP
nsVirtualFileSystemReadFileRequestValue::SetData(const nsACString& aData)
{
  mData.Assign(aData);
  return NS_OK;
}

NS_IMETHODIMP
nsVirtualFileSystemReadFileRequestValue::GetData(nsACString & aData)
{
  aData.Assign(mData);
  return NS_OK;
}

NS_IMETHODIMP
nsVirtualFileSystemReadFileRequestValue::Concat(nsIVirtualFileSystemRequestValue* aValue)
{
  nsCOMPtr<nsIVirtualFileSystemReadFileRequestValue> value = do_QueryInterface(aValue);
  if (NS_WARN_IF(!value)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsAutoCString data;
  value->GetData(data);

  mData.Append(data.get(), data.Length());
  return NS_OK;
}

} // end of namespace mozilla
} // end of namespace dom
} // end of namespace virtualfilesystem
