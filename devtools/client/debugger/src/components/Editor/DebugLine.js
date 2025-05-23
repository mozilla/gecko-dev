/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { PureComponent } from "devtools/client/shared/vendor/react";
import PropTypes from "devtools/client/shared/vendor/react-prop-types";
import { toEditorPosition } from "../../utils/editor/index";
import { isException } from "../../utils/pause/index";
import { connect } from "devtools/client/shared/vendor/react-redux";
import { markerTypes } from "../../constants";
import {
  getVisibleSelectedFrame,
  getPauseReason,
  getSourceTextContent,
  getCurrentThread,
  getViewport,
  getSelectedTraceLocation,
} from "../../selectors/index";

export class DebugLine extends PureComponent {
  debugExpression;

  static get propTypes() {
    return {
      editor: PropTypes.object,
      selectedSource: PropTypes.object,
      location: PropTypes.object,
      why: PropTypes.object,
      sourceTextContent: PropTypes.object,
    };
  }

  componentDidMount() {
    this.setDebugLine();
  }

  componentWillUnmount() {
    this.clearDebugLine(this.props);
  }

  componentDidUpdate(prevProps) {
    this.clearDebugLine(prevProps);
    this.setDebugLine();
  }

  setDebugLine() {
    const { why, location, editor, selectedSource } = this.props;
    if (!location) {
      return;
    }

    if (!selectedSource || location.source.id !== selectedSource.id) {
      return;
    }

    const { lineClass, markTextClass } = this.getTextClasses(why);
    const editorLocation = toEditorPosition(location);

    // Show the paused "caret", to highlight on which particular line **and column** we are paused.
    //
    // Using only a `positionClassName` wouldn't only be applied to the immediate
    // token after the position and force to use ::before to show the paused location.
    // Using ::before prevents using :hover to be able to hide the icon on mouse hovering.
    //
    // So we have to use `createPositionElementNode`, similarly to column breakpoints
    // to have a new dedicated DOM element for the paused location.
    editor.setPositionContentMarker({
      id: markerTypes.PAUSED_LOCATION_MARKER,

      // Ensure displaying the marker after all the other markers and especially the column breakpoint markers
      displayLast: true,

      positions: [editorLocation],
      createPositionElementNode(_line, _column, isFirstNonSpaceColumn) {
        const pausedLocation = document.createElement("span");
        pausedLocation.className = `paused-location${isFirstNonSpaceColumn ? " first-column" : ""}`;

        const bar = document.createElement("span");
        bar.className = `vertical-bar`;
        pausedLocation.appendChild(bar);

        return pausedLocation;
      },
    });

    editor.setLineContentMarker({
      id: markerTypes.DEBUG_LINE_MARKER,
      lineClassName: lineClass,
      lines: [{ line: editorLocation.line }],
    });
    editor.setPositionContentMarker({
      id: markerTypes.DEBUG_POSITION_MARKER,
      positionClassName: markTextClass,
      positions: [editorLocation],
    });
  }

  clearDebugLine(otherProps = {}) {
    const { location, editor, selectedSource } = this.props;
    // Remove the debug line marker when no longer paused, or the selected source
    // is no longer the source where the pause occured.
    if (
      !location ||
      location.source.id !== selectedSource.id ||
      otherProps?.location !== location ||
      otherProps?.selectedSource?.id !== selectedSource.id
    ) {
      editor.removeLineContentMarker(markerTypes.DEBUG_LINE_MARKER);
      editor.removePositionContentMarker(markerTypes.DEBUG_POSITION_MARKER);
      editor.removePositionContentMarker(markerTypes.PAUSED_LOCATION_MARKER);
    }
  }

  getTextClasses(why) {
    if (why && isException(why)) {
      return {
        markTextClass: "debug-expression-error",
        lineClass: "new-debug-line-error",
      };
    }

    // We no longer highlight the next token via debug-expression
    // and only highlight the line via paused-line.
    return {
      markTextClass: null,
      lineClass: why == "tracer" ? "traced-line" : "paused-line",
    };
  }

  render() {
    return null;
  }
}

function isDocumentReady(location, sourceTextContent) {
  return location && sourceTextContent;
}

const mapStateToProps = state => {
  // If we aren't paused, fallback on showing the JS tracer
  // currently selected trace location.
  // If any trace is selected in the JS Tracer, this takes the lead over
  // any paused location. (the same choice is made when showing inline previews)
  let why;
  let location = getSelectedTraceLocation(state);
  if (location) {
    why = "tracer";
  } else {
    // Avoid unecessary intermediate updates when there is no location
    // or the source text content isn't yet fully loaded
    const frame = getVisibleSelectedFrame(state);
    location = frame?.location;

    // We are not tracing, nor pausing
    if (!location) {
      return {};
    }

    why = getPauseReason(state, getCurrentThread(state));
  }

  // if we have a valid viewport.
  // This is a way to know if the actual source is displayed
  // and we are no longer on the "loading..." message
  if (!getViewport(state)) {
    return {};
  }

  const sourceTextContent = getSourceTextContent(state, location);
  if (!isDocumentReady(location, sourceTextContent)) {
    return {};
  }

  return {
    location,
    why,
    sourceTextContent,
  };
};

export default connect(mapStateToProps)(DebugLine);
