/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React, { Component } from "devtools/client/shared/vendor/react";
import ReactDOM from "devtools/client/shared/vendor/react-dom";

import actions from "../../actions/index";
import { div } from "devtools/client/shared/vendor/react-dom-factories";
import PropTypes from "devtools/client/shared/vendor/react-prop-types";
import InlinePreviewRow from "./InlinePreviewRow";
import InlinePreview from "./InlinePreview";
import { connect } from "devtools/client/shared/vendor/react-redux";
import {
  getSelectedFrame,
  getCurrentThread,
  getInlinePreviews,
} from "../../selectors/index";

import { features } from "../../utils/prefs";
import { markerTypes } from "../../constants";

function hasPreviews(previews) {
  return !!previews && !!Object.keys(previews).length;
}

class InlinePreviews extends Component {
  static get propTypes() {
    return {
      editor: PropTypes.object.isRequired,
      previews: PropTypes.object,
    };
  }

  componentDidMount() {
    this.renderInlinePreviewMarker();
  }

  componentDidUpdate() {
    this.renderInlinePreviewMarker();
  }

  renderInlinePreviewMarker() {
    const {
      editor,
      previews,
      openElementInInspector,
      highlightDomElement,
      unHighlightDomElement,
    } = this.props;

    if (!features.codemirrorNext) {
      return;
    }

    if (!previews) {
      editor.removeLineContentMarker(markerTypes.INLINE_PREVIEW_MARKER);
      return;
    }

    editor.setLineContentMarker({
      id: markerTypes.INLINE_PREVIEW_MARKER,
      lines: Object.keys(previews).map(line => {
        // CM6 line is 1-based unlike CM5 which is 0-based.
        // The preview keys line numbers as strings so cast to number to avoid string concatenation
        line = Number(line);
        return {
          line: line + 1,
          value: previews[line],
        };
      }),
      createLineElementNode: (line, value) => {
        const widgetNode = document.createElement("div");
        widgetNode.className = "inline-preview";

        ReactDOM.render(
          React.createElement(
            React.Fragment,
            null,
            value.map(preview =>
              React.createElement(InlinePreview, {
                line,
                key: `${line}-${preview.name}`,
                type: preview.type,
                variable: preview.name,
                value: preview.value,
                openElementInInspector,
                highlightDomElement,
                unHighlightDomElement,
              })
            )
          ),
          widgetNode
        );
        return widgetNode;
      },
    });
  }

  componentWillUnmount() {
    if (!features.codemirrorNext) {
      return;
    }
    this.props.editor.removeLineContentMarker(
      markerTypes.INLINE_PREVIEW_MARKER
    );
  }

  render() {
    const { editor, previews } = this.props;

    if (features.codemirrorNext) {
      return null;
    }

    if (!previews) {
      return null;
    }
    const previewsObj = previews;

    let inlinePreviewRows;
    editor.codeMirror.operation(() => {
      inlinePreviewRows = Object.keys(previewsObj).map(line => {
        const lineNum = parseInt(line, 10);
        return React.createElement(InlinePreviewRow, {
          editor,
          key: line,
          line: lineNum,
          previews: previewsObj[line],
        });
      });
    });
    return div(null, inlinePreviewRows);
  }
}

const mapStateToProps = (state, props) => {
  const thread = getCurrentThread(state);
  const selectedFrame = getSelectedFrame(state, thread);
  const previews = getInlinePreviews(state, thread, selectedFrame?.id);
  // When we are paused, render only if currently open file is the one where debugger is paused
  if (
    (selectedFrame &&
      selectedFrame.location.source.id !== props.selectedSource.id) ||
    !hasPreviews(previews)
  ) {
    return {
      previews: null,
    };
  }

  return {
    previews,
  };
};

export default connect(mapStateToProps, {
  openElementInInspector: actions.openElementInInspectorCommand,
  highlightDomElement: actions.highlightDomElement,
  unHighlightDomElement: actions.unHighlightDomElement,
})(InlinePreviews);
