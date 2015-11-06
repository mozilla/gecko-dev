"use strict";

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;
const dummyFileSystemId = "testFileSystemId";
const dummyPath = "/dummy/path/";

var emptyCallback = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIVirtualFileSystemCallback]),

  onSuccess: function(requestId, value, hasMore) {},

  onError: function(requestId, error) {},
};

var option = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIVirtualFileSystemGetMetadataRequestOption]),

  fileSystemId: dummyFileSystemId,

  entryPath: dummyPath,
};

var emptyDispatcher = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIFileSystemProviderEventDispatcher]),

  dispatchFileSystemProviderEvent: function(requestId, type, option) {},
};

function do_check_throws(f, result, stack)
{
  if (!stack) {
    try {
      // We might not have a 'Components' object.
      stack = Components.stack.caller;
    } catch (e) {
    }
  }

  try {
    f();
  } catch (exc) {
    do_check_eq(exc.result, result);
    return;
  }
  do_throw("expected " + result + " exception, none thrown", stack);
}

function run_test() {
  run_next_test();
}

add_test(function test_virtualfilesystem_invalid_request_type() {
  var manager = Cc["@mozilla.org/virtualfilesystem/virtualfilesystem-request-manager;1"].
                createInstance(Ci.nsIVirtualFileSystemRequestManager);
  do_check_throws(function() {
    manager.createRequest(0xFFFFFFFF, null, null);
  }, Cr.NS_ERROR_INVALID_ARG);

  run_next_test();
});

add_test(function test_virtualfilesystem__setRequestDispatcher() {
  var manager = Cc["@mozilla.org/virtualfilesystem/virtualfilesystem-request-manager;1"].
                createInstance(Ci.nsIVirtualFileSystemRequestManager);

  do_check_throws(function() {
    manager.createRequest(0, option, emptyCallback);
  }, Cr.NS_ERROR_NOT_INITIALIZED);

  manager.setRequestDispatcher(emptyDispatcher);
  do_check_true(manager.createRequest(0, option, emptyCallback) > 0);

  run_next_test();
});

add_test(function test_virtualfilesystem__dispatcher() {
  var manager = Cc["@mozilla.org/virtualfilesystem/virtualfilesystem-request-manager;1"].
                createInstance(Ci.nsIVirtualFileSystemRequestManager);

  var dispatcher = {
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIFileSystemProviderEventDispatcher]),

    dispatchFileSystemProviderEvent: function(aRequestId, aType, aOption) {
      var getMetadataOption = aOption.QueryInterface(Ci.nsIVirtualFileSystemGetMetadataRequestOption);
      do_check_true(aRequestId > 0);
      equal(aType, Ci.nsIVirtualFileSystemRequestManager.REQUEST_GETMETADATA);
      equal(getMetadataOption.fileSystemId, dummyFileSystemId);
      equal(getMetadataOption.entryPath, dummyPath);
    },
  };

  manager.setRequestDispatcher(dispatcher);
  manager.createRequest(Ci.nsIVirtualFileSystemRequestManager.REQUEST_GETMETADATA, option, emptyCallback);

  run_next_test();
});

add_test(function test_virtualfilesystem_callback_onSuccess() {
  var manager = Cc["@mozilla.org/virtualfilesystem/virtualfilesystem-request-manager;1"].
                createInstance(Ci.nsIVirtualFileSystemRequestManager);
  var requestId = 0;

  var dispatcher = {
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIFileSystemProviderEventDispatcher]),

    dispatchFileSystemProviderEvent: function(aRequestId, aType, aOption) {
      equal(aRequestId, requestId);
      manager.fufillRequest(aRequestId, null, false);
    },
  };
  var successCallback = {
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIVirtualFileSystemCallback]),

    onSuccess: function(aRequestId, aValue, aHasMore) {
      equal(aRequestId, requestId);
    },

    onError: function(aRequestId, aErrorCode) {},
  };
  manager.setRequestDispatcher(dispatcher);
  requestId = manager.createRequest(Ci.nsIVirtualFileSystemRequestManager.REQUEST_GETMETADATA, option, successCallback);
  do_check_true(requestId > 0);

  run_next_test();
});

add_test(function test_virtualfilesystem_callback_onError() {
  var manager = Cc["@mozilla.org/virtualfilesystem/virtualfilesystem-request-manager;1"].
                createInstance(Ci.nsIVirtualFileSystemRequestManager);
  var requestId = 0;
  var dispatcher = {
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIFileSystemProviderEventDispatcher]),

    dispatchFileSystemProviderEvent: function(aRequestId, aType, aOption) {
      equal(aRequestId, requestId);
      manager.rejectRequest(aRequestId, 0);
    },
  };
  var errorCallback = {
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIVirtualFileSystemCallback]),

    onSuccess: function(aRequestId, aValue, aHasMore) {},

    onError: function(aRequestId, aErrorCode) {
      equal(aRequestId, requestId);
    },
  };
  manager.setRequestDispatcher(dispatcher);
  requestId = manager.createRequest(Ci.nsIVirtualFileSystemRequestManager.REQUEST_GETMETADATA, option, errorCallback);
  do_check_true(requestId > 0);

  run_next_test();
});

add_test(function test_virtualfilesystem_callback_in_order() {
  var manager = Cc["@mozilla.org/virtualfilesystem/virtualfilesystem-request-manager;1"].
                createInstance(Ci.nsIVirtualFileSystemRequestManager);
  var baseRequestId = 0;
  var currentRequestId = 0;
  var requestCount = 5;

  var callback = {
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIVirtualFileSystemCallback]),

    onSuccess: function(aRequestId, aValue, aHasMore) {
      equal(aRequestId, currentRequestId);
      currentRequestId++;
    },

    onError: function(aRequestId, aErrorCode) {},
  };

  manager.setRequestDispatcher(emptyDispatcher);
  for (var i = 0; i < requestCount; i++) {
    var id = manager.createRequest(Ci.nsIVirtualFileSystemRequestManager.REQUEST_GETMETADATA, option, callback);
    if (i ==0) {
      currentRequestId = id;
    }
  }

  for (var i = 0; i < requestCount; i++) {
    manager.fufillRequest(requestCount + currentRequestId - i - 1, null, false);
  }

  run_next_test();
});
