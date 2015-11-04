"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;
const fileSystemId = "testFileSystemId";
const requestId = 1;
const path = "/dummy/path/";

function run_test() {
  run_next_test();
}

add_test(function test_virtualfilesystem_abort_option() {
  var option = Cc["@mozilla.org/virtualfilesystem/virtualfilesystem-abort-request-option;1"].
               createInstance(Ci.nsIVirtualFileSystemAbortRequestOption);
  option.fileSystemId = fileSystemId;
  option.operationRequestId = requestId;

  equal(option.fileSystemId, fileSystemId);
  equal(option.operationRequestId, requestId);

  run_next_test();
});

add_test(function test_virtualfilesystem_getMetadata_option() {
  var option = Cc["@mozilla.org/virtualfilesystem/virtualfilesystem-getmetadata-request-option;1"].
               createInstance(Ci.nsIVirtualFileSystemGetMetadataRequestOption);
  option.fileSystemId = fileSystemId;
  option.entryPath = path;

  equal(option.fileSystemId, fileSystemId);
  equal(option.entryPath, path);

  run_next_test();
});

add_test(function test_virtualfilesystem_closeFile_option() {
  var option = Cc["@mozilla.org/virtualfilesystem/virtualfilesystem-closefile-request-option;1"].
               createInstance(Ci.nsIVirtualFileSystemCloseFileRequestOption);
  option.fileSystemId = fileSystemId;
  option.openRequestId = requestId;

  equal(option.fileSystemId, fileSystemId);
  equal(option.openRequestId, requestId);

  run_next_test();
});

add_test(function test_virtualfilesystem_openFile_option() {
  var mode = Ci.nsIVirtualFileSystemOpenFileRequestOption.OPEN_MODE_READ;
  var option = Cc["@mozilla.org/virtualfilesystem/virtualfilesystem-openfile-request-option;1"].
               createInstance(Ci.nsIVirtualFileSystemOpenFileRequestOption);
  option.fileSystemId = fileSystemId;
  option.filePath = path;
  option.mode = mode;

  equal(option.fileSystemId, fileSystemId);
  equal(option.filePath, path);
  equal(option.mode, mode);

  run_next_test();
});

add_test(function test_virtualfilesystem_readDirectory_option() {
  var option = Cc["@mozilla.org/virtualfilesystem/virtualfilesystem-readdirectory-request-option;1"].
               createInstance(Ci.nsIVirtualFileSystemReadDirectoryRequestOption);
  option.fileSystemId = fileSystemId;
  option.dirPath = path;

  equal(option.fileSystemId, fileSystemId);
  equal(option.dirPath, path);

  run_next_test();
});

add_test(function test_virtualfilesystem_readFile_option() {
  var offset = 0;
  var length = 100;
  var option = Cc["@mozilla.org/virtualfilesystem/virtualfilesystem-readfile-request-option;1"].
               createInstance(Ci.nsIVirtualFileSystemReadFileRequestOption);
  option.fileSystemId = fileSystemId;
  option.openRequestId = requestId;
  option.offset = offset;
  option.length = length;

  equal(option.fileSystemId, fileSystemId);
  equal(option.openRequestId, requestId);
  equal(option.offset, offset);
  equal(option.length, length);

  run_next_test();
});

add_test(function test_virtualfilesystem_unmount_option() {
  var option = Cc["@mozilla.org/virtualfilesystem/virtualfilesystem-unmount-request-option;1"].
               createInstance(Ci.nsIVirtualFileSystemUnmountRequestOption);
  option.fileSystemId = fileSystemId;

  equal(option.fileSystemId, fileSystemId);

  run_next_test();
});
