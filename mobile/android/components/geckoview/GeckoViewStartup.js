/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/GeckoViewUtils.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  ActorManagerParent: "resource://gre/modules/ActorManagerParent.jsm",
  EventDispatcher: "resource://gre/modules/Messaging.jsm",
  FileSource: "resource://gre/modules/L10nRegistry.jsm",
  GeckoViewTelemetryController: "resource://gre/modules/GeckoViewTelemetryController.jsm",
  L10nRegistry: "resource://gre/modules/L10nRegistry.jsm",
  Preferences: "resource://gre/modules/Preferences.jsm",
  Services: "resource://gre/modules/Services.jsm",
});

/* global debug:false, warn:false */
GeckoViewUtils.initLogging("Startup", this);

function GeckoViewStartup() {
}

GeckoViewStartup.prototype = {
  classID: Components.ID("{8e993c34-fdd6-432c-967e-f995d888777f}"),

  QueryInterface: ChromeUtils.generateQI([Ci.nsIObserver]),

  /**
   * Register resource://android as the APK root.
   *
   * Consumers can access Android assets using resource://android/assets/FILENAME.
   */
  setResourceSubstitutions: function() {
    let registry = Cc["@mozilla.org/chrome/chrome-registry;1"].getService(Ci.nsIChromeRegistry);
    // Like jar:jar:file:///data/app/org.mozilla.geckoview.test.apk!/assets/omni.ja!/chrome/geckoview/content/geckoview.js
    let url = registry.convertChromeURL(Services.io.newURI("chrome://geckoview/content/geckoview.js")).spec;
    // Like jar:file:///data/app/org.mozilla.geckoview.test.apk!/
    url = url.substring(4, url.indexOf("!/") + 2);

    let protocolHandler = Services.io.getProtocolHandler("resource").QueryInterface(Ci.nsIResProtocolHandler);
    protocolHandler.setSubstitution("android", Services.io.newURI(url));
  },

  /* ----------  nsIObserver  ---------- */
  observe: function(aSubject, aTopic, aData) {
    debug `observe: ${aTopic}`;
    switch (aTopic) {
      case "app-startup": {
        // Parent and content process.
        GeckoViewUtils.addLazyGetter(this, "GeckoViewPermission", {
          service: "@mozilla.org/content-permission/prompt;1",
          observers: [
            "getUserMedia:ask-device-permission",
            "getUserMedia:request",
            "PeerConnection:request",
          ],
          ppmm: [
            "GeckoView:AddCameraPermission",
          ],
        });

        GeckoViewUtils.addLazyGetter(this, "GeckoViewConsole", {
          module: "resource://gre/modules/GeckoViewConsole.jsm",
        });

        GeckoViewUtils.addLazyPrefObserver({
          name: "geckoview.console.enabled",
          default: false,
        }, {
          handler: _ => this.GeckoViewConsole,
        });

        if (Services.appinfo.processType == Services.appinfo.PROCESS_TYPE_DEFAULT) {
          ActorManagerParent.flush();

          // Parent process only.
          this.setResourceSubstitutions();

          Services.mm.loadFrameScript(
              "chrome://geckoview/content/GeckoViewPromptChild.js", true);

          GeckoViewUtils.addLazyGetter(this, "ContentCrashHandler", {
            module: "resource://gre/modules/ContentCrashHandler.jsm",
            observers: [
              "ipc:content-shutdown",
            ],
          });
        }
        break;
      }

      case "profile-after-change": {
        // Parent process only.
        // ContentPrefServiceParent is needed for e10s file picker.
        GeckoViewUtils.addLazyGetter(this, "ContentPrefServiceParent", {
          module: "resource://gre/modules/ContentPrefServiceParent.jsm",
          init: cpsp => cpsp.alwaysInit(),
          ppmm: [
            "ContentPrefs:FunctionCall",
            "ContentPrefs:AddObserverForName",
            "ContentPrefs:RemoveObserverForName",
          ],
        });

        GeckoViewUtils.addLazyGetter(this, "GeckoViewRemoteDebugger", {
          module: "resource://gre/modules/GeckoViewRemoteDebugger.jsm",
          init: gvrd => gvrd.onInit(),
        });

        GeckoViewUtils.addLazyPrefObserver({
          name: "devtools.debugger.remote-enabled",
          default: false,
        }, {
          handler: _ => this.GeckoViewRemoteDebugger,
        });

        // This initializes Telemetry for GeckoView only in the parent process.
        // The Telemetry initialization for the content process is performed in
        // ContentProcessSingleton.js for consistency with Desktop Telemetry.
        GeckoViewTelemetryController.setup();

        // Initialize the default l10n resource sources for L10nRegistry.
        let locales = Services.locale.packagedLocales;
        const greSource = new FileSource("toolkit", locales, "resource://gre/localization/{locale}/");
        L10nRegistry.registerSource(greSource);

        // Listen for global EventDispatcher messages
        EventDispatcher.instance.registerListener(this,
          ["GeckoView:ResetUserPrefs",
           "GeckoView:SetDefaultPrefs",
           "GeckoView:SetLocale"]);
        break;
      }
    }
  },

  onEvent(aEvent, aData, aCallback) {
    debug `onEvent ${aEvent}`;

    switch (aEvent) {
      case "GeckoView:ResetUserPrefs": {
        const prefs = new Preferences();
        prefs.reset(aData.names);
        break;
      }
      case "GeckoView:SetDefaultPrefs": {
        const prefs = new Preferences({ defaultBranch: true });
        for (const name of Object.keys(aData)) {
          try {
            prefs.set(name, aData[name]);
          } catch (e) {
            warn `Failed to set preference ${name}: ${e}`;
          }
        }
        break;
      }
      case "GeckoView:SetLocale":
        Services.locale.requestedLocales = aData.requestedLocales;
        break;
    }
  },
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([GeckoViewStartup]);
