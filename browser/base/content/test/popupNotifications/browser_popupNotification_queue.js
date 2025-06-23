/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint-disable mozilla/no-arbitrary-setTimeout */

function test() {
  waitForExplicitFinish();

  ok(PopupNotifications, "PopupNotifications object exists");
  ok(PopupNotifications.panel, "PopupNotifications panel exists");

  setup();
}

var tests = [
  {
    id: "Test#1",
    run() {
      const panel = document.createXULElement("panel");
      panel.setAttribute("id", "non-queue-popup");
      panel.setAttribute("type", "arrow");
      panel.setAttribute("flip", "both");
      panel.setAttribute("consumeoutsideclicks", "false");
      document.documentElement.appendChild(panel);

      this.state = "PANEL_SHOWN";
      panel.openPopup(null, "topcenter topleft", 100, 100, false, null);

      this.notifyObj = new BasicNotification(this.id);
      this.notifyObj.options.queue = true;
      showNotification(this.notifyObj);

      new Promise(r => setTimeout(r, 200)).then(() => {
        this.state = "PANEL_HIDDEN";
        panel.hidePopup();
      });
    },

    onShown(popup) {
      is(
        this.state,
        "PANEL_HIDDEN",
        "The notification is shown only when the panel is dismissed"
      );

      checkPopup(popup, this.notifyObj);
      triggerMainCommand(popup);
    },
    onHidden() {},
  },
];
