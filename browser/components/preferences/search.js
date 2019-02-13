/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
                                  "resource://gre/modules/PlacesUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Task",
                                  "resource://gre/modules/Task.jsm");

const ENGINE_FLAVOR = "text/x-moz-search-engine";

var gEngineView = null;

var gSearchPane = {

  init: function ()
  {
    gEngineView = new EngineView(new EngineStore());
    document.getElementById("engineList").view = gEngineView;
    this.buildDefaultEngineDropDown();

    Services.obs.addObserver(this, "browser-search-engine-modified", false);
    window.addEventListener("unload", () => {
      Services.obs.removeObserver(this, "browser-search-engine-modified", false);
    });
  },

  buildDefaultEngineDropDown: function() {
    // This is called each time something affects the list of engines.
    let list = document.getElementById("defaultEngine");
    let currentEngine;

    // First, try to preserve the current selection.
    if (list.selectedItem)
      currentEngine = list.selectedItem.label;

    // If there's no current selection, use the current default engine.
    if (!currentEngine)
      currentEngine = Services.search.currentEngine.name;

    // If the current engine isn't in the list any more, select the first item.
    let engines = gEngineView._engineStore._engines;
    if (!engines.some(e => e.name == currentEngine))
      currentEngine = engines[0].name;

    // Now clean-up and rebuild the list.
    list.removeAllItems();
    gEngineView._engineStore._engines.forEach(e => {
      let item = list.appendItem(e.name);
      item.setAttribute("class", "menuitem-iconic searchengine-menuitem menuitem-with-favicon");
      if (e.iconURI) {
        let uri = PlacesUtils.getImageURLForResolution(window, e.iconURI.spec);
        item.setAttribute("image", uri);
      }
      item.engine = e;
      if (e.name == currentEngine)
        list.selectedItem = item;
    });
  },

  observe: function(aEngine, aTopic, aVerb) {
    if (aTopic == "browser-search-engine-modified") {
      aEngine.QueryInterface(Components.interfaces.nsISearchEngine);
      switch (aVerb) {
      case "engine-added":
        gEngineView._engineStore.addEngine(aEngine);
        gEngineView.rowCountChanged(gEngineView.lastIndex, 1);
        gSearchPane.buildDefaultEngineDropDown();
        break;
      case "engine-changed":
        gEngineView._engineStore.reloadIcons();
        gEngineView.invalidate();
        break;
      case "engine-removed":
      case "engine-current":
      case "engine-default":
        // Not relevant
        break;
      }
    }
  },

  onTreeSelect: function() {
    document.getElementById("removeEngineButton").disabled =
      gEngineView.selectedIndex == -1 || gEngineView.lastIndex == 0;
  },

  onTreeKeyPress: function(aEvent) {
    let index = gEngineView.selectedIndex;
    let tree = document.getElementById("engineList");
    if (tree.hasAttribute("editing"))
      return;

    if (aEvent.charCode == KeyEvent.DOM_VK_SPACE) {
      // Space toggles the checkbox.
      let newValue = !gEngineView._engineStore.engines[index].shown;
      gEngineView.setCellValue(index, tree.columns.getFirstColumn(),
                               newValue.toString());
    }
    else {
      let isMac = Services.appinfo.OS == "Darwin";
      if ((isMac && aEvent.keyCode == KeyEvent.DOM_VK_RETURN) ||
          (!isMac && aEvent.keyCode == KeyEvent.DOM_VK_F2))
        tree.startEditing(index, tree.columns.getLastColumn());
    }
  },

  onRestoreDefaults: function() {
    let num = gEngineView._engineStore.restoreDefaultEngines();
    gEngineView.rowCountChanged(0, num);
    gEngineView.invalidate();
  },

  showRestoreDefaults: function(aEnable) {
    document.getElementById("restoreDefaultSearchEngines").disabled = !aEnable;
  },

  remove: function() {
    gEngineView._engineStore.removeEngine(gEngineView.selectedEngine);
    let index = gEngineView.selectedIndex;
    gEngineView.rowCountChanged(index, -1);
    gEngineView.invalidate();
    gEngineView.selection.select(Math.min(index, gEngineView.lastIndex));
    gEngineView.ensureRowIsVisible(gEngineView.currentIndex);
    document.getElementById("engineList").focus();
  },

  editKeyword: Task.async(function* (aEngine, aNewKeyword) {
    if (aNewKeyword) {
      let eduplicate = false;
      let dupName = "";

      // Check for duplicates in Places keywords.
      let bduplicate = !!(yield PlacesUtils.keywords.fetch(aNewKeyword));

      // Check for duplicates in changes we haven't committed yet
      let engines = gEngineView._engineStore.engines;
      for each (let engine in engines) {
        if (engine.alias == aNewKeyword &&
            engine.name != aEngine.name) {
          eduplicate = true;
          dupName = engine.name;
          break;
        }
      }

      // Notify the user if they have chosen an existing engine/bookmark keyword
      if (eduplicate || bduplicate) {
        let strings = document.getElementById("engineManagerBundle");
        let dtitle = strings.getString("duplicateTitle");
        let bmsg = strings.getString("duplicateBookmarkMsg");
        let emsg = strings.getFormattedString("duplicateEngineMsg", [dupName]);

        Services.prompt.alert(window, dtitle, eduplicate ? emsg : bmsg);
        return false;
      }
    }

    gEngineView._engineStore.changeEngine(aEngine, "alias", aNewKeyword);
    gEngineView.invalidate();
    return true;
  }),

  saveOneClickEnginesList: function () {
    let hiddenList = [];
    for (let engine of gEngineView._engineStore.engines) {
      if (!engine.shown)
        hiddenList.push(engine.name);
    }
    document.getElementById("browser.search.hiddenOneOffs").value =
      hiddenList.join(",");
  },

  setDefaultEngine: function () {
    if (document.documentElement.instantApply) {
      Services.search.currentEngine =
        document.getElementById("defaultEngine").selectedItem.engine;
    }
  },

  loadAddEngines: function () {
    window.opener.BrowserSearch.loadAddEngines();
    window.document.documentElement.acceptDialog();
  }
};

