/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = [ "TranslationContentHandler" ];

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.defineModuleGetter(this, "LanguageDetector",
  "resource:///modules/translation/LanguageDetector.jsm");

const STATE_OFFER = 0;
const STATE_TRANSLATED = 2;
const STATE_ERROR = 3;

var TranslationContentHandler = function(global, docShell) {
  let webProgress = docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                            .getInterface(Ci.nsIWebProgress);
  webProgress.addProgressListener(this, Ci.nsIWebProgress.NOTIFY_STATE_DOCUMENT);

  global.addEventListener("pageshow", this);

  global.addMessageListener("Translation:TranslateDocument", this);
  global.addMessageListener("Translation:ShowTranslation", this);
  global.addMessageListener("Translation:ShowOriginal", this);
  this.global = global;
};

TranslationContentHandler.prototype = {
  handleEvent(aEvent) {
    // We are only listening to pageshow events.
    let target = aEvent.target;

    // Only handle top-level frames.
    let win = target.defaultView;
    if (win.parent !== win)
      return;

    let content = this.global.content;
    if (!content.detectedLanguage)
      return;

    let data = {};
    let trDoc = content.translationDocument;
    if (trDoc) {
      data.state = trDoc.translationError ? STATE_ERROR : STATE_TRANSLATED;
      data.translatedFrom = trDoc.translatedFrom;
      data.translatedTo = trDoc.translatedTo;
      data.originalShown = trDoc.originalShown;
    } else {
      data.state = STATE_OFFER;
      data.originalShown = true;
    }
    data.detectedLanguage = content.detectedLanguage;

    this.global.sendAsyncMessage("Translation:DocumentState", data);
  },

  /* nsIWebProgressListener implementation */
  onStateChange(aWebProgress, aRequest, aStateFlags, aStatus) {
    if (!aWebProgress.isTopLevel ||
        !(aStateFlags & Ci.nsIWebProgressListener.STATE_STOP) ||
        !this.global.content)
      return;

    try {
      let url = aRequest.name;
      if (!url.startsWith("http://") && !url.startsWith("https://"))
        return;
    } catch (e) {
      // nsIRequest.name throws NS_ERROR_NOT_IMPLEMENTED for view-source: tabs.
      return;
    }

    let content = this.global.content;
    if (content.detectedLanguage)
      return;

    // Grab a 60k sample of text from the page.
    let encoder = Cu.createDocumentEncoder("text/plain");
    encoder.init(content.document, "text/plain", encoder.SkipInvisibleContent);
    let string = encoder.encodeToStringWithMaxLength(60 * 1024);

    // Language detection isn't reliable on very short strings.
    if (string.length < 100)
      return;

    LanguageDetector.detectLanguage(string).then(result => {
      // Bail if we're not confident.
      if (!result.confident) {
        return;
      }

      // The window might be gone by now.
      if (Cu.isDeadWrapper(content)) {
        return;
      }

      content.detectedLanguage = result.language;

      let data = {
        state: STATE_OFFER,
        originalShown: true,
        detectedLanguage: result.language,
      };
      this.global.sendAsyncMessage("Translation:DocumentState", data);
    });
  },

  QueryInterface: ChromeUtils.generateQI([Ci.nsIWebProgressListener,
                                          Ci.nsISupportsWeakReference]),

  receiveMessage(msg) {
    switch (msg.name) {
      case "Translation:TranslateDocument":
      {
        ChromeUtils.import("resource:///modules/translation/TranslationDocument.jsm");

        // If a TranslationDocument already exists for this document, it should
        // be used instead of creating a new one so that we can use the original
        // content of the page for the new translation instead of the newly
        // translated text.
        let translationDocument = this.global.content.translationDocument ||
                                  new TranslationDocument(this.global.content.document);

        let engine = Services.prefs.getCharPref("browser.translation.engine");
        let importScope =
          ChromeUtils.import(`resource:///modules/translation/${engine}Translator.jsm`, {});
        let translator = new importScope[engine + "Translator"](translationDocument,
                                                                msg.data.from,
                                                                msg.data.to);

        this.global.content.translationDocument = translationDocument;
        translationDocument.translatedFrom = msg.data.from;
        translationDocument.translatedTo = msg.data.to;
        translationDocument.translationError = false;

        translator.translate().then(
          result => {
            this.global.sendAsyncMessage("Translation:Finished", {
              characterCount: result.characterCount,
              from: msg.data.from,
              to: msg.data.to,
              success: true,
            });
            translationDocument.showTranslation();
          },
          error => {
            translationDocument.translationError = true;
            let data = {success: false};
            if (error == "unavailable")
              data.unavailable = true;
            this.global.sendAsyncMessage("Translation:Finished", data);
          }
        );
        break;
      }

      case "Translation:ShowOriginal":
        this.global.content.translationDocument.showOriginal();
        break;

      case "Translation:ShowTranslation":
        this.global.content.translationDocument.showTranslation();
        break;
    }
  },
};
