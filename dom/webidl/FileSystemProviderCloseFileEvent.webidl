/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://developer.chrome.com/apps/fileSystemProvider
 */

// Options for FileSystemProviderCloseFileEvent.
[Pref="device.storage.enabled", CheckAnyPermissions="filesystemprovider", AvailableIn="CertifiedApps"]
interface CloseFileRequestedOptions : FileSystemProviderRequestedOptions {
  // A request ID used to open the file.
  readonly attribute unsigned long openRequestId;
};

// Raised when opening a file previously opened with
// openRequestId is requested to be closed.
[Pref="device.storage.enabled", CheckAnyPermissions="filesystemprovider", AvailableIn="CertifiedApps"]
interface FileSystemProviderCloseFileEvent : FileSystemProviderEvent {
  readonly attribute CloseFileRequestedOptions options;
  void successCallback();
};