function onDragEngineStart(event) {
  var selectedIndex = gEngineView.selectedIndex;
  var tree = document.getElementById("engineList");
  var row = { }, col = { }, child = { };
  tree.treeBoxObject.getCellAt(event.clientX, event.clientY, row, col, child);
  if (selectedIndex >= 0 && !gEngineView.isCheckBox(row.value, col.value)) {
    event.dataTransfer.setData(ENGINE_FLAVOR, selectedIndex.toString());
    event.dataTransfer.effectAllowed = "move";
  }
}

// "Operation" objects
function EngineMoveOp(aEngineClone, aNewIndex) {
  if (!aEngineClone)
    throw new Error("bad args to new EngineMoveOp!");
  this._engine = aEngineClone.originalEngine;
  this._newIndex = aNewIndex;
}
EngineMoveOp.prototype = {
  _engine: null,
  _newIndex: null,
  commit: function EMO_commit() {
    Services.search.moveEngine(this._engine, this._newIndex);
  }
};

function EngineRemoveOp(aEngineClone) {
  if (!aEngineClone)
    throw new Error("bad args to new EngineRemoveOp!");
  this._engine = aEngineClone.originalEngine;
}
EngineRemoveOp.prototype = {
  _engine: null,
  commit: function ERO_commit() {
    Services.search.removeEngine(this._engine);
  }
};

function EngineUnhideOp(aEngineClone, aNewIndex) {
  if (!aEngineClone)
    throw new Error("bad args to new EngineUnhideOp!");
  this._engine = aEngineClone.originalEngine;
  this._newIndex = aNewIndex;
}
EngineUnhideOp.prototype = {
  _engine: null,
  _newIndex: null,
  commit: function EUO_commit() {
    this._engine.hidden = false;
    Services.search.moveEngine(this._engine, this._newIndex);
  }
};

function EngineChangeOp(aEngineClone, aProp, aValue) {
  if (!aEngineClone)
    throw new Error("bad args to new EngineChangeOp!");

  this._engine = aEngineClone.originalEngine;
  this._prop = aProp;
  this._newValue = aValue;
}
EngineChangeOp.prototype = {
  _engine: null,
  _prop: null,
  _newValue: null,
  commit: function ECO_commit() {
    this._engine[this._prop] = this._newValue;
  }
};

