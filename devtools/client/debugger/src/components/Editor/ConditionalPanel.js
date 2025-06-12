/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React, { PureComponent } from "devtools/client/shared/vendor/react";
import {
  div,
  input,
  button,
  form,
  label,
} from "devtools/client/shared/vendor/react-dom-factories";
import ReactDOM from "devtools/client/shared/vendor/react-dom";
import PropTypes from "devtools/client/shared/vendor/react-prop-types";
import { connect } from "devtools/client/shared/vendor/react-redux";
import { toEditorLine } from "../../utils/editor/index";
import { createEditor } from "../../utils/editor/create-editor";
import actions from "../../actions/index";
import { markerTypes } from "../../constants";

import {
  getClosestBreakpoint,
  getConditionalPanelLocation,
  getLogPointStatus,
} from "../../selectors/index";

const classnames = require("resource://devtools/client/shared/classnames.js");

export class ConditionalPanel extends PureComponent {
  cbPanel;
  input;
  codeMirror;
  panelNode;
  scrollParent;

  constructor() {
    super();
    this.cbPanel = null;
    this.breakpointPanelEditor = null;
    this.formRef = React.createRef();
  }

  static get propTypes() {
    return {
      breakpoint: PropTypes.object,
      closeConditionalPanel: PropTypes.func.isRequired,
      editor: PropTypes.object.isRequired,
      location: PropTypes.any.isRequired,
      log: PropTypes.bool.isRequired,
      openConditionalPanel: PropTypes.func.isRequired,
      setBreakpointOptions: PropTypes.func.isRequired,
      selectedSource: PropTypes.object.isRequired,
    };
  }

  removeBreakpointPanelEditor() {
    if (this.breakpointPanelEditor) {
      this.breakpointPanelEditor.destroy();
    }
    this.breakpointPanelEditor = null;
  }

  keepFocusOnInput() {
    if (this.input) {
      this.input.focus();
    } else if (this.breakpointPanelEditor) {
      if (!this.breakpointPanelEditor.isDestroyed()) {
        this.breakpointPanelEditor.focus();
      }
    }
  }

  onFormSubmit = e => {
    if (e && e.preventDefault) {
      e.preventDefault();
    }
    const formData = new FormData(this.formRef.current);
    const showStacktrace = formData.get("showStacktrace") === "on";

    if (
      !this.breakpointPanelEditor ||
      this.breakpointPanelEditor.isDestroyed()
    ) {
      return;
    }
    const expression = this.breakpointPanelEditor.getText(null);
    this.saveAndClose(expression, showStacktrace);
  };

  onPanelBlur = e => {
    // if the focus is outside of the conditional panel,
    // close/hide the conditional panel
    if (
      e.relatedTarget &&
      e.relatedTarget.closest(".conditional-breakpoint-panel-container")
    ) {
      return;
    }
    this.props.closeConditionalPanel();
  };

  /**
   * Set the breakpoint/logpoint if expression isn't empty, and close the panel.
   *
   * @param {String} expression: The expression that will be used for setting the
   *        conditional breakpoint/logpoint
   * @param {Boolean} showStacktrace: Whether to show the stacktrace for the logpoint
   */
  saveAndClose = (expression = null, showStacktrace = false) => {
    if (typeof expression === "string") {
      const trimmedExpression = expression.trim();
      if (trimmedExpression) {
        this.setBreakpoint(trimmedExpression, showStacktrace);
      } else if (this.props.breakpoint) {
        // if the user was editing the condition/log of an existing breakpoint,
        // we remove the condition/log.
        this.setBreakpoint(null);
      }
    }

    this.props.closeConditionalPanel();
  };

  /**
   * Handle inline editor keydown event
   *
   * @param {Event} e: The keydown event
   */
  onKey = e => {
    if (e.key === "Enter" && !e.shiftKey) {
      e.preventDefault();
      this.formRef.current.requestSubmit();
    } else if (e.key === "Escape") {
      this.props.closeConditionalPanel();
    }
  };

  /**
   * Handle inline editor blur event
   *
   * @param {Event} e: The blur event
   */
  onBlur = e => {
    let explicitOriginalTarget = e?.explicitOriginalTarget;
    // The explicit original target can be a text node, in such case retrieve its parent
    // element so we can use `closest` on it.
    if (explicitOriginalTarget && !Element.isInstance(explicitOriginalTarget)) {
      explicitOriginalTarget = explicitOriginalTarget.parentElement;
    }

    if (
      // if there is no event
      // or if the focus is the conditional panel
      // do not close the conditional panel
      !e ||
      (explicitOriginalTarget &&
        explicitOriginalTarget.closest(
          ".conditional-breakpoint-panel-container"
        ))
    ) {
      return;
    }

    this.props.closeConditionalPanel();
  };

