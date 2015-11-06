/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://developer.chrome.com/apps/fileSystemProvider
 */

// Options for the FileSystemProviderGetMetadataEvent.
[Pref="device.storage.enabled", CheckAnyPermissions="filesystemprovider", AvailableIn="CertifiedApps"]
interface GetMetadataRequestedOptions : FileSystemProviderRequestedOptions {
  // The path of the entry to fetch metadata about.
  readonly attribute DOMString entryPath;
};

// Represents metadata of a file or a directory.
dictionary EntryMetadata {
  // True if it is a directory.
  required boolean isDirectory;

  // Name of this entry (not full path name). Must not contain '/'. For root
  // it must be empty.
  required DOMString name;

  // File size in bytes.
  required unsigned long long size;

  // The last modified time of this entry.
  required DOMTimeStamp modificationTime;

  // Mime type for the entry.
  DOMString? mimeType;
};

// Raised when metadata of a file or a directory at entryPath
// is requested. The metadata must be returned with the
// successCallback call. In case of an error,
// errorCallback must be called.
[Pref="device.storage.enabled", CheckAnyPermissions="filesystemprovider", AvailableIn="CertifiedApps"]
interface FileSystemProviderGetMetadataEvent : FileSystemProviderEvent {
  readonly attribute GetMetadataRequestedOptions options;
  void successCallback(EntryMetadata metadata);
};
