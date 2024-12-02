/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import PropTypes from "devtools/client/shared/vendor/react-prop-types";
import React, { PureComponent } from "devtools/client/shared/vendor/react";
import { div } from "devtools/client/shared/vendor/react-dom-factories";
import { bindActionCreators } from "devtools/client/shared/vendor/redux";
import ReactDOM from "devtools/client/shared/vendor/react-dom";
import { connect } from "devtools/client/shared/vendor/react-redux";

import { getLineText, isLineBlackboxed } from "./../../utils/source";
import { createLocation } from "./../../utils/location";
import { getIndentation } from "../../utils/indentation";
import { isWasm } from "../../utils/wasm";
import { features } from "../../utils/prefs";
import { markerTypes } from "../../constants";
import { asSettled, isFulfilled, isRejected } from "../../utils/async-value";

import {
  getActiveSearch,
  getSelectedLocation,
  getSelectedSource,
  getSelectedSourceTextContent,
  getSelectedBreakableLines,
  getConditionalPanelLocation,
  getIsCurrentThreadPaused,
  getSkipPausing,
  getInlinePreview,
  getBlackBoxRanges,
  isSourceBlackBoxed,
  getHighlightedLineRangeForSelectedSource,
  isSourceMapIgnoreListEnabled,
  isSourceOnSourceMapIgnoreList,
  isMapScopesEnabled,
  getSelectedTraceIndex,
  getShouldScrollToSelectedLocation,
} from "../../selectors/index";

// Redux actions
import actions from "../../actions/index";

import SearchInFileBar from "./SearchInFileBar";
import HighlightLines from "./HighlightLines";
import Preview from "./Preview/index";
import Breakpoints from "./Breakpoints";
import ColumnBreakpoints from "./ColumnBreakpoints";
import DebugLine from "./DebugLine";
import HighlightLine from "./HighlightLine";
import EmptyLines from "./EmptyLines";
import ConditionalPanel from "./ConditionalPanel";
import InlinePreviews from "./InlinePreviews";
import Exceptions from "./Exceptions";
import BlackboxLines from "./BlackboxLines";

import {
  fromEditorLine,
  showSourceText,
  setDocument,
  resetLineNumberFormat,
  getEditor,
  lineAtHeight,
  toSourceLine,
  getDocument,
  toEditorPosition,
  getSourceLocationFromMouseEvent,
  hasDocument,
  onMouseOver,
  startOperation,
  endOperation,
} from "../../utils/editor/index";

import {
  resizeToggleButton,
  getLineNumberWidth,
  resizeBreakpointGutter,
} from "../../utils/ui";

const { debounce } = require("resource://devtools/shared/debounce.js");
const classnames = require("resource://devtools/client/shared/classnames.js");

const { appinfo } = Services;
const isMacOS = appinfo.OS === "Darwin";

function isSecondary(ev) {
  return isMacOS && ev.ctrlKey && ev.button === 0;
}

function isCmd(ev) {
  return isMacOS ? ev.metaKey : ev.ctrlKey;
}

const cssVars = {
  searchbarHeight: "var(--editor-searchbar-height)",
};

class Editor extends PureComponent {
  static get propTypes() {
    return {
      selectedSource: PropTypes.object,
      selectedSourceTextContent: PropTypes.object,
      selectedSourceIsBlackBoxed: PropTypes.bool,
      closeTab: PropTypes.func.isRequired,
      toggleBreakpointAtLine: PropTypes.func.isRequired,
      conditionalPanelLocation: PropTypes.object,
      closeConditionalPanel: PropTypes.func.isRequired,
      openConditionalPanel: PropTypes.func.isRequired,
      updateViewport: PropTypes.func.isRequired,
      isPaused: PropTypes.bool.isRequired,
      addBreakpointAtLine: PropTypes.func.isRequired,
      continueToHere: PropTypes.func.isRequired,
      updateCursorPosition: PropTypes.func.isRequired,
      jumpToMappedLocation: PropTypes.func.isRequired,
      selectedLocation: PropTypes.object,
      startPanelSize: PropTypes.number.isRequired,
      endPanelSize: PropTypes.number.isRequired,
      searchInFileEnabled: PropTypes.bool.isRequired,
      inlinePreviewEnabled: PropTypes.bool.isRequired,
      skipPausing: PropTypes.bool.isRequired,
      blackboxedRanges: PropTypes.object.isRequired,
      breakableLines: PropTypes.object.isRequired,
      highlightedLineRange: PropTypes.object,
      isSourceOnIgnoreList: PropTypes.bool,
      mapScopesEnabled: PropTypes.bool,
      shouldScrollToSelectedLocation: PropTypes.bool,
    };
  }

