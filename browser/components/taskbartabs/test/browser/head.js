/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Creates a web app window with the given tab,
 * then returns the window object for testing.
 *
 * @param {Tab} tab
 *        The tab that the web app should open with,
 *        about:blank will be opened if this value is null.
 * @returns {Promise}
 *        The web app window object.
 */
async function openTaskbarTabWindow(tab = null) {
  let extraOptions = Cc["@mozilla.org/hash-property-bag;1"].createInstance(
    Ci.nsIWritablePropertyBag2
  );
  extraOptions.setPropertyAsBool("taskbartab", true);

  let args = Cc["@mozilla.org/array;1"].createInstance(Ci.nsIMutableArray);

  args.appendElement(tab);
  args.appendElement(extraOptions);
  args.appendElement(null);

  // Simulate opening a taskbar tab window
  let win = Services.ww.openWindow(
    null,
    AppConstants.BROWSER_CHROME_URL,
    "_blank",
    "chrome,dialog=no,titlebar,close,toolbar,location,personalbar=no,status,menubar=no,resizable,minimizable,scrollbars",
    args
  );

  await new Promise(resolve => {
    win.addEventListener("load", resolve, { once: true });
  });
  await win.delayedStartupPromise;

  return win;
}
