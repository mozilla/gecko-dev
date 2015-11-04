/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://developer.chrome.com/apps/fileSystemProvider
 */

// Mode of opening a file.
enum OpenFileMode {
  "Read",
  "Write"
};

// Options for the mount method.
dictionary MountOptions {
  // The string indentifier of the file system. Must be unique per each
  // extension.
  required DOMString fileSystemId;

  // A human-readable name for the file system.
  required DOMString displayName;

  // Whether the file system supports operations which may change contents
  // of the file system (such as creating, deleting or writing to files).
  boolean? writable;

  // The maximum number of files that can be opened at once. If not specified,
  // or 0, then not limited.
  unsigned long? openedFilesLimit;
};

// Options for the unmount method.
dictionary UnmountOptions {
  // The identifier of the file system to be unmounted.
  required DOMString fileSystemId;
};

// Represents an opened file.
dictionary OpenedFile {
  // A request ID to be be used by consecutive read/write and close requests.
  unsigned long openRequestId;

  // The path of the opened file.
  DOMString filePath;

  // Whether the file was opened for reading or writing.
  OpenFileMode mode;
};

// Represents a mounted file system.
dictionary FileSystemInfo {
  // The identifier of the file system.
  DOMString fileSystemId;

  // A human-readable name for the file system.
  DOMString displayName;

  // Whether the file system supports operations which may change contents
  // of the file system (such as creating, deleting or writing to files).
  boolean writable;

  // The maximum number of files that can be opened at once. If 0, then not
  // limited.
  unsigned long openedFilesLimit;

  // List of currently opened files.
  sequence<OpenedFile> openedFiles;
};

[Pref="device.storage.enabled", CheckAnyPermissions="filesystemprovider", AvailableIn="CertifiedApps"]
interface FileSystemProvider : EventTarget {
  // Mounts a file system with the given fileSystemId and displayName.
  [Throws]
  Promise<void> mount(MountOptions options);

  // Unmounts a file system with the given fileSystemId.
  [Throws]
  Promise<void> unmount(UnmountOptions options);

  // Returns information about a file system with the passed fileSystemId.
  [Throws]
  Promise<FileSystemInfo> get(DOMString fileSystemId);

  attribute EventHandler onunmountrequested;
  attribute EventHandler ongetmetadatarequested;
  attribute EventHandler onreaddirectoryrequested;
  attribute EventHandler onopenfilerequested;
  attribute EventHandler onclosefilerequested;
  attribute EventHandler onreadfilerequested;
  attribute EventHandler onabortrequested;
};