  setBreakpoint(value, showStacktrace) {
    const { log, breakpoint } = this.props;
    // If breakpoint is `pending`, props will not contain a breakpoint.
    // If source is a URL without location, breakpoint will contain no generatedLocation.
    const location =
      breakpoint && breakpoint.generatedLocation
        ? breakpoint.generatedLocation
        : this.props.location;
    const options = breakpoint ? breakpoint.options : {};
    const type = log ? "logValue" : "condition";
    return this.props.setBreakpointOptions(location, {
      ...options,
      [type]: value,
      showStacktrace,
    });
  }

  clearConditionalPanel() {
    if (this.cbPanel) {
      this.cbPanel.clear();
      this.cbPanel = null;
    }
    if (this.scrollParent) {
      this.scrollParent.removeEventListener("scroll", this.repositionOnScroll);
    }
  }

  repositionOnScroll = () => {
    if (this.panelNode && this.scrollParent) {
      const { scrollLeft } = this.scrollParent;
      this.panelNode.style.transform = `translateX(${scrollLeft}px)`;
    }
  };

  showConditionalPanel(prevProps) {
    const { location, log, editor, breakpoint, selectedSource } = this.props;

    if (!selectedSource || !location) {
      this.removeBreakpointPanelEditor();
      return;
    }
    // When breakpoint is removed
    if (prevProps?.breakpoint && !breakpoint) {
      editor.removeLineContentMarker(markerTypes.CONDITIONAL_BP_MARKER);
      this.removeBreakpointPanelEditor();
      return;
    }
    if (selectedSource.id !== location.source.id) {
      editor.removeLineContentMarker(markerTypes.CONDITIONAL_BP_MARKER);
      this.removeBreakpointPanelEditor();
      return;
    }
    const line = toEditorLine(location.source, location.line || 0);
    editor.setLineContentMarker({
      id: markerTypes.CONDITIONAL_BP_MARKER,
      lines: [{ line }],
      renderAsBlock: true,
      createLineElementNode: () => {
        // Create a Codemirror editor for the breakpoint panel

        const onEnterKeyMapConfig = {
          preventDefault: true,
          stopPropagation: true,
          run: () => this.formRef.current.requestSubmit(),
        };

        const breakpointPanelEditor = createEditor({
          cm6: true,
          readOnly: false,
          lineNumbers: false,
          placeholder: L10N.getStr(
            log
              ? "editor.conditionalPanel.logPoint.placeholder2"
              : "editor.conditionalPanel.placeholder2"
          ),
          keyMap: [
            {
              key: "Enter",
              ...onEnterKeyMapConfig,
            },
            {
              key: "Mod-Enter",
              ...onEnterKeyMapConfig,
            },
            {
              key: "Escape",
              preventDefault: true,
              stopPropagation: true,
              run: () => this.props.closeConditionalPanel(),
            },
          ],
        });

        this.breakpointPanelEditor = breakpointPanelEditor;
        return this.renderConditionalPanel(this.props, breakpointPanelEditor);
      },
    });
  }

  // FIXME: https://bugzilla.mozilla.org/show_bug.cgi?id=1774507
  UNSAFE_componentWillMount() {
    this.showConditionalPanel();
  }

  componentDidUpdate(prevProps) {
    this.showConditionalPanel(prevProps);
    this.keepFocusOnInput();
  }

  componentWillUnmount() {
    // This is called if CodeMirror is re-initializing itself before the
    // user closes the conditional panel. Clear the widget, and re-render it
    // as soon as this component gets remounted
    const { editor } = this.props;
    editor.removeLineContentMarker(markerTypes.CONDITIONAL_BP_MARKER);
    this.removeBreakpointPanelEditor();
  }

  componentDidMount() {
    if (this.formRef && this.formRef.current) {
      const checkbox = this.formRef.current.querySelector("#showStacktrace");
      if (checkbox) {
        checkbox.checked = this.props.breakpoint?.options?.showStacktrace;
      }
    }
  }

