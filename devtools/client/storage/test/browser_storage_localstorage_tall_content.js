"use strict";

// List of rows initially inserted by the test HTML
const ROW_IDS = ["emoji", "tall", "long"];

add_task(async function test_storage_layout_tall_content() {
  // Load the HTML page that pre-fills localStorage
  await openTabAndSetupStorage(
    MAIN_DOMAIN_SECURED + "storage-localstorage-tall-content.html"
  );

  // Ensure all columns are visible before testing layout
  showAllColumns(true);

  // Focus on the relevant storage tree item
  await selectTreeItem(["localStorage", "https://test1.example.org"]);

  // Check row heights before any actions
  testRowHeights(ROW_IDS, "initial layout");

  // Click to sort and check again
  clickColumnHeader("value");
  testRowHeights(ROW_IDS, "after sorting by value");

  // Add a new tall value and test layout
  await updateLocalStorageItem("newTall", "add", "🧵".repeat(300));
  testRowHeights(["newTall"], "after adding new tall value");

  // Edit an existing row to be taller
  await updateLocalStorageItem("emoji", "edit", "🧵".repeat(400));
  testRowHeights(["emoji"], "after editing 'emoji' to be taller");

  // Remove the tall one and test remaining layout
  await updateLocalStorageItem("newTall", "remove");
  testRowHeights(ROW_IDS, "after removing 'newTall'");
});

function testRowHeights(rowIds, description) {
  info(`Checking row heights: ${description}`);
  for (const rowId of rowIds) {
    checkRowHeights(rowId);
  }
}

function checkRowHeights(rowId) {
  const cells = getRowCells(rowId, true);
  const nameCell = cells.name;
  const valueCell = cells.value;

  ok(
    nameCell && valueCell,
    `Both name and value cells exist for row '${rowId}'`
  );

  const nameHeight = nameCell.getBoundingClientRect().height;
  const valueHeight = valueCell.getBoundingClientRect().height;

  is(
    nameHeight,
    valueHeight,
    `Row '${rowId}' columns have matching heights of ${valueHeight}px`
  );
}

async function updateLocalStorageItem(key, action, value = "true") {
  await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [key, action, value],
    (k, a, v) => {
      switch (a) {
        case "add":
        case "edit":
          content.localStorage.setItem(k, v);
          break;
        case "remove":
          content.localStorage.removeItem(k);
          break;
        default:
          throw new Error(`Unknown action: ${a}`);
      }
    }
  );

  info(`${action.toUpperCase()} "${key}"`);

  // Choose the correct event based on the action
  const event =
    action === "remove" ? "store-objects-edit" : "store-objects-updated";
  await gUI.once(event);
}
