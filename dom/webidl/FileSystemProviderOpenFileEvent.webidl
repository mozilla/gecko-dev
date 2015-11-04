/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://developer.chrome.com/apps/fileSystemProvider
 */

// Options for FileSystemProviderOpenFileEvent.
[Pref="device.storage.enabled", CheckAnyPermissions="filesystemprovider", AvailableIn="CertifiedApps"]
interface OpenFileRequestedOptions : FileSystemProviderRequestedOptions {
  // The path of the file to be opened.
  readonly attribute DOMString filePath;

  // Whether the file will be used for reading or writing.
  readonly attribute OpenFileMode mode;
};

// Raised when opening a file at filePath is requested. If the
// file does not exist, then the operation must fail. Maximum number of
// files opened at once can be specified with MountOptions.
[Pref="device.storage.enabled", CheckAnyPermissions="filesystemprovider", AvailableIn="CertifiedApps"]
interface FileSystemProviderOpenFileEvent : FileSystemProviderEvent {
  readonly attribute OpenFileRequestedOptions options;
  void successCallback();
};
