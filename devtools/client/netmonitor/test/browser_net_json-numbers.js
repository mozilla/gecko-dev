/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if JSON responses with numbers that can't be accurately represented in JS are correctly rendered.
 */
add_task(async function () {
  const { tab, monitor } = await initNetMonitor(
    JSON_BASIC_URL + "?name=numbers",
    { requestCount: 1 }
  );
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");

  store.dispatch(Actions.batchEnable(false));

  await performRequests(monitor, tab, 1);

  const onResponsePanelReady = waitForDOM(
    document,
    "#response-panel .data-header"
  );

  const onPropsViewReady = waitForDOM(
    document,
    "#response-panel .properties-view",
    1
  );

  store.dispatch(Actions.toggleNetworkDetails());
  clickOnSidebarTab(document, "response");
  await Promise.all([onResponsePanelReady, onPropsViewReady]);

  const tabpanel = document.querySelector("#response-panel");
  const labels = tabpanel.querySelectorAll("tr .treeLabelCell .treeLabel");
  const values = tabpanel.querySelectorAll("tr .treeValueCell .objectBox");

  info("Checking that regular numbers aren't rendered with JsonNumber");
  is(
    labels[0].textContent,
    "small",
    "The first json property name is correct."
  );
  is(values[0].textContent, "12", "The first json property value is correct.");
  ok(
    !values[0].classList.contains("objectBox-json-number") &&
      !values[0].querySelector(".source-value"),
    "Regular number does not get the json number class"
  );

  info("Checking that negative numbers aren't rendered with JsonNumber");
  is(
    labels[1].textContent,
    "negzero",
    "The second json property name is correct."
  );
  ok(
    !values[1].classList.contains("objectBox-json-number") &&
      !values[1].querySelector(".source-value"),
    "-0 does not get the json number class"
  );

  info("Check numbers bigger than Number.MAX_SAFE_INTEGER");
  is(labels[2].textContent, "big", "The third json property name is correct.");
  ok(
    values[2].classList.contains("objectBox-json-number"),
    "Big number get the json number class"
  );
  is(
    values[2].querySelector(".source-value").textContent,
    "1516340399466235648",
    "Big number has expected source text"
  );
  is(
    values[2].querySelector(".parsed-value").textContent,
    "JS:1516340399466235600",
    "Big number has expected parsed value text"
  );
  ok(
    values[2].querySelector(".parsed-value").getAttribute("title"),
    "Big number parsed value label has a title attribute"
  );

  info("Check numbers with higher precision than what's possible in JS");
  is(labels[3].textContent, "precise", "Got expected fourth item");
  ok(
    values[3].classList.contains("objectBox-json-number"),
    "High precision number get the json number class"
  );
  is(
    values[3].querySelector(".source-value").textContent,
    "3.141592653589793238462643383279",
    "High precision number has expected source text"
  );
  is(
    values[3].querySelector(".parsed-value").textContent,
    "JS:3.141592653589793",
    "High precision number has expected parsed value text"
  );
  ok(
    values[3].querySelector(".parsed-value").getAttribute("title"),
    "High precision number parsed value label has a title attribute"
  );

  info("Checking that exponential numbers source is displayed");
  is(labels[4].textContent, "exp", "Got expected fourth item");
  ok(
    values[4].classList.contains("objectBox-json-number"),
    "Exponential number get the json number class"
  );
  is(
    values[4].querySelector(".source-value").textContent,
    "1e2",
    "Exponential number has expected source text"
  );
  is(
    values[4].querySelector(".parsed-value").textContent,
    "JS:100",
    "Exponential number has expected parsed value text"
  );
  ok(
    values[4].querySelector(".parsed-value").getAttribute("title"),
    "Exponential number parsed value label has a title attribute"
  );

  await teardown(monitor);
});
