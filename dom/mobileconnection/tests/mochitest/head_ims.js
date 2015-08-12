/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var mockImsRegServiceHelper = SpecialPowers.loadChromeScript(
  SimpleTest.getTestFileURL("mock_ims_reg_service.js")
);
function ensureMockImsRegService() {
  return new Promise(function(resolve, reject) {
    mockImsRegServiceHelper.addMessageListener('setup-complete',
                                               function onevent() {
      mockImsRegServiceHelper
        .removeMessageListener('setup-complete', onevent);

      ok(true, "MockImsRegService has been setup.");
      resolve();
    });

    mockImsRegServiceHelper.sendAsyncMessage('setup');
  });
}

var imsRegHandler;
function ensureImsRegHandler() {
  return new Promise(function(resolve, reject) {
    SpecialPowers.pushPermissions([{ "type": "mobileconnection",
                                     "allow": 1, "context": document }],
                                  function() {
      info("permission of mobileconnection is pushed");
      imsRegHandler = navigator.mozMobileConnections[0].imsHandler;
      ok(imsRegHandler, "imsRegHandler is granted.");
      if (imsRegHandler) {
        resolve();
      } else {
        reject("imsRegHandler is not granted.");
      }
    });
  });
}

function getImsEnabled() {
  return imsRegHandler.enabled;
}

function setImsEnabled(aEnabled) {
  return imsRegHandler.setEnabled(aEnabled);
}

function getPreferredProfile() {
  return imsRegHandler.preferredProfile;
}

function setPreferredProfile(aProfile) {
  return imsRegHandler.setPreferredProfile(aProfile);
}

function updateImsCapability(aCapability, aUnregisteredReason) {
  return new Promise(function(resolve, reject) {
    imsRegHandler.addEventListener("capabilitychange", function onevent() {
      imsRegHandler.removeEventListener("capabilitychange", onevent);
      resolve();
    });

    mockImsRegServiceHelper.sendAsyncMessage("updateImsCapability", {
      capability: aCapability,
      unregisteredReason: aUnregisteredReason
    });
  });
}

function mockSetterError() {
  return new Promise(function(resolve, reject) {
    mockImsRegServiceHelper.addMessageListener('mockSetterError-complete',
                                               function onevent() {
      mockImsRegServiceHelper
        .removeMessageListener('mockSetterError-complete', onevent);

      ok(true, "mockSetterError completed.");
      resolve();
    });

    mockImsRegServiceHelper.sendAsyncMessage('mockSetterError');
  });
}

function cleanUp() {
  imsRegHandler = null;
  mockImsRegServiceHelper.sendAsyncMessage('teardown');
  mockImsRegServiceHelper.destroy();
  mockImsRegServiceHelper = null;
  SimpleTest.finish();
}

function startTestCommon(aTestCaseMain) {
  SimpleTest.waitForExplicitFinish();
  return ensureMockImsRegService()
    .then(ensureImsRegHandler)
    .then(aTestCaseMain)
    .catch(function(e) {
      ok(false, 'promise rejects during test: ' + e);
    })
    .then(cleanUp);
}
