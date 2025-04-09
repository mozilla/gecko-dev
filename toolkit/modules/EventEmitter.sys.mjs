/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "loggingEnabled",
  "toolkit.dump.emit",
  false
);

function describeNthCaller(n) {
  let caller = Components.stack;
  // Do one extra iteration to skip this function.
  while (n >= 0) {
    --n;
    caller = caller.caller;
  }

  let func = caller.name;
  let file = caller.filename;
  if (file.includes(" -> ")) {
    file = caller.filename.split(/ -> /)[1];
  }
  let path = file + ":" + caller.lineNumber;

  return func + "() -> " + path;
}

/**
 * An event emitter class that handles listeners for events being emitted.
 */
export class EventEmitter {
  /**
   * Decorate an object with event emitter functionality.
   *
   * @param {object} objectToDecorate
   *   Bind all public methods of EventEmitter to the objectToDecorate object.
   */
  static decorate(objectToDecorate) {
    let emitter = new EventEmitter();
    objectToDecorate.on = emitter.on.bind(emitter);
    objectToDecorate.off = emitter.off.bind(emitter);
    objectToDecorate.once = emitter.once.bind(emitter);
    objectToDecorate.emit = emitter.emit.bind(emitter);
  }

  /**
   * Connect a listener.
   *
   * @param {string} event
   *   The event name to which we're connecting.
   * @param {Function} listener
   *   The function called when the event is fired.
   */
  on(event, listener) {
    if (!this._eventEmitterListeners) {
      this._eventEmitterListeners = new Map();
    }
    if (!this._eventEmitterListeners.has(event)) {
      this._eventEmitterListeners.set(event, []);
    }
    this._eventEmitterListeners.get(event).push(listener);
  }

  /**
   * Listen for the next time an event is fired.
   *
   * @param {string} event
   *   The event name to which we're connecting.
   * @param {Function} [listener]
   *   Called when the event is fired. Will be called at most one time.
   * @returns {Promise}
   *   A promise which is resolved when the event next happens. The resolution
   *   value of the promise is the first event argument. If you need access to
   *   second or subsequent event arguments (it's rare that this is needed) then
   *   use listener
   */
  once(event, listener) {
    return new Promise(resolve => {
      let handler = (_, first, ...rest) => {
        this.off(event, handler);
        if (listener) {
          listener(event, first, ...rest);
        }
        resolve(first);
      };

      handler._originalListener = listener;
      this.on(event, handler);
    });
  }

  /**
   * Remove a previously-registered event listener.  Works for events
   * registered with either on or once.
   *
   * @param {string} event
   *   The event name whose listener we're disconnecting.
   * @param {Function} listener
   *   The listener to remove.
   */
  off(event, listener) {
    if (!this._eventEmitterListeners) {
      return;
    }
    let listeners = this._eventEmitterListeners.get(event);
    if (listeners) {
      this._eventEmitterListeners.set(
        event,
        listeners.filter(l => {
          return l !== listener && l._originalListener !== listener;
        })
      );
    }
  }

  /**
   * Emit an event.  All arguments to this method will
   * be sent to listener functions.
   *
   * @param {string} event
   *   The event name whose listener we're disconnecting.
   * @param {...any} args
   *   Arguments to be passed to the event listeners.
   */
  emit(event, ...args) {
    this.#logEvent(event, args);

    if (
      !this._eventEmitterListeners ||
      !this._eventEmitterListeners.has(event)
    ) {
      return;
    }

    let originalListeners = this._eventEmitterListeners.get(event);
    for (let listener of this._eventEmitterListeners.get(event)) {
      // If the object was destroyed during event emission, stop
      // emitting.
      if (!this._eventEmitterListeners) {
        break;
      }

      // If listeners were removed during emission, make sure the
      // event handler we're going to fire wasn't removed.
      if (
        originalListeners === this._eventEmitterListeners.get(event) ||
        this._eventEmitterListeners.get(event).some(l => l === listener)
      ) {
        try {
          listener(event, ...args);
        } catch (ex) {
          console.error(ex);
        }
      }
    }
  }

  #logEvent(event, args) {
    if (!lazy.loggingEnabled) {
      return;
    }

    let description = describeNthCaller(2);

    let argOut = "(";
    if (args.length === 1) {
      argOut += event;
    }

    let out = "EMITTING: ";

    // We need this try / catch to prevent any dead object errors.
    try {
      for (let i = 1; i < args.length; i++) {
        if (i === 1) {
          argOut = "(" + event + ", ";
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
    } catch (e) {
      // Object is dead so the toolbox is most likely shutting down,
      // do nothing.
    }

    argOut += ")";
    out += "emit" + argOut + " from " + description + "\n";

    dump(out);
  }
}
