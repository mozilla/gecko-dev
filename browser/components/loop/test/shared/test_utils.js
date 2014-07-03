/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global React */

/**
 * Stubs a React component.
 *
 * @param  {Function} ComponentClass Original component constructor.
 * @param  {Object}   specObject     Spec object.
 * @return {Function}                Component constructor
 */
function stubComponent(ComponentClass, specObject) {
  if (!TestUtils.isCompositeComponent(ComponentClass)) {
    throw Error("stubComponent() expects a valid React component constructor.");
  }
  var stubbedSpec = _.extend({}, ComponentClass.originalSpec, specObject);
  return React.createClass(stubbedSpec);
}

React.addons.TestUtils.stubComponent = stubComponent;
