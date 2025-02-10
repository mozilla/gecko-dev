/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env module */

const { AppConstants } = ChromeUtils.importESModule(
  "resource://gre/modules/AppConstants.sys.mjs"
);

const { Interactions } = ChromeUtils.importESModule(
  "resource:///modules/Interactions.sys.mjs"
);
const { PlacesUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/PlacesUtils.sys.mjs"
);
const { PlacesDBUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/PlacesDBUtils.sys.mjs"
);

const lazy = {};

ChromeUtils.defineLazyGetter(lazy, "PlacesFrecencyRecalculator", () => {
  return Cc["@mozilla.org/places/frecency-recalculator;1"].getService(
    Ci.nsIObserver
  ).wrappedJSObject;
});

/**
 * Methods of sorting.
 *
 * @readonly
 * @enum {SortingType}
 */
const SortingType = {
  ASCENDING: "ASC",
  DESCENDING: "DESC",
};

/**
 * How to sort a table of values.
 *
 * @typedef SortSetting
 *
 * @property {string} column
 *   Which column the table should be sorted by.
 * @property {SortingType} order
 *   How order the sorting.
 */

/**
 * Base class for the table display. Handles table layout and updates.
 */
class TableViewer {
  /**
   * Maximum number of rows to display by default.
   *
   * @type {number}
   */
  maxRows = 100;

  /**
   * The number of rows that we last filled in on the table. This allows
   * tracking to know when to clear unused rows.
   *
   * @type {number}
   */
  #lastFilledRows = 0;

  /**
   * A map of columns that are displayed by default. This is set by sub-classes.
   *
   * - The key is the column name in the database.
   * - The header is the column header on the table.
   * - The modifier is a function to modify the returned value from the database
   *   for display.
   * - includeTitle determines if the title attribute should be set on that
   *   column, for tooltips, e.g. if an element is likely to overflow.
   *
   * @type {Map<string, object>}
   */
  columnMap;

  /**
   * A reference for the current interval timer, if any.
   *
   * @type {number}
   */
  #timer;

  /**
   * How the table should be sorted. If not provided, the view will not allow
   * sorting and default to the initial way the rows were pulled from the data
   * source.
   *
   * @type {SortSetting}
   */
  sortSetting = null;

  /**
   * Starts the display of the table. Setting up the table display and doing
   * an initial output. Also starts the interval timer.
   */
  async start() {
    this.setupUI();
    await this.updateDisplay();
    this.#timer = setInterval(this.updateDisplay.bind(this), 10000);
  }

