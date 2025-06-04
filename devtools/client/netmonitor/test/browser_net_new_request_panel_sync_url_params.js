/* Any copyright is dedicated to the Public Domain.
  http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const asyncStorage = require("resource://devtools/shared/async-storage.js");

/**
 * Test to check the sync between URL parameters and the parameters section
 */

add_task(async function () {
  // Turn on the pref
  await pushPref("devtools.netmonitor.features.newEditAndResend", true);
  // Reset the storage for the persisted custom request
  await asyncStorage.removeItem("devtools.netmonitor.customRequest");

  const { tab, monitor } = await initNetMonitor(HTTPS_CUSTOM_GET_URL, {
    requestCount: 1,
  });
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;

  // Action should be processed synchronously in tests.
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  store.dispatch(Actions.batchEnable(false));

  await performRequests(monitor, tab, 1);

  info("selecting first request");
  const firstRequestItem = document.querySelectorAll(".request-list-item")[0];
  const waitForHeaders = waitUntil(() =>
    document.querySelector(".headers-overview")
  );
  EventUtils.sendMouseEvent({ type: "mousedown" }, firstRequestItem);
  await waitForHeaders;
  EventUtils.sendMouseEvent({ type: "contextmenu" }, firstRequestItem);

  info("Opening the new request panel");
  const waitForPanels = waitForDOM(
    document,
    ".monitor-panel .network-action-bar"
  );
  await selectContextMenuItem(monitor, "request-list-context-edit-resend");
  await waitForPanels;

  const queryScenarios = [
    {
      url: HTTPS_CUSTOM_GET_URL,
      queryString: "",
      expectedParametersSize: 0,
      expectedParameters: [],
      expectedInputValueAfterAddingParamName: `${HTTPS_CUSTOM_GET_URL}?My-param=`,
      expectedInputValueAfterAddingParamValue: `${HTTPS_CUSTOM_GET_URL}?My-param=my-value`,
      expectedInputValueAfterUncheckingParam: HTTPS_CUSTOM_GET_URL,
    },
    {
      url: HTTPS_CUSTOM_GET_URL,
      queryString: "?",
      expectedParametersSize: 0,
      expectedParameters: [],
      expectedInputValueAfterAddingParamName: `${HTTPS_CUSTOM_GET_URL}?My-param=`,
      expectedInputValueAfterAddingParamValue: `${HTTPS_CUSTOM_GET_URL}?My-param=my-value`,
      expectedInputValueAfterUncheckingParam: HTTPS_CUSTOM_GET_URL,
    },
    {
      url: HTTPS_CUSTOM_GET_URL,
      queryString: "?a",
      expectedParametersSize: 1,
      expectedParameters: [{ name: "a", value: "" }],
      expectedInputValueAfterAddingParamName: `${HTTPS_CUSTOM_GET_URL}?a=&My-param=`,
      expectedInputValueAfterAddingParamValue: `${HTTPS_CUSTOM_GET_URL}?a=&My-param=my-value`,
      expectedInputValueAfterUncheckingParam: `${HTTPS_CUSTOM_GET_URL}?a=`,
    },
    {
      url: HTTPS_CUSTOM_GET_URL,
      queryString: "?a=",
      expectedParametersSize: 1,
      expectedParameters: [{ name: "a", value: "" }],
      expectedInputValueAfterAddingParamName: `${HTTPS_CUSTOM_GET_URL}?a=&My-param=`,
      expectedInputValueAfterAddingParamValue: `${HTTPS_CUSTOM_GET_URL}?a=&My-param=my-value`,
      expectedInputValueAfterUncheckingParam: `${HTTPS_CUSTOM_GET_URL}?a=`,
    },
    {
      url: HTTPS_CUSTOM_GET_URL,
      queryString: "?a=3",
      expectedParametersSize: 1,
      expectedParameters: [{ name: "a", value: "3" }],
      expectedInputValueAfterAddingParamName: `${HTTPS_CUSTOM_GET_URL}?a=3&My-param=`,
      expectedInputValueAfterAddingParamValue: `${HTTPS_CUSTOM_GET_URL}?a=3&My-param=my-value`,
      expectedInputValueAfterUncheckingParam: `${HTTPS_CUSTOM_GET_URL}?a=3`,
    },
    {
      url: HTTPS_CUSTOM_GET_URL,
      queryString: "?a=3&",
      expectedParametersSize: 2,
      expectedParameters: [
        { name: "a", value: "3" },
        { name: "", value: "" },
      ],
      expectedInputValueAfterAddingParamName: `${HTTPS_CUSTOM_GET_URL}?a=3&My-param=`,
      expectedInputValueAfterAddingParamValue: `${HTTPS_CUSTOM_GET_URL}?a=3&My-param=my-value`,
      expectedInputValueAfterUncheckingParam: `${HTTPS_CUSTOM_GET_URL}?a=3`,
    },
    {
      url: HTTPS_CUSTOM_GET_URL,
      queryString: "?a=3&b=4",
      expectedParametersSize: 2,
      expectedParameters: [
        { name: "a", value: "3" },
        { name: "b", value: "4" },
      ],
      expectedInputValueAfterAddingParamName: `${HTTPS_CUSTOM_GET_URL}?a=3&b=4&My-param=`,
      expectedInputValueAfterAddingParamValue: `${HTTPS_CUSTOM_GET_URL}?a=3&b=4&My-param=my-value`,
      expectedInputValueAfterUncheckingParam: `${HTTPS_CUSTOM_GET_URL}?a=3&b=4`,
    },
    // Checking with an invalid URL string
    {
      url: "invalid",
      queryString: "",
      expectedParametersSize: 0,
      expectedParameters: [],
      expectedInputValueAfterAddingParamName: `invalid?My-param=`,
      expectedInputValueAfterAddingParamValue: `invalid?My-param=my-value`,
      expectedInputValueAfterUncheckingParam: `invalid`,
    },
    {
      url: "invalid",
      queryString: "?",
      expectedParametersSize: 0,
      expectedParameters: [],
      expectedInputValueAfterAddingParamName: `invalid?My-param=`,
      expectedInputValueAfterAddingParamValue: `invalid?My-param=my-value`,
      expectedInputValueAfterUncheckingParam: "invalid",
    },
    {
      url: "invalid",
      queryString: "?a",
      expectedParametersSize: 1,
      expectedParameters: [{ name: "a", value: "" }],
      expectedInputValueAfterAddingParamName: `invalid?a=&My-param=`,
      expectedInputValueAfterAddingParamValue: `invalid?a=&My-param=my-value`,
      expectedInputValueAfterUncheckingParam: `invalid?a=`,
    },
    {
      url: "invalid",
      queryString: "?a=",
      expectedParametersSize: 1,
      expectedParameters: [{ name: "a", value: "" }],
      expectedInputValueAfterAddingParamName: `invalid?a=&My-param=`,
      expectedInputValueAfterAddingParamValue: `invalid?a=&My-param=my-value`,
      expectedInputValueAfterUncheckingParam: `invalid?a=`,
    },
    {
      url: "invalid",
      queryString: "?a=3",
      expectedParametersSize: 1,
      expectedParameters: [{ name: "a", value: "3" }],
      expectedInputValueAfterAddingParamName: `invalid?a=3&My-param=`,
      expectedInputValueAfterAddingParamValue: `invalid?a=3&My-param=my-value`,
      expectedInputValueAfterUncheckingParam: `invalid?a=3`,
    },
    {
      url: "invalid",
      queryString: "?a=3&",
      expectedParametersSize: 2,
      expectedParameters: [
        { name: "a", value: "3" },
        { name: "", value: "" },
      ],
      expectedInputValueAfterAddingParamName: `invalid?a=3&My-param=`,
      expectedInputValueAfterAddingParamValue: `invalid?a=3&My-param=my-value`,
      expectedInputValueAfterUncheckingParam: `invalid?a=3`,
    },
    {
      url: "invalid",
      queryString: "?a=3&b=4",
      expectedParametersSize: 2,
      expectedParameters: [
        { name: "a", value: "3" },
        { name: "b", value: "4" },
      ],
      expectedInputValueAfterAddingParamName: `invalid?a=3&b=4&My-param=`,
      expectedInputValueAfterAddingParamValue: `invalid?a=3&b=4&My-param=my-value`,
      expectedInputValueAfterUncheckingParam: `invalid?a=3&b=4`,
    },
  ];

  for (const sceanario of queryScenarios) {
    assertQueryScenario(document, sceanario);
  }

  await teardown(monitor);
});

