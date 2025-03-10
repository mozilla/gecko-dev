/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { connect } from "devtools/client/shared/vendor/react-redux";

// The React component is in a distinct module in order to prevent loading all Debugger actions and selectors
// when SmartTrace imports the Frames Component.
import { Frames } from "./Frames";

import actions from "../../../actions/index";

import {
  getFrameworkGroupingState,
  getSelectedFrame,
  getCurrentThreadFrames,
  getShouldSelectOriginalLocation,
  getSelectedTraceIndex,
} from "../../../selectors/index";

const mapStateToProps = state => ({
  frames: getCurrentThreadFrames(state),
  frameworkGroupingOn: getFrameworkGroupingState(state),
  selectedFrame: getSelectedFrame(state),
  isTracerFrameSelected: getSelectedTraceIndex(state) != null,
  shouldDisplayOriginalLocation: getShouldSelectOriginalLocation(state),
  disableFrameTruncate: false,
  disableContextMenu: false,
  displayFullUrl: false,
});

export default connect(mapStateToProps, {
  selectFrame: actions.selectFrame,
  showFrameContextMenu: actions.showFrameContextMenu,
})(Frames);
