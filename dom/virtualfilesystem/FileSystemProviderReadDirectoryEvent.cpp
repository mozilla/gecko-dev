/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/FileSystemProviderReadDirectoryEvent.h"
#include "nsVirtualFileSystemData.h"
#include "nsVirtualFileSystemRequestValue.h"
#include "nsCOMArray.h"
#include "nsIVirtualFileSystemRequestOption.h"
#include "nsIVirtualFileSystemRequestManager.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(ReadDirectoryRequestedOptions)

NS_IMPL_ADDREF_INHERITED(ReadDirectoryRequestedOptions, FileSystemProviderRequestedOptions)
NS_IMPL_RELEASE_INHERITED(ReadDirectoryRequestedOptions, FileSystemProviderRequestedOptions)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(ReadDirectoryRequestedOptions,
                                                  FileSystemProviderRequestedOptions)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(ReadDirectoryRequestedOptions,
                                                FileSystemProviderRequestedOptions)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(ReadDirectoryRequestedOptions)
NS_INTERFACE_MAP_ENTRY(nsIVirtualFileSystemReadDirectoryRequestOption)
NS_INTERFACE_MAP_END_INHERITING(FileSystemProviderRequestedOptions)

JSObject*
ReadDirectoryRequestedOptions::WrapObject(JSContext* aCx,
                                          JS::Handle<JSObject*> aGivenProto)
{
  return ReadDirectoryRequestedOptionsBinding::Wrap(aCx, this, aGivenProto);
}

NS_IMETHODIMP
ReadDirectoryRequestedOptions::GetDirPath(nsAString& aDirPath)
{
  aDirPath = mDirectoryPath;
  return NS_OK;
}

NS_IMETHODIMP
ReadDirectoryRequestedOptions::SetDirPath(const nsAString& aDirPath)
{
  mDirectoryPath = aDirPath;
  return NS_OK;
}

FileSystemProviderReadDirectoryEvent::FileSystemProviderReadDirectoryEvent(
  EventTarget* aOwner,
  nsIVirtualFileSystemRequestManager* aManager)
  : FileSystemProviderEvent(aOwner, aManager)
{

}

JSObject*
FileSystemProviderReadDirectoryEvent::WrapObjectInternal(JSContext* aCx,
                                                         JS::Handle<JSObject*> aGivenProto)
{
  return FileSystemProviderReadDirectoryEventBinding::Wrap(aCx, this, aGivenProto);
}

ReadDirectoryRequestedOptions*
FileSystemProviderReadDirectoryEvent::Options() const
{
  MOZ_ASSERT(mOptions);

  return static_cast<ReadDirectoryRequestedOptions*>(mOptions.get());
}

nsresult
FileSystemProviderReadDirectoryEvent::InitFileSystemProviderEvent(
  uint32_t aRequestId,
  nsIVirtualFileSystemRequestOption* aOption)
{
  nsCOMPtr<nsIVirtualFileSystemReadDirectoryRequestOption> option = do_QueryInterface(aOption);
  if (!option) {
    MOZ_ASSERT(false);
    return NS_ERROR_INVALID_ARG;
  }

  InitFileSystemProviderEventInternal(NS_LITERAL_STRING("readdirectoryrequested"),
                                      aRequestId,
                                      option);
  return NS_OK;
}

void
FileSystemProviderReadDirectoryEvent::SuccessCallback(const Sequence<EntryMetadata>& aEntries,
                                                      bool aHasMore)
{
  nsTArray<nsCOMPtr<nsIEntryMetadata>> entries;
  for (uint32_t i = 0; i < aEntries.Length(); i++) {
    entries.AppendElement(virtualfilesystem::nsEntryMetadata::FromEntryMetadata(aEntries[i]));
  }

  nsCOMPtr<nsIVirtualFileSystemReadDirectoryRequestValue> value =
    virtualfilesystem::nsVirtualFileSystemReadDirectoryRequestValue::CreateFromEntryMetadataArray(
      entries);

  FileSystemProviderEvent::OnSuccess(value, aHasMore);
}

} // namespace dom
} // namespace mozilla
