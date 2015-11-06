/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://developer.chrome.com/apps/fileSystemProvider
 */

// Options for FileSystemProviderAbortEvent.
[Pref="device.storage.enabled", CheckAnyPermissions="filesystemprovider", AvailableIn="CertifiedApps"]
interface AbortRequestedOptions : FileSystemProviderRequestedOptions {
  // An ID of the request to be aborted.
  readonly attribute unsigned long operationRequestId;
};

// Raised when aborting an operation with operationRequestId
// is requested. The operation executed with operationRequestId
// must be immediately stopped and successCallback of this
// abort request executed. If aborting fails, then
// errorCallback must be called. Note, that callbacks of the
// aborted operation must not be called, as they will be ignored. Despite
// calling errorCallback, the request may be forcibly aborted.
[Pref="device.storage.enabled", CheckAnyPermissions="filesystemprovider", AvailableIn="CertifiedApps"]
interface FileSystemProviderAbortEvent : FileSystemProviderEvent {
  readonly attribute AbortRequestedOptions options;
  void successCallback();
};
