/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { truncateString } from "devtools/shared/string";
import { WorkerDispatcher } from "devtools/client/shared/worker-utils";

const MAX_SCRIPT_LOG_LENGTH = 500;
const WORKER_URL =
  "resource://devtools/client/debugger/dist/pretty-print-worker.js";

export class PrettyPrintDispatcher extends WorkerDispatcher {
  constructor(jestUrl) {
    super(jestUrl || WORKER_URL);
  }

  #prettyPrintTask = this.task("prettyPrint");
  #prettyPrintInlineScriptTask = this.task("prettyPrintInlineScript");
  #getSourceMapForTask = this.task("getSourceMapForTask");

  async prettyPrint(options) {
    try {
      return await this.#prettyPrintTask(options);
    } catch (e) {
      console.error(
        `[pretty-print] Failed to pretty print script (${options.url}):\n`,
        truncateString(options.sourceText, MAX_SCRIPT_LOG_LENGTH)
      );
      throw e;
    }
  }

  async prettyPrintInlineScript(options) {
    try {
      return await this.#prettyPrintInlineScriptTask(options);
    } catch (e) {
      console.error(
        `[pretty-print] Failed to pretty print inline script (${options.url}):\n`,
        truncateString(options.sourceText, MAX_SCRIPT_LOG_LENGTH)
      );
      throw e;
    }
  }

  getSourceMap(taskId) {
    return this.#getSourceMapForTask(taskId);
  }
}
