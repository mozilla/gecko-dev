/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint-disable spaced-comment, brace-style, indent-legacy, consistent-return */

"use strict";

// Create proxies for objects in the replaying process.

const ReplayDebugger = require("devtools/server/actors/replay/debugger");

let _dbg = null;
function dbg() {
  if (!_dbg) {
    _dbg = new ReplayDebugger();
  }
  return _dbg;
}

function dbgObject(id) {
  return dbg()._pool.getObject(id);
}

///////////////////////////////////////////////////////////////////////////////
// Public Interface
///////////////////////////////////////////////////////////////////////////////

const ReplayInspector = {
  // For use by ReplayDebugger.
  wrapObject,
  unwrapObject(obj) {
    return proxyMap.get(obj);
  },
};

///////////////////////////////////////////////////////////////////////////////
// Replaying Object Proxies
///////////////////////////////////////////////////////////////////////////////

// Map from replaying object proxies to the underlying Debugger.Object.
const proxyMap = new Map();

function createInspectorObject(obj) {
  let target;
  if (obj.callable) {
    // Proxies need callable targets in order to be callable themselves.
    target = function() {};
    target.object = obj;
  } else {
    // Place non-callable targets in a box as well, so that we can change the
    // underlying ReplayDebugger.Object later.
    target = { object: obj };
  }
  const proxy = new Proxy(target, ReplayInspectorProxyHandler);

  proxyMap.set(proxy, obj);
  return proxy;
}

function wrapObject(obj) {
  assert(obj instanceof ReplayDebugger.Object);
  if (!obj._inspectorObject) {
    obj._inspectorObject = createInspectorObject(obj);
  }
  return obj._inspectorObject;
}

function wrapValue(value) {
  if (value && typeof value == "object") {
    return wrapObject(value);
  }
  return value;
}

function unwrapValue(value) {
  if (!isNonNullObject(value)) {
    return value;
  }

  const obj = proxyMap.get(value);
  if (obj) {
    return obj;
  }

  if (value instanceof Object) {
    const rv = dbg()._sendRequest({ type: "createObject" });
    const newobj = dbgObject(rv.id);

    Object.entries(value).forEach(([name, propvalue]) => {
      const unwrapped = unwrapValue(propvalue);
      setObjectProperty(newobj, name, unwrapped);
    });
    return newobj;
  }

  ThrowError("Can't unwrap value");
}

function getObjectProperty(obj, name) {
  assert(obj._pool == dbg()._pool);
  const rv = dbg()._sendRequestAllowDiverge({
    type: "getObjectPropertyValue",
    id: obj._data.id,
    name,
  });
  return dbg()._pool.convertCompletionValue(rv);
}

function setObjectProperty(obj, name, value) {
  assert(obj._pool == dbg()._pool);
  const rv = dbg()._sendRequestAllowDiverge({
    type: "setObjectPropertyValue",
    id: obj._data.id,
    name,
    value: dbg()._convertValueForChild(value),
  });
  return dbg()._pool.convertCompletionValue(rv);
}

function getTargetObject(target) {
  assert(target.object._data);
  return target.object;
}

const ReplayInspectorProxyHandler = {
  getPrototypeOf(target) {
    target = getTargetObject(target);

    // Cherry pick some objects that are used in instanceof comparisons by
    // server inspector code.
    if (target._data.class == "NamedNodeMap") {
      return NamedNodeMap.prototype;
    }

    return null;
  },

  has(target, name) {
    target = getTargetObject(target);

    if (typeof name == "symbol") {
      return name == Symbol.iterator;
    }

    if (name == "toString") {
      return true;
    }

    // See if this is an 'own' data property.
    const desc = target.getOwnPropertyDescriptor(name);
    return !!desc;
  },

  get(target, name, receiver) {
    target = getTargetObject(target);

    if (typeof name == "symbol") {
      return undefined;
    }

    if (name == "toString") {
      return () => `ReplayInspectorProxy #${target._data.id}`;
    }

    // See if this is an 'own' data property.
    if (!target._modifiedProperties || !target._modifiedProperties.has(name)) {
      const desc = target.getOwnPropertyDescriptor(name);
      if (desc && "value" in desc) {
        return wrapValue(desc.value);
      }
    }

    // Get the property on the target object directly in the replaying process.
    const rv = getObjectProperty(target, name);
    if ("return" in rv) {
      return wrapValue(rv.return);
    }
    ThrowError(rv.throw);
  },

  set(target, name, value) {
    target = getTargetObject(target);

    if (!target._modifiedProperties) {
      target._modifiedProperties = new Set();
    }
    target._modifiedProperties.add(name);

    const rv = setObjectProperty(target, name, unwrapValue(value));
    if ("return" in rv) {
      return true;
    }
    ThrowError(rv.throw);
  },

  apply(target, thisArg, args) {
    target = getTargetObject(target);

    const rv = target.apply(
      unwrapValue(thisArg),
      args.map(v => unwrapValue(v))
    );
    if ("return" in rv) {
      return wrapValue(rv.return);
    }
    ThrowError(rv.throw);
  },

  construct(target, args) {
    NotAllowed();
  },

  getOwnPropertyDescriptor(target, name) {
    target = getTargetObject(target);

    const desc = target.getOwnPropertyDescriptor(name);
    if (!desc) {
      return null;
    }

    // Note: ReplayDebugger.Object.getOwnPropertyDescriptor always returns a
    // fresh object, so we can modify it in place.
    if ("value" in desc) {
      desc.value = wrapValue(desc.value);
    }
    if ("get" in desc) {
      desc.get = wrapValue(desc.get);
    }
    if ("set" in desc) {
      desc.set = wrapValue(desc.set);
    }
    desc.configurable = true;
    return desc;
  },

  ownKeys(target) {
    target = getTargetObject(target);
    return target.getOwnPropertyNames();
  },

  isExtensible(target) {
    NYI();
  },

  setPrototypeOf() {
    NotAllowed();
  },
  preventExtensions() {
    NotAllowed();
  },
  defineProperty() {
    NotAllowed();
  },
  deleteProperty() {
    NotAllowed();
  },
};

///////////////////////////////////////////////////////////////////////////////
// Utilities
///////////////////////////////////////////////////////////////////////////////

function NYI() {
  ThrowError("Not yet implemented");
}

function NotAllowed() {
  ThrowError("Not allowed");
}

function ThrowError(msg) {
  const error = new Error(msg);
  dump(
    "ReplayInspector Server Error: " + msg + " Stack: " + error.stack + "\n"
  );
  throw error;
}

function assert(v) {
  if (!v) {
    ThrowError("Assertion Failed!");
  }
}

function isNonNullObject(obj) {
  return obj && (typeof obj == "object" || typeof obj == "function");
}

module.exports = ReplayInspector;
