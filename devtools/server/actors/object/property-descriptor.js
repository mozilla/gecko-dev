/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const DevToolsUtils = require("resource://devtools/shared/DevToolsUtils.js");

const {
  isStorage,
} = require("resource://devtools/server/actors/object/utils.js");

/**
 * A helper method that creates a property descriptor for the provided object,
 * properly formatted for sending in a protocol response.
 *
 * @param ObjectActor objectActor
 *        The object actor of the object we are current listing properties.
 * @param string name
 *        The property that the descriptor is generated for.
 * @param {Number} depth
 *                 Current depth in the generated preview object sent to the client.
 * @param boolean [onlyEnumerable]
 *        Optional: true if you want a descriptor only for an enumerable
 *        property, false otherwise.
 * @return object|undefined
 *         The property descriptor, or undefined if objectActor is not an enumerable
 *         property and onlyEnumerable=true.
 */
function propertyDescriptor(objectActor, name, depth, onlyEnumerable) {
  if (!DevToolsUtils.isSafeDebuggerObject(objectActor.obj)) {
    return undefined;
  }

  let desc;
  try {
    desc = objectActor.obj.getOwnPropertyDescriptor(name);
  } catch (e) {
    // Calling getOwnPropertyDescriptor on wrapped native prototypes is not
    // allowed (bug 560072). Inform the user with a bogus, but hopefully
    // explanatory, descriptor.
    return {
      configurable: false,
      writable: false,
      enumerable: false,
      value: e.name,
    };
  }

  if (isStorage(objectActor.obj)) {
    if (name === "length") {
      return undefined;
    }
    return desc;
  }

  if (!desc || (onlyEnumerable && !desc.enumerable)) {
    return undefined;
  }

  const retval = {
    configurable: desc.configurable,
    enumerable: desc.enumerable,
  };
  const { rawObj } = objectActor;

  if ("value" in desc) {
    retval.writable = desc.writable;
    retval.value = objectActor.createValueGrip(desc.value, depth);
  } else if (objectActor.threadActor.getWatchpoint(rawObj, name.toString())) {
    const watchpoint = objectActor.threadActor.getWatchpoint(rawObj, name.toString());
    retval.value = objectActor.createValueGrip(watchpoint.desc.value, depth);
    retval.watchpoint = watchpoint.watchpointType;
  } else {
    if ("get" in desc) {
      retval.get = objectActor.createValueGrip(desc.get, depth);
    }

    if ("set" in desc) {
      retval.set = objectActor.createValueGrip(desc.set, depth);
    }
  }
  return retval;
}

exports.propertyDescriptor = propertyDescriptor;
