/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React, { PureComponent } from "devtools/client/shared/vendor/react";
import {
  div,
  input,
  span,
} from "devtools/client/shared/vendor/react-dom-factories";
import PropTypes from "devtools/client/shared/vendor/react-prop-types";
import { connect } from "devtools/client/shared/vendor/react-redux";
import actions from "../../../actions/index";

import { CloseButton } from "../../shared/Button/index";

import {
  getSelectedText,
  makeBreakpointId,
} from "../../../utils/breakpoint/index";
import { getSelectedLocation } from "../../../utils/selected-location";
import { isLineBlackboxed } from "../../../utils/source";

import {
  getSelectedFrame,
  getSelectedSource,
  isSourceMapIgnoreListEnabled,
  isSourceOnSourceMapIgnoreList,
  getBlackBoxRanges,
} from "../../../selectors/index";

const classnames = require("resource://devtools/client/shared/classnames.js");

class Breakpoint extends PureComponent {
  static get propTypes() {
    return {
      breakpoint: PropTypes.object.isRequired,
      disableBreakpoint: PropTypes.func.isRequired,
      editor: PropTypes.object.isRequired,
      enableBreakpoint: PropTypes.func.isRequired,
      openConditionalPanel: PropTypes.func.isRequired,
      removeBreakpoint: PropTypes.func.isRequired,
      selectSpecificLocation: PropTypes.func.isRequired,
      selectedBreakpointLocation: PropTypes.object.isRequired,
      isCurrentlyPausedAtBreakpoint: PropTypes.bool.isRequired,
      source: PropTypes.object.isRequired,
      checkSourceOnIgnoreList: PropTypes.func.isRequired,
      isBreakpointLineBlackboxed: PropTypes.bool,
      showBreakpointContextMenu: PropTypes.func.isRequired,
      breakpointText: PropTypes.string.isRequired,
    };
  }

  onContextMenu = event => {
    event.preventDefault();

    this.props.showBreakpointContextMenu(
      event,
      this.props.breakpoint,
      this.props.source
    );
  };

  stopClicks = event => event.stopPropagation();

  onDoubleClick = () => {
    const { breakpoint, openConditionalPanel } = this.props;
    if (breakpoint.options.condition) {
      openConditionalPanel(this.props.selectedBreakpointLocation);
    } else if (breakpoint.options.logValue) {
      openConditionalPanel(this.props.selectedBreakpointLocation, true);
    }
  };

  onKeyDown = event => {
    // Handling only the Enter/Space keys, bail if another key was pressed
    if (event.key !== "Enter" && event.key !== " ") {
      return;
    }

    if (event.shiftKey) {
      this.onDoubleClick();
      return;
    }
    this.selectBreakpoint(event);
  };

  selectBreakpoint = event => {
    // Ignore double click as we have a dedicated double click listener
    if (event.type == "click" && event.detail > 1) {
      return;
    }
    event.preventDefault();
    const { selectSpecificLocation } = this.props;
    selectSpecificLocation(this.props.selectedBreakpointLocation);
  };

  removeBreakpoint = event => {
    const { removeBreakpoint, breakpoint } = this.props;
    event.stopPropagation();
    removeBreakpoint(breakpoint);
  };

  handleBreakpointCheckbox = () => {
    const { breakpoint, enableBreakpoint, disableBreakpoint } = this.props;
    if (breakpoint.disabled) {
      enableBreakpoint(breakpoint);
    } else {
      disableBreakpoint(breakpoint);
    }
  };

  getBreakpointLocation() {
    const { source } = this.props;
    const { column, line } = this.props.selectedBreakpointLocation;

    const isWasm = source?.isWasm;
    // column is 0-based everywhere, but we want to display 1-based to the user.
    const columnVal = column ? `:${column + 1}` : "";
    const bpLocation = isWasm
      ? `0x${line.toString(16).toUpperCase()}`
      : `${line}${columnVal}`;

    return bpLocation;
  }

