/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Emulate Promise.jsm semantics.
Promise.defer = function() { return new Deferred(); };
function Deferred()  {
  this.promise = new Promise(function(resolve, reject) {
    this.resolve = resolve;
    this.reject = reject;
  }.bind(this));
  Object.freeze(this);
}

let telephony;
let conference;

const kPrefRilDebuggingEnabled = "ril.debugging.enabled";

/**
 * Emulator helper.
 */
let emulator = (function() {
  let pendingCmdCount = 0;
  let originalRunEmulatorCmd = runEmulatorCmd;

  let pendingShellCount = 0;
  let originalRunEmulatorShell = runEmulatorShell;

  // Overwritten it so people could not call this function directly.
  runEmulatorCmd = function() {
    throw "Use emulator.runCmdWithCallback(cmd, callback) instead of runEmulatorCmd";
  };

  // Overwritten it so people could not call this function directly.
  runEmulatorShell = function() {
    throw "Use emulator.runShellCmd(cmd) instead of runEmulatorShell";
  };

  /**
   * @return Promise
   */
  function runCmd(cmd) {
    return new Promise(function(resolve, reject) {
      pendingCmdCount++;
      originalRunEmulatorCmd(cmd, function(result) {
        pendingCmdCount--;
        if (result[result.length - 1] === "OK") {
          resolve(result);
        } else {
          is(result[result.length - 1], "OK", "emulator command result.");
          reject();
        }
      });
    });
  }

  /**
   * @return Promise
   */
  function runCmdWithCallback(cmd, callback) {
    return runCmd(cmd).then(result => {
      if (callback && typeof callback === "function") {
        callback(result);
      }
    });
  }

  /**
   * @return Promise
   */
  function runShellCmd(aCommands) {
    return new Promise(function(resolve, reject) {
      ++pendingShellCount;
      originalRunEmulatorShell(aCommands, function(aResult) {
        --pendingShellCount;
        resolve(aResult);
      });
    });
  }

  /**
   * @return Promise
   */
  function waitFinish() {
    return new Promise(function(resolve, reject) {
      waitFor(function() {
        resolve();
      }, function() {
        return pendingCmdCount === 0 && pendingShellCount === 0;
      });
    });
  }

  return {
    runCmd: runCmd,
    runCmdWithCallback: runCmdWithCallback,
    runShellCmd: runShellCmd,
    waitFinish: waitFinish
  };
}());

/**
 * Telephony related helper functions.
 */
