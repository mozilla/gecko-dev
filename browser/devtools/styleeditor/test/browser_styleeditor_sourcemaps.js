/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// https rather than chrome to improve coverage
const TESTCASE_URI = TEST_BASE_HTTPS + "sourcemaps.html";
const PREF = "devtools.styleeditor.source-maps-enabled";


const contents = {
  "sourcemaps.scss": [
    "",
    "$paulrougetpink: #f06;",
    "",
    "div {",
    "  color: $paulrougetpink;",
    "}",
    "",
    "span {",
    "  background-color: #EEE;",
    "}"
  ].join("\n"),
  "contained.scss": [
    "$pink: #f06;",
    "",
    "#header {",
    "  color: $pink;",
    "}"
  ].join("\n"),
  "sourcemaps.css": [
    "div {",
    "  color: #ff0066; }",
    "",
    "span {",
    "  background-color: #EEE; }",
    "",
    "/*# sourceMappingURL=sourcemaps.css.map */"
  ].join("\n"),
  "contained.css": [
    "#header {",
    "  color: #f06; }",
    "",
    "/*# sourceMappingURL=data:application/json;base64,eyJ2ZXJzaW9uIjozLCJmaWxlIjoiIiwic291cmNlcyI6WyJzYXNzL2NvbnRhaW5lZC5zY3NzIl0sIm5hbWVzIjpbXSwibWFwcGluZ3MiOiJBQUVBO0VBQ0UsT0FISyIsInNvdXJjZXNDb250ZW50IjpbIiRwaW5rOiAjZjA2O1xuXG4jaGVhZGVyIHtcbiAgY29sb3I6ICRwaW5rO1xufSJdfQ==*/"
  ].join("\n")
}

const cssNames = ["sourcemaps.css", "contained.css"];
const scssNames = ["sourcemaps.scss", "contained.scss"];

waitForExplicitFinish();

let test = asyncTest(function*() {
  Services.prefs.setBoolPref(PREF, true);

  let {UI} = yield addTabAndOpenStyleEditors(5, null, TESTCASE_URI);

  is(UI.editors.length, 3,
    "correct number of editors with source maps enabled");

  // Test first plain css editor
  testFirstEditor(UI.editors[0]);

  // Test Scss editors
  yield testEditor(UI.editors[1], scssNames);
  yield testEditor(UI.editors[2], scssNames);

  // Test disabling original sources
  yield togglePref(UI);

  is(UI.editors.length, 3, "correct number of editors after pref toggled");

  // Test CSS editors
  yield testEditor(UI.editors[1], cssNames);
  yield testEditor(UI.editors[2], cssNames);

  Services.prefs.clearUserPref(PREF);
});

function testFirstEditor(editor) {
  let name = getStylesheetNameFor(editor);
  is(name, "simple.css", "First style sheet display name is correct");
}

function testEditor(editor, possibleNames) {
  let name = getStylesheetNameFor(editor);
  ok(possibleNames.indexOf(name) >= 0, name + " editor name is correct");

  return openEditor(editor).then(() => {
    let expectedText = contents[name];

    let text = editor.sourceEditor.getText();
    is(text, expectedText, name + " editor contains expected text");
  });
}

/* Helpers */

function togglePref(UI) {
  let deferred = promise.defer();
  let count = 0;

  UI.on("editor-added", (event, editor) => {
    if (++count == 3) {
      deferred.resolve();
    }
  })
  let editorsPromise = deferred.promise;

  let selectedPromise = UI.once("editor-selected");

  Services.prefs.setBoolPref(PREF, false);

  return promise.all([editorsPromise, selectedPromise]);
}

function openEditor(editor) {
  getLinkFor(editor).click();

  return editor.getSourceEditor();
}

function getLinkFor(editor) {
  return editor.summary.querySelector(".stylesheet-name");
}

function getStylesheetNameFor(editor) {
  return editor.summary.querySelector(".stylesheet-name > label")
         .getAttribute("value")
}