  renderToWidget(props) {
    if (this.cbPanel) {
      this.clearConditionalPanel();
    }
    const { location, editor } = props;
    if (!location) {
      return;
    }

    const editorLine = toEditorLine(location.source, location.line || 0);
    this.cbPanel = editor.codeMirror.addLineWidget(
      editorLine,
      this.renderConditionalPanel(props, editor),
      {
        coverGutter: true,
        noHScroll: true,
      }
    );

    if (this.input) {
      let parent = this.input.parentNode;
      while (parent) {
        if (
          HTMLElement.isInstance(parent) &&
          parent.classList.contains("CodeMirror-scroll")
        ) {
          this.scrollParent = parent;
          break;
        }
        parent = parent.parentNode;
      }

      if (this.scrollParent) {
        this.scrollParent.addEventListener("scroll", this.repositionOnScroll);
        this.repositionOnScroll();
      }
    }
  }

  setupAndAppendInlineEditor = (el, editor) => {
    editor.appendToLocalElement(el);
    editor.on("blur", e => this.onBlur(e));

    editor.setText(this.getDefaultValue());
    editor.focus();
    editor.selectAll();
  };

  getDefaultValue() {
    const { breakpoint, log } = this.props;
    const options = breakpoint?.options || {};
    const value = log ? options.logValue : options.condition;
    return value || "";
  }

  renderConditionalPanel(props, editor) {
    const { log } = props;
    const panel = document.createElement("div");
    const isWindows = Services.appinfo.OS.startsWith("WINNT");

    const isCreating = !this.props.breakpoint;

    const saveButton = button(
      {
        type: "submit",
        id: "save-logpoint",
        className: "devtools-button conditional-breakpoint-panel-save-button",
      },
      L10N.getStr(
        isCreating
          ? "editor.conditionalPanel.create"
          : "editor.conditionalPanel.update"
      )
    );

    const cancelButton = button(
      {
        type: "button",
        className: "devtools-button conditional-breakpoint-panel-cancel-button",
        onClick: () => this.props.closeConditionalPanel(),
      },
      L10N.getStr("editor.conditionalPanel.cancel")
    );

    // CodeMirror6 can't have margin on a block widget, so we need to wrap the actual
    // panel inside a container which won't have any margin
    const reactElPanel = div(
      {
        className: "conditional-breakpoint-panel-container",
        onBlur: this.onPanelBlur,
        tabIndex: -1,
      },
      form(
        {
          className: classnames("conditional-breakpoint-panel", {
            "log-point": log,
          }),
          onSubmit: this.onFormSubmit,
          ref: this.formRef,
        },
        div(
          {
            className: "input-container",
          },
          div(
            {
              className: "prompt",
            },
            "»"
          ),
          div({
            className: "inline-codemirror-container",
            ref: el => this.setupAndAppendInlineEditor(el, editor),
          })
        ),
        div(
          {
            className: "conditional-breakpoint-panel-controls",
          },
          log
            ? label(
                {
                  className: "conditional-breakpoint-panel-checkbox-label",
                  htmlFor: "showStacktrace",
                },
                input({
                  type: "checkbox",
                  id: "showStacktrace",
                  name: "showStacktrace",
                  defaultChecked:
                    this.props.breakpoint?.options?.showStacktrace,
                  "aria-label": L10N.getStr(
                    "editor.conditionalPanel.logPoint.showStacktrace"
                  ),
                }),
                L10N.getStr("editor.conditionalPanel.logPoint.showStacktrace")
              )
            : null,
          div(
            {
              className: "conditional-breakpoint-panel-buttons",
            },
            isWindows ? saveButton : cancelButton,
            isWindows ? cancelButton : saveButton
          )
        )
      )
    );
    ReactDOM.render(reactElPanel, panel);
    return panel;
  }

  render() {
    return null;
  }
}

const mapStateToProps = state => {
  const location = getConditionalPanelLocation(state);

  if (!location) {
    return {};
  }

  const breakpoint = getClosestBreakpoint(state, location);

  return {
    breakpoint,
    location,
    log: getLogPointStatus(state),
  };
};

const { setBreakpointOptions, openConditionalPanel, closeConditionalPanel } =
  actions;

const mapDispatchToProps = {
  setBreakpointOptions,
  openConditionalPanel,
  closeConditionalPanel,
};

export default connect(mapStateToProps, mapDispatchToProps)(ConditionalPanel);
