/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const kObservedTopics = [
  "getUserMedia:response:allow",
  "getUserMedia:revoke",
  "getUserMedia:response:deny",
  "getUserMedia:request",
  "recording-device-events",
  "recording-window-ended"
];

const PREF_PERMISSION_FAKE = "media.navigator.permission.fake";
const PREF_LOOP_CSP = "loop.CSP";


Cu.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyServiceGetter(this, "MediaManagerService",
                                   "@mozilla.org/mediaManagerService;1",
                                   "nsIMediaManagerService");

var gTab;

// Taken from dom/media/tests/mochitest/head.js
function isMacOSX10_6orOlder() {
  var is106orOlder = false;

  if (navigator.platform.indexOf("Mac") == 0) {
    var version = Cc["@mozilla.org/system-info;1"]
                    .getService(Ci.nsIPropertyBag2)
                    .getProperty("version");
    // the next line is correct: Mac OS 10.6 corresponds to Darwin version 10.x !
    // Mac OS 10.7 is Darwin version 11.x. the |version| string we've got here
    // is the Darwin version.
    is106orOlder = (parseFloat(version) < 11.0);
  }
  return is106orOlder;
}

// Screensharing is disabled on older platforms (WinXP and Mac 10.6).
function isOldPlatform() {
  const isWinXP = navigator.userAgent.indexOf("Windows NT 5.1") != -1;
  if (isMacOSX10_6orOlder() || isWinXP) {
    info(true, "Screensharing disabled for OSX10.6 and WinXP");
    return true;
  }
  return false;
}

// Linux prompts aren't working for screensharing.
function isLinux() {
  return navigator.platform.indexOf("Linux") != -1;
}

var gObservedTopics = {};
function observer(aSubject, aTopic, aData) {
  if (!(aTopic in gObservedTopics))
    gObservedTopics[aTopic] = 1;
  else
    ++gObservedTopics[aTopic];
}

function promiseObserverCalled(aTopic, aAction) {
  let deferred = Promise.defer();
  info("Waiting for " + aTopic);

  Services.obs.addObserver(function observer(aSubject, topic, aData) {
    ok(true, "got " + aTopic + " notification");
    info("Message: " + aData);
    Services.obs.removeObserver(observer, aTopic);

    if (kObservedTopics.indexOf(aTopic) != -1) {
      if (!(aTopic in gObservedTopics))
        gObservedTopics[aTopic] = -1;
      else
        --gObservedTopics[aTopic];
    }

    deferred.resolve();
  }, aTopic, false);

  if (aAction)
    aAction();

  return deferred.promise;
}

function promisePopupNotification(aName) {
  let deferred = Promise.defer();

  waitForCondition(() => PopupNotifications.getNotification(aName),
                   () => {
    ok(!!PopupNotifications.getNotification(aName),
       aName + " notification appeared");

    deferred.resolve();
  }, "timeout waiting for popup notification " + aName);

  return deferred.promise;
}

function promiseNoPopupNotification(aName) {
  let deferred = Promise.defer();
  info("Waiting for " + aName + " to be removed");

  waitForCondition(() => !PopupNotifications.getNotification(aName),
                   () => {
    ok(!PopupNotifications.getNotification(aName),
       aName + " notification removed");
    deferred.resolve();
  }, "timeout waiting for popup notification " + aName + " to disappear");

  return deferred.promise;
}

function expectObserverCalled(aTopic) {
  is(gObservedTopics[aTopic], 1, "expected notification " + aTopic);
  if (aTopic in gObservedTopics)
    --gObservedTopics[aTopic];
}

function expectNoObserverCalled() {
  for (let topic in gObservedTopics) {
    if (gObservedTopics[topic])
      is(gObservedTopics[topic], 0, topic + " notification unexpected");
  }
  gObservedTopics = {};
}

function promiseMessage(aMessage, aAction) {
  let deferred = Promise.defer();

  content.addEventListener("message", function messageListener(event) {
    content.removeEventListener("message", messageListener);
    is(event.data, aMessage, "received " + aMessage);
    if (event.data == aMessage)
      deferred.resolve();
    else
      deferred.reject();
  });

  if (aAction)
    aAction();

  return deferred.promise;
}

function getMediaCaptureState() {
  let hasVideo = {};
  let hasAudio = {};
  let hasScreenShare = {};
  let hasWindowShare = {};
  MediaManagerService.mediaCaptureWindowState(content, hasVideo, hasAudio,
                                              hasScreenShare, hasWindowShare);
  if (hasVideo.value && hasAudio.value)
    return "CameraAndMicrophone";
  if (hasVideo.value)
    return "Camera";
  if (hasAudio.value)
    return "Microphone";
  if (hasScreenShare)
    return "Screen";
  if (hasWindowShare)
    return "Window";
  return "none";
}

function* closeStream(aAlreadyClosed) {
  expectNoObserverCalled();

  info("closing the stream");
  content.wrappedJSObject.closeStream();

  if (!aAlreadyClosed)
    yield promiseObserverCalled("recording-device-events");

  yield promiseNoPopupNotification("webRTC-sharingDevices");
  if (!aAlreadyClosed)
    expectObserverCalled("recording-window-ended");

  yield* assertWebRTCIndicatorStatus(null);
}

function loadPage(aUrl) {
  let deferred = Promise.defer();

  gTab.linkedBrowser.addEventListener("load", function onload() {
    gTab.linkedBrowser.removeEventListener("load", onload, true);

    is(PopupNotifications._currentNotifications.length, 0,
       "should start the test without any prior popup notification");

    deferred.resolve();
  }, true);
  content.location = aUrl;
  return deferred.promise;
}

