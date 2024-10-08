/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Actor } = require("resource://devtools/shared/protocol.js");
const {
  propertyIteratorSpec,
} = require("resource://devtools/shared/specs/property-iterator.js");

const DevToolsUtils = require("resource://devtools/shared/DevToolsUtils.js");
loader.lazyRequireGetter(
  this,
  "ObjectUtils",
  "resource://devtools/server/actors/object/utils.js"
);
loader.lazyRequireGetter(
  this,
  "propertyDescriptor",
  "resource://devtools/server/actors/object/property-descriptor.js",
  true
);

/**
 * Creates an actor to iterate over an object's property names and values.
 *
 * @param objectActor ObjectActor
 *        The object actor.
 * @param options Object
 *        A dictionary object with various boolean attributes:
 *        - enumEntries Boolean
 *          If true, enumerates the entries of a Map or Set object
 *          instead of enumerating properties.
 *        - ignoreIndexedProperties Boolean
 *          If true, filters out Array items.
 *          e.g. properties names between `0` and `object.length`.
 *        - ignoreNonIndexedProperties Boolean
 *          If true, filters out items that aren't array items
 *          e.g. properties names that are not a number between `0`
 *          and `object.length`.
 *        - sort Boolean
 *          If true, the iterator will sort the properties by name
 *          before dispatching them.
 *        - query String
 *          If non-empty, will filter the properties by names and values
 *          containing this query string. The match is not case-sensitive.
 *          Regarding value filtering it just compare to the stringification
 *          of the property value.
 */
class PropertyIteratorActor extends Actor {
    constructor(objectActor, options, conn) {
      super(conn, propertyIteratorSpec);
      if (!DevToolsUtils.isSafeDebuggerObject(objectActor.obj)) {
        this.iterator = {
          size: 0,
          propertyName: index => undefined,
          propertyDescription: index => undefined,
        };
      } else if (options.enumEntries) {
        const cls = objectActor.className;
        if (cls == "Map") {
          this.iterator = enumMapEntries(objectActor, 0);
        } else if (cls == "WeakMap") {
          this.iterator = enumWeakMapEntries(objectActor, 0);
        } else if (cls == "Set") {
          this.iterator = enumSetEntries(objectActor, 0);
        } else if (cls == "WeakSet") {
          this.iterator = enumWeakSetEntries(objectActor, 0);
        } else if (cls == "Storage") {
          this.iterator = enumStorageEntries(objectActor, 0);
        } else if (cls == "URLSearchParams") {
          this.iterator = enumURLSearchParamsEntries(objectActor, 0);
        } else if (cls == "Headers") {
          this.iterator = enumHeadersEntries(objectActor, 0);
        } else if (cls == "HighlightRegistry") {
          this.iterator = enumHighlightRegistryEntries(objectActor, 0);
        } else if (cls == "FormData") {
          this.iterator = enumFormDataEntries(objectActor, 0);
        } else if (cls == "MIDIInputMap") {
          this.iterator = enumMidiInputMapEntries(objectActor, 0);
        } else if (cls == "MIDIOutputMap") {
          this.iterator = enumMidiOutputMapEntries(objectActor, 0);
        } else if (cls == "CustomStateSet") {
          this.iterator = enumCustomStateSetEntries(objectActor, 0);
        } else {
          throw new Error(
            "Unsupported class to enumerate entries from: " + cls
          );
        }
      } else if (
        ObjectUtils.isArray(objectActor.obj) &&
        options.ignoreNonIndexedProperties &&
        !options.query
      ) {
        this.iterator = enumArrayProperties(objectActor, options, 0);
      } else {
        this.iterator = enumObjectProperties(objectActor, options, 0);
      }
    }

    form() {
      return {
        type: this.typeName,
        actor: this.actorID,
        count: this.iterator.size,
      };
    }

    names({ indexes }) {
      const list = [];
      for (const idx of indexes) {
        list.push(this.iterator.propertyName(idx));
      }
      return indexes;
    }

    slice({ start, count }) {
      const ownProperties = Object.create(null);
      for (let i = start, m = start + count; i < m; i++) {
        const name = this.iterator.propertyName(i);
        ownProperties[name] = this.iterator.propertyDescription(i);
      }

      return {
        ownProperties,
      };
    }

    all() {
      return this.slice({ start: 0, count: this.iterator.size });
    }
  }

