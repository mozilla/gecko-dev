/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const {Cc: Cc, Ci: Ci, Cr: Cr, Cu: Cu} = SpecialPowers;

const SETTINGS_KEY_DATA_ENABLED = "ril.data.enabled";
const SETTINGS_KEY_DATA_APN_SETTINGS = "ril.data.apnSettings";

const PREF_KEY_RIL_DEBUGGING_ENABLED = "ril.debugging.enabled";

const TEST_APN_SETTINGS = [
  [{ "carrier": "T-Mobile US",
     "apn": "epc.tmobile.com",
     "mmsc": "http://mms.msg.eng.t-mobile.com/mms/wapenc",
     "types": ["default","supl","mms", "ims", "dun"] }]
];

const TEST_HOST_ROUTE = "10.1.2.200";

let _pendingEmulatorCmdCount = 0;
let _pendingEmulatorShellCmdCount = 0;

/**
 * Send emulator command with safe guard.
 *
 * We should only call |finish()| after all emulator command transactions
 * end, so here comes with the pending counter.  Resolve when the emulator
 * gives positive response, and reject otherwise.
 *
 * Fulfill params:
 *   result -- an array of emulator response lines.
 * Reject params:
 *   result -- an array of emulator response lines.
 *
 * @param aCommand
 *        A string command to be passed to emulator through its telnet console.
 *
 * @return A deferred promise.
 */
function runEmulatorCmdSafe(aCommand) {
  log("Emulator command: " + aCommand);

  return new Promise(function(aResolve, aReject) {
    ++_pendingEmulatorCmdCount;
    runEmulatorCmd(aCommand, function(aResult) {
      --_pendingEmulatorCmdCount;

      log("Emulator response: " + JSON.stringify(aResult));
      if (Array.isArray(aResult) &&
          aResult[aResult.length - 1] === "OK") {
        aResolve(aResult);
      } else {
        aReject(aResult);
      }
    });
  });
}

/**
 * Send emulator shell command with safe guard.
 *
 * We should only call |finish()| after all emulator shell command transactions
 * end, so here comes with the pending counter.  Resolve when the emulator
 * shell gives response. Never reject.
 *
 * Fulfill params:
 *   result -- an array of emulator shell response lines.
 *
 * @param aCommands
 *        A string array commands to be passed to emulator through adb shell.
 *
 * @return A deferred promise.
 */
function runEmulatorShellCmdSafe(aCommands) {
  return new Promise(function(aResolve, aReject) {
    ++_pendingEmulatorShellCmdCount;
    runEmulatorShell(aCommands, function(aResult) {
      --_pendingEmulatorShellCmdCount;

      log("Emulator shell response: " + JSON.stringify(aResult));
      aResolve(aResult);
    });
  });
}

/**
 * Get mozSettings value specified by @aKey.
 *
 * Resolve if that mozSettings value is retrieved successfully, reject
 * otherwise.
 *
 * Fulfill params:
 *   The corresponding mozSettings value of the key.
 * Reject params: (none)
 *
 * @param aKey
 *        A string.
 * @param aAllowError [optional]
 *        A boolean value.  If set to true, an error response won't be treated
 *        as test failure.  Default: false.
 *
 * @return A deferred promise.
 */
function getSettings(aKey, aAllowError) {
  let request = window.navigator.mozSettings.createLock().get(aKey);
  return request.then(function resolve(aValue) {
      ok(true, "getSettings(" + aKey + ") - success");
      return aValue[aKey];
    }, function reject(aError) {
      ok(aAllowError, "getSettings(" + aKey + ") - error");
    });
}

/**
 * Set mozSettings values.
 *
 * Resolve if that mozSettings value is set successfully, reject otherwise.
 *
 * Fulfill params: (none)
 * Reject params: (none)
 *
 * @param aSettings
 *        An object of format |{key1: value1, key2: value2, ...}|.
 * @param aAllowError [optional]
 *        A boolean value.  If set to true, an error response won't be treated
 *        as test failure.  Default: false.
 *
 * @return A deferred promise.
 */

