/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";
// These are all the current lezer node types used in the source editor,
// Add more here as are needed
const nodeTypes = {
  FunctionExpression: "FunctionExpression",
  FunctionDeclaration: "FunctionDeclaration",
  ArrowFunction: "ArrowFunction",
  MethodDeclaration: "MethodDeclaration",
  Property: "Property",
  PropertyDeclaration: "PropertyDeclaration",
  PropertyDefinition: "PropertyDefinition",
  MemberExpression: "MemberExpression",
  VariableDefinition: "VariableDefinition",
  VariableName: "VariableName",
  this: "this",
  PropertyName: "PropertyName",
  Equals: "Equals",
  ParamList: "ParamList",
  Spread: "Spread",
};

const nodeTypeSets = {
  functions: new Set([
    nodeTypes.FunctionExpression,
    nodeTypes.FunctionDeclaration,
    nodeTypes.ArrowFunction,
    nodeTypes.MethodDeclaration,
  ]),
  expressions: new Set([
    nodeTypes.MemberExpression,
    nodeTypes.VariableDefinition,
    nodeTypes.VariableName,
    nodeTypes.this,
    nodeTypes.PropertyName,
  ]),
};

/**
 * Walk the syntax tree of the langauge provided
 *
 * @param {Object}   view - Codemirror view (https://codemirror.net/docs/ref/#view)
 * @param {Object}   language - Codemirror Language (https://codemirror.net/docs/ref/#language)
 * @param {Object}   options
 *        {Boolean}  options.forceParseTo - Force parsing the document up to a certain point
 *        {Function} options.enterVisitor - A function that is called when a node is entered
 *        {Set}      options.filterSet - A set of node types which should be visited, all others should be ignored
 *        {Number}   options.walkFrom - Determine the location in the AST where the iteration of the syntax tree should start
 *        {Number}   options.walkTo - Determine the location in the AST where the iteration of the syntax tree should end
 */
async function walkTree(view, language, options) {
  const { forceParsing, syntaxTree } = language;
  if (options.forceParseTo) {
    // Force parsing the source, up to the end of the current viewport,
    // Also increasing the timeout threshold so we make sure
    // all required content is parsed (this is mostly needed for larger sources).
    await forceParsing(view, options.forceParseTo, 10000);
  }
  await syntaxTree(view.state).iterate({
    enter: node => {
      if (options.filterSet?.has(node.name)) {
        options.enterVisitor(node);
      }
    },
    from: options.walkFrom,
    to: options.walkTo,
  });
}

module.exports = {
  nodeTypes,
  nodeTypeSets,
  walkTree,
};
