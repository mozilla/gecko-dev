/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for autocomplete of CSS variables in the Rules view.

const IFRAME_URL = `https://example.org/document-builder.sjs?html=${encodeURIComponent(`
  <style>
    @property --iframe {
      syntax: "*";
      inherits: true;
    }
    body {
      --iframe-not-registered: turquoise;
    }

    h1 {
      color: tomato;
    }
  </style>
  <h1>iframe</h1>
`)}`;

const TEST_URI = `https://example.org/document-builder.sjs?html=
 <script>
    CSS.registerProperty({
      name: "--js",
      syntax: "<color>",
      inherits: false,
      initialValue: "gold"
    });
  </script>
  <style>
    @property --css {
      syntax: "<color>";
      inherits: false;
      initial-value: tomato;
    }

    h1 {
      --css: red;
      --not-registered: blue;
      --nested: var(--js);
      --nested-with-function: color-mix(in srgb, var(--css) 50%, var(--not-registered));
      color: gold;
    }
  </style>
  <h1>Hello world</h1>
  <iframe src="${encodeURIComponent(IFRAME_URL)}"></iframe>`;

add_task(async function () {
  await pushPref("layout.css.properties-and-values.enabled", true);

  await addTab(TEST_URI);
  const { inspector, view } = await openRuleView();
  await selectNode("h1", inspector);

  info("Wait for @property panel to be displayed");
  await waitFor(() =>
    view.styleDocument.querySelector("#registered-properties-container")
  );

  const topLevelVariables = [
    { label: "--css", postLabel: "rgb(255, 0, 0)", hasColorSwatch: true },
    { label: "--js", postLabel: "gold", hasColorSwatch: true },
    { label: "--nested", postLabel: "rgb(255, 215, 0)", hasColorSwatch: true },
    {
      label: "--nested-with-function",
      postLabel: "color-mix(in srgb, rgb(255, 0, 0) 50%, blue)",
      hasColorSwatch: true,
    },
    { label: "--not-registered", postLabel: "blue", hasColorSwatch: true },
  ];
  await checkNewPropertyCssVariableAutocomplete(view, topLevelVariables);

  await checkCssVariableAutocomplete(
    view,
    getTextProperty(view, 1, { color: "gold" }).editor.valueSpan,
    topLevelVariables
  );

  info(
    "Check that the list is correct when selecting a node from another document"
  );
  await selectNodeInFrames(["iframe", "h1"], inspector);

  const iframeVariables = [
    { label: "--iframe" },
    {
      label: "--iframe-not-registered",
      postLabel: "turquoise",
      hasColorSwatch: true,
    },
  ];
  await checkNewPropertyCssVariableAutocomplete(view, iframeVariables);

  await checkCssVariableAutocomplete(
    view,
    getTextProperty(view, 1, { color: "tomato" }).editor.valueSpan,
    iframeVariables
  );
});

async function checkNewPropertyCssVariableAutocomplete(
  view,
  expectedPopupItems
) {
  const ruleEditor = getRuleViewRuleEditor(view, 1);
  const editor = await focusNewRuleViewProperty(ruleEditor);
  const onPopupOpen = editor.popup.once("popup-opened");
  EventUtils.sendString("--");
  await onPopupOpen;

  assertEditorPopupItems(
    editor,
    // we don't display postLabel for the new property
    expectedPopupItems.map(item => ({ label: item.label }))
  );

  info("Close the popup");
  const onPopupClosed = once(editor.popup, "popup-closed");
  EventUtils.synthesizeKey("VK_ESCAPE", {}, view.styleWindow);
  await onPopupClosed;

  info("Close the editor");
  EventUtils.synthesizeKey("VK_ESCAPE", {}, view.styleWindow);
}

async function checkCssVariableAutocomplete(
  view,
  inplaceEditorEl,
  expectedPopupItems
) {
  const editor = await focusEditableField(view, inplaceEditorEl);
  await wait(500);

  const onCloseParenthesisAppended = editor.once("after-suggest");
  EventUtils.sendString("var(");
  await onCloseParenthesisAppended;

  let onRuleViewChanged = view.once("ruleview-changed");
  EventUtils.sendString("--");
  const onPopupOpen = editor.popup.once("popup-opened");
  view.debounce.flush();
  await onPopupOpen;
  assertEditorPopupItems(editor, expectedPopupItems);
  await onRuleViewChanged;

  info("Close the popup");
  const onPopupClosed = once(editor.popup, "popup-closed");
  EventUtils.synthesizeKey("VK_ESCAPE", {}, view.styleWindow);
  await onPopupClosed;

  info("Cancel");
  onRuleViewChanged = view.once("ruleview-changed");
  EventUtils.synthesizeKey("VK_ESCAPE", {}, view.styleWindow);
  await onRuleViewChanged;

  view.debounce.flush();
}

/**
 * Check that the popup items are the expected ones.
 *
 * @param {InplaceEditor} editor
 * @param {Array{Object}} expectedPopupItems
 */
function assertEditorPopupItems(editor, expectedPopupItems) {
  const popupListItems = Array.from(editor.popup._list.querySelectorAll("li"));
  is(
    popupListItems.length,
    expectedPopupItems.length,
    "Popup has expected number of items"
  );
  popupListItems.forEach((li, i) => {
    const expected = expectedPopupItems[i];
    const value =
      (li.querySelector(".initial-value")?.textContent ?? "") +
      li.querySelector(".autocomplete-value").textContent;
    is(value, expected.label, `Popup item #${i} as expected label`);

    // Don't pollute test logs if we don't have the expected variable
    if (value !== expected.label) {
      return;
    }

    const postLabelEl = li.querySelector(".autocomplete-postlabel");
    is(
      li.querySelector(".autocomplete-postlabel")?.textContent,
      expected.postLabel,
      `${expected.label} has expected post label`
    );
    is(
      !!postLabelEl?.querySelector(".autocomplete-swatch"),
      !!expected.hasColorSwatch,
      `${expected.label} ${
        expected.hasColorSwatch ? "has" : "does not have"
      } a post label color swatch`
    );
  });
}
