/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Contains elements of the Content Analysis UI, which are integrated into
 * various browser behaviors (uploading, downloading, printing, etc) that
 * require content analysis to be done.
 * The content analysis itself is done by the clients of this script, who
 * use nsIContentAnalysis to talk to the external CA system.
 */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gContentAnalysis",
  "@mozilla.org/contentanalysis;1",
  Ci.nsIContentAnalysis
);

ChromeUtils.defineESModuleGetters(lazy, {
  clearTimeout: "resource://gre/modules/Timer.sys.mjs",
  PanelMultiView: "resource:///modules/PanelMultiView.sys.mjs",
  setTimeout: "resource://gre/modules/Timer.sys.mjs",
});

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "silentNotifications",
  "browser.contentanalysis.silent_notifications",
  false
);

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "agentName",
  "browser.contentanalysis.agent_name",
  "A DLP agent"
);

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "showBlockedResult",
  "browser.contentanalysis.show_blocked_result",
  true
);

/**
 * A class that groups browsing contexts by their top-level one.
 * This is necessary because if there may be a subframe that
 * is showing a "DLP request busy" dialog when another subframe
 * (other than the outer frame) wants to show one. This is also needed
 * because there may be multiple requests active for a given top-level
 * or subframe.
 *
 * This class makes it convenient to find if another frame with
 * the same top browsing context is currently showing a dialog, and
 * also to find if there are any pending dialogs to show when one closes.
 */
class MapByTopBrowsingContext {
  /**
   * A map from top-level browsing context to
   *    a map from browsing context to a list of entries
   *
   * @type {Map<BrowsingContext, Map<BrowsingContext, Array<object>>>}
   */
  #map;
  constructor() {
    this.#map = new Map();
  }
  /**
   * Gets a specific request associated with the browsing context
   *
   * @param {BrowsingContext} aBrowsingContext the browsing context to search for
   * @param {string} aRequestToken the request token to search for
   * @returns {object | undefined} the existing data, or `undefined` if there is none
   */
  getAndRemoveEntry(aBrowsingContext, aRequestToken) {
    const topEntry = this.#map.get(aBrowsingContext.top);
    if (!topEntry) {
      return undefined;
    }
    const browsingContextEntries = topEntry.get(aBrowsingContext);
    if (!browsingContextEntries) {
      return undefined;
    }
    for (let i = 0; i < browsingContextEntries.length; i++) {
      if (browsingContextEntries[i].request.requestToken === aRequestToken) {
        // Remove and return this entry
        return browsingContextEntries.splice(i, 1)[0];
      }
    }
    return undefined;
  }

  /**
   * Adds or replaces the associated entry for the browsing context
   *
   * @param {BrowsingContext} aBrowsingContext the browsing context to set the data for
   * @param {object} aValue the data to associated with the browsing context
   * @returns {MapByTopBrowsingContext} this
   */
  addOrReplaceEntry(aBrowsingContext, aValue) {
    if (!aValue.request) {
      console.error(
        "MapByTopBrowsingContext.setEntry() called with a value without a request!"
      );
    }
    let topEntry = this.#map.get(aBrowsingContext.top);
    if (!topEntry) {
      topEntry = new Map();
      this.#map.set(aBrowsingContext.top, topEntry);
    }

    let existingEntries = topEntry.get(aBrowsingContext);
    if (existingEntries) {
      for (let i = 0; i < existingEntries.length; ++i) {
        let existingEntry = existingEntries[i];
        if (
          existingEntry.request.requestToken === aValue.request.requestToken
        ) {
          existingEntries[i] = aValue;
          return this;
        }
      }
      existingEntries.push(aValue);
    } else {
      topEntry.set(aBrowsingContext, [aValue]);
    }
    return this;
  }

  /**
   * Gets all requests across all browsing contexts
   *
   * @returns {Array<object>} all the requests
   */
  getAllRequests() {
    let requests = [];
    this.#map.forEach(topBrowsingContext => {
      for (const entries of topBrowsingContext.values()) {
        for (const entry of entries) {
          requests.push(entry.request);
        }
      }
    });
    return requests;
  }
}

