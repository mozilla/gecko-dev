/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import SourceEditor from "devtools/client/shared/sourceeditor/editor";
import { features, prefs } from "../prefs";

/**
 * Create a SourceEditor
 *
 * @param {Object} config: SourceEditor config object
 * @returns
 */
export function createEditor(config = { cm6: false }) {
  const gutters = ["breakpoints", "hit-markers", "CodeMirror-linenumbers"];

  if (features.codeFolding) {
    gutters.push("CodeMirror-foldgutter");
  }

  return new SourceEditor({
    mode: SourceEditor.modes.js,
    foldGutter: features.codeFolding,
    enableCodeFolding: features.codeFolding,
    readOnly: true,
    lineNumbers: true,
    theme: "mozilla",
    styleActiveLine: false,
    lineWrapping: prefs.editorWrapping,
    matchBrackets: true,
    showAnnotationRuler: true,
    gutters,
    value: " ",
    extraKeys: {
      // Override code mirror keymap to avoid conflicts with split console and tabbing to other elements.
      Esc: false,
      Tab: false,
      "Shift-Tab": false,
      "Cmd-F": false,
      "Ctrl-F": false,
      "Cmd-G": false,
      "Ctrl-G": false,
    },
    cursorBlinkRate: prefs.cursorBlinkRate,
    ...config,
  });
}

/**
 * Create an headless editor (can be used for syntax highlighting for example)
 *
 * @param {Boolean} useCm6: Should the headless editor use CodeMirror 6
 * @returns {CodeMirror}
 */
export function createHeadlessEditor(useCm6) {
  const editor = createEditor({ cm6: useCm6 });
  editor.appendToLocalElement(document.createElement("div"));
  return editor;
}