function EngineStore() {
  let pref = document.getElementById("browser.search.hiddenOneOffs").value;
  this.hiddenList = pref ? pref.split(",") : [];

  this._engines = Services.search.getVisibleEngines().map(this._cloneEngine, this);
  this._defaultEngines = Services.search.getDefaultEngines().map(this._cloneEngine, this);

  if (document.documentElement.instantApply) {
    this._ops = {
      push: function(op) { op.commit(); }
    };
  }
  else {
    this._ops = [];
    document.documentElement.addEventListener("beforeaccept", () => {
      gEngineView._engineStore.commit();
    });
  }

  // check if we need to disable the restore defaults button
  var someHidden = this._defaultEngines.some(function (e) e.hidden);
  gSearchPane.showRestoreDefaults(someHidden);
}
EngineStore.prototype = {
  _engines: null,
  _defaultEngines: null,
  _ops: null,

  get engines() {
    return this._engines;
  },
  set engines(val) {
    this._engines = val;
    return val;
  },

  _getIndexForEngine: function ES_getIndexForEngine(aEngine) {
    return this._engines.indexOf(aEngine);
  },

  _getEngineByName: function ES_getEngineByName(aName) {
    for each (var engine in this._engines)
      if (engine.name == aName)
        return engine;

    return null;
  },

  _cloneEngine: function ES_cloneEngine(aEngine) {
    var clonedObj={};
    for (var i in aEngine)
      clonedObj[i] = aEngine[i];
    clonedObj.originalEngine = aEngine;
    clonedObj.shown = this.hiddenList.indexOf(clonedObj.name) == -1;
    return clonedObj;
  },

  // Callback for Array's some(). A thisObj must be passed to some()
  _isSameEngine: function ES_isSameEngine(aEngineClone) {
    return aEngineClone.originalEngine == this.originalEngine;
  },

  commit: function ES_commit() {
    for (op of this._ops)
      op.commit();

    Services.search.currentEngine =
      document.getElementById("defaultEngine").selectedItem.engine;
  },

  addEngine: function ES_addEngine(aEngine) {
    this._engines.push(this._cloneEngine(aEngine));
  },

  moveEngine: function ES_moveEngine(aEngine, aNewIndex) {
    if (aNewIndex < 0 || aNewIndex > this._engines.length - 1)
      throw new Error("ES_moveEngine: invalid aNewIndex!");
    var index = this._getIndexForEngine(aEngine);
    if (index == -1)
      throw new Error("ES_moveEngine: invalid engine?");

    if (index == aNewIndex)
      return; // nothing to do

    // Move the engine in our internal store
    var removedEngine = this._engines.splice(index, 1)[0];
    this._engines.splice(aNewIndex, 0, removedEngine);

    this._ops.push(new EngineMoveOp(aEngine, aNewIndex));
  },

  removeEngine: function ES_removeEngine(aEngine) {
    var index = this._getIndexForEngine(aEngine);
    if (index == -1)
      throw new Error("invalid engine?");

    this._engines.splice(index, 1);
    this._ops.push(new EngineRemoveOp(aEngine));
    if (this._defaultEngines.some(this._isSameEngine, aEngine))
      gSearchPane.showRestoreDefaults(true);
    gSearchPane.buildDefaultEngineDropDown();
  },

  restoreDefaultEngines: function ES_restoreDefaultEngines() {
    var added = 0;

    for (var i = 0; i < this._defaultEngines.length; ++i) {
      var e = this._defaultEngines[i];

      // If the engine is already in the list, just move it.
      if (this._engines.some(this._isSameEngine, e)) {
        this.moveEngine(this._getEngineByName(e.name), i);
      } else {
        // Otherwise, add it back to our internal store

        // The search service removes the alias when an engine is hidden,
        // so clear any alias we may have cached before unhiding the engine.
        e.alias = "";

        this._engines.splice(i, 0, e);
        this._ops.push(new EngineUnhideOp(e, i));
        added++;
      }
    }
    gSearchPane.showRestoreDefaults(false);
    gSearchPane.buildDefaultEngineDropDown();
    return added;
  },

  changeEngine: function ES_changeEngine(aEngine, aProp, aNewValue) {
    var index = this._getIndexForEngine(aEngine);
    if (index == -1)
      throw new Error("invalid engine?");

    this._engines[index][aProp] = aNewValue;
    this._ops.push(new EngineChangeOp(aEngine, aProp, aNewValue));
  },

  reloadIcons: function ES_reloadIcons() {
    this._engines.forEach(function (e) {
      e.uri = e.originalEngine.uri;
    });
  }
};

