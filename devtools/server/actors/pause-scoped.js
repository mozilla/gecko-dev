/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { ObjectActor } = require("resource://devtools/server/actors/object.js");

class PauseScopedObjectActor extends ObjectActor {
  /**
   * Creates a pause-scoped actor for the specified object.
   * @see ObjectActor
   */
  constructor(threadActor, obj, hooks) {
    super(threadActor, obj, hooks);

    const guardWithPaused = [
      "decompile",
      "displayString",
      "ownPropertyNames",
      "parameterNames",
      "property",
      "prototype",
      "prototypeAndProperties",
      "scope",
    ];

    for (const methodName of guardWithPaused) {
      this[methodName] = this.withPaused(this[methodName]);
    }

    // Cache this thread actor attribute as we may query it after the actor destruction.
    this.threadLifetimePool = this.threadActor.threadLifetimePool;
  }

  isThreadLifetimePool() {
    return this.getParent() === this.threadLifetimePool;
  }

  isPaused() {
    return this.threadActor ? this.threadActor.state === "paused" : true;
  }

  withPaused(method) {
    return function () {
      if (this.isPaused()) {
        return method.apply(this, arguments);
      }

      return {
        error: "wrongState",
        message:
          this.constructor.name +
          " actors can only be accessed while the thread is paused.",
      };
    };
  }

  /**
   * Handle a protocol request to promote a pause-lifetime grip to a
   * thread-lifetime grip.
   *
   * This method isn't used by DevTools frontend, but by VS Code Firefox adapter
   * in order to keep the object actor alive after resume and be able to remove
   * watchpoints.
   */
  threadGrip() {
    this.threadActor.promoteObjectToThreadLifetime(this);
    return {};
  }

  /**
   * Handle a protocol request to release a thread-lifetime grip.
   */
  destroy() {
    if (!this.isThreadLifetimePool()) {
      return {
        error: "notReleasable",
        message: "Only thread-lifetime actors can be released.",
      };
    }

    super.destroy();
    return null;
  }
}

exports.PauseScopedObjectActor = PauseScopedObjectActor;
