/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const DevToolsUtils = require("resource://devtools/shared/DevToolsUtils.js");
loader.lazyRequireGetter(
  this,
  "ObjectUtils",
  "resource://devtools/server/actors/object/utils.js"
);
const {
  formatDisplayName,
} = require("resource://devtools/server/actors/frame.js");
const {
  TYPES,
  getResourceWatcher,
} = require("resource://devtools/server/actors/resources/index.js");
loader.lazyRequireGetter(
  this,
  ["isValidSavedFrame"],
  "devtools/server/actors/frame",
  true
);

// Get a string message to display when a frame evaluation throws.
function getThrownMessage(completion) {
  try {
    if (completion.throw.getOwnPropertyDescriptor) {
      return completion.throw.getOwnPropertyDescriptor("message").value;
    } else if (completion.toString) {
      return completion.toString();
    }
  } catch (ex) {
    // ignore
  }
  return "Unknown exception";
}
module.exports.getThrownMessage = getThrownMessage;

function evalAndLogEvent({
  threadActor,
  frame,
  level,
  expression,
  bindings,
  showStacktrace,
}) {
  const frameLocation = threadActor.sourcesManager.getFrameLocation(frame);
  const { sourceActor, line } = frameLocation;
  const displayName = formatDisplayName(frame);
  const stacktrace = showStacktrace ? [] : undefined;

  if (showStacktrace) {
    let currentFrame = frame;
    while (currentFrame) {
      if (currentFrame.script) {
        stacktrace.push({
          filename: currentFrame.script.url,
          functionName: currentFrame.script.displayName,
          lineNumber: currentFrame.script.startLine,
          columnNumber: currentFrame.script.startColumn,
          sourceId: currentFrame.script.source.id,
        });
      } else {
        stacktrace.push({
          filename: "unknown",
          functionName: currentFrame.displayName || "anonymous",
          lineNumber: 0,
          columnNumber: 0,
          sourceId: "",
        });
      }
      const olderSavedFrame =
        currentFrame.olderSavedFrame &&
        isValidSavedFrame(threadActor, currentFrame.olderSavedFrame)
          ? currentFrame.olderSavedFrame
          : null;
      currentFrame = currentFrame.older || olderSavedFrame;
    }
  }

  // TODO remove this branch when (#1749668) lands (#1609540)
  if (isWorker) {
    threadActor.targetActor._consoleActor.evaluateJS({
      text: showStacktrace
        ? `console.trace(...${expression})`
        : `console.log(...${expression})`,
      bindings: { displayName, ...bindings },
      url: sourceActor.url,
      lineNumber: line,
      disableBreaks: true,
    });

    return undefined;
  }

  let completion;
  // Ensure disabling all types of breakpoints for all sources while evaluating the log points
  threadActor.insideClientEvaluation = { disableBreaks: true };
  try {
    completion = frame.evalWithBindings(
      expression,
      {
        displayName,
        ...bindings,
      },
      { hideFromDebugger: true }
    );
  } finally {
    threadActor.insideClientEvaluation = null;
  }

  let value;
  if (!completion) {
    // The evaluation was killed (possibly by the slow script dialog).
    value = ["Evaluation failed"];
  } else if ("return" in completion) {
    value = [];
    const length = ObjectUtils.getArrayLength(completion.return);
    for (let i = 0; i < length; i++) {
      value.push(DevToolsUtils.getProperty(completion.return, i));
    }
  } else {
    value = [getThrownMessage(completion)];
    level = `${level}Error`;
  }

  ChromeUtils.addProfilerMarker("Debugger log point", undefined, value);

  emitConsoleMessage(threadActor, frameLocation, value, level, stacktrace);

  return undefined;
}

function logEvent({ threadActor, frame }) {
  const frameLocation = threadActor.sourcesManager.getFrameLocation(frame);
  const { sourceActor, line } = frameLocation;

  // TODO remove this branch when (#1749668) lands (#1609540)
  if (isWorker) {
    const bindings = {};
    for (let i = 0; i < frame.arguments.length; i++) {
      bindings[`x${i}`] = frame.arguments[i];
    }
    threadActor.targetActor._consoleActor.evaluateJS({
      text: `console.log(${Object.keys(bindings).join(",")})`,
      bindings,
      url: sourceActor.url,
      lineNumber: line,
      disableBreaks: true,
    });

    return undefined;
  }

  emitConsoleMessage(threadActor, frameLocation, frame.arguments, "logPoint");

  return undefined;
}

function emitConsoleMessage(
  threadActor,
  frameLocation,
  args,
  level,
  stacktrace
) {
  const targetActor = threadActor.targetActor;
  const { sourceActor, line, column } = frameLocation;

  const message = {
    filename: sourceActor.url,

    // The line is 1-based
    lineNumber: line,
    // `frameLocation` comes from the SourcesManager which uses 0-base column
    // whereas CONSOLE_MESSAGE resources emits 1-based columns.
    columnNumber: column + 1,
    arguments: args,
    level,
    timeStamp: ChromeUtils.dateNow(),
    chromeContext:
      targetActor.actorID &&
      /conn\d+\.parentProcessTarget\d+/.test(targetActor.actorID),
    // The 'prepareConsoleMessageForRemote' method in webconsoleActor expects internal source ID,
    // thus we can't set sourceId directly to sourceActorID.
    sourceId: sourceActor.internalSourceId,
    stacktrace,
  };

  // Note that only WindowGlobalTarget actor support resource watcher
  // This is still missing for worker and content processes
  const consoleMessageWatcher = getResourceWatcher(
    targetActor,
    TYPES.CONSOLE_MESSAGE
  );
  if (consoleMessageWatcher) {
    consoleMessageWatcher.emitMessages([message], false);
  } else {
    // Bug 1642296: Once we enable ConsoleMessage resource on the server, we should remove onConsoleAPICall
    // from the WebConsoleActor, and only support the ConsoleMessageWatcher codepath.
    message.arguments = message.arguments.map(arg =>
      arg && typeof arg.unsafeDereference === "function"
        ? arg.unsafeDereference()
        : arg
    );
    targetActor._consoleActor.onConsoleAPICall(message);
  }
}

module.exports.evalAndLogEvent = evalAndLogEvent;
module.exports.logEvent = logEvent;