  /**
   * Pauses updates for this table, use start() to re-start.
   */
  pause() {
    if (this.#timer) {
      clearInterval(this.#timer);
      this.#timer = null;
    }
  }

  /**
   * Creates the initial table layout and sets the styles to match the number
   * of columns.
   */
  setupUI() {
    document.getElementById("title").textContent = this.title;

    let viewer = document.getElementById("tableViewer");
    viewer.textContent = "";

    // Set up the table styles.
    let existingStyle = document.getElementById("tableStyle");
    let numColumns = this.columnMap.size;
    let styleText = `
#tableViewer {
  display: grid;
  grid-template-columns: ${this.cssGridTemplateColumns}
}

/* Sets the first row of elements to bold. The number is the number of columns */
#tableViewer > div:nth-child(-n+${numColumns}) {
  font-weight: bold;
  white-space: break-spaces;
}

/* Highlights every other row to make visual scanning of the table easier.
   The numbers need to be adapted if the number of columns changes. */
`;
    for (let i = numColumns + 1; i <= numColumns * 2 - 1; i++) {
      styleText += `#tableViewer > div:nth-child(${numColumns}n+${i}):nth-child(${
        numColumns * 2
      }n+${i}),\n`;
    }
    styleText += `#tableViewer > div:nth-child(${numColumns}n+${
      numColumns * 2
    }):nth-child(${numColumns * 2}n+${numColumns * 2})\n
{
  background: var(--table-row-background-color-alternate);
}`;
    existingStyle.innerText = styleText;

    // Now set up the table itself with empty cells, this avoids having to
    // create and delete rows all the time.
    let tableBody = document.createDocumentFragment();
    let header = document.createDocumentFragment();
    for (let [key, details] of this.columnMap.entries()) {
      let columnDiv = document.createElement("div");
      columnDiv.classList.add("column-title");
      columnDiv.setAttribute("data-column-title", key);
      columnDiv.textContent = details.header;
      header.appendChild(columnDiv);
    }
    tableBody.appendChild(header);

    for (let i = 0; i < this.maxRows; i++) {
      let row = document.createDocumentFragment();
      for (let j = 0; j < this.columnMap.size; j++) {
        row.appendChild(document.createElement("div"));
      }
      tableBody.appendChild(row);
    }
    viewer.appendChild(tableBody);

    let limit = document.getElementById("tableLimit");
    limit.textContent = `Maximum rows displayed: ${this.maxRows}.`;

    this.#lastFilledRows = 0;
  }

  /**
   * Displays the provided data in the table.
   *
   * @param {object[]} rows
   *   An array of rows to display. The rows are objects with the values for
   *   the rows being the keys of the columnMap.
   */
  displayData(rows) {
    if (gCurrentHandler != this) {
      /* Data is no more relevant for the current view. */
      return;
    }
    let viewer = document.getElementById("tableViewer");
    let index = this.columnMap.size;
    for (let row of rows) {
      for (let [column, details] of this.columnMap.entries()) {
        let value = row[column];

        if (details.includeTitle) {
          viewer.children[index].setAttribute("title", value);
        }

        viewer.children[index].textContent = details.modifier
          ? details.modifier(value)
          : value;

        index++;
      }
    }
    let numRows = rows.length;
    if (numRows < this.#lastFilledRows) {
      for (let r = numRows; r < this.#lastFilledRows; r++) {
        for (let c = 0; c < this.columnMap.size; c++) {
          viewer.children[index].textContent = "";
          viewer.children[index].removeAttribute("title");
          index++;
        }
      }
    }
    this.#lastFilledRows = numRows;

    this.updateDisplayedSort();
  }

  updateDisplayedSort() {
    if (this.sortable) {
      let viewer = document.getElementById("tableViewer");
      let element = viewer.querySelector(
        `[data-column-title="${this.sortSetting.column}"]`
      );
      let symbolHolder = document.getElementById("column-title-sort-indicator");
      if (!symbolHolder) {
        symbolHolder = document.createElement("span");
        symbolHolder.style.marginLeft = "5px";
        // Let the column header receive the click.
        symbolHolder.style.pointerEvents = "none";
        symbolHolder.id = "column-title-sort-indicator";
      }
      element.appendChild(symbolHolder);
      symbolHolder.textContent =
        this.sortSetting.order == SortingType.DESCENDING
          ? "\u2B07\uFE0F"
          : "\u2B06\uFE0F";
    }
  }

  changeSort(column) {
    if (this.sortSetting.column == column) {
      this.sortSetting.order =
        this.sortSetting.order == SortingType.DESCENDING
          ? SortingType.ASCENDING
          : SortingType.DESCENDING;
    } else {
      this.sortSetting = { column, order: SortingType.DESCENDING };
    }
  }

  get sortable() {
    return !!this.sortSetting;
  }
}

/**
 * Viewer definition for the page metadata.
 */
const metadataHandler = new (class extends TableViewer {
  title = "Interactions";
  cssGridTemplateColumns =
    "max-content fit-content(100%) repeat(6, min-content) fit-content(100%);";

  /**
   * @see TableViewer.columnMap
   */
  columnMap = new Map([
    ["id", { header: "ID" }],
    ["url", { header: "URL", includeTitle: true }],
    [
      "updated_at",
      {
        header: "Updated",
        modifier: updatedAt => new Date(updatedAt).toLocaleString(),
      },
    ],
    [
      "total_view_time",
      {
        header: "View Time (s)",
        modifier: totalViewTime => (totalViewTime / 1000).toFixed(2),
      },
    ],
    [
      "typing_time",
      {
        header: "Typing Time (s)",
        modifier: typingTime => (typingTime / 1000).toFixed(2),
      },
    ],
    ["key_presses", { header: "Key Presses" }],
    [
      "scrolling_time",
      {
        header: "Scroll Time (s)",
        modifier: scrollingTime => (scrollingTime / 1000).toFixed(2),
      },
    ],
    ["scrolling_distance", { header: "Scroll Distance (pixels)" }],
    ["referrer", { header: "Referrer", includeTitle: true }],
  ]);

  sortSetting = { column: "updated_at", order: SortingType.DESCENDING };

  /**
   * A reference to the database connection.
   *
   * @type {mozIStorageConnection}
   */
  #db = null;

  async #getRows(query, columns = [...this.columnMap.keys()]) {
    if (!this.#db) {
      this.#db = await PlacesUtils.promiseDBConnection();
    }
    let rows = await this.#db.executeCached(query);
    return rows.map(r => {
      let result = {};
      for (let column of columns) {
        result[column] = r.getResultByName(column);
      }
      return result;
    });
  }

  /**
   * Loads the current metadata from the database and updates the display.
   */
  async updateDisplay() {
    let rows = await this.#getRows(
      `SELECT m.id AS id, h.url AS url, updated_at, total_view_time,
              typing_time, key_presses, scrolling_time, scrolling_distance, h2.url as referrer
       FROM moz_places_metadata m
       JOIN moz_places h ON h.id = m.place_id
       LEFT JOIN moz_places h2 ON h2.id = m.referrer_place_id
       ORDER BY ${this.sortSetting.column} ${this.sortSetting.order}
       LIMIT ${this.maxRows}`
    );
    this.displayData(rows);
  }

