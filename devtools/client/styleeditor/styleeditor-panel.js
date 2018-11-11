/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var Services = require("Services");
var promise = require("promise");
var {Task} = require("devtools/shared/task");
var {XPCOMUtils} = require("resource://gre/modules/XPCOMUtils.jsm");
var EventEmitter = require("devtools/shared/event-emitter");

var {StyleEditorUI} = require("resource://devtools/client/styleeditor/StyleEditorUI.jsm");
var {getString} = require("resource://devtools/client/styleeditor/StyleEditorUtil.jsm");
var {initCssProperties} = require("devtools/shared/fronts/css-properties");

loader.lazyGetter(this, "StyleSheetsFront",
  () => require("devtools/shared/fronts/stylesheets").StyleSheetsFront);

loader.lazyGetter(this, "StyleEditorFront",
  () => require("devtools/shared/fronts/styleeditor").StyleEditorFront);

var StyleEditorPanel = function StyleEditorPanel(panelWin, toolbox) {
  EventEmitter.decorate(this);

  this._toolbox = toolbox;
  this._target = toolbox.target;
  this._panelWin = panelWin;
  this._panelDoc = panelWin.document;

  this.destroy = this.destroy.bind(this);
  this._showError = this._showError.bind(this);
};

exports.StyleEditorPanel = StyleEditorPanel;

StyleEditorPanel.prototype = {
  get target() {
    return this._toolbox.target;
  },

  get panelWindow() {
    return this._panelWin;
  },

  /**
   * open is effectively an asynchronous constructor
   */
  open: Task.async(function* () {
    // We always interact with the target as if it were remote
    if (!this.target.isRemote) {
      yield this.target.makeRemote();
    }

    this.target.on("close", this.destroy);

    if (this.target.form.styleSheetsActor) {
      this._debuggee = StyleSheetsFront(this.target.client, this.target.form);
    } else {
      /* We're talking to a pre-Firefox 29 server-side */
      this._debuggee = StyleEditorFront(this.target.client, this.target.form);
    }

    // Initialize the CSS properties database.
    const {cssProperties} = yield initCssProperties(this._toolbox);

    // Initialize the UI
    this.UI = new StyleEditorUI(this._debuggee, this.target, this._panelDoc,
                                cssProperties);
    this.UI.on("error", this._showError);
    yield this.UI.initialize();

    this.isReady = true;

    return this;
  }),

  /**
   * Show an error message from the style editor in the toolbox
   * notification box.
   *
   * @param  {string} event
   *         Type of event
   * @param  {string} data
   *         The parameters to customize the error message
   */
  _showError: function (event, data) {
    if (!this._toolbox) {
      // could get an async error after we've been destroyed
      return;
    }

    let errorMessage = getString(data.key);
    if (data.append) {
      errorMessage += " " + data.append;
    }

    let notificationBox = this._toolbox.getNotificationBox();
    let notification =
        notificationBox.getNotificationWithValue("styleeditor-error");
    let level = (data.level === "info") ?
                notificationBox.PRIORITY_INFO_LOW :
                notificationBox.PRIORITY_CRITICAL_LOW;

    if (!notification) {
      notificationBox.appendNotification(errorMessage, "styleeditor-error",
                                         "", level);
    }
  },

  /**
   * Select a stylesheet.
   *
   * @param {string} href
   *        Url of stylesheet to find and select in editor
   * @param {number} line
   *        Line number to jump to after selecting. One-indexed
   * @param {number} col
   *        Column number to jump to after selecting. One-indexed
   * @return {Promise}
   *         Promise that will resolve when the editor is selected and ready
   *         to be used.
   */
  selectStyleSheet: function (href, line, col) {
    if (!this._debuggee || !this.UI) {
      return null;
    }
    return this.UI.selectStyleSheet(href, line - 1, col ? col - 1 : 0);
  },

  /**
   * Destroy the style editor.
   */
  destroy: function () {
    if (!this._destroyed) {
      this._destroyed = true;

      this._target.off("close", this.destroy);
      this._target = null;
      this._toolbox = null;
      this._panelWin = null;
      this._panelDoc = null;
      this._debuggee.destroy();
      this._debuggee = null;

      this.UI.destroy();
      this.UI = null;
    }

    return promise.resolve(null);
  },
};

XPCOMUtils.defineLazyGetter(StyleEditorPanel.prototype, "strings",
  function () {
    return Services.strings.createBundle(
            "chrome://devtools/locale/styleeditor.properties");
  });
