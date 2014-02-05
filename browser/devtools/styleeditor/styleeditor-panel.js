/* -*- Mode: Javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const {Cc, Ci, Cu, Cr} = require("chrome");

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

let promise = require("sdk/core/promise");
let EventEmitter = require("devtools/shared/event-emitter");

Cu.import("resource:///modules/devtools/StyleEditorUI.jsm");
Cu.import("resource:///modules/devtools/StyleEditorUtil.jsm");

loader.lazyGetter(this, "StyleSheetsFront",
  () => require("devtools/server/actors/stylesheets").StyleSheetsFront);

loader.lazyGetter(this, "StyleEditorFront",
  () => require("devtools/server/actors/styleeditor").StyleEditorFront);

this.StyleEditorPanel = function StyleEditorPanel(panelWin, toolbox) {
  EventEmitter.decorate(this);

  this._toolbox = toolbox;
  this._target = toolbox.target;
  this._panelWin = panelWin;
  this._panelDoc = panelWin.document;

  this.destroy = this.destroy.bind(this);
  this._showError = this._showError.bind(this);
}

exports.StyleEditorPanel = StyleEditorPanel;

StyleEditorPanel.prototype = {
  get target() this._toolbox.target,

  get panelWindow() this._panelWin,

  /**
   * open is effectively an asynchronous constructor
   */
  open: function() {
    let deferred = promise.defer();

    let targetPromise;
    // We always interact with the target as if it were remote
    if (!this.target.isRemote) {
      targetPromise = this.target.makeRemote();
    } else {
      targetPromise = promise.resolve(this.target);
    }

    targetPromise.then(() => {
      this.target.on("close", this.destroy);

      if (this.target.form.styleSheetsActor) {
        this._debuggee = StyleSheetsFront(this.target.client, this.target.form);
      }
      else {
        /* We're talking to a pre-Firefox 29 server-side */
        this._debuggee = StyleEditorFront(this.target.client, this.target.form);
      }
      this.UI = new StyleEditorUI(this._debuggee, this.target, this._panelDoc);
      this.UI.initialize().then(() => {
        this.UI.on("error", this._showError);

        this.isReady = true;

        deferred.resolve(this);
      });
    }, console.error);

    return deferred.promise;
  },

  /**
   * Show an error message from the style editor in the toolbox
   * notification box.
   *
   * @param  {string} event
   *         Type of event
   * @param  {string} code
   *         Error code of error to report
   * @param  {string} message
   *         Extra message to append to error message
   */
  _showError: function(event, code, message) {
    if (!this._toolbox) {
      // could get an async error after we've been destroyed
      return;
    }

    let errorMessage = _(code);
    if (message) {
      errorMessage += " " + message;
    }

    let notificationBox = this._toolbox.getNotificationBox();
    let notification = notificationBox.getNotificationWithValue("styleeditor-error");
    if (!notification) {
      notificationBox.appendNotification(errorMessage,
        "styleeditor-error", "", notificationBox.PRIORITY_CRITICAL_LOW);
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
   */
  selectStyleSheet: function(href, line, col) {
    if (!this._debuggee || !this.UI) {
      return;
    }
    this.UI.selectStyleSheet(href, line - 1, col ? col - 1 : 0);
  },

  /**
   * Destroy the style editor.
   */
  destroy: function() {
    if (!this._destroyed) {
      this._destroyed = true;

      this._target.off("close", this.destroy);
      this._target = null;
      this._toolbox = null;
      this._panelDoc = null;

      this.UI.destroy();
    }

    return promise.resolve(null);
  },
}

XPCOMUtils.defineLazyGetter(StyleEditorPanel.prototype, "strings",
  function () {
    return Services.strings.createBundle(
            "chrome://browser/locale/devtools/styleeditor.properties");
  });