  $editorWrapper;
  constructor(props) {
    super(props);

    this.state = {
      editor: null,
    };
  }

  // FIXME: https://bugzilla.mozilla.org/show_bug.cgi?id=1774507
  UNSAFE_componentWillReceiveProps(nextProps) {
    let { editor } = this.state;
    const prevEditor = editor;

    if (!editor) {
      // See Bug 1913061
      if (!nextProps.selectedSource) {
        return;
      }
      editor = this.setupEditor();
    }

    if (!features.codemirrorNext) {
      const shouldUpdateSize =
        nextProps.startPanelSize !== this.props.startPanelSize ||
        nextProps.endPanelSize !== this.props.endPanelSize;

      startOperation();
      if (shouldUpdateSize) {
        editor.codeMirror.setSize();
      }
      this.setTextContent(nextProps, editor, prevEditor);
      endOperation();

      if (this.props.selectedSource != nextProps.selectedSource) {
        this.props.updateViewport();
        resizeBreakpointGutter(editor.codeMirror);
        resizeToggleButton(getLineNumberWidth(editor.codeMirror));
      }
    } else {
      // For codemirror 6
      this.setTextContent(nextProps, editor, prevEditor);
    }
  }

  async setTextContent(nextProps, editor, prevEditor) {
    const shouldUpdateText =
      nextProps.selectedSource !== this.props.selectedSource ||
      nextProps.selectedSourceTextContent?.value !==
        this.props.selectedSourceTextContent?.value ||
      // If the selectedSource gets set before the editor get selected, make sure we update the text
      prevEditor !== editor;

    const shouldScroll =
      nextProps.selectedLocation &&
      nextProps.shouldScrollToSelectedLocation &&
      this.shouldScrollToLocation(nextProps);

    if (shouldUpdateText) {
      await this.setText(nextProps, editor);
    }

    if (shouldScroll) {
      this.scrollToLocation(nextProps, editor);
    }
  }

  onEditorUpdated = viewUpdate => {
    if (viewUpdate.docChanged || viewUpdate.geometryChanged) {
      resizeToggleButton(
        viewUpdate.view.dom.querySelector(".cm-gutters").clientWidth
      );
      this.props.updateViewport();
    } else if (viewUpdate.selectionSet) {
      this.onCursorChange();
    }
  };

  setupEditor() {
    const editor = getEditor(features.codemirrorNext);

    // disables the default search shortcuts
    editor._initShortcuts = () => {};

    const node = ReactDOM.findDOMNode(this);
    const mountEl = node.querySelector(".editor-mount");
    if (node instanceof HTMLElement) {
      editor.appendToLocalElement(mountEl);
    }

    if (!features.codemirrorNext) {
      const { codeMirror } = editor;

      this.abortController = new window.AbortController();

      // CodeMirror refreshes its internal state on window resize, but we need to also
      // refresh it when the side panels are resized.
      // We could have a ResizeObserver instead, but we wouldn't be able to differentiate
      // between window resize and side panel resize and as a result, might refresh
      // codeMirror twice, which is wasteful.
      window.document
        .querySelector(".editor-pane")
        .addEventListener("resizeend", () => codeMirror.refresh(), {
          signal: this.abortController.signal,
        });

      codeMirror.on("gutterClick", this.onGutterClick);
      codeMirror.on("cursorActivity", this.onCursorChange);

      const codeMirrorWrapper = codeMirror.getWrapperElement();
      // Set code editor wrapper to be focusable
      codeMirrorWrapper.tabIndex = 0;
      codeMirrorWrapper.addEventListener("click", e => this.onClick(e));
      codeMirrorWrapper.addEventListener("mouseover", onMouseOver(editor));
      codeMirrorWrapper.addEventListener("contextmenu", event =>
        this.openMenu(event)
      );

      codeMirror.on("scroll", this.onEditorScroll);
      this.onEditorScroll();
    } else {
      editor.setUpdateListener(this.onEditorUpdated);
      editor.setGutterEventListeners({
        click: (event, cm, line) => {
          // Ignore clicks on the code folding button
          if (
            event.target.className.includes("cm6-dt-foldgutter__toggle-button")
          ) {
            return;
          }
          // Clicking any where on the fold gutter (except on a code folding button)
          // should toggle the breakpoint for this line, if possible.
          if (event.target.className.includes("cm-foldGutter")) {
            this.props.toggleBreakpointAtLine(line);
            return;
          }
          this.onGutterClick(cm, line, null, event);
        },
        contextmenu: (event, cm, line) => this.openMenu(event, line),
      });
      editor.addEditorDOMEventListeners({
        click: (event, cm, line, column) => this.onClick(event, line, column),
        contextmenu: (event, cm, line, column) =>
          this.openMenu(event, line, column),
        mouseover: onMouseOver(editor),
      });
    }
    this.setState({ editor });
    // Used for tests
    Object.defineProperty(window, "codeMirrorSourceEditorTestInstance", {
      get() {
        return editor;
      },
    });
    return editor;
  }

