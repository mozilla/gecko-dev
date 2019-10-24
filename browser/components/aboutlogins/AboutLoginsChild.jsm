/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["AboutLoginsChild"];

const { ActorChild } = ChromeUtils.import(
  "resource://gre/modules/ActorChild.jsm"
);
const { LoginHelper } = ChromeUtils.import(
  "resource://gre/modules/LoginHelper.jsm"
);
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

ChromeUtils.defineModuleGetter(
  this,
  "AppConstants",
  "resource://gre/modules/AppConstants.jsm"
);

XPCOMUtils.defineLazyServiceGetter(
  this,
  "ClipboardHelper",
  "@mozilla.org/widget/clipboardhelper;1",
  "nsIClipboardHelper"
);

const TELEMETRY_EVENT_CATEGORY = "pwmgr";
const TELEMETRY_MIN_MS_BETWEEN_OPEN_MANAGEMENT = 5000;

let lastOpenManagementOuterWindowID = null;
let lastOpenManagementEventTime = Number.NEGATIVE_INFINITY;
let masterPasswordPromise;

class AboutLoginsChild extends ActorChild {
  handleEvent(event) {
    switch (event.type) {
      case "AboutLoginsInit": {
        let messageManager = this.mm;
        messageManager.sendAsyncMessage("AboutLogins:Subscribe");

        let documentElement = this.content.document.documentElement;
        documentElement.classList.toggle(
          "official-branding",
          AppConstants.MOZILLA_OFFICIAL
        );

        let waivedContent = Cu.waiveXrays(this.content);
        let AboutLoginsUtils = {
          doLoginsMatch(loginA, loginB) {
            return LoginHelper.doLoginsMatch(loginA, loginB, {});
          },
          getLoginOrigin(uriString) {
            return LoginHelper.getLoginOrigin(uriString);
          },
          promptForMasterPassword(resolve) {
            masterPasswordPromise = {
              resolve,
            };

            messageManager.sendAsyncMessage(
              "AboutLogins:MasterPasswordRequest"
            );
          },
          // Default to enabled just in case a search is attempted before we get a response.
          masterPasswordEnabled: true,
          passwordRevealVisible: true,
        };
        waivedContent.AboutLoginsUtils = Cu.cloneInto(
          AboutLoginsUtils,
          waivedContent,
          {
            cloneFunctions: true,
          }
        );

        const SUPPORT_URL =
          Services.urlFormatter.formatURLPref("app.support.baseURL") +
          "firefox-lockwise";
        let loginIntro = Cu.waiveXrays(
          this.content.document.querySelector("login-intro")
        );
        loginIntro.supportURL = SUPPORT_URL;
        break;
      }
      case "AboutLoginsCopyLoginDetail": {
        ClipboardHelper.copyString(event.detail);
        break;
      }
      case "AboutLoginsCreateLogin": {
        this.mm.sendAsyncMessage("AboutLogins:CreateLogin", {
          login: event.detail,
        });
        break;
      }
      case "AboutLoginsDeleteLogin": {
        this.mm.sendAsyncMessage("AboutLogins:DeleteLogin", {
          login: event.detail,
        });
        break;
      }
      case "AboutLoginsDismissBreachAlert": {
        this.mm.sendAsyncMessage("AboutLogins:DismissBreachAlert", {
          login: event.detail,
        });
        break;
      }
      case "AboutLoginsGetHelp": {
        this.mm.sendAsyncMessage("AboutLogins:GetHelp");
        break;
      }
      case "AboutLoginsHideFooter": {
        this.mm.sendAsyncMessage("AboutLogins:HideFooter");
        break;
      }
      case "AboutLoginsImport": {
        this.mm.sendAsyncMessage("AboutLogins:Import");
        break;
      }
      case "AboutLoginsOpenMobileAndroid": {
        this.mm.sendAsyncMessage("AboutLogins:OpenMobileAndroid", {
          source: event.detail,
        });
        break;
      }
      case "AboutLoginsOpenMobileIos": {
        this.mm.sendAsyncMessage("AboutLogins:OpenMobileIos", {
          source: event.detail,
        });
        break;
      }
      case "AboutLoginsOpenPreferences": {
        this.mm.sendAsyncMessage("AboutLogins:OpenPreferences");
        break;
      }
      case "AboutLoginsOpenSite": {
        this.mm.sendAsyncMessage("AboutLogins:OpenSite", {
          login: event.detail,
        });
        break;
      }
      case "AboutLoginsRecordTelemetryEvent": {
        let { method, object, extra = {} } = event.detail;

        if (method == "open_management") {
          let { docShell } = event.target.ownerGlobal;
          // Compare to the last time open_management was recorded for the same
          // outerWindowID to not double-count them due to a redirect to remove
          // the entryPoint query param (since replaceState isn't allowed for
          // about:). Don't use performance.now for the tab since you can't
          // compare that number between different tabs and this JSM is shared.
          let now = docShell.now();
          if (
            docShell.outerWindowID == lastOpenManagementOuterWindowID &&
            now - lastOpenManagementEventTime <
              TELEMETRY_MIN_MS_BETWEEN_OPEN_MANAGEMENT
          ) {
            return;
          }
          lastOpenManagementEventTime = now;
          lastOpenManagementOuterWindowID = docShell.outerWindowID;
        }

        try {
          Services.telemetry.recordEvent(
            TELEMETRY_EVENT_CATEGORY,
            method,
            object,
            null,
            extra
          );
        } catch (ex) {
          Cu.reportError(
            "AboutLoginsChild: error recording telemetry event: " + ex.message
          );
        }
        break;
      }
      case "AboutLoginsSortChanged": {
        this.mm.sendAsyncMessage("AboutLogins:SortChanged", event.detail);
        break;
      }
      case "AboutLoginsSyncEnable": {
        this.mm.sendAsyncMessage("AboutLogins:SyncEnable");
        break;
      }
      case "AboutLoginsSyncOptions": {
        this.mm.sendAsyncMessage("AboutLogins:SyncOptions");
        break;
      }
      case "AboutLoginsUpdateLogin": {
        this.mm.sendAsyncMessage("AboutLogins:UpdateLogin", {
          login: event.detail,
        });
        break;
      }
    }
  }

