/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import('resource://gre/modules/XPCOMUtils.jsm');
Cu.import('resource://gre/modules/Services.jsm');

this.EXPORTED_SYMBOLS = ['SystemAppProxy'];

let SystemAppProxy = {
  _frame: null,
  _isReady: false,
  _pendingEvents: [],
  _pendingListeners: [],

  // To call when a new system app iframe is created
  registerFrame: function (frame) {
    this._isReady = false;
    this._frame = frame;

    // Register all DOM event listeners added before we got a ref to the app iframe
    this._pendingListeners
        .forEach((args) =>
                 this.addEventListener.apply(this, args));
    this._pendingListeners = [];
  },

  // Get the system app frame
  getFrame: function () {
    return this._frame;
  },

  // To call when it is ready to receive events
  setIsReady: function () {
    if (this._isReady) {
      Cu.reportError('SystemApp has already been declared as being ready.');
    }
    this._isReady = true;

    // Dispatch all events being queued while the system app was still loading
    this._pendingEvents
        .forEach(([type, details]) =>
                 this._sendCustomEvent(type, details));
    this._pendingEvents = [];
  },

  /*
   * Common way to send an event to the system app.
   *
   * // In gecko code:
   *   SystemAppProxy.sendCustomEvent('foo', { data: 'bar' });
   * // In system app:
   *   window.addEventListener('foo', function (event) {
   *     event.details == 'bar'
   *   });
   *
   *   @param type      The custom event type.
   *   @param details   The event details.
   *   @param noPending Set to true to emit this event even before the system
   *                    app is ready.
   */
  _sendCustomEvent: function systemApp_sendCustomEvent(type,
                                                       details,
                                                       noPending,
                                                       target) {
    let content = this._frame ? this._frame.contentWindow : null;

    // If the system app isn't ready yet,
    // queue events until someone calls setIsReady
    if (!content || (!this._isReady && !noPending)) {
      this._pendingEvents.push([type, details]);
      return null;
    }

    let event = content.document.createEvent('CustomEvent');

    let payload;
    // If the root object already has __exposedProps__,
    // we consider the caller already wrapped (correctly) the object.
    if ('__exposedProps__' in details) {
      payload = details;
    } else {
      payload = details ? Cu.cloneInto(details, content) : {};
    }

    event.initCustomEvent(type, true, false, payload);
    (target || content).dispatchEvent(event);

    return event;
  },

  // Now deprecated, use sendCustomEvent with a custom event name
  dispatchEvent: function systemApp_dispatchEvent(details, target) {
    return this._sendCustomEvent('mozChromeEvent', details, false, target);
  },

  dispatchKeyboardEvent: function systemApp_dispatchKeyboardEvent(type, details) {
    let content = this._frame ? this._frame.contentWindow : null;
    let e = new content.KeyboardEvent(type, details);
    content.dispatchEvent(e);
  },

  // Listen for dom events on the system app
  addEventListener: function systemApp_addEventListener() {
    let content = this._frame ? this._frame.contentWindow : null;
    if (!content) {
      this._pendingListeners.push(arguments);
      return false;
    }

    content.addEventListener.apply(content, arguments);
    return true;
  },

  removeEventListener: function systemApp_removeEventListener(name, listener) {
    let content = this._frame ? this._frame.contentWindow : null;
    if (content) {
      content.removeEventListener.apply(content, arguments);
    } else {
      this._pendingListeners = this._pendingListeners.filter(
        args => {
          return args[0] != name || args[1] != listener;
        });
    }
  },

  getFrames: function systemApp_getFrames() {
    let systemAppFrame = this._frame;
    if (!systemAppFrame) {
      return [];
    }
    let list = [systemAppFrame];
    let frames = systemAppFrame.contentDocument.querySelectorAll('iframe');
    for (let i = 0; i < frames.length; i++) {
      list.push(frames[i]);
    }
    return list;
  }
};
this.SystemAppProxy = SystemAppProxy;