function setSettings(aSettings, aAllowError) {
  let lock = window.navigator.mozSettings.createLock();
  let request = lock.set(aSettings);

  return new Promise(function(aResolve, aReject) {
    lock.onsettingstransactionsuccess = function () {
      ok(true, "setSettings(" + JSON.stringify(aSettings) + ")");
      aResolve();
    };
    lock.onsettingstransactionfailure = function (aEvent) {
      ok(aAllowError, "setSettings(" + JSON.stringify(aSettings) + ")");
      aReject();
    };
  });
}

/**
 * Set mozSettings value with only one key.
 *
 * Resolve if that mozSettings value is set successfully, reject otherwise.
 *
 * Fulfill params: (none)
 * Reject params: (none)
 *
 * @param aKey
 *        A string key.
 * @param aValue
 *        An object value.
 * @param aAllowError [optional]
 *        A boolean value.  If set to true, an error response won't be treated
 *        as test failure.  Default: false.
 *
 * @return A deferred promise.
 */
function setSettings1(aKey, aValue, aAllowError) {
  let settings = {};
  settings[aKey] = aValue;
  return setSettings(settings, aAllowError);
}

/**
 * Convenient MozSettings getter for SETTINGS_KEY_DATA_ENABLED.
 */
function getDataEnabled(aAllowError) {
  return getSettings(SETTINGS_KEY_DATA_ENABLED, aAllowError);
}

/**
 * Convenient MozSettings getter for SETTINGS_KEY_DATA_APN_SETTINGS.
 */
function getDataApnSettings(aAllowError) {
  return getSettings(SETTINGS_KEY_DATA_APN_SETTINGS, aAllowError);
}

/**
 * Convenient MozSettings setter for SETTINGS_KEY_DATA_APN_SETTINGS.
 */
function setDataApnSettings(aApnSettings, aAllowError) {
  return setSettings1(SETTINGS_KEY_DATA_APN_SETTINGS, aApnSettings, aAllowError);
}

/**
 * Wait for one named event.
 *
 * Resolve if that named event occurs.  Never reject.
 *
 * Fulfill params: the DOMEvent passed.
 *
 * @param aEventTarget
 *        An EventTarget object.
 * @param aEventName
 *        A string event name.
 * @param aMatchFun [optional]
 *        A matching function returns true or false to filter the event.
 *
 * @return A deferred promise.
 */
function waitForTargetEvent(aEventTarget, aEventName, aMatchFun) {
  return new Promise(function(aResolve, aReject) {
    aEventTarget.addEventListener(aEventName, function onevent(aEvent) {
      if (!aMatchFun || aMatchFun(aEvent)) {
        aEventTarget.removeEventListener(aEventName, onevent);
        ok(true, "Event '" + aEventName + "' got.");
        aResolve(aEvent);
      }
    });
  });
}

/**
 * Set voice/data state and wait for state change.
 *
 * Fulfill params: (none)
 *
 * @param aWhich
 *        "voice" or "data".
 * @param aState
 *        "unregistered", "searching", "denied", "roaming", or "home".
 * @param aServiceId [optional]
 *        A numeric DSDS service id. Default: the one indicated in
 *        start*TestCommon() or 0 if not indicated.
 *
 * @return A deferred promise.
 */
function setEmulatorVoiceDataStateAndWait(aWhich, aState, aServiceId) {
  aServiceId = aServiceId || 0;
  let mobileConn = navigator.mozMobileConnections[aServiceId];

  let promises = [];
  promises.push(waitForTargetEvent(mobileConn, aWhich + "change"));

  let cmd = "gsm " + aWhich + " " + aState;
  promises.push(runEmulatorCmdSafe(cmd));
  return Promise.all(promises);
}

