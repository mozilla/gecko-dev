/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import * as t from "@babel/types";

export function isFunction(node) {
  return (
    t.isFunction(node) ||
    t.isArrowFunctionExpression(node) ||
    t.isObjectMethod(node) ||
    t.isClassMethod(node)
  );
}

export function isAwaitExpression(path) {
  const { node, parent } = path;
  return (
    t.isAwaitExpression(node) ||
    t.isAwaitExpression(parent.init) ||
    t.isAwaitExpression(parent)
  );
}

export function isYieldExpression(path) {
  const { node, parent } = path;
  return (
    t.isYieldExpression(node) ||
    t.isYieldExpression(parent.init) ||
    t.isYieldExpression(parent)
  );
}

export function getMemberExpression(root) {
  function _getMemberExpression(node, expr) {
    if (t.isMemberExpression(node)) {
      expr = [node.property.name].concat(expr);
      return _getMemberExpression(node.object, expr);
    }

    if (t.isCallExpression(node)) {
      return [];
    }

    if (t.isThisExpression(node)) {
      return ["this"].concat(expr);
    }

    return [node.name].concat(expr);
  }

  const expr = _getMemberExpression(root, []);
  return expr.join(".");
}

export function getVariables(dec) {
  if (!dec.id) {
    return [];
  }

  if (t.isArrayPattern(dec.id)) {
    if (!dec.id.elements) {
      return [];
    }

    // NOTE: it's possible that an element is empty or has several variables
    // e.g. const [, a] = arr
    // e.g. const [{a, b }] = 2
    return dec.id.elements
      .filter(Boolean)
      .map(element => ({
        name: t.isAssignmentPattern(element)
          ? element.left.name
          : element.name || element.argument?.name,
        location: element.loc,
      }))
      .filter(({ name }) => name);
  }

  return [
    {
      name: dec.id.name,
      location: dec.loc,
    },
  ];
}

/**
 * Add the identifiers for a given object pattern.
 *
 * @param {Array.<Object>} identifiers
 *        the current list of identifiers where to push the new identifiers
 *        related to this path.
 * @param {Set<String>} identifiersKeys
 *        List of currently registered identifier location key.
 * @param {Object} pattern
 */
export function addPatternIdentifiers(identifiers, identifiersKeys, pattern) {
  let items;
  if (t.isObjectPattern(pattern)) {
    items = pattern.properties.map(({ value }) => value);
  }

  if (t.isArrayPattern(pattern)) {
    items = pattern.elements;
  }

  if (items) {
    addIdentifiers(identifiers, identifiersKeys, items);
  }
}

function addIdentifiers(identifiers, identifiersKeys, items) {
  for (const item of items) {
    if (t.isObjectPattern(item) || t.isArrayPattern(item)) {
      addPatternIdentifiers(identifiers, identifiersKeys, item);
    } else if (t.isIdentifier(item)) {
      if (!identifiersKeys.has(nodeLocationKey(item.loc))) {
        identifiers.push({
          name: item.name,
          expression: item.name,
          location: item.loc,
        });
      }
    }
  }
}

// Top Level checks the number of "body" nodes in the ancestor chain
// if the node is top-level, then it shoul only have one body.
export function isTopLevel(ancestors) {
  return ancestors.filter(ancestor => ancestor.key == "body").length == 1;
}

export function nodeLocationKey({ start, end }) {
  return `${start.line}:${start.column}:${end.line}:${end.column}`;
}
