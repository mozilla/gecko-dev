/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env node */

const { logTest, logTask } = require("./utils/profiling");

module.exports = logTest(
  "JetStream 3 test",
  async function (context, commands) {
    context.log.info("Starting JetStream 3 test");
    let url = context.options.browsertime.url;
    let page_cycles = context.options.browsertime.page_cycles;
    let suite_name = context.options.browsertime.suite_name;
    let page_cycle_delay = context.options.browsertime.page_cycle_delay;
    let post_startup_delay = context.options.browsertime.post_startup_delay;
    let page_timeout = context.options.timeouts.pageLoad;
    let expose_profiler = context.options.browsertime.expose_profiler;

    context.log.info(
      "Waiting for %d ms (post_startup_delay)",
      post_startup_delay
    );
    await commands.wait.byTime(post_startup_delay);

    for (let count = 0; count < page_cycles; count++) {
      await logTask(context, "cycle " + count, async function () {
        context.log.info("Navigating to about:blank");
        await commands.navigate("about:blank");

        context.log.info(
          "Cycle %d, waiting for %d ms",
          count,
          page_cycle_delay
        );
        await commands.wait.byTime(page_cycle_delay);

        context.log.info("Cycle %d, starting the measure", count);
        if (expose_profiler === "true") {
          context.log.info("Custom profiler start!");
          if (context.options.browser === "firefox") {
            await commands.profiler.start();
          } else if (context.options.browser === "chrome") {
            await commands.trace.start();
          }
        }
        await commands.measure.start(url);

        // Wait up to 30s for the UI to fully intialize. In particular, for the
        // status button to be ready.
        await commands.js.runAndWait(`
          return new Promise(resolve => {
            let tries = 0;
            // 300 * 100ms = 30 seconds
            const maxTries = 300;
            const waitForJetStreamUIReady = () => {
              const status = document.getElementById("status");
              if (status && typeof status.onclick === "function") {
                JetStream.start();
                resolve("Started JetStream after UI ready");
              } else if (++tries > maxTries) {
                resolve("Timed out waiting for JetStream UI readiness");
              } else {
                setTimeout(waitForJetStreamUIReady, 100);
              }
            };
            waitForJetStreamUIReady();
          });
        `);

        let data_exists = null;
        let starttime = await commands.js.run(`return performance.now();`);
        while (
          (data_exists == null || !Object.keys(data_exists).length) &&
          (await commands.js.run(`return performance.now();`)) - starttime <
            page_timeout
        ) {
          let wait_time = 3000;
          context.log.info(
            "Waiting %d ms for data from %s...",
            wait_time,
            suite_name
          );
          await commands.wait.byTime(wait_time);

          data_exists = await commands.js.run(`
            return new Promise(resolve => {
                globalThis.addEventListener("JetStreamDone", (event) => {
                    resolve(event.detail);
                }, { once: true });
            });
        `);
        }

        if (expose_profiler === "true") {
          context.log.info("Custom profiler stop!");
          if (context.options.browser === "firefox") {
            await commands.profiler.stop();
          } else if (context.options.browser === "chrome") {
            await commands.trace.stop();
          }
        }
        if (
          !data_exists &&
          (await commands.js.run(`return performance.now();`)) - starttime >=
            page_timeout
        ) {
          context.log.error("Benchmark timed out. Aborting...");
          return false;
        }

        let data = null;

        const score = data_exists[suite_name].metrics.Score;
        const tests = data_exists[suite_name].tests;

        data = {
          score,
          tests,
        };
        data.suite_name = suite_name;

        commands.measure.addObject({ js3_res: data });
        context.log.info("Value of summarized benchmark data: ", data);
        return true;
      });
    }

    return true;
  }
);
