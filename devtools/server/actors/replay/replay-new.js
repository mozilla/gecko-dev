/* Copyright 2020 Brian Hackett */

"use strict";

let assert,
    divergeFromRecording,
    getWindow,
    getObjectId,
    makeDebuggeeValue,
    convertValue,
    gPausedObjects,
    InspectorUtils;
function doImport(exports) {
  ({
    assert,
    divergeFromRecording,
    getWindow,
    getObjectId,
    makeDebuggeeValue,
    convertValue,
    gPausedObjects,
    InspectorUtils,
  } = exports);
}

const gDOMProperties = {
  CSSRuleList: [
    "length",
  ],
  CSSRulePrototype: [
    "cssRules",
    "media",
    "parentRule",
    "parentStyleSheet",
    "selectorText",
    "style",
    "type",
  ],
  CSSStyleSheet: [
    "href",
    "parsingMode",
    "disabled",
    "media",
    "cssRules",
    "parentStyleSheet",
    "ownerNode",
    "title",
    "sourceMapURL",
  ],
  EventTargetPrototype: [
    "ownerGlobal",
  ],
  HTMLDocument: [
    "defaultView",
    "documentElement",
    "contentType",
    "location",
    "nodePrincipal",
  ],
  Location: [
    "href",
  ],
  MediaList: [
    "mediaText",
  ],
  NodePrototype: [
    // This mixes together Node and Element stuff, because the server doesn't
    // readily distinguish between the two types.
    "nodeName",
    "nodeType",
    "nodeValue",
    "isNativeAnonymous",
    "ownerDocument",
    "containingShadowRoot",
    "baseURI",
    "parentNode",
    "childNodes",
    "isConnected",
    "parentFlexElement",
    "clientHeight",
    "scrollHeight",
    "clientWidth",
    "scrollWidth",
    "localName",
    "prefix",
    "openOrClosedShadowRoot",
    "namespaceURI",
    "attributes",
    "id",
    "style",
    "children",
    "contentDocument",
    "name",
    "publicId",
    "systemId",
    "offsetParent",
    "offsetWidth",
    "classList",
  ],
  Window: [
    "location",
    "frameElement",
    "windowUtils",
    "scrollX",
    "scrollY",
  ],
  XPCWrappedNative_NoHelper: [
    // WindowUtils
    "fullZoom",

    // Principal
    "isSystemPrincipal",
  ],
};

const gPseudoKinds = [
  "",
  ":before",
  ":after",
  ":marker",
  ":backdrop",
  ":cue",
  ":first-letter",
  ":first-line",
  ":selection",
  ":placeholder",
];

const gBoxKinds = ["border", "content", "margin", "padding"];

// IDs of any DOM nodes that have already been sent up to the middleman.
const gUploadedDOMNodes = new Set();

let gDOMData;

function addObject(obj, options) {
  const dbgObj = makeDebuggeeValue(obj);
  const id = getObjectId(dbgObj);

  if (gUploadedDOMNodes.has(id)) {
    return;
  }
  gUploadedDOMNodes.add(id);

  const data = { id, className: dbgObj.class };
  gDOMData[id] = data;

  assert(!isArrayLike(data.className));
  addObjectProperties(data, obj, dbgObj, options);
  return id;
}

function addObjectId(id, options) {
  return addObject(gPausedObjects.getObject(id).unsafeDereference(), options);
}

function addObjectProperties(data, obj, dbgObj, options) {
  data.properties = {};

  const classNames = [];
  for (let nobj = dbgObj; nobj; nobj = nobj.proto) {
    classNames.push(nobj.class);
  }

  //dump(`ObjectClassNames: ${data.id} ${classNames}\n`);

  for (const className of classNames) {
    const properties = gDOMProperties[className];
    if (properties) {
      for (const name of properties) {
        data.properties[name] = addValue(obj[name], options);
      }
    }

    switch (className) {
      case "HTMLDocument": {
        const sheets = InspectorUtils.getAllStyleSheets(obj, true);
        if (sheets) {
          data.styleSheets = addValue(sheets, options);
        }
        break;
      }
      case "ElementPrototype":
        addElementData(data, obj, options);
        break;
      case "NodePrototype":
        addBoxQuads(data, obj, "border", undefined, false);
        for (const box of gBoxKinds) {
          addBoxQuads(data, obj, box, getWindow().document, false);
        }
        break;
      case "CSSRuleList":
        data._items = [];
        for (let i = 0; i < obj.length; i++) {
          data._items[i] = addValue(obj.item(i), options);
        }
        break;
      case "CSSRulePrototype":
        data._ruleLine = InspectorUtils.getRuleLine(obj);
        data._ruleColumn = InspectorUtils.getRuleColumn(obj);
        data._ruleRelativeLine = InspectorUtils.getRelativeRuleLine(obj);
        break;
      case "CSSStyleRule":
        data._ruleSelectorCount = InspectorUtils.getSelectorCount(obj);
        data._ruleSelectors = [];
        for (let i = 0; i < data._ruleSelectorCount; i++) {
          data._ruleSelectors[i] = InspectorUtils.getSelectorText(obj, i);
        }
        break;
      case "CSSStyleSheet":
        data._hasRulesModifiedByCSSOM = InspectorUtils.hasRulesModifiedByCSSOM(obj);
        break;
    }
  }

  if (CSSRule.isInstance(obj)) {
    data.isInstance = "CSSRule";
  } else if (Event.isInstance(obj)) {
    data.isInstance = "Event";
  }
}

