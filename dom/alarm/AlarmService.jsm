/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* static functions */
const DEBUG = false;

function debug(aStr) {
  DEBUG && dump("AlarmService: " + aStr + "\n");
}

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/AlarmDB.jsm");

this.EXPORTED_SYMBOLS = ["AlarmService"];

XPCOMUtils.defineLazyGetter(this, "appsService", function() {
  return Cc["@mozilla.org/AppsService;1"].getService(Ci.nsIAppsService);
});

XPCOMUtils.defineLazyServiceGetter(this, "ppmm",
                                   "@mozilla.org/parentprocessmessagemanager;1",
                                   "nsIMessageListenerManager");

XPCOMUtils.defineLazyGetter(this, "messenger", function() {
  return Cc["@mozilla.org/system-message-internal;1"]
           .getService(Ci.nsISystemMessagesInternal);
});

XPCOMUtils.defineLazyGetter(this, "powerManagerService", function() {
  return Cc["@mozilla.org/power/powermanagerservice;1"]
           .getService(Ci.nsIPowerManagerService);
});

/**
 * AlarmService provides an API to schedule alarms using the device's RTC.
 *
 * AlarmService is primarily used by the mozAlarms API (navigator.mozAlarms)
 * which uses IPC to communicate with the service.
 *
 * AlarmService can also be used by Gecko code by importing the module and then
 * using AlarmService.add() and AlarmService.remove(). Only Gecko code running
 * in the parent process should do this.
 */

