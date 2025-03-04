/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import PropTypes from "devtools/client/shared/vendor/react-prop-types";
import React, { PureComponent } from "devtools/client/shared/vendor/react";
import { connect } from "devtools/client/shared/vendor/react-redux";

import Popup from "./Popup";

import {
  getIsCurrentThreadPaused,
  getSelectedTraceIndex,
} from "../../../selectors/index";
import actions from "../../../actions/index";
import { features } from "../../../utils/prefs";

const EXCEPTION_MARKER = "mark-text-exception";

const LOADING_CLASS = "preview-loading-token";

class Preview extends PureComponent {
  target = null;
  constructor(props) {
    super(props);
    this.state = { selecting: false, loading: null };
  }

  static get propTypes() {
    return {
      editor: PropTypes.object.isRequired,
      editorRef: PropTypes.object.isRequired,
      isPaused: PropTypes.bool.isRequired,
      hasSelectedTrace: PropTypes.bool.isRequired,
      getExceptionPreview: PropTypes.func.isRequired,
      getPreview: PropTypes.func,
    };
  }

  componentDidMount() {
    if (features.codemirrorNext) {
      this.props.editor.on("tokenenter", this.onTokenEnter);
      this.props.editor.addEditorDOMEventListeners({
        mouseup: this.onMouseUp,
        mousedown: this.onMouseDown,
        scroll: this.onScroll,
      });
    } else {
      const { codeMirror } = this.props.editor;
      const codeMirrorWrapper = codeMirror.getWrapperElement();
      codeMirror.on("tokenenter", this.onTokenEnter);
      codeMirror.on("scroll", this.onScroll);
      codeMirrorWrapper.addEventListener("mouseup", this.onMouseUp);
      codeMirrorWrapper.addEventListener("mousedown", this.onMouseDown);
    }
  }

  componentWillUnmount() {
    if (features.codemirrorNext) {
      this.props.editor.off("tokenenter", this.onTokenEnter);
      this.props.editor.removeEditorDOMEventListeners({
        mouseup: this.onMouseUp,
        mousedown: this.onMouseDown,
        scroll: this.onScroll,
      });
    } else {
      const { codeMirror } = this.props.editor;
      const codeMirrorWrapper = codeMirror.getWrapperElement();

      codeMirror.off("tokenenter", this.onTokenEnter);
      codeMirror.off("scroll", this.onScroll);
      codeMirrorWrapper.removeEventListener("mouseup", this.onMouseUp);
      codeMirrorWrapper.removeEventListener("mousedown", this.onMouseDown);
    }
  }

  componentDidUpdate(_prevProps, prevState) {
    // Ensure that only one token is highlighted as "loading"
    const previous = prevState.loading;
    if (previous) {
      previous.classList.remove(LOADING_CLASS);
    }
    const { loading } = this.state;
    if (loading) {
      loading.classList.add(LOADING_CLASS);
    }
  }

  // Note that these events are emitted by utils/editor/tokens.js
  onTokenEnter = async ({ target, tokenPos }) => {
    // Use a temporary object to uniquely identify the asynchronous processing of this user event
    // and bail out if we started hovering another token.
    const tokenId = {};
    this.currentTokenId = tokenId;

    // Immediately highlight the hovered token as "loading"
    this.setState({ loading: target });

    const {
      editor,
      getPausedPreview,
      getTracerPreview,
      getExceptionPreview,
      isPaused,
      hasSelectedTrace,
    } = this.props;
    const isTargetException = target.closest(`.${EXCEPTION_MARKER}`);

    let preview;
    try {
      if (isTargetException) {
        preview = await getExceptionPreview(target, tokenPos, editor);
      }

      if (!preview && (hasSelectedTrace || isPaused) && !this.state.selecting) {
        if (hasSelectedTrace) {
          preview = await getTracerPreview(target, tokenPos, editor);
        }
        if (!preview && isPaused) {
          preview = await getPausedPreview(target, tokenPos, editor);
        }
      }
    } catch (e) {
      // Ignore any exception and dismiss the popup (as preview will be null)
    }

    // Prevent modifying state and showing this preview if we started hovering another token
    if (this.currentTokenId !== tokenId) {
      return;
    }

    this.setState({ loading: null, preview });
  };

  onMouseUp = () => {
    if (this.props.isPaused || this.props.hasSelectedTrace) {
      this.setState({ selecting: false });
    }
  };

  onMouseDown = () => {
    if (this.props.isPaused || this.props.hasSelectedTrace) {
      this.setState({ selecting: true });
    }
  };

  onScroll = () => {
    if (this.props.isPaused || this.props.hasSelectedTrace) {
      this.clearPreview();
    }
  };

  clearPreview = () => {
    this.setState({ loading: null, preview: null });
  };

  render() {
    if (this.state.selecting) {
      return null;
    }

    const { preview } = this.state;
    if (!preview) {
      return null;
    }

    return React.createElement(Popup, {
      preview,
      editor: this.props.editor,
      editorRef: this.props.editorRef,
      clearPreview: this.clearPreview,
    });
  }
}

const mapStateToProps = state => {
  return {
    isPaused: getIsCurrentThreadPaused(state),
    hasSelectedTrace: getSelectedTraceIndex(state) != null,
  };
};

export default connect(mapStateToProps, {
  addExpression: actions.addExpression,
  getPausedPreview: actions.getPausedPreview,
  getTracerPreview: actions.getTracerPreview,
  getExceptionPreview: actions.getExceptionPreview,
})(Preview);
