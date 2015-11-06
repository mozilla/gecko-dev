/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://developer.chrome.com/apps/fileSystemProvider
 */

enum FileSystemProviderError {
    "Failed",
    "InUse",
    "Exists",
    "NotFound",
    "AccessDenied",
    "TooManyOpened",
    "NoMemory",
    "NoSpace",
    "NotADirectory",
    "InvalidOperation",
    "Security",
    "Abort",
    "NotAFile",
    "NotEmpty",
    "InvalidURL",
  };

[Pref="device.storage.enabled", CheckAnyPermissions="filesystemprovider", AvailableIn="CertifiedApps"]
interface FileSystemProviderRequestedOptions {
  // The identifier of the file system related to this operation.
  readonly attribute DOMString fileSystemId;

  // The unique identifier of this request.
  readonly attribute unsigned long requestId;
};

[Pref="device.storage.enabled", CheckAnyPermissions="filesystemprovider", AvailableIn="CertifiedApps"]
interface FileSystemProviderEvent : Event {
  void errorCallback(FileSystemProviderError error);
};
