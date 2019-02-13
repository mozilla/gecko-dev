/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const URL = "data:text/html;charset=utf8,test for textbox context menu";

add_task(function*() {
  let tab = yield addTab(URL);
  let toolbox = yield gDevTools.showToolbox(TargetFactory.forTab(tab));
  let textboxContextMenu = toolbox.textboxContextMenuPopup;

  ok(textboxContextMenu, "The textbox context menu is loaded in the toolbox");

  let cmdUndo = textboxContextMenu.querySelector("[command=cmd_undo]");
  let cmdDelete = textboxContextMenu.querySelector("[command=cmd_delete]");
  let cmdSelectAll = textboxContextMenu.querySelector("[command=cmd_selectAll]");
  let cmdCut = textboxContextMenu.querySelector("[command=cmd_cut]");
  let cmdCopy = textboxContextMenu.querySelector("[command=cmd_copy]");
  let cmdPaste = textboxContextMenu.querySelector("[command=cmd_paste]");

  info("Opening context menu");

  let onContextMenuPopup = once(textboxContextMenu, "popupshowing");
  textboxContextMenu.openPopupAtScreen(0, 0, true);
  yield onContextMenuPopup;

  is(cmdUndo.getAttribute("disabled"), "true", "cmdUndo is disabled");
  is(cmdDelete.getAttribute("disabled"), "true", "cmdDelete is disabled");
  is(cmdSelectAll.getAttribute("disabled"), "", "cmdSelectAll is enabled");
  is(cmdCut.getAttribute("disabled"), "true", "cmdCut is disabled");
  is(cmdCopy.getAttribute("disabled"), "true", "cmdCopy is disabled");
  is(cmdPaste.getAttribute("disabled"), "true", "cmdPaste is disabled");

  yield cleanup(toolbox);
});

function* cleanup(toolbox) {
  yield toolbox.destroy();
  gBrowser.removeCurrentTab();
}