  componentDidMount() {
    const { shortcuts } = this.context;

    shortcuts.on(L10N.getStr("toggleBreakpoint.key"), this.onToggleBreakpoint);
    shortcuts.on(
      L10N.getStr("toggleCondPanel.breakpoint.key"),
      this.onToggleConditionalPanel
    );
    shortcuts.on(
      L10N.getStr("toggleCondPanel.logPoint.key"),
      this.onToggleLogPanel
    );
    shortcuts.on(
      L10N.getStr("sourceTabs.closeTab.key"),
      this.onCloseShortcutPress
    );
    shortcuts.on("Esc", this.onEscape);
  }

  onCloseShortcutPress = e => {
    const { selectedSource } = this.props;
    if (selectedSource) {
      e.preventDefault();
      e.stopPropagation();
      this.props.closeTab(selectedSource, "shortcut");
    }
  };

  componentDidUpdate(prevProps, prevState) {
    const {
      selectedSource,
      blackboxedRanges,
      isSourceOnIgnoreList,
      breakableLines,
    } = this.props;
    const { editor } = this.state;

    if (!selectedSource || !editor) {
      return;
    }

    // Sets the breakables lines for codemirror 6
    if (features.codemirrorNext) {
      const shouldUpdateBreakableLines =
        prevProps.breakableLines.size !== this.props.breakableLines.size ||
        prevProps.selectedSource?.id !== selectedSource.id ||
        // Make sure we update after the editor has loaded
        (!prevState.editor && !!editor);

      const isSourceWasm = isWasm(selectedSource.id);

      if (shouldUpdateBreakableLines) {
        editor.setLineGutterMarkers([
          {
            id: markerTypes.EMPTY_LINE_MARKER,
            lineClassName: "empty-line",
            condition: line => {
              const lineNumber = fromEditorLine(
                selectedSource.id,
                line,
                isSourceWasm
              );
              return !breakableLines.has(lineNumber);
            },
          },
        ]);
      }

      editor.setLineGutterMarkers([
        {
          id: markerTypes.BLACKBOX_LINE_GUTTER_MARKER,
          lineClassName: "blackboxed-line",
          condition: line => {
            const lineNumber = fromEditorLine(selectedSource.id, line);
            return isLineBlackboxed(
              blackboxedRanges[selectedSource.url],
              lineNumber,
              isSourceOnIgnoreList
            );
          },
        },
      ]);

      if (
        prevProps.selectedSource?.id !== selectedSource.id ||
        prevProps.blackboxedRanges[selectedSource.url]?.length !==
          blackboxedRanges[selectedSource.url]?.length ||
        (!prevState.editor && !!editor)
      ) {
        if (blackboxedRanges[selectedSource.url] == undefined) {
          editor.removeLineContentMarker(markerTypes.BLACKBOX_LINE_MARKER);
          return;
        }

        const lines = [];
        for (const range of blackboxedRanges[selectedSource.url]) {
          for (let line = range.start.line; line <= range.end.line; line++) {
            lines.push({ line });
          }
        }

        editor.setLineContentMarker({
          id: markerTypes.BLACKBOX_LINE_MARKER,
          lineClassName: "blackboxed-line",
          // If the the whole source is blackboxed, lets just mark all positions.
          shouldMarkAllLines: !blackboxedRanges[selectedSource.url].length,
          lines,
        });
      }
    }
  }

