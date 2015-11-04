/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_virtualfilesystem_nsVirtualFileSystemRequestValue_h
#define mozilla_dom_virtualfilesystem_nsVirtualFileSystemRequestValue_h

#include "nsIVirtualFileSystemCallback.h"
#include "nsString.h"

class ArrayBuffer;
class nsIEntryMetadata;
class nsIMutableArray;

namespace mozilla {
namespace dom {
namespace virtualfilesystem {

class nsVirtualFileSystemGetMetadataRequestValue final
  : public nsIVirtualFileSystemGetMetadataRequestValue
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIVIRTUALFILESYSTEMREQUESTVALUE
  NS_DECL_NSIVIRTUALFILESYSTEMGETMETADATAREQUESTVALUE

  nsVirtualFileSystemGetMetadataRequestValue() = default;

  static already_AddRefed<nsIVirtualFileSystemGetMetadataRequestValue>
    CreateFromEntryMetadata(const EntryMetadata& aData);

private:
  ~nsVirtualFileSystemGetMetadataRequestValue() = default;

  nsCOMPtr<nsIEntryMetadata> mMetadata;
};

class nsVirtualFileSystemReadDirectoryRequestValue final
  : public nsIVirtualFileSystemReadDirectoryRequestValue
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIVIRTUALFILESYSTEMREQUESTVALUE
  NS_DECL_NSIVIRTUALFILESYSTEMREADDIRECTORYREQUESTVALUE

  nsVirtualFileSystemReadDirectoryRequestValue() = default;

  static already_AddRefed<nsIVirtualFileSystemReadDirectoryRequestValue>
    CreateFromEntryMetadataArray(const nsTArray<nsCOMPtr<nsIEntryMetadata>>& aArray);

private:
  ~nsVirtualFileSystemReadDirectoryRequestValue() = default;
  void AppendElementsInArray(nsIMutableArray* aMergedArray, nsIArray* aToBeMergedArray);

  nsCOMPtr<nsIArray> mEntries;
};

class nsVirtualFileSystemReadFileRequestValue final
  : public nsIVirtualFileSystemReadFileRequestValue
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIVIRTUALFILESYSTEMREQUESTVALUE
  NS_DECL_NSIVIRTUALFILESYSTEMREADFILEREQUESTVALUE

  nsVirtualFileSystemReadFileRequestValue() = default;

  static already_AddRefed<nsIVirtualFileSystemReadFileRequestValue>
    CreateFromArrayBuffer(const ArrayBuffer& aBuffer);

private:
  ~nsVirtualFileSystemReadFileRequestValue() = default;

  nsCString mData;
};

} // end of namespace virtualfilesystem
} // end of namespace dom
} // end of namespace mozilla
#endif
