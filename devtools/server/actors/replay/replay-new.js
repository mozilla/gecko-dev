/* Copyright 2020 Brian Hackett */

"use strict";

const gDOMProperties = {
  CSSRuleList: [
    "length",
  ],
  CSSStyleRule: [
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

function getDOM(outer) {
  outer.divergeFromRecording();

  var rv = {};
  let pending = [];

  const window = outer.getWindow();
  rv.window = addObject(window);
  rv.document = addObject(window.document);

  const walker = Cc["@mozilla.org/inspector/deep-tree-walker;1"].createInstance(
    Ci.inIDeepTreeWalker
  );

  walker.showAnonymousContent = true;
  walker.showSubDocuments = true;
  walker.showDocumentsAsNodes = true;
  walker.init(window.document, 0xffffffff);

  while (true) {
    const node = walker.nextNode();
    if (!node) {
      break;
    }
    addObject(node);
    walker.currentNode = node;
  }

  while (pending.length) {
    addObject(pending.pop());
  }

  return rv;

  function addObject(obj) {
    const dbgObj = outer.makeDebuggeeValue(obj);
    const id = outer.getObjectId(dbgObj);
    if (rv[id]) {
      return;
    }

    const data = { id, className: dbgObj.class };
    rv[id] = data;

    outer.assert(!isArrayLike(data.className));
    addObjectProperties(data, obj, dbgObj);
    return id;
  }

  function addObjectId(id) {
    return addObject(outer.gPausedObjects.getObject(id).unsafeDereference());
  }

  function addObjectProperties(data, obj, dbgObj) {
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
          data.properties[name] = addValue(obj[name]);
        }
      }

      switch (className) {
        case "HTMLDocument": {
          const sheets = outer.InspectorUtils.getAllStyleSheets(obj, true);
          if (sheets) {
            data.styleSheets = addValue(sheets);
          }
          break;
        }
        case "ElementPrototype":
          addElementData(data, obj);
          break;
        case "NodePrototype":
          addBoxQuads(data, obj, "border", undefined, false);
          for (const box of gBoxKinds) {
            addBoxQuads(data, obj, box, window.document, false);
          }
          break;
        case "CSSRuleList":
          data._items = [];
          for (let i = 0; i < obj.length; i++) {
            data._items[i] = addValue(obj.item(i));
          }
          break;
        case "CSSStyleRule":
          data._ruleLine = outer.InspectorUtils.getRuleLine(obj);
          data._ruleColumn = outer.InspectorUtils.getRuleColumn(obj);
          data._ruleRelativeLine = outer.InspectorUtils.getRelativeRuleLine(obj);
          data._ruleSelectorCount = outer.InspectorUtils.getSelectorCount(obj);
          data._ruleSelectors = [];
          for (let i = 0; i < data._ruleSelectorCount; i++) {
            data._ruleSelectors[i] = outer.InspectorUtils.getSelectorText(obj, i);
          }
          break;
        case "CSSStyleSheet":
          data._hasRulesModifiedByCSSOM = outer.InspectorUtils.hasRulesModifiedByCSSOM(obj);
          break;
      }
    }

    if (CSSRule.isInstance(obj)) {
      data.isInstance = "CSSRule";
    } else if (Event.isInstance(obj)) {
      data.isInstance = "Event";
    }
  }

  function addElementData(data, obj) {
    for (const pseudo of gPseudoKinds) {
      addStyleInfo(data, obj, pseudo);
    }
    if (obj.getBoundingClientRect) {
      data.boundingClientRect = rectToJSON(obj.getBoundingClientRect());
    }
    data.contentState = outer.InspectorUtils.getContentState(obj);
    data.styleAttribute = obj.getAttribute("style");
  }

  function addStyleInfo(data, obj, pseudo) {
    const rules = outer.InspectorUtils.getCSSStyleRules(obj, pseudo);
    if (!data.styleRules) {
      data.styleRules = {};
    }
    data.styleRules[pseudo] = addValue(rules);

    if (!data.matchedSelectors) {
      data.matchedSelectors = {};
    }
    for (const rule of rules) {
      const ruleId = outer.getObjectId(outer.makeDebuggeeValue(rule));
      const count = outer.InspectorUtils.getSelectorCount(rule);
      for (let i = 0; i < count; i++) {
        const includeVisitedStyle = false;
        const key = `${ruleId}:${i}:${pseudo}:${includeVisitedStyle}`;
        data.matchedSelectors[key] = outer.InspectorUtils.selectorMatchesElement(
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
    data.computedStyles[pseudo] = addValue(style, { rules });
  }

  function addBoxQuads(data, obj, box, relativeTo, createFramesForSuppressedWhitespace) {
    const relativeId = outer.getObjectId(outer.makeDebuggeeValue(relativeTo));
    const key = `${box}:${relativeId}:${createFramesForSuppressedWhitespace}`;
    const quads = obj.getBoxQuads({ box, relativeTo, createFramesForSuppressedWhitespace });
    if (!data.boxQuads) {
      data.boxQuads = {};
    }
    data.boxQuads[key] = quads.map(quadToJSON);
  }

  function addValue(value, options) {
    const converted = convertDebuggeeValue(outer, value, options);
    if (isNonNullObject(converted)) {
      if (converted instanceof Array) {
        converted.forEach(v => {
          if (v && v.object) {
            addObjectId(v.object);
          }
        });
      } else if (converted.object) {
        addObjectId(converted.object);
      }
    }
    return converted;
  }
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

function objectPropertyApply(outer, { thisId, name, args }) {
  const object = outer.gPausedObjects.getObject(thisId);
  try {
    const rv = object.unsafeDereference()[name](...args);
    return convertDebuggeeValue(outer, rv);
  } catch (e) {
    return convertDebuggeeValue(outer, e);
  }
}

function convertDebuggeeValue(outer, value, options) {
  const dbgValue = outer.makeDebuggeeValue(value);

  if (isNonNullObject(dbgValue)) {
    if (isArrayLike(dbgValue.class)) {
      const rv = [];
      for (const v of value) {
        const nv = convertDebuggeeValue(outer, v);
        rv.push(nv);
      }
      return rv;
    }
    if (isStyleDeclaration(dbgValue.class)) {
      const names = new Set();
      if (options && "rules" in options) {
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

  return outer.convertValue(dbgValue);
}

const ReplayNew = {
  requestHandlers: {
    getDOM,
    objectPropertyApply,
  },
};

function isNonNullObject(obj) {
  return obj && (typeof obj == "object" || typeof obj == "function");
}

var EXPORTED_SYMBOLS = ["ReplayNew"];