// A fake about module to map get_user_media.html to different about urls.
function fakeLoopAboutModule() {
}

fakeLoopAboutModule.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIAboutModule]),
  newChannel: function (aURI, aLoadInfo) {
    let rootDir = getRootDirectory(gTestPath);
    let uri = Services.io.newURI(rootDir + "get_user_media.html", null, null);
    let chan = Services.io.newChannelFromURIWithLoadInfo(uri, aLoadInfo);

    chan.owner = Services.scriptSecurityManager.getSystemPrincipal();
    return chan;
  },
  getURIFlags: function (aURI) {
    return Ci.nsIAboutModule.URI_SAFE_FOR_UNTRUSTED_CONTENT |
           Ci.nsIAboutModule.ALLOW_SCRIPT |
           Ci.nsIAboutModule.HIDE_FROM_ABOUTABOUT;
  }
};

let factory = XPCOMUtils._getFactory(fakeLoopAboutModule);
let registrar = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);

let originalLoopCsp = Services.prefs.getCharPref(PREF_LOOP_CSP);
registerCleanupFunction(function() {
  gBrowser.removeCurrentTab();
  kObservedTopics.forEach(topic => {
    Services.obs.removeObserver(observer, topic);
  });
  Services.prefs.clearUserPref(PREF_PERMISSION_FAKE);
  Services.prefs.setCharPref(PREF_LOOP_CSP, originalLoopCsp);
});

const permissionError = "error: PermissionDeniedError: The user did not grant permission for the operation.";

let gTests = [

{
  desc: "getUserMedia about:loopconversation shouldn't prompt",
  run: function checkAudioVideoLoop() {
    Services.prefs.setCharPref(PREF_LOOP_CSP, "default-src 'unsafe-inline'");

    let classID = Cc["@mozilla.org/uuid-generator;1"]
                    .getService(Ci.nsIUUIDGenerator).generateUUID();
    registrar.registerFactory(classID, "",
                              "@mozilla.org/network/protocol/about;1?what=loopconversation",
                              factory);

    yield loadPage("about:loopconversation");

    yield promiseObserverCalled("recording-device-events", () => {
      info("requesting devices");
      content.wrappedJSObject.requestDevice(true, true);
    });
    // Wait for the devices to actually be captured and running before
    // proceeding.
    yield promisePopupNotification("webRTC-sharingDevices");

    is(getMediaCaptureState(), "CameraAndMicrophone",
       "expected camera and microphone to be shared");

    yield closeStream();

    registrar.unregisterFactory(classID, factory);
    Services.prefs.setCharPref(PREF_LOOP_CSP, originalLoopCsp);
  }
},

{
  desc: "getUserMedia about:loopconversation should prompt for window sharing",
  run: function checkShareScreenLoop() {
    if (isOldPlatform() || isLinux()) {
      return;
    }

    Services.prefs.setCharPref(PREF_LOOP_CSP, "default-src 'unsafe-inline'");

    let classID = Cc["@mozilla.org/uuid-generator;1"]
                    .getService(Ci.nsIUUIDGenerator).generateUUID();
    registrar.registerFactory(classID, "",
                              "@mozilla.org/network/protocol/about;1?what=loopconversation",
                              factory);

    yield loadPage("about:loopconversation");

    yield promiseObserverCalled("getUserMedia:request", () => {
      info("requesting screen");
      content.wrappedJSObject.requestDevice(false, true, "window");
    });
    // Wait for the devices to actually be captured and running before
    // proceeding.
    yield promisePopupNotification("webRTC-shareDevices");

    isnot(getMediaCaptureState(), "Window",
       "expected camera and microphone not to be shared");

    yield promiseMessage(permissionError, () => {
      PopupNotifications.panel.firstChild.button.click();
    });

    expectObserverCalled("getUserMedia:response:deny");
    expectObserverCalled("recording-window-ended");

    registrar.unregisterFactory(classID, factory);
    Services.prefs.setCharPref(PREF_LOOP_CSP, originalLoopCsp);
  }
},

{
  desc: "getUserMedia about:evil should prompt",
  run: function checkAudioVideoNonLoop() {
    let classID = Cc["@mozilla.org/uuid-generator;1"]
                    .getService(Ci.nsIUUIDGenerator).generateUUID();
    registrar.registerFactory(classID, "",
                              "@mozilla.org/network/protocol/about;1?what=evil",
                              factory);

    yield loadPage("about:evil");

    yield promiseObserverCalled("getUserMedia:request", () => {
      info("requesting devices");
      content.wrappedJSObject.requestDevice(true, true);
    });

    isnot(getMediaCaptureState(), "CameraAndMicrophone",
       "expected camera and microphone not to be shared");

    registrar.unregisterFactory(classID, factory);
  }
},

];

function test() {
  waitForExplicitFinish();

  Services.prefs.setBoolPref(PREF_PERMISSION_FAKE, true);
  // Ensure this is always true
  Services.prefs.setBoolPref("media.getusermedia.screensharing.enabled", true);

  gTab = gBrowser.addTab();
  gBrowser.selectedTab = gTab;

  kObservedTopics.forEach(topic => {
    Services.obs.addObserver(observer, topic, false);
  });

  Task.spawn(function () {
    for (let test of gTests) {
      info(test.desc);
      yield test.run();

      // Cleanup before the next test
      expectNoObserverCalled();
    }
  }).then(finish, ex => {
    ok(false, "Unexpected Exception: " + ex);
    finish();
  });
}

function wait(time) {
  let deferred = Promise.defer();
  setTimeout(deferred.resolve, time);
  return deferred.promise;
}
