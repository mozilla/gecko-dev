/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import * as Undefined from "resource://devtools/client/shared/components/reps/reps/undefined.mjs";
import * as Null from "resource://devtools/client/shared/components/reps/reps/null.mjs";
import * as StringRep from "resource://devtools/client/shared/components/reps/reps/string.mjs";
import * as NumberRep from "resource://devtools/client/shared/components/reps/reps/number.mjs";
import * as JsonNumber from "resource://devtools/client/shared/components/reps/reps/json-number.mjs";
import * as ArrayRep from "resource://devtools/client/shared/components/reps/reps/array.mjs";
import * as Obj from "resource://devtools/client/shared/components/reps/reps/object.mjs";
import * as SymbolRep from "resource://devtools/client/shared/components/reps/reps/symbol.mjs";
import * as InfinityRep from "resource://devtools/client/shared/components/reps/reps/infinity.mjs";
import * as NaNRep from "resource://devtools/client/shared/components/reps/reps/nan.mjs";
import * as Accessor from "resource://devtools/client/shared/components/reps/reps/accessor.mjs";
import * as Accessible from "resource://devtools/client/shared/components/reps/reps/accessible.mjs";
import * as Attribute from "resource://devtools/client/shared/components/reps/reps/attribute.mjs";
import * as BigIntRep from "resource://devtools/client/shared/components/reps/reps/big-int.mjs";
import * as DateTime from "resource://devtools/client/shared/components/reps/reps/date-time.mjs";
import * as DocumentRep from "resource://devtools/client/shared/components/reps/reps/document.mjs";
import * as DocumentTypeRep from "resource://devtools/client/shared/components/reps/reps/document-type.mjs";
import * as EventRep from "resource://devtools/client/shared/components/reps/reps/event.mjs";
import * as Func from "resource://devtools/client/shared/components/reps/reps/function.mjs";
import * as PromiseRep from "resource://devtools/client/shared/components/reps/reps/promise.mjs";
import * as RegExpRep from "resource://devtools/client/shared/components/reps/reps/regexp.mjs";
import * as StyleSheetRep from "resource://devtools/client/shared/components/reps/reps/stylesheet.mjs";
import * as CommentNode from "resource://devtools/client/shared/components/reps/reps/comment-node.mjs";
import * as ElementNode from "resource://devtools/client/shared/components/reps/reps/element-node.mjs";
import * as TextNode from "resource://devtools/client/shared/components/reps/reps/text-node.mjs";
import * as ErrorRep from "resource://devtools/client/shared/components/reps/reps/error.mjs";
import * as WindowRep from "resource://devtools/client/shared/components/reps/reps/window.mjs";
import * as ObjectWithText from "resource://devtools/client/shared/components/reps/reps/object-with-text.mjs";
import * as ObjectWithURL from "resource://devtools/client/shared/components/reps/reps/object-with-url.mjs";
import * as GripArray from "resource://devtools/client/shared/components/reps/reps/grip-array.mjs";
import * as GripEntry from "resource://devtools/client/shared/components/reps/reps/grip-entry.mjs";
import * as GripMap from "resource://devtools/client/shared/components/reps/reps/grip-map.mjs";
import * as Grip from "resource://devtools/client/shared/components/reps/reps/grip.mjs";
import * as CustomFormatter from "resource://devtools/client/shared/components/reps/reps/custom-formatter.mjs";

// List of all registered template.
// XXX there should be a way for extensions to register a new
// or modify an existing rep.
const reps = [
  RegExpRep,
  StyleSheetRep,
  EventRep,
  DateTime,
  CommentNode,
  Accessible,
  ElementNode,
  TextNode,
  Attribute,
  Func,
  PromiseRep,
  DocumentRep,
  DocumentTypeRep,
  WindowRep,
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
  NumberRep,
  BigIntRep,
  SymbolRep,
  InfinityRep,
  NaNRep,
  Accessor,
];

// Reps for rendering of native object reference (e.g. used from the JSONViewer, Netmonitor, …)
const noGripReps = [
  StringRep,
  JsonNumber,
  NumberRep,
  ArrayRep,
  Undefined,
  Null,
  Obj,
];

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
  BigInt: BigIntRep,
  CommentNode,
  DateTime,
  Document: DocumentRep,
  DocumentType: DocumentTypeRep,
  ElementNode,
  ErrorRep,
  Event: EventRep,
  Func,
  Grip,
  GripArray,
  GripMap,
  GripEntry,
  InfinityRep,
  NaNRep,
  Null,
  Number: NumberRep,
  Obj,
  ObjectWithText,
  ObjectWithURL,
  PromiseRep,
  RegExp: RegExpRep,
  Rep,
  StringRep,
  StyleSheet: StyleSheetRep,
  SymbolRep,
  TextNode,
  Undefined,
  WindowRep,
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

export { Rep, exportedReps as REPS, getRep };