  receiveMessage(message) {
    switch (message.name) {
      case "AboutLogins:AllLogins":
        this.sendToContent("AllLogins", message.data);
        break;
      case "AboutLogins:LoginAdded":
        this.sendToContent("LoginAdded", message.data);
        break;
      case "AboutLogins:LoginModified":
        this.sendToContent("LoginModified", message.data);
        break;
      case "AboutLogins:LoginRemoved":
        this.sendToContent("LoginRemoved", message.data);
        break;
      case "AboutLogins:MasterPasswordAuthRequired":
        this.sendToContent("MasterPasswordAuthRequired", message.data);
        break;
      case "AboutLogins:MasterPasswordResponse":
        if (masterPasswordPromise) {
          masterPasswordPromise.resolve(message.data);
        }
        break;
      case "AboutLogins:SendFavicons":
        this.sendToContent("SendFavicons", message.data);
        break;
      case "AboutLogins:SetBreaches":
        this.sendToContent("SetBreaches", message.data);
        break;
      case "AboutLogins:Setup":
        this.sendToContent("Setup", message.data);
        Cu.waiveXrays(this.content).AboutLoginsUtils.masterPasswordEnabled =
          message.data.masterPasswordEnabled;
        Cu.waiveXrays(this.content).AboutLoginsUtils.passwordRevealVisible =
          message.data.passwordRevealVisible;
        break;
      case "AboutLogins:ShowLoginItemError":
        this.sendToContent("ShowLoginItemError", message.data);
        break;
      case "AboutLogins:SyncState":
        this.sendToContent("SyncState", message.data);
        break;
      case "AboutLogins:UpdateBreaches":
        this.sendToContent("UpdateBreaches", message.data);
        break;
    }
  }

  sendToContent(messageType, detail) {
    let message = Object.assign({ messageType }, { value: detail });
    let event = new this.content.CustomEvent("AboutLoginsChromeToContent", {
      detail: Cu.cloneInto(message, this.content),
    });
    this.content.dispatchEvent(event);
  }
}
