/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

import {
  actionTypes as at,
  actionCreators as ac,
} from "resource://newtab/common/Actions.mjs";

export class NewTabMessaging {
  constructor() {
    this.initialized = false;
    this.ASRouterDispatch = null;
  }

  init() {
    if (!this.initialized) {
      Services.obs.addObserver(this, "newtab-message");
      this.initialized = true;
    }
  }

  uninit() {
    Services.obs.removeObserver(this, "newtab-message");
  }

  observe(subject, topic, _data) {
    if (topic === "newtab-message") {
      let { targetBrowser, message, dispatch } = subject.wrappedJSObject;
      this.ASRouterDispatch = dispatch;
      this.showMessage(targetBrowser, message);
    }
  }

  async showMessage(targetBrowser, message) {
    if (targetBrowser) {
      let actor =
        targetBrowser.browsingContext.currentWindowGlobal.getActor(
          "AboutNewTab"
        );
      if (actor) {
        let tabDetails = actor.getTabDetails();
        if (tabDetails) {
          // Only send the message for the tab that triggered the message
          this.store.dispatch(
            ac.OnlyToOneContent(
              {
                type: at.MESSAGE_SET,
                // send along portID as well so that the child process can easily just update the single tab
                data: {
                  message,
                  portID: tabDetails.portID,
                },
              },
              tabDetails.portID
            )
          );
        }
      }
    } else {
      // if targetBrowser is null, send to the preloaded tab / main process
      // This should only run if message is triggered from asrouter during dev
      this.store.dispatch(
        ac.AlsoToPreloaded({
          type: at.MESSAGE_SET,
          data: {
            message,
          },
        })
      );
      // Also force visibility for messages sent from about:asrouter during dev
      this.store.dispatch(
        ac.AlsoToPreloaded({
          type: at.MESSAGE_TOGGLE_VISIBILITY,
          data: false,
        })
      );
    }
  }

  /**
   *
   * @param {string} id ID of message to be blocked
   */
  blockMessage(id) {
    if (id) {
      this.ASRouterDispatch?.({
        type: "BLOCK_MESSAGE_BY_ID",
        data: {
          id,
        },
      });
    }
  }

  /**
   * Send impression to ASRouter
   * @param {Object} message
   */
  handleImpression(message) {
    this.sendTelemetry("IMPRESSION", message);
    this.ASRouterDispatch?.({
      type: "IMPRESSION",
      data: message,
    });
  }

  /**
   * Sends telemetry data through ASRouter to
   * match pattern with ASRouterTelemetry
   */
  sendTelemetry(event, message, source = "newtab") {
    const data = {
      action: "newtab_message_user_event",
      event,
      event_context: { source, page: "about:newtab" },
      message_id: message.id,
    };
    this.ASRouterDispatch?.({
      type: "NEWTAB_MESSAGE_TELEMETRY",
      data,
    });
  }

  async onAction(action) {
    switch (action.type) {
      case at.INIT:
        this.init();
        break;
      case at.UNINIT:
        this.uninit();
        break;
      case at.MESSAGE_IMPRESSION:
        this.handleImpression(action.data);
        break;
      case at.MESSAGE_DISMISS:
        this.sendTelemetry("DISMISS", action.data.message);
        break;
      case at.MESSAGE_CLICK:
        this.sendTelemetry("CLICK", action.data.message, action.data.source);
        break;
      case at.MESSAGE_BLOCK:
        this.blockMessage(action.data);
        break;
    }
  }
}
