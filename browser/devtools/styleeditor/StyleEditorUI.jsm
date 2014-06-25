/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["StyleEditorUI"];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import("resource://gre/modules/osfile.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/devtools/event-emitter.js");
Cu.import("resource:///modules/devtools/gDevTools.jsm");
Cu.import("resource:///modules/devtools/StyleEditorUtil.jsm");
Cu.import("resource:///modules/devtools/SplitView.jsm");
Cu.import("resource:///modules/devtools/StyleSheetEditor.jsm");
const { Promise: promise } = Cu.import("resource://gre/modules/Promise.jsm", {});

XPCOMUtils.defineLazyModuleGetter(this, "PluralForm",
                                  "resource://gre/modules/PluralForm.jsm");

const require = Cu.import("resource://gre/modules/devtools/Loader.jsm", {}).devtools.require;
const { PrefObserver, PREF_ORIG_SOURCES } = require("devtools/styleeditor/utils");
const csscoverage = require("devtools/server/actors/csscoverage");
const console = require("resource://gre/modules/devtools/Console.jsm").console;

const LOAD_ERROR = "error-load";
const STYLE_EDITOR_TEMPLATE = "stylesheet";
const PREF_MEDIA_SIDEBAR = "devtools.styleeditor.showMediaSidebar";
const PREF_SIDEBAR_WIDTH = "devtools.styleeditor.mediaSidebarWidth";
const PREF_NAV_WIDTH = "devtools.styleeditor.navSidebarWidth";

/**
 * StyleEditorUI is controls and builds the UI of the Style Editor, including
 * maintaining a list of editors for each stylesheet on a debuggee.
 *
 * Emits events:
 *   'editor-added': A new editor was added to the UI
 *   'editor-selected': An editor was selected
 *   'error': An error occured
 *
 * @param {StyleEditorFront} debuggee
 *        Client-side front for interacting with the page's stylesheets
 * @param {Target} target
 *        Interface for the page we're debugging
 * @param {Document} panelDoc
 *        Document of the toolbox panel to populate UI in.
 */
function StyleEditorUI(debuggee, target, panelDoc) {
  EventEmitter.decorate(this);

  this._debuggee = debuggee;
  this._target = target;
  this._panelDoc = panelDoc;
  this._window = this._panelDoc.defaultView;
  this._root = this._panelDoc.getElementById("style-editor-chrome");

  this.editors = [];
  this.selectedEditor = null;
  this.savedLocations = {};

  this._onOptionsPopupShowing = this._onOptionsPopupShowing.bind(this);
  this._onOptionsPopupHiding = this._onOptionsPopupHiding.bind(this);
  this._onStyleSheetCreated = this._onStyleSheetCreated.bind(this);
  this._onNewDocument = this._onNewDocument.bind(this);
  this._onMediaPrefChanged = this._onMediaPrefChanged.bind(this);
  this._updateMediaList = this._updateMediaList.bind(this);
  this._clear = this._clear.bind(this);
  this._onError = this._onError.bind(this);

  this._prefObserver = new PrefObserver("devtools.styleeditor.");
  this._prefObserver.on(PREF_ORIG_SOURCES, this._onNewDocument);
  this._prefObserver.on(PREF_MEDIA_SIDEBAR, this._onMediaPrefChanged);
}

