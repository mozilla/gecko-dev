// Test endless scrolling when a lot of items are present in the storage
// inspector table.
"use strict";

add_task(function* () {
  yield openTabAndSetupStorage(MAIN_DOMAIN + "storage-overflow.html");

  let $ = id => gPanelWindow.document.querySelector(id);
  let $$ = sel => gPanelWindow.document.querySelectorAll(sel);

  gUI.tree.expandAll();
  yield selectTreeItem(["localStorage", "http://test1.example.org"]);

  let table = $("#storage-table .table-widget-body");
  let cellHeight = $(".table-widget-cell").getBoundingClientRect().height;

  is($$("#value .table-widget-cell").length, 50,
     "Table should initially display 50 items");

  let onStoresUpdate = gUI.once("store-objects-updated");
  table.scrollTop += cellHeight * 50;
  yield onStoresUpdate;

  is($$("#value .table-widget-cell").length, 100,
     "Table should display 100 items after scrolling");

  onStoresUpdate = gUI.once("store-objects-updated");
  table.scrollTop += cellHeight * 50;
  yield onStoresUpdate;

  is($$("#value .table-widget-cell").length, 150,
     "Table should display 150 items after scrolling");

  onStoresUpdate = gUI.once("store-objects-updated");
  table.scrollTop += cellHeight * 50;
  yield onStoresUpdate;

  is($$("#value .table-widget-cell").length, 160,
     "Table should display all 160 items after scrolling");
  yield finishTests();
});