function assertQueryScenario(
  document,
  {
    url,
    queryString,
    expectedParametersSize,
    expectedParameters,
    expectedInputValueAfterAddingParamName,
    expectedInputValueAfterAddingParamValue,
    expectedInputValueAfterUncheckingParam,
  }
) {
  info(`Assert that "${queryString}" shows in the list properly`);
  const urlValue = document.querySelector(".http-custom-url-value");
  urlValue.focus();
  urlValue.value = "";

  EventUtils.sendString(url + queryString);

  is(
    document.querySelectorAll(
      "#http-custom-query .tabpanel-summary-container.http-custom-input"
    ).length,
    expectedParametersSize,
    `The parameter section should have ${expectedParametersSize} elements`
  );

  // Check if the parameter name and value are what we expect
  const parameterNames = document.querySelectorAll(
    "#http-custom-query .http-custom-input-and-map-form .http-custom-input-name"
  );
  const parameterValues = document.querySelectorAll(
    "#http-custom-query .http-custom-input-and-map-form .http-custom-input-value"
  );

  for (let i = 0; i < expectedParameters.length; i++) {
    is(
      parameterNames[i].value,
      expectedParameters[i].name,
      "The query param name in the form should match on the URL"
    );
    is(
      parameterValues[i].value,
      expectedParameters[i].value,
      "The query param value in the form should match on the URL"
    );
  }

  info("Adding new parameters by query parameters section");

  const newParameterName = document.querySelector(
    "#http-custom-query .map-add-new-inputs .http-custom-input-name"
  );
  newParameterName.focus();
  EventUtils.sendString("My-param");

  is(
    document.querySelector("#http-custom-url-value").value,
    expectedInputValueAfterAddingParamName,
    "The URL should be updated with the new parameter name"
  );

  const newParameterValue = Array.from(
    document.querySelectorAll(
      "#http-custom-query .http-custom-input .http-custom-input-value"
    )
  ).pop();
  newParameterValue.focus();
  EventUtils.sendString("my-value");

  // Check if the url is updated
  is(
    document.querySelector("#http-custom-url-value").value,
    expectedInputValueAfterAddingParamValue,
    "The URL should be updated with the new parameter value"
  );

  is(
    document.querySelectorAll(
      "#http-custom-query .tabpanel-summary-container.http-custom-input"
    ).length,
    expectedParametersSize + 1,
    "The parameter section should now have a new element"
  );

  info(
    "Uncheck the parameter an make sure the parameter is removed from the new url"
  );
  const params = document.querySelectorAll(
    "#http-custom-query  .tabpanel-summary-container.http-custom-input"
  );

  const lastParam = Array.from(params).at(-1);
  const checkbox = lastParam.querySelector("input[type=checkbox]");
  checkbox.click();

  // Check if the url is updated after uncheck one parameter through the parameter section
  is(
    document.querySelector("#http-custom-url-value").value,
    expectedInputValueAfterUncheckingParam,
    "The URL should be updated after unchecking the added parameter"
  );
}