  highlightText(text = "", editor) {
    const htmlString = editor.highlightText(document, text);
    return { __html: htmlString };
  }

  render() {
    const { breakpoint, editor, isBreakpointLineBlackboxed, breakpointText } =
      this.props;
    const labelId = `${breakpoint.id}-label`;
    return div(
      {
        className: classnames({
          breakpoint,
          paused: this.props.isCurrentlyPausedAtBreakpoint,
          disabled: breakpoint.disabled,
          "is-conditional": !!breakpoint.options.condition,
          "is-log": !!breakpoint.options.logValue,
        }),
        onClick: this.selectBreakpoint,
        onDoubleClick: this.onDoubleClick,
        onContextMenu: this.onContextMenu,
        onKeyDown: this.onKeyDown,
        role: "button",
        tabIndex: 0,
        title: breakpointText,
      },
      input({
        id: breakpoint.id,
        type: "checkbox",
        className: "breakpoint-checkbox",
        checked: !breakpoint.disabled,
        disabled: isBreakpointLineBlackboxed,
        onChange: this.handleBreakpointCheckbox,
        onClick: this.stopClicks,
        "aria-labelledby": labelId,
      }),
      span(
        {
          id: labelId,
          className: "breakpoint-label cm-s-mozilla devtools-monospace",
        },
        span({
          className: "cm-highlighted",
          dangerouslySetInnerHTML: this.highlightText(breakpointText, editor),
        })
      ),
      div(
        {
          className: "breakpoint-line-close",
        },
        div(
          {
            className: "breakpoint-line devtools-monospace",
          },
          this.getBreakpointLocation()
        ),
        React.createElement(CloseButton, {
          handleClick: this.removeBreakpoint,
          tooltip: L10N.getStr("breakpoints.removeBreakpointTooltip"),
        })
      )
    );
  }
}

function isCurrentlyPausedAtBreakpoint(
  state,
  selectedBreakpointLocation,
  selectedSource
) {
  const frame = getSelectedFrame(state);
  if (!frame) {
    return false;
  }
  const bpId = makeBreakpointId(selectedBreakpointLocation);
  const frameId = makeBreakpointId(getSelectedLocation(frame, selectedSource));
  return bpId == frameId;
}

function getBreakpointText(breakpoint, selectedSource) {
  const { condition, logValue } = breakpoint.options;
  return logValue || condition || getSelectedText(breakpoint, selectedSource);
}

const mapStateToProps = (state, props) => {
  const { breakpoint, source } = props;
  const selectedSource = getSelectedSource(state);
  const selectedBreakpointLocation = getSelectedLocation(
    breakpoint,
    selectedSource
  );
  const blackboxedRangesForSource = getBlackBoxRanges(state)[source.url];
  const isSourceOnIgnoreList =
    isSourceMapIgnoreListEnabled(state) &&
    isSourceOnSourceMapIgnoreList(state, source);
  return {
    selectedBreakpointLocation,
    isCurrentlyPausedAtBreakpoint: isCurrentlyPausedAtBreakpoint(
      state,
      selectedBreakpointLocation,
      selectedSource
    ),
    isBreakpointLineBlackboxed: isLineBlackboxed(
      blackboxedRangesForSource,
      breakpoint.location.line,
      isSourceOnIgnoreList
    ),
    breakpointText: getBreakpointText(breakpoint, selectedSource),
  };
};

export default connect(mapStateToProps, {
  enableBreakpoint: actions.enableBreakpoint,
  removeBreakpoint: actions.removeBreakpoint,
  disableBreakpoint: actions.disableBreakpoint,
  selectSpecificLocation: actions.selectSpecificLocation,
  openConditionalPanel: actions.openConditionalPanel,
  showBreakpointContextMenu: actions.showBreakpointContextMenu,
})(Breakpoint);
