/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/browser-window */

document.addEventListener(
  "DOMContentLoaded",
  () => {
    const IS_WEBEXT_PANELS =
      document.documentElement.id === "webextpanels-window";

    const contextMenuPopup = document.getElementById("contentAreaContextMenu");

    // eslint-disable-next-line complexity
    contextMenuPopup.addEventListener("command", event => {
      switch (event.target.id) {
        case "context-viewsource-goToLine":
          gViewSourceUtils
            .getPageActor(gContextMenu.browser)
            .promptAndGoToLine();
          break;
        case "context-viewsource-wrapLongLines":
          gViewSourceUtils
            .getPageActor(gContextMenu.browser)
            .sendAsyncMessage("ViewSource:ToggleWrapping");
          break;
        case "context-viewsource-highlightSyntax":
          gViewSourceUtils
            .getPageActor(gContextMenu.browser)
            .sendAsyncMessage("ViewSource:ToggleSyntaxHighlighting");
          break;
        case "spell-add-to-dictionary":
          InlineSpellCheckerUI.addToDictionary();
          break;
        case "spell-undo-add-to-dictionary":
          InlineSpellCheckerUI.undoAddToDictionary();
          break;
        case "context-openlinkincurrent":
          gContextMenu.openLinkInCurrent();
          break;
        case "context-openlinkincontainertab":
        case "context-openlinkintab":
          gContextMenu.openLinkInTab(event);
          break;
        case "context-openlink":
          gContextMenu.openLink();
          break;
        case "context-openlinkprivate":
          gContextMenu.openLinkInPrivateWindow();
          break;
        case "context-bookmarklink":
          gContextMenu.bookmarkLink();
          break;
        case "context-savelink":
          gContextMenu.saveLink();
          break;
        case "context-savelinktopocket":
          Pocket.savePage(gContextMenu.browser, gContextMenu.linkURL);
          break;
        case "context-copyemail":
          gContextMenu.copyEmail();
          break;
        case "context-copyphone":
          gContextMenu.copyPhone();
          break;
        case "context-copylink":
          gContextMenu.copyLink();
          break;
        case "context-stripOnShareLink":
          gContextMenu.copyStrippedLink();
          break;
        case "context-media-play":
          gContextMenu.mediaCommand("play");
          break;
        case "context-media-pause":
          gContextMenu.mediaCommand("pause");
          break;
        case "context-media-mute":
          gContextMenu.mediaCommand("mute");
          break;
        case "context-media-unmute":
          gContextMenu.mediaCommand("unmute");
          break;
        case "context-media-playbackrate-050x":
          gContextMenu.mediaCommand("playbackRate", 0.5);
          break;
        case "context-media-playbackrate-100x":
          gContextMenu.mediaCommand("playbackRate", 1.0);
          break;
        case "context-media-playbackrate-125x":
          gContextMenu.mediaCommand("playbackRate", 1.25);
          break;
        case "context-media-playbackrate-150x":
          gContextMenu.mediaCommand("playbackRate", 1.5);
          break;
        case "context-media-playbackrate-200x":
          gContextMenu.mediaCommand("playbackRate", 2.0);
          break;
        case "context-media-loop":
          gContextMenu.mediaCommand("loop");
          break;
        case "context-leave-dom-fullscreen":
          gContextMenu.leaveDOMFullScreen();
          break;
        case "context-video-fullscreen":
          gContextMenu.mediaCommand("fullscreen");
          break;
        case "context-media-hidecontrols":
          gContextMenu.mediaCommand("hidecontrols");
          break;
        case "context-media-showcontrols":
          gContextMenu.mediaCommand("showcontrols");
          break;
        case "context-viewimage":
        case "context-viewvideo":
          gContextMenu.viewMedia(event);
          break;
        case "context-video-pictureinpicture":
          gContextMenu.mediaCommand("pictureinpicture");
          break;
        case "context-reloadimage":
          gContextMenu.reloadImage();
          break;
        case "context-video-saveimage":
          gContextMenu.saveVideoFrameAsImage();
          break;
        case "context-saveaudio":
        case "context-saveimage":
        case "context-savevideo":
          gContextMenu.saveMedia();
          break;
        case "context-copyimage-contents":
          goDoCommand("cmd_copyImage");
          break;
        case "context-copyaudiourl":
        case "context-copyimage":
        case "context-copyvideourl":
          gContextMenu.copyMediaLocation();
          break;
        case "context-sendaudio":
        case "context-sendimage":
        case "context-sendvideo":
          gContextMenu.sendMedia();
          break;
        case "context-imagetext":
          gContextMenu.getImageText();
          break;
        case "context-viewimageinfo":
          gContextMenu.viewImageInfo();
          break;
        case "context-viewimagedesc":
          gContextMenu.viewImageDesc(event);
          break;
        case "context-setDesktopBackground":
          gContextMenu.setDesktopBackground();
          break;
        case "context-bookmarkpage":
          gContextMenu.bookmarkThisPage();
          break;
        case "context-savepage":
          gContextMenu.savePageAs();
          break;
        case "context-pocket":
          Pocket.savePage(
            gContextMenu.browser,
            gContextMenu.browser.currentURI.spec,
            gContextMenu.browser.contentTitle
          );
          break;
        case "fill-login-generated-password":
          gContextMenu.useGeneratedPassword();
          break;
        case "use-relay-mask":
          gContextMenu.useRelayMask();
          break;
        case "manage-saved-logins":
          gContextMenu.openPasswordManager();
          break;
        case "context-pdfjs-highlight-selection":
          gContextMenu.pdfJSCmd("highlightSelection");
          break;
        case "context-reveal-password":
          gContextMenu.toggleRevealPassword();
          break;
        case "context-print-selection":
          gContextMenu.printSelection();
          break;
        case "context-pdfjs-undo":
          gContextMenu.pdfJSCmd("undo");
          break;
        case "context-pdfjs-redo":
          gContextMenu.pdfJSCmd("redo");
          break;
        case "context-pdfjs-cut":
          gContextMenu.pdfJSCmd("cut");
          break;
        case "context-pdfjs-copy":
          gContextMenu.pdfJSCmd("copy");
          break;
        case "context-pdfjs-paste":
          gContextMenu.pdfJSCmd("paste");
          break;
        case "context-pdfjs-delete":
          gContextMenu.pdfJSCmd("delete");
          break;
        case "context-pdfjs-selectall":
          gContextMenu.pdfJSCmd("selectAll");
          break;
        case "context-take-screenshot":
          gContextMenu.takeScreenshot();
          break;
        case "context-keywordfield":
          AddKeywordForSearchField();
          break;
        case "context-searchselect": {
          let { searchTerms, usePrivate, principal, csp } = event.target;
          BrowserSearch.loadSearchFromContext(
            searchTerms,
            usePrivate,
            principal,
            csp,
            event
          );
          break;
        }
        case "context-searchselect-private": {
          let { searchTerms, principal, csp } = event.target;
          BrowserSearch.loadSearchFromContext(
            searchTerms,
            true,
            principal,
            csp,
            event
          );
          break;
        }
        case "context-translate-selection":
          gContextMenu.openSelectTranslationsPanel(event);
          break;
        case "context-ask-chat":
          nsContextMenu.GenAI.handleAskChat(event);
          break;
        case "context-showonlythisframe":
          gContextMenu.showOnlyThisFrame();
          break;
        case "context-openframeintab":
          gContextMenu.openFrameInTab();
          break;
        case "context-openframe":
          gContextMenu.openFrame();
          break;
        case "context-reloadframe":
          gContextMenu.reloadFrame(event);
          break;
        case "context-bookmarkframe":
          gContextMenu.addBookmarkForFrame();
          break;
        case "context-saveframe":
          gContextMenu.saveFrame();
          break;
        case "context-printframe":
          gContextMenu.printFrame();
          break;
        case "context-take-frame-screenshot":
          gContextMenu.takeScreenshot();
          break;
        case "context-viewframesource":
          gContextMenu.viewFrameSource();
          break;
        case "context-viewframeinfo":
          gContextMenu.viewFrameInfo();
          break;
        case "spell-check-enabled":
          InlineSpellCheckerUI.toggleEnabled(window);
          break;
        case "spell-add-dictionaries":
        case "spell-add-dictionaries-main":
          gContextMenu.addDictionaries();
          break;
        case "context-bidi-page-direction-toggle":
          gContextMenu.switchPageDirection();
          break;
        case "context-viewpartialsource-selection":
          gContextMenu.viewPartialSource();
          break;
        case "context-viewsource":
          BrowserCommands.viewSource(gContextMenu.browser);
          break;
        case "context-inspect-a11y":
          gContextMenu.inspectA11Y();
          break;
        case "context-inspect":
          gContextMenu.inspectNode();
          break;
        case "context-media-eme-learnmore":
          gContextMenu.drmLearnMore(event);
          break;
      }
    });
    contextMenuPopup.addEventListener("popupshowing", event => {
      if (event.target != contextMenuPopup) {
        return;
      }

      // eslint-disable-next-line no-global-assign
      gContextMenu = new nsContextMenu(contextMenuPopup, event.shiftKey);
      if (!gContextMenu.shouldDisplay) {
        event.preventDefault();
        return;
      }

      if (!IS_WEBEXT_PANELS) {
        updateEditUIVisibility();
      }
    });
    contextMenuPopup.addEventListener("popuphiding", event => {
      if (event.target != contextMenuPopup) {
        return;
      }

      gContextMenu.hiding(contextMenuPopup);
      // eslint-disable-next-line no-global-assign
      gContextMenu = null;
      if (!IS_WEBEXT_PANELS) {
        updateEditUIVisibility();
      }
    });
  },
  { once: true }
);
