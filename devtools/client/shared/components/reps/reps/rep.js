/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

// Make this available to both AMD and CJS environments
define(function (require, exports, module) {
  // Load all existing rep templates
  const Undefined = require("resource://devtools/client/shared/components/reps/reps/undefined.js");
  const Null = require("resource://devtools/client/shared/components/reps/reps/null.js");
  const StringRep = require("resource://devtools/client/shared/components/reps/reps/string.js");
  const Number = require("resource://devtools/client/shared/components/reps/reps/number.js");
  const ArrayRep = require("resource://devtools/client/shared/components/reps/reps/array.js");
  const Obj = require("resource://devtools/client/shared/components/reps/reps/object.js");
  const SymbolRep = require("resource://devtools/client/shared/components/reps/reps/symbol.js");
  const InfinityRep = require("resource://devtools/client/shared/components/reps/reps/infinity.js");
  const NaNRep = require("resource://devtools/client/shared/components/reps/reps/nan.js");
  const Accessor = require("resource://devtools/client/shared/components/reps/reps/accessor.js");

  // DOM types (grips)
  const Accessible = require("resource://devtools/client/shared/components/reps/reps/accessible.js");
  const Attribute = require("resource://devtools/client/shared/components/reps/reps/attribute.js");
  const BigInt = require("resource://devtools/client/shared/components/reps/reps/big-int.js");
  const DateTime = require("resource://devtools/client/shared/components/reps/reps/date-time.js");
  const Document = require("resource://devtools/client/shared/components/reps/reps/document.js");
  const DocumentType = require("resource://devtools/client/shared/components/reps/reps/document-type.js");
  const Event = require("resource://devtools/client/shared/components/reps/reps/event.js");
  const Func = require("resource://devtools/client/shared/components/reps/reps/function.js");
  const PromiseRep = require("resource://devtools/client/shared/components/reps/reps/promise.js");
  const RegExp = require("resource://devtools/client/shared/components/reps/reps/regexp.js");
  const StyleSheet = require("resource://devtools/client/shared/components/reps/reps/stylesheet.js");
  const CommentNode = require("resource://devtools/client/shared/components/reps/reps/comment-node.js");
  const ElementNode = require("resource://devtools/client/shared/components/reps/reps/element-node.js");
  const TextNode = require("resource://devtools/client/shared/components/reps/reps/text-node.js");
  const ErrorRep = require("resource://devtools/client/shared/components/reps/reps/error.js");
  const Window = require("resource://devtools/client/shared/components/reps/reps/window.js");
  const ObjectWithText = require("resource://devtools/client/shared/components/reps/reps/object-with-text.js");
  const ObjectWithURL = require("resource://devtools/client/shared/components/reps/reps/object-with-url.js");
  const GripArray = require("resource://devtools/client/shared/components/reps/reps/grip-array.js");
  const GripEntry = require("resource://devtools/client/shared/components/reps/reps/grip-entry.js");
  const GripMap = require("resource://devtools/client/shared/components/reps/reps/grip-map.js");
  const Grip = require("resource://devtools/client/shared/components/reps/reps/grip.js");

  // List of all registered template.
  // XXX there should be a way for extensions to register a new
  // or modify an existing rep.
  const reps = [
    RegExp,
    StyleSheet,
    Event,
    DateTime,
    CommentNode,
    Accessible,
    ElementNode,
    TextNode,
    Attribute,
    Func,
    PromiseRep,
    Document,
    DocumentType,
    Window,
    ObjectWithText,
    ObjectWithURL,
    ErrorRep,
    GripArray,
    GripMap,
    GripEntry,
    Grip,
    Undefined,
    Null,
    StringRep,
    Number,
    BigInt,
    SymbolRep,
    InfinityRep,
    NaNRep,
    Accessor,
  ];

  // Reps for rendering of native object reference (e.g. used from the JSONViewer, Netmonitor, …)
  const noGripReps = [StringRep, Number, ArrayRep, Undefined, Null, Obj];

  /**
   * Generic rep that is used for rendering native JS types or an object.
   * The right template used for rendering is picked automatically according
   * to the current value type. The value must be passed in as the 'object'
   * property.
   */
  const Rep = function (props) {
    const { object, defaultRep } = props;
    const rep = getRep(
      object,
      defaultRep,
      props.noGrip,
      props.mayUseCustomFormatter
    );
    return rep(props);
  };

  const exportedReps = {
    Accessible,
    Accessor,
    ArrayRep,
    Attribute,
    BigInt,
    CommentNode,
    DateTime,
    Document,
    DocumentType,
    ElementNode,
    ErrorRep,
    Event,
    Func,
    Grip,
    GripArray,
    GripMap,
    GripEntry,
    InfinityRep,
    NaNRep,
    Null,
    Number,
    Obj,
    ObjectWithText,
    ObjectWithURL,
    PromiseRep,
    RegExp,
    Rep,
    StringRep,
    StyleSheet,
    SymbolRep,
    TextNode,
    Undefined,
    Window,
  };

  // Custom Formatters
  // Services.prefs isn't available in jsonviewer. It doesn't matter as we don't want to use
  // custom formatters there
  if (typeof Services == "object" && Services?.prefs) {
    const useCustomFormatters = Services.prefs.getBoolPref(
      "devtools.custom-formatters.enabled",
      false
    );

    if (useCustomFormatters) {
      const CustomFormatter = require("resource://devtools/client/shared/components/reps/reps/custom-formatter.js");
      reps.unshift(CustomFormatter);
      exportedReps.CustomFormatter = CustomFormatter;
    }
  }

  // Helpers

  /**
   * Return a rep object that is responsible for rendering given
   * object.
   *
   * @param object {Object} Object to be rendered in the UI. This
   * can be generic JS object as well as a grip (handle to a remote
   * debuggee object).
   *
   * @param defaultRep {React.Component} The default template
   * that should be used to render given object if none is found.
   *
   * @param noGrip {Boolean} If true, will only check reps not made for remote
   *                         objects.
   *
   * @param mayUseCustomFormatter {Boolean} If true, custom formatters are
   * allowed to be used as rep.
   */
  function getRep(
    object,
    defaultRep = Grip,
    noGrip = false,
    mayUseCustomFormatter = false
  ) {
    const repsList = noGrip ? noGripReps : reps;
    for (const rep of repsList) {
      if (rep === exportedReps.CustomFormatter && !mayUseCustomFormatter) {
        continue;
      }

      try {
        // supportsObject could return weight (not only true/false
        // but a number), which would allow to priorities templates and
        // support better extensibility.
        if (rep.supportsObject(object, noGrip)) {
          return rep.rep;
        }
      } catch (err) {
        console.error(err);
      }
    }

    return defaultRep.rep;
  }

  module.exports = {
    Rep,
    REPS: exportedReps,
    // Exporting for tests
    getRep,
  };
});
