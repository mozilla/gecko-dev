/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { isConsole } from "../utils/preview";
import { getGrip, getFront } from "../utils/evaluation-result";

import {
  isLineInScope,
  isSelectedFrameVisible,
  getSelectedSource,
  getSelectedLocation,
  getSelectedFrame,
  getCurrentThread,
  getSelectedException,
  getSelectedTraceIndex,
  getAllTraces,
} from "../selectors/index";
import { features } from "../utils/prefs";

import { getMappedExpression } from "./expressions";
const {
  TRACER_FIELDS_INDEXES,
} = require("resource://devtools/server/actors/tracer.js");

async function findExpressionMatches(state, parserWorker, editor, tokenPos) {
  const location = getSelectedLocation(state);
  if (!location) {
    return [];
  }
  if (features.codemirrorNext) {
    return editor.findBestMatchExpressions(tokenPos);
  }

  let match = await parserWorker.findBestMatchExpression(
    location.source.id,
    tokenPos
  );
  if (!match) {
    // Fallback on expression from codemirror cursor,
    // if parser worker misses symbols or is unable to find a match.
    match = editor.getExpressionFromCoords(tokenPos);
    return match ? [match] : [];
  }
  return [match];
}

/**
 * Get a preview object for the currently selected frame in the JS Tracer.
 *
 * @param {Object} target
 *        The hovered DOM Element within CodeMirror rendering.
 * @param {Object} tokenPos
 *        The CodeMirror position object for the hovered token.
 * @param {Object} editor
 *        The CodeMirror editor object.
 */
export function getTracerPreview(target, tokenPos, editor) {
  return async thunkArgs => {
    const { getState, parserWorker } = thunkArgs;
    const selectedTraceIndex = getSelectedTraceIndex(getState());
    if (selectedTraceIndex == null) {
      return null;
    }

    const trace = getAllTraces(getState())[selectedTraceIndex];

    // We may be selecting a mutation trace, which doesn't expose any value,
    // so only consider method calls.
    if (trace[TRACER_FIELDS_INDEXES.TYPE] != "enter") {
      return null;
    }

    const matches = await findExpressionMatches(
      getState(),
      parserWorker,
      editor,
      tokenPos
    );
    if (!matches.length) {
      return null;
    }

    let { expression, location } = matches[0];
    const source = getSelectedSource(getState());
    if (location && source.isOriginal) {
      const thread = getCurrentThread(getState());
      const mapResult = await getMappedExpression(
        expression,
        thread,
        thunkArgs
      );
      if (mapResult) {
        expression = mapResult.expression;
      }
    }

    const argumentValues = trace[TRACER_FIELDS_INDEXES.ENTER_ARGS];
    const argumentNames = trace[TRACER_FIELDS_INDEXES.ENTER_ARG_NAMES];
    if (!argumentNames || !argumentValues) {
      return null;
    }

    const argumentIndex = argumentNames.indexOf(expression);
    if (argumentIndex == -1) {
      return null;
    }

    const result = argumentValues[argumentIndex];
    // Values are either primitives, or an Object Front
    const resultGrip = result?.getGrip ? result?.getGrip() : result;

    const root = {
      path: expression,
      contents: {
        value: resultGrip,
        front: getFront(result),
      },
    };
    return {
      previewType: "tracer",
      target,
      tokenPos,
      cursorPos: target.getBoundingClientRect(),
      expression,
      root,
      resultGrip,
    };
  };
}

/**
 * Get a preview object for the currently paused frame, if paused.
 *
 * @param {Object} target
 *        The hovered DOM Element within CodeMirror rendering.
 * @param {Object} tokenPos
 *        The CodeMirror position object for the hovered token.
 * @param {Object} editor
 *        The CodeMirror editor object.
 */
export function getPausedPreview(target, tokenPos, editor) {
  return async thunkArgs => {
    const { getState, client, parserWorker } = thunkArgs;
    if (
      !isSelectedFrameVisible(getState()) ||
      !isLineInScope(getState(), tokenPos.line)
    ) {
      return null;
    }

    const source = getSelectedSource(getState());
    if (!source) {
      return null;
    }
    const thread = getCurrentThread(getState());
    const selectedFrame = getSelectedFrame(getState());
    if (!selectedFrame) {
      return null;
    }
    const matches = await findExpressionMatches(
      getState(),
      parserWorker,
      editor,
      tokenPos
    );
    if (!matches.length) {
      return null;
    }

    let { expression, location } = matches[0];

    if (isConsole(expression)) {
      return null;
    }

    if (location && source.isOriginal) {
      const mapResult = await getMappedExpression(
        expression,
        thread,
        thunkArgs
      );
      if (mapResult) {
        expression = mapResult.expression;
      }
    }

    const { result, hasException, exception } = await client.evaluate(
      expression,
      {
        frameId: selectedFrame.id,
      }
    );

    // The evaluation shouldn't return an exception.
    if (hasException) {
      const errorClass = exception?.getGrip()?.class || "Error";
      throw new Error(
        `Debugger internal exception: Preview for <${expression}> threw a ${errorClass}`
      );
    }

    const resultGrip = getGrip(result);

    // Error case occurs for a token that follows an errored evaluation
    // https://github.com/firefox-devtools/debugger/pull/8056
    // Accommodating for null allows us to show preview for falsy values
    // line "", false, null, Nan, and more
    if (resultGrip === null) {
      return null;
    }

    // Handle cases where the result is invisible to the debugger
    // and not possible to preview. Bug 1548256
    if (
      resultGrip &&
      resultGrip.class &&
      typeof resultGrip.class === "string" &&
      resultGrip.class.includes("InvisibleToDebugger")
    ) {
      return null;
    }

    const root = {
      path: expression,
      contents: {
        value: resultGrip,
        front: getFront(result),
      },
    };

    return {
      previewType: "pause",
      target,
      tokenPos,
      cursorPos: target.getBoundingClientRect(),
      expression,
      root,
      resultGrip,
    };
  };
}

export function getExceptionPreview(target, tokenPos, editor) {
  return async ({ getState, parserWorker }) => {
    const matches = await findExpressionMatches(
      getState(),
      parserWorker,
      editor,
      tokenPos
    );
    if (!matches.length) {
      return null;
    }
    let exception;
    // Lezer might return multiple matches in certain scenarios.
    // Example: For this expression `[].inlineException()` is likely to throw an exception,
    // but if the user hovers over `inlineException` lezer finds 2 matches :
    // 1) `inlineException` for `PropertyName`,
    // 2) `[].inlineException()` for `MemberExpression`
    // Babel seems to only include the `inlineException`.
    for (const match of matches) {
      const tokenColumnStart = match.location.start.column + 1;
      exception = getSelectedException(
        getState(),
        tokenPos.line,
        tokenColumnStart
      );
      if (exception) {
        break;
      }
    }

    if (!exception) {
      return null;
    }

    return {
      target,
      tokenPos,
      cursorPos: target.getBoundingClientRect(),
      exception,
    };
  };
}
