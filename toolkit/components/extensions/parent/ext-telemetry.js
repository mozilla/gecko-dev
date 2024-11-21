/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  TelemetryController: "resource://gre/modules/TelemetryController.sys.mjs",
  TelemetryUtils: "resource://gre/modules/TelemetryUtils.sys.mjs",
});

// Currently unsupported on Android: blocked on 1220177.
// See 1280234 c67 for discussion.
function desktopCheck() {
  if (AppConstants.MOZ_BUILD_APP !== "browser") {
    throw new ExtensionUtils.ExtensionError(
      "This API is only supported on desktop"
    );
  }
}

this.telemetry = class extends ExtensionAPI {
  getAPI(_context) {
    return {
      telemetry: {
        submitPing(type, payload, options) {
          desktopCheck();
          try {
            TelemetryController.submitExternalPing(type, payload, options);
          } catch (ex) {
            throw new ExtensionUtils.ExtensionError(ex);
          }
        },
        canUpload() {
          desktopCheck();
          // Note: remove the ternary and direct pref check when
          // TelemetryController.canUpload() is implemented (bug 1440089).
          try {
            const result =
              "canUpload" in TelemetryController
                ? TelemetryController.canUpload()
                : Services.prefs.getBoolPref(
                    TelemetryUtils.Preferences.FhrUploadEnabled,
                    false
                  );
            return result;
          } catch (ex) {
            throw new ExtensionUtils.ExtensionError(ex);
          }
        },
        scalarAdd(_name, _value) {
          desktopCheck();
          // No-op since bug 1930196 (Fx134).
        },
        scalarSet(_name, _value) {
          desktopCheck();
          // No-op since bug 1930196 (Fx134).
        },
        scalarSetMaximum(_name, _value) {
          desktopCheck();
          // No-op since bug 1930196 (Fx134).
        },
        keyedScalarAdd(_name, _key, _value) {
          desktopCheck();
          // No-op since bug 1930196 (Fx134).
        },
        keyedScalarSet(_name, _key, _value) {
          desktopCheck();
          // No-op since bug 1930196 (Fx134).
        },
        keyedScalarSetMaximum(_name, _key, _value) {
          desktopCheck();
          // No-op since bug 1930196 (Fx134).
        },
        recordEvent(_category, _method, _object, _value, _extra) {
          desktopCheck();
          // No-op since bug 1894533 (Fx132).
        },
        registerScalars(_category, _data) {
          desktopCheck();
          // No-op since bug 1930196 (Fx134).
        },
        setEventRecordingEnabled(_category, _enabled) {
          desktopCheck();
          // No-op since bug 1920562 (Fx133).
        },
        registerEvents(_category, _data) {
          desktopCheck();
          // No-op since bug 1894533 (Fx132).
        },
      },
    };
  }
};