/**
 * Set radio enabling state.
 *
 * Resolve no matter the request succeeds or fails. Never reject.
 *
 * Fulfill params: (none)
 *
 * @param aEnabled
 *        A boolean state.
 * @param aServiceId [optional]
 *        A numeric DSDS service id. Default: the one indicated in
 *        start*TestCommon() or 0 if not indicated.
 *
 * @return A deferred promise.
 */
function setRadioEnabled(aEnabled, aServiceId) {
  aServiceId = aServiceId || 0;
  let mobileConn = navigator.mozMobileConnections[aServiceId];

  let request = mobileConn.setRadioEnabled(aEnabled);
  return request.then(function onsuccess() {
      ok(true, "setRadioEnabled " + aEnabled + " on " + aServiceId + " success.");
    }, function onerror() {
      ok(false, "setRadioEnabled " + aEnabled + " on " + aServiceId + " " +
         request.error.name);
    });
}

/**
 * Set radio enabling state and wait for "radiostatechange" event.
 *
 * Resolve if radio state changed to the expected one. Never reject.
 *
 * Fulfill params: (none)
 *
 * @param aEnabled
 *        A boolean state.
 * @param aServiceId [optional]
 *        A numeric DSDS service id. Default: the one indicated in
 *        start*TestCommon() or 0 if not indicated.
 *
 * @return A deferred promise.
 */
function setRadioEnabledAndWait(aEnabled, aServiceId) {
  aServiceId = aServiceId || 0;
  let mobileConn = navigator.mozMobileConnections[aServiceId];
  let promises = [];

  promises.push(waitForTargetEvent(mobileConn, "radiostatechange", function() {
    // To ignore some transient states, we only resolve that deferred promise
    // when |radioState| equals to the expected one.
    return mobileConn.radioState === (aEnabled ? "enabled" : "disabled");
  }));
  promises.push(setRadioEnabled(aEnabled, aServiceId));

  return Promise.all(promises);
}

/**
 * Verify if a host route exists or not.
 *
 * @param aHost
 *        The host route to be verified.
 * @param aInterfaceName
 *        The interface name of the host route to be verified.
 * @param aShouldExists
 *        Indicates whether this host route should exist or not.
 */
function verifyHostRoute(aHost, aInterfaceName, aShouldExist) {
  return runEmulatorShellCmdSafe(['ip', 'route'])
    .then(function (aLines) {
      let exists = aLines.some(aLine => {
        let tokens = aLine.trim().split(/\s+/);
        let host = tokens[0];
        let devIndex = tokens.indexOf('dev');
        if (devIndex < 0 || devIndex + 1 >= tokens.length) {
          return false;
        }
        let ifname = tokens[devIndex + 1];

        if (host == aHost && ifname == aInterfaceName) {
          return true;
        }
      });

      is(aShouldExist, exists, "Host route (should not) exists.");
    });
}

/**
 * Verify the attributes of a data call, including name, addresses, gateways and
 * dnses.
 *
 * @param aDataCall
 *        The data call to be verified.
 * @param aIsAvailable
 *        Indicates whether the data call attributes should be available or not.
 */
function verifyDataCallAttributes(aDataCall, aIsAvailable) {
  if (aIsAvailable) {
    ok(aDataCall.name.length > 0, "check name");
    ok(aDataCall.addresses.length > 0, "check addresses");
    ok(aDataCall.gateways.length > 0, "check gateways");
    ok(aDataCall.dnses.length > 0, "check dnses");
  } else {
    is(aDataCall.name.length, 0, "check name");
    is(aDataCall.addresses.length, 0, "check addresses");
    is(aDataCall.dnses.length, 0, "check dnses");
    is(aDataCall.gateways.length, 0, "check gateways");
  }
}

/**
 * Request data call.
 *
 * Resolve if data call is requested successfully.
 *
 * @param aType
 *        Mobile network type.
 * @param aServiceId [optional]
 *        A numeric DSDS service id. Default, 0 if not indicated.
 *
 * @return A deferred promise.
 */
