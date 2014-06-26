/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

loadHelperScript("helper_edits.js");

// Test menu bar enabled / disabled state.

let test = asyncTest(function*() {
  let projecteditor = yield addProjectEditorTabForTempDirectory();
  let menubar = projecteditor.menubar;

  // let projecteditor = yield addProjectEditorTabForTempDirectory();
  ok(projecteditor, "ProjectEditor has loaded");

  let fileMenu = menubar.querySelector("#file-menu");
  let editMenu = menubar.querySelector("#edit-menu");
  ok (fileMenu, "The menu has loaded in the projecteditor document");
  ok (editMenu, "The menu has loaded in the projecteditor document");

  let cmdNew = fileMenu.querySelector("[command=cmd-new]");
  let cmdSave = fileMenu.querySelector("[command=cmd-save]");
  let cmdSaveas = fileMenu.querySelector("[command=cmd-saveas]");

  let cmdUndo = editMenu.querySelector("[command=cmd_undo]");
  let cmdRedo = editMenu.querySelector("[command=cmd_redo]");
  let cmdCut = editMenu.querySelector("[command=cmd_cut]");
  let cmdCopy = editMenu.querySelector("[command=cmd_copy]");
  let cmdPaste = editMenu.querySelector("[command=cmd_paste]");

  info ("Checking initial state of menus");
  yield openAndCloseMenu(fileMenu);
  yield openAndCloseMenu(editMenu);

  is (cmdNew.getAttribute("disabled"), "", "File menu item is enabled");
  is (cmdSave.getAttribute("disabled"), "true", "File menu item is disabled");
  is (cmdSaveas.getAttribute("disabled"), "true", "File menu item is disabled");

  is (cmdUndo.getAttribute("disabled"), "true", "Edit menu item is disabled");
  is (cmdRedo.getAttribute("disabled"), "true", "Edit menu item is disabled");
  is (cmdCut.getAttribute("disabled"), "true", "Edit menu item is disabled");
  is (cmdCopy.getAttribute("disabled"), "true", "Edit menu item is disabled");
  is (cmdPaste.getAttribute("disabled"), "true", "Edit menu item is disabled");

  projecteditor.menuEnabled = false;

  info ("Checking with menuEnabled = false");
  yield openAndCloseMenu(fileMenu);
  yield openAndCloseMenu(editMenu);

  is (cmdNew.getAttribute("disabled"), "true", "File menu item is disabled");
  is (cmdSave.getAttribute("disabled"), "true", "File menu item is disabled");
  is (cmdSaveas.getAttribute("disabled"), "true", "File menu item is disabled");

  is (cmdUndo.getAttribute("disabled"), "true", "Edit menu item is disabled");
  is (cmdRedo.getAttribute("disabled"), "true", "Edit menu item is disabled");
  is (cmdCut.getAttribute("disabled"), "true", "Edit menu item is disabled");
  is (cmdCopy.getAttribute("disabled"), "true", "Edit menu item is disabled");
  is (cmdPaste.getAttribute("disabled"), "true", "Edit menu item is disabled");

  info ("Checking with menuEnabled=true");
  projecteditor.menuEnabled = true;

  yield openAndCloseMenu(fileMenu);
  yield openAndCloseMenu(editMenu);

  is (cmdNew.getAttribute("disabled"), "", "File menu item is enabled");
  is (cmdSave.getAttribute("disabled"), "true", "File menu item is disabled");
  is (cmdSaveas.getAttribute("disabled"), "true", "File menu item is disabled");

  is (cmdUndo.getAttribute("disabled"), "true", "Edit menu item is disabled");
  is (cmdRedo.getAttribute("disabled"), "true", "Edit menu item is disabled");
  is (cmdCut.getAttribute("disabled"), "true", "Edit menu item is disabled");
  is (cmdCopy.getAttribute("disabled"), "true", "Edit menu item is disabled");
  is (cmdPaste.getAttribute("disabled"), "true", "Edit menu item is disabled");

  info ("Checking with resource selected");
  let resource = projecteditor.project.allResources()[2];
  yield selectFile(projecteditor, resource);
  let editor = projecteditor.currentEditor;

  editor.editor.focus();
  EventUtils.synthesizeKey("foo", { }, projecteditor.window);

  yield openAndCloseMenu(fileMenu);
  yield openAndCloseMenu(editMenu);

  is (cmdNew.getAttribute("disabled"), "", "File menu item is enabled");
  is (cmdSave.getAttribute("disabled"), "", "File menu item is enabled");
  is (cmdSaveas.getAttribute("disabled"), "", "File menu item is enabled");

  is (cmdUndo.getAttribute("disabled"), "", "Edit menu item is enabled");
  is (cmdRedo.getAttribute("disabled"), "true", "Edit menu item is disabled");
  is (cmdCut.getAttribute("disabled"), "true", "Edit menu item is disabled");
  is (cmdCopy.getAttribute("disabled"), "true", "Edit menu item is disabled");
  is (cmdPaste.getAttribute("disabled"), "", "Edit menu item is enabled");
});

function openAndCloseMenu(menu) {
  let shown = onPopupShow(menu)
  EventUtils.synthesizeMouseAtCenter(menu, {}, menu.ownerDocument.defaultView);
  yield shown;
  let hidden = onPopupHidden(menu)
  EventUtils.synthesizeMouseAtCenter(menu, {}, menu.ownerDocument.defaultView);
  yield hidden;
}

function onPopupShow(menu) {
  let defer = promise.defer();
  menu.addEventListener("popupshown", function onpopupshown() {
    menu.removeEventListener("popupshown", onpopupshown);
    defer.resolve();
  });
  return defer.promise;
}

function onPopupHidden(menu) {
  let defer = promise.defer();
  menu.addEventListener("popuphidden", function onpopupshown() {
    menu.removeEventListener("popuphidden", onpopupshown);
    defer.resolve();
  });
  return defer.promise;
}
