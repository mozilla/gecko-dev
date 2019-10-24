/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests whether complex request params and payload sent via POST are
 * displayed correctly.
 */

add_task(async function() {
  const { tab, monitor } = await initNetMonitor(PARAMS_URL);
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  const { L10N } = windowRequire("devtools/client/netmonitor/src/utils/l10n");

  store.dispatch(Actions.batchEnable(false));

  // Execute requests.
  await performRequests(monitor, tab, 12);

  let wait = waitForDOM(document, "#params-panel .tree-section", 3);
  EventUtils.sendMouseEvent(
    { type: "mousedown" },
    document.querySelectorAll(".request-list-item")[0]
  );
  EventUtils.sendMouseEvent(
    { type: "click" },
    document.querySelector("#params-tab")
  );
  await wait;
  testParamsTab1("a", "", '{ "foo": "bar" }', "");

  wait = waitForDOM(document, "#params-panel .tree-section", 3);
  EventUtils.sendMouseEvent(
    { type: "mousedown" },
    document.querySelectorAll(".request-list-item")[1]
  );
  await wait;
  testParamsTab1("a", "b", '{ "foo": "bar" }', "");

  wait = waitForDOM(document, "#params-panel .tree-section", 3);
  EventUtils.sendMouseEvent(
    { type: "mousedown" },
    document.querySelectorAll(".request-list-item")[2]
  );
  await wait;
  testParamsTab1("a", "b", "?foo", "bar");

  let waitSections, waitSourceEditor;
  waitSections = waitForDOM(
    document,
    "#params-panel tr:not(.tree-section).treeRow",
    2
  );
  waitSourceEditor = waitForDOM(document, "#params-panel .CodeMirror-code");
  EventUtils.sendMouseEvent(
    { type: "mousedown" },
    document.querySelectorAll(".request-list-item")[3]
  );
  await Promise.all([waitSections, waitSourceEditor]);
  testParamsTab2("a", "", '{ "foo": "bar" }', "js");

  waitSections = waitForDOM(
    document,
    "#params-panel tr:not(.tree-section).treeRow",
    2
  );
  waitSourceEditor = waitForDOM(document, "#params-panel .CodeMirror-code");
  EventUtils.sendMouseEvent(
    { type: "mousedown" },
    document.querySelectorAll(".request-list-item")[4]
  );
  await Promise.all([waitSections, waitSourceEditor]);
  testParamsTab2("a", "b", '{ "foo": "bar" }', "js");

  // Wait for all tree sections and editor updated by react
  waitSections = waitForDOM(document, "#params-panel .tree-section", 2);
  waitSourceEditor = waitForDOM(document, "#params-panel .CodeMirror-code");
  EventUtils.sendMouseEvent(
    { type: "mousedown" },
    document.querySelectorAll(".request-list-item")[5]
  );
  await Promise.all([waitSections, waitSourceEditor]);
  testParamsTab2("a", "b", "?foo=bar", "text");

  EventUtils.sendMouseEvent(
    { type: "mousedown" },
    document.querySelectorAll(".request-list-item")[6]
  );
  testParamsTab3();

  wait = waitForDOM(document, "#params-panel .tree-section", 3);
  EventUtils.sendMouseEvent(
    { type: "mousedown" },
    document.querySelectorAll(".request-list-item")[7]
  );
  await wait;
  testParamsTab1("a", "b", '{ "foo": "bar" }', "");

  wait = waitForDOM(document, "#params-panel .tree-section", 3);
  EventUtils.sendMouseEvent(
    { type: "mousedown" },
    document.querySelectorAll(".request-list-item")[8]
  );
  await wait;
  testParamsTab1("a", "b", '{ "foo": "bar" }', "");

  wait = waitForDOM(document, "#params-panel .tree-section", 1);
  EventUtils.sendMouseEvent(
    { type: "mousedown" },
    document.querySelectorAll(".request-list-item")[9]
  );
  await wait;
  testParamsTabGetWithArgs(new Map([["species", "in=(52,60)"]]));

  wait = waitForDOM(document, "#params-panel .tree-section", 1);
  EventUtils.sendMouseEvent(
    { type: "mousedown" },
    document.querySelectorAll(".request-list-item")[10]
  );
  await wait;
  testParamsTabGetWithArgs(new Map([["a", ["", "b"]]]));

  wait = waitForDOM(document, "#params-panel .tree-section", 1);
  EventUtils.sendMouseEvent(
    { type: "mousedown" },
    document.querySelectorAll(".request-list-item")[11]
  );
  await wait;
  testParamsTabGetWithArgs(new Map([["a", ["b", "c"]], ["d", "1"]]));

  await teardown(monitor);

  function testParamsTab1(
    queryStringParamName,
    queryStringParamValue,
    formDataParamName,
    formDataParamValue
  ) {
    const tabpanel = document.querySelector("#params-panel");

    is(
      tabpanel.querySelectorAll(".tree-section").length,
      3,
      "The number of param tree sections displayed in this tabpanel is incorrect."
    );
    is(
      tabpanel.querySelectorAll("tr:not(.tree-section).treeRow").length,
      2,
      "The number of param rows displayed in this tabpanel is incorrect."
    );
    is(
      tabpanel.querySelectorAll(".empty-notice").length,
      0,
      "The empty notice should not be displayed in this tabpanel."
    );

    ok(
      tabpanel.querySelector(".treeTable"),
      "The request params box should be displayed."
    );
    ok(
      tabpanel.querySelector(".CodeMirror-code") === null,
      "The request post data editor should not be displayed."
    );

    const treeSections = tabpanel.querySelectorAll(".tree-section");
    const labels = tabpanel.querySelectorAll(
      "tr:not(.tree-section) .treeLabelCell .treeLabel"
    );
    const values = tabpanel.querySelectorAll(
      "tr:not(.tree-section) .treeValueCell .objectBox"
    );

    is(
      treeSections[0].querySelector(".treeLabel").textContent,
      L10N.getStr("paramsQueryString"),
      "The params section doesn't have the correct title."
    );
    is(
      treeSections[1].querySelector(".treeLabel").textContent,
      L10N.getStr("paramsFormData"),
      "The form data section doesn't have the correct title."
    );

    is(
      labels[0].textContent,
      queryStringParamName,
      "The first query string param name was incorrect."
    );
    is(
      values[0].textContent,
      queryStringParamValue,
      "The first query string param value was incorrect."
    );

    is(
      labels[1].textContent,
      formDataParamName,
      "The first form data param name was incorrect."
    );
    is(
      values[1].textContent,
      formDataParamValue,
      "The first form data param value was incorrect."
    );
  }

  function testParamsTab2(
    queryStringParamName,
    queryStringParamValue,
    requestPayload,
    editorMode
  ) {
    const isJSON = editorMode === "js";
    const tabpanel = document.querySelector("#params-panel");

    is(
      tabpanel.querySelectorAll(".tree-section").length,
      isJSON ? 3 : 2,
      "The number of param tree sections displayed in this tabpanel is incorrect."
    );
    is(
      tabpanel.querySelectorAll("tr:not(.tree-section).treeRow").length,
      isJSON ? 2 : 1,
      "The number of param rows displayed in this tabpanel is incorrect."
    );
    is(
      tabpanel.querySelectorAll(".empty-notice").length,
      0,
      "The empty notice should not be displayed in this tabpanel."
    );

    ok(
      tabpanel.querySelector(".treeTable"),
      "The request params box should be displayed."
    );
    ok(
      tabpanel.querySelector(".CodeMirror-code"),
      "The request post data editor should be displayed."
    );

    const treeSections = tabpanel.querySelectorAll(".tree-section");

    is(
      treeSections[0].querySelector(".treeLabel").textContent,
      L10N.getStr("paramsQueryString"),
      "The query section doesn't have the correct title."
    );
    is(
      treeSections[1].querySelector(".treeLabel").textContent,
      isJSON ? L10N.getStr("jsonScopeName") : L10N.getStr("paramsPostPayload"),
      "The post section doesn't have the correct title."
    );

    const labels = tabpanel.querySelectorAll(
      "tr:not(.tree-section) .treeLabelCell .treeLabel"
    );
    const values = tabpanel.querySelectorAll(
      "tr:not(.treeS-section) .treeValueCell .objectBox"
    );

    is(
      labels[0].textContent,
      queryStringParamName,
      "The first query string param name was incorrect."
    );
    is(
      values[0].textContent,
      queryStringParamValue,
      "The first query string param value was incorrect."
    );

    ok(
      getCodeMirrorValue(monitor).includes(requestPayload),
      "The text shown in the source editor is incorrect."
    );

    if (isJSON) {
      is(
        treeSections[2].querySelector(".treeLabel").textContent,
        L10N.getStr("paramsPostPayload"),
        "The post section doesn't have the correct title."
      );

      const requestPayloadObject = JSON.parse(requestPayload);
      const requestPairs = Object.keys(requestPayloadObject).map(k => [
        k,
        requestPayloadObject[k],
      ]);
      for (let i = 1; i < requestPairs.length; i++) {
        const [requestPayloadName, requestPayloadValue] = requestPairs[i];
        is(
          requestPayloadName,
          labels[i].textContent,
          "JSON property name " + i + " should be displayed correctly"
        );
        is(
          '"' + requestPayloadValue + '"',
          values[i].textContent,
          "JSON property value " + i + " should be displayed correctly"
        );
      }
    }
  }

  function testParamsTab3() {
    const tabpanel = document.querySelector("#params-panel");

    is(
      tabpanel.querySelectorAll(".tree-section").length,
      0,
      "The number of param tree sections displayed in this tabpanel is incorrect."
    );
    is(
      tabpanel.querySelectorAll("tr:not(.tree-section).treeRow").length,
      0,
      "The number of param rows displayed in this tabpanel is incorrect."
    );
    is(
      tabpanel.querySelectorAll(".empty-notice").length,
      1,
      "The empty notice should be displayed in this tabpanel."
    );

    ok(
      !tabpanel.querySelector(".treeTable"),
      "The request params box should be hidden."
    );
    ok(
      !tabpanel.querySelector(".CodeMirror-code"),
      "The request post data editor should be hidden."
    );
  }

  /**
   * @param {Map} expectedParams A map of expected parameter keys, and values
   * as Strings or an array of Strings if the parameter key has multiple
   * values
   */
  function testParamsTabGetWithArgs(expectedParams) {
    const tabpanel = document.querySelector("#params-panel");

    let numParamRows = 0;
    expectedParams.forEach((v, k, m) => {
      numParamRows += typeof v === "object" ? v.length + 1 : 1;
    });

    is(
      tabpanel.querySelectorAll(".tree-section").length,
      1,
      "Check the number of param tree sections displayed in this tabpanel."
    );
    is(
      tabpanel.querySelectorAll("tr:not(.tree-section).treeRow").length,
      numParamRows,
      "Check the number of param rows displayed in this tabpanel."
    );
    ok(
      !tabpanel.querySelector(".empty-notice"),
      "The empty notice should not be displayed in this tabpanel."
    );

    ok(
      tabpanel.querySelector(".treeTable"),
      "The request params box should be shown."
    );
    ok(
      !tabpanel.querySelector(".CodeMirror-code"),
      "The request post data editor should be hidden."
    );

    const treeSections = tabpanel.querySelectorAll(".tree-section");
    const labels = tabpanel.querySelectorAll(
      "tr:not(.tree-section) .treeLabelCell .treeLabel"
    );
    const values = tabpanel.querySelectorAll(
      "tr:not(.tree-section) .treeValueCell .objectBox"
    );

    is(
      treeSections[0].querySelector(".treeLabel").textContent,
      L10N.getStr("paramsQueryString"),
      "Check the displayed params section title."
    );

    const labelsIter = labels.values();
    const valuesIter = values.values();
    for (const [expKey, expValue] of expectedParams) {
      let label = labelsIter.next().value;
      let value = valuesIter.next().value;

      if (typeof expValue === "object") {
        // multiple values for one parameter
        is(label.textContent, expKey, "Check that parameter name matches.");
        is(
          value.textContent,
          "[\u2026]", // horizontal ellipsis
          "Check that parameter value indicates multiple."
        );

        for (let i = 0; i < expValue.length; i++) {
          label = labelsIter.next().value;
          value = valuesIter.next().value;
          is(
            label.textContent,
            i + "",
            "Check that multi-value parameter index matches."
          );
          is(
            value.textContent,
            expValue[i],
            "Check that multi-value parameter value matches."
          );
          is(label.dataset.level, 2, "Check that parameter is nested.");
        }
      } else {
        is(label.textContent, expKey, "Check that parameter name matches.");
        is(value.textContent, expValue, "Check that parameter value matches.");
      }
    }
  }
});