function EngineView(aEngineStore) {
  this._engineStore = aEngineStore;
}
EngineView.prototype = {
  _engineStore: null,
  tree: null,

  get lastIndex() {
    return this.rowCount - 1;
  },
  get selectedIndex() {
    var seln = this.selection;
    if (seln.getRangeCount() > 0) {
      var min = {};
      seln.getRangeAt(0, min, {});
      return min.value;
    }
    return -1;
  },
  get selectedEngine() {
    return this._engineStore.engines[this.selectedIndex];
  },

  // Helpers
  rowCountChanged: function (index, count) {
    this.tree.rowCountChanged(index, count);
  },

  invalidate: function () {
    this.tree.invalidate();
  },

  ensureRowIsVisible: function (index) {
    this.tree.ensureRowIsVisible(index);
  },

  getSourceIndexFromDrag: function (dataTransfer) {
    return parseInt(dataTransfer.getData(ENGINE_FLAVOR));
  },

  isCheckBox: function(index, column) {
    return column.id == "engineShown";
  },

  // nsITreeView
  get rowCount() {
    return this._engineStore.engines.length;
  },

  getImageSrc: function(index, column) {
    if (column.id == "engineName" && this._engineStore.engines[index].iconURI) {
      let uri = this._engineStore.engines[index].iconURI.spec;
      return PlacesUtils.getImageURLForResolution(window, uri);
    }
    return "";
  },

  getCellText: function(index, column) {
    if (column.id == "engineName")
      return this._engineStore.engines[index].name;
    else if (column.id == "engineKeyword")
      return this._engineStore.engines[index].alias;
    return "";
  },

  setTree: function(tree) {
    this.tree = tree;
  },

  canDrop: function(targetIndex, orientation, dataTransfer) {
    var sourceIndex = this.getSourceIndexFromDrag(dataTransfer);
    return (sourceIndex != -1 &&
            sourceIndex != targetIndex &&
            sourceIndex != targetIndex + orientation);
  },

  drop: function(dropIndex, orientation, dataTransfer) {
    var sourceIndex = this.getSourceIndexFromDrag(dataTransfer);
    var sourceEngine = this._engineStore.engines[sourceIndex];

    const nsITreeView = Components.interfaces.nsITreeView;
    if (dropIndex > sourceIndex) {
      if (orientation == nsITreeView.DROP_BEFORE)
        dropIndex--;
    } else {
      if (orientation == nsITreeView.DROP_AFTER)
        dropIndex++;
    }

    this._engineStore.moveEngine(sourceEngine, dropIndex);
    gSearchPane.showRestoreDefaults(true);
    gSearchPane.buildDefaultEngineDropDown();

    // Redraw, and adjust selection
    this.invalidate();
    this.selection.select(dropIndex);
  },

  selection: null,
  getRowProperties: function(index) { return ""; },
  getCellProperties: function(index, column) { return ""; },
  getColumnProperties: function(column) { return ""; },
  isContainer: function(index) { return false; },
  isContainerOpen: function(index) { return false; },
  isContainerEmpty: function(index) { return false; },
  isSeparator: function(index) { return false; },
  isSorted: function(index) { return false; },
  getParentIndex: function(index) { return -1; },
  hasNextSibling: function(parentIndex, index) { return false; },
  getLevel: function(index) { return 0; },
  getProgressMode: function(index, column) { },
  getCellValue: function(index, column) {
    if (column.id == "engineShown")
      return this._engineStore.engines[index].shown;
    return undefined;
  },
  toggleOpenState: function(index) { },
  cycleHeader: function(column) { },
  selectionChanged: function() { },
  cycleCell: function(row, column) { },
  isEditable: function(index, column) { return column.id != "engineName"; },
  isSelectable: function(index, column) { return false; },
  setCellValue: function(index, column, value) {
    if (column.id == "engineShown") {
      this._engineStore.engines[index].shown = value == "true";
      gEngineView.invalidate();
      gSearchPane.saveOneClickEnginesList();
    }
  },
  setCellText: function(index, column, value) {
    if (column.id == "engineKeyword") {
      gSearchPane.editKeyword(this._engineStore.engines[index], value)
                 .then(valid => {
        if (!valid)
          document.getElementById("engineList").startEditing(index, column);
      });
    }
  },
  performAction: function(action) { },
  performActionOnRow: function(action, index) { },
  performActionOnCell: function(action, index, column) { }
};
