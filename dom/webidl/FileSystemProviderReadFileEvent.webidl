/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://developer.chrome.com/apps/fileSystemProvider
 */

// Options for FileSystemProviderReadFileEvent.
[Pref="device.storage.enabled", CheckAnyPermissions="filesystemprovider", AvailableIn="CertifiedApps"]
interface ReadFileRequestedOptions : FileSystemProviderRequestedOptions {
  // A request ID used to open the file.
  readonly attribute unsigned long openRequestId;

  // Position in the file (in bytes) to start reading from.
  readonly attribute unsigned long long offset;

  // Number of bytes to be returned.
  readonly attribute unsigned long long length;
};

// Raised when reading contents of a file opened previously with
// openRequestId is requested. The results must be returned in
// chunks by calling successCallback several times. In case of
// an error, errorCallback must be called.
[Pref="device.storage.enabled", CheckAnyPermissions="filesystemprovider", AvailableIn="CertifiedApps"]
interface FileSystemProviderReadFileEvent : FileSystemProviderEvent {
  readonly attribute ReadFileRequestedOptions options;

  // Success callback for FileSystemProviderReadFileEvent. If more
  // data will be returned, then hasMore must be true, and it
  // has to be called again with additional entries. If no more data is
  // available, then hasMore must be set to false.
  void successCallback(ArrayBuffer data, boolean hasMore);
};
