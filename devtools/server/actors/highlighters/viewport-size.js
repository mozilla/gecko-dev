/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EventEmitter = require("resource://devtools/shared/event-emitter.js");
const {
  setIgnoreLayoutChanges,
} = require("resource://devtools/shared/layout/utils.js");
const {
  CanvasFrameAnonymousContentHelper,
} = require("resource://devtools/server/actors/highlighters/utils/markup.js");

/**
 * The ViewportSizeHighlighter is a class that displays the viewport
 * width and height on a small overlay on the top right edge of the page
 * while the rulers are turned on. This class is also extended by ViewportSizeOnResizeHighlighter,
 * which is used to show the viewport information when the rulers aren't displayed.
 */
class ViewportSizeHighlighter {
  /**
   *
   * @param {HighlighterEnvironment} highlighterEnv
   * @param {InspectorActor} parent
   * @param {Object} options
   * @param {Number} options.hideTimeout: An optional number. When passed, the viewport
   *        information will automatically hide after {hideTimeout} ms.
   * @param {String} options.prefix: The prefix to use when creating anonymous elements.
   *        Defaults to "viewport-size-highlighter-"
   * @param {Boolean} options.waitForDocumentToLoad: Option that will be passed to
   *        CanvasFrameAnonymousContentHelper. Defaults to true
   * @param {Boolean} options.avoidForcedSynchronousLayoutUpdate: Option that will be passed to
   *        CanvasFrameAnonymousContentHelper. Defaults to false
   */
  constructor(highlighterEnv, parent, options = {}) {
    this.env = highlighterEnv;
    this.parent = parent;

    this.ID_CLASS_PREFIX = options?.prefix || "viewport-size-highlighter-";
    this.hideTimeout = options?.hideTimeout;

    this.markup = new CanvasFrameAnonymousContentHelper(
      highlighterEnv,
      this._buildMarkup.bind(this),
      {
        waitForDocumentToLoad: options?.waitForDocumentToLoad ?? true,
        avoidForcedSynchronousLayoutUpdate:
          options?.avoidForcedSynchronousLayoutUpdate ?? false,
      }
    );
    this._onPageResize = this._onPageResize.bind(this);
    this.isReady = this.markup.initialize();

    const { pageListenerTarget } = highlighterEnv;
    pageListenerTarget.addEventListener("pagehide", this);
  }

  /**
   * Static getter that indicates that BoxModelHighlighter supports
   * highlighting in XUL windows.
   */
  static get XULSupported() {
    return true;
  }

  get isFadingViewportHighlighter() {
    return this.hideTimeout !== undefined;
  }

  _buildMarkup() {
    const prefix = this.ID_CLASS_PREFIX;

    const container = this.markup.createNode({
      attributes: { class: "highlighter-container" },
    });

    this.markup.createNode({
      parent: container,
      attributes: {
        class: "viewport-infobar-container",
        id: "viewport-infobar-container",
        position: "top",
        hidden: "true",
      },
      prefix,
    });

    return container;
  }

  handleEvent(event) {
    switch (event.type) {
      case "pagehide":
        // If a page hide event is triggered for current window's highlighter, hide the
        // highlighter.
        if (event.target.defaultView === this.env.window) {
          this.destroy();
        }
        break;
    }
  }

  _update() {
    const { window } = this.env;

    setIgnoreLayoutChanges(true);
    this.updateViewportInfobar();
    setIgnoreLayoutChanges(false, window.document.documentElement);
  }

  updateViewportInfobar() {
    const { window } = this.env;
    const { innerHeight, innerWidth } = window;
    const infobarId = this.ID_CLASS_PREFIX + "viewport-infobar-container";
    const textContent = innerWidth + "px \u00D7 " + innerHeight + "px";
    this.markup.getElement(infobarId).setTextContent(textContent);
  }

  destroy() {
    if (this._destroyed) {
      return;
    }
    this._destroyed = true;

    if (
      this.isFadingViewportHighlighter &&
      this.parent.highlightersState?.fadingViewportSizeHiglighter
    ) {
      this.parent.highlightersState.fadingViewportSizeHiglighter = null;
    }

    this.hide();

    const { pageListenerTarget } = this.env;

    if (pageListenerTarget) {
      pageListenerTarget.removeEventListener("pagehide", this);
    }

    this.markup.destroy();

    this.env = null;
    this.parent = null;
    this.markup = null;
    this.isReady = null;

    EventEmitter.emit(this, "destroy");
  }

  show() {
    const { pageListenerTarget } = this.env;
    pageListenerTarget.addEventListener("resize", this._onPageResize);
    if (this.isFadingViewportHighlighter) {
      this.parent.highlightersState.fadingViewportSizeHiglighter = this;
    } else {
      // If this is handling the regular viewport highlighter (i.e. we want to show rulers)
      // hide the viewport on resize highlighter we might have.
      if (this.parent.highlightersState.fadingViewportSizeHiglighter) {
        this.parent.highlightersState.fadingViewportSizeHiglighter.hide();
      }

      // show infobar so that it's not hidden after re-enabling rulers
      this._showInfobarContainer();
      this._update();
    }

    return true;
  }

  _onPageResize() {
    const { window } = this.env;
    if (this.isFadingViewportHighlighter) {
      window.clearTimeout(this.resizeTimer);
    }
    this._showInfobarContainer();
    this._update();

    if (this.isFadingViewportHighlighter) {
      this.resizeTimer = window.setTimeout(() => {
        this._hideInfobarContainer();
      }, this.hideTimeout);
    }
  }

  _showInfobarContainer() {
    this.markup.removeAttributeForElement(
      this.ID_CLASS_PREFIX + "viewport-infobar-container",
      "hidden"
    );
  }

  hide() {
    const { pageListenerTarget, window } = this.env;
    pageListenerTarget.removeEventListener("resize", this._onPageResize);
    this._hideInfobarContainer();
    if (this.isFadingViewportHighlighter) {
      window.clearTimeout(this.resizeTimer);
    } else if (this.parent.highlightersState?.fadingViewportSizeHiglighter) {
      // Re-set the viewport on resize highlighter when hiding the rulers
      this.parent.highlightersState.fadingViewportSizeHiglighter.show();
    }
  }

  _hideInfobarContainer() {
    this.markup.setAttributeForElement(
      this.ID_CLASS_PREFIX + "viewport-infobar-container",
      "hidden",
      "true"
    );
  }
}
exports.ViewportSizeHighlighter = ViewportSizeHighlighter;
