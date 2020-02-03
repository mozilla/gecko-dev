/* Copyright 2020 Brian Hackett */

"use strict";

const ReplayDebugger = require("devtools/server/actors/replay/debugger");

let _dbg = null;
function dbg() {
  if (!_dbg) {
    _dbg = new ReplayDebugger();
  }
  return _dbg;
}

const BaseProxyHandler = {
  getPrototypeOf() { NotAllowed(); },
  has() { NotAllowed(); },
  get() { NotAllowed(); },
  set() { NotAllowed();},
  apply() { NotAllowed(); },
  construct() { NotAllowed(); },
  getOwnPropertyDescriptor() { NotAllowed(); },
  ownKeys() { NotAllowed(); },
  isExtensible() { NotAllowed(); },
  setPrototypeOf() { NotAllowed(); },
  preventExtensions() { NotAllowed(); },
  defineProperty() { NotAllowed(); },
  deleteProperty() { NotAllowed(); },
};

function objectPropertyApply(thisId, name, args) {
  assert(dbg()._paused);
  const rv = dbg()._sendRequestAllowDiverge({
    type: "objectPropertyApply",
    thisId,
    name,
    args,
  });
  return convertValue(rv);
}

const ProxyHandler = {
  ...BaseProxyHandler,

  getPrototypeOf(target) {
    return null;
  },

  get(target, name, receiver) {
    let data;
    if (target.fixedProxyKind) {
      if (typeof name == "symbol") {
        if (name == Symbol.toStringTag) {
          return `FixedReplayInspectorProxy ${target.kind}`;
        }
        return undefined;
      }

      switch (`${target.fixedProxyKind}.${name}`) {
      case "window.document": return ReplayInspector.document;
      case "window.removeEventListener": return () => {};
      case "document.readyState": return "complete";
      }

      const id = DOM()[target.fixedProxyKind];
      data = DOM()[id];
    } else {
      data = target;
    }

    const { id, className, properties } = data;

    if (typeof name == "symbol") {
      if (name == Symbol.toStringTag) {
        return `ReplayInspectorProxy #${id} ${className}`;
      }
      if (name == Symbol.iterator && className == "CSSRuleList") {
        const items = data._items.map(convertValue);
        return items[Symbol.iterator].bind(items);
      }
      return undefined;
    }

    if (name == "toString") {
      return () => `ReplayInspectorProxy #${id} ${className}`;
    }

    if (properties && name in properties) {
      return convertValue(properties[name]);
    }

    const proxy = gIdToProxyMap.get(id);

    switch (name) {
      case "getSVGDocument":
        return null; // NYI
      case "getGridFragments":
        return null; // NYI
      case "getAsFlexContainer":
        return null; // NYI
      case "getBoxQuads":
        return ({
          box = "border",
          relativeTo,
          createFramesForSuppressedWhitespace = true,
        }) => {
          const relativeId = relativeTo ? getProxyData(relativeTo).id : 0;
          const key = `${box}:${relativeId}:${createFramesForSuppressedWhitespace}`;
          if (data.boxQuads && key in data.boxQuads) {
            return data.boxQuads[key].map(quad => ({
              ...quad,
              getBounds() { return quad._bounds; },
            }));
          }
          ThrowError(`Missing box quads for ${proxy} ${key}`);
        };
      case "contains":
        return node => {
          while (node) {
            if (node == proxy) {
              return true;
            }
            node = node.parentNode;
          }
          return false;
        };
      case "rawNode":
        // For server code that can operate on either DOM nodes or node actors.
        return undefined;
      case "querySelector":
        return sel => objectPropertyApply(id, "querySelector", [sel]);
      case "querySelectorAll":
        return sel => objectPropertyApply(id, "querySelectorAll", [sel]);
      case "getBoundingClientRect":
        return () => data.boundingClientRect;
      case "getAttribute":
        return name => {
          if (name == "style") {
            return data.styleAttribute;
          }
          NYI();
        };
      case "scrollIntoView":
        return () => {};
    }

    const key = `${className}.${name}`;
    switch (key) {
      case "CSSRuleList.item":
        return n => convertValue(data._items[n]);
      case "CSSRuleList.then":
        // For Promise.resolve(rules)
        return undefined;
      case "HTMLHtmlElement.getElementsWithGrid":
        return () => []; // NYI
      case "Window.customElements":
        return null; // NYI
      case "Window.getComputedStyle":
        return (element, pseudo) => {
          pseudo = pseudo || "";
          const { computedStyles } = getProxyData(element);
          if (!computedStyles) {
            return new Proxy(
              { cssText: "", properties: [] },
              StyleDeclarationProxyHandler
            );
          }
          if (computedStyles && pseudo in computedStyles) {
            return convertValue(computedStyles[pseudo]);
          }
          ThrowError(`Missing computed style for ${element} "${pseudo}"`);
        };
      case "Window.HTMLTemplateElement":
      case "Window.CSS":
        return getWindow()[name];
    }

    if (className == "CSSRuleList") {
      const num = parseInt(name);
      if (!Number.isNaN(num)) {
        return convertValue(data._items[num]);
      }
    }

    if (name.endsWith("_NODE") && Node[name]) {
      return Node[name];
    }

    dump(`ProxyHandler.get NYI ${key}\n`);
    NYI();
  },
};

