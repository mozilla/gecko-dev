/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * EventEmitter.
 */

this.EventEmitter = function EventEmitter() {};

if (typeof(require) === "function") {
   module.exports = EventEmitter;
   var {Cu, components} = require("chrome");
} else {
  var EXPORTED_SYMBOLS = ["EventEmitter"];
  var Cu = this["Components"].utils;
  var components = Components;
}

const { Promise: promise } = Cu.import("resource://gre/modules/Promise.jsm", {});
const { Services } = Cu.import("resource://gre/modules/Services.jsm");

/**
 * Decorate an object with event emitter functionality.
 *
 * @param Object aObjectToDecorate
 *        Bind all public methods of EventEmitter to
 *        the aObjectToDecorate object.
 */
EventEmitter.decorate = function EventEmitter_decorate (aObjectToDecorate) {
  let emitter = new EventEmitter();
  aObjectToDecorate.on = emitter.on.bind(emitter);
  aObjectToDecorate.off = emitter.off.bind(emitter);
  aObjectToDecorate.once = emitter.once.bind(emitter);
  aObjectToDecorate.emit = emitter.emit.bind(emitter);
};

EventEmitter.prototype = {
  /**
   * Connect a listener.
   *
   * @param string aEvent
   *        The event name to which we're connecting.
   * @param function aListener
   *        Called when the event is fired.
   */
  on: function EventEmitter_on(aEvent, aListener) {
    if (!this._eventEmitterListeners)
      this._eventEmitterListeners = new Map();
    if (!this._eventEmitterListeners.has(aEvent)) {
      this._eventEmitterListeners.set(aEvent, []);
    }
    this._eventEmitterListeners.get(aEvent).push(aListener);
  },

  /**
   * Listen for the next time an event is fired.
   *
   * @param string aEvent
   *        The event name to which we're connecting.
   * @param function aListener
   *        (Optional) Called when the event is fired. Will be called at most
   *        one time.
   * @return promise
   *        A promise which is resolved when the event next happens. The
   *        resolution value of the promise is the first event argument. If
   *        you need access to second or subsequent event arguments (it's rare
   *        that this is needed) then use aListener
   */
  once: function EventEmitter_once(aEvent, aListener) {
    let deferred = promise.defer();

    let handler = function(aEvent, aFirstArg) {
      this.off(aEvent, handler);
      if (aListener) {
        aListener.apply(null, arguments);
      }
      deferred.resolve(aFirstArg);
    }.bind(this);

    handler._originalListener = aListener;
    this.on(aEvent, handler);

    return deferred.promise;
  },

  /**
   * Remove a previously-registered event listener.  Works for events
   * registered with either on or once.
   *
   * @param string aEvent
   *        The event name whose listener we're disconnecting.
   * @param function aListener
   *        The listener to remove.
   */
  off: function EventEmitter_off(aEvent, aListener) {
    if (!this._eventEmitterListeners)
      return;
    let listeners = this._eventEmitterListeners.get(aEvent);
    if (listeners) {
      this._eventEmitterListeners.set(aEvent, listeners.filter(l => {
        return l !== aListener && l._originalListener !== aListener;
      }));
    }
  },

  /**
   * Emit an event.  All arguments to this method will
   * be sent to listner functions.
   */
  emit: function EventEmitter_emit(aEvent) {
    this.logEvent(aEvent, arguments);

    if (!this._eventEmitterListeners || !this._eventEmitterListeners.has(aEvent)) {
      return;
    }

    let originalListeners = this._eventEmitterListeners.get(aEvent);
    for (let listener of this._eventEmitterListeners.get(aEvent)) {
      // If the object was destroyed during event emission, stop
      // emitting.
      if (!this._eventEmitterListeners) {
        break;
      }

      // If listeners were removed during emission, make sure the
      // event handler we're going to fire wasn't removed.
      if (originalListeners === this._eventEmitterListeners.get(aEvent) ||
          this._eventEmitterListeners.get(aEvent).some(function(l) l === listener)) {
        try {
          listener.apply(null, arguments);
        }
        catch (ex) {
          // Prevent a bad listener from interfering with the others.
          let msg = ex + ": " + ex.stack;
          Cu.reportError(msg);
          dump(msg + "\n");
        }
      }
    }
  },

  logEvent: function(aEvent, args) {
    let logging = Services.prefs.getBoolPref("devtools.dump.emit");

    if (logging) {
      let caller = components.stack.caller.caller;
      let func = caller.name;
      let path = caller.filename.split(/ -> /)[1] + ":" + caller.lineNumber;

      let argOut = "(";
      if (args.length === 1) {
        argOut += aEvent;
      }

      let out = "EMITTING: ";

      // We need this try / catch to prevent any dead object errors.
      try {
        for (let i = 1; i < args.length; i++) {
          if (i === 1) {
            argOut = "(" + aEvent + ", ";
          } else {
            argOut += ", ";
          }

          let arg = args[i];
          argOut += arg;

          if (arg && arg.nodeName) {
            argOut += " (" + arg.nodeName;
            if (arg.id) {
              argOut += "#" + arg.id;
            }
            if (arg.className) {
              argOut += "." + arg.className;
            }
            argOut += ")";
          }
        }
      } catch(e) {
        // Object is dead so the toolbox is most likely shutting down,
        // do nothing.
      }

      argOut += ")";
      out += "emit" + argOut + " from " + func + "() -> " + path + "\n";

      dump(out);
    }
  },
};