  componentWillUnmount() {
    const { editor } = this.state;
    const { shortcuts } = this.context;
    shortcuts.off(L10N.getStr("sourceTabs.closeTab.key"));
    shortcuts.off(L10N.getStr("toggleBreakpoint.key"));
    shortcuts.off(L10N.getStr("toggleCondPanel.breakpoint.key"));
    shortcuts.off(L10N.getStr("toggleCondPanel.logPoint.key"));

    if (this.abortController) {
      this.abortController.abort();
      this.abortController = null;
    }

    if (editor) {
      if (!features.codemirrorNext) {
        editor.codeMirror.off("scroll", this.onEditorScroll);
      }
      editor.destroy();
      this.setState({ editor: null });
    }
  }

  getCurrentPosition() {
    const { editor } = this.state;
    const { selectedLocation } = this.props;
    if (!selectedLocation) {
      return null;
    }
    let { line, column } = selectedLocation;
    // When no specific line has been selected, fallback to the current cursor posiiton
    if (line == 0) {
      const selectionCursor = editor.getSelectionCursor();
      line = toSourceLine(
        selectedLocation.source.id,
        selectionCursor.from.line
      );
      column = selectionCursor.from.ch + 1;
    }
    return { line, column };
  }

  onToggleBreakpoint = e => {
    e.preventDefault();
    e.stopPropagation();

    const currentPosition = this.getCurrentPosition();
    if (!currentPosition || typeof currentPosition.line !== "number") {
      return;
    }

    this.props.toggleBreakpointAtLine(currentPosition.line);
  };

  onToggleLogPanel = e => {
    e.stopPropagation();
    e.preventDefault();
    this.toggleBreakpointPanel(true);
  };

  onToggleConditionalPanel = e => {
    e.stopPropagation();
    e.preventDefault();
    this.toggleBreakpointPanel(false);
  };

  toggleBreakpointPanel(logPanel) {
    const {
      conditionalPanelLocation,
      closeConditionalPanel,
      openConditionalPanel,
      selectedSource,
    } = this.props;

    const currentPosition = this.getCurrentPosition();

    if (conditionalPanelLocation) {
      return closeConditionalPanel();
    }

    if (!selectedSource || typeof currentPosition?.line !== "number") {
      return null;
    }

    return openConditionalPanel(
      createLocation({
        line: currentPosition.line,
        column: currentPosition.column,
        source: selectedSource,
      }),
      logPanel
    );
  }

  onEditorScroll = debounce(this.props.updateViewport, 75);

  /*
   * The default Esc command is overridden in the CodeMirror keymap to allow
   * the Esc keypress event to be catched by the toolbox and trigger the
   * split console. Restore it here, but preventDefault if and only if there
   * is a multiselection.
   */
  onEscape = e => {
    if (!this.state.editor) {
      return;
    }

    if (!features.codemirrorNext) {
      const { codeMirror } = this.state.editor;
      if (codeMirror.listSelections().length > 1) {
        codeMirror.execCommand("singleSelection");
        e.preventDefault();
      }
    }
  };
  // Note: The line is optional, if not passed it fallsback to lineAtHeight.
  openMenu(event, line, ch) {
    event.stopPropagation();
    event.preventDefault();

    const {
      selectedSource,
      selectedSourceTextContent,
      conditionalPanelLocation,
      closeConditionalPanel,
    } = this.props;

    const { editor } = this.state;

    if (!selectedSource || !editor) {
      return;
    }

    // only allow one conditionalPanel location.
    if (conditionalPanelLocation) {
      closeConditionalPanel();
    }

    const target = event.target;
    const { id: sourceId } = selectedSource;
    if (!features.codemirrorNext) {
      line = line ?? lineAtHeight(editor, sourceId, event);
    }

    if (typeof line != "number") {
      return;
    }

    if (
      // handles codemirror 6
      target.classList.contains("cm-gutter") ||
      target.classList.contains("cm-gutterElement") ||
      // handles codemirror 5
      target.classList.contains("CodeMirror-linenumber")
    ) {
      const location = createLocation({
        line,
        column: undefined,
        source: selectedSource,
      });

      const lineText = getLineText(
        sourceId,
        selectedSourceTextContent,
        line
      ).trim();

      const lineObject = { from: { line, ch }, to: { line, ch } };

      this.props.showEditorGutterContextMenu(
        event,
        lineObject,
        location,
        lineText
      );
      return;
    }

    if (target.getAttribute("id") === "columnmarker") {
      return;
    }

    let location;
    if (features.codemirrorNext) {
      location = createLocation({
        source: selectedSource,
        line: fromEditorLine(
          selectedSource.id,
          line,
          isWasm(selectedSource.id)
        ),
        column: isWasm(selectedSource.id) ? 0 : ch + 1,
      });
    } else {
      location = getSourceLocationFromMouseEvent(editor, selectedSource, event);
    }

    const lineObject = editor.getSelectionCursor();
    this.props.showEditorContextMenu(event, editor, lineObject, location);
  }

