// Check to make sure that a worker can be attached to a toolbox
// directly, and that the toolbox has expected properties.

"use strict";

// Import helpers for the workers
/* import-globals-from helper_workers.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/shared/test/helper_workers.js",
  this);

const TAB_URL = EXAMPLE_URL + "doc_WorkerTargetActor.attachThread-tab.html";
const WORKER_URL = "code_WorkerTargetActor.attachThread-worker.js";

add_task(async function() {
  await pushPrefs(["devtools.scratchpad.enabled", true]);

  const tab = await addTab(TAB_URL);
  const target = await TargetFactory.forTab(tab);
  await target.attach();
  const targetFront = target.activeTab;

  await listWorkers(targetFront);
  await createWorkerInTab(tab, WORKER_URL);

  const { workers } = await listWorkers(targetFront);
  const workerTargetFront = findWorker(workers, WORKER_URL);

  const toolbox = await gDevTools.showToolbox(TargetFactory.forWorker(workerTargetFront),
                                            "jsdebugger",
                                            Toolbox.HostType.WINDOW);

  is(toolbox.hostType, "window", "correct host");

  await new Promise(done => {
    toolbox.win.parent.addEventListener("message", function onmessage(event) {
      if (event.data.name == "set-host-title") {
        toolbox.win.parent.removeEventListener("message", onmessage);
        done();
      }
    });
  });
  ok(toolbox.win.parent.document.title.includes(WORKER_URL),
     "worker URL in host title");

  const toolTabs = toolbox.doc.querySelectorAll(".devtools-tab");
  const activeTools = [...toolTabs].map(toolTab => toolTab.getAttribute("data-id"));

  is(activeTools.join(","), "webconsole,jsdebugger,scratchpad",
    "Correct set of tools supported by worker");

  terminateWorkerInTab(tab, WORKER_URL);
  await waitForWorkerClose(workerTargetFront);
  await target.destroy();

  await toolbox.destroy();
});