function waiveXrays(obj) {
  return isWorker ? obj : Cu.waiveXrays(obj);
}

function unwaiveXrays(obj) {
  return isWorker ? obj : Cu.unwaiveXrays(obj);
}

/**
 * Helper function to create a grip from a Map/Set entry
 */
function gripFromEntry(objectActor, entry, depth) {
  entry = unwaiveXrays(entry);
  return objectActor.createValueGrip(
    ObjectUtils.makeDebuggeeValueIfNeeded(objectActor.obj, entry),
    depth
  );
}

function enumArrayProperties(objectActor, options, depth) {
  return {
    size: ObjectUtils.getArrayLength(objectActor.obj),
    propertyName(index) {
      return index;
    },
    propertyDescription(index) {
      return propertyDescriptor(objectActor, index, depth);
    },
  };
}

function enumObjectProperties(objectActor, options, depth) {
  let names = [];
  try {
    names = objectActor.obj.getOwnPropertyNames();
  } catch (ex) {
    // Calling getOwnPropertyNames() on some wrapped native prototypes is not
    // allowed: "cannot modify properties of a WrappedNative". See bug 952093.
  }

  if (options.ignoreNonIndexedProperties || options.ignoreIndexedProperties) {
    const length = DevToolsUtils.getProperty(objectActor.obj, "length");
    let sliceIndex;

    const isLengthTrustworthy =
      isUint32(length) &&
      (!length || ObjectUtils.isArrayIndex(names[length - 1])) &&
      !ObjectUtils.isArrayIndex(names[length]);

    if (!isLengthTrustworthy) {
      // The length property may not reflect what the object looks like, let's find
      // where indexed properties end.

      if (!ObjectUtils.isArrayIndex(names[0])) {
        // If the first item is not a number, this means there is no indexed properties
        // in this object.
        sliceIndex = 0;
      } else {
        sliceIndex = names.length;
        while (sliceIndex > 0) {
          if (ObjectUtils.isArrayIndex(names[sliceIndex - 1])) {
            break;
          }
          sliceIndex--;
        }
      }
    } else {
      sliceIndex = length;
    }

    // It appears that getOwnPropertyNames always returns indexed properties
    // first, so we can safely slice `names` for/against indexed properties.
    // We do such clever operation to optimize very large array inspection.
    if (options.ignoreIndexedProperties) {
      // Keep items after `sliceIndex` index
      names = names.slice(sliceIndex);
    } else if (options.ignoreNonIndexedProperties) {
      // Keep `sliceIndex` first items
      names.length = sliceIndex;
    }
  }

  const safeGetterValues = objectActor._findSafeGetterValues(names, depth);
  const safeGetterNames = Object.keys(safeGetterValues);
  // Merge the safe getter values into the existing properties list.
  for (const name of safeGetterNames) {
    if (!names.includes(name)) {
      names.push(name);
    }
  }

  if (options.query) {
    let { query } = options;
    query = query.toLowerCase();
    names = names.filter(name => {
      // Filter on attribute names
      if (name.toLowerCase().includes(query)) {
        return true;
      }
      // and then on attribute values
      let desc;
      try {
        desc = objectActor.obj.getOwnPropertyDescriptor(name);
      } catch (e) {
        // Calling getOwnPropertyDescriptor on wrapped native prototypes is not
        // allowed (bug 560072).
      }
      if (desc?.value && String(desc.value).includes(query)) {
        return true;
      }
      return false;
    });
  }

  if (options.sort) {
    names.sort();
  }

  return {
    size: names.length,
    propertyName(index) {
      return names[index];
    },
    propertyDescription(index) {
      const name = names[index];
      let desc = propertyDescriptor(objectActor, name, depth);
      if (!desc) {
        desc = safeGetterValues[name];
      } else if (name in safeGetterValues) {
        // Merge the safe getter values into the existing properties list.
        const { getterValue, getterPrototypeLevel } = safeGetterValues[name];
        desc.getterValue = getterValue;
        desc.getterPrototypeLevel = getterPrototypeLevel;
      }
      return desc;
    },
  };
}

