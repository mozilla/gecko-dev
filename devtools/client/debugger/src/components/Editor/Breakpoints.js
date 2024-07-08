/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import PropTypes from "devtools/client/shared/vendor/react-prop-types";
import React, { Component } from "devtools/client/shared/vendor/react";
import { div } from "devtools/client/shared/vendor/react-dom-factories";
import Breakpoint from "./Breakpoint";

import {
  getSelectedSource,
  getFirstVisibleBreakpoints,
} from "../../selectors/index";
import { getSelectedLocation } from "../../utils/selected-location";
import { makeBreakpointId } from "../../utils/breakpoint/index";
import { connect } from "devtools/client/shared/vendor/react-redux";
import { fromEditorLine } from "../../utils/editor/index";
import actions from "../../actions/index";
import { markerTypes } from "../../constants";
import { features } from "../../utils/prefs";
const classnames = require("resource://devtools/client/shared/classnames.js");

const isMacOS = Services.appinfo.OS === "Darwin";

class Breakpoints extends Component {
  static get propTypes() {
    return {
      breakpoints: PropTypes.array,
      editor: PropTypes.object,
      selectedSource: PropTypes.object,
      removeBreakpointsAtLine: PropTypes.func,
      toggleBreakpointsAtLine: PropTypes.func,
      continueToHere: PropTypes.func,
      showEditorEditBreakpointContextMenu: PropTypes.func,
    };
  }

  constructor(props) {
    super(props);
  }

  componentDidMount() {
    this.setMarkers();
  }

  componentDidUpdate() {
    this.setMarkers();
  }

  setMarkers() {
    const { selectedSource, editor, breakpoints } = this.props;

    // Only for codemirror 6
    if (!features.codemirrorNext) {
      return;
    }

    if (!selectedSource || !breakpoints || !editor) {
      return;
    }

    const markers = [
      {
        id: markerTypes.GUTTER_BREAKPOINT_MARKER,
        lineClassName: "cm6-gutter-breakpoint",
        condition: line => {
          const lineNumber = fromEditorLine(selectedSource.id, line);
          const breakpoint = breakpoints.find(
            bp => getSelectedLocation(bp, selectedSource).line === lineNumber
          );
          if (!breakpoint) {
            return false;
          }
          return breakpoint;
        },
        createLineElementNode: (line, breakpoint) => {
          const lineNumber = fromEditorLine(selectedSource.id, line);
          const breakpointNode = document.createElement("div");
          breakpointNode.appendChild(document.createTextNode(lineNumber));
          breakpointNode.className = classnames("breakpoint-marker", {
            "breakpoint-disabled": breakpoint.disabled,
            "has-condition": breakpoint?.options.condition,
            "has-log": breakpoint?.options.logValue,
          });
          breakpointNode.onclick = event => this.onClick(event, breakpoint);
          breakpointNode.oncontextmenu = event =>
            this.onContextMenu(event, breakpoint);
          return breakpointNode;
        },
      },
    ];
    editor.setLineGutterMarkers(markers);
  }

  onClick = (event, breakpoint) => {
    const {
      continueToHere,
      toggleBreakpointsAtLine,
      removeBreakpointsAtLine,
      selectedSource,
    } = this.props;

    event.stopPropagation();
    event.preventDefault();

    // ignore right clicks when clicking on the breakpoint
    if (event.button === 2) {
      return;
    }

    const selectedLocation = getSelectedLocation(breakpoint, selectedSource);
    const ctrlOrCmd = isMacOS ? event.metaKey : event.ctrlKey;

    if (ctrlOrCmd) {
      continueToHere(selectedLocation);
      return;
    }

    if (event.shiftKey) {
      toggleBreakpointsAtLine(!breakpoint.disabled, selectedLocation.line);
      return;
    }

    removeBreakpointsAtLine(selectedLocation.source, selectedLocation.line);
  };

  onContextMenu = (event, breakpoint) => {
    event.stopPropagation();
    event.preventDefault();

    this.props.showEditorEditBreakpointContextMenu(event, breakpoint);
  };

  render() {
    const {
      breakpoints,
      selectedSource,
      editor,
      showEditorEditBreakpointContextMenu,
      continueToHere,
      toggleBreakpointsAtLine,
      removeBreakpointsAtLine,
    } = this.props;

    if (!selectedSource || !breakpoints) {
      return null;
    }

    if (features.codemirrorNext) {
      return null;
    }

    return div(
      null,
      breakpoints.map(breakpoint => {
        return React.createElement(Breakpoint, {
          key: makeBreakpointId(breakpoint.location),
          breakpoint,
          selectedSource,
          showEditorEditBreakpointContextMenu,
          continueToHere,
          toggleBreakpointsAtLine,
          removeBreakpointsAtLine,
          editor,
        });
      })
    );
  }
}

const mapStateToProps = state => {
  const selectedSource = getSelectedSource(state);
  return {
    // Retrieves only the first breakpoint per line so that the
    // breakpoint marker represents only the first breakpoint
    breakpoints: getFirstVisibleBreakpoints(state),
    selectedSource,
  };
};

export default connect(mapStateToProps, {
  showEditorEditBreakpointContextMenu:
    actions.showEditorEditBreakpointContextMenu,
  continueToHere: actions.continueToHere,
  toggleBreakpointsAtLine: actions.toggleBreakpointsAtLine,
  removeBreakpointsAtLine: actions.removeBreakpointsAtLine,
})(Breakpoints);