function requestDataCall(aType, aServiceId) {
  aServiceId = aServiceId || 0;

  return dataCallManager.requestDataCall(aType, aServiceId)
    .then(aDataCall => {
      is(aDataCall.state, "connected", "check state");
      is(aDataCall.type, aType, "check type");
      ok(aDataCall.addresses.length > 0, "check addresses");
      ok(aDataCall.gateways.length > 0, "check gateways");
      ok(aDataCall.dnses.length > 0, "check dnses");

      return aDataCall;
    }, aReason => {
      throw new Error(aReason);
    });
}

/**
 * Release data call.
 *
 * Resolve if release data call succeeds.
 *
 * @param aDataCall
 *        The data call to be released.
 *
 * @return A deferred promise.
 */
function releaseDataCall(aDataCall) {
  if (!aDataCall) {
    return Promise.reject("aDataCall does not exist");
  }

  return aDataCall.releaseDataCall();
}

/**
 * Push required permissions and test if |navigator.dataCallManager| exists.
 * Resolve if it does, reject otherwise.
 *
 * Fulfill params:
 *   dataCallManager -- a reference to navigator.dataCallManager.
 *
 * Reject params: (none)
 *
 * @return A deferred promise.
 */
let dataCallManager;
function ensureDataCallManager(aAdditionalPermissions) {
  if (aAdditionalPermissions.indexOf("datacall") < 0) {
    aAdditionalPermissions.push("datacall");
  }
  let permissions = [];
  for (let perm of aAdditionalPermissions) {
    permissions.push({ "type": perm, "allow": 1, "context": document });
  }

  return new Promise(function(aResolve, aReject) {
    SpecialPowers.pushPermissions(permissions, function() {
      ok(true, "permissions pushed: " + JSON.stringify(permissions));

      dataCallManager = window.navigator.dataCallManager;
      if (dataCallManager instanceof DataCallManager) {
        aResolve();
      } else {
        log("navigator.dataCallManager is unavailable");
        aReject();
      }
    });
  });
}

/**
 * Wait for pending emulator transactions and call |finish()|.
 */
function cleanUp() {
  // Use ok here so that we have at least one test run.
  ok(true, ":: CLEANING UP ::");

  waitFor(finish, function() {
    return _pendingEmulatorCmdCount === 0 &&
           _pendingEmulatorShellCmdCount === 0;
  });
}

/**
 * Basic test routine helper for data call tests.
 *
 * @param aTestCaseMain
 *        A function that takes no parameter.
 */
function startTestBase(aTestCaseMain) {
  // Turn on debugging pref.
  let debugPref = SpecialPowers.getBoolPref(PREF_KEY_RIL_DEBUGGING_ENABLED);
  SpecialPowers.setBoolPref(PREF_KEY_RIL_DEBUGGING_ENABLED, true);

  return Promise.resolve()
    .then(aTestCaseMain)
    .catch((aError) => {
      ok(false, "promise rejects during test: " + aError);
    })
    .then(() => {
      // Restore debugging pref.
      SpecialPowers.setBoolPref(PREF_KEY_RIL_DEBUGGING_ENABLED, debugPref);
      cleanUp();
    });
}

/**
 * Common test routine helper for data call tests.
 *
 * This function ensures global |dataCallManager| variable is available during
 * the process and performs clean-ups as well.
 *
 * @param aTestCaseMain
 *        A function that takes no parameter.
 * @param aAdditonalPermissions [optional]
 *        An array of permission strings other than "datacall" to be
 *        pushed. Default: empty string.
 */
function startTestCommon(aTestCaseMain, aAdditionalPermissions) {
  startTestBase(function() {
    return ensureDataCallManager(aAdditionalPermissions)
      .then(aTestCaseMain);
  });
}