function getMapEntries(objectActor) {
  const { obj, rawObj } = objectActor;
  // Iterating over a Map via .entries goes through various intermediate
  // objects - an Iterator object, then a 2-element Array object, then the
  // actual values we care about. We don't have Xrays to Iterator objects,
  // so we get Opaque wrappers for them. And even though we have Xrays to
  // Arrays, the semantics often deny access to the entires based on the
  // nature of the values. So we need waive Xrays for the iterator object
  // and the tupes, and then re-apply them on the underlying values until
  // we fix bug 1023984.
  //
  // Even then though, we might want to continue waiving Xrays here for the
  // same reason we do so for Arrays above - this filtering behavior is likely
  // to be more confusing than beneficial in the case of Object previews.
  const iterator = obj.makeDebuggeeValue(
    waiveXrays(Map.prototype.keys.call(rawObj))
  );
  return [...DevToolsUtils.makeDebuggeeIterator(iterator)].map(k => {
    const key = waiveXrays(ObjectUtils.unwrapDebuggeeValue(k));
    const value = Map.prototype.get.call(rawObj, key);
    return [key, value];
  });
}

function enumMapEntries(objectActor, depth) {
  const entries = getMapEntries(objectActor);

  return {
    [Symbol.iterator]: function*() {
      for (const [key, value] of entries) {
        yield [key, value].map(val => gripFromEntry(objectActor, val, depth));
      }
    },
    size: entries.length,
    propertyName(index) {
      return index;
    },
    propertyDescription(index) {
      const [key, val] = entries[index];
      return {
        enumerable: true,
        value: {
          type: "mapEntry",
          preview: {
            key: gripFromEntry(objectActor, key, depth),
            value: gripFromEntry(objectActor, val, depth),
          },
        },
      };
    },
  };
}

function enumStorageEntries(objectActor, depth) {
  // Iterating over local / sessionStorage entries goes through various
  // intermediate objects - an Iterator object, then a 2-element Array object,
  // then the actual values we care about. We don't have Xrays to Iterator
  // objects, so we get Opaque wrappers for them.
  const { rawObj } = objectActor;
  const keys = [];
  for (let i = 0; i < rawObj.length; i++) {
    keys.push(rawObj.key(i));
  }
  return {
    [Symbol.iterator]: function*() {
      for (const key of keys) {
        const value = rawObj.getItem(key);
        yield [key, value].map(val => gripFromEntry(objectActor, val, depth));
      }
    },
    size: keys.length,
    propertyName(index) {
      return index;
    },
    propertyDescription(index) {
      const key = keys[index];
      const val = rawObj.getItem(key);
      return {
        enumerable: true,
        value: {
          type: "storageEntry",
          preview: {
            key: gripFromEntry(objectActor, key, depth),
            value: gripFromEntry(objectActor, val, depth),
          },
        },
      };
    },
  };
}

function enumURLSearchParamsEntries(objectActor, depth) {
  const entries = [...waiveXrays(URLSearchParams.prototype.entries.call(objectActor.rawObj))];

  return {
    [Symbol.iterator]: function*() {
      for (const [key, value] of entries) {
        yield [key, value];
      }
    },
    size: entries.length,
    propertyName(index) {
      // UrlSearchParams entries can have the same key multiple times (e.g. `?a=1&a=2`),
      // so let's return the index as a name to be able to display them properly in the client.
      return index;
    },
    propertyDescription(index) {
      const [key, value] = entries[index];

      return {
        enumerable: true,
        value: {
          type: "urlSearchParamsEntry",
          preview: {
            key: gripFromEntry(objectActor, key, depth),
            value: gripFromEntry(objectActor, value, depth),
          },
        },
      };
    },
  };
}

function enumFormDataEntries(objectActor, depth) {
  const entries = [...waiveXrays(FormData.prototype.entries.call(objectActor.rawObj))];

  return {
    [Symbol.iterator]: function*() {
      for (const [key, value] of entries) {
        yield [key, value];
      }
    },
    size: entries.length,
    propertyName(index) {
      return index;
    },
    propertyDescription(index) {
      const [key, value] = entries[index];

      return {
        enumerable: true,
        value: {
          type: "formDataEntry",
          preview: {
            key: gripFromEntry(objectActor, key, depth),
            value: gripFromEntry(objectActor, value, depth),
          },
        },
      };
    },
  };
}

