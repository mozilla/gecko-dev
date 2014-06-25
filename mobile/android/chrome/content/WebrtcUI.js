/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

XPCOMUtils.defineLazyModuleGetter(this, "Notifications", "resource://gre/modules/Notifications.jsm");

var WebrtcUI = {
  _notificationId: null,

  observe: function(aSubject, aTopic, aData) {
    if (aTopic === "getUserMedia:request") {
      this.handleRequest(aSubject, aTopic, aData);
    } else if (aTopic === "recording-device-events") {
      switch (aData) {
        case "shutdown":
        case "starting":
          this.notify();
          break;
      }
    }
  },

  notify: function() {
    let windows = MediaManagerService.activeMediaCaptureWindows;
    let count = windows.Count();
    let msg = {};
    if (count == 0) {
      if (this._notificationId) {
        Notifications.cancel(this._notificationId);
        this._notificationId = null;
      }
    } else {
      let notificationOptions = {
        title: Strings.brand.GetStringFromName("brandShortName"),
        when: null, // hide the date row
        light: [0xFF9500FF, 1000, 1000],
        ongoing: true
      };

      let cameraActive = false;
      let audioActive = false;
      for (let i = 0; i < count; i++) {
        let win = windows.GetElementAt(i);
        let hasAudio = {};
        let hasVideo = {};
        MediaManagerService.mediaCaptureWindowState(win, hasVideo, hasAudio);
        if (hasVideo.value) cameraActive = true;
        if (hasAudio.value) audioActive = true;
      }

      if (cameraActive && audioActive) {
        notificationOptions.message = Strings.browser.GetStringFromName("getUserMedia.sharingCameraAndMicrophone.message2");
        notificationOptions.icon = "drawable:alert_mic_camera";
      } else if (cameraActive) {
        notificationOptions.message = Strings.browser.GetStringFromName("getUserMedia.sharingCamera.message2");
        notificationOptions.icon = "drawable:alert_camera";
      } else if (audioActive) {
        notificationOptions.message = Strings.browser.GetStringFromName("getUserMedia.sharingMicrophone.message2");
        notificationOptions.icon = "drawable:alert_mic";
      } else {
        // somethings wrong. lets throw
        throw "Couldn't find any cameras or microphones being used"
      }

      if (this._notificationId)
          Notifications.update(this._notificationId, notificationOptions);
      else
        this._notificationId = Notifications.create(notificationOptions);
      if (count > 1)
        msg.count = count;
    }
  },

  handleRequest: function handleRequest(aSubject, aTopic, aData) {
    let constraints = aSubject.getConstraints();
    let contentWindow = Services.wm.getOuterWindowWithId(aSubject.windowID);

    contentWindow.navigator.mozGetUserMediaDevices(
      constraints,
      function (devices) {
        WebrtcUI.prompt(contentWindow, aSubject.callID, constraints.audio,
                        constraints.video, devices);
      },
      function (error) {
        Cu.reportError(error);
      },
      aSubject.innerWindowID);
  },

  getDeviceButtons: function(audioDevices, videoDevices, aCallID) {
    return [{
      label: Strings.browser.GetStringFromName("getUserMedia.denyRequest.label"),
      callback: function() {
        Services.obs.notifyObservers(null, "getUserMedia:response:deny", aCallID);
      }
    },
    {
      label: Strings.browser.GetStringFromName("getUserMedia.shareRequest.label"),
      callback: function(checked /* ignored */, inputs) {
        let allowedDevices = Cc["@mozilla.org/supports-array;1"].createInstance(Ci.nsISupportsArray);

        let audioId = 0;
        if (inputs && inputs.audioDevice != undefined)
          audioId = inputs.audioDevice;
        if (audioDevices[audioId])
          allowedDevices.AppendElement(audioDevices[audioId]);

        let videoId = 0;
        if (inputs && inputs.videoSource != undefined)
          videoId = inputs.videoSource;
        if (videoDevices[videoId])
          allowedDevices.AppendElement(videoDevices[videoId]);

        Services.obs.notifyObservers(allowedDevices, "getUserMedia:response:allow", aCallID);
      }
    }];
  },

  // Get a list of string names for devices. Ensures that none of the strings are blank
  _getList: function(aDevices, aType) {
    let defaultCount = 0;
    return aDevices.map(function(device) {
        // if this is a Camera input, convert the name to something readable
        let res = /Camera\ \d+,\ Facing (front|back)/.exec(device.name);
        if (res)
          return Strings.browser.GetStringFromName("getUserMedia." + aType + "." + res[1] + "Camera");

        if (device.name.startsWith("&") && device.name.endsWith(";"))
          return Strings.browser.GetStringFromName(device.name.substring(1, device.name.length -1));

        if (device.name.trim() == "") {
          defaultCount++;
          return Strings.browser.formatStringFromName("getUserMedia." + aType + ".default", [defaultCount], 1);
        }
        return device.name
      }, this);
  },

  _addDevicesToOptions: function(aDevices, aType, aOptions, extraOptions) {
    if (aDevices.length) {

      // Filter out empty items from the list
      let list = this._getList(aDevices, aType);
      if (extraOptions)
        list = list.concat(extraOptions);

      if (list.length > 0) {
        aOptions.inputs.push({
          id: aType,
          type: "menulist",
          label: Strings.browser.GetStringFromName("getUserMedia." + aType + ".prompt"),
          values: list
        });

      }
    }
  },

  prompt: function prompt(aContentWindow, aCallID, aAudioRequested,
                          aVideoRequested, aDevices) {
    let audioDevices = [];
    let videoDevices = [];
    for (let device of aDevices) {
      device = device.QueryInterface(Ci.nsIMediaDevice);
      switch (device.type) {
      case "audio":
        if (aAudioRequested)
          audioDevices.push(device);
        break;
      case "video":
        if (aVideoRequested)
          videoDevices.push(device);
        break;
      }
    }

    let requestType;
    if (audioDevices.length && videoDevices.length)
      requestType = "CameraAndMicrophone";
    else if (audioDevices.length)
      requestType = "Microphone";
    else if (videoDevices.length)
      requestType = "Camera";
    else
      return;

    let host = aContentWindow.document.documentURIObject.host;
    let requestor = BrowserApp.manifest ? "'" + BrowserApp.manifest.name  + "'" : host;
    let message = Strings.browser.formatStringFromName("getUserMedia.share" + requestType + ".message", [ requestor ], 1);

    let options = { inputs: [] };
    // if the users only option would be to select "No Audio" or "No Video"
    // i.e. we're only showing audio or only video and there is only one device for that type
    // don't bother showing a menulist to select from
    var extraItems = null;
    if (videoDevices.length > 1 || audioDevices.length > 0) {
      // Only show the No Video option if there are also Audio devices to choose from
      if (audioDevices.length > 0)
        extraItems = [ Strings.browser.GetStringFromName("getUserMedia.videoSource.none") ];
      // videoSource is both the string used for l10n lookup and the object that will be returned
      this._addDevicesToOptions(videoDevices, "videoSource", options, extraItems);
    }

    if (audioDevices.length > 1 || videoDevices.length > 0) {
      // Only show the No Audio option if there are also Video devices to choose from
      if (videoDevices.length > 0)
        extraItems = [ Strings.browser.GetStringFromName("getUserMedia.audioDevice.none") ];
      this._addDevicesToOptions(audioDevices, "audioDevice", options, extraItems);
    }

    let buttons = this.getDeviceButtons(audioDevices, videoDevices, aCallID);

    NativeWindow.doorhanger.show(message, "webrtc-request", buttons, BrowserApp.selectedTab.id, options);
  }
}