let _DOM = null;
function DOM() {
  if (!_DOM) {
    assert(dbg()._paused);
    _DOM = dbg()._sendRequestAllowDiverge({ type: "getDOM" });

    mapFixedProxy("window");
    mapFixedProxy("document");
  }
  return _DOM;
}

const gIdToProxyMap = new Map();
const gProxyToIdMap = new Map();

function invalidateAfterUnpause() {
  _DOM = null;
  gIdToProxyMap.clear();
  gProxyToIdMap.clear();
}

function mapProxy(id, proxy) {
  assert(id);
  gIdToProxyMap.set(id, proxy);
  gProxyToIdMap.set(proxy, id);
}

function mapFixedProxy(name) {
  mapProxy(_DOM[name], ReplayInspector[name]);
}

function newFixedProxy(name) {
  return new Proxy({ fixedProxyKind: name }, ProxyHandler);
}

function ensureProxy(id) {
  assert(typeof id == "number");
  if (!gIdToProxyMap.has(id)) {
    const data = DOM()[id];
    assert(data);
    const proxy = new Proxy(data, ProxyHandler);
    mapProxy(id, proxy);
  }
  return gIdToProxyMap.get(id);
}

function getProxyData(proxy) {
  assert(gProxyToIdMap.has(proxy));
  return DOM()[gProxyToIdMap.get(proxy)];
}

function convertValue(v) {
  if (isNonNullObject(v)) {
    if ("length" in v) {
      return v.map(nv => convertValue(nv));
    }
    if (v.object) {
      return ensureProxy(v.object);
    }
    if ("cssText" in v) {
      return new Proxy(v, StyleDeclarationProxyHandler);
    }
  }
  return dbg()._pool.convertValue(v);
}