  export(includeUrlAndTitle = false) {
    return this.#getRows(
      `SELECT
      m.id,
      ${includeUrlAndTitle ? "h.title," : ""}
      ${includeUrlAndTitle ? "h.url" : "m.place_id"},
      m.updated_at,
      h.frecency,
      m.total_view_time,
      m.typing_time,
      m.key_presses,
      m.scrolling_time,
      m.scrolling_distance,
      ${includeUrlAndTitle ? "r.url AS referrer_url" : "m.referrer_place_id"},
      ${includeUrlAndTitle ? "o.host" : "h.origin_id"},
      h.visit_count,
      vall.visit_dates,
      vall.visit_types
  FROM moz_places_metadata m
  JOIN moz_places h ON h.id = m.place_id
  JOIN
      (SELECT
          place_id,
          group_concat(visit_date, ',') AS visit_dates,
          group_concat(visit_type, ',') AS visit_types
      FROM moz_historyvisits
      GROUP BY place_id
      ORDER BY visit_date DESC
      ) vall ON vall.place_id = m.place_id
  JOIN moz_origins o ON h.origin_id = o.id
  LEFT JOIN moz_places r ON m.referrer_place_id = r.id

  ORDER BY m.place_id DESC
     `,
      [
        "id",
        ...(includeUrlAndTitle ? ["title"] : []),
        includeUrlAndTitle ? "url" : "place_id",
        "updated_at",
        "frecency",
        "total_view_time",
        "typing_time",
        "key_presses",
        "scrolling_time",
        "scrolling_distance",
        includeUrlAndTitle ? "referrer_url" : "referrer_place_id",
        includeUrlAndTitle ? "host" : "origin_id",
        "visit_count",
        "visit_dates",
        "visit_types",
      ]
    );
  }
})();

/**
 * Viewer definition for the Places database stats.
 */
const placesStatsHandler = new (class extends TableViewer {
  title = "Places Database Statistics";
  cssGridTemplateColumns = "fit-content(100%) repeat(5, max-content);";

  /**
   * @see TableViewer.columnMap
   */
  columnMap = new Map([
    ["entity", { header: "Entity" }],
    ["count", { header: "Count" }],
    [
      "sizeBytes",
      {
        header: "Size (KiB)",
        modifier: c => c / 1024,
      },
    ],
    [
      "sizePerc",
      {
        header: "Size (Perc.)",
      },
    ],
    [
      "efficiencyPerc",
      {
        header: "Space Eff. (Perc.)",
      },
    ],
    [
      "sequentialityPerc",
      {
        header: "Sequentiality (Perc.)",
      },
    ],
  ]);

  /**
   * Loads the current metadata from the database and updates the display.
   */
  async updateDisplay() {
    let data = await PlacesDBUtils.getEntitiesStatsAndCounts();
    this.displayData(data);
  }
})();

/**
 * Places database with frecency scores.
 */
const placesViewerHandler = new (class extends TableViewer {
  title = "Places Viewer";
  cssGridTemplateColumns = "fit-content(100%) repeat(6, min-content);";
  #db = null;
  #maxRows = 100;

  /**
   * @see TableViewer.columnMap
   */
  columnMap = new Map([
    ["url", { header: "URL" }],
    ["title", { header: "Title" }],
    [
      "last_visit_date",
      {
        header: "Last Visit Date",
        modifier: lastVisitDate =>
          new Date(lastVisitDate / 1000).toLocaleString(),
      },
    ],
    ["frecency", { header: "Frecency" }],
    [
      "recalc_frecency",
      {
        header: "Recalc Frecency",
      },
    ],
    [
      "alt_frecency",
      {
        header: "Alt Frecency",
      },
    ],
    [
      "recalc_alt_frecency",
      {
        header: "Recalc Alt Frecency",
      },
    ],
  ]);

  sortSetting = { column: "last_visit_date", order: SortingType.DESCENDING };

  async #getRows(query, columns = [...this.columnMap.keys()]) {
    if (!this.#db) {
      this.#db = await PlacesUtils.promiseDBConnection();
    }
    let rows = await this.#db.executeCached(query);
    return rows.map(r => {
      let result = {};
      for (let column of columns) {
        result[column] = r.getResultByName(column);
      }
      return result;
    });
  }

  /**
   * Loads the current metadata from the database and updates the display.
   */
  async updateDisplay() {
    let rows = await this.#getRows(
      `
        SELECT
          url,
          title,
          last_visit_date,
          frecency,
          recalc_frecency,
          alt_frecency,
          recalc_alt_frecency
        FROM moz_places
        ORDER BY ${this.sortSetting.column} ${this.sortSetting.order}
        LIMIT ${this.#maxRows}`
    );
    this.displayData(rows);
  }
})();