StyleEditorUI.prototype = {
  /**
   * Get whether any of the editors have unsaved changes.
   *
   * @return boolean
   */
  get isDirty() {
    if (this._markedDirty === true) {
      return true;
    }
    return this.editors.some((editor) => {
      return editor.sourceEditor && !editor.sourceEditor.isClean();
    });
  },

  /*
   * Mark the style editor as having or not having unsaved changes.
   */
  set isDirty(value) {
    this._markedDirty = value;
  },

  /*
   * Index of selected stylesheet in document.styleSheets
   */
  get selectedStyleSheetIndex() {
    return this.selectedEditor ?
           this.selectedEditor.styleSheet.styleSheetIndex : -1;
  },

  /**
   * Initiates the style editor ui creation and the inspector front to get
   * reference to the walker.
   */
  initialize: function() {
    let toolbox = gDevTools.getToolbox(this._target);
    return toolbox.initInspector().then(() => {
      this._walker = toolbox.walker;
    }).then(() => {
      this.createUI();
      this._debuggee.getStyleSheets().then((styleSheets) => {
        this._resetStyleSheetList(styleSheets);

        this._target.on("will-navigate", this._clear);
        this._target.on("navigate", this._onNewDocument);
      });
    });
  },

  /**
   * Build the initial UI and wire buttons with event handlers.
   */
  createUI: function() {
    let viewRoot = this._root.parentNode.querySelector(".splitview-root");

    this._view = new SplitView(viewRoot);

    wire(this._view.rootElement, ".style-editor-newButton", () =>{
      this._debuggee.addStyleSheet(null).then(this._onStyleSheetCreated);
    });

    wire(this._view.rootElement, ".style-editor-importButton", ()=> {
      this._importFromFile(this._mockImportFile || null, this._window);
    });

    this._optionsButton = this._panelDoc.getElementById("style-editor-options");
    this._optionsMenu = this._panelDoc.getElementById("style-editor-options-popup");
    this._optionsMenu.addEventListener("popupshowing",
                                       this._onOptionsPopupShowing);
    this._optionsMenu.addEventListener("popuphiding",
                                       this._onOptionsPopupHiding);

    this._sourcesItem = this._panelDoc.getElementById("options-origsources");
    this._sourcesItem.addEventListener("command",
                                       this._toggleOrigSources);
    this._mediaItem = this._panelDoc.getElementById("options-show-media");
    this._mediaItem.addEventListener("command",
                                     this._toggleMediaSidebar);

    let nav = this._panelDoc.querySelector(".splitview-controller");
    nav.setAttribute("width", Services.prefs.getIntPref(PREF_NAV_WIDTH));
  },

  /**
   * Listener handling the 'gear menu' popup showing event.
   * Update options menu items to reflect current preference settings.
   */
  _onOptionsPopupShowing: function() {
    this._optionsButton.setAttribute("open", "true");
    this._sourcesItem.setAttribute("checked",
      Services.prefs.getBoolPref(PREF_ORIG_SOURCES));
    this._mediaItem.setAttribute("checked",
      Services.prefs.getBoolPref(PREF_MEDIA_SIDEBAR));
  },

  /**
   * Listener handling the 'gear menu' popup hiding event.
   */
  _onOptionsPopupHiding: function() {
    this._optionsButton.removeAttribute("open");
  },

  /**
   * Refresh editors to reflect the stylesheets in the document.
   *
   * @param {string} event
   *        Event name
   * @param {StyleSheet} styleSheet
   *        StyleSheet object for new sheet
   */
  _onNewDocument: function() {
    this._debuggee.getStyleSheets().then((styleSheets) => {
      this._resetStyleSheetList(styleSheets);
    })
  },

  /**
   * Add editors for all the given stylesheets to the UI.
   *
   * @param  {array} styleSheets
   *         Array of StyleSheetFront
   */
  _resetStyleSheetList: function(styleSheets) {
    this._clear();

    for (let sheet of styleSheets) {
      this._addStyleSheet(sheet);
    }

    this._root.classList.remove("loading");

    this.emit("stylesheets-reset");
  },

  /**
   * Remove all editors and add loading indicator.
   */
  _clear: function() {
    // remember selected sheet and line number for next load
    if (this.selectedEditor && this.selectedEditor.sourceEditor) {
      let href = this.selectedEditor.styleSheet.href;
      let {line, ch} = this.selectedEditor.sourceEditor.getCursor();

      this._styleSheetToSelect = {
        stylesheet: href,
        line: line,
        col: ch
      };
    }

    // remember saved file locations
    for (let editor of this.editors) {
      if (editor.savedFile) {
        let identifier = this.getStyleSheetIdentifier(editor.styleSheet);
        this.savedLocations[identifier] = editor.savedFile;
      }
    }

    this._clearStyleSheetEditors();
    this._view.removeAll();

    this.selectedEditor = null;

    this._root.classList.add("loading");
  },

  /**
   * Add an editor for this stylesheet. Add editors for its original sources
   * instead (e.g. Sass sources), if applicable.
   *
   * @param  {StyleSheetFront} styleSheet
   *         Style sheet to add to style editor
   */
  _addStyleSheet: function(styleSheet) {
    let editor = this._addStyleSheetEditor(styleSheet);

    if (!Services.prefs.getBoolPref(PREF_ORIG_SOURCES)) {
      return;
    }

    styleSheet.getOriginalSources().then((sources) => {
      if (sources && sources.length) {
        this._removeStyleSheetEditor(editor);
        sources.forEach((source) => {
          // set so the first sheet will be selected, even if it's a source
          source.styleSheetIndex = styleSheet.styleSheetIndex;
          source.relatedStyleSheet = styleSheet;

          this._addStyleSheetEditor(source);
        });
      }
    });
  },

  /**
   * Add a new editor to the UI for a source.
   *
   * @param {StyleSheet}  styleSheet
   *        Object representing stylesheet
   * @param {nsIfile}  file
   *         Optional file object that sheet was imported from
   * @param {Boolean} isNew
   *         Optional if stylesheet is a new sheet created by user
   */
  _addStyleSheetEditor: function(styleSheet, file, isNew) {
    // recall location of saved file for this sheet after page reload
    let identifier = this.getStyleSheetIdentifier(styleSheet);
    let savedFile = this.savedLocations[identifier];
    if (savedFile && !file) {
      file = savedFile;
    }

    let editor =
      new StyleSheetEditor(styleSheet, this._window, file, isNew, this._walker);

    editor.on("property-change", this._summaryChange.bind(this, editor));
    editor.on("media-rules-changed", this._updateMediaList.bind(this, editor));
    editor.on("linked-css-file", this._summaryChange.bind(this, editor));
    editor.on("linked-css-file-error", this._summaryChange.bind(this, editor));
    editor.on("error", this._onError);

    this.editors.push(editor);

    editor.fetchSource(this._sourceLoaded.bind(this, editor));
    return editor;
  },

  /**
   * Import a style sheet from file and asynchronously create a
   * new stylesheet on the debuggee for it.
   *
   * @param {mixed} file
   *        Optional nsIFile or filename string.
   *        If not set a file picker will be shown.
   * @param {nsIWindow} parentWindow
   *        Optional parent window for the file picker.
   */
  _importFromFile: function(file, parentWindow) {
    let onFileSelected = (file) => {
      if (!file) {
        // nothing selected
        return;
      }
      NetUtil.asyncFetch(file, (stream, status) => {
        if (!Components.isSuccessCode(status)) {
          this.emit("error", { key: LOAD_ERROR });
          return;
        }
        let source = NetUtil.readInputStreamToString(stream, stream.available());
        stream.close();

        this._debuggee.addStyleSheet(source).then((styleSheet) => {
          this._onStyleSheetCreated(styleSheet, file);
        });
      });

    };

    showFilePicker(file, false, parentWindow, onFileSelected);
  },


  /**
   * When a new or imported stylesheet has been added to the document.
   * Add an editor for it.
   */
  _onStyleSheetCreated: function(styleSheet, file) {
    this._addStyleSheetEditor(styleSheet, file, true);
  },

  /**
   * Forward any error from a stylesheet.
   *
   * @param  {string} event
   *         Event name
   * @param  {data} data
   *         The event data
   */
  _onError: function(event, data) {
    this.emit("error", data);
  },

  /**
   * Toggle the original sources pref.
   */
  _toggleOrigSources: function() {
    let isEnabled = Services.prefs.getBoolPref(PREF_ORIG_SOURCES);
    Services.prefs.setBoolPref(PREF_ORIG_SOURCES, !isEnabled);
  },

  /**
   * Toggle the pref for showing a @media rules sidebar in each editor.
   */
  _toggleMediaSidebar: function() {
    let isEnabled = Services.prefs.getBoolPref(PREF_MEDIA_SIDEBAR);
    Services.prefs.setBoolPref(PREF_MEDIA_SIDEBAR, !isEnabled);
  },

  /**
   * Toggle the @media sidebar in each editor depending on the setting.
   */
  _onMediaPrefChanged: function() {
    this.editors.forEach(this._updateMediaList);
  },

  /**
   * Remove a particular stylesheet editor from the UI
   *
   * @param {StyleSheetEditor}  editor
   *        The editor to remove.
   */
  _removeStyleSheetEditor: function(editor) {
    if (editor.summary) {
      this._view.removeItem(editor.summary);
    }
    else {
      let self = this;
      this.on("editor-added", function onAdd(event, added) {
        if (editor == added) {
          self.off("editor-added", onAdd);
          self._view.removeItem(editor.summary);
        }
      })
    }

    editor.destroy();
    this.editors.splice(this.editors.indexOf(editor), 1);
  },

  /**
   * Clear all the editors from the UI.
   */
  _clearStyleSheetEditors: function() {
    for (let editor of this.editors) {
      editor.destroy();
    }
    this.editors = [];
  },

  /**
   * Called when a StyleSheetEditor's source has been fetched. Create a
   * summary UI for the editor.
   *
   * @param  {StyleSheetEditor} editor
   *         Editor to create UI for.
   */
  _sourceLoaded: function(editor) {
    // add new sidebar item and editor to the UI
    this._view.appendTemplatedItem(STYLE_EDITOR_TEMPLATE, {
      data: {
        editor: editor
      },
      disableAnimations: this._alwaysDisableAnimations,
      ordinal: editor.styleSheet.styleSheetIndex,
      onCreate: function(summary, details, data) {
        let editor = data.editor;
        editor.summary = summary;
        editor.details = details;

        wire(summary, ".stylesheet-enabled", function onToggleDisabled(event) {
          event.stopPropagation();
          event.target.blur();

          editor.toggleDisabled();
        });

        wire(summary, ".stylesheet-name", {
          events: {
            "keypress": (aEvent) => {
              if (aEvent.keyCode == aEvent.DOM_VK_RETURN) {
                this._view.activeSummary = summary;
              }
            }
          }
        });

        wire(summary, ".stylesheet-saveButton", function onSaveButton(event) {
          event.stopPropagation();
          event.target.blur();

          editor.saveToFile(editor.savedFile);
        });

        this._updateSummaryForEditor(editor, summary);

        summary.addEventListener("focus", function onSummaryFocus(event) {
          if (event.target == summary) {
            // autofocus the stylesheet name
            summary.querySelector(".stylesheet-name").focus();
          }
        }, false);

        let sidebar = details.querySelector(".stylesheet-sidebar");
        sidebar.setAttribute("width",
            Services.prefs.getIntPref(PREF_SIDEBAR_WIDTH));

        let splitter = details.querySelector(".devtools-side-splitter");
        splitter.addEventListener("mousemove", () => {
          let sidebarWidth = sidebar.getAttribute("width");
          Services.prefs.setIntPref(PREF_SIDEBAR_WIDTH, sidebarWidth);

          // update all @media sidebars for consistency
          let sidebars = [...this._panelDoc.querySelectorAll(".stylesheet-sidebar")];
          for (let mediaSidebar of sidebars) {
            mediaSidebar.setAttribute("width", sidebarWidth);
          }
        });

        // autofocus if it's a new user-created stylesheet
        if (editor.isNew) {
          this._selectEditor(editor);
        }

        if (this._styleSheetToSelect
            && this._styleSheetToSelect.stylesheet == editor.styleSheet.href) {
          this.switchToSelectedSheet();
        }

        // If this is the first stylesheet and there is no pending request to
        // select a particular style sheet, select this sheet.
        if (!this.selectedEditor && !this._styleSheetBoundToSelect
            && editor.styleSheet.styleSheetIndex == 0) {
          this._selectEditor(editor);
        }
        this.emit("editor-added", editor);
      }.bind(this),

      onShow: function(summary, details, data) {
        let editor = data.editor;
        this.selectedEditor = editor;

        Task.spawn(function* () {
          if (!editor.sourceEditor) {
            // only initialize source editor when we switch to this view
            let inputElement = details.querySelector(".stylesheet-editor-input");
            yield editor.load(inputElement);
          }

          editor.onShow();

          this.emit("editor-selected", editor);

          // Is there any CSS coverage markup to include?
          csscoverage.getUsage(this._target).then(usage => {
            if (usage == null) {
              return;
            }

            let href = csscoverage.sheetToUrl(editor.styleSheet);
            usage.createEditorReport(href).then(data => {
              editor.removeAllUnusedRegions();

              if (data.reports.length > 0) {
                // So there is some coverage markup, but can we apply it?
                let text = editor.sourceEditor.getText() + "\r";
                // If the CSS text contains a '}' with some non-whitespace
                // after then we assume this is compressed CSS and stop
                // marking-up.
                if (!/}\s*\S+\s*\r/.test(text)) {
                  editor.addUnusedRegions(data.reports);
                }
                else {
                  this.emit("error", { key: "error-compressed", level: "info" });
                }
              }
            });
          }, console.error);
        }.bind(this)).then(null, Cu.reportError);
      }.bind(this)
    });
  },

  /**
   * Switch to the editor that has been marked to be selected.
   *
   * @return {Promise}
   *         Promise that will resolve when the editor is selected.
   */
  switchToSelectedSheet: function() {
    let sheet = this._styleSheetToSelect;
    let isHref = sheet.stylesheet === null || typeof sheet.stylesheet == "string";

    for (let editor of this.editors) {
      if ((isHref && editor.styleSheet.href == sheet.stylesheet) ||
          sheet.stylesheet == editor.styleSheet) {
        // The _styleSheetBoundToSelect will always hold the latest pending
        // requested style sheet (with line and column) which is not yet
        // selected by the source editor. Only after we select that particular
        // editor and go the required line and column, it will become null.
        this._styleSheetBoundToSelect = this._styleSheetToSelect;
        this._styleSheetToSelect = null;
        return this._selectEditor(editor, sheet.line, sheet.col);
      }
    }

    return promise.resolve();
  },

  /**
   * Select an editor in the UI.
   *
   * @param  {StyleSheetEditor} editor
   *         Editor to switch to.
   * @param  {number} line
   *         Line number to jump to
   * @param  {number} col
   *         Column number to jump to
   * @return {Promise}
   *         Promise that will resolve when the editor is selected.
   */
  _selectEditor: function(editor, line, col) {
    line = line || 0;
    col = col || 0;

    let editorPromise = editor.getSourceEditor().then(() => {
      editor.sourceEditor.setCursor({line: line, ch: col});
      this._styleSheetBoundToSelect = null;
    });

    let summaryPromise = this.getEditorSummary(editor).then((summary) => {
      this._view.activeSummary = summary;
    });

    return promise.all([editorPromise, summaryPromise]);
  },

  getEditorSummary: function(editor) {
    if (editor.summary) {
      return promise.resolve(editor.summary);
    }

    let deferred = promise.defer();
    let self = this;

    this.on("editor-added", function onAdd(e, selected) {
      if (selected == editor) {
        self.off("editor-added", onAdd);
        deferred.resolve(editor.summary);
      }
    });

    return deferred.promise;
  },

  getEditorDetails: function(editor) {
    if (editor.details) {
      return promise.resolve(editor.details);
    }

    let deferred = promise.defer();
    let self = this;

    this.on("editor-added", function onAdd(e, selected) {
      if (selected == editor) {
        self.off("editor-added", onAdd);
        deferred.resolve(editor.details);
      }
    });

    return deferred.promise;
  },

  /**
   * Returns an identifier for the given style sheet.
   *
   * @param {StyleSheet} aStyleSheet
   *        The style sheet to be identified.
   */
  getStyleSheetIdentifier: function (aStyleSheet) {
    // Identify inline style sheets by their host page URI and index at the page.
    return aStyleSheet.href ? aStyleSheet.href :
            "inline-" + aStyleSheet.styleSheetIndex + "-at-" + aStyleSheet.nodeHref;
  },

  /**
   * selects a stylesheet and optionally moves the cursor to a selected line
   *
   * @param {StyleSheetFront} [stylesheet]
   *        Stylesheet to select or href of stylesheet to select
   * @param {Number} [line]
   *        Line to which the caret should be moved (zero-indexed).
   * @param {Number} [col]
   *        Column to which the caret should be moved (zero-indexed).
   */
  selectStyleSheet: function(stylesheet, line, col) {
    this._styleSheetToSelect = {
      stylesheet: stylesheet,
      line: line,
      col: col,
    };

    /* Switch to the editor for this sheet, if it exists yet.
       Otherwise each editor will be checked when it's created. */
    this.switchToSelectedSheet();
  },


  /**
   * Handler for an editor's 'property-changed' event.
   * Update the summary in the UI.
   *
   * @param  {StyleSheetEditor} editor
   *         Editor for which a property has changed
   */
  _summaryChange: function(editor) {
    this._updateSummaryForEditor(editor);
  },

  /**
   * Update split view summary of given StyleEditor instance.
   *
   * @param {StyleSheetEditor} editor
   * @param {DOMElement} summary
   *        Optional item's summary element to update. If none, item corresponding
   *        to passed editor is used.
   */
  _updateSummaryForEditor: function(editor, summary) {
    summary = summary || editor.summary;
    if (!summary) {
      return;
    }

    let ruleCount = editor.styleSheet.ruleCount;
    if (editor.styleSheet.relatedStyleSheet && editor.linkedCSSFile) {
      ruleCount = editor.styleSheet.relatedStyleSheet.ruleCount;
    }
    if (ruleCount === undefined) {
      ruleCount = "-";
    }

    var flags = [];
    if (editor.styleSheet.disabled) {
      flags.push("disabled");
    }
    if (editor.unsaved) {
      flags.push("unsaved");
    }
    if (editor.linkedCSSFileError) {
      flags.push("linked-file-error");
    }
    this._view.setItemClassName(summary, flags.join(" "));

    let label = summary.querySelector(".stylesheet-name > label");
    label.setAttribute("value", editor.friendlyName);
    if (editor.styleSheet.href) {
      label.setAttribute("tooltiptext", editor.styleSheet.href);
    }

    let linkedCSSFile = "";
    if (editor.linkedCSSFile) {
      linkedCSSFile = OS.Path.basename(editor.linkedCSSFile);
    }
    text(summary, ".stylesheet-linked-file", linkedCSSFile);
    text(summary, ".stylesheet-title", editor.styleSheet.title || "");
    text(summary, ".stylesheet-rule-count",
      PluralForm.get(ruleCount, _("ruleCount.label")).replace("#1", ruleCount));
  },

  /**
   * Update the @media rules sidebar for an editor. Hide if there are no rules
   * Display a list of the @media rules in the editor's associated style sheet.
   * Emits a 'media-list-changed' event after updating the UI.
   *
   * @param  {StyleSheetEditor} editor
   *         Editor to update @media sidebar of
   */
  _updateMediaList: function(editor) {
    Task.spawn(function* () {
      let details = yield this.getEditorDetails(editor);
      let list = details.querySelector(".stylesheet-media-list");

      while (list.firstChild) {
        list.removeChild(list.firstChild);
      }

      let rules = editor.mediaRules;
      let showSidebar = Services.prefs.getBoolPref(PREF_MEDIA_SIDEBAR);
      let sidebar = details.querySelector(".stylesheet-sidebar");

      let inSource = false;

      for (let rule of rules) {
        let {line, column, parentStyleSheet} = rule;

        let location = {
          line: line,
          column: column,
          source: editor.styleSheet.href,
          styleSheet: parentStyleSheet
        };
        if (editor.styleSheet.isOriginalSource) {
          location = yield editor.cssSheet.getOriginalLocation(line, column);
        }

        // this @media rule is from a different original source
        if (location.source != editor.styleSheet.href) {
          continue;
        }
        inSource = true;

        let div = this._panelDoc.createElement("div");
        div.className = "media-rule-label";
        div.addEventListener("click", this._jumpToLocation.bind(this, location));

        let cond = this._panelDoc.createElement("div");
        cond.textContent = rule.conditionText;
        cond.className = "media-rule-condition"
        if (!rule.matches) {
          cond.classList.add("media-condition-unmatched");
        }
        div.appendChild(cond);

        let link = this._panelDoc.createElement("div");
        link.className = "media-rule-line theme-link";
        if (location.line != -1) {
          link.textContent = ":" + location.line;
        }
        div.appendChild(link);

        list.appendChild(div);
      }

      sidebar.hidden = !showSidebar || !inSource;

      this.emit("media-list-changed", editor);
    }.bind(this)).then(null, Cu.reportError);
  },

  /**
   * Jump cursor to the editor for a stylesheet and line number for a rule.
   *
   * @param  {object} location
   *         Location object with 'line', 'column', and 'source' properties.
   */
  _jumpToLocation: function(location) {
    let source = location.styleSheet || location.source;
    this.selectStyleSheet(source, location.line - 1, location.column - 1);
  },

  destroy: function() {
    this._clearStyleSheetEditors();

    let sidebar = this._panelDoc.querySelector(".splitview-controller");
    let sidebarWidth = sidebar.getAttribute("width");
    Services.prefs.setIntPref(PREF_NAV_WIDTH, sidebarWidth);

    this._optionsMenu.removeEventListener("popupshowing",
                                          this._onOptionsPopupShowing);
    this._optionsMenu.removeEventListener("popuphiding",
                                          this._onOptionsPopupHiding);

    this._prefObserver.off(PREF_ORIG_SOURCES, this._onNewDocument);
    this._prefObserver.off(PREF_MEDIA_SIDEBAR, this._onMediaPrefChanged);
    this._prefObserver.destroy();
  }
}
