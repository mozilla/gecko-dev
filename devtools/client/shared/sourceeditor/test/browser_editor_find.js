/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const L10N = new LocalizationHelper(
  "devtools/client/locales/sourceeditor.properties"
);
const FIND_KEY = L10N.getStr("find.key");

add_task(async function () {
  const { ed, win } = await setup();
  ed.setText(`
    // line 1
    //  line 2
    //   line 3
    //    line 4
    //     line 5
  `);

  await promiseWaitForFocus();

  const editorDocument = ed.container.contentDocument;
  const editorWindow = editorDocument.defaultView;

  ok(
    !getSearchInput(editorDocument),
    "Search input isn't visible in the beginning"
  );

  // The editor needs the focus to properly receive the `synthesizeKey`
  ed.focus();
  synthesizeKeyShortcut(FIND_KEY, editorWindow);

  let input = getSearchInput(editorDocument);
  ok(!!input, "Search input is visible after hitting the keyboard shortcut");

  info(`Search for "line"`);
  input.value = "line";

  info("Hit Enter to trigger the search");
  EventUtils.synthesizeKey("VK_RETURN", {}, editorWindow);
  ch(
    ed.getCursor(),
    { line: 1, ch: 11 },
    `Editor cursor is on the first result`
  );
  ok(
    !!getSearchInput(editorDocument),
    "Search input is still visible after hitting Enter"
  );

  info("Hit Enter again to navigate to next result");
  EventUtils.synthesizeKey("VK_RETURN", {}, editorWindow);
  ch(
    ed.getCursor(),
    { line: 2, ch: 12 },
    `Editor cursor moved to the second result`
  );
  ok(
    !!getSearchInput(editorDocument),
    "Search input is still visible after hitting Enter a second time"
  );

  info("Hit Shift+Enter again to navigate to previous result");
  EventUtils.synthesizeKey("VK_RETURN", { shiftKey: true }, editorWindow);
  ch(
    ed.getCursor(),
    { line: 1, ch: 11 },
    `Editor cursor is back on the first result`
  );
  ok(
    !!getSearchInput(editorDocument),
    "Search input is still visible after hitting Shift+Enter"
  );

  info("Hit Escape to close the search input");
  getSearchInput(editorDocument).focus();
  EventUtils.synthesizeKey("VK_ESCAPE", {}, editorWindow);
  await waitFor(() => !getSearchInput(editorDocument));
  ok(true, "Search input is hidden after hitting Escape");

  info("Display the search input again");

  synthesizeKeyShortcut(FIND_KEY, editorWindow);
  input = getSearchInput(editorDocument);
  ok(!!input, "Search input is visible after hitting the keyboard shortcut");
  is(input.value, "line", "input still has the expected value");

  info("Hit Enter to trigger the search");
  EventUtils.synthesizeKey("VK_RETURN", {}, editorWindow);
  ch(
    ed.getCursor(),
    { line: 2, ch: 12 },
    `Editor cursor is on the second result`
  );
  ok(
    !!getSearchInput(editorDocument),
    "Search input is still visible after hitting Enter"
  );

  info("Hit Enter again to navigate to next result");
  EventUtils.synthesizeKey("VK_RETURN", {}, editorWindow);
  ch(
    ed.getCursor(),
    { line: 3, ch: 13 },
    `Editor cursor moved to the third result`
  );
  ok(
    !!getSearchInput(editorDocument),
    "Search input is still visible after hitting Enter a second time"
  );

  info(
    "Check that triggering the Search again when the input is visible works as expected"
  );
  EventUtils.synthesizeKey("VK_RIGHT", {}, editorWindow);
  is(
    input.selectionStart,
    input.selectionEnd,
    "Search input text isn't selected"
  );
  synthesizeKeyShortcut(FIND_KEY, editorWindow);
  input = getSearchInput(editorDocument);
  ok(
    !!input,
    "Search input is still visible after hitting the keyboard shortcut"
  );
  is(input.value, "line", "input still has the expected value");
  is(
    input.selectionEnd - input.selectionStart,
    "line".length,
    "Search input text is selected after hitting the keyboard shortcut"
  );

  info("Hit Enter again to navigate to next result");
  EventUtils.synthesizeKey("VK_RETURN", {}, editorWindow);
  ch(
    ed.getCursor(),
    { line: 4, ch: 14 },
    `Editor cursor moved to the fourth result`
  );
  ok(
    !!getSearchInput(editorDocument),
    "Search input is still visible after pressing Enter after the search shortcut was hit"
  );

  teardown(ed, win);
});

function getSearchInput(editorDocument) {
  return editorDocument.querySelector("input[type=search]");
}
