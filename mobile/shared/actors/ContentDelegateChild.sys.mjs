/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { GeckoViewActorChild } from "resource://gre/modules/GeckoViewActorChild.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  ManifestObtainer: "resource://gre/modules/ManifestObtainer.sys.mjs",
  SelectionUtils: "resource://gre/modules/SelectionUtils.sys.mjs",
  SpellCheckHelper: "resource://gre/modules/InlineSpellChecker.sys.mjs",
});

const MAX_TEXT_LENGTH = 4096;

export class ContentDelegateChild extends GeckoViewActorChild {
  notifyParentOfViewportFit() {
    if (this.triggerViewportFitChange) {
      this.contentWindow.cancelIdleCallback(this.triggerViewportFitChange);
    }
    this.triggerViewportFitChange = this.contentWindow.requestIdleCallback(
      () => {
        this.triggerViewportFitChange = null;
        const viewportFit = this.contentWindow.windowUtils.getViewportFitInfo();
        if (this.lastViewportFit === viewportFit) {
          return;
        }
        this.lastViewportFit = viewportFit;
        this.eventDispatcher.sendRequest({
          type: "GeckoView:DOMMetaViewportFit",
          viewportfit: viewportFit,
        });
      }
    );
  }

  #getSelection(aElement, aEditFlags) {
    if (aEditFlags & lazy.SpellCheckHelper.TEXTINPUT) {
      return aElement?.editor?.selection;
    }

