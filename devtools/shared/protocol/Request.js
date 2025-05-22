/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { extend } = require("resource://devtools/shared/extend.js");
var {
  findPlaceholders,
  getPath,
} = require("resource://devtools/shared/protocol/utils.js");
var {
  types,
  BULK_REQUEST,
} = require("resource://devtools/shared/protocol/types.js");

/**
 * Manages a request template.
 *
 * @param string type
 *    The type defined in the specification for this request.
 *    For methods, it will be the attribute name in "methods" dictionary.
 *    For events, it will be the attribute name in "events" dictionary.
 * @param object template
 *    The request template.
 * @construcor
 */
var Request = function (type, template = {}) {
  // The EventEmitter event name (this.type, attribute name in the event specification file) emitted on the Actor/Front,
  // may be different from the RDP JSON packet event name (ret[type], type attribute value in the event specification file)
  // In the specification:
  //   "my-event": { // <= EventEmitter name
  //     type: "myEvent", // <= RDP packet type attribute
  //     ...
  //   }
  this.type = template.type || type;

  this.template = template;
  this.args = findPlaceholders(template, Arg);
};

Request.prototype = {
  /**
   * Write a request.
   *
   * @param array fnArgs
   *    The function arguments to place in the request.
   * @param object ctx
   *    The object making the request.
   * @returns a request packet.
   */
  write(fnArgs, ctx) {
    // Bulk request can't send custom attributes/custom JSON packet.
    // Only communicate "type" and "length" attributes to the transport layer,
    // which will emit a JSON RDP packet with an additional "actor attribute.
    if (this.template === BULK_REQUEST) {
      // The Front's method is expected to be called with a unique object argument
      // with a "length" attribute, which refers to the total size of bytes to be
      // sent via a the bulk StreamCopier.
      if (typeof fnArgs[0].length != "number") {
        throw new Error(
          "This front's method is expected to send a bulk request and should be called with an object argument with a length attribute."
        );
      }
      return { type: this.type, length: fnArgs[0].length };
    }

    const ret = {
      type: this.type,
    };
    for (const key in this.template) {
      const value = this.template[key];
      if (value instanceof Arg || value instanceof Option) {
        ret[key] = value.write(
          value.index in fnArgs ? fnArgs[value.index] : undefined,
          ctx,
          key
        );
      } else if (key == "type") {
        // Ignore the type attribute which have already been considered in the constructor.
        continue;
      } else {
        throw new Error(
          "Request can only an object with `Arg` or `Option` properties"
        );
      }
    }
    return ret;
  },

  /**
   * Read a request.
   *
   * @param object packet
   *    The request packet.
   * @param object ctx
   *    The object making the request.
   * @returns an arguments array
   */
  read(packet, ctx) {
    if (this.template === BULK_REQUEST) {
      // The transport layer will convey a custom packet object with length, copyTo and copyToBuffer,
      // which we transfer to the Actor's method via a unique object argument.
      // This help know about the incoming data size and read the binary buffer via `copyTo`
      // or `copyToBuffer`.
      return [
        {
          length: packet.length,
          copyTo: packet.copyTo,
          copyToBuffer: packet.copyToBuffer,
        },
      ];
    }

    const fnArgs = [];
    for (const templateArg of this.args) {
      const arg = templateArg.placeholder;
      const path = templateArg.path;
      const name = path[path.length - 1];
      arg.read(getPath(packet, path), ctx, fnArgs, name);
    }
    return fnArgs;
  },
};

exports.Request = Request;

/**
 * Request/Response templates and generation
 *
 * Request packets are specified as json templates with
 * Arg and Option placeholders where arguments should be
 * placed.
 *
 * Reponse packets are also specified as json templates,
 * with a RetVal placeholder where the return value should be
 * placed.
 */

/**
 * Placeholder for simple arguments.
 *
 * @param number index
 *    The argument index to place at this position.
 * @param type type
 *    The argument should be marshalled as this type.
 * @constructor
 */
var Arg = function (index, type) {
  this.index = index;
  // Prevent force loading all Arg types by accessing it only when needed
  loader.lazyGetter(this, "type", function () {
    return types.getType(type);
  });
};

Arg.prototype = {
  write(arg, ctx) {
    return this.type.write(arg, ctx);
  },

  read(v, ctx, outArgs) {
    outArgs[this.index] = this.type.read(v, ctx);
  },
};

// Outside of protocol.js, Arg is called as factory method, without the new keyword.
exports.Arg = function (index, type) {
  return new Arg(index, type);
};

/**
 * Placeholder for an options argument value that should be hoisted
 * into the packet.
 *
 * If provided in a method specification:
 *
 *   { optionArg: Option(1)}
 *
 * Then arguments[1].optionArg will be placed in the packet in this
 * value's place.
 *
 * @param number index
 *    The argument index of the options value.
 * @param type type
 *    The argument should be marshalled as this type.
 * @constructor
 */
var Option = function (index, type) {
  Arg.call(this, index, type);
};

Option.prototype = extend(Arg.prototype, {
  write(arg, ctx, name) {
    // Ignore if arg is undefined or null; allow other falsy values
    if (arg == undefined || arg[name] == undefined) {
      return undefined;
    }
    const v = arg[name];
    return this.type.write(v, ctx);
  },
  read(v, ctx, outArgs, name) {
    if (outArgs[this.index] === undefined) {
      outArgs[this.index] = {};
    }
    if (v === undefined) {
      return;
    }
    outArgs[this.index][name] = this.type.read(v, ctx);
  },
});

// Outside of protocol.js, Option is called as factory method, without the new keyword.
exports.Option = function (index, type) {
  return new Option(index, type);
};
