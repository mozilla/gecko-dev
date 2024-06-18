/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

loader.lazyRequireGetter(
  this,
  "CssLogic",
  "resource://devtools/shared/inspector/css-logic.js"
);

class StyleSheetWatcher {
  constructor() {
    this._onApplicableStylesheetAdded =
      this._onApplicableStylesheetAdded.bind(this);
    this._onStylesheetUpdated = this._onStylesheetUpdated.bind(this);
    this._onStylesheetRemoved = this._onStylesheetRemoved.bind(this);
  }

  /**
   * Start watching for all stylesheets related to a given Target Actor.
   *
   * @param TargetActor targetActor
   *        The target actor from which we should observe css changes.
   * @param Object options
   *        Dictionary object with following attributes:
   *        - onAvailable: mandatory function
   *          This will be called for each resource.
   */
  async watch(targetActor, { onAvailable, onUpdated, onDestroyed }) {
    this._targetActor = targetActor;
    this._onAvailable = onAvailable;
    this._onUpdated = onUpdated;
    this._onDestroyed = onDestroyed;

    this._styleSheetsManager = targetActor.getStyleSheetsManager();

    // watch will call onAvailable for already existing stylesheets
    await this._styleSheetsManager.watch({
      onAvailable: this._onApplicableStylesheetAdded,
      onUpdated: this._onStylesheetUpdated,
      onDestroyed: this._onStylesheetRemoved,
    });
  }

  async _onApplicableStylesheetAdded(styleSheetData) {
    const { resourceId, styleSheet, creationData } = styleSheetData;
    const resource = await this._toResource(styleSheet, {
      resourceId,
      isCreatedByDevTools: creationData?.isCreatedByDevTools,
      fileName: creationData?.fileName,
    });

    this._onAvailable([resource]);
  }

  _onStylesheetUpdated({ resourceId, updateKind, updates = {} }) {
    const { resourceUpdates, nestedResourceUpdates, event } = updates;
    this._onUpdated([
      {
        browsingContextID: this._targetActor.browsingContextID,
        innerWindowId: this._targetActor.innerWindowId,
        resourceId,
        updateType: updateKind,
        resourceUpdates,
        nestedResourceUpdates,
        event,
      },
    ]);
  }

  _onStylesheetRemoved({ resourceId }) {
    return this._onDestroyed([resourceId]);
  }

  async _toResource(
    styleSheet,
    { isCreatedByDevTools = false, fileName = null, resourceId } = {}
  ) {
    const { atRules, ruleCount } =
      this._styleSheetsManager.getStyleSheetRuleCountAndAtRules(styleSheet);

    const resource = {
      resourceId,
      disabled: styleSheet.disabled,
      constructed: styleSheet.constructed,
      fileName,
      href: styleSheet.href,
      isNew: isCreatedByDevTools,
      atRules,
      nodeHref: this._styleSheetsManager.getNodeHref(styleSheet),
      ruleCount,
      sourceMapBaseURL:
        this._styleSheetsManager.getSourcemapBaseURL(styleSheet),
      sourceMapURL: styleSheet.sourceMapURL,
      styleSheetIndex: this._styleSheetsManager.getStyleSheetIndex(resourceId),
      system: CssLogic.isAgentStylesheet(styleSheet),
      title: styleSheet.title,
    };

    return resource;
  }

  destroy() {
    this._styleSheetsManager.unwatch({
      onAvailable: this._onApplicableStylesheetAdded,
      onUpdated: this._onStylesheetUpdated,
      onDestroyed: this._onStylesheetRemoved,
    });
  }
}

module.exports = StyleSheetWatcher;