(function() {
  /**
   * @return Promise
   */
  function delay(ms) {
    return new Promise(function(resolve, reject) {
      let startTime = Date.now();
      waitFor(function() {
        resolve();
      },function() {
        let duration = Date.now() - startTime;
        return (duration >= ms);
      });
    });
  }

  /**
   * Wait for one named system message.
   *
   * Resolve if that named message is received. Never reject.
   *
   * Fulfill params: the message passed.
   *
   * @param aEventName
   *        A string message name.
   * @param aMatchFun [optional]
   *        A matching function returns true or false to filter the message. If no
   *        matching function passed the promise is resolved after receiving the
   *        first message.
   *
   * @return Promise<Message>
   */
  function waitForSystemMessage(aMessageName, aMatchFun = null) {
    // Current page may not register to receiving the message. We should
    // register it first.
    let systemMessenger = SpecialPowers.Cc["@mozilla.org/system-message-internal;1"]
                                       .getService(SpecialPowers.Ci.nsISystemMessagesInternal);

    // TODO: Find a better way to get current pageURI and manifestURI.
    systemMessenger.registerPage(aMessageName,
                                 SpecialPowers.Services.io.newURI("app://system.gaiamobile.org/index.html", null, null),
                                 SpecialPowers.Services.io.newURI("app://system.gaiamobile.org/manifest.webapp", null, null));

    return new Promise(function(aResolve, aReject) {
      window.navigator.mozSetMessageHandler(aMessageName, function(aMessage) {
        if (!aMatchFun || aMatchFun(aMessage)) {
          log("System message '" + aMessageName + "' got.");
          window.navigator.mozSetMessageHandler(aMessageName, null);
          aResolve(aMessage);
        }
      });
    });
  }

  /**
   * Wait for one named event.
   *
   * @param aTarget
   *        A event target.
   * @param aEventName
   *        A string event name.
   * @param aPredicate [optional]
   *        A predicate function, resolve the promise if aPredicate(event)
   *        return true
   * @return Promise<DOMEvent>
   */
  function waitForEvent(aTarget, aEventName, aPredicate) {
    return new Promise(function(resolve, reject) {
      aTarget.addEventListener(aEventName, function onevent(aEvent) {
        if (aPredicate === undefined || aPredicate(aEvent)) {
          aTarget.removeEventListener(aEventName, onevent);

          let label = "X";
          if (aTarget instanceof TelephonyCall) {
            label = "Call (" + aTarget.id.number + ")";
          } else if (aTarget instanceof TelephonyCallGroup) {
            label = "Conference";
          } else if (aTarget instanceof Telephony) {
            label = "Telephony";
          }

          log(label + " received event '" + aEventName + "'");
          resolve(aEvent);
        }
      });
    });
  }

  /**
   * Wait for callschanged event with event.call == aExpectedCall
   *
   * @param aTarget
   *        A event target.
   * @param aExpectedCall [optional]
   *        Expected call for event.call
   * @return Promise<TelephonyCall>
   */
  function waitForCallsChangedEvent(aTarget, aExpectedCall) {
    if (aExpectedCall === undefined) {
      return waitForEvent(aTarget, "callschanged").then(event => event.call);
    } else {
      return waitForEvent(aTarget, "callschanged",
                          event => event.call == aExpectedCall)
               .then(event => event.call);
    }
  }

  /**
   * Wait for call state event, e.g., "connected", "disconnected", ...
   *
   * @param aTarget
   *        A event target.
   * @param aState
   *        State name
   * @return Promise<TelephonyCall>
   */
  function waitForNamedStateEvent(aTarget, aState) {
    return waitForEvent(aTarget, aState)
      .then(event => {
        if (aTarget instanceof TelephonyCall) {
          is(aTarget, event.call, "event.call");
        }
        is(aTarget.state, aState, "check state");
        return aTarget;
      });
  }

  /**
   * Wait for groupchange event.
   *
   * @param aCall
   *        A TelephonyCall object.
   * @param aGroup
   *        The new group
   * @return Promise<TelephonyCall>
   */
  function waitForGroupChangeEvent(aCall, aGroup) {
    return waitForEvent(aCall, "groupchange")
      .then(() => {
        is(aCall.group, aGroup, "call group");
        return aCall;
      });
  }

  /**
   * Wait for statechange event.
   *
   * @param aTarget
   *        A event target.
   * @param aState
   *        The desired new state. Check it.
   * @return Promise<DOMEvent>
   */
  function waitForStateChangeEvent(aTarget, aState) {
    return waitForEvent(aTarget, "statechange")
      .then(() => {
        is(aTarget.state, aState);
        return aTarget;
      });
  }

  /**
   * @return Promise
   */
  function waitForNoCall() {
    return new Promise(function(resolve, reject) {
      waitFor(function() {
        resolve();
      }, function() {
        return telephony.calls.length === 0;
      });
    });
  }

  /**
   * @return Promise
   */
  function changeModemTech(aTech, aPreferredMask) {
    return Promise.resolve()
      .then(() => emulator.runCmd("modem tech " + aTech + " " + aPreferredMask))
      .then(() => emulator.runCmd("modem tech"))
      .then(result => is(result[0],
                         aTech + " " + aPreferredMask,
                         "Check modem 'tech/preferred mask'"));
  }

  /**
   * @return Promise
   */
  function clearCalls() {
    log("Clear existing calls.");

    // Hang up all calls.
    let hangUpPromises = [];

    for (let call of telephony.calls) {
      log(".. hangUp " + call.id.number);
      hangUpPromises.push(hangUp(call));
    }

    for (let call of conference.calls) {
      log(".. hangUp " + call.id.number);
      hangUpPromises.push(hangUp(call));
    }

    return Promise.all(hangUpPromises)
      .then(() => {
        return emulator.runCmd("gsm clear").then(waitForNoCall);
      })
      .then(waitForNoCall);
  }

  /**
   * Provide a string with format of the emulator call list result.
   *
   * @param prefix
   *        Possible values are "outbound" and "inbound".
   * @param number
   *        Call number.
   * @return A string with format of the emulator call list result.
   */
  function callStrPool(prefix, number) {
    let padding = "           : ";
    let numberInfo = prefix + number + padding.substr(number.length);

    let info = {};
    let states = ["ringing", "incoming", "waiting", "active", "held"];
    for (let state of states) {
      info[state] = numberInfo + state;
    }

    return info;
  }

  /**
   * Provide a corresponding string of an outgoing call. The string is with
   * format of the emulator call list result.
   *
   * @param number
   *        Number of an outgoing call.
   * @return A string with format of the emulator call list result.
   */
  function outCallStrPool(number) {
    return callStrPool("outbound to  ", number);
  }

  /**
   * Provide a corresponding string of an incoming call. The string is with
   * format of the emulator call list result.
   *
   * @param number
   *        Number of an incoming call.
   * @return A string with format of the emulator call list result.
   */
  function inCallStrPool(number) {
    return callStrPool("inbound from ", number);
  }

  /**
   * Check utility functions.
   */

  function checkInitialState() {
    log("Verify initial state.");
    ok(telephony.calls, 'telephony.call');
    ok(conference.calls, 'conference.calls');
    checkState(null, [], "", []);
  }

  /**
   * Convenient helper to compare two call lists (order is not important).
   */
  function checkCalls(actualCalls, expectedCalls) {
    if (actualCalls.length != expectedCalls.length) {
      ok(false, "check calls.length");
      return;
    }

    let expectedSet = new Set(expectedCalls);
    for (let i = 0; i < actualCalls.length; ++i) {
      ok(expectedSet.has(actualCalls[i]), "should contain the call");
    }
  }

  /**
   * Convenient helper to check the expected call number and name.
   *
   * @param number
   *        A string sent to modem.
   * @param numberPresentation
   *        An unsigned short integer sent to modem.
   * @param name
   *        A string sent to modem.
   * @param namePresentation
   *        An unsigned short integer sent to modem.
   * @param receivedNumber
   *        A string exposed by Telephony API.
   * @param receivedName
   *        A string exposed by Telephony API.
   */
  function checkCallId(number, numberPresentation, name, namePresentation,
                       receivedNumber, receivedName) {
    let expectedNum = !numberPresentation ? number : "";
    is(receivedNumber, expectedNum, "check number per numberPresentation");

    let expectedName;
    if (numberPresentation) {
      expectedName = "";
    } else if (!namePresentation) {
      expectedName = name ? name : "";
    } else {
      expectedName = "";
    }
    is(receivedName, expectedName, "check name per number/namePresentation");
  }

  /**
   * Convenient helper to check the call list existing in the emulator.
   *
   * @param expectedCallList
   *        An array of call info with the format of "callStrPool()[state]".
   * @return Promise
   */
  function checkEmulatorCallList(expectedCallList) {
    return emulator.runCmd("gsm list").then(result => {
      log("Call list is now: " + result);
      for (let i = 0; i < expectedCallList.length; ++i) {
        is(result[i], expectedCallList[i], "emulator calllist");
      }
    });
  }

  /**
   * Super convenient helper to check calls and state of mozTelephony and
   * mozTelephony.conferenceGroup.
   *
   * @param active
   *        A TelephonyCall object. Should be the expected active call.
   * @param calls
   *        An array of TelephonyCall objects. Should be the expected list of
   *        mozTelephony.calls.
   * @param conferenceState
   *        A string. Should be the expected conference state.
   * @param conferenceCalls
   *        An array of TelephonyCall objects. Should be the expected list of
   *        mozTelephony.conferenceGroup.calls.
   */
  function checkState(active, calls, conferenceState, conferenceCalls) {
    is(telephony.active, active, "telephony.active");
    checkCalls(telephony.calls, calls);
    is(conference.state, conferenceState, "conference.state");
    checkCalls(conference.calls, conferenceCalls);
  }

  /**
   * Super convenient helper to check calls and state of mozTelephony and
   * mozTelephony.conferenceGroup as well as the calls existing in the emulator.
   *
   * @param active
   *        A TelephonyCall object. Should be the expected active call.
   * @param calls
   *        An array of TelephonyCall objects. Should be the expected list of
   *        mozTelephony.calls.
   * @param conferenceState
   *        A string. Should be the expected conference state.
   * @param conferenceCalls
   *        An array of TelephonyCall objects. Should be the expected list of
   *        mozTelephony.conferenceGroup.calls.
   * @param callList
   *        An array of call info with the format of "callStrPool()[state]".
   * @return Promise
   */
  function checkAll(active, calls, conferenceState, conferenceCalls, callList) {
    checkState(active, calls, conferenceState, conferenceCalls);
    return checkEmulatorCallList(callList);
  }

  /**
   * Request utility functions.
   */

  /**
   * Make an outgoing call.
   *
   * @param number
   *        A string.
   * @param serviceId [optional]
   *        Identification of a service. 0 is set as default.
   * @return Promise<TelephonyCall>
   */
  function dial(number, serviceId) {
    serviceId = typeof serviceId !== "undefined" ? serviceId : 0;
    log("Make an outgoing call: " + number + ", serviceId: " + serviceId);

    let outCall;

    return telephony.dial(number, serviceId)
      .then(call => outCall = call)
      .then(() => {
        ok(outCall instanceof TelephonyCall, "check instance");
        is(outCall.id.number, number);
        is(outCall.state, "dialing");
        is(outCall.serviceId, serviceId);
      })
      .then(() => waitForNamedStateEvent(outCall, "alerting"));
  }

  /**
   * Make an outgoing emergency call.
   *
   * @param number
   *        A string.
   * @return Promise<TelephonyCall>
   */
  function dialEmergency(number) {
    log("Make an outgoing emergency call: " + number);

    let outCall;

    return telephony.dialEmergency(number)
      .then(call => outCall = call)
      .then(() => {
        ok(outCall instanceof TelephonyCall, "check instance");
        ok(outCall);
        is(outCall.id.number, number);
        is(outCall.state, "dialing");
      })
      .then(() => waitForNamedStateEvent(outCall, "alerting"))
      .then(() => {
        is(outCall.emergency, true, "check emergency");
        return outCall;
      });
  }

  /**
   * Simulate a call dialed out by STK directly.
   *
   * @param number
   *        A string.
   * @return Promise<TelephonyCall>
   */
  function dialSTK(number) {
    log("STK makes an outgoing call: " + number);

    let p1 = waitForCallsChangedEvent(telephony);
    let p2 = emulator.runCmd("stk setupcall " + number);

    return Promise.all([p1, p2])
      .then(result => {
        let call = result[0];

        ok(call instanceof TelephonyCall, "check instance");
        is(call.id.number, number, "check number");
        is(call.state, "dialing", "check call state");

        return waitForNamedStateEvent(call, "alerting");
      });
  }

  /**
   * Answer an incoming call.
   *
   * @param call
   *        An incoming TelephonyCall object.
   * @param conferenceStateChangeCallback [optional]
   *        A callback function which is called if answering an incoming call
   *        triggers conference state change.
   * @return Promise<TelephonyCall>
   */
  function answer(call, conferenceStateChangeCallback) {
    log("Answering the incoming call.");

    let promises = [];

    // incoming call triggers conference state change. We should wait for
    // |conference.onstatechange| before checking the state of the conference
    // call.
    if (conference.state === "connected") {
      let promise = waitForStateChangeEvent(conference, "held")
        .then(() => {
          if (typeof conferenceStateChangeCallback === "function") {
            conferenceStateChangeCallback();
          }
        });

      promises.push(promise);
    }

    promises.push(waitForNamedStateEvent(call, "connected"));
    promises.push(call.answer());

    return Promise.all(promises).then(() => call);
  }

  /**
   * Hold a call.
   *
   * @param call
   *        A TelephonyCall object.
   * @return Promise<TelephonyCall>
   */
  function hold(call) {
    log("Putting the call on hold.");

    let promises = [];

    promises.push(waitForNamedStateEvent(call, "held"));
    promises.push(call.hold());

    return Promise.all(promises).then(() => call);
  }

  /**
   * Resume a call.
   *
   * @param call
   *        A TelephonyCall object.
   * @return Promise<TelephonyCall>
   */
  function resume(call) {
    log("Resuming the held call.");

    let promises = [];

    promises.push(waitForNamedStateEvent(call, "connected"));
    promises.push(call.resume());

    return Promise.all(promises).then(() => call);
  }

  /**
   * Locally hang up a call.
   *
   * @param call
   *        A TelephonyCall object.
   * @return Promise<TelephonyCall>
   */
  function hangUp(call) {
    log("Local hanging up the call: " + call.id.number);

    let promises = [];

    promises.push(waitForNamedStateEvent(call, "disconnected"));
    promises.push(call.hangUp());

    return Promise.all(promises).then(() => call);
  }

  /**
   * Simulate an incoming call.
   *
   * @param number
   *        A string.
   * @param numberPresentation [optional]
   *        An unsigned short integer.
   * @param name [optional]
   *        A string.
   * @param namePresentation [optional]
   *        An unsigned short integer.
   * @return Promise<TelephonyCall>
   */
  function remoteDial(number, numberPresentation, name, namePresentation) {
    log("Simulating an incoming call.");

    numberPresentation = numberPresentation || "";
    name = name || "";
    namePresentation = namePresentation || "";
    emulator.runCmd("gsm call " + number + "," + numberPresentation + "," + name +
                 "," + namePresentation);

    return waitForEvent(telephony, "incoming")
      .then(event => {
        let call = event.call;

        ok(call);
        is(call.state, "incoming");
        checkCallId(number, numberPresentation, name, namePresentation,
                    call.id.number, call.id.name);

        return call;
      });
  }

  /**
   * Remote party answers the call.
   *
   * @param call
   *        A TelephonyCall object.
   * @return Promise<TelephonyCall>
   */
  function remoteAnswer(call) {
    log("Remote answering the call: " + call.id.number);

    emulator.runCmd("gsm accept " + call.id.number);

    return waitForNamedStateEvent(call, "connected");
  }

  /**
   * Remote party hangs up the call.
   *
   * @param call
   *        A TelephonyCall object.
   * @return Promise<TelephonyCall>
   */
  function remoteHangUp(call) {
    log("Remote hanging up the call: " + call.id.number);

    emulator.runCmd("gsm cancel " + call.id.number);

    return waitForNamedStateEvent(call, "disconnected");
  }

  /**
   * Remote party hangs up all the calls.
   *
   * @param calls
   *        An array of TelephonyCall objects.
   * @return Promise
   */
  function remoteHangUpCalls(calls) {
    let promises = calls.map(remoteHangUp);
    return Promise.all(promises);
  }

  /**
   * Add calls to conference.
   *
   * @param callsToAdd
   *        An array of TelephonyCall objects to be added into conference. The
   *        length of the array should be 1 or 2.
   * @param connectedCallback [optional]
   *        A callback function which is called when conference state becomes
   *        connected.
   * @return Promise<[TelephonyCall ...]>
   */
  function addCallsToConference(callsToAdd, connectedCallback) {
    log("Add " + callsToAdd.length + " calls into conference.");

    let promises = [];

    for (let call of callsToAdd) {
      promises.push(waitForCallsChangedEvent(conference, call));
      promises.push(waitForGroupChangeEvent(call, conference));
      promises.push(waitForNamedStateEvent(call, "connected"));
      promises.push(waitForStateChangeEvent(call, "connected"));
    }

    let promise = waitForNamedStateEvent(conference, "connected")
      .then(() => {
        if (typeof connectedCallback === "function") {
          connectedCallback();
        }
      });
    promises.push(promise);

    // Cannot use apply() through webidl, so just separate the cases to handle.
    if (callsToAdd.length == 2) {
      promise = conference.add(callsToAdd[0], callsToAdd[1]);
      promises.push(promise);
    } else {
      promise = conference.add(callsToAdd[0]);
      promises.push(promise);
    }

    return Promise.all(promises).then(() => conference.calls);
  }

  /**
   * Hold the conference.
   *
   * @param callsInConference
   *        An array of TelephonyCall objects existing in conference.
   * @param heldCallback [optional]
   *        A callback function which is called when conference state becomes
   *        held.
   * @return Promise<[TelephonyCall ...]>
   */
  function holdConference(callsInConference, heldCallback) {
    log("Holding the conference call.");

    let promises = [];

    for (let call of callsInConference) {
      promises.push(waitForNamedStateEvent(call, "held"));
    }

    let promise = waitForNamedStateEvent(conference, "held")
      .then(() => {
        if (typeof heldCallback === "function") {
          heldCallback();
        }
      });
    promises.push(promise);

    promises.push(conference.hold());

    return Promise.all(promises).then(() => conference.calls);
  }

  /**
   * Resume the conference.
   *
   * @param callsInConference
   *        An array of TelephonyCall objects existing in conference.
   * @param connectedCallback [optional]
   *        A callback function which is called when conference state becomes
   *        connected.
   * @return Promise<[TelephonyCall ...]>
   */
  function resumeConference(callsInConference, connectedCallback) {
    log("Resuming the held conference call.");

    let promises = [];

    for (let call of callsInConference) {
      promises.push(waitForNamedStateEvent(call, "connected"));
    }

    let promise = waitForNamedStateEvent(conference, "connected")
      .then(() => {
        if (typeof connectedCallback === "function") {
          connectedCallback();
        }
      });
    promises.push(promise);

    promises.push(conference.resume());

    return Promise.all(promises).then(() => conference.calls);
  }

  /**
   * Remove a call out of conference.
   *
   * @param callToRemove
   *        A TelephonyCall object existing in conference.
   * @param autoRemovedCalls
   *        An array of TelephonyCall objects which is going to be automatically
   *        removed. The length of the array should be 0 or 1.
   * @param remainedCalls
   *        An array of TelephonyCall objects which remain in conference.
   * @param stateChangeCallback [optional]
   *        A callback function which is called when conference state changes.
   * @return Promise<[TelephonyCall ...]>
   */
  function removeCallInConference(callToRemove, autoRemovedCalls, remainedCalls,
                                  statechangeCallback) {
    log("Removing a participant from the conference call.");

    is(conference.state, 'connected');

    let promises = [];

    // callToRemove.
    promises.push(waitForCallsChangedEvent(telephony, callToRemove));
    promises.push(waitForCallsChangedEvent(conference, callToRemove));
    promises.push(waitForGroupChangeEvent(callToRemove, null).then(() => {
      is(callToRemove.state, 'connected');
    }));

    // When a call is removed from conference with 2 calls, another one will be
    // automatically removed from group and be put on hold.
    for (let call of autoRemovedCalls) {
      promises.push(waitForCallsChangedEvent(telephony, call));
      promises.push(waitForCallsChangedEvent(conference, call));
      promises.push(waitForGroupChangeEvent(call, null));
      promises.push(waitForStateChangeEvent(call, "held"));
    }

    // Remained call in conference will be held.
    for (let call of remainedCalls) {
      promises.push(waitForStateChangeEvent(call, "held"));
    }

    let finalConferenceState = remainedCalls.length ? "held" : "";
    let promise = waitForStateChangeEvent(conference, finalConferenceState)
      .then(() => {
        if (typeof statechangeCallback === 'function') {
          statechangeCallback();
        }
      });
    promises.push(promise);

    promises.push(conference.remove(callToRemove));

    return Promise.all(promises)
      .then(() => checkCalls(conference.calls, remainedCalls))
      .then(() => conference.calls);
  }

  /**
   * Hangup a call in conference.
   *
   * @param callToHangUp
   *        A TelephonyCall object existing in conference.
   * @param autoRemovedCalls
   *        An array of TelephonyCall objects which is going to be automatically
   *        removed. The length of the array should be 0 or 1.
   * @param remainedCalls
   *        An array of TelephonyCall objects which remain in conference.
   * @param stateChangeCallback [optional]
   *        A callback function which is called when conference state changes.
   * @return Promise<[TelephonyCall ...]>
   */
  function hangUpCallInConference(callToHangUp, autoRemovedCalls, remainedCalls,
                                  statechangeCallback) {
    log("Release one call in conference.");

    let promises = [];

    // callToHangUp.
    promises.push(waitForCallsChangedEvent(conference, callToHangUp));

    // When a call is removed from conference with 2 calls, another one will be
    // automatically removed from group.
    for (let call of autoRemovedCalls) {
      promises.push(waitForCallsChangedEvent(telephony, call));
      promises.push(waitForCallsChangedEvent(conference, call));
      promises.push(waitForGroupChangeEvent(call, null));
    }

    if (remainedCalls.length === 0) {
      let promise = waitForStateChangeEvent(conference, "")
        .then(() => {
          if (typeof statechangeCallback === 'function') {
            statechangeCallback();
          }
        });
      promises.push(promise);
    }

    promises.push(remoteHangUp(callToHangUp));

    return Promise.all(promises)
      .then(() => checkCalls(conference.calls, remainedCalls))
      .then(() => conference.calls);
  }

  /**
   * Hangup conference.
   *
   * @return Promise
   */
  function hangUpConference() {
    log("Hangup conference.");

    let promises = [];

    promises.push(waitForStateChangeEvent(conference, ""));

    for (let call of conference.calls) {
      promises.push(waitForNamedStateEvent(call, "disconnected"));
    }

    return conference.hangUp().then(() => {
      return Promise.all(promises);
    });
  }

  /**
   * Create a conference with an outgoing call and an incoming call.
   *
   * @param outNumber
   *        Number of an outgoing call.
   * @param inNumber
   *        Number of an incoming call.
   * @return Promise<[outCall, inCall]>
   */
  function createConferenceWithTwoCalls(outNumber, inNumber) {
    let outCall;
    let inCall;
    let outInfo = outCallStrPool(outNumber);
    let inInfo = inCallStrPool(inNumber);

    return Promise.resolve()
      .then(checkInitialState)
      .then(() => dial(outNumber))
      .then(call => { outCall = call; })
      .then(() => checkAll(outCall, [outCall], '', [], [outInfo.ringing]))
      .then(() => remoteAnswer(outCall))
      .then(() => checkAll(outCall, [outCall], '', [], [outInfo.active]))
      .then(() => remoteDial(inNumber))
      .then(call => { inCall = call; })
      .then(() => checkAll(outCall, [outCall, inCall], '', [],
                           [outInfo.active, inInfo.waiting]))
      .then(() => answer(inCall))
      .then(() => checkAll(inCall, [outCall, inCall], '', [],
                           [outInfo.held, inInfo.active]))
      .then(() => addCallsToConference([outCall, inCall], function() {
        checkState(conference, [], 'connected', [outCall, inCall]);
      }))
      .then(() => checkAll(conference, [], 'connected', [outCall, inCall],
                           [outInfo.active, inInfo.active]))
      .then(() => {
        return [outCall, inCall];
      });
  }

  /**
   * Create a new incoming call and add it into the conference.
   *
   * @param inNumber
   *        Number of an incoming call.
   * @param conferenceCalls
   *        Calls already in conference.
   * @return Promise<[calls in the conference]>
   */
  function createCallAndAddToConference(inNumber, conferenceCalls) {
    // Create an info array. allInfo = [info1, info2, ...].
    let allInfo = conferenceCalls.map(function(call, i) {
      return (i === 0) ? outCallStrPool(call.id.number)
                       : inCallStrPool(call.id.number);
    });

    // Define state property of the info array.
    // Ex: allInfo.active = [info1.active, info2.active, ...].
    function addInfoState(allInfo, state) {
      Object.defineProperty(allInfo, state, {
        get: function() {
          return allInfo.map(function(info) { return info[state]; });
        }
      });
    }

    for (let state of ["ringing", "incoming", "waiting", "active", "held"]) {
      addInfoState(allInfo, state);
    }

    let newCall;
    let newInfo = inCallStrPool(inNumber);

    return remoteDial(inNumber)
      .then(call => { newCall = call; })
      .then(() => checkAll(conference, [newCall], 'connected', conferenceCalls,
                           allInfo.active.concat(newInfo.waiting)))
      .then(() => answer(newCall, function() {
        checkState(newCall, [newCall], 'held', conferenceCalls);
      }))
      .then(() => checkAll(newCall, [newCall], 'held', conferenceCalls,
                           allInfo.held.concat(newInfo.active)))
      .then(() => {
        // We are going to add the new call into the conference.
        conferenceCalls.push(newCall);
        allInfo.push(newInfo);
      })
      .then(() => addCallsToConference([newCall], function() {
        checkState(conference, [], 'connected', conferenceCalls);
      }))
      .then(() => checkAll(conference, [], 'connected', conferenceCalls,
                           allInfo.active))
      .then(() => {
        return conferenceCalls;
      });
  }

  /**
   * Setup a conference with an outgoing call and N incoming calls.
   *
   * @param callNumbers
   *        Array of numbers, the first number is for outgoing call and the
   *        remaining numbers are for incoming calls.
   * @return Promise<[calls in the conference]>
   */
  function setupConference(callNumbers) {
    log("Create a conference with " + callNumbers.length + " calls.");

    let promise = createConferenceWithTwoCalls(callNumbers[0], callNumbers[1]);

    callNumbers.shift();
    callNumbers.shift();
    for (let number of callNumbers) {
      promise = promise.then(createCallAndAddToConference.bind(null, number));
    }

    return promise;
  }

  /**
   * Send out the MMI code.
   *
   * @param mmi
   *        String of MMI code
   * @return Promise<MozMMIResult>
   */
  function sendMMI(mmi) {
    return telephony.dial(mmi).then(mmiCall => {
      ok(mmiCall instanceof MMICall, "mmiCall is instance of MMICall");
      ok(mmiCall.result instanceof Promise, "result is Promise");
      return mmiCall.result;
    });
  }

  function sendTone(tone, pause, serviceId) {
    log("Send DTMF " + tone + " serviceId " + serviceId);
    return telephony.sendTones(tone, pause, null, serviceId);
  }

  /**
   * Config radio.
   *
   * @param connection
   *        MobileConnection object.
   * @param enabled
   *        True to enable the radio.
   * @return Promise
   */
  function setRadioEnabled(connection, enabled) {
    let desiredRadioState = enabled ? 'enabled' : 'disabled';
    log("Set radio: " + desiredRadioState);

    if (connection.radioState === desiredRadioState) {
      return Promise.resolve();
    }

    let promises = [];

    promises.push(gWaitForEvent(connection, "radiostatechange", event => {
      let state = connection.radioState;
      log("current radioState: " + state);
      return state == desiredRadioState;
    }));

    // Wait for icc status to finish updating. Please see bug 1169504 for the
    // reason why we need this.
    promises.push(gWaitForEvent(connection, "iccchange", event => {
      let iccId = connection.iccId;
      log("current iccId: " + iccId);
      return !!iccId === enabled;
    }));

    promises.push(connection.setRadioEnabled(enabled));

    return Promise.all(promises);
  }

  function setRadioEnabledAll(enabled) {
    let promises = [];
    let numOfSim = navigator.mozMobileConnections.length;

    for (let i = 0; i < numOfSim; i++) {
      let connection = navigator.mozMobileConnections[i];
      ok(connection instanceof MozMobileConnection,
         "connection[" + i + "] is instanceof " + connection.constructor);

         promises.push(setRadioEnabled(connection, enabled));
    }

    return Promise.all(promises);
  }

  /**
   * Public members.
   */

  this.gDelay = delay;
  this.gWaitForSystemMessage = waitForSystemMessage;
  this.gWaitForEvent = waitForEvent;
  this.gWaitForCallsChangedEvent = waitForCallsChangedEvent;
  this.gWaitForNamedStateEvent = waitForNamedStateEvent;
  this.gWaitForStateChangeEvent = waitForStateChangeEvent;
  this.gCheckInitialState = checkInitialState;
  this.gChangeModemTech = changeModemTech;
  this.gClearCalls = clearCalls;
  this.gOutCallStrPool = outCallStrPool;
  this.gInCallStrPool = inCallStrPool;
  this.gCheckState = checkState;
  this.gCheckAll = checkAll;
  this.gSendMMI = sendMMI;
  this.gDial = dial;
  this.gDialEmergency = dialEmergency;
  this.gDialSTK = dialSTK;
  this.gAnswer = answer;
  this.gHangUp = hangUp;
  this.gHold = hold;
  this.gResume = resume;
  this.gRemoteDial = remoteDial;
  this.gRemoteAnswer = remoteAnswer;
  this.gRemoteHangUp = remoteHangUp;
  this.gRemoteHangUpCalls = remoteHangUpCalls;
  this.gAddCallsToConference = addCallsToConference;
  this.gHoldConference = holdConference;
  this.gResumeConference = resumeConference;
  this.gRemoveCallInConference = removeCallInConference;
  this.gHangUpCallInConference = hangUpCallInConference;
  this.gHangUpConference = hangUpConference;
  this.gSendTone = sendTone;
  this.gSetupConference = setupConference;
  this.gSetRadioEnabled = setRadioEnabled;
  this.gSetRadioEnabledAll = setRadioEnabledAll;
}());