    return this.contentWindow.getSelection();
  }

  #getSelectionBoundingRect(aFocusedElement, aEditFlags) {
    const selection = this.#getSelection(aFocusedElement, aEditFlags);
    if (!selection || selection.isCollapsed || selection.rangeCount != 1) {
      return null;
    }
    const range = selection.getRangeAt(0);
    return range.getBoundingClientRect();
  }

  /**
   * Show action menu if contextmenu is by mouse and we have selected text
   */
  #showActionMenu(aEvent) {
    if (aEvent.pointerType !== "mouse") {
      return false;
    }

    const win = this.contentWindow;
    const selectionInfo = lazy.SelectionUtils.getSelectionDetails(win);
    if (!selectionInfo.text.length) {
      // Don't show action menu by contextmenu event if no selection
      return false;
    }

    // The selection isn't collapsed and has a text.  We try to show action menu
    const focusedElement =
      Services.focus.focusedElement || aEvent.composedTarget;

    const editFlags = lazy.SpellCheckHelper.isEditable(focusedElement, win);
    const selectionEditable = !!(
      editFlags &
      (lazy.SpellCheckHelper.EDITABLE | lazy.SpellCheckHelper.CONTENTEDITABLE)
    );
    const boundingClientRect = this.#getSelectionBoundingRect(
      focusedElement,
      editFlags
    );

    const caretEvent = new CaretStateChangedEvent("mozcaretstatechanged", {
      bubbles: true,
      collapsed: selectionInfo.docSelectionIsCollapsed,
      boundingClientRect,
      reason: "taponcaret",
      caretVisible: true,
      selectionVisible: true,
      selectionEditable,
      selectedTextContent: selectionInfo.text,
    });

    win.dispatchEvent(caretEvent);

    // If selection is changed, or focus is changed, dismiss action menu
    const eventTarget = (() => {
      if (editFlags & lazy.SpellCheckHelper.TEXTINPUT) {
        return focusedElement;
      }
      return this.contentWindow.document;
    })();

    function dismissHandler() {
      const dismissEvent = new CaretStateChangedEvent("mozcaretstatechanged", {
        bubbles: true,
        reason: "visibilitychange",
      });
      win.dispatchEvent(dismissEvent);

      eventTarget.removeEventListener("selectionchange", dismissHandler);
      win.removeEventListener("focusin", dismissHandler);
      win.removeEventListener("focusout", dismissHandler);
      win.removeEventListener("blur", dismissHandler);
    }

    eventTarget.addEventListener("selectionchange", dismissHandler);
    win.addEventListener("focusin", dismissHandler);
    win.addEventListener("focusout", dismissHandler);
    win.addEventListener("blur", dismissHandler);

    return true;
  }

  // eslint-disable-next-line complexity
  handleEvent(aEvent) {
    debug`handleEvent: ${aEvent.type}`;

    switch (aEvent.type) {
      case "contextmenu": {
        if (aEvent.defaultPrevented) {
          return;
        }

        if (this.#showActionMenu(aEvent)) {
          aEvent.preventDefault();
          return;
        }

        function nearestParentAttribute(aNode, aAttribute) {
          while (
            aNode &&
            aNode.hasAttribute &&
            !aNode.hasAttribute(aAttribute)
          ) {
            aNode = aNode.parentNode;
          }
          return aNode && aNode.getAttribute && aNode.getAttribute(aAttribute);
        }

        function createAbsoluteUri(aBaseUri, aUri) {
          if (!aUri || !aBaseUri || !aBaseUri.displaySpec) {
            return null;
          }
          return Services.io.newURI(aUri, null, aBaseUri).displaySpec;
        }

        const node = aEvent.composedTarget;
        const baseUri = node.ownerDocument.baseURIObject;
        const uri = createAbsoluteUri(
          baseUri,
          nearestParentAttribute(node, "href")
        );
        const title = nearestParentAttribute(node, "title");
        const alt = nearestParentAttribute(node, "alt");
        const elementType = ChromeUtils.getClassName(node);
        const isImage = elementType === "HTMLImageElement";
        const isMedia =
          elementType === "HTMLVideoElement" ||
          elementType === "HTMLAudioElement";
        let elementSrc = (isImage || isMedia) && (node.currentSrc || node.src);
        if (elementSrc) {
          const isBlob = elementSrc.startsWith("blob:");
          if (isBlob && !URL.isValidObjectURL(elementSrc)) {
            elementSrc = null;
          }
        }

        if (uri || isImage || isMedia) {
          const msg = {
            type: "GeckoView:ContextMenu",
            // We don't have full zoom on Android, so using CSS coordinates
            // here is fine, since the CSS coordinate spaces match between the
            // child and parent processes.
            //
            // TODO(m_kato):
            // title, alt and textContent should consider surrogate pair and grapheme cluster?
            screenX: aEvent.screenX,
            screenY: aEvent.screenY,
            baseUri: (baseUri && baseUri.displaySpec) || null,
            uri,
            title: (title && title.substring(0, MAX_TEXT_LENGTH)) || null,
            alt: (alt && alt.substring(0, MAX_TEXT_LENGTH)) || null,
            elementType,
            elementSrc: elementSrc || null,
            textContent:
              (node.textContent &&
                node.textContent.substring(0, MAX_TEXT_LENGTH)) ||
              null,
          };

          this.eventDispatcher.sendRequest(msg);
          aEvent.preventDefault();
        }
        break;
      }
      case "MozDOMFullscreen:Request": {
        this.sendAsyncMessage("GeckoView:DOMFullscreenRequest", {});
        break;
      }
      case "MozDOMFullscreen:Entered":
      case "MozDOMFullscreen:Exited":
        // Content may change fullscreen state by itself, and we should ensure
        // that the parent always exits fullscreen when content has left
        // full screen mode.
        if (this.contentWindow?.document.fullscreenElement) {
          break;
        }
      // fall-through
      case "MozDOMFullscreen:Exit":
        this.sendAsyncMessage("GeckoView:DOMFullscreenExit", {});
        break;
      case "DOMMetaViewportFitChanged":
        if (aEvent.originalTarget.ownerGlobal == this.contentWindow) {
          this.notifyParentOfViewportFit();
        }
        break;
      case "DOMContentLoaded": {
        if (aEvent.originalTarget.ownerGlobal == this.contentWindow) {
          // If loaded content doesn't have viewport-fit, parent still
          // uses old value of previous content.
          this.notifyParentOfViewportFit();
        }
        if (this.contentWindow !== this.contentWindow?.top) {
          // Only check WebApp manifest on the top level window.
          return;
        }
        this.contentWindow.requestIdleCallback(async () => {
          const manifest = await lazy.ManifestObtainer.contentObtainManifest(
            this.contentWindow
          );
          if (manifest) {
            this.eventDispatcher.sendRequest({
              type: "GeckoView:WebAppManifest",
              manifest,
            });
          }
        });
        break;
      }
      case "MozFirstContentfulPaint": {
        this.eventDispatcher.sendRequest({
          type: "GeckoView:FirstContentfulPaint",
        });
        break;
      }
      case "MozPaintStatusReset": {
        this.eventDispatcher.sendRequest({
          type: "GeckoView:PaintStatusReset",
        });
        break;
      }
    }
  }
}

const { debug, warn } = ContentDelegateChild.initLogging(
  "ContentDelegateChild"
);