  /**
   * CodeMirror event handler, called whenever the cursor moves
   * for user-driven or programatic reasons.
   */
  onCursorChange = () => {
    const { editor } = this.state;
    const selectionCursor = editor.getSelectionCursor();
    const { line, ch } = selectionCursor.to;
    this.props.selectLocation(
      createLocation({
        source: this.props.selectedSource,
        line: toSourceLine(this.props.selectedSource.id, line),
        column: ch,
      }),
      {
        // Reset the context, so that we don't switch to original
        // while moving the cursor within a bundle
        keepContext: false,

        // Avoid highlighting the selected line
        highlight: false,

        // Avoid scrolling to the selected line, it's already visible
        scroll: false,
      }
    );
  };

  onGutterClick = (cm, line, gutter, ev) => {
    const {
      selectedSource,
      conditionalPanelLocation,
      closeConditionalPanel,
      addBreakpointAtLine,
      continueToHere,
      breakableLines,
      blackboxedRanges,
      isSourceOnIgnoreList,
    } = this.props;

    // ignore right clicks in the gutter
    if (isSecondary(ev) || ev.button === 2 || !selectedSource) {
      return;
    }

    if (conditionalPanelLocation) {
      closeConditionalPanel();
      return;
    }

    if (gutter === "CodeMirror-foldgutter") {
      return;
    }

    const sourceLine = toSourceLine(selectedSource.id, line);
    if (typeof sourceLine !== "number") {
      return;
    }

    // ignore clicks on a non-breakable line
    if (!breakableLines.has(sourceLine)) {
      return;
    }

    if (isCmd(ev)) {
      continueToHere(
        createLocation({
          line: sourceLine,
          column: undefined,
          source: selectedSource,
        })
      );
      return;
    }

    addBreakpointAtLine(
      sourceLine,
      ev.altKey,
      ev.shiftKey ||
        isLineBlackboxed(
          blackboxedRanges[selectedSource.url],
          sourceLine,
          isSourceOnIgnoreList
        )
    );
  };

  onClick(e, line, ch) {
    const { selectedSource, updateCursorPosition, jumpToMappedLocation } =
      this.props;

    if (selectedSource) {
      let sourceLocation;
      if (features.codemirrorNext) {
        sourceLocation = createLocation({
          source: selectedSource,
          line: fromEditorLine(
            selectedSource.id,
            line,
            isWasm(selectedSource.id)
          ),
          column: isWasm(selectedSource.id) ? 0 : ch + 1,
        });
      } else {
        sourceLocation = getSourceLocationFromMouseEvent(
          this.state.editor,
          selectedSource,
          e
        );
      }

      if (e.metaKey && e.altKey) {
        jumpToMappedLocation(sourceLocation);
      }

      updateCursorPosition(sourceLocation);
    }
  }

  shouldScrollToLocation(nextProps) {
    if (
      !nextProps.selectedLocation?.line ||
      !nextProps.selectedSourceTextContent
    ) {
      return false;
    }

    const { selectedLocation, selectedSourceTextContent } = this.props;
    const contentChanged =
      !selectedSourceTextContent?.value &&
      nextProps.selectedSourceTextContent?.value;
    const locationChanged = selectedLocation !== nextProps.selectedLocation;

    return contentChanged || locationChanged;
  }

  scrollToLocation(nextProps, editor) {
    const { selectedLocation, selectedSource } = nextProps;

    let { line, column } = toEditorPosition(selectedLocation);

    if (selectedSource && hasDocument(selectedSource.id)) {
      const doc = getDocument(selectedSource.id);
      const lineText = doc.getLine(line);
      column = Math.max(column, getIndentation(lineText));
    }
    editor.scrollTo(line, column);
  }