function checkPrefs() {
  if (
    !Services.prefs.getBoolPref("browser.places.interactions.enabled", false)
  ) {
    let warning = document.getElementById("enabledWarning");
    warning.hidden = false;
  }
}

function show(selectedButton) {
  let currentButton = document.querySelector(".category.selected");
  if (currentButton == selectedButton) {
    return;
  }

  gCurrentHandler.pause();
  currentButton.classList.remove("selected");
  selectedButton.classList.add("selected");
  switch (selectedButton.getAttribute("value")) {
    case "metadata":
      (gCurrentHandler = metadataHandler).start();
      metadataHandler.start();
      break;
    case "places-stats":
      (gCurrentHandler = placesStatsHandler).start();
      break;
    case "places-viewer":
      (gCurrentHandler = placesViewerHandler).start();
      break;
  }
}

function createObjectURL(data, type) {
  // Downloading the Blob will throw errors in debug mode because the
  // principal is system and nsUrlClassifierDBService::lookup does not expect
  // a caller from this principal. Thus, we use the null principal. However, in
  // non-debug mode we'd rather not run eval and use the Javascript API.
  if (AppConstants.DEBUG) {
    let escapedData = data.replaceAll("'", "\\'").replaceAll("\n", "\\n");
    let sb = new Cu.Sandbox(null, { wantGlobalProperties: ["Blob", "URL"] });
    return Cu.evalInSandbox(
      `URL.createObjectURL(new Blob(['${escapedData}'], {type: '${type}'}))`,
      sb,
      "",
      null,
      0,
      false
    );
  }
  let blob = new Blob([data], {
    type,
  });
  return window.URL.createObjectURL(blob);
}

function downloadFile(data, blobType, fileType) {
  const a = document.createElement("a");
  a.setAttribute("download", `places-${Date.now()}.${fileType}`);
  a.setAttribute("href", createObjectURL(data, blobType));
  a.click();
  a.remove();
}

async function getData() {
  let includeUrlAndTitle =
    document.getElementById("include-place-data").checked;
  return await metadataHandler.export(includeUrlAndTitle);
}

function setupListeners() {
  let menu = document.getElementById("categories");
  menu.addEventListener("click", e => {
    if (e.target && e.target.parentNode == menu) {
      show(e.target);
    }
  });

  document.getElementById("export-json").addEventListener("click", async e => {
    e.preventDefault();
    const data = await getData();
    downloadFile(JSON.stringify(data), "text/json;charset=utf-8", "json");
  });

  document.getElementById("export-csv").addEventListener("click", async e => {
    e.preventDefault();
    const data = await getData();

    // Convert Javascript to CSV string.
    let headers = Object.keys(data.at(0));
    let rows = [
      headers.join(","),
      ...data.map(obj =>
        headers.map(field => JSON.stringify(obj[field] ?? "")).join(",")
      ),
    ];
    rows = rows.join("\n");

    downloadFile(rows, "text/csv", "csv");
  });

  // Allow users to force frecency to update instead of waiting for an idle
  // event.
  document
    .getElementById("recalc-alt-frecency")
    .addEventListener("click", async e => {
      e.preventDefault();
      lazy.PlacesFrecencyRecalculator.recalculateAnyOutdatedFrecencies();
    });

  document.getElementById("tableViewer").addEventListener("click", e => {
    if (gCurrentHandler.sortable && e.target.dataset.columnTitle) {
      gCurrentHandler.changeSort(e.target.dataset.columnTitle);
      gCurrentHandler.updateDisplay();
    }
  });
}

checkPrefs();
// Set the initial handler here.
let gCurrentHandler = metadataHandler;
gCurrentHandler.start().catch(console.error);
setupListeners();
