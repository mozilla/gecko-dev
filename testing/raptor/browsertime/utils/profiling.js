/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env node */

function logCommands(commands, logger, command, printFirstArg) {
  let object = commands;
  let path = command.split(".");
  while (path.length > 1) {
    object = object[path.shift()];
  }
  let methodName = path[0];
  let originalFun = object[methodName];
  object[methodName] = async function () {
    let logString = ": " + command;
    if (printFirstArg && arguments.length) {
      logString += ": " + arguments[0];
    }
    logger.info("BEGIN" + logString);
    let rv = await originalFun.apply(object, arguments);
    logger.info("END" + logString);
    return rv;
  };
}

async function logTask(context, logString, task) {
  context.log.info("BEGIN: " + logString);
  let rv = await task();
  context.log.info("END: " + logString);

  return rv;
}

function logTest(name, test) {
  return async function wrappedTest(context, commands) {
    for (let [commandName, printFirstArg] of [
      ["addText.bySelector", true],
      ["android.shell", true],
      ["click.byXpath", true],
      ["click.byXpathAndWait", true],
      ["js.run", false],
      ["js.runAndWait", false],
      ["js.runPrivileged", false],
      ["measure.add", true],
      ["measure.addObject", false],
      ["measure.start", true],
      ["measure.stop", false],
      ["mouse.doubleClick.bySelector", true],
      ["mouse.doubleClick.byXpath", true],
      ["mouse.singleClick.bySelector", true],
      ["navigate", true],
      ["profiler.start", false],
      ["profiler.stop", false],
      ["trace.start", false],
      ["trace.stop", false],
      ["wait.byTime", true],
    ]) {
      logCommands(commands, context.log, commandName, printFirstArg);
    }

    let iterationName = "iteration";
    if (
      context.options.firefox.geckoProfiler ||
      context.options.browsertime.expose_profiler === "true"
    ) {
      iterationName = "profiling iteration";
    }
    let logString = `: ${iterationName} ${context.index}: ${name}`;
    context.log.info("BEGIN" + logString);
    let rv = await test(context, commands);
    context.log.info("END" + logString);

    return rv;
  };
}

module.exports = {
  logTest,
  logTask,
};
