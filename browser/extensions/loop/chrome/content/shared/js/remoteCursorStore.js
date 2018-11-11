/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

var loop = loop || {};
loop.store = loop.store || {};

/**
 *  Manages the different cursors' events being exchanged between the parts.
 */
loop.store.RemoteCursorStore = (function() {
  "use strict";

  var CURSOR_MESSAGE_TYPES = loop.shared.utils.CURSOR_MESSAGE_TYPES;

  /**
   * A store to handle remote cursors events.
   */
  var RemoteCursorStore = loop.store.createStore({
    actions: [
      "sendCursorData",
      "receivedCursorData",
      "videoDimensionsChanged",
      "videoScreenStreamChanged"
    ],

    /**
     * Initializes the store.
     *
     * @param  {Object} options An object containing options for this store.
     *                          It should consist of:
     *                          - sdkDriver: The sdkDriver to use for sending
     *                                       the cursor events.
     */
    initialize: function(options) {
      options = options || {};

      if (!options.sdkDriver) {
        throw new Error("Missing option sdkDriver");
      }

      this._sdkDriver = options.sdkDriver;

      loop.subscribe("CursorPositionChange",
                     this._cursorPositionChangeListener.bind(this));
    },

    /**
     * Returns initial state data for this active room.
     */
    getInitialStoreState: function() {
      return {
        realVideoSize: null,
        remoteCursorPosition: null
      };
    },

    /**
     * Sends cursor position through the sdk.
     *
     * @param {Object} event An object containing the cursor position and
     *                       stream dimensions. It should contain:
     *                       - ratioX: Left position. Number between 0 and 1.
     *                       - ratioY: Top position. Number between 0 and 1.
     */
    _cursorPositionChangeListener: function(event) {
      this.sendCursorData({
        ratioX: event.ratioX,
        ratioY: event.ratioY,
        type: CURSOR_MESSAGE_TYPES.POSITION
      });
    },

    /**
     * Sends the cursor data to the SDK for broadcasting.
     *
     * @param {sharedActions.SendCursorData}
     *  actionData Contains the updated information for the cursor's position
     *      {
     *       ratioX {[0-1]} Cursor's position on the X axis
     *       ratioY {[0-1]} Cursor's position on the Y axis
     *       type   {String} Type of the data being sent
     *      }
     */
    sendCursorData: function(actionData) {
      switch (actionData.type) {
        case CURSOR_MESSAGE_TYPES.POSITION:
          this._sdkDriver.sendCursorMessage(actionData);
          break;
      }
    },

    /**
     * Receives cursor data and updates the store.
     *
     * @param {sharedActions.receivedCursorData} actionData
     */
    receivedCursorData: function(actionData) {
      switch (actionData.type) {
        case CURSOR_MESSAGE_TYPES.POSITION:
          this.setStoreState({
            remoteCursorPosition: {
              ratioX: actionData.ratioX,
              ratioY: actionData.ratioY
            }
          });
          break;
      }
    },

    /**
     * Listen to stream dimension changes.
     *
     * @param {sharedActions.VideoDimensionsChanged} actionData
     */
    videoDimensionsChanged: function(actionData) {
      if (actionData.videoType !== "screen") {
        return;
      }

      this.setStoreState({
        realVideoSize: {
          height: actionData.dimensions.height,
          width: actionData.dimensions.width
        }
      });
    },

    /**
     * Listen to screen stream changes. Because the cursor's position is likely
     * to be different respect to the new screen size, it's better to delete the
     * previous position and keep waiting for the next one.

     * @param {sharedActions.VideoScreenStreamChanged} actionData
     */
    videoScreenStreamChanged: function(actionData) {
      if (actionData.hasVideo) {
        return;
      }

      this.setStoreState({
        remoteCursorPosition: null
      });
    }
  });

  return RemoteCursorStore;
})();