function addElementData(data, obj, options) {
  for (const pseudo of gPseudoKinds) {
    addStyleInfo(data, obj, pseudo, options);
  }
  if (obj.getBoundingClientRect) {
    data.boundingClientRect = rectToJSON(obj.getBoundingClientRect());
  }
  data.contentState = InspectorUtils.getContentState(obj);
  data.styleAttribute = obj.getAttribute("style");
}

function addStyleInfo(data, obj, pseudo, options) {
  const rules = InspectorUtils.getCSSStyleRules(obj, pseudo);
  if (!data.styleRules) {
    data.styleRules = {};
  }
  data.styleRules[pseudo] = addValue(rules, options);

  if (!data.matchedSelectors) {
    data.matchedSelectors = {};
  }
  for (const rule of rules) {
    const ruleId = getObjectId(makeDebuggeeValue(rule));
    const count = InspectorUtils.getSelectorCount(rule);
    for (let i = 0; i < count; i++) {
      const includeVisitedStyle = false;
      const key = `${ruleId}:${i}:${pseudo}:${includeVisitedStyle}`;
      data.matchedSelectors[key] = InspectorUtils.selectorMatchesElement(
        obj,
        rule,
        i,
        pseudo,
        includeVisitedStyle
      );
    }
  }

  const style = obj.ownerGlobal.getComputedStyle(obj, pseudo);
  if (!data.computedStyles) {
    data.computedStyles = {};
  }
  data.computedStyles[pseudo] = addValue(style, { ...options, rules });
}

function addBoxQuads(data, obj, box, relativeTo, createFramesForSuppressedWhitespace) {
  if (!obj.getBoxQuads) {
    return;
  }
  const relativeId = getObjectId(makeDebuggeeValue(relativeTo));
  const key = `${box}:${relativeId}:${createFramesForSuppressedWhitespace}`;
  const quads = obj.getBoxQuads({ box, relativeTo, createFramesForSuppressedWhitespace });
  if (!data.boxQuads) {
    data.boxQuads = {};
  }
  data.boxQuads[key] = quads.map(quadToJSON);
}

function addValue(value, options) {
  const converted = convertDebuggeeValue(value, options);
  if (isNonNullObject(converted)) {
    options = { ...options, depth: options.depth - 1};
    if (converted instanceof Array) {
      converted.forEach(v => {
        if (v && v.object) {
          if (options.depth) {
            addObjectId(v.object, options);
          }
        }
      });
    } else if (converted.object) {
      addObjectId(converted.object, options);
    }
  }
  return converted;
}

function rectToJSON({ x, y, width, height, top, right, bottom, left }) {
  return { x, y, width, height, top, right, bottom, left };
}

function quadToJSON(quad) {
  const { p1, p2, p3, p4 } = quad;
  return {
    p1: pointToJSON(p1),
    p2: pointToJSON(p2),
    p3: pointToJSON(p3),
    p4: pointToJSON(p4),
    _bounds: rectToJSON(quad.getBounds()),
  };
}

function pointToJSON({ x, y, z, w }) {
  return { x, y, z, w };
}

function isArrayLike(className) {
  switch (className) {
  case "Array":
  case "NodeList":
  case "HTMLCollection":
  case "DOMTokenList":
    return true;
  }
  return false;
}

function isStyleDeclaration(className) {
  return className == "CSS2Properties";
}

function objectPropertyApply({ thisId, name, args }) {
  const object = gPausedObjects.getObject(thisId);
  try {
    const rv = object.unsafeDereference()[name](...args);
    return convertDebuggeeValue(rv, {});
  } catch (e) {
    return convertDebuggeeValue(e, {});
  }
}

function convertDebuggeeValue(value, options) {
  const dbgValue = makeDebuggeeValue(value);

  if (isNonNullObject(dbgValue)) {
    if (isArrayLike(dbgValue.class)) {
      const rv = [];
      for (const v of value) {
        const nv = convertDebuggeeValue(v, options);
        rv.push(nv);
      }
      return rv;
    }
    if (isStyleDeclaration(dbgValue.class)) {
      const names = new Set();
      if ("rules" in options) {
        // Only include properties mentioned in one of the rules. Otherwise
        // the style might include every possible CSS property.
        for (const { style } of options.rules) {
          for (const name of style) {
            names.add(name);
          }
        }
      } else {
        for (const name of value) {
          names.add(name);
        }
      }

      const data = { cssText: value.cssText, properties: [] };
      for (const name of names) {
        data.properties.push({
          name,
          value: value.getPropertyValue(name),
          priority: value.getPropertyPriority(name),
        });
      }
      return data;
    }
  }

  return convertValue(dbgValue);
}

function dumpJSON(obj, name = "root", indent = 0) {
  const len = JSON.stringify(obj).length;
  dump(`${" ".repeat(indent)}${name}: ${len}\n`);
  if (len >= 100) {
    Object.entries(obj).forEach(([name, value]) => {
      if (isNonNullObject(value)) {
        dumpJSON(value, name, indent + 2);
      }
    });
  }
}

const gDefaultDepth = 4;

const ReplayNew = {
  doImport,

  requestHandlers: {
    getDOM() {
      divergeFromRecording();
      const window = getWindow();

      gDOMData = {};
      gDOMData.window = addObject(window, { depth: gDefaultDepth });
      gDOMData.document = addObject(window.document, { depth: gDefaultDepth });

      return gDOMData;
    },

    growDOM({ ids }) {
      divergeFromRecording();

      gDOMData = {};
      ids.forEach(id => addObjectId(id, { depth: gDefaultDepth }));
      return gDOMData;
    },

    objectPropertyApply,
  },
};

function isNonNullObject(obj) {
  return obj && (typeof obj == "object" || typeof obj == "function");
}

var EXPORTED_SYMBOLS = ["ReplayNew"];
