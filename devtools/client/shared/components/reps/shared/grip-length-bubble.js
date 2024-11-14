/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

// Make this available to both AMD and CJS environments
define(function (require, exports, module) {
  const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");

  const {
    wrapRender,
  } = require("resource://devtools/client/shared/components/reps/reps/rep-utils.js");
  const {
    MODE,
  } = require("resource://devtools/client/shared/components/reps/reps/constants.js");
  const {
    ModePropType,
  } = require("resource://devtools/client/shared/components/reps/reps/array.js");

  const dom = require("resource://devtools/client/shared/vendor/react-dom-factories.js");
  const { span } = dom;

  GripLengthBubble.propTypes = {
    object: PropTypes.object.isRequired,
    maxLengthMap: PropTypes.instanceOf(Map).isRequired,
    getLength: PropTypes.func.isRequired,
    mode: ModePropType,
    visibilityThreshold: PropTypes.number,
  };

  function GripLengthBubble(props) {
    const {
      object,
      mode = MODE.SHORT,
      visibilityThreshold = 2,
      maxLengthMap,
      getLength,
      showZeroLength = false,
    } = props;

    const length = getLength(object);
    const isEmpty = length === 0;
    const isObvious =
      [MODE.SHORT, MODE.LONG].includes(mode) &&
      length > 0 &&
      length <= maxLengthMap.get(mode) &&
      length <= visibilityThreshold;
    if ((isEmpty && !showZeroLength) || isObvious) {
      return "";
    }

    return span(
      {
        className: "objectLengthBubble",
      },
      `(${length})`
    );
  }

  module.exports = {
    lengthBubble: wrapRender(GripLengthBubble),
  };
});