function enumHeadersEntries(objectActor, depth) {
  const entries = [...waiveXrays(Headers.prototype.entries.call(objectActor.rawObj))];

  return {
    [Symbol.iterator]: function*() {
      for (const [key, value] of entries) {
        yield [key, value];
      }
    },
    size: entries.length,
    propertyName(index) {
      return entries[index][0];
    },
    propertyDescription(index) {
      return {
        enumerable: true,
        value: gripFromEntry(objectActor, entries[index][1], depth),
      };
    },
  };
}

function enumHighlightRegistryEntries(objectActor, depth) {
  const entriesFuncDbgObj = objectActor.obj.getProperty("entries").return;
  const entriesDbgObj = entriesFuncDbgObj ? entriesFuncDbgObj.call(objectActor.obj).return : null;
  const entries = entriesDbgObj
    ? [...waiveXrays( entriesDbgObj.unsafeDereference())]
    : [];

  return {
    [Symbol.iterator]: function*() {
      for (const [key, value] of entries) {
        yield [key, gripFromEntry(objectActor, value, depth)];
      }
    },
    size: entries.length,
    propertyName(index) {
      return index;
    },
    propertyDescription(index) {
      const [key, value] = entries[index];
      return {
        enumerable: true,
        value: {
          type: "highlightRegistryEntry",
          preview: {
            key: key,
            value: gripFromEntry(objectActor, value, depth),
          },
        },
      };
    },
  };
}

function enumMidiInputMapEntries(objectActor, depth) {
  // We need to waive `rawObj` as we can't get the iterator from the Xray for MapLike (See Bug 1173651).
  // We also need to waive Xrays on the result of the call to `entries` as we don't have
  // Xrays to Iterator objects (see Bug 1023984)
  const entries = Array.from(
    waiveXrays(MIDIInputMap.prototype.entries.call(waiveXrays(objectActor.rawObj)))
  );

  return {
    [Symbol.iterator]: function*() {
      for (const [key, value] of entries) {
        yield [key, gripFromEntry(objectActor, value, depth)];
      }
    },
    size: entries.length,
    propertyName(index) {
      return entries[index][0];
    },
    propertyDescription(index) {
      return {
        enumerable: true,
        value: gripFromEntry(objectActor, entries[index][1], depth),
      };
    },
  };
}

function enumMidiOutputMapEntries(objectActor, depth) {
  // We need to waive `rawObj` as we can't get the iterator from the Xray for MapLike (See Bug 1173651).
  // We also need to waive Xrays on the result of the call to `entries` as we don't have
  // Xrays to Iterator objects (see Bug 1023984)
  const entries = Array.from(
    waiveXrays(MIDIOutputMap.prototype.entries.call(waiveXrays(objectActor.rawObj)))
  );

  return {
    [Symbol.iterator]: function*() {
      for (const [key, value] of entries) {
        yield [key, gripFromEntry(objectActor, value, depth)];
      }
    },
    size: entries.length,
    propertyName(index) {
      return entries[index][0];
    },
    propertyDescription(index) {
      return {
        enumerable: true,
        value: gripFromEntry(objectActor, entries[index][1], depth),
      };
    },
  };
}

function getWeakMapEntries(rawObj) {
  // We currently lack XrayWrappers for WeakMap, so when we iterate over
  // the values, the temporary iterator objects get created in the target
  // compartment. However, we _do_ have Xrays to Object now, so we end up
  // Xraying those temporary objects, and filtering access to |it.value|
  // based on whether or not it's Xrayable and/or callable, which breaks
  // the for/of iteration.
  //
  // This code is designed to handle untrusted objects, so we can safely
  // waive Xrays on the iterable, and relying on the Debugger machinery to
  // make sure we handle the resulting objects carefully.
  const keys = waiveXrays(ChromeUtils.nondeterministicGetWeakMapKeys(rawObj));

  return keys.map(k => [k, WeakMap.prototype.get.call(rawObj, k)]);
}

function enumWeakMapEntries(objectActor, depth) {
  const entries = getWeakMapEntries(objectActor.rawObj);

  return {
    [Symbol.iterator]: function*() {
      for (let i = 0; i < entries.length; i++) {
        yield entries[i].map(val => gripFromEntry(objectActor, val, depth));
      }
    },
    size: entries.length,
    propertyName(index) {
      return index;
    },
    propertyDescription(index) {
      const [key, val] = entries[index];
      return {
        enumerable: true,
        value: {
          type: "mapEntry",
          preview: {
            key: gripFromEntry(objectActor, key, depth),
            value: gripFromEntry(objectActor, val, depth),
          },
        },
      };
    },
  };
}

