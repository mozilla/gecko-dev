/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var { settleAll } = require("devtools/shared/DevToolsUtils");
var EventEmitter = require("devtools/shared/event-emitter");

var { Pool } = require("./Pool");
var {
  getStack,
  callFunctionWithAsyncStack,
} = require("devtools/shared/platform/stack");
const defer = require("devtools/shared/defer");

/**
 * Base class for client-side actor fronts.
 *
 * @param [DebuggerClient|null] conn
 *   The conn must either be DebuggerClient or null. Must have
 *   addActorPool, removeActorPool, and poolFor.
 *   conn can be null if the subclass provides a conn property.
 * @param [Target|null] target
 *   If we are instantiating a target-scoped front, this is a reference to the front's
 *   Target instance, otherwise this is null.
 * @param [Front|null] parentFront
 *   The parent front. This is only available if the Front being initialized is a child
 *   of a parent front.
 * @constructor
 */
class Front extends Pool {
  constructor(conn = null, targetFront = null, parentFront = null) {
    super(conn);
    this.actorID = null;
    // The targetFront attribute represents the debuggable context. Only target-scoped
    // fronts and their children fronts will have the targetFront attribute set.
    this.targetFront = targetFront;
    // The parentFront attribute points to its parent front. Only children of
    // target-scoped fronts will have the parentFront attribute set.
    this.parentFront = parentFront;
    this._requests = [];

    // Front listener functions registered via `onFront` get notified
    // of new fronts via this dedicated EventEmitter object.
    this._frontListeners = new EventEmitter();

    // List of optional listener for each event, that is processed immediatly on packet
    // receival, before emitting event via EventEmitter on the Front.
    // These listeners are register via Front.before function.
    // Map(Event Name[string] => Event Listener[function])
    this._beforeListeners = new Map();
  }

  destroy() {
    // Reject all outstanding requests, they won't make sense after
    // the front is destroyed.
    while (this._requests && this._requests.length > 0) {
      const { deferred, to, type, stack } = this._requests.shift();
      const msg =
        "Connection closed, pending request to " +
        to +
        ", type " +
        type +
        " failed" +
        "\n\nRequest stack:\n" +
        stack.formattedStack;
      deferred.reject(new Error(msg));
    }
    super.destroy();
    this.clearEvents();
    this.actorID = null;
    this.targetFront = null;
    this.parentFront = null;
    this._frontListeners = null;
    this._beforeListeners = null;
  }

  async manage(front, form, ctx) {
    if (!front.actorID) {
      throw new Error(
        "Can't manage front without an actor ID.\n" +
          "Ensure server supports " +
          front.typeName +
          "."
      );
    }
    super.manage(front);

    if (typeof front.initialize == "function") {
      await front.initialize();
    }

    // Ensure calling form() *before* notifying about this front being just created.
    // We exprect the front to be fully initialized, especially via its form attributes.
    // But do that *after* calling manage() so that the front is already registered
    // in Pools and can be fetched by its ID, in case a child actor, created in form()
    // tries to get a reference to its parent via the actor ID.
    if (form) {
      front.form(form, ctx);
    }

    // Call listeners registered via `onFront` method
    this._frontListeners.emit(front.typeName, front);
  }

  // Run callback on every front of this type that currently exists, and on every
  // instantiation of front type in the future.
  onFront(typeName, callback) {
    // First fire the callback on already instantiated fronts
    for (const front of this.poolChildren()) {
      if (front.typeName == typeName) {
        callback(front);
      }
    }
    // Then register the callback for fronts instantiated in the future
    this._frontListeners.on(typeName, callback);
  }

  /**
   * Register an event listener that will be called immediately on packer receival.
   * The given callback is going to be called before emitting the event via EventEmitter
   * API on the Front. Event emitting will be delayed if the callback is async.
   * Only one such listener can be registered per type of event.
   *
   * @param String type
   *   Event emitted by the actor to intercept.
   * @param Function callback
   *   Function that will process the event.
   */
  before(type, callback) {
    if (this._beforeListeners.has(type)) {
      throw new Error(
        `Can't register multiple before listeners for "${type}".`
      );
    }
    this._beforeListeners.set(type, callback);
  }

  toString() {
    return "[Front for " + this.typeName + "/" + this.actorID + "]";
  }

  /**
   * Update the actor from its representation.
   * Subclasses should override this.
   */
  form(form) {}

  /**
   * Send a packet on the connection.
   */
  send(packet) {
    if (packet.to) {
      this.conn._transport.send(packet);
    } else {
      packet.to = this.actorID;
      // The connection might be closed during the promise resolution
      if (this.conn._transport) {
        this.conn._transport.send(packet);
      }
    }
  }

  /**
   * Send a two-way request on the connection.
   */
  request(packet) {
    const deferred = defer();
    // Save packet basics for debugging
    const { to, type } = packet;
    this._requests.push({
      deferred,
      to: to || this.actorID,
      type,
      stack: getStack(),
    });
    this.send(packet);
    return deferred.promise;
  }

  /**
   * Handler for incoming packets from the client's actor.
   */
  onPacket(packet) {
    // Pick off event packets
    const type = packet.type || undefined;
    if (this._clientSpec.events && this._clientSpec.events.has(type)) {
      const event = this._clientSpec.events.get(packet.type);
      let args;
      try {
        args = event.request.read(packet, this);
      } catch (ex) {
        console.error("Error reading event: " + packet.type);
        console.exception(ex);
        throw ex;
      }
      // Check for "pre event" callback to be processed before emitting events on fronts
      // Use event.name instead of packet.type to use specific event name instead of RDP
      // packet's type.
      const beforeEvent = this._beforeListeners.get(event.name);
      if (beforeEvent) {
        const result = beforeEvent.apply(this, args);
        // Check to see if the beforeEvent returned a promise -- if so,
        // wait for their resolution before emitting. Otherwise, emit synchronously.
        if (result && typeof result.then == "function") {
          result.then(() => {
            super.emit(event.name, ...args);
          });
          return;
        }
      }

      super.emit(event.name, ...args);
      return;
    }

    // Remaining packets must be responses.
    if (this._requests.length === 0) {
      const msg =
        "Unexpected packet " + this.actorID + ", " + JSON.stringify(packet);
      const err = Error(msg);
      console.error(err);
      throw err;
    }

    const { deferred, stack } = this._requests.shift();
    callFunctionWithAsyncStack(
      () => {
        if (packet.error) {
          // "Protocol error" is here to avoid TBPL heuristics. See also
          // https://dxr.mozilla.org/webtools-central/source/tbpl/php/inc/GeneralErrorFilter.php
          let message;
          if (packet.error && packet.message) {
            message =
              "Protocol error (" + packet.error + "): " + packet.message;
          } else {
            message = packet.error;
          }
          deferred.reject(message);
        } else {
          deferred.resolve(packet);
        }
      },
      stack,
      "DevTools RDP"
    );
  }

  hasRequests() {
    return !!this._requests.length;
  }

  /**
   * Wait for all current requests from this front to settle.  This is especially useful
   * for tests and other utility environments that may not have events or mechanisms to
   * await the completion of requests without this utility.
   *
   * @return Promise
   *         Resolved when all requests have settled.
   */
  waitForRequestsToSettle() {
    return settleAll(this._requests.map(({ deferred }) => deferred.promise));
  }
}

exports.Front = Front;
