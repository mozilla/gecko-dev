/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

// Make this available to both AMD and CJS environments
define(function (require, exports, module) {
  // Dependencies
  const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");
  const {
    span,
  } = require("resource://devtools/client/shared/vendor/react-dom-factories.js");

  const {
    appendRTLClassNameIfNeeded,
    maybeEscapePropertyName,
    wrapRender,
  } = require("resource://devtools/client/shared/components/reps/reps/rep-utils.js");
  const {
    MODE,
  } = require("resource://devtools/client/shared/components/reps/reps/constants.js");

  /**
   * Property for Obj (local JS objects), Grip (remote JS objects)
   * and GripMap (remote JS maps and weakmaps) reps.
   * It's used to render object properties.
   */
  PropRep.propTypes = {
    // Additional class to set on the key element
    keyClassName: PropTypes.string,
    // Property name.
    name: PropTypes.oneOfType([PropTypes.string, PropTypes.object]).isRequired,
    // Equal character rendered between property name and value.
    equal: PropTypes.string,
    mode: PropTypes.oneOf(Object.values(MODE)),
    onDOMNodeMouseOver: PropTypes.func,
    onDOMNodeMouseOut: PropTypes.func,
    onInspectIconClick: PropTypes.func,
    // Normally a PropRep will quote a property name that isn't valid
    // when unquoted; but this flag can be used to suppress the
    // quoting.
    suppressQuotes: PropTypes.bool,
    shouldRenderTooltip: PropTypes.bool,
  };

  /**
   * Function that given a name, a delimiter and an object returns an array
   * of React elements representing an object property (e.g. `name: value`)
   *
   * @param {Object} props
   * @return {Array} Array of React elements.
   */

  function PropRep(props) {
    const Grip = require("resource://devtools/client/shared/components/reps/reps/grip.js");
    const {
      Rep,
    } = require("resource://devtools/client/shared/components/reps/reps/rep.js");

    let {
      equal,
      keyClassName,
      mode,
      name,
      shouldRenderTooltip,
      suppressQuotes,
    } = props;

    const className = `nodeName${keyClassName ? " " + keyClassName : ""}`;

    let key;
    // The key can be a simple string, for plain objects,
    // or another object for maps and weakmaps.
    if (typeof name === "string") {
      if (!suppressQuotes) {
        name = maybeEscapePropertyName(name);
      }
      key = span(
        {
          className: appendRTLClassNameIfNeeded(className, name),
          title: shouldRenderTooltip ? name : null,
        },
        name
      );
    } else {
      key = Rep({
        ...props,
        className,
        object: name,
        mode: mode || MODE.TINY,
        defaultRep: Grip,
      });
    }

    return [
      key,
      span(
        {
          className: "objectEqual",
        },
        equal
      ),
      Rep({ ...props }),
    ];
  }

  // Exports from this module
  module.exports = wrapRender(PropRep);
});
