/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Pool } = require("devtools/shared/protocol");

loader.lazyRequireGetter(
  this,
  "ObjectActor",
  "resource://devtools/server/actors/object.js",
  true
);
loader.lazyRequireGetter(
  this,
  "PauseScopedObjectActor",
  "resource://devtools/server/actors/pause-scoped.js",
  true
);

/**
 * A Pool dedicated to host only Object Actors for given JS values.
 *
 * It optionaly ensures that we instantate only one Object Actor
 * for any unique JS value.
 *
 * @param {ThreadActor} threadActor
 *        The related Thread Actor from which JS values are coming from.
 * @param {String} label
 *        Pool's description (for debugging purpose)
 * @param {Boolean} uniqueActorPerValue
 *        Ensure instantiating only one Object Actor for each unique JS Value
 *        passed to createObjectGrip.
 */
class ObjectActorPool extends Pool {
  constructor(threadActor, label, uniqueActorPerValue = false) {
    super(threadActor.conn, label);
    this.threadActor = threadActor;

    this.uniqueActorPerValue = uniqueActorPerValue;
  }

  objectActors = new WeakMap();

  /**
   * Create a grip for the given object.
   *
   * @param object object
   *        The object you want.
   * @param Number depth
   *        Depth of the object compared to the top level object,
   *        when we are inspecting nested attributes.
   * @param object [objectActorAttributes]
   *        An optional object whose properties will be assigned to the ObjectActor being created.
   * @return object
   *        The object actor form, aka "grip"
   */
  createObjectGrip(
    object,
    depth,
    objectActorAttributes = {},
  ) {
    // When we are creating object actors within the thread or JS tracer actor pools,
    // we have some caching to prevent instantiating object actors twice for the same JS object.
    if (this.uniqueActorPerValue) {
      if (this.objectActors.has(object)) {
        return this.objectActors.get(object).form({ depth });
      }

      // Even if we are currently creating objects actors while being paused,
      // in threadActor.pauseLifetimePool, we are looking into threadLifetimePool
      // in case we created an actor for that object *before* pausing.
      if (this.threadActor.threadLifetimePool.objectActors.has(object)) {
        return this.threadActor.threadLifetimePool.objectActors.get(object).form({ depth });
      }
    }

    // We instantiate PauseScopedObjectActor instances for any actor created by the Thread Actor
    const isGripForThreadActor = this == this.threadActor.threadLifetimePool || this == this.threadActor.pauseLifetimePool;

    const ActorClass = isGripForThreadActor ? PauseScopedObjectActor : ObjectActor;

    const actor = new ActorClass(this.threadActor, object, {
      // custom formatters are injecting their own attributes here
      ...objectActorAttributes,
    });
    this.manage(actor);

    if (this.uniqueActorPerValue) {
      this.objectActors.set(object, actor);
    }

    // Pass the current depth to form method so that it can communicate it to the previewers.
    // So that the actor form output may change depending on the current depth of the object within the requested preview.
    return actor.form({ depth });
  }
}

exports.ObjectActorPool = ObjectActorPool;