const ReplayInspector = {
  window: newFixedProxy("window"),
  document: newFixedProxy("document"),

  wrapRequireHook(hook) {
    return (id, require) => {
      const rv = hook(id, require);
      return newRequire(id, rv);
    };
  },

  createInspectorUtils(utils) {
    return makeSubstituteProxy(utils, {
      "getAllStyleSheets": (document, documentOnly) => {
        assert(documentOnly);
        const { styleSheets } = getProxyData(document);
        return convertValue(styleSheets);
      },
      "getContentState": element => {
        const { contentState } = getProxyData(element);
        return contentState;
      },
      "getCSSStyleRules": (element, pseudo) => {
        pseudo = pseudo || "";
        const { styleRules } = getProxyData(element);
        if (pseudo.startsWith(":-moz-")) {
          // Mozilla specific CSS is ignored.
          return [];
        }
        if (styleRules && pseudo in styleRules) {
          return convertValue(styleRules[pseudo]);
        }
        ThrowError(`Missing style rules for ${element} "${pseudo}"`);
      },
      "getRuleLine": rule => getProxyData(rule)._ruleLine,
      "getRuleColumn": rule => getProxyData(rule)._ruleColumn,
      "getRelativeRuleLine": rule => getProxyData(rule)._ruleRelativeLine,
      "getSelectorCount": rule => getProxyData(rule)._ruleSelectorCount,
      "getSelectorText": (rule, i) => getProxyData(rule)._ruleSelectors[i],
      "selectorMatchesElement": (element, rule, index, pseudo, includeVisitedStyle = false) => {
        pseudo = pseudo || "";
        const { matchedSelectors } = getProxyData(element);
        const ruleId = getProxyData(rule).id;
        const key = `${ruleId}:${index}:${pseudo}:${includeVisitedStyle}`;
        if (matchedSelectors && key in matchedSelectors) {
          return matchedSelectors[key];
        }
        ThrowError(`Missing selector matching data for ${element} ${key}`);
      },
      "hasRulesModifiedByCSSOM": sheet => getProxyData(sheet)._hasRulesModifiedByCSSOM,
      "getSpecificity": () => NYI(),
      "hasPseudoClassLock": () => false,
    });
  },

  isPaused() {
    return dbg()._paused;
  },

  invalidateAfterUnpause,

  // Find the element in the replaying process which is being targeted by a
  // mouse event in the middleman process.
  findEventTarget(event) {
    const { id } = dbg()._sendRequestAllowDiverge({
      type: "findEventTarget",
      clientX: event.clientX,
      clientY: event.clientY,
    });
    if (id) {
      return ensureProxy(id);
    }
    return null;
  },
};

// Objects we need to override isInstance for.
const gOverrideIsInstance = ["CSSRule", "Event"];

for (const name of gOverrideIsInstance) {
  ReplayInspector[`create${name}`] = original => ({
    ...original,
    isInstance(obj) {
      if (gProxyToIdMap.has(obj)) {
        const { isInstance } = DOM()[gProxyToIdMap.get(obj)];
        return isInstance == name;
      }
      return original.isInstance(obj);
    },
  });
}

const DeepTreeWalkerProxyHandler = {
  defaultData() {
    return {
      rootNode: null,
      currentNode: null,
      siblings: null,
      siblingIndex: 0,
    };
  },

  ...BaseProxyHandler,

  get(target, name) {
    switch (name) {
      case "init":
        return root => { target.rootNode = target.currentNode = root; };
      case "currentNode":
        return target.currentNode;
      case "parentNode":
        return () => {
          const parent = target.currentNode.parentNode;
          if (parent) {
            target.currentNode = parent;
          }
          return parent;
        };
      case "firstChild":
      case "lastChild":
        return () => {
          const children = target.currentNode.childNodes;
          if (children && children.length > 0) {
            const index = name == "firstChild" ? 0 : children.length - 1;
            target.currentNode = children[index];
            target.siblings = children;
            target.siblingIndex = index;
            return target.currentNode;
          }
          return null;
        };
      case "nextSibling":
        return () => {
          ensureSiblings();
          if (target.siblings && target.siblingIndex + 1 < target.siblings.length) {
            target.currentNode = target.siblings[++target.siblingIndex];
            return target.currentNode;
          }
          return null;
        };
      case "previousSibling":
        return () => {
          ensureSiblings();
          if (target.siblings && target.siblingIndex >= 1) {
            target.currentNode = target.siblings[--target.siblingIndex];
            return target.currentNode;
          }
          return null;
        };
    }

    NYI();

    function ensureSiblings() {
      if (
        !target.siblings &&
        target.currentNode.parentNode &&
        target.currentNode.nodeType != Node.DOCUMENT_NODE
      ) {
        target.siblings = target.currentNode.parentNode.childNodes;
        target.siblingIndex = target.siblings.indexOf(target.currentNode);
      }
    }
  },

  set(target, name, value) {
    switch (name) {
    case "showAnonymousContent":
    case "showSubDocuments":
    case "showDocumentsAsNodes":
    case "currentNode":
      target[name] = value;
      return true;
    }

    NYI();
  },
};