this.AlarmService = {
  init: function init() {
    debug("init()");

    Services.obs.addObserver(this, "profile-change-teardown", false);
    Services.obs.addObserver(this, "webapps-clear-data",false);

    this._currentTimezoneOffset = (new Date()).getTimezoneOffset();

    let alarmHalService = this._alarmHalService =
      Cc["@mozilla.org/alarmHalService;1"].getService(Ci.nsIAlarmHalService);

    alarmHalService.setAlarmFiredCb(this._onAlarmFired.bind(this));
    alarmHalService.setTimezoneChangedCb(this._onTimezoneChanged.bind(this));
    alarmHalService.setSystemClockChangedCb(
      this._onSystemClockChanged.bind(this));

    // Add the messages to be listened to.
    this._messages = ["AlarmsManager:GetAll",
                      "AlarmsManager:Add",
                      "AlarmsManager:Remove"];
    this._messages.forEach(function addMessage(msgName) {
      ppmm.addMessageListener(msgName, this);
    }.bind(this));

    // Set the indexeddb database.
    this._db = new AlarmDB();
    this._db.init();

    // Variable to save alarms waiting to be set.
    this._alarmQueue = [];

    this._restoreAlarmsFromDb();
  },

  // Getter/setter to access the current alarm set in system.
  _alarm: null,
  get _currentAlarm() {
    return this._alarm;
  },
  set _currentAlarm(aAlarm) {
    this._alarm = aAlarm;
    if (!aAlarm) {
      return;
    }

    let alarmTimeInMs = this._getAlarmTime(aAlarm);
    let ns = (alarmTimeInMs % 1000) * 1000000;
    if (!this._alarmHalService.setAlarm(alarmTimeInMs / 1000, ns)) {
      throw Components.results.NS_ERROR_FAILURE;
    }
  },

  receiveMessage: function receiveMessage(aMessage) {
    debug("receiveMessage(): " + aMessage.name);
    let json = aMessage.json;

    // To prevent the hacked child process from sending commands to parent
    // to schedule alarms, we need to check its permission and manifest URL.
    if (this._messages.indexOf(aMessage.name) != -1) {
      if (!aMessage.target.assertPermission("alarms")) {
        debug("Got message from a child process with no 'alarms' permission.");
        return null;
      }

      if (!aMessage.target.assertContainApp(json.manifestURL)) {
        debug("Got message from a child process containing illegal manifest URL.");
        return null;
      }
    }

    let mm = aMessage.target.QueryInterface(Ci.nsIMessageSender);

    switch (aMessage.name) {
      case "AlarmsManager:GetAll":
        this._db.getAll(json.manifestURL,
          function getAllSuccessCb(aAlarms) {
            debug("Callback after getting alarms from database: " +
                  JSON.stringify(aAlarms));

            this._sendAsyncMessage(mm, "GetAll", true, json.requestId, aAlarms);
          }.bind(this),
          function getAllErrorCb(aErrorMsg) {
            this._sendAsyncMessage(mm, "GetAll", false, json.requestId, aErrorMsg);
          }.bind(this));
        break;

      case "AlarmsManager:Add":
        // Prepare a record for the new alarm to be added.
        let newAlarm = { date: json.date,
                         ignoreTimezone: json.ignoreTimezone,
                         data: json.data,
                         pageURL: json.pageURL,
                         manifestURL: json.manifestURL };

        this.add(newAlarm, null,
          // Receives the alarm ID as the last argument.
          this._sendAsyncMessage.bind(this, mm, "Add", true, json.requestId),
          // Receives the error message as the last argument.
          this._sendAsyncMessage.bind(this, mm, "Add", false, json.requestId));
        break;

      case "AlarmsManager:Remove":
        this.remove(json.id, json.manifestURL);
        break;

      default:
        throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
        break;
    }
  },

  _sendAsyncMessage: function _sendAsyncMessage(aMessageManager, aMessageName,
                                                aSuccess, aRequestId, aData) {
    debug("_sendAsyncMessage()");

    if (!aMessageManager) {
      debug("Invalid message manager: null");
      throw Components.results.NS_ERROR_FAILURE;
    }

    let json = null;
    switch (aMessageName) {
      case "Add":
        json = aSuccess ?
          { requestId: aRequestId, id: aData } :
          { requestId: aRequestId, errorMsg: aData };
        break;

      case "GetAll":
        json = aSuccess ?
          { requestId: aRequestId, alarms: aData } :
          { requestId: aRequestId, errorMsg: aData };
        break;

      default:
        throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
        break;
    }

    aMessageManager.sendAsyncMessage("AlarmsManager:" + aMessageName +
                                       ":Return:" + (aSuccess ? "OK" : "KO"),
                                     json);
  },

  _removeAlarmFromDb: function _removeAlarmFromDb(aId, aManifestURL,
                                                  aRemoveSuccessCb) {
    debug("_removeAlarmFromDb()");

    // If the aRemoveSuccessCb is undefined or null, set a dummy callback for
    // it which is needed for _db.remove().
    if (!aRemoveSuccessCb) {
      aRemoveSuccessCb = function removeSuccessCb() {
        debug("Remove alarm from DB successfully.");
      };
    }

    this._db.remove(aId, aManifestURL, aRemoveSuccessCb,
                    function removeErrorCb(aErrorMsg) {
                      throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
                    });
  },

  /**
   * Create a copy of the alarm that does not expose internal fields to
   * receivers and sticks to the public |respectTimezone| API rather than the
   * boolean |ignoreTimezone| field.
   */
  _publicAlarm: function _publicAlarm(aAlarm) {
    let alarm = { "id": aAlarm.id,
                  "date": aAlarm.date,
                  "respectTimezone": aAlarm.ignoreTimezone ?
                                       "ignoreTimezone" : "honorTimezone",
                  "data": aAlarm.data };

    return alarm;
  },

  _fireSystemMessage: function _fireSystemMessage(aAlarm) {
    debug("Fire system message: " + JSON.stringify(aAlarm));

    let manifestURI = Services.io.newURI(aAlarm.manifestURL, null, null);
    let pageURI = Services.io.newURI(aAlarm.pageURL, null, null);

    messenger.sendMessage("alarm",
                          this._publicAlarm(aAlarm),
                          pageURI,
                          manifestURI);
  },

  _notifyAlarmObserver: function _notifyAlarmObserver(aAlarm) {
    debug("_notifyAlarmObserver()");

    if (aAlarm.manifestURL) {
      this._fireSystemMessage(aAlarm);
    } else if (typeof aAlarm.alarmFiredCb === "function") {
      aAlarm.alarmFiredCb(this._publicAlarm(aAlarm));
    }
  },

  _onAlarmFired: function _onAlarmFired() {
    debug("_onAlarmFired()");

    if (this._currentAlarm) {
      this._removeAlarmFromDb(this._currentAlarm.id, null);
      this._notifyAlarmObserver(this._currentAlarm);
      this._currentAlarm = null;
    }

    // Reset the next alarm from the queue.
    let alarmQueue = this._alarmQueue;
    while (alarmQueue.length > 0) {
      let nextAlarm = alarmQueue.shift();
      let nextAlarmTime = this._getAlarmTime(nextAlarm);

      // If the next alarm has been expired, directly notify the observer.
      // it instead of setting it.
      if (nextAlarmTime <= Date.now()) {
        this._removeAlarmFromDb(nextAlarm.id, null);
        this._notifyAlarmObserver(nextAlarm);
      } else {
        this._currentAlarm = nextAlarm;
        break;
      }
    }

    this._debugCurrentAlarm();
  },

  _onTimezoneChanged: function _onTimezoneChanged(aTimezoneOffset) {
    debug("_onTimezoneChanged()");

    this._currentTimezoneOffset = aTimezoneOffset;
    this._restoreAlarmsFromDb();
  },

  _onSystemClockChanged: function _onSystemClockChanged(aClockDeltaMS) {
    debug("_onSystemClockChanged");
    this._restoreAlarmsFromDb();
  },

  _restoreAlarmsFromDb: function _restoreAlarmsFromDb() {
    debug("_restoreAlarmsFromDb()");

    this._db.getAll(null,
      function getAllSuccessCb(aAlarms) {
        debug("Callback after getting alarms from database: " +
              JSON.stringify(aAlarms));

        // Clear any alarms set or queued in the cache.
        let alarmQueue = this._alarmQueue;
        alarmQueue.length = 0;
        this._currentAlarm = null;

        // Only restore the alarm that's not yet expired; otherwise, remove it
        // from the database and notify the observer.
        aAlarms.forEach(function addAlarm(aAlarm) {
          if (this._getAlarmTime(aAlarm) > Date.now()) {
            alarmQueue.push(aAlarm);
          } else {
            this._removeAlarmFromDb(aAlarm.id, null);
            this._notifyAlarmObserver(aAlarm);
          }
        }.bind(this));

        // Set the next alarm from the queue.
        if (alarmQueue.length) {
          alarmQueue.sort(this._sortAlarmByTimeStamps.bind(this));
          this._currentAlarm = alarmQueue.shift();
        }

        this._debugCurrentAlarm();
      }.bind(this),
      function getAllErrorCb(aErrorMsg) {
        throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
      });
  },

  _getAlarmTime: function _getAlarmTime(aAlarm) {
    // Avoid casting a Date object to a Date again to
    // preserve milliseconds. See bug 810973.
    let alarmTime;
    if (aAlarm.date instanceof Date) {
      alarmTime = aAlarm.date.getTime();
    } else {
      alarmTime = (new Date(aAlarm.date)).getTime();
    }

    // For an alarm specified with "ignoreTimezone", it must be fired respect
    // to the user's timezone.  Supposing an alarm was set at 7:00pm at Tokyo,
    // it must be gone off at 7:00pm respect to Paris' local time when the user
    // is located at Paris.  We can adjust the alarm UTC time by calculating
    // the difference of the orginal timezone and the current timezone.
    if (aAlarm.ignoreTimezone) {
      alarmTime +=
        (this._currentTimezoneOffset - aAlarm.timezoneOffset) * 60000;
    }
    return alarmTime;
  },

  _sortAlarmByTimeStamps: function _sortAlarmByTimeStamps(aAlarm1, aAlarm2) {
    return this._getAlarmTime(aAlarm1) - this._getAlarmTime(aAlarm2);
  },

  _debugCurrentAlarm: function _debugCurrentAlarm() {
    debug("Current alarm: " + JSON.stringify(this._currentAlarm));
    debug("Alarm queue: " + JSON.stringify(this._alarmQueue));
  },

  /**
   *
   * Add a new alarm. This will set the RTC to fire at the selected date and
   * notify the caller. Notifications are delivered via System Messages if the
   * alarm is added on behalf of a app. Otherwise aAlarmFiredCb is called.
   *
   * @param object aNewAlarm
   *        Should contain the following literal properties:
   *          - |date| date: when the alarm should timeout.
   *          - |ignoreTimezone| boolean: See [1] for the details.
   *          - |manifestURL| string: Manifest of app on whose behalf the alarm
   *                                  is added.
   *          - |pageURL| string: The page in the app that receives the system
   *                              message.
   *          - |data| object [optional]: Data that can be stored in DB.
   * @param function aAlarmFiredCb
   *        Callback function invoked when the alarm is fired.
   *        It receives a single argument, the alarm object.
   *        May be null.
   * @param function aSuccessCb
   *        Callback function to receive an alarm ID (number).
   * @param function aErrorCb
   *        Callback function to receive an error message (string).
   * @returns void
   *
   * Notes:
   * [1] https://wiki.mozilla.org/WebAPI/AlarmAPI#Proposed_API
   */

  add: function(aNewAlarm, aAlarmFiredCb, aSuccessCb, aErrorCb) {
    debug("add(" + aNewAlarm.date + ")");

    aSuccessCb = aSuccessCb || function() {};
    aErrorCb = aErrorCb || function() {};

    if (!aNewAlarm) {
      aErrorCb("alarm is null");
      return;
    }

    if (!aNewAlarm.date) {
      aErrorCb("alarm.date is null");
      return;
    }

    aNewAlarm['timezoneOffset'] = this._currentTimezoneOffset;

    this._db.add(aNewAlarm,
      function addSuccessCb(aNewId) {
        debug("Callback after adding alarm in database.");

        aNewAlarm['id'] = aNewId;

        // Now that the alarm has been added to the database, we can tack on
        // the non-serializable callback to the in-memory object.
        aNewAlarm['alarmFiredCb'] = aAlarmFiredCb;

        // If there is no alarm being set in system, set the new alarm.
        if (this._currentAlarm == null) {
          this._currentAlarm = aNewAlarm;
          this._debugCurrentAlarm();
          aSuccessCb(aNewId);
          return;
        }

        // If the new alarm is earlier than the current alarm, swap them and
        // push the previous alarm back to the queue.
        let alarmQueue = this._alarmQueue;
        let aNewAlarmTime = this._getAlarmTime(aNewAlarm);
        let currentAlarmTime = this._getAlarmTime(this._currentAlarm);
        if (aNewAlarmTime < currentAlarmTime) {
          alarmQueue.unshift(this._currentAlarm);
          this._currentAlarm = aNewAlarm;
          this._debugCurrentAlarm();
          aSuccessCb(aNewId);
          return;
        }

        // Push the new alarm in the queue.
        alarmQueue.push(aNewAlarm);
        alarmQueue.sort(this._sortAlarmByTimeStamps.bind(this));
        this._debugCurrentAlarm();
        aSuccessCb(aNewId);
      }.bind(this),
      function addErrorCb(aErrorMsg) {
        aErrorCb(aErrorMsg);
      }.bind(this));
  },

  /*
   * Remove the alarm associated with an ID.
   *
   * @param number aAlarmId
   *        The ID of the alarm to be removed.
   * @param string aManifestURL
   *        Manifest URL for application which added the alarm. (Optional)
   * @returns void
   */
  remove: function(aAlarmId, aManifestURL) {
    debug("remove(" + aAlarmId + ", " + aManifestURL + ")");

    this._removeAlarmFromDb(aAlarmId, aManifestURL,
      function removeSuccessCb() {
        debug("Callback after removing alarm from database.");

        // If there are no alarms set, nothing to do.
        if (!this._currentAlarm) {
          debug("No alarms set.");
          return;
        }

        // Check if the alarm to be removed is in the queue and whether it
        // belongs to the requesting app.
        let alarmQueue = this._alarmQueue;
        if (this._currentAlarm.id != aAlarmId ||
            this._currentAlarm.manifestURL != aManifestURL) {

          for (let i = 0; i < alarmQueue.length; i++) {
            if (alarmQueue[i].id == aAlarmId &&
                alarmQueue[i].manifestURL == aManifestURL) {

              alarmQueue.splice(i, 1);
              break;
            }
          }
          this._debugCurrentAlarm();
          return;
        }

        // The alarm to be removed is the current alarm reset the next alarm
        // from the queue if any.
        if (alarmQueue.length) {
          this._currentAlarm = alarmQueue.shift();
          this._debugCurrentAlarm();
          return;
        }

        // No alarm waiting to be set in the queue.
        this._currentAlarm = null;
        this._debugCurrentAlarm();
      }.bind(this));
  },

  observe: function(aSubject, aTopic, aData) {
    debug("observe(): " + aTopic);

    switch (aTopic) {
      case "profile-change-teardown":
        this.uninit();
        break;

      case "webapps-clear-data":
        let params =
          aSubject.QueryInterface(Ci.mozIApplicationClearPrivateDataParams);
        if (!params) {
          debug("Error! Fail to remove alarms for an uninstalled app.");
          return;
        }

        // Only remove alarms for apps.
        if (params.browserOnly) {
          return;
        }

        let manifestURL = appsService.getManifestURLByLocalId(params.appId);
        if (!manifestURL) {
          debug("Error! Fail to remove alarms for an uninstalled app.");
          return;
        }

        this._db.getAll(manifestURL,
          function getAllSuccessCb(aAlarms) {
            aAlarms.forEach(function removeAlarm(aAlarm) {
              this.remove(aAlarm.id, manifestURL);
            }, this);
          }.bind(this),
          function getAllErrorCb(aErrorMsg) {
            throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
          });
        break;
    }
  },

  uninit: function uninit() {
    debug("uninit()");

    Services.obs.removeObserver(this, "profile-change-teardown");
    Services.obs.removeObserver(this, "webapps-clear-data");

    this._messages.forEach(function(aMsgName) {
      ppmm.removeMessageListener(aMsgName, this);
    }.bind(this));
    ppmm = null;

    if (this._db) {
      this._db.close();
    }
    this._db = null;

    this._alarmHalService = null;
  }
}

AlarmService.init();