function _startTest(permissions, test) {
  function typesToPermissions(types) {
    return types.map(type => {
      return {
        "type": type,
        "allow": 1,
        "context": document
      };
    });
  }

  function ensureRadio() {
    log("== Ensure Radio ==");
    return new Promise(function(resolve, reject) {
      SpecialPowers.pushPermissions(typesToPermissions(["mobileconnection"]), () => {
        gSetRadioEnabledAll(true).then(() => {
          SpecialPowers.popPermissions(() => {
            resolve();
          });
        });
      });
    });
  }

  function permissionSetUp() {
    log("== Permission SetUp ==");
    return new Promise(function(resolve, reject) {
      SpecialPowers.pushPermissions(typesToPermissions(permissions), resolve);
    });
  }

  let debugPref;

  function setUp() {
    log("== Test SetUp ==");

    // Turn on debugging pref.
    debugPref = SpecialPowers.getBoolPref(kPrefRilDebuggingEnabled);
    SpecialPowers.setBoolPref(kPrefRilDebuggingEnabled, true);
    log("Set debugging pref: " + debugPref + " => true");

    return Promise.resolve()
      .then(ensureRadio)
      .then(permissionSetUp)
      .then(() => {
        // Make sure that we get the telephony after adding permission.
        telephony = window.navigator.mozTelephony;
        ok(telephony);
        conference = telephony.conferenceGroup;
        ok(conference);
      })
      .then(gClearCalls)
      .then(gCheckInitialState);
  }

  // Extend finish() with tear down.
  finish = (function() {
    let originalFinish = finish;

    function tearDown() {
      log("== Test TearDown ==");
      emulator.waitFinish()
        .then(() => {
          // Restore debugging pref.
          SpecialPowers.setBoolPref(kPrefRilDebuggingEnabled, debugPref);
          log("Set debugging pref: true => " + debugPref);
        })
        .then(function() {
          originalFinish.apply(this, arguments);
        });
    }

    return tearDown.bind(this);
  }());

  setUp().then(() => {
    log("== Test Start ==");
    test();
  })
  .catch(error => ok(false, error));
}

function startTest(test) {
  _startTest(["telephony"], test);
}

function startTestWithPermissions(permissions, test) {
  _startTest(permissions.concat("telephony"), test);
}

function startDSDSTest(test) {
  let numRIL;
  try {
    numRIL = SpecialPowers.getIntPref("ril.numRadioInterfaces");
  } catch (ex) {
    numRIL = 1;  // Pref not set.
  }

  if (numRIL > 1) {
    startTest(test);
  } else {
    log("Not a DSDS environment. Test is skipped.");
    ok(true);  // We should run at least one test.
    finish();
  }
}
