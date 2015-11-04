/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_virtualfilesystem_nsVirtualFileSystemData_h
#define mozilla_dom_virtualfilesystem_nsVirtualFileSystemData_h

#include "nsIVirtualFileSystemCallback.h"
#include "nsIVirtualFileSystemService.h"
#include "nsString.h"

namespace mozilla {
namespace dom {

struct EntryMetadata;

namespace virtualfilesystem {

class nsVirtualFileSystemUnmountOptions : public nsIVirtualFileSystemUnmountOptions
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIVIRTUALFILESYSTEMUNMOUNTOPTIONS

  nsVirtualFileSystemUnmountOptions()
    : mRequestId(0) {}

protected:
  virtual ~nsVirtualFileSystemUnmountOptions() = default;

  uint32_t mRequestId;
  nsString mFileSystemId;
};

class nsVirtualFileSystemMountOptions final : public nsVirtualFileSystemUnmountOptions
                                       , public nsIVirtualFileSystemMountOptions
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIVIRTUALFILESYSTEMMOUNTOPTIONS
  NS_FORWARD_NSIVIRTUALFILESYSTEMUNMOUNTOPTIONS(nsVirtualFileSystemUnmountOptions::)

  nsVirtualFileSystemMountOptions()
    : mWritable(false)
    , mOpenedFilesLimit(0) {}

private:
  virtual ~nsVirtualFileSystemMountOptions() = default;

  nsString mDisplayName;
  bool mWritable;
  uint32_t mOpenedFilesLimit;
};

class nsEntryMetadata final : public nsIEntryMetadata
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIENTRYMETADATA

  explicit nsEntryMetadata() = default;
  static already_AddRefed<nsIEntryMetadata> FromEntryMetadata(const EntryMetadata& data);

private:
  virtual ~nsEntryMetadata() = default;

  bool mIsDirectory;
  nsString mName;
  uint64_t mSize;
  DOMTimeStamp mModificationTime;
  nsString mMimeType;
};

} // namespace virtualfilesystem
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_virtualfilesystem_nsVirtualFileSystemData_h