export const ContentAnalysis = {
  _SHOW_NOTIFICATIONS: true,

  _SHOW_DIALOGS: false,

  _SLOW_DLP_NOTIFICATION_BLOCKING_TIMEOUT_MS: 250,

  _SLOW_DLP_NOTIFICATION_NONBLOCKING_TIMEOUT_MS: 3 * 1000,

  _RESULT_NOTIFICATION_TIMEOUT_MS: 5 * 60 * 1000, // 5 min

  _RESULT_NOTIFICATION_FAST_TIMEOUT_MS: 60 * 1000, // 1 min

  PROMPTID_PREFIX: "ContentAnalysisDialog-",

  isInitialized: false,

  dlpBusyViewsByTopBrowsingContext: new MapByTopBrowsingContext(),

  /**
   * @type {Map<string, {browsingContext: BrowsingContext, resourceNameOrOperationType: object}>}
   */
  requestTokenToRequestInfo: new Map(),

  /**
   * Registers for various messages/events that will indicate the
   * need for communicating something to the user.
   */
  initialize(window) {
    if (!lazy.gContentAnalysis.isActive) {
      return;
    }
    let doc = window.document;
    if (!this.isInitialized) {
      this.isInitialized = true;
      this.initializeDownloadCA();

      ChromeUtils.defineLazyGetter(this, "l10n", function () {
        return new Localization(
          ["branding/brand.ftl", "toolkit/contentanalysis/contentanalysis.ftl"],
          true
        );
      });
    }

    // Do this even if initialized so the icon shows up on new windows, not just the
    // first one.
    doc.l10n.setAttributes(
      doc.getElementById("content-analysis-indicator"),
      "content-analysis-indicator-tooltip",
      { agentName: lazy.agentName }
    );
    doc.documentElement.setAttribute("contentanalysisactive", "true");
  },

  async uninitialize() {
    if (this.isInitialized) {
      this.isInitialized = false;
      this.requestTokenToRequestInfo.clear();
    }
  },

  /**
   * Register UI for file download CA events.
   */
  async initializeDownloadCA() {
    Services.obs.addObserver(this, "dlp-request-made");
    Services.obs.addObserver(this, "dlp-response");
    Services.obs.addObserver(this, "quit-application");
    Services.obs.addObserver(this, "quit-application-requested");
  },

  // nsIObserver
  async observe(aSubj, aTopic, _aData) {
    switch (aTopic) {
      case "quit-application-requested": {
        let quitCancelled = false;
        let pendingRequests =
          this.dlpBusyViewsByTopBrowsingContext.getAllRequests();
        if (pendingRequests.length) {
          let messageBody = this.l10n.formatValueSync(
            "contentanalysis-inprogress-quit-message"
          );
          messageBody = messageBody + "\n\n";
          for (const pendingRequest of pendingRequests) {
            let name = this._getResourceNameFromNameOrOperationType(
              this._getResourceNameOrOperationTypeFromRequest(
                pendingRequest,
                true
              )
            );
            messageBody = messageBody + name + "\n";
          }
          let buttonSelected = Services.prompt.confirmEx(
            null,
            this.l10n.formatValueSync("contentanalysis-inprogress-quit-title"),
            messageBody,
            Ci.nsIPromptService.BUTTON_POS_0 *
              Ci.nsIPromptService.BUTTON_TITLE_IS_STRING +
              Ci.nsIPromptService.BUTTON_POS_1 *
                Ci.nsIPromptService.BUTTON_TITLE_CANCEL +
              Ci.nsIPromptService.BUTTON_POS_0_DEFAULT,
            this.l10n.formatValueSync(
              "contentanalysis-inprogress-quit-yesbutton"
            ),
            null,
            null,
            null,
            { value: 0 }
          );
          if (buttonSelected === 1) {
            aSubj.data = true;
            quitCancelled = true;
          }
        }
        if (!quitCancelled) {
          // Ideally we would wait until "quit-application" to cancel outstanding
          // DLP requests, but the "DLP busy" or "DLP blocked" dialog can block the
          // main thread, thus preventing the "quit-application" from being sent,
          // which causes a shutdownhang. (bug 1899703)
          lazy.gContentAnalysis.cancelAllRequests();
        }
        break;
      }
      case "quit-application": {
        this.uninitialize();
        break;
      }
      case "dlp-request-made":
        {
          const request = aSubj.QueryInterface(Ci.nsIContentAnalysisRequest);
          if (!request) {
            console.error(
              "Showing in-browser Content Analysis notification but no request was passed"
            );
            return;
          }
          const analysisType = request.analysisType;
          // For operations that block browser interaction, show the "slow content analysis"
          // dialog faster
          let slowTimeoutMs = this._shouldShowBlockingNotification(analysisType)
            ? this._SLOW_DLP_NOTIFICATION_BLOCKING_TIMEOUT_MS
            : this._SLOW_DLP_NOTIFICATION_NONBLOCKING_TIMEOUT_MS;
          let browsingContext = request.windowGlobalParent?.browsingContext;
          if (!browsingContext) {
            throw new Error(
              "Got dlp-request-made message but couldn't find a browsingContext!"
            );
          }

          // Start timer that, when it expires,
          // presents a "slow CA check" message.
          let resourceNameOrOperationType =
            this._getResourceNameOrOperationTypeFromRequest(request, false);
          this.requestTokenToRequestInfo.set(request.requestToken, {
            browsingContext,
            resourceNameOrOperationType,
          });
          this.dlpBusyViewsByTopBrowsingContext.addOrReplaceEntry(
            browsingContext,
            {
              timer: lazy.setTimeout(() => {
                this.dlpBusyViewsByTopBrowsingContext.addOrReplaceEntry(
                  browsingContext,
                  {
                    notification: this._showSlowCAMessage(
                      analysisType,
                      request,
                      resourceNameOrOperationType,
                      browsingContext
                    ),
                    request,
                  }
                );
              }, slowTimeoutMs),
              request,
            }
          );
        }
        break;
      case "dlp-response": {
        const request = aSubj.QueryInterface(Ci.nsIContentAnalysisResponse);
        // Cancels timer or slow message UI,
        // if present, and possibly presents the CA verdict.
        if (!request) {
          throw new Error("Got dlp-response message but no request was passed");
        }

        let windowAndResourceNameOrOperationType =
          this.requestTokenToRequestInfo.get(request.requestToken);
        if (!windowAndResourceNameOrOperationType) {
          // Perhaps this was cancelled just before the response came in from the
          // DLP agent.
          console.warn(
            `Got dlp-response message with unknown token ${request.requestToken}`
          );
          return;
        }
        this.requestTokenToRequestInfo.delete(request.requestToken);
        let dlpBusyView =
          this.dlpBusyViewsByTopBrowsingContext.getAndRemoveEntry(
            windowAndResourceNameOrOperationType.browsingContext,
            request.requestToken
          );
        this._disconnectFromView(dlpBusyView);
        const responseResult =
          request?.action ?? Ci.nsIContentAnalysisResponse.eUnspecified;
        // Don't show dialog if this is a cached response
        if (!request?.isCachedResponse) {
          await this._showCAResult(
            windowAndResourceNameOrOperationType.resourceNameOrOperationType,
            windowAndResourceNameOrOperationType.browsingContext,
            request.requestToken,
            responseResult,
            request.cancelError
          );
        }
        break;
      }
    }
  },

  async showPanel(element, panelUI) {
    element.ownerDocument.l10n.setAttributes(
      lazy.PanelMultiView.getViewNode(
        element.ownerDocument,
        "content-analysis-panel-description"
      ),
      "content-analysis-panel-text-styled",
      { agentName: lazy.agentName }
    );
    panelUI.showSubView("content-analysis-panel", element);
  },

  _disconnectFromView(caView) {
    if (!caView) {
      return;
    }
    if (caView.timer) {
      lazy.clearTimeout(caView.timer);
    } else if (caView.notification) {
      if (caView.notification.close) {
        // native notification
        caView.notification.close();
      } else if (caView.notification.dialogBrowsingContext) {
        // in-browser notification
        let browser =
          caView.notification.dialogBrowsingContext.top.embedderElement;
        // browser will be null if the tab was closed
        let win = browser?.ownerGlobal;
        if (win) {
          let dialogBox = win.gBrowser.getTabDialogBox(browser);
          // Just close the dialog associated with this CA request.
          dialogBox.getTabDialogManager().abortDialogs(dialog => {
            return (
              dialog.promptID ==
              this.PROMPTID_PREFIX + caView.request.requestToken
            );
          });
        }
      } else {
        console.error(
          "Unexpected content analysis notification - can't close it!"
        );
      }
    }
  },

  _showMessage(aMessage, aBrowsingContext, aTimeout = 0) {
    if (this._SHOW_DIALOGS) {
      Services.prompt.asyncAlert(
        aBrowsingContext,
        Ci.nsIPrompt.MODAL_TYPE_WINDOW,
        this.l10n.formatValueSync("contentanalysis-alert-title"),
        aMessage
      );
    }

    if (this._SHOW_NOTIFICATIONS) {
      let topWindow =
        aBrowsingContext.topChromeWindow ??
        aBrowsingContext.embedderWindowGlobal.browsingContext.topChromeWindow;
      const notification = new topWindow.Notification(
        this.l10n.formatValueSync("contentanalysis-notification-title"),
        {
          body: aMessage,
          silent: lazy.silentNotifications,
        }
      );

      if (aTimeout != 0) {
        lazy.setTimeout(() => {
          notification.close();
        }, aTimeout);
      }
      return notification;
    }

    return null;
  },

  _shouldShowBlockingNotification(aAnalysisType) {
    return !(
      aAnalysisType == Ci.nsIContentAnalysisRequest.eFileDownloaded ||
      aAnalysisType == Ci.nsIContentAnalysisRequest.ePrint
    );
  },

  // This function also transforms the nameOrOperationType so we won't have to
  // look it up again.
  _getResourceNameFromNameOrOperationType(nameOrOperationType) {
    if (!nameOrOperationType.name) {
      let l10nId = undefined;
      switch (nameOrOperationType.operationType) {
        case Ci.nsIContentAnalysisRequest.eClipboard:
          l10nId = "contentanalysis-operationtype-clipboard";
          break;
        case Ci.nsIContentAnalysisRequest.eDroppedText:
          l10nId = "contentanalysis-operationtype-dropped-text";
          break;
        case Ci.nsIContentAnalysisRequest.eOperationPrint:
          l10nId = "contentanalysis-operationtype-print";
          break;
      }
      if (!l10nId) {
        console.error(
          "Unknown operationTypeForDisplay: " +
            nameOrOperationType.operationType
        );
        return "";
      }
      nameOrOperationType.name = this.l10n.formatValueSync(l10nId);
    }
    return nameOrOperationType.name;
  },

  /**
   * Gets a name or operation type from a request
   *
   * @param {object} aRequest The nsIContentAnalysisRequest
   * @param {boolean} aStandalone Whether the message is going to be used on its own
   *                              line. This is used to add more context to the message
   *                              if a file is being uploaded rather than just the name
   *                              of the file.
   * @returns {object} An object with either a name property that can be used as-is, or
   *                   an operationType property.
   */
  _getResourceNameOrOperationTypeFromRequest(aRequest, aStandalone) {
    if (
      aRequest.operationTypeForDisplay ==
      Ci.nsIContentAnalysisRequest.eCustomDisplayString
    ) {
      if (aStandalone) {
        return {
          name: this.l10n.formatValueSync(
            "contentanalysis-customdisplaystring-description",
            {
              filename: aRequest.operationDisplayString,
            }
          ),
        };
      }
      return { name: aRequest.operationDisplayString };
    }
    return { operationType: aRequest.operationTypeForDisplay };
  },

  /**
   * Show a message to the user to indicate that a CA request is taking
   * a long time.
   */
  _showSlowCAMessage(
    aOperation,
    aRequest,
    aResourceNameOrOperationType,
    aBrowsingContext
  ) {
    if (!this._shouldShowBlockingNotification(aOperation)) {
      return this._showMessage(
        this._getSlowDialogMessage(aResourceNameOrOperationType),
        aBrowsingContext
      );
    }

    if (!aRequest) {
      throw new Error(
        "Showing in-browser Content Analysis notification but no request was passed"
      );
    }

    return this._showSlowCABlockingMessage(
      aBrowsingContext,
      aRequest.requestToken,
      aResourceNameOrOperationType
    );
  },

  _getSlowDialogMessage(aResourceNameOrOperationType) {
    if (aResourceNameOrOperationType.name) {
      return this.l10n.formatValueSync(
        "contentanalysis-slow-agent-dialog-body-file",
        {
          agent: lazy.agentName,
          filename: aResourceNameOrOperationType.name,
        }
      );
    }
    let l10nId = undefined;
    switch (aResourceNameOrOperationType.operationType) {
      case Ci.nsIContentAnalysisRequest.eClipboard:
        l10nId = "contentanalysis-slow-agent-dialog-body-clipboard";
        break;
      case Ci.nsIContentAnalysisRequest.eDroppedText:
        l10nId = "contentanalysis-slow-agent-dialog-body-dropped-text";
        break;
      case Ci.nsIContentAnalysisRequest.eOperationPrint:
        l10nId = "contentanalysis-slow-agent-dialog-body-print";
        break;
    }
    if (!l10nId) {
      console.error(
        "Unknown operationTypeForDisplay: ",
        aResourceNameOrOperationType
      );
      return "";
    }
    return this.l10n.formatValueSync(l10nId, {
      agent: lazy.agentName,
    });
  },

  _getErrorDialogMessage(aResourceNameOrOperationType) {
    if (aResourceNameOrOperationType.name) {
      return this.l10n.formatValueSync(
        "contentanalysis-error-message-upload-file",
        {
          filename: aResourceNameOrOperationType.name,
        }
      );
    }
    let l10nId = undefined;
    switch (aResourceNameOrOperationType.operationType) {
      case Ci.nsIContentAnalysisRequest.eClipboard:
        l10nId = "contentanalysis-error-message-clipboard";
        break;
      case Ci.nsIContentAnalysisRequest.eDroppedText:
        l10nId = "contentanalysis-error-message-dropped-text";
        break;
      case Ci.nsIContentAnalysisRequest.eOperationPrint:
        l10nId = "contentanalysis-error-message-print";
        break;
    }
    if (!l10nId) {
      console.error(
        "Unknown operationTypeForDisplay: ",
        aResourceNameOrOperationType
      );
      return "";
    }
    return this.l10n.formatValueSync(l10nId);
  },
  _showSlowCABlockingMessage(
    aBrowsingContext,
    aRequestToken,
    aResourceNameOrOperationType
  ) {
    let bodyMessage = this._getSlowDialogMessage(aResourceNameOrOperationType);
    // Note that TabDialogManager maintains a list of displaying dialogs, and so
    // we can pop up multiple of these and the first one will keep displaying until
    // it is closed, at which point the next one will display, etc.
    let promise = Services.prompt.asyncConfirmEx(
      aBrowsingContext,
      Ci.nsIPromptService.MODAL_TYPE_TAB,
      this.l10n.formatValueSync("contentanalysis-slow-agent-dialog-header"),
      bodyMessage,
      Ci.nsIPromptService.BUTTON_POS_0 *
        Ci.nsIPromptService.BUTTON_TITLE_CANCEL +
        Ci.nsIPromptService.BUTTON_POS_1_DEFAULT +
        Ci.nsIPromptService.SHOW_SPINNER,
      null,
      null,
      null,
      null,
      false,
      { promptID: this.PROMPTID_PREFIX + aRequestToken }
    );
    promise
      .catch(() => {
        // need a catch clause to avoid an unhandled JS exception
        // when we programmatically close the dialog.
        // Since this only happens when we are programmatically closing
        // the dialog, no need to log the exception.
      })
      .finally(() => {
        // This is also be called if the tab/window is closed while a request is in progress,
        // in which case we need to cancel the request.
        if (this.requestTokenToRequestInfo.delete(aRequestToken)) {
          lazy.gContentAnalysis.cancelContentAnalysisRequest(
            aRequestToken,
            false
          );
          let dlpBusyView =
            this.dlpBusyViewsByTopBrowsingContext.getAndRemoveEntry(
              aBrowsingContext,
              aRequestToken
            );
          this._disconnectFromView(dlpBusyView);
        }
      });
    return {
      requestToken: aRequestToken,
      dialogBrowsingContext: aBrowsingContext,
    };
  },

  /**
   * Show a message to the user to indicate the result of a CA request.
   *
   * @returns {object} a notification object (if shown)
   */
  async _showCAResult(
    aResourceNameOrOperationType,
    aBrowsingContext,
    aRequestToken,
    aCAResult,
    aRequestCancelError
  ) {
    let message = null;
    let timeoutMs = 0;

    switch (aCAResult) {
      case Ci.nsIContentAnalysisResponse.eAllow:
        // We don't need to show anything
        return null;
      case Ci.nsIContentAnalysisResponse.eReportOnly:
        message = await this.l10n.formatValue(
          "contentanalysis-genericresponse-message",
          {
            content: this._getResourceNameFromNameOrOperationType(
              aResourceNameOrOperationType
            ),
            response: "REPORT_ONLY",
          }
        );
        timeoutMs = this._RESULT_NOTIFICATION_FAST_TIMEOUT_MS;
        break;
      case Ci.nsIContentAnalysisResponse.eWarn: {
        let allow = false;
        try {
          const result = await Services.prompt.asyncConfirmEx(
            aBrowsingContext,
            Ci.nsIPromptService.MODAL_TYPE_TAB,
            await this.l10n.formatValue("contentanalysis-warndialogtitle"),
            await this._warnDialogText(aResourceNameOrOperationType),
            Ci.nsIPromptService.BUTTON_POS_0 *
              Ci.nsIPromptService.BUTTON_TITLE_IS_STRING +
              Ci.nsIPromptService.BUTTON_POS_1 *
                Ci.nsIPromptService.BUTTON_TITLE_IS_STRING +
              Ci.nsIPromptService.BUTTON_POS_2_DEFAULT,
            await this.l10n.formatValue(
              "contentanalysis-warndialog-response-allow"
            ),
            await this.l10n.formatValue(
              "contentanalysis-warndialog-response-deny"
            ),
            null,
            null,
            {}
          );
          allow = result.get("buttonNumClicked") === 0;
        } catch {
          // This can happen if the dialog is closed programmatically, for
          // example if the tab is moved to a new window.
          // In this case just pretend the user clicked deny, as this
          // emulates the behavior of cancelling when
          // the request is still active.
          allow = false;
        }
        lazy.gContentAnalysis.respondToWarnDialog(aRequestToken, allow);
        return null;
      }
      case Ci.nsIContentAnalysisResponse.eBlock: {
        if (!lazy.showBlockedResult) {
          // Don't show anything
          return null;
        }
        let titleId = undefined;
        let body = undefined;
        if (aResourceNameOrOperationType.name) {
          titleId = "contentanalysis-block-dialog-title-upload-file";
          body = this.l10n.formatValueSync(
            "contentanalysis-block-dialog-body-upload-file",
            {
              filename: aResourceNameOrOperationType.name,
            }
          );
        } else {
          let bodyId = undefined;
          let bodyHasContent = false;
          switch (aResourceNameOrOperationType.operationType) {
            case Ci.nsIContentAnalysisRequest.eClipboard: {
              // Unlike the cases below, this can be shown when the DLP
              // agent is not available.  We use a different message for that.
              const caInfo = await lazy.gContentAnalysis.getDiagnosticInfo();
              titleId = "contentanalysis-block-dialog-title-clipboard";
              bodyId = caInfo.connectedToAgent
                ? "contentanalysis-block-dialog-body-clipboard"
                : "contentanalysis-no-agent-connected-message-content";
              bodyHasContent = true;
              break;
            }
            case Ci.nsIContentAnalysisRequest.eDroppedText:
              titleId = "contentanalysis-block-dialog-title-dropped-text";
              bodyId = "contentanalysis-block-dialog-body-dropped-text";
              break;
            case Ci.nsIContentAnalysisRequest.eOperationPrint:
              titleId = "contentanalysis-block-dialog-title-print";
              bodyId = "contentanalysis-block-dialog-body-print";
              break;
          }
          if (!titleId || !bodyId) {
            console.error(
              "Unknown operationTypeForDisplay: ",
              aResourceNameOrOperationType
            );
            return null;
          }
          if (bodyHasContent) {
            body = this.l10n.formatValueSync(bodyId, {
              agent: lazy.agentName,
              content: "",
            });
          } else {
            body = this.l10n.formatValueSync(bodyId);
          }
        }
        let alertBrowsingContext = aBrowsingContext;
        if (aBrowsingContext.embedderElement?.getAttribute("printpreview")) {
          // If we're in a print preview dialog, things are tricky.
          // The window itself is about to close (because of the thrown NS_ERROR_CONTENT_BLOCKED),
          // so using an async call would just immediately make the dialog disappear. (bug 1899714)
          // Using a blocking version can cause a hang if the window is resizing while
          // we show the dialog. (bug 1900798)
          // So instead, try to find the browser that this print preview dialog is on top of
          // and show the dialog there.
          let printPreviewBrowser = aBrowsingContext.embedderElement;
          let win = printPreviewBrowser.ownerGlobal;
          for (let browser of win.gBrowser.browsers) {
            if (
              win.PrintUtils.getPreviewBrowser(browser)?.browserId ===
              printPreviewBrowser.browserId
            ) {
              alertBrowsingContext = browser.browsingContext;
              break;
            }
          }
        }
        await Services.prompt.asyncAlert(
          alertBrowsingContext,
          Ci.nsIPromptService.MODAL_TYPE_TAB,
          this.l10n.formatValueSync(titleId),
          body
        );
        return null;
      }
      case Ci.nsIContentAnalysisResponse.eUnspecified:
        message = await this.l10n.formatValue(
          "contentanalysis-unspecified-error-message-content",
          {
            agent: lazy.agentName,
            content: this._getErrorDialogMessage(aResourceNameOrOperationType),
          }
        );
        timeoutMs = this._RESULT_NOTIFICATION_TIMEOUT_MS;
        break;
      case Ci.nsIContentAnalysisResponse.eCanceled:
        {
          let messageId;
          switch (aRequestCancelError) {
            case Ci.nsIContentAnalysisResponse.eUserInitiated:
              console.error(
                "Got unexpected cancel response with eUserInitiated"
              );
              return null;
            case Ci.nsIContentAnalysisResponse.eOtherRequestInGroupCancelled:
              return null;
            case Ci.nsIContentAnalysisResponse.eNoAgent:
              messageId = "contentanalysis-no-agent-connected-message-content";
              break;
            case Ci.nsIContentAnalysisResponse.eInvalidAgentSignature:
              messageId =
                "contentanalysis-invalid-agent-signature-message-content";
              break;
            case Ci.nsIContentAnalysisResponse.eErrorOther:
              messageId = "contentanalysis-unspecified-error-message-content";
              break;
            case Ci.nsIContentAnalysisResponse.eShutdown:
              // we're shutting down, no need to show a dialog
              return null;
            default:
              console.error(
                "Unexpected CA cancelError value: " + aRequestCancelError
              );
              messageId = "contentanalysis-unspecified-error-message-content";
              break;
          }
          message = await this.l10n.formatValue(messageId, {
            agent: lazy.agentName,
            content: this._getErrorDialogMessage(aResourceNameOrOperationType),
          });
          timeoutMs = this._RESULT_NOTIFICATION_TIMEOUT_MS;
        }
        break;
      default:
        throw new Error("Unexpected CA result value: " + aCAResult);
    }

    if (!message) {
      console.error(
        "_showCAResult did not get a message populated for result value " +
          aCAResult
      );
      return null;
    }

    return this._showMessage(message, aBrowsingContext, timeoutMs);
  },

  /**
   * Returns the correct text for warn dialog contents.
   */
  async _warnDialogText(aResourceNameOrOperationType) {
    const caInfo = await lazy.gContentAnalysis.getDiagnosticInfo();
    if (caInfo.connectedToAgent) {
      return await this.l10n.formatValue("contentanalysis-warndialogtext", {
        content: this._getResourceNameFromNameOrOperationType(
          aResourceNameOrOperationType
        ),
      });
    }
    return await this.l10n.formatValue(
      "contentanalysis-no-agent-connected-message-content",
      {
        agent: lazy.agentName,
        content: "",
      }
    );
  },
};
