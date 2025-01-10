/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Implements nsIPromptCollection
 *
 * @class PromptCollection
 */
export class PromptCollection {
  confirmRepost(browsingContext) {
    let brandName;
    try {
      brandName = this.stringBundles.brand.GetStringFromName("brandShortName");
    } catch (exception) {
      // That's ok, we'll use a generic version of the prompt
    }

    let message;
    let resendLabel;
    try {
      if (brandName) {
        message = this.stringBundles.app.formatStringFromName(
          "confirmRepostPrompt",
          [brandName]
        );
      } else {
        // Use a generic version of this prompt.
        message = this.stringBundles.app.GetStringFromName(
          "confirmRepostPrompt"
        );
      }
      resendLabel =
        this.stringBundles.app.GetStringFromName("resendButton.label");
    } catch (exception) {
      console.error("Failed to get strings from appstrings.properties");
      return false;
    }

    let docViewer = browsingContext?.docShell?.docViewer;
    let modalType = docViewer?.isTabModalPromptAllowed
      ? Ci.nsIPromptService.MODAL_TYPE_CONTENT
      : Ci.nsIPromptService.MODAL_TYPE_WINDOW;
    let buttonFlags =
      (Ci.nsIPromptService.BUTTON_TITLE_IS_STRING *
        Ci.nsIPromptService.BUTTON_POS_0) |
      (Ci.nsIPromptService.BUTTON_TITLE_CANCEL *
        Ci.nsIPromptService.BUTTON_POS_1);
    let buttonPressed = Services.prompt.confirmExBC(
      browsingContext,
      modalType,
      null,
      message,
      buttonFlags,
      resendLabel,
      null,
      null,
      null,
      {}
    );

    return buttonPressed === 0;
  }

  async asyncBeforeUnloadCheck(browsingContext) {
    const docViewer = browsingContext?.docShell?.docViewer;
    if (
      (docViewer && !docViewer.isTabModalPromptAllowed) ||
      !browsingContext.ancestorsAreCurrent
    ) {
      console.error("Can't prompt from inactive content viewer");
      return true;
    }

    const isPDFjs =
      browsingContext.embedderElement?.contentPrincipal.originNoSuffix ===
      "resource://pdf.js";
    let title, message, leaveLabel, stayLabel, buttonFlags;
    let args = {
      // Tell the prompt service that this is a permit unload prompt
      // so that it can set the appropriate flag on the detail object
      // of the events it dispatches.
      inPermitUnload: true,
    };

    try {
      if (isPDFjs) {
        title = this.stringBundles.dom.GetStringFromName(
          "OnBeforeUnloadPDFjsTitle"
        );
        message = this.stringBundles.dom.GetStringFromName(
          "OnBeforeUnloadPDFjsMessage"
        );
        buttonFlags =
          Ci.nsIPromptService.BUTTON_POS_0_DEFAULT |
          (Ci.nsIPrompt.BUTTON_TITLE_SAVE * Ci.nsIPrompt.BUTTON_POS_0) |
          (Ci.nsIPrompt.BUTTON_TITLE_CANCEL * Ci.nsIPrompt.BUTTON_POS_1) |
          (Ci.nsIPrompt.BUTTON_TITLE_DONT_SAVE * Ci.nsIPrompt.BUTTON_POS_2);
        args.useTitle = true;
        args.headerIconCSSValue =
          "url('chrome://branding/content/document_pdf.svg')";
      } else {
        title = this.stringBundles.dom.GetStringFromName("OnBeforeUnloadTitle");
        message = this.stringBundles.dom.GetStringFromName(
          "OnBeforeUnloadMessage2"
        );
        leaveLabel = this.stringBundles.dom.GetStringFromName(
          "OnBeforeUnloadLeaveButton"
        );
        stayLabel = this.stringBundles.dom.GetStringFromName(
          "OnBeforeUnloadStayButton"
        );
        buttonFlags =
          Ci.nsIPromptService.BUTTON_POS_0_DEFAULT |
          (Ci.nsIPromptService.BUTTON_TITLE_IS_STRING *
            Ci.nsIPromptService.BUTTON_POS_0) |
          (Ci.nsIPromptService.BUTTON_TITLE_IS_STRING *
            Ci.nsIPromptService.BUTTON_POS_1);
      }
    } catch (exception) {
      console.error("Failed to get strings from dom.properties");
      return false;
    }

    const result = await Services.prompt.asyncConfirmEx(
      browsingContext,
      Services.prompt.MODAL_TYPE_CONTENT,
      title,
      message,
      buttonFlags,
      leaveLabel,
      stayLabel,
      null,
      null,
      false,
      args
    );
    const buttonNumClicked = result
      .QueryInterface(Ci.nsIPropertyBag2)
      .get("buttonNumClicked");
    if (isPDFjs) {
      if (buttonNumClicked === 0) {
        const savePdfPromise = new Promise(resolve => {
          Services.obs.addObserver(
            {
              observe(_aSubject, aTopic) {
                if (aTopic === "pdfjs:saveComplete") {
                  Services.obs.removeObserver(this, aTopic);
                  resolve();
                }
              },
            },
            "pdfjs:saveComplete"
          );
        });
        const actor = browsingContext.currentWindowGlobal.getActor("Pdfjs");
        actor.sendAsyncMessage("PDFJS:Save");
        await savePdfPromise;
      }
      return buttonNumClicked !== 1;
    }

    return buttonNumClicked === 0;
  }

  confirmFolderUpload(browsingContext, directoryName) {
    let title;
    let message;
    let acceptLabel;

    try {
      title = this.stringBundles.dom.GetStringFromName(
        "FolderUploadPrompt.title"
      );
      message = this.stringBundles.dom.formatStringFromName(
        "FolderUploadPrompt.message",
        [directoryName]
      );
      acceptLabel = this.stringBundles.dom.GetStringFromName(
        "FolderUploadPrompt.acceptButtonLabel"
      );
    } catch (exception) {
      console.error("Failed to get strings from dom.properties");
      return false;
    }

    let buttonFlags =
      Services.prompt.BUTTON_TITLE_IS_STRING * Services.prompt.BUTTON_POS_0 +
      Services.prompt.BUTTON_TITLE_CANCEL * Services.prompt.BUTTON_POS_1 +
      Services.prompt.BUTTON_POS_1_DEFAULT;

    return (
      Services.prompt.confirmExBC(
        browsingContext,
        Services.prompt.MODAL_TYPE_TAB,
        title,
        message,
        buttonFlags | Ci.nsIPrompt.BUTTON_DELAY_ENABLE,
        acceptLabel,
        null,
        null,
        null,
        {}
      ) === 0
    );
  }
}

const BUNDLES = {
  dom: "chrome://global/locale/dom/dom.properties",
  app: "chrome://global/locale/appstrings.properties",
  brand: "chrome://branding/locale/brand.properties",
};

PromptCollection.prototype.stringBundles = {};

for (const [bundleName, bundleUrl] of Object.entries(BUNDLES)) {
  ChromeUtils.defineLazyGetter(
    PromptCollection.prototype.stringBundles,
    bundleName,
    function () {
      let bundle = Services.strings.createBundle(bundleUrl);
      if (!bundle) {
        throw new Error("String bundle for dom not present!");
      }
      return bundle;
    }
  );
}

PromptCollection.prototype.QueryInterface = ChromeUtils.generateQI([
  "nsIPromptCollection",
]);
