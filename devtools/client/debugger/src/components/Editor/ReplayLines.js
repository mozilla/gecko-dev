/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow
import { PureComponent } from "react";
import {
  toEditorPosition,
  getDocument,
  hasDocument,
  startOperation,
  endOperation,
  getTokenEnd,
} from "../../utils/editor";
import { isException } from "../../utils/pause";
import { getIndentation } from "../../utils/indentation";
import { connect } from "../../utils/connect";
import {
  getFramePositions,
  getVisibleSelectedFrame,
  getPauseReason,
  getSourceWithContent,
  getCurrentThread,
  getPausePreviewLocation,
} from "../../selectors";

import type { SourceLocation, SourceWithContent } from "../../types";

type OwnProps = {||};
type Props = {
  unexecutedLocations: SourceLocation[],
  source: ?SourceWithContent,
};

function isDocumentReady(
  source: ?SourceWithContent,
  location: ?SourceLocation
) {
  return location && source && source.content && hasDocument(location.sourceId);
}

export class ReplayLines extends PureComponent<Props> {
  componentDidMount() {
    const { unexecutedLocations, source } = this.props;
    this.setUnexecutedLocations(unexecutedLocations, source);
  }

  componentWillUnmount() {
    const { unexecutedLocations, source } = this.props;
    this.clearUnexecutedLocations(unexecutedLocations, source);
  }

  componentDidUpdate(prevProps: Props) {
    const { unexecutedLocations, source } = this.props;

    startOperation();
    this.clearUnexecutedLocations(prevProps.unexecutedLocations, prevProps.source);
    this.setUnexecutedLocations(unexecutedLocations, source);
    endOperation();
  }

  setUnexecutedLocations(unexecutedLocations, source) {
    for (const location of unexecutedLocations) {
      if (!isDocumentReady(source, location)) {
        continue;
      }
      const { line } = toEditorPosition(location);
      const doc = getDocument(location.sourceId);
      doc.addLineClass(line, "line", "unexecuted-line");
    }
  }

  clearUnexecutedLocations(unexecutedLocations, source) {
    for (const location of unexecutedLocations) {
      if (!isDocumentReady(source, location)) {
        continue;
      }
      const { line } = toEditorPosition(location);
      const doc = getDocument(location.sourceId);
      doc.removeLineClass(line, "line", "unexecuted-line");
    }
  }

  render() {
    return null;
  }
}

const mapStateToProps = state => {
  const framePositions = getFramePositions(state) || { positions: [], unexecuted: [] };

  dump(`MAP_STATE ${JSON.stringify(framePositions)}\n`);

  const frame = getVisibleSelectedFrame(state);
  const previewLocation = getPausePreviewLocation(state) || (frame && frame.location);

  const unexecutedLocations = framePositions.unexecuted.map(({ location, generatedLocation }) => {
    if (previewLocation && previewLocation.sourceId == location.sourceId) {
      return location;
    }
    return generatedLocation;
  });

  return {
    unexecutedLocations,
    source: previewLocation && getSourceWithContent(state, previewLocation.sourceId),
  };
};

export default connect<Props, OwnProps, _, _, _, _>(mapStateToProps)(ReplayLines);
