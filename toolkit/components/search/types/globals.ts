/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Exports for all modules redirected here by a catch-all rule in tsconfig.json.
// We define them here, rather than let them be imported directly as they haven't
// been updated to work fully with TypeScript yet.
export var RemoteSettingsClient;
export var FfiConverterTypeRemoteSettingsService;
export var RemoteSettingsService;

declare global {
  // We use `Extension` from the extensions code, but importing that requires
  // a lot of set-up, and so we skip it for now.
  type Extension = any;

  // The WebExtensionPolicy that is returned from WebExtensionPolicy.getByID()
  // has a slightly different interface to how the webidl defines it, so we define
  // that here.
  interface WebExtensionPolicy {
    extension: Extension;
    debugName: string;
    instanceId: string;
    optionalPermissions: string[];
  }
}