  async setText(props, editor) {
    const { selectedSource, selectedSourceTextContent } = props;

    if (!editor) {
      return;
    }

    // check if we previously had a selected source
    if (!selectedSource) {
      if (!features.codemirrorNext) {
        this.clearEditor();
      }
      return;
    }

    if (!selectedSourceTextContent?.value) {
      this.showLoadingMessage(editor);
      return;
    }

    if (isRejected(selectedSourceTextContent)) {
      let { value } = selectedSourceTextContent;
      if (typeof value !== "string") {
        value = "Unexpected source error";
      }

      this.showErrorMessage(value);
      return;
    }

    if (!features.codemirrorNext) {
      showSourceText(editor, selectedSource, selectedSourceTextContent);
    } else {
      await editor.setText(
        selectedSourceTextContent.value.value,
        selectedSource.id
      );
    }
  }

  clearEditor() {
    const { editor } = this.state;
    if (!editor) {
      return;
    }

    const doc = editor.createDocument("", { name: "text" });
    editor.replaceDocument(doc);
    resetLineNumberFormat(editor);
  }

  showErrorMessage(msg) {
    const { editor } = this.state;
    if (!editor) {
      return;
    }

    let error;
    if (msg.includes("WebAssembly binary source is not available")) {
      error = L10N.getStr("wasmIsNotAvailable");
    } else {
      error = L10N.getFormatStr("errorLoadingText3", msg);
    }
    if (!features.codemirrorNext) {
      const doc = editor.createDocument(error, { name: "text" });
      editor.replaceDocument(doc);
      resetLineNumberFormat(editor);
    } else {
      editor.setText(error);
    }
  }

  showLoadingMessage(editor) {
    if (!features.codemirrorNext) {
      // Create the "loading message" document only once
      let doc = getDocument("loading");
      if (!doc) {
        doc = editor.createDocument(L10N.getStr("loadingText"), {
          name: "text",
        });
        setDocument("loading", doc);
      }
      // `createDocument` won't be used right away in the editor, we still need to
      // explicitely update it
      editor.replaceDocument(doc);
    } else {
      editor.setText(L10N.getStr("loadingText"));
    }
  }

  getInlineEditorStyles() {
    const { searchInFileEnabled } = this.props;

    if (searchInFileEnabled) {
      return {
        height: `calc(100% - ${cssVars.searchbarHeight})`,
      };
    }

    return {
      height: "100%",
    };
  }

  // eslint-disable-next-line complexity
  renderItems() {
    const {
      selectedSource,
      conditionalPanelLocation,
      isPaused,
      isTraceSelected,
      inlinePreviewEnabled,
      highlightedLineRange,
      blackboxedRanges,
      isSourceOnIgnoreList,
      selectedSourceIsBlackBoxed,
      mapScopesEnabled,
      selectedSourceTextContent,
    } = this.props;
    const { editor } = this.state;

    if (!selectedSource || !editor) {
      return null;
    }

    if (features.codemirrorNext) {
      // Only load the sub components if the content has loaded without issues.
      if (
        selectedSourceTextContent &&
        !isFulfilled(selectedSourceTextContent)
      ) {
        return null;
      }

      return React.createElement(
        React.Fragment,
        null,
        React.createElement(Breakpoints, { editor }),
        (isPaused || isTraceSelected) &&
          selectedSource.isOriginal &&
          !selectedSource.isPrettyPrinted &&
          !mapScopesEnabled
          ? null
          : React.createElement(Preview, {
              editor,
              editorRef: this.$editorWrapper,
            }),
        React.createElement(DebugLine, { editor, selectedSource }),
        React.createElement(HighlightLine, { editor }),
        React.createElement(Exceptions, { editor }),
        conditionalPanelLocation
          ? React.createElement(ConditionalPanel, {
              editor,
              selectedSource,
            })
          : null,
        (isPaused || isTraceSelected) &&
          inlinePreviewEnabled &&
          (!selectedSource.isOriginal ||
            selectedSource.isPrettyPrinted ||
            mapScopesEnabled)
          ? React.createElement(InlinePreviews, {
              editor,
            })
          : null,
        highlightedLineRange
          ? React.createElement(HighlightLines, {
              editor,
              range: highlightedLineRange,
            })
          : null,
        React.createElement(ColumnBreakpoints, {
          editor,
        })
      );
    }

    if (!getDocument(selectedSource.id)) {
      return null;
    }
    return div(
      null,
      React.createElement(DebugLine, null),
      React.createElement(HighlightLine, null),
      React.createElement(EmptyLines, {
        editor,
      }),
      React.createElement(Breakpoints, {
        editor,
      }),
      (isPaused || isTraceSelected) &&
        selectedSource.isOriginal &&
        !selectedSource.isPrettyPrinted &&
        !mapScopesEnabled
        ? null
        : React.createElement(Preview, {
            editor,
            editorRef: this.$editorWrapper,
          }),
      highlightedLineRange
        ? React.createElement(HighlightLines, {
            editor,
            range: highlightedLineRange,
          })
        : null,
      isSourceOnIgnoreList || selectedSourceIsBlackBoxed
        ? React.createElement(BlackboxLines, {
            editor,
            selectedSource,
            isSourceOnIgnoreList,
            blackboxedRangesForSelectedSource:
              blackboxedRanges[selectedSource.url],
          })
        : null,
      React.createElement(Exceptions, null),
      conditionalPanelLocation
        ? React.createElement(ConditionalPanel, {
            editor,
          })
        : null,
      React.createElement(ColumnBreakpoints, {
        editor,
      }),
      (isPaused || isTraceSelected) &&
        inlinePreviewEnabled &&
        (!selectedSource.isOriginal ||
          (selectedSource.isOriginal && selectedSource.isPrettyPrinted) ||
          (selectedSource.isOriginal && mapScopesEnabled))
        ? React.createElement(InlinePreviews, {
            editor,
          })
        : null
    );
  }

