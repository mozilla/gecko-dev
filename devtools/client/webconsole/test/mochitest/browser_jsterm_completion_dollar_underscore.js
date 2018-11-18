/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that code completion works properly on $_.

"use strict";

const TEST_URI = `data:text/html;charset=utf8,<p>test code completion on $_`;

add_task(async function() {
  // Run test with legacy JsTerm
  await pushPref("devtools.webconsole.jsterm.codeMirror", false);
  await performTests();
  // And then run it with the CodeMirror-powered one.
  await pushPref("devtools.webconsole.jsterm.codeMirror", true);
  await performTests();
});

async function performTests() {
  const {jsterm} = await openNewTabAndConsole(TEST_URI);
  const {autocompletePopup} = jsterm;

  info("Test that there's no issue when trying to do an autocompletion without last " +
    "evaluation result");
  await setInputValueForAutocompletion(jsterm, "$_.");
  is(autocompletePopup.items.length, 0, "autocomplete popup has no items");
  is(autocompletePopup.isOpen, false, "autocomplete popup is not open");

  info("Populate $_ by executing a command");
  await jsterm.execute(`Object.create(null, Object.getOwnPropertyDescriptors({
    x: 1,
    y: "hello"
  }))`);

  await setInputValueForAutocompletion(jsterm, "$_.");
  checkJsTermCompletionValue(jsterm, "   x", "'$_.' completion (completeNode)");
  is(getAutocompletePopupLabels(autocompletePopup).join("|"), "x|y",
    "autocomplete popup has expected items");
  is(autocompletePopup.isOpen, true, "autocomplete popup is open");

  await setInputValueForAutocompletion(jsterm, "$_.x.");
  is(autocompletePopup.isOpen, true, "autocomplete popup is open");
  is(getAutocompletePopupLabels(autocompletePopup).includes("toExponential"), true,
    "autocomplete popup has expected items");

  await setInputValueForAutocompletion(jsterm, "$_.y.");
  is(autocompletePopup.isOpen, true, "autocomplete popup is open");
  is(getAutocompletePopupLabels(autocompletePopup).includes("trim"), true,
    "autocomplete popup has expected items");
}

function getAutocompletePopupLabels(autocompletePopup) {
  return autocompletePopup.items.map(i => i.label);
}
