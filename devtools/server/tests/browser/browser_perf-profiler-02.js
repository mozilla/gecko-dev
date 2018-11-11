/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the built-in profiler module doesn't deactivate when the toolbox
 * is destroyed if there are other consumers using it.
 */

const { PerformanceFront } = require("devtools/shared/fronts/performance");
const { pmmIsProfilerActive, pmmStopProfiler, pmmLoadFrameScripts } = require("devtools/client/performance/test/helpers/profiler-mm-utils");

add_task(function* () {
  yield addTab(MAIN_DOMAIN + "doc_perf.html");
  initDebuggerServer();
  let client = new DebuggerClient(DebuggerServer.connectPipe());
  let form = yield connectDebuggerClient(client);
  let firstFront = PerformanceFront(client, form);
  yield firstFront.connect();

  pmmLoadFrameScripts(gBrowser);

  yield firstFront.startRecording();

  yield addTab(MAIN_DOMAIN + "doc_perf.html");
  let client2 = new DebuggerClient(DebuggerServer.connectPipe());
  let form2 = yield connectDebuggerClient(client2);
  let secondFront = PerformanceFront(client2, form2);
  yield secondFront.connect();
  pmmLoadFrameScripts(gBrowser);

  yield secondFront.startRecording();

  // Manually teardown the tabs so we can check profiler status
  yield secondFront.destroy();
  yield client2.close();
  ok((yield pmmIsProfilerActive()),
    "The built-in profiler module should still be active.");

  yield firstFront.destroy();
  yield client.close();
  ok(!(yield pmmIsProfilerActive()),
    "The built-in profiler module should no longer be active.");

  gBrowser.removeCurrentTab();
  gBrowser.removeCurrentTab();
});
