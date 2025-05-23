/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

export * from "./source-search";
export * from "../ui";
export * from "./tokens";

import { createEditor } from "./create-editor";

let editor;

export function getEditor() {
  if (editor) {
    return editor;
  }

  editor = createEditor({ cm6: true });
  return editor;
}

export function removeEditor() {
  editor = null;
}

/**
 *  Update line wrapping for the codemirror editor.
 */
export function updateEditorLineWrapping(value) {
  if (!editor) {
    return;
  }
  editor.setLineWrapping(value);
}

export function toWasmSourceLine(offset) {
  return editor.wasmOffsetToLine(offset) || 0;
}

/**
 * Convert source lines / WASM line offsets to Codemirror lines
 * @param {Object} source
 * @param {Number} lineOrOffset
 * @returns
 */
export function toEditorLine(source, lineOrOffset) {
  if (editor.isWasm && !source.isOriginal) {
    // TODO ensure offset is always "mappable" to edit line.
    return toWasmSourceLine(lineOrOffset) + 1;
  }
  return lineOrOffset;
}

export function fromEditorLine(source, line) {
  // Also ignore the original source related to the .wasm file.
  if (editor.isWasm && !source.isOriginal) {
    // Content lines is 1-based in CM6 and 0-based in WASM
    return editor.lineToWasmOffset(line - 1);
  }
  return line;
}

export function toEditorPosition(location) {
  // Note that Spidermonkey, Debugger frontend and CodeMirror are all consistent regarding column
  // and are 0-based. But only CodeMirror consider the line to be 0-based while the two others
  // consider lines to be 1-based.
  const isSourceWasm = editor.isWasm && !location.source.isOriginal;
  return {
    line: toEditorLine(location.source, location.line),
    column: isSourceWasm || !location.column ? 0 : location.column,
  };
}

export function toSourceLine(source, line) {
  if (editor.isWasm && !source.isOriginal) {
    return editor.lineToWasmOffset(line - 1);
  }
  return line;
}
