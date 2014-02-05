/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Makes sure Pie Charts have the right internal structure when
 * initialized with empty data.
 */

function test() {
  initNetMonitor(SIMPLE_URL).then(([aTab, aDebuggee, aMonitor]) => {
    info("Starting test... ");

    let { document, L10N, Chart } = aMonitor.panelWin;
    let container = document.createElement("box");

    let pie = Chart.Pie(document, {
      data: null,
      width: 100,
      height: 100
    });

    let node = pie.node;
    let slices = node.querySelectorAll(".pie-chart-slice.chart-colored-blob");
    let labels = node.querySelectorAll(".pie-chart-label");

    ok(node.classList.contains("pie-chart-container") &&
       node.classList.contains("generic-chart-container"),
      "A pie chart container was created successfully.");

    is(slices.length, 1,
      "There should be 1 pie chart slice created.");
    ok(slices[0].getAttribute("d").match(/\s*M 50,50 L 50\.\d+,2\.5\d* A 47\.5,47\.5 0 1 1 49\.\d+,2\.5\d* Z/),
      "The first slice has the correct data.");

    ok(slices[0].hasAttribute("largest"),
      "The first slice should be the largest one.");
    ok(slices[0].hasAttribute("smallest"),
      "The first slice should also be the smallest one.");
    ok(slices[0].getAttribute("name"), L10N.getStr("pieChart.empty"),
      "The first slice's name is correct.");

    is(labels.length, 1,
      "There should be 1 pie chart label created.");
    is(labels[0].textContent, "Loading",
      "The first label's text is correct.");

    teardown(aMonitor).then(finish);
  });
}