  renderSearchInFileBar() {
    if (!this.props.selectedSource) {
      return null;
    }
    return React.createElement(SearchInFileBar, {
      editor: this.state.editor,
    });
  }

  render() {
    const { selectedSourceIsBlackBoxed, skipPausing } = this.props;
    return div(
      {
        className: classnames("editor-wrapper", {
          blackboxed: selectedSourceIsBlackBoxed,
          "skip-pausing": skipPausing,
        }),
        ref: c => (this.$editorWrapper = c),
      },
      div({
        className: "editor-mount devtools-monospace",
        style: this.getInlineEditorStyles(),
      }),
      this.renderSearchInFileBar(),
      this.renderItems()
    );
  }
}

Editor.contextTypes = {
  shortcuts: PropTypes.object,
};

const mapStateToProps = state => {
  const selectedSource = getSelectedSource(state);
  const selectedLocation = getSelectedLocation(state);

  const selectedSourceTextContent = getSelectedSourceTextContent(state);

  return {
    selectedLocation,
    selectedSource,
    // Settled means the content loaded succesfully (fulfilled) or the there was
    // error (rejected)
    selectedSourceTextContent: asSettled(selectedSourceTextContent),
    selectedSourceIsBlackBoxed: selectedSource
      ? isSourceBlackBoxed(state, selectedSource)
      : null,
    isSourceOnIgnoreList:
      isSourceMapIgnoreListEnabled(state) &&
      isSourceOnSourceMapIgnoreList(state, selectedSource),
    searchInFileEnabled: getActiveSearch(state) === "file",
    conditionalPanelLocation: getConditionalPanelLocation(state),
    isPaused: getIsCurrentThreadPaused(state),
    isTraceSelected: getSelectedTraceIndex(state) != null,
    skipPausing: getSkipPausing(state),
    inlinePreviewEnabled: getInlinePreview(state),
    blackboxedRanges: getBlackBoxRanges(state),
    breakableLines: getSelectedBreakableLines(state),
    highlightedLineRange: getHighlightedLineRangeForSelectedSource(state),
    mapScopesEnabled: selectedSource?.isOriginal
      ? isMapScopesEnabled(state)
      : null,
    shouldScrollToSelectedLocation: getShouldScrollToSelectedLocation(state),
  };
};

const mapDispatchToProps = dispatch => ({
  ...bindActionCreators(
    {
      openConditionalPanel: actions.openConditionalPanel,
      closeConditionalPanel: actions.closeConditionalPanel,
      continueToHere: actions.continueToHere,
      toggleBreakpointAtLine: actions.toggleBreakpointAtLine,
      addBreakpointAtLine: actions.addBreakpointAtLine,
      jumpToMappedLocation: actions.jumpToMappedLocation,
      updateViewport: actions.updateViewport,
      updateCursorPosition: actions.updateCursorPosition,
      closeTab: actions.closeTab,
      showEditorContextMenu: actions.showEditorContextMenu,
      showEditorGutterContextMenu: actions.showEditorGutterContextMenu,
      selectLocation: actions.selectLocation,
    },
    dispatch
  ),
});

export default connect(mapStateToProps, mapDispatchToProps)(Editor);
