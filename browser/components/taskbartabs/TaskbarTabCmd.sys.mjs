/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";
import { nsBrowserContentHandler } from "resource:///modules/BrowserContentHandler.sys.mjs";
/**
 * A command line handler for Firefox shortcuts with the flag "-taskbar-tab",
 * which will trigger a web app window to be opened
 *
 */
export class CommandLineHandler {
  static classID = Components.ID("{974fe39b-4584-4cb5-bf62-5c141aedc557}");
  static contractID = "@mozilla.org/browser/taskbar-tabs-clh;1";

  QueryInterface = ChromeUtils.generateQI([Ci.nsICommandLineHandler]);

  handle(cmdLine) {
    let taskbartabUrl;
    try {
      taskbartabUrl = cmdLine.handleFlagWithParam("taskbar-tab", false);
    } catch (e) {
      console.error(e);
    }

    if (taskbartabUrl) {
      let args = Cc["@mozilla.org/array;1"].createInstance(Ci.nsIMutableArray);
      let url = Cc["@mozilla.org/supports-string;1"].createInstance(
        Ci.nsISupportsString
      );
      url.data = taskbartabUrl;

      let extraOptions = Cc["@mozilla.org/hash-property-bag;1"].createInstance(
        Ci.nsIWritablePropertyBag2
      );
      extraOptions.setPropertyAsBool("taskbartab", true);

      args.appendElement(url);
      args.appendElement(extraOptions);
      args.appendElement(null);
      args.appendElement(null);
      args.appendElement(undefined);
      args.appendElement(undefined);
      args.appendElement(null);
      args.appendElement(null);
      args.appendElement(Services.scriptSecurityManager.getSystemPrincipal());

      const isStartup = cmdLine.state == Ci.nsICommandLine.STATE_INITIAL_LAUNCH;
      if (isStartup) {
        let contentHandler = new nsBrowserContentHandler();
        contentHandler.replaceStartupWindow(args, false);
      }
      cmdLine.preventDefault = true;
      Services.ww.openWindow(
        null,
        AppConstants.BROWSER_CHROME_URL,
        "_blank",
        "chrome,dialog=no,titlebar,close,toolbar,location,personalbar=no,status,menubar=no,resizable,minimizable,scrollbars",
        args
      );
    }
  }
}