const StyleDeclarationProxyHandler = {
  ...BaseProxyHandler,

  has({ properties }, name) {
    const num = parseInt(name);
    if (!Number.isNaN(num)) {
      return num < properties.length;
    }

    dump(`StyleDeclaration.has NYI ${name}\n`);
    NYI();
  },

  get({ cssText, properties }, name) {
    if (typeof name == "symbol") {
      if (name == Symbol.toStringTag) {
        return `StyleDeclarationProxy ${cssText}`;
      }
      return undefined;
    }

    if (name ==  "toString") {
      return () => `StyleDeclarationProxy ${cssText}`;
    }

    function getPropertyInfo(name, kind) {
      for (const entry of properties) {
        if (name == entry.name) {
          return entry[kind];
        }
      }
      return "";
    }

    switch (name) {
      case "cssText":
        return cssText;
      case "length":
        return properties.length;
      case "getPropertyValue":
        return name => getPropertyInfo(name, "value");
      case "getPropertyPriority":
        return name => getPropertyInfo(name, "priority");
      case "display":
      case "animationName":
      case "position":
        return getPropertyInfo(name, "value");
    }

    const num = parseInt(name);
    if (!Number.isNaN(num)) {
      return properties[num].name;
    }

    dump(`NYI StyleDeclaration.${name}\n`);
    NYI();
  },

  ownKeys() { NotAllowed(); },
};

let gNewChrome;
function getNewChrome(chrome) {
  if (!gNewChrome) {
    gNewChrome = makeSubstituteProxy(chrome, {
      Cc: makeSubstituteProxy(chrome.Cc, {
        "@mozilla.org/inspector/deep-tree-walker;1": {
          createInstance() {
            return new Proxy(
              DeepTreeWalkerProxyHandler.defaultData(),
              DeepTreeWalkerProxyHandler
            );
          },
        },
      }),
      Cu: makeSubstituteProxy(chrome.Cu, {
        isDeadWrapper(node) {
          if (gProxyToIdMap.has(node)) {
            return !_DOM;
          }
          return chrome.Cu.isDeadWrapper(node);
        },
      }),
    });
  }
  return gNewChrome;
}

let gNewServices;
function getNewServices(Services) {
  if (!gNewServices) {
    gNewServices = makeSubstituteProxy(Services, {
      els: {
        getListenerInfoFor: node => [],
      },
    });
  }
  return gNewServices;
}

function newRequire(id, rv) {
  switch (id) {
  case "chrome": return getNewChrome(rv);
  case "Services": return getNewServices(rv);
  }
  return rv;
}

///////////////////////////////////////////////////////////////////////////////
// Utilities
///////////////////////////////////////////////////////////////////////////////

function NYI() {
  ThrowError("Not yet implemented");
}

function NotAllowed() {
  ThrowError("Not allowed");
}

function assert(v) {
  if (!v) {
    ThrowError("Assertion Failed!");
  }
}

function ThrowError(msg) {
  const error = new Error(msg);
  dump(`ReplayInspector Server Error: ${msg} Stack: ${error.stack}\n`);
  throw error;
}

// Get a proxy which returns mapping[name] if it exists, else target[name].
function makeSubstituteProxy(target, mapping) {
  return new Proxy({}, {
    get(_, name) {
      if (mapping[name]) {
        return mapping[name];
      }
      return target[name];
    },
  });
}

function isNonNullObject(obj) {
  return obj && (typeof obj == "object" || typeof obj == "function");
}

function getWindow() {
  // Hopefully there is exactly one window in this enumerator.
  for (const window of gNewServices.ww.getWindowEnumerator()) {
    return window;
  }
  return null;
}

module.exports = ReplayInspector;
