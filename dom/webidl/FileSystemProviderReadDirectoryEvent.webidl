/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://developer.chrome.com/apps/fileSystemProvider
 */

// Options for the FileSystemProviderReadDirectoryEvent.
[Pref="device.storage.enabled", CheckAnyPermissions="filesystemprovider", AvailableIn="CertifiedApps"]
interface ReadDirectoryRequestedOptions : FileSystemProviderRequestedOptions {
  // The path of the directory which contents are requested.
  readonly attribute DOMString directoryPath;
};

// Raised when contents of a directory at directoryPath are
// requested. The results must be returned in chunks by calling the
// successCallback several times. In case of an error,
// errorCallback must be called.
[Pref="device.storage.enabled", CheckAnyPermissions="filesystemprovider", AvailableIn="CertifiedApps"]
interface FileSystemProviderReadDirectoryEvent : FileSystemProviderEvent {
  readonly attribute ReadDirectoryRequestedOptions options;

  // Success callback for the FileSystemProviderReadDirectoryEvent. If more
  // entries will be returned, then hasMore must be true, and it
  // has to be called again with additional entries. If no more entries are
  // available, then hasMore must be set to false.
  void successCallback(sequence<EntryMetadata> entries, boolean hasMore);
};