function getSetValues(objectActor) {
  // We currently lack XrayWrappers for Set, so when we iterate over
  // the values, the temporary iterator objects get created in the target
  // compartment. However, we _do_ have Xrays to Object now, so we end up
  // Xraying those temporary objects, and filtering access to |it.value|
  // based on whether or not it's Xrayable and/or callable, which breaks
  // the for/of iteration.
  //
  // This code is designed to handle untrusted objects, so we can safely
  // waive Xrays on the iterable, and relying on the Debugger machinery to
  // make sure we handle the resulting objects carefully.
  const iterator = objectActor.obj.makeDebuggeeValue(
    waiveXrays(Set.prototype.values.call(objectActor.rawObj))
  );
  return [...DevToolsUtils.makeDebuggeeIterator(iterator)];
}

function enumSetEntries(objectActor, depth) {
  const values = getSetValues(objectActor).map(v =>
    waiveXrays(ObjectUtils.unwrapDebuggeeValue(v))
  );

  return {
    [Symbol.iterator]: function*() {
      for (const item of values) {
        yield gripFromEntry(objectActor, item, depth);
      }
    },
    size: values.length,
    propertyName(index) {
      return index;
    },
    propertyDescription(index) {
      const val = values[index];
      return {
        enumerable: true,
        value: gripFromEntry(objectActor, val, depth),
      };
    },
  };
}

function getWeakSetEntries(rawObj) {
  // We currently lack XrayWrappers for WeakSet, so when we iterate over
  // the values, the temporary iterator objects get created in the target
  // compartment. However, we _do_ have Xrays to Object now, so we end up
  // Xraying those temporary objects, and filtering access to |it.value|
  // based on whether or not it's Xrayable and/or callable, which breaks
  // the for/of iteration.
  //
  // This code is designed to handle untrusted objects, so we can safely
  // waive Xrays on the iterable, and relying on the Debugger machinery to
  // make sure we handle the resulting objects carefully.
  return waiveXrays(ChromeUtils.nondeterministicGetWeakSetKeys(rawObj));
}

function enumWeakSetEntries(objectActor, depth) {
  const keys = getWeakSetEntries(objectActor.rawObj);

  return {
    [Symbol.iterator]: function*() {
      for (const item of keys) {
        yield gripFromEntry(objectActor, item, depth);
      }
    },
    size: keys.length,
    propertyName(index) {
      return index;
    },
    propertyDescription(index) {
      const val = keys[index];
      return {
        enumerable: true,
        value: gripFromEntry(objectActor, val, depth),
      };
    },
  };
}

function enumCustomStateSetEntries(objectActor, depth) {
  let { rawObj } = objectActor;
  // We need to waive `rawObj` as we can't get the iterator from the Xray for SetLike (See Bug 1173651).
  // We also need to waive Xrays on the result of the call to `values` as we don't have
  // Xrays to Iterator objects (see Bug 1023984)
  const values = Array.from(
    waiveXrays(CustomStateSet.prototype.values.call(waiveXrays(rawObj)))
  );

  return {
    [Symbol.iterator]: function*() {
      for (const item of values) {
        yield gripFromEntry(objectActor, item, depth);
      }
    },
    size: values.length,
    propertyName(index) {
      return index;
    },
    propertyDescription(index) {
      const val = values[index];
      return {
        enumerable: true,
        value: gripFromEntry(objectActor, val, depth),
      };
    },
  };
}

/**
 * Returns true if the parameter can be stored as a 32-bit unsigned integer.
 * If so, it will be suitable for use as the length of an array object.
 *
 * @param num Number
 *        The number to test.
 * @return Boolean
 */
function isUint32(num) {
  return num >>> 0 === num;
}

module.exports = {
  PropertyIteratorActor,
  enumCustomStateSetEntries,
  enumMapEntries,
  enumMidiInputMapEntries,
  enumMidiOutputMapEntries,
  enumSetEntries,
  enumURLSearchParamsEntries,
  enumFormDataEntries,
  enumHeadersEntries,
  enumHighlightRegistryEntries,
  enumWeakMapEntries,
  enumWeakSetEntries,
};
