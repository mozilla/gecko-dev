/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createRef,
  PureComponent,
} = require("resource://devtools/client/shared/vendor/react.js");
const dom = require("resource://devtools/client/shared/vendor/react-dom-factories.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");

const { KeyCodes } = require("resource://devtools/client/shared/keycodes.js");

const {
  getStr,
} = require("resource://devtools/client/inspector/animation/utils/l10n.js");
const {
  hasRunningAnimation,
} = require("resource://devtools/client/inspector/animation/utils/utils.js");

class PauseResumeButton extends PureComponent {
  static get propTypes() {
    return {
      animations: PropTypes.arrayOf(PropTypes.object).isRequired,
      setAnimationsPlayState: PropTypes.func.isRequired,
    };
  }

  constructor(props) {
    super(props);

    this.onKeyDown = this.onKeyDown.bind(this);
    this.pauseResumeButtonRef = createRef();
  }

  componentDidMount() {
    const targetEl = this.getKeyEventTarget();
    targetEl.addEventListener("keydown", this.onKeyDown);
  }

  componentWillUnount() {
    const targetEl = this.getKeyEventTarget();
    targetEl.removeEventListener("keydown", this.onKeyDown);
  }

  getKeyEventTarget() {
    return this.pauseResumeButtonRef.current.closest("#animation-container");
  }

  onToggleAnimationsPlayState(event) {
    event.stopPropagation();
    const { setAnimationsPlayState, animations } = this.props;
    const isRunning = hasRunningAnimation(animations);

    setAnimationsPlayState(!isRunning);
  }

  onKeyDown(event) {
    // Prevent to the duplicated call from the key listener and click listener.
    if (
      event.keyCode === KeyCodes.DOM_VK_SPACE &&
      event.target !== this.pauseResumeButtonRef.current
    ) {
      this.onToggleAnimationsPlayState(event);
    }
  }

  render() {
    const isRunning = hasRunningAnimation(this.props.animations);

    return dom.button({
      className:
        "pause-resume-button devtools-button" + (isRunning ? "" : " paused"),
      onClick: this.onToggleAnimationsPlayState.bind(this),
      title: isRunning
        ? getStr("timeline.resumedButtonTooltip")
        : getStr("timeline.pausedButtonTooltip"),
      ref: this.pauseResumeButtonRef,
    });
  }
}

module.exports = PauseResumeButton;
