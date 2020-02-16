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
  positions: any,
  unexecuted: any,
};

function forEachDocument({ location, generatedLocation }, callback) {
  if (hasDocument(location.sourceId)) {
    callback(location);
  }
  if (hasDocument(generatedLocation.sourceId)) {
    callback(generatedLocation);
  }
}

export class ReplayLines extends PureComponent<Props> {
  componentDidMount() {
    const { unexecuted } = this.props;
    this.setUnexecutedLocations(unexecuted);
  }

  componentWillUnmount() {
    const { unexecuted } = this.props;
    this.clearUnexecutedLocations(unexecuted);
  }

  componentDidUpdate(prevProps: Props) {
    const { unexecuted } = this.props;

    startOperation();
    this.clearUnexecutedLocations(prevProps.unexecuted);
    this.setUnexecutedLocations(unexecuted);
    endOperation();
  }

  setUnexecutedLocations(unexecuted) {
    for (const info of unexecuted) {
      forEachDocument(info, location => {
        const { line } = toEditorPosition(location);
        const doc = getDocument(location.sourceId);
        doc.addLineClass(line, "line", "unexecuted-line");
      });
    }
  }

  clearUnexecutedLocations(unexecuted) {
    for (const info of unexecuted) {
      forEachDocument(info, location => {
        const { line } = toEditorPosition(location);
        const doc = getDocument(location.sourceId);
        doc.removeLineClass(line, "line", "unexecuted-line");
      });
    }
  }

  render() {
    return null;
  }
}

const mapStateToProps = state => {
  const { positions, unexecuted } = getFramePositions(state) || { positions: [], unexecuted: [] };
  return { positions, unexecuted };
};

export default connect<Props, OwnProps, _, _, _, _>(mapStateToProps)(ReplayLines);
