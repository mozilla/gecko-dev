/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/browser-window */

"use strict";

// This is loaded into all browser windows. Wrap in a block to prevent
// leaking to window scope.
{
  const TAB_PREVIEW_PREF = "browser.tabs.hoverPreview.enabled";

  const DIRECTION_BACKWARD = -1;
  const DIRECTION_FORWARD = 1;

  const GROUP_DROP_ACTION_CREATE = 0x1;
  const GROUP_DROP_ACTION_APPEND = 0x2;

  /**
   * @param {MozTabbrowserTab|MozTabbrowserTabGroup} element
   * @returns {boolean}
   *   `true` if element is a `<tab>`
   */
  const isTab = element => element.tagName == "tab";

  /**
   * @param {MozTabbrowserTab|MozTabbrowserTabGroup} element
   * @returns {boolean}
   *   `true` if element is a `<tab-group>`
   */
  const isTabGroup = element => element.tagName == "tab-group";

  /**
   * @param {MozTabbrowserTab|MozTextLabel} element
   * @returns {boolean}
   *   `true` if element is the `<label>` in a `<tab-group>`
   */
  const isTabGroupLabel = element =>
    element.classList.contains("tab-group-label");

  class MozTabbrowserTabs extends MozElements.TabsBase {
    static observedAttributes = ["orient"];

    #maxTabsPerRow;
    #dragOverCreateGroupTimer;
    #mustUpdateTabMinHeight = false;
    #tabMinHeight = 36;

    constructor() {
      super();

      this.addEventListener("TabSelect", this);
      this.addEventListener("TabClose", this);
      this.addEventListener("TabAttrModified", this);
      this.addEventListener("TabHide", this);
      this.addEventListener("TabShow", this);
      this.addEventListener("TabPinned", this);
      this.addEventListener("TabUnpinned", this);
      this.addEventListener("TabHoverStart", this);
      this.addEventListener("TabHoverEnd", this);
      this.addEventListener("TabGroupExpand", this);
      this.addEventListener("TabGroupCollapse", this);
      this.addEventListener("transitionend", this);
      this.addEventListener("dblclick", this);
      this.addEventListener("click", this);
      this.addEventListener("click", this, true);
      this.addEventListener("dragstart", this);
      this.addEventListener("dragover", this);
      this.addEventListener("drop", this);
      this.addEventListener("dragend", this);
      this.addEventListener("dragleave", this);
      this.addEventListener("mouseleave", this);
      this.addEventListener("focusin", this);
      this.addEventListener("focusout", this);
    }

    init() {
      this.startupTime = Services.startup.getStartupInfo().start.getTime();

      this.arrowScrollbox = this.querySelector("arrowscrollbox");
      this.arrowScrollbox.addEventListener("wheel", this, true);
      this.arrowScrollbox.addEventListener("underflow", this);
      this.arrowScrollbox.addEventListener("overflow", this);
      this.verticalPinnedTabsContainer = document.getElementById(
        "vertical-pinned-tabs-container"
      );
      // Override arrowscrollbox.js method, since our scrollbox's children are
      // inherited from the scrollbox binding parent (this).
      this.arrowScrollbox._getScrollableElements = () => {
        return this.ariaFocusableItems.filter(
          this.arrowScrollbox._canScrollToElement
        );
      };
      this.arrowScrollbox._canScrollToElement = element => {
        if (isTab(element)) {
          return !element.pinned || !this.hasAttribute("positionpinnedtabs");
        }
        return true;
      };

      // Override for performance reasons. This is the size of a single element
      // that can be scrolled when using mouse wheel scrolling. If we don't do
      // this then arrowscrollbox computes this value by calling
      // _getScrollableElements and dividing the box size by that number.
      // However in the tabstrip case we already know the answer to this as,
      // when we're overflowing, it is always the same as the tab min width or
      // height. For tab group labels, the number won't exactly match, but
      // that shouldn't be a problem in practice since the arrowscrollbox
      // stops at element bounds when finishing scrolling.
      Object.defineProperty(this.arrowScrollbox, "lineScrollAmount", {
        get: () =>
          this.verticalMode ? this.#tabMinHeight : this._tabMinWidthPref,
      });

      this.baseConnect();

      this._blockDblClick = false;
      this._tabDropIndicator = this.querySelector(".tab-drop-indicator");
      this._dragOverDelay = 350;
      this._dragTime = 0;
      this._closeButtonsUpdatePending = false;
      this._closingTabsSpacer = this.querySelector(".closing-tabs-spacer");
      this._tabDefaultMaxWidth = NaN;
      this._lastTabClosedByMouse = false;
      this._hasTabTempMaxWidth = false;
      this._scrollButtonWidth = 0;
      this._lastNumPinned = 0;
      this._pinnedTabsLayoutCache = null;
      this._animateElement = this.arrowScrollbox;
      this._tabClipWidth = Services.prefs.getIntPref(
        "browser.tabs.tabClipWidth"
      );
      this._hiddenSoundPlayingTabs = new Set();
      this.previewPanel = null;

      this.allTabs[0].label = this.emptyTabTitle;

      // Hide the secondary text for locales where it is unsupported due to size constraints.
      const language = Services.locale.appLocaleAsBCP47;
      const unsupportedLocales = Services.prefs.getCharPref(
        "browser.tabs.secondaryTextUnsupportedLocales"
      );
      this.toggleAttribute(
        "secondarytext-unsupported",
        unsupportedLocales.split(",").includes(language.split("-")[0])
      );

      this.newTabButton.setAttribute(
        "aria-label",
        GetDynamicShortcutTooltipText("tabs-newtab-button")
      );

      let handleResize = () => {
        this._updateCloseButtons();
        this._handleTabSelect(true);
      };
      window.addEventListener("resize", handleResize);
      this._fullscreenMutationObserver = new MutationObserver(handleResize);
      this._fullscreenMutationObserver.observe(document.documentElement, {
        attributeFilter: ["inFullscreen", "inDOMFullscreen"],
      });

      this.boundObserve = (...args) => this.observe(...args);
      Services.prefs.addObserver("privacy.userContext", this.boundObserve);
      this.observe(null, "nsPref:changed", "privacy.userContext.enabled");

      document
        .getElementById("vertical-tabs-newtab-button")
        .addEventListener("keypress", this);
      document
        .getElementById("tabs-newtab-button")
        .addEventListener("keypress", this);

      XPCOMUtils.defineLazyPreferenceGetter(
        this,
        "_tabMinWidthPref",
        "browser.tabs.tabMinWidth",
        null,
        (pref, prevValue, newValue) => this.#updateTabMinWidth(newValue),
        newValue => {
          const LIMIT = 50;
          return Math.max(newValue, LIMIT);
        }
      );
      this.#updateTabMinWidth(this._tabMinWidthPref);
      this.#updateTabMinHeight();

      CustomizableUI.addListener(this);
      this._updateNewTabVisibility();

      XPCOMUtils.defineLazyPreferenceGetter(
        this,
        "_closeTabByDblclick",
        "browser.tabs.closeTabByDblclick",
        false
      );

      if (gMultiProcessBrowser) {
        this.tabbox.tabpanels.setAttribute("async", "true");
      }

      XPCOMUtils.defineLazyPreferenceGetter(
        this,
        "_showCardPreviews",
        TAB_PREVIEW_PREF,
        false
      );
      this.tooltip = "tabbrowser-tab-tooltip";
    }

    attributeChangedCallback(name, oldValue, newValue) {
      if (name != "orient") {
        return;
      }

      if (oldValue == "vertical" && newValue == "horizontal") {
        this._resetVerticalPinnedTabs();
      }
      if (this.overflowing) {
        // reset this value so we don't have incorrect styling for vertical tabs
        this.removeAttribute("overflow");
      }
      this._positionPinnedTabs();

      this.#updateTabMinWidth();
      this.#updateTabMinHeight();

      let indicatorTabs = gBrowser.visibleTabs.filter(tab => {
        return (
          tab.hasAttribute("soundplaying") ||
          tab.hasAttribute("muted") ||
          tab.hasAttribute("activemedia-blocked")
        );
      });
      for (const indicatorTab of indicatorTabs) {
        this.updateTabIndicatorAttr(indicatorTab);
      }

      super.attributeChangedCallback(name, oldValue, newValue);
    }

    on_TabSelect() {
      this._handleTabSelect();
    }

    on_TabClose(event) {
      this._hiddenSoundPlayingStatusChanged(event.target, { closed: true });
    }

    on_TabAttrModified(event) {
      if (
        ["soundplaying", "muted", "activemedia-blocked", "sharing"].some(attr =>
          event.detail.changed.includes(attr)
        )
      ) {
        this.updateTabIndicatorAttr(event.target);
      }

      if (
        event.detail.changed.includes("soundplaying") &&
        !event.target.visible
      ) {
        this._hiddenSoundPlayingStatusChanged(event.target);
      }
    }

    on_TabHide(event) {
      if (event.target.soundPlaying) {
        this._hiddenSoundPlayingStatusChanged(event.target);
      }
    }

    on_TabShow(event) {
      if (event.target.soundPlaying) {
        this._hiddenSoundPlayingStatusChanged(event.target);
      }
    }

    on_TabPinned(event) {
      this.updateTabIndicatorAttr(event.target);
    }

    on_TabUnpinned(event) {
      this.updateTabIndicatorAttr(event.target);
    }

    on_TabHoverStart(event) {
      if (!this._showCardPreviews) {
        return;
      }
      if (!this.previewPanel) {
        // load the tab preview component
        const TabHoverPreviewPanel = ChromeUtils.importESModule(
          "chrome://browser/content/tabbrowser/tab-hover-preview.mjs"
        ).default;
        this.previewPanel = new TabHoverPreviewPanel(
          document.getElementById("tab-preview-panel")
        );
      }
      this.previewPanel.activate(event.target);
    }

    on_TabHoverEnd(event) {
      this.previewPanel?.deactivate(event.target);
    }

    on_TabGroupExpand() {
      this._invalidateCachedVisibleTabs();
    }

    on_TabGroupCollapse(event) {
      this._invalidateCachedVisibleTabs();

      // If the user's selected tab is in the collapsing group, kick them off
      // the tab. If no tabs exist outside the group, create a new one and
      // select it.
      const group = event.target;
      if (gBrowser.selectedTab.group === group) {
        gBrowser.selectedTab =
          gBrowser._findTabToBlurTo(
            gBrowser.selectedTab,
            gBrowser.tabsInCollapsedTabGroups
          ) ||
          gBrowser.addTrustedTab(BROWSER_NEW_TAB_URL, { skipAnimation: true });
      }
    }

    on_transitionend(event) {
      if (event.propertyName != "max-width") {
        return;
      }

      let tab = event.target ? event.target.closest("tab") : null;

      if (tab.hasAttribute("fadein")) {
        if (tab._fullyOpen) {
          this._updateCloseButtons();
          if (tab.group && tab == tab.group.lastChild) {
            this._notifyBackgroundTab(tab);
          }
        } else {
          this._handleNewTab(tab);
        }
      } else if (tab.closing) {
        gBrowser._endRemoveTab(tab);
      }

      let evt = new CustomEvent("TabAnimationEnd", { bubbles: true });
      tab.dispatchEvent(evt);
    }

    on_dblclick(event) {
      // When the tabbar has an unified appearance with the titlebar
      // and menubar, a double-click in it should have the same behavior
      // as double-clicking the titlebar
      if (CustomTitlebar.enabled && !this.verticalMode) {
        return;
      }

      // Make sure it is the primary button, we are hitting our arrowscrollbox,
      // and we're not hitting the scroll buttons.
      if (
        event.button != 0 ||
        event.target != this.arrowScrollbox ||
        event.composedTarget.localName == "toolbarbutton"
      ) {
        return;
      }

      if (!this._blockDblClick) {
        BrowserCommands.openTab();
      }

      event.preventDefault();
    }

    on_click(event) {
      if (event.eventPhase == Event.CAPTURING_PHASE && event.button == 0) {
        /* Catches extra clicks meant for the in-tab close button.
         * Placed here to avoid leaking (a temporary handler added from the
         * in-tab close button binding would close over the tab and leak it
         * until the handler itself was removed). (bug 897751)
         *
         * The only sequence in which a second click event (i.e. dblclik)
         * can be dispatched on an in-tab close button is when it is shown
         * after the first click (i.e. the first click event was dispatched
         * on the tab). This happens when we show the close button only on
         * the active tab. (bug 352021)
         * The only sequence in which a third click event can be dispatched
         * on an in-tab close button is when the tab was opened with a
         * double click on the tabbar. (bug 378344)
         * In both cases, it is most likely that the close button area has
         * been accidentally clicked, therefore we do not close the tab.
         *
         * We don't want to ignore processing of more than one click event,
         * though, since the user might actually be repeatedly clicking to
         * close many tabs at once.
         */
        let target = event.originalTarget;
        if (target.classList.contains("tab-close-button")) {
          // We preemptively set this to allow the closing-multiple-tabs-
          // in-a-row case.
          if (this._blockDblClick) {
            target._ignoredCloseButtonClicks = true;
          } else if (event.detail > 1 && !target._ignoredCloseButtonClicks) {
            target._ignoredCloseButtonClicks = true;
            event.stopPropagation();
            return;
          } else {
            // Reset the "ignored click" flag
            target._ignoredCloseButtonClicks = false;
          }
        }

        /* Protects from close-tab-button errant doubleclick:
         * Since we're removing the event target, if the user
         * double-clicks the button, the dblclick event will be dispatched
         * with the tabbar as its event target (and explicit/originalTarget),
         * which treats that as a mouse gesture for opening a new tab.
         * In this context, we're manually blocking the dblclick event.
         */
        if (this._blockDblClick) {
          if (!("_clickedTabBarOnce" in this)) {
            this._clickedTabBarOnce = true;
            return;
          }
          delete this._clickedTabBarOnce;
          this._blockDblClick = false;
        }
      } else if (
        event.eventPhase == Event.BUBBLING_PHASE &&
        event.button == 1
      ) {
        let tab = event.target ? event.target.closest("tab") : null;
        if (tab) {
          if (tab.multiselected) {
            gBrowser.removeMultiSelectedTabs();
          } else {
            gBrowser.removeTab(tab, {
              animate: true,
              triggeringEvent: event,
            });
          }
        } else if (
          event.originalTarget.closest("scrollbox") &&
          !Services.prefs.getBoolPref(
            "widget.gtk.titlebar-action-middle-click-enabled"
          )
        ) {
          // Check whether the click
          // was dispatched on the open space of it.
          let visibleTabs = this.visibleTabs;
          let lastTab = visibleTabs.at(-1);
          let winUtils = window.windowUtils;
          let endOfTab =
            winUtils.getBoundsWithoutFlushing(lastTab)[
              (this.verticalMode && "bottom") ||
                (this.#rtlMode ? "left" : "right")
            ];
          if (
            (this.verticalMode && event.clientY > endOfTab) ||
            (!this.verticalMode &&
              (this.#rtlMode
                ? event.clientX < endOfTab
                : event.clientX > endOfTab))
          ) {
            BrowserCommands.openTab();
          }
        } else {
          return;
        }

        event.preventDefault();
        event.stopPropagation();
      }
    }

    on_keydown(event) {
      let { altKey, shiftKey } = event;
      let [accel, nonAccel] =
        AppConstants.platform == "macosx"
          ? [event.metaKey, event.ctrlKey]
          : [event.ctrlKey, event.metaKey];

      let keyComboForFocusedElement =
        !accel && !shiftKey && !altKey && !nonAccel;
      let keyComboForMove = accel && shiftKey && !altKey && !nonAccel;
      let keyComboForFocus = accel && !shiftKey && !altKey && !nonAccel;

      if (!keyComboForFocusedElement && !keyComboForMove && !keyComboForFocus) {
        return;
      }

      if (keyComboForFocusedElement) {
        let ariaFocusedItem = this.ariaFocusedItem;
        if (ariaFocusedItem && isTabGroupLabel(ariaFocusedItem)) {
          switch (event.keyCode) {
            case KeyEvent.DOM_VK_SPACE:
            case KeyEvent.DOM_VK_RETURN: {
              ariaFocusedItem.click();
              event.preventDefault();
              return;
            }
          }
        }
        // defer to the parent `on_keydown` handler
        MozElements.TabsBase.prototype.on_keydown.call(this, event);
      } else if (keyComboForMove) {
        switch (event.keyCode) {
          case KeyEvent.DOM_VK_UP:
            gBrowser.moveTabBackward();
            break;
          case KeyEvent.DOM_VK_DOWN:
            gBrowser.moveTabForward();
            break;
          case KeyEvent.DOM_VK_RIGHT:
            if (RTL_UI) {
              gBrowser.moveTabBackward();
            } else {
              gBrowser.moveTabForward();
            }
            break;
          case KeyEvent.DOM_VK_LEFT:
            if (RTL_UI) {
              gBrowser.moveTabForward();
            } else {
              gBrowser.moveTabBackward();
            }
            break;
          case KeyEvent.DOM_VK_HOME:
            gBrowser.moveTabToStart();
            break;
          case KeyEvent.DOM_VK_END:
            gBrowser.moveTabToEnd();
            break;
          default:
            // Consume the keydown event for the above keyboard
            // shortcuts only.
            return;
        }

        event.preventDefault();
      } else if (keyComboForFocus) {
        switch (event.keyCode) {
          case KeyEvent.DOM_VK_UP:
            this.#advanceFocus(DIRECTION_BACKWARD);
            break;
          case KeyEvent.DOM_VK_DOWN:
            this.#advanceFocus(DIRECTION_FORWARD);
            break;
          case KeyEvent.DOM_VK_RIGHT:
            if (RTL_UI) {
              this.#advanceFocus(DIRECTION_BACKWARD);
            } else {
              this.#advanceFocus(DIRECTION_FORWARD);
            }
            break;
          case KeyEvent.DOM_VK_LEFT:
            if (RTL_UI) {
              this.#advanceFocus(DIRECTION_FORWARD);
            } else {
              this.#advanceFocus(DIRECTION_BACKWARD);
            }
            break;
          case KeyEvent.DOM_VK_HOME:
            this.ariaFocusedItem = this.ariaFocusableItems.at(0);
            break;
          case KeyEvent.DOM_VK_END:
            this.ariaFocusedItem = this.ariaFocusableItems.at(-1);
            break;
          case KeyEvent.DOM_VK_SPACE: {
            let ariaFocusedItem = this.ariaFocusedItem;
            if (isTab(ariaFocusedItem)) {
              if (ariaFocusedItem.multiselected) {
                gBrowser.removeFromMultiSelectedTabs(ariaFocusedItem);
              } else {
                gBrowser.addToMultiSelectedTabs(ariaFocusedItem);
              }
            }
            break;
          }
          default:
            // Consume the keydown event for the above keyboard
            // shortcuts only.
            return;
        }

        event.preventDefault();
      }
    }

    /**
     * @param {FocusEvent} event
     */
    on_focusin(event) {
      if (event.target == this.selectedItem) {
        this.tablistHasFocus = true;
        if (!this.ariaFocusedItem) {
          // If the active tab is receiving focus and there isn't a keyboard
          // focus target yet, set the keyboard focus target to the active
          // tab. Do not override the keyboard-focused item if the user
          // already set a keyboard focus.
          this.ariaFocusedItem = this.selectedItem;
        }
      }
    }

    /**
     * @param {FocusEvent} event
     */
    on_focusout(event) {
      if (event.target == this.selectedItem) {
        this.tablistHasFocus = false;
      }
    }

    /**
     * Moves the focus in the tab strip left or right, as appropriate, to
     * the next tab or tab group label.
     *
     * @param {-1|1} direction
     */
    #advanceFocus(direction) {
      let currentIndex = this.ariaFocusableItems.indexOf(this.ariaFocusedItem);
      let newIndex = currentIndex + direction;

      // Clamp the index so that the focus stops at the edges of the tab strip
      newIndex = Math.min(
        this.ariaFocusableItems.length - 1,
        Math.max(0, newIndex)
      );

      let itemToFocus = this.ariaFocusableItems[newIndex];
      this.ariaFocusedItem = itemToFocus;
    }

    on_keypress(event) {
      if (event.defaultPrevented) {
        return;
      }
      if (event.key == " " || event.key == "Enter") {
        event.preventDefault();
        event.target.click();
      }
    }

    on_dragstart(event) {
      var tab = this._getDragTargetTab(event);
      if (!tab || this._isCustomizing) {
        return;
      }

      this.previewPanel?.deactivate();
      this.startTabDrag(event, tab);
    }

    startTabDrag(event, tab, { fromTabList = false } = {}) {
      if (this.#isContainerVerticalPinnedExpanded(tab)) {
        // In expanded vertical mode, the max number of pinned tabs per row is dynamic
        // Set this before adjusting dragged tab's position
        let pinnedTabs = this.visibleTabs.slice(0, gBrowser.pinnedTabCount);
        let tabsPerRow = 0;
        let position = 0;
        for (let pinnedTab of pinnedTabs) {
          let tabPosition =
            window.windowUtils.getBoundsWithoutFlushing(pinnedTab).left;
          if (tabPosition < position) {
            break;
          }
          tabsPerRow++;
          position = tabPosition;
        }
        this.#maxTabsPerRow = tabsPerRow;
      }

      let dataTransferOrderedTabs;
      if (!fromTabList) {
        let selectedTabs = gBrowser.selectedTabs;
        let otherSelectedTabs = selectedTabs.filter(
          selectedTab => selectedTab != tab
        );
        dataTransferOrderedTabs = [tab].concat(otherSelectedTabs);
      } else {
        // Dragging an item in the tabs list doesn't change the currently
        // selected tabs, and it's not possible to select multiple tabs from
        // the list, thus handle only the dragged tab in this case.
        dataTransferOrderedTabs = [tab];
      }

      let dt = event.dataTransfer;
      for (let i = 0; i < dataTransferOrderedTabs.length; i++) {
        let dtTab = dataTransferOrderedTabs[i];

        dt.mozSetDataAt(TAB_DROP_TYPE, dtTab, i);
        let dtBrowser = dtTab.linkedBrowser;

        // We must not set text/x-moz-url or text/plain data here,
        // otherwise trying to detach the tab by dropping it on the desktop
        // may result in an "internet shortcut"
        dt.mozSetDataAt(
          "text/x-moz-text-internal",
          dtBrowser.currentURI.spec,
          i
        );
      }

      // Set the cursor to an arrow during tab drags.
      dt.mozCursor = "default";

      // Set the tab as the source of the drag, which ensures we have a stable
      // node to deliver the `dragend` event.  See bug 1345473.
      dt.addElement(tab);

      if (tab.multiselected) {
        this.#moveTogetherSelectedTabs(tab);
      }

      // Create a canvas to which we capture the current tab.
      // Until canvas is HiDPI-aware (bug 780362), we need to scale the desired
      // canvas size (in CSS pixels) to the window's backing resolution in order
      // to get a full-resolution drag image for use on HiDPI displays.
      let scale = window.devicePixelRatio;
      let canvas = this._dndCanvas;
      if (!canvas) {
        this._dndCanvas = canvas = document.createElementNS(
          "http://www.w3.org/1999/xhtml",
          "canvas"
        );
        canvas.style.width = "100%";
        canvas.style.height = "100%";
        canvas.mozOpaque = true;
      }

      canvas.width = 160 * scale;
      canvas.height = 90 * scale;
      let toDrag = canvas;
      let dragImageOffset = -16;
      let browser = tab.linkedBrowser;
      if (gMultiProcessBrowser) {
        var context = canvas.getContext("2d");
        context.fillStyle = "white";
        context.fillRect(0, 0, canvas.width, canvas.height);

        let captureListener;
        let platform = AppConstants.platform;
        // On Windows and Mac we can update the drag image during a drag
        // using updateDragImage. On Linux, we can use a panel.
        if (platform == "win" || platform == "macosx") {
          captureListener = function () {
            dt.updateDragImage(canvas, dragImageOffset, dragImageOffset);
          };
        } else {
          // Create a panel to use it in setDragImage
          // which will tell xul to render a panel that follows
          // the pointer while a dnd session is on.
          if (!this._dndPanel) {
            this._dndCanvas = canvas;
            this._dndPanel = document.createXULElement("panel");
            this._dndPanel.className = "dragfeedback-tab";
            this._dndPanel.setAttribute("type", "drag");
            let wrapper = document.createElementNS(
              "http://www.w3.org/1999/xhtml",
              "div"
            );
            wrapper.style.width = "160px";
            wrapper.style.height = "90px";
            wrapper.appendChild(canvas);
            this._dndPanel.appendChild(wrapper);
            document.documentElement.appendChild(this._dndPanel);
          }
          toDrag = this._dndPanel;
        }
        // PageThumb is async with e10s but that's fine
        // since we can update the image during the dnd.
        PageThumbs.captureToCanvas(browser, canvas)
          .then(captureListener)
          .catch(e => console.error(e));
      } else {
        // For the non e10s case we can just use PageThumbs
        // sync, so let's use the canvas for setDragImage.
        PageThumbs.captureToCanvas(browser, canvas).catch(e =>
          console.error(e)
        );
        dragImageOffset = dragImageOffset * scale;
      }
      dt.setDragImage(toDrag, dragImageOffset, dragImageOffset);

      // _dragData.offsetX/Y give the coordinates that the mouse should be
      // positioned relative to the corner of the new window created upon
      // dragend such that the mouse appears to have the same position
      // relative to the corner of the dragged tab.
      let clientPos = ele => {
        const rect = ele.getBoundingClientRect();
        return this.verticalMode ? rect.top : rect.left;
      };

      let tabOffset = clientPos(tab) - clientPos(this);

      tab._dragData = {
        offsetX: this.verticalMode
          ? event.screenX - window.screenX
          : event.screenX - window.screenX - tabOffset,
        offsetY: this.verticalMode
          ? event.screenY - window.screenY - tabOffset
          : event.screenY - window.screenY,
        scrollPos:
          this.verticalMode && tab.pinned
            ? this.verticalPinnedTabsContainer.scrollTop
            : this.arrowScrollbox.scrollPosition,
        screenX: event.screenX,
        screenY: event.screenY,
        movingTabs: (tab.multiselected ? gBrowser.selectedTabs : [tab]).filter(
          t => t.pinned == tab.pinned
        ),
        fromTabList,
        tabGroupCreationColor: gBrowser.tabGroupMenu.nextUnusedColor,
      };

      event.stopPropagation();

      if (fromTabList) {
        Glean.browserUiInteraction.allTabsPanelDragstartTabEventCount.add(1);
      }
    }

    on_dragover(event) {
      var effects = this.getDropEffectForTabDrag(event);

      var ind = this._tabDropIndicator;
      if (effects == "" || effects == "none") {
        ind.hidden = true;
        return;
      }
      event.preventDefault();
      event.stopPropagation();

      var arrowScrollbox = this.arrowScrollbox;

      // autoscroll the tab strip if we drag over the scroll
      // buttons, even if we aren't dragging a tab, but then
      // return to avoid drawing the drop indicator
      var pixelsToScroll = 0;
      if (this.overflowing) {
        switch (event.originalTarget) {
          case arrowScrollbox._scrollButtonUp:
            pixelsToScroll = arrowScrollbox.scrollIncrement * -1;
            break;
          case arrowScrollbox._scrollButtonDown:
            pixelsToScroll = arrowScrollbox.scrollIncrement;
            break;
        }
        if (pixelsToScroll) {
          arrowScrollbox.scrollByPixels(
            (this.#rtlMode ? -1 : 1) * pixelsToScroll,
            true
          );
        }
      }

      let draggedTab = event.dataTransfer.mozGetDataAt(TAB_DROP_TYPE, 0);
      if (
        (effects == "move" || effects == "copy") &&
        this == draggedTab.container &&
        !draggedTab._dragData.fromTabList
      ) {
        ind.hidden = true;

        if (this.#isAnimatingMoveTogetherSelectedTabs()) {
          // Wait for moving selected tabs together animation to finish.
          return;
        }
        this._finishMoveTogetherSelectedTabs(draggedTab);

        if (effects == "move") {
          // Pinned tabs in expanded vertical mode are on a grid format and require
          // different logic to drag and drop.
          if (this.#isContainerVerticalPinnedExpanded(draggedTab)) {
            this.#animateExpandedPinnedTabMove(event);
            return;
          }
          this._animateTabMove(event);
          return;
        }
      }

      this._finishAnimateTabMove();

      if (effects == "link") {
        let tab = this._getDragTargetTab(event, { ignoreTabSides: true });
        if (tab) {
          if (!this._dragTime) {
            this._dragTime = Date.now();
          }
          if (Date.now() >= this._dragTime + this._dragOverDelay) {
            this.selectedItem = tab;
          }
          ind.hidden = true;
          return;
        }
      }

      var rect = arrowScrollbox.getBoundingClientRect();
      var newMargin;
      if (pixelsToScroll) {
        // if we are scrolling, put the drop indicator at the edge
        // so that it doesn't jump while scrolling
        let scrollRect = arrowScrollbox.scrollClientRect;
        let minMargin = this.verticalMode
          ? scrollRect.top - rect.top
          : scrollRect.left - rect.left;
        let maxMargin = this.verticalMode
          ? Math.min(minMargin + scrollRect.height, scrollRect.bottom)
          : Math.min(minMargin + scrollRect.width, scrollRect.right);
        if (this.#rtlMode) {
          [minMargin, maxMargin] = [
            this.clientWidth - maxMargin,
            this.clientWidth - minMargin,
          ];
        }
        newMargin = pixelsToScroll > 0 ? maxMargin : minMargin;
      } else {
        let newIndex = this._getDropIndex(event);
        let children = this.allTabs;
        if (newIndex == children.length) {
          let tabRect = this.visibleTabs.at(-1).getBoundingClientRect();
          if (this.verticalMode) {
            newMargin = tabRect.bottom - rect.top;
          } else if (this.#rtlMode) {
            newMargin = rect.right - tabRect.left;
          } else {
            newMargin = tabRect.right - rect.left;
          }
        } else {
          let tabRect = children[newIndex].getBoundingClientRect();
          if (this.verticalMode) {
            newMargin = rect.top - tabRect.bottom;
          } else if (this.#rtlMode) {
            newMargin = rect.right - tabRect.right;
          } else {
            newMargin = tabRect.left - rect.left;
          }
        }
      }

      ind.hidden = false;
      newMargin += this.verticalMode ? ind.clientHeight : ind.clientWidth / 2;
      if (this.#rtlMode) {
        newMargin *= -1;
      }
      ind.style.transform = this.verticalMode
        ? "translateY(" + Math.round(newMargin) + "px)"
        : "translateX(" + Math.round(newMargin) + "px)";
    }

    // eslint-disable-next-line complexity
    on_drop(event) {
      var dt = event.dataTransfer;
      var dropEffect = dt.dropEffect;
      var draggedTab;
      let movingTabs;
      if (dt.mozTypesAt(0)[0] == TAB_DROP_TYPE) {
        // tab copy or move
        draggedTab = dt.mozGetDataAt(TAB_DROP_TYPE, 0);
        // not our drop then
        if (!draggedTab) {
          return;
        }
        movingTabs = draggedTab._dragData.movingTabs;
        draggedTab.container._finishMoveTogetherSelectedTabs(draggedTab);
      }

      this._tabDropIndicator.hidden = true;
      event.stopPropagation();
      if (draggedTab && dropEffect == "copy") {
        // copy the dropped tab (wherever it's from)
        let newIndex = this._getDropIndex(event);
        let draggedTabCopy;
        for (let tab of movingTabs) {
          let newTab = gBrowser.duplicateTab(tab);
          gBrowser.moveTabTo(newTab, newIndex++);
          if (tab == draggedTab) {
            draggedTabCopy = newTab;
          }
        }
        if (draggedTab.container != this || event.shiftKey) {
          this.selectedItem = draggedTabCopy;
        }
      } else if (draggedTab && draggedTab.container == this) {
        let oldTranslateX = Math.round(draggedTab._dragData.translateX);
        let oldTranslateY = Math.round(draggedTab._dragData.translateY);
        let tabWidth = Math.round(draggedTab._dragData.tabWidth);
        let tabHeight = Math.round(draggedTab._dragData.tabHeight);
        let translateOffsetX = oldTranslateX % tabWidth;
        let translateOffsetY = oldTranslateY % tabHeight;
        let newTranslateX = oldTranslateX - translateOffsetX;
        let newTranslateY = oldTranslateY - translateOffsetY;

        if (this.#isContainerVerticalPinnedExpanded(draggedTab)) {
          // Update both translate axis for pinned vertical expanded tabs
          if (oldTranslateX > 0 && translateOffsetX > tabWidth / 2) {
            newTranslateX += tabWidth;
          } else if (oldTranslateX < 0 && -translateOffsetX > tabWidth / 2) {
            newTranslateX -= tabWidth;
          }
          if (oldTranslateY > 0 && translateOffsetY > tabHeight / 2) {
            newTranslateY += tabHeight;
          } else if (oldTranslateY < 0 && -translateOffsetY > tabHeight / 2) {
            newTranslateY -= tabHeight;
          }
        } else {
          let pinned = draggedTab.pinned;
          let numPinned = gBrowser.pinnedTabCount;
          let tabs = this.visibleTabs.slice(
            pinned ? 0 : numPinned,
            pinned ? numPinned : undefined
          );
          let size = this.verticalMode ? "height" : "width";
          let screenAxis = this.verticalMode ? "screenY" : "screenX";
          let tabSize = this.verticalMode ? tabHeight : tabWidth;
          let firstTab = tabs[0];
          let lastTab = tabs.at(-1);
          let lastMovingTabScreen = movingTabs.at(-1)[screenAxis];
          let firstMovingTabScreen = movingTabs[0][screenAxis];
          let firstBound = firstTab[screenAxis] - firstMovingTabScreen;
          let lastBound =
            lastTab[screenAxis] +
            window.windowUtils.getBoundsWithoutFlushing(lastTab)[size] -
            (lastMovingTabScreen + tabSize);

          if (this.verticalMode) {
            newTranslateY = Math.min(
              Math.max(oldTranslateY, firstBound),
              lastBound
            );
          } else {
            newTranslateX = Math.min(
              Math.max(oldTranslateX, firstBound),
              lastBound
            );
          }
        }

        let dropIndex;
        if (draggedTab._dragData.fromTabList) {
          dropIndex = this._getDropIndex(event);
        } else {
          dropIndex =
            "animDropIndex" in draggedTab._dragData &&
            draggedTab._dragData.animDropIndex;
        }
        let directionForward = false;
        let originalDropIndex = dropIndex;
        if (dropIndex && dropIndex > movingTabs[0]._tPos) {
          dropIndex--;
          directionForward = true;
        }

        const { groupDropAction, groupDropIndex } = draggedTab._dragData;

        let shouldTranslate =
          !gReduceMotion && groupDropAction != GROUP_DROP_ACTION_CREATE;
        if (this.#isContainerVerticalPinnedExpanded(draggedTab)) {
          shouldTranslate &&=
            (oldTranslateX && oldTranslateX != newTranslateX) ||
            (oldTranslateY && oldTranslateY != newTranslateY);
        } else if (this.verticalMode) {
          shouldTranslate &&= oldTranslateY && oldTranslateY != newTranslateY;
        } else {
          shouldTranslate &&= oldTranslateX && oldTranslateX != newTranslateX;
        }

        if (
          this.hasAttribute("movingtab-ungroup") &&
          dropIndex == this.allTabs.length - 1
        ) {
          // Allow a tab to ungroup if the last item in the tab strip
          // is a grouped tab.
          dropIndex++;
        }

        if (shouldTranslate) {
          if (groupDropAction == GROUP_DROP_ACTION_APPEND) {
            let groupTab = this.allTabs[groupDropIndex];
            groupTab.group.addTabs(movingTabs);
          } else {
            for (let tab of movingTabs) {
              tab.toggleAttribute("tabdrop-samewindow", true);
              tab.style.transform = `translate(${newTranslateX}px, ${newTranslateY}px)`;
              let postTransitionCleanup = () => {
                tab.removeAttribute("tabdrop-samewindow");

                this._finishAnimateTabMove();
                if (dropIndex !== false) {
                  gBrowser.moveTabTo(tab, dropIndex);
                  if (!directionForward) {
                    dropIndex++;
                  }
                }

                gBrowser.syncThrobberAnimations(tab);
              };
              if (gReduceMotion) {
                postTransitionCleanup();
              } else {
                let onTransitionEnd = transitionendEvent => {
                  if (
                    transitionendEvent.propertyName != "transform" ||
                    transitionendEvent.originalTarget != tab
                  ) {
                    return;
                  }
                  tab.removeEventListener("transitionend", onTransitionEnd);

                  postTransitionCleanup();
                };
                tab.addEventListener("transitionend", onTransitionEnd);
              }
            }
          }
        } else {
          this._finishAnimateTabMove();
          if (groupDropAction == GROUP_DROP_ACTION_APPEND) {
            let groupTab = this.allTabs[groupDropIndex];
            groupTab.group.addTabs(movingTabs);
          } else if (groupDropAction == GROUP_DROP_ACTION_CREATE) {
            let groupTab = this.allTabs[groupDropIndex];
            // If dropping on the forward edge of the `groupTab`, create the
            // tab group with `groupTab` as the first tab in the new tab group
            // followed by the dragged tabs.
            // If dropping on the backward edge of the `groupTab`, create the
            // tab group with the dragged tabs followed by `groupTab` as the
            // last tab in the new tab group.
            // This makes the tab group contents reflect the visual order of
            // the tabs right before dropping.
            let tabsInGroup =
              originalDropIndex <= groupTab._tPos
                ? [...movingTabs, groupTab]
                : [groupTab, ...movingTabs];
            gBrowser.addTabGroup(tabsInGroup, {
              insertBefore: groupTab,
              showCreateUI: true,
              color: draggedTab._dragData.tabGroupCreationColor,
            });
          } else if (dropIndex !== false) {
            for (let tab of movingTabs) {
              gBrowser.moveTabTo(tab, dropIndex);
              if (!directionForward) {
                dropIndex++;
              }
            }
          }
        }
      } else if (draggedTab) {
        // Move the tabs. To avoid multiple tab-switches in the original window,
        // the selected tab should be adopted last.
        const dropIndex = this._getDropIndex(event);
        let newIndex = dropIndex;
        let selectedTab;
        let indexForSelectedTab;
        for (let i = 0; i < movingTabs.length; ++i) {
          const tab = movingTabs[i];
          if (tab.selected) {
            selectedTab = tab;
            indexForSelectedTab = newIndex;
          } else {
            const newTab = gBrowser.adoptTab(tab, newIndex, tab == draggedTab);
            if (newTab) {
              ++newIndex;
            }
          }
        }
        if (selectedTab) {
          const newTab = gBrowser.adoptTab(
            selectedTab,
            indexForSelectedTab,
            selectedTab == draggedTab
          );
          if (newTab) {
            ++newIndex;
          }
        }

        // Restore tab selection
        gBrowser.addRangeToMultiSelectedTabs(
          gBrowser.tabs[dropIndex],
          gBrowser.tabs[newIndex - 1]
        );
      } else {
        // Pass true to disallow dropping javascript: or data: urls
        let links;
        try {
          links = browserDragAndDrop.dropLinks(event, true);
        } catch (ex) {}

        if (!links || links.length === 0) {
          return;
        }

        let inBackground = Services.prefs.getBoolPref(
          "browser.tabs.loadInBackground"
        );
        if (event.shiftKey) {
          inBackground = !inBackground;
        }

        let targetTab = this._getDragTargetTab(event, { ignoreTabSides: true });
        let userContextId = this.selectedItem.getAttribute("usercontextid");
        let replace = !!targetTab;
        let newIndex = this._getDropIndex(event);
        let urls = links.map(link => link.url);
        let csp = browserDragAndDrop.getCsp(event);
        let triggeringPrincipal =
          browserDragAndDrop.getTriggeringPrincipal(event);

        (async () => {
          if (
            urls.length >=
            Services.prefs.getIntPref("browser.tabs.maxOpenBeforeWarn")
          ) {
            // Sync dialog cannot be used inside drop event handler.
            let answer = await OpenInTabsUtils.promiseConfirmOpenInTabs(
              urls.length,
              window
            );
            if (!answer) {
              return;
            }
          }

          gBrowser.loadTabs(urls, {
            inBackground,
            replace,
            allowThirdPartyFixup: true,
            targetTab,
            newIndex,
            userContextId,
            triggeringPrincipal,
            csp,
          });
        })();
      }

      if (draggedTab) {
        delete draggedTab._dragData;
      }
    }

    on_dragend(event) {
      var dt = event.dataTransfer;
      var draggedTab = dt.mozGetDataAt(TAB_DROP_TYPE, 0);

      // Prevent this code from running if a tabdrop animation is
      // running since calling _finishAnimateTabMove would clear
      // any CSS transition that is running.
      if (draggedTab.hasAttribute("tabdrop-samewindow")) {
        return;
      }

      this._finishMoveTogetherSelectedTabs(draggedTab);
      this._finishAnimateTabMove();

      if (
        dt.mozUserCancelled ||
        dt.dropEffect != "none" ||
        this._isCustomizing
      ) {
        delete draggedTab._dragData;
        return;
      }

      // Check if tab detaching is enabled
      if (!Services.prefs.getBoolPref("browser.tabs.allowTabDetach")) {
        return;
      }

      // Disable detach within the browser toolbox
      let [tabAxisPos, tabAxisStart, tabAxisEnd] = this.verticalMode
        ? [event.screenY, window.screenY, window.screenY + window.outerHeight]
        : [event.screenX, window.screenX, window.screenX + window.outerWidth];

      if (tabAxisPos > tabAxisStart && tabAxisPos < tabAxisEnd) {
        // also avoid detaching if the the tab was dropped too close to
        // the tabbar (half a tab)
        let rect = window.windowUtils.getBoundsWithoutFlushing(
          this.arrowScrollbox
        );
        let crossAxisPos = this.verticalMode ? event.screenX : event.screenY;
        let crossAxisStart, crossAxisEnd;
        if (this.verticalMode) {
          if (RTL_UI) {
            crossAxisStart = window.screenX + rect.right - 1.5 * rect.width;
            crossAxisEnd = window.screenX;
          } else {
            crossAxisStart = window.screenX;
            crossAxisEnd = window.screenX + rect.left + 1.5 * rect.width;
          }
        } else {
          crossAxisStart = window.screenY;
          crossAxisEnd = window.screenY + rect.top + 1.5 * rect.height;
        }
        if (crossAxisPos > crossAxisStart && crossAxisPos < crossAxisEnd) {
          return;
        }
      }

      // screen.availLeft et. al. only check the screen that this window is on,
      // but we want to look at the screen the tab is being dropped onto.
      var screen = event.screen;
      var availX = {},
        availY = {},
        availWidth = {},
        availHeight = {};
      // Get available rect in desktop pixels.
      screen.GetAvailRectDisplayPix(availX, availY, availWidth, availHeight);
      availX = availX.value;
      availY = availY.value;
      availWidth = availWidth.value;
      availHeight = availHeight.value;

      // Compute the final window size in desktop pixels ensuring that the new
      // window entirely fits within `screen`.
      let ourCssToDesktopScale =
        window.devicePixelRatio / window.desktopToDeviceScale;
      let screenCssToDesktopScale =
        screen.defaultCSSScaleFactor / screen.contentsScaleFactor;

      // NOTE(emilio): Multiplying the sizes here for screenCssToDesktopScale
      // means that we'll try to create a window that has the same amount of CSS
      // pixels than our current window, not the same amount of device pixels.
      // There are pros and cons of both conversions, though this matches the
      // pre-existing intended behavior.
      var winWidth = Math.min(
        window.outerWidth * screenCssToDesktopScale,
        availWidth
      );
      var winHeight = Math.min(
        window.outerHeight * screenCssToDesktopScale,
        availHeight
      );

      // This is slightly tricky: _dragData.offsetX/Y is an offset in CSS
      // pixels. Since we're doing the sizing above based on those, we also need
      // to apply the offset with pixels relative to the screen's scale rather
      // than our scale.
      var left = Math.min(
        Math.max(
          event.screenX * ourCssToDesktopScale -
            draggedTab._dragData.offsetX * screenCssToDesktopScale,
          availX
        ),
        availX + availWidth - winWidth
      );
      var top = Math.min(
        Math.max(
          event.screenY * ourCssToDesktopScale -
            draggedTab._dragData.offsetY * screenCssToDesktopScale,
          availY
        ),
        availY + availHeight - winHeight
      );

      // Convert back left and top to our CSS pixel space.
      left /= ourCssToDesktopScale;
      top /= ourCssToDesktopScale;

      delete draggedTab._dragData;

      if (gBrowser.tabs.length == 1) {
        // resize _before_ move to ensure the window fits the new screen.  if
        // the window is too large for its screen, the window manager may do
        // automatic repositioning.
        //
        // Since we're resizing before moving to our new screen, we need to use
        // sizes relative to the current screen. If we moved, then resized, then
        // we could avoid this special-case and share this with the else branch
        // below...
        winWidth /= ourCssToDesktopScale;
        winHeight /= ourCssToDesktopScale;

        window.resizeTo(winWidth, winHeight);
        window.moveTo(left, top);
        window.focus();
      } else {
        // We're opening a new window in a new screen, so make sure to use sizes
        // relative to the new screen.
        winWidth /= screenCssToDesktopScale;
        winHeight /= screenCssToDesktopScale;

        let props = { screenX: left, screenY: top, suppressanimation: 1 };
        if (AppConstants.platform != "win") {
          props.outerWidth = winWidth;
          props.outerHeight = winHeight;
        }
        gBrowser.replaceTabsWithWindow(draggedTab, props);
      }
      event.stopPropagation();
    }

    on_dragleave(event) {
      this._dragTime = 0;

      // This does not work at all (see bug 458613)
      var target = event.relatedTarget;
      while (target && target != this) {
        target = target.parentNode;
      }
      if (target) {
        return;
      }

      this._tabDropIndicator.hidden = true;
      event.stopPropagation();
    }

    on_wheel(event) {
      if (
        Services.prefs.getBoolPref("toolkit.tabbox.switchByScrolling", false)
      ) {
        event.stopImmediatePropagation();
      }
    }

    on_overflow(event) {
      // Ignore overflow events from nested scrollable elements
      if (event.target != this.arrowScrollbox) {
        return;
      }

      this.toggleAttribute("overflow", true);
      this._positionPinnedTabs();
      this._updateCloseButtons();
      this._handleTabSelect(true);

      document
        .getElementById("tab-preview-panel")
        ?.setAttribute("rolluponmousewheel", true);
    }

    on_underflow(event) {
      // Ignore underflow events:
      // - from nested scrollable elements
      // - corresponding to an overflow event that we ignored
      if (event.target != this.arrowScrollbox || !this.overflowing) {
        return;
      }

      this.removeAttribute("overflow");

      if (this._lastTabClosedByMouse) {
        this._expandSpacerBy(this._scrollButtonWidth);
      }

      for (let tab of gBrowser._removingTabs) {
        gBrowser.removeTab(tab);
      }

      this._positionPinnedTabs();
      this._updateCloseButtons();

      document
        .getElementById("tab-preview-panel")
        ?.removeAttribute("rolluponmousewheel");
    }

    get emptyTabTitle() {
      // Normal tab title is used also in the permanent private browsing mode.
      const l10nId =
        PrivateBrowsingUtils.isWindowPrivate(window) &&
        !Services.prefs.getBoolPref("browser.privatebrowsing.autostart")
          ? "tabbrowser-empty-private-tab-title"
          : "tabbrowser-empty-tab-title";
      return gBrowser.tabLocalization.formatValueSync(l10nId);
    }

    get tabbox() {
      return document.getElementById("tabbrowser-tabbox");
    }

    get newTabButton() {
      return this.querySelector("#tabs-newtab-button");
    }

    get verticalMode() {
      return this.getAttribute("orient") == "vertical";
    }

    get #rtlMode() {
      return !this.verticalMode && RTL_UI;
    }

    get overflowing() {
      return this.hasAttribute("overflow");
    }

    #allTabs;
    get allTabs() {
      if (this.#allTabs) {
        return this.#allTabs;
      }
      let children = Array.from(this.arrowScrollbox.children);
      // remove arrowScrollbox periphery element
      children.pop();

      // explode tab groups
      // Iterate backwards over the array to preserve indices while we modify
      // things in place
      for (let i = children.length - 1; i >= 0; i--) {
        if (children[i].tagName == "tab-group") {
          children.splice(i, 1, ...children[i].tabs);
        }
      }

      this.#allTabs = [
        ...this.verticalPinnedTabsContainer.children,
        ...children,
      ];
      return this.#allTabs;
    }

    get allGroups() {
      let children = Array.from(this.arrowScrollbox.children);
      return children.filter(node => node.tagName == "tab-group");
    }

    /**
     * Returns all tabs in the current window, including hidden tabs and tabs
     * in collapsed groups, but excluding closing tabs and the Firefox View tab.
     */
    get openTabs() {
      if (!this.#openTabs) {
        this.#openTabs = this.allTabs.filter(tab => tab.isOpen);
      }
      return this.#openTabs;
    }
    #openTabs;

    /**
     * Same as `openTabs` but excluding hidden tabs and tabs in collapsed groups.
     */
    get visibleTabs() {
      if (!this.#visibleTabs) {
        this.#visibleTabs = this.openTabs.filter(tab => tab.visible);
      }
      return this.#visibleTabs;
    }
    #visibleTabs;

    /**
     * @returns {boolean} true if the keyboard focus is on the active tab
     */
    get tablistHasFocus() {
      return this.hasAttribute("tablist-has-focus");
    }

    /**
     * @param {boolean} hasFocus true if the keyboard focus is on the active tab
     */
    set tablistHasFocus(hasFocus) {
      this.toggleAttribute("tablist-has-focus", hasFocus);
    }

    /** @typedef {MozTabbrowserTab|MozTextLabel} FocusableItem */

    /** @type {FocusableItem[]} */
    #focusableItems;

    /**
     * @returns {FocusableItem[]}
     * @override tabbox.js:TabsBase
     */
    get ariaFocusableItems() {
      if (this.#focusableItems) {
        return this.#focusableItems;
      }

      let verticalPinnedTabsContainer = document.getElementById(
        "vertical-pinned-tabs-container"
      );
      let children = Array.from(this.arrowScrollbox.children);

      let focusableItems = [];
      for (let child of children) {
        if (isTab(child) && child.visible) {
          focusableItems.push(child);
        } else if (isTabGroup(child)) {
          focusableItems.push(child.labelElement);
          if (!child.collapsed) {
            let visibleTabsInGroup = child.tabs.filter(tab => tab.visible);
            focusableItems.push(...visibleTabsInGroup);
          }
        }
      }

      this.#focusableItems = [
        ...verticalPinnedTabsContainer.children,
        ...focusableItems,
      ];

      return this.#focusableItems;
    }

    _invalidateCachedTabs() {
      this.#allTabs = null;
      this._invalidateCachedVisibleTabs();
    }

    _invalidateCachedVisibleTabs() {
      this.#openTabs = null;
      this.#visibleTabs = null;
      // Focusable items must also be visible, but they do not depend on
      // this.#visibleTabs, so changes to visible tabs need to also invalidate
      // the focusable items cache
      this.#focusableItems = null;
    }

    #isContainerVerticalPinnedExpanded(tab) {
      return (
        this.verticalMode &&
        tab.hasAttribute("pinned") &&
        this.hasAttribute("expanded")
      );
    }

    appendChild(tab) {
      return this.insertBefore(tab, null);
    }

    insertBefore(tab, node) {
      if (!this.arrowScrollbox) {
        throw new Error("Shouldn't call this without arrowscrollbox");
      }

      if (node == null) {
        // We have a container for non-tab elements at the end of the scrollbox.
        node = this.arrowScrollbox.lastChild;
      }

      node.before(tab);

      if (this.#mustUpdateTabMinHeight) {
        this.#updateTabMinHeight();
      }
    }

    #updateTabMinWidth(val) {
      const minWidthVariable = "--tab-min-width";
      if (this.verticalMode) {
        this.style.removeProperty(minWidthVariable);
      } else {
        this.style.setProperty(
          minWidthVariable,
          (val ?? this._tabMinWidthPref) + "px"
        );
      }
    }

    #updateTabMinHeight() {
      if (!this.verticalMode || !window.toolbar.visible) {
        this.#mustUpdateTabMinHeight = false;
        return;
      }

      // Find at least one tab we can scroll to.
      let firstScrollableTab = this.visibleTabs.find(
        this.arrowScrollbox._canScrollToElement
      );

      if (!firstScrollableTab) {
        // If not, we're in a pickle. We should never get here except if we
        // also don't use the outcome of this work (because there's nothing to
        // scroll so we don't care about the scrollbox size).
        // So just set a flag so we re-run once we do have a new tab.
        this.#mustUpdateTabMinHeight = true;
        return;
      }

      let { height } =
        window.windowUtils.getBoundsWithoutFlushing(firstScrollableTab);

      // Use the current known height or a sane default.
      this.#tabMinHeight = height || 36;

      // The height we got may be incorrect if a flush is pending so re-check it after
      // a flush completes.
      window
        .promiseDocumentFlushed(() => {})
        .then(
          () => {
            height =
              window.windowUtils.getBoundsWithoutFlushing(
                firstScrollableTab
              ).height;

            if (height) {
              this.#tabMinHeight = height;
            }
          },
          () => {
            /* ignore errors */
          }
        );
    }

    get _isCustomizing() {
      return document.documentElement.hasAttribute("customizing");
    }

    // This overrides the TabsBase _selectNewTab method so that we can
    // potentially interrupt keyboard tab switching when sharing the
    // window or screen.
    _selectNewTab(aNewTab, aFallbackDir, aWrap) {
      if (!gSharedTabWarning.willShowSharedTabWarning(aNewTab)) {
        super._selectNewTab(aNewTab, aFallbackDir, aWrap);
      }
    }

    observe(aSubject, aTopic) {
      switch (aTopic) {
        case "nsPref:changed":
          // This is has to deal with changes in
          // privacy.userContext.enabled and
          // privacy.userContext.newTabContainerOnLeftClick.enabled.
          let containersEnabled =
            Services.prefs.getBoolPref("privacy.userContext.enabled") &&
            !PrivateBrowsingUtils.isWindowPrivate(window);

          // This pref won't change so often, so just recreate the menu.
          const newTabLeftClickOpensContainersMenu = Services.prefs.getBoolPref(
            "privacy.userContext.newTabContainerOnLeftClick.enabled"
          );

          // There are separate "new tab" buttons for horizontal tabs toolbar, vertical tabs and
          // for when the tab strip is overflowed (which is shared by vertical and horizontal tabs);
          // Attach the long click popup to all of them.
          const newTab = document.getElementById("new-tab-button");
          const newTab2 = this.newTabButton;
          const newTabVertical = document.getElementById(
            "vertical-tabs-newtab-button"
          );

          for (let parent of [newTab, newTab2, newTabVertical]) {
            if (!parent) {
              continue;
            }

            parent.removeAttribute("type");
            if (parent.menupopup) {
              parent.menupopup.remove();
            }

            if (containersEnabled) {
              parent.setAttribute("context", "new-tab-button-popup");

              let popup = document
                .getElementById("new-tab-button-popup")
                .cloneNode(true);
              popup.removeAttribute("id");
              popup.className = "new-tab-popup";
              popup.setAttribute("position", "after_end");
              popup.addEventListener("popupshowing", CreateContainerTabMenu);
              parent.prepend(popup);
              parent.setAttribute("type", "menu");
              // Update tooltip text
              nodeToTooltipMap[parent.id] = newTabLeftClickOpensContainersMenu
                ? "newTabAlwaysContainer.tooltip"
                : "newTabContainer.tooltip";
            } else {
              nodeToTooltipMap[parent.id] = "newTabButton.tooltip";
              parent.removeAttribute("context", "new-tab-button-popup");
            }
            // evict from tooltip cache
            gDynamicTooltipCache.delete(parent.id);

            // If containers and press-hold container menu are both used,
            // add to gClickAndHoldListenersOnElement; otherwise, remove.
            if (containersEnabled && !newTabLeftClickOpensContainersMenu) {
              gClickAndHoldListenersOnElement.add(parent);
            } else {
              gClickAndHoldListenersOnElement.remove(parent);
            }
          }

          break;
      }
    }

    _updateCloseButtons() {
      if (this.overflowing) {
        // Tabs are at their minimum widths.
        this.setAttribute("closebuttons", "activetab");
        return;
      }

      if (this._closeButtonsUpdatePending) {
        return;
      }
      this._closeButtonsUpdatePending = true;

      // Wait until after the next paint to get current layout data from
      // getBoundsWithoutFlushing.
      window.requestAnimationFrame(() => {
        window.requestAnimationFrame(() => {
          this._closeButtonsUpdatePending = false;

          // The scrollbox may have started overflowing since we checked
          // overflow earlier, so check again.
          if (this.overflowing) {
            this.setAttribute("closebuttons", "activetab");
            return;
          }

          // Check if tab widths are below the threshold where we want to
          // remove close buttons from background tabs so that people don't
          // accidentally close tabs by selecting them.
          let rect = ele => {
            return window.windowUtils.getBoundsWithoutFlushing(ele);
          };
          let tab = this.visibleTabs[gBrowser.pinnedTabCount];
          if (tab && rect(tab).width <= this._tabClipWidth) {
            this.setAttribute("closebuttons", "activetab");
          } else {
            this.removeAttribute("closebuttons");
          }
        });
      });
    }

    _handleTabSelect(aInstant) {
      let selectedTab = this.selectedItem;
      if (this.overflowing) {
        this.arrowScrollbox.ensureElementIsVisible(selectedTab, aInstant);
      }

      selectedTab._notselectedsinceload = false;
    }

    /**
     * Try to keep the active tab's close button under the mouse cursor
     */
    _lockTabSizing(aTab, aTabWidth) {
      if (this.verticalMode) {
        return;
      }

      let tabs = this.visibleTabs;
      if (!tabs.length) {
        return;
      }

      var isEndTab = aTab._tPos > tabs.at(-1)._tPos;

      if (!this._tabDefaultMaxWidth) {
        this._tabDefaultMaxWidth = parseFloat(
          window.getComputedStyle(aTab).maxWidth
        );
      }
      this._lastTabClosedByMouse = true;
      this._scrollButtonWidth = window.windowUtils.getBoundsWithoutFlushing(
        this.arrowScrollbox._scrollButtonDown
      ).width;

      if (this.overflowing) {
        // Don't need to do anything if we're in overflow mode and aren't scrolled
        // all the way to the right, or if we're closing the last tab.
        if (isEndTab || !this.arrowScrollbox.hasAttribute("scrolledtoend")) {
          return;
        }
        // If the tab has an owner that will become the active tab, the owner will
        // be to the left of it, so we actually want the left tab to slide over.
        // This can't be done as easily in non-overflow mode, so we don't bother.
        if (aTab.owner) {
          return;
        }
        this._expandSpacerBy(aTabWidth);
      } else {
        // non-overflow mode
        // Locking is neither in effect nor needed, so let tabs expand normally.
        if (isEndTab && !this._hasTabTempMaxWidth) {
          return;
        }
        let numPinned = gBrowser.pinnedTabCount;
        // Force tabs to stay the same width, unless we're closing the last tab,
        // which case we need to let them expand just enough so that the overall
        // tabbar width is the same.
        if (isEndTab) {
          let numNormalTabs = tabs.length - numPinned;
          aTabWidth = (aTabWidth * (numNormalTabs + 1)) / numNormalTabs;
          if (aTabWidth > this._tabDefaultMaxWidth) {
            aTabWidth = this._tabDefaultMaxWidth;
          }
        }
        aTabWidth += "px";
        let tabsToReset = [];
        for (let i = numPinned; i < tabs.length; i++) {
          let tab = tabs[i];
          tab.style.setProperty("max-width", aTabWidth, "important");
          if (!isEndTab) {
            // keep tabs the same width
            tab.style.transition = "none";
            tabsToReset.push(tab);
          }
        }

        if (tabsToReset.length) {
          window
            .promiseDocumentFlushed(() => {})
            .then(() => {
              window.requestAnimationFrame(() => {
                for (let tab of tabsToReset) {
                  tab.style.transition = "";
                }
              });
            });
        }

        this._hasTabTempMaxWidth = true;
        gBrowser.addEventListener("mousemove", this);
        window.addEventListener("mouseout", this);
      }
    }

    _expandSpacerBy(pixels) {
      let spacer = this._closingTabsSpacer;
      spacer.style.width = parseFloat(spacer.style.width) + pixels + "px";
      this.toggleAttribute("using-closing-tabs-spacer", true);
      gBrowser.addEventListener("mousemove", this);
      window.addEventListener("mouseout", this);
    }

    _unlockTabSizing() {
      gBrowser.removeEventListener("mousemove", this);
      window.removeEventListener("mouseout", this);

      if (this._hasTabTempMaxWidth) {
        this._hasTabTempMaxWidth = false;
        // Only visible tabs have their sizes locked, but those visible tabs
        // could become invisible before being unlocked (e.g. by being inside
        // of a collapsing tab group), so it's better to reset all tabs.
        let tabs = this.allTabs;
        for (let i = 0; i < tabs.length; i++) {
          tabs[i].style.maxWidth = "";
        }
      }

      if (this.hasAttribute("using-closing-tabs-spacer")) {
        this.removeAttribute("using-closing-tabs-spacer");
        this._closingTabsSpacer.style.width = 0;
      }
    }

    uiDensityChanged() {
      this._positionPinnedTabs();
      this._updateCloseButtons();
      this.#updateTabMinHeight();
      this._handleTabSelect(true);
    }

    _updateVerticalPinnedTabs() {
      // Move pinned tabs to another container when the tabstrip is toggled to vertical
      // and when session restore code calls _positionPinnedTabs; update styling whenever
      // the number of pinned tabs changes.
      let verticalTabsContainer = document.getElementById(
        "vertical-pinned-tabs-container"
      );
      let numPinned = gBrowser.pinnedTabCount;

      if (gBrowser.pinnedTabCount !== verticalTabsContainer.children.length) {
        let tabs = this.visibleTabs;
        for (let i = 0; i < numPinned; i++) {
          tabs[i].style.marginInlineStart = "";
          verticalTabsContainer.appendChild(tabs[i]);
        }
      }

      this.style.removeProperty("--tab-overflow-pinned-tabs-width");
    }

    _resetVerticalPinnedTabs() {
      let verticalTabsContainer = document.getElementById(
        "vertical-pinned-tabs-container"
      );

      if (!verticalTabsContainer.children.length) {
        return;
      }
      for (const child of Array.from(
        verticalTabsContainer.children
      ).reverse()) {
        this.arrowScrollbox.prepend(child);
      }
    }

    _positionPinnedTabs() {
      let tabs = this.visibleTabs;
      let numPinned = gBrowser.pinnedTabCount;
      let absPositionHorizontalTabs =
        this.overflowing && tabs.length > numPinned && numPinned > 0;

      this.toggleAttribute("haspinnedtabs", !!numPinned);
      this.toggleAttribute("positionpinnedtabs", absPositionHorizontalTabs);

      if (this.verticalMode) {
        this._updateVerticalPinnedTabs();
      } else if (absPositionHorizontalTabs) {
        let layoutData = this._pinnedTabsLayoutCache;
        let uiDensity = document.documentElement.getAttribute("uidensity");
        if (!layoutData || layoutData.uiDensity != uiDensity) {
          let arrowScrollbox = this.arrowScrollbox;
          layoutData = this._pinnedTabsLayoutCache = {
            uiDensity,
            pinnedTabWidth: tabs[0].getBoundingClientRect().width,
            scrollStartOffset:
              arrowScrollbox.scrollbox.getBoundingClientRect().left -
              arrowScrollbox.getBoundingClientRect().left +
              parseFloat(
                getComputedStyle(arrowScrollbox.scrollbox).paddingInlineStart
              ),
          };
        }

        let width = 0;
        for (let i = numPinned - 1; i >= 0; i--) {
          let tab = tabs[i];
          width += layoutData.pinnedTabWidth;
          tab.style.setProperty(
            "margin-inline-start",
            -(width + layoutData.scrollStartOffset) + "px",
            "important"
          );
        }
        this.style.setProperty(
          "--tab-overflow-pinned-tabs-width",
          width + "px"
        );
      } else {
        for (let i = 0; i < numPinned; i++) {
          let tab = tabs[i];
          tab.style.marginInlineStart = "";
        }

        this.style.removeProperty("--tab-overflow-pinned-tabs-width");
      }

      if (this._lastNumPinned != numPinned) {
        this._lastNumPinned = numPinned;
        this._handleTabSelect(true);
      }
    }

    #animateExpandedPinnedTabMove(event) {
      let draggedTab = event.dataTransfer.mozGetDataAt(TAB_DROP_TYPE, 0);
      let dragData = draggedTab._dragData;
      let movingTabs = dragData.movingTabs;

      if (!this.hasAttribute("movingtab")) {
        this.toggleAttribute("movingtab", true);
        gNavToolbox.toggleAttribute("movingtab", true);
        if (!draggedTab.multiselected) {
          this.selectedItem = draggedTab;
        }
      }

      dragData.animLastScreenX ??= dragData.screenX;
      dragData.animLastScreenY ??= dragData.screenY;

      let screenX = event.screenX;
      let screenY = event.screenY;

      if (
        screenY == dragData.animLastScreenY &&
        screenX == dragData.animLastScreenX
      ) {
        return;
      }

      let tabs = this.visibleTabs.slice(0, gBrowser.pinnedTabCount);

      let directionX = screenX > dragData.animLastScreenX;
      let directionY = screenY > dragData.animLastScreenY;
      dragData.animLastScreenY = screenY;
      dragData.animLastScreenX = screenX;

      let { width: tabWidth, height: tabHeight } =
        draggedTab.getBoundingClientRect();
      let shiftSizeX = tabWidth * movingTabs.length;
      let shiftSizeY = tabHeight;
      dragData.tabWidth = tabWidth;
      dragData.tabHeight = tabHeight;

      // Move the dragged tab based on the mouse position.
      let firstTabInRow;
      let lastTabInRow;
      let lastTab = tabs.at(-1);
      if (RTL_UI) {
        firstTabInRow =
          tabs.length >= this.#maxTabsPerRow
            ? tabs[this.#maxTabsPerRow - 1]
            : lastTab;
        lastTabInRow = tabs[0];
      } else {
        firstTabInRow = tabs[0];
        lastTabInRow =
          tabs.length >= this.#maxTabsPerRow
            ? tabs[this.#maxTabsPerRow - 1]
            : lastTab;
      }
      let lastMovingTabScreenX = movingTabs.at(-1).screenX;
      let lastMovingTabScreenY = movingTabs.at(-1).screenY;
      let firstMovingTabScreenX = movingTabs[0].screenX;
      let firstMovingTabScreenY = movingTabs[0].screenY;
      let translateX = screenX - dragData.screenX;
      let translateY = screenY - dragData.screenY;
      translateY +=
        this.verticalPinnedTabsContainer.scrollTop - dragData.scrollPos;
      let firstBoundX = firstTabInRow.screenX - firstMovingTabScreenX;
      let firstBoundY = firstTabInRow.screenY - firstMovingTabScreenY;
      let lastBoundX =
        lastTabInRow.screenX +
        lastTabInRow.getBoundingClientRect().width -
        (lastMovingTabScreenX + tabWidth);
      let lastBoundY =
        lastTab.screenY +
        lastTab.getBoundingClientRect().height -
        (lastMovingTabScreenY + tabHeight);
      translateX = Math.min(Math.max(translateX, firstBoundX), lastBoundX);
      translateY = Math.min(Math.max(translateY, firstBoundY), lastBoundY);

      for (let tab of movingTabs) {
        tab.style.transform = `translate(${translateX}px, ${translateY}px)`;
      }

      dragData.translateX = translateX;
      dragData.translateY = translateY;

      // Determine what tab we're dragging over.
      // * Single tab dragging: Point of reference is the center of the dragged tab. If that
      //   point touches a background tab, the dragged tab would take that
      //   tab's position when dropped.
      // * Multiple tabs dragging: All dragged tabs are one "giant" tab with two
      //   points of reference (center of tabs on the extremities). When
      //   mouse is moving from top to bottom, the bottom reference gets activated,
      //   otherwise the top reference will be used. Everything else works the same
      //   as single tab dragging.
      // * We're doing a binary search in order to reduce the amount of
      //   tabs we need to check.

      tabs = tabs.filter(t => !movingTabs.includes(t) || t == draggedTab);
      let firstTabCenterX = firstMovingTabScreenX + translateX + tabWidth / 2;
      let lastTabCenterX = lastMovingTabScreenX + translateX + tabWidth / 2;
      let tabCenterX = directionX ? lastTabCenterX : firstTabCenterX;
      let firstTabCenterY = firstMovingTabScreenY + translateY + tabHeight / 2;
      let lastTabCenterY = lastMovingTabScreenY + translateY + tabHeight / 2;
      let tabCenterY = directionY ? lastTabCenterY : firstTabCenterY;

      let shiftNumber = this.#maxTabsPerRow - movingTabs.length;

      let getTabShift = (tab, dropIndex) => {
        if (tab._tPos < draggedTab._tPos && tab._tPos >= dropIndex) {
          // If tab is at the end of a row, shift back and down
          let tabRow = Math.ceil((tab._tPos + 1) / this.#maxTabsPerRow);
          let shiftedTabRow = Math.ceil(
            (tab._tPos + 1 + movingTabs.length) / this.#maxTabsPerRow
          );
          if (tab._tPos && tabRow != shiftedTabRow) {
            return [
              RTL_UI ? tabWidth * shiftNumber : -tabWidth * shiftNumber,
              shiftSizeY,
            ];
          }
          return [RTL_UI ? -shiftSizeX : shiftSizeX, 0];
        }
        if (tab._tPos > draggedTab._tPos && tab._tPos < dropIndex) {
          // If tab is not index 0 and at the start of a row, shift across and up
          let tabRow = Math.floor(tab._tPos / this.#maxTabsPerRow);
          let shiftedTabRow = Math.floor(
            (tab._tPos - movingTabs.length) / this.#maxTabsPerRow
          );
          if (tab._tPos && tabRow != shiftedTabRow) {
            return [
              RTL_UI ? -tabWidth * shiftNumber : tabWidth * shiftNumber,
              -shiftSizeY,
            ];
          }
          return [RTL_UI ? shiftSizeX : -shiftSizeX, 0];
        }
        return [0, 0];
      };

      let low = 0;
      let high = tabs.length - 1;
      let newIndex = -1;
      let oldIndex = dragData.animDropIndex ?? movingTabs[0]._tPos;
      while (low <= high) {
        let mid = Math.floor((low + high) / 2);
        if (tabs[mid] == draggedTab && ++mid > high) {
          break;
        }
        let [shiftX, shiftY] = getTabShift(tabs[mid], oldIndex);
        screenX = tabs[mid].screenX + shiftX;
        screenY = tabs[mid].screenY + shiftY;

        if (screenY + tabHeight < tabCenterY) {
          low = mid + 1;
        } else if (screenY > tabCenterY) {
          high = mid - 1;
        } else if (screenX > tabCenterX) {
          high = mid - 1;
        } else if (screenX + tabWidth < tabCenterX) {
          low = mid + 1;
        } else {
          newIndex = tabs[mid]._tPos;
          break;
        }
      }

      if (newIndex >= oldIndex) {
        newIndex++;
      }

      if (newIndex < 0 || newIndex == oldIndex) {
        return;
      }
      dragData.animDropIndex = newIndex;

      // Shift background tabs to leave a gap where the dragged tab
      // would currently be dropped.
      for (let tab of tabs) {
        if (tab != draggedTab) {
          let [shiftX, shiftY] = getTabShift(tab, newIndex);
          tab.style.transform =
            shiftX || shiftY ? `translate(${shiftX}px, ${shiftY}px)` : "";
        }
      }
    }

    _animateTabMove(event) {
      let draggedTab = event.dataTransfer.mozGetDataAt(TAB_DROP_TYPE, 0);
      let dragData = draggedTab._dragData;
      let movingTabs = dragData.movingTabs;

      if (!this.hasAttribute("movingtab")) {
        this.toggleAttribute("movingtab", true);
        gNavToolbox.toggleAttribute("movingtab", true);
        if (!draggedTab.multiselected) {
          this.selectedItem = draggedTab;
        }
      }

      dragData.animLastScreenPos ??= this.verticalMode
        ? dragData.screenY
        : dragData.screenX;

      let screen = this.verticalMode ? event.screenY : event.screenX;
      if (screen == dragData.animLastScreenPos) {
        return;
      }

      let pinned = draggedTab.pinned;
      let numPinned = gBrowser.pinnedTabCount;
      let tabs = this.visibleTabs.slice(
        pinned ? 0 : numPinned,
        pinned ? numPinned : undefined
      );

      if (this.#rtlMode) {
        tabs.reverse();
        // Copy moving tabs array to avoid infinite reversing.
        movingTabs = [...movingTabs].reverse();
      }

      let directionForward = screen > dragData.animLastScreenPos;
      dragData.animLastScreenPos = screen;

      let screenAxis = this.verticalMode ? "screenY" : "screenX";
      let size = this.verticalMode ? "height" : "width";
      let translateAxis = this.verticalMode ? "translateY" : "translateX";
      let scrollDirection = this.verticalMode ? "scrollTop" : "scrollLeft";
      let { width: tabWidth, height: tabHeight } =
        draggedTab.getBoundingClientRect();
      let translateX = event.screenX - dragData.screenX;
      let translateY = event.screenY - dragData.screenY;

      dragData.tabWidth = tabWidth;
      dragData.tabHeight = tabHeight;
      dragData.translateX = translateX;
      dragData.translateY = translateY;

      // Move the dragged tab based on the mouse position.
      let firstTab = tabs[0];
      let lastTab = tabs.at(-1);
      let lastMovingTabScreen = movingTabs.at(-1)[screenAxis];
      let firstMovingTabScreen = movingTabs[0][screenAxis];
      let tabSize = this.verticalMode ? tabHeight : tabWidth;
      let shiftSize = lastMovingTabScreen + tabSize - firstMovingTabScreen;
      let translate = screen - dragData[screenAxis];
      if (!pinned) {
        translate +=
          this.arrowScrollbox.scrollbox[scrollDirection] - dragData.scrollPos;
      } else if (pinned && this.verticalMode) {
        translate +=
          this.verticalPinnedTabsContainer.scrollTop - dragData.scrollPos;
      }
      let firstBound = firstTab[screenAxis] - firstMovingTabScreen;
      let lastBound =
        lastTab[screenAxis] +
        lastTab.getBoundingClientRect()[size] -
        (lastMovingTabScreen + tabSize);
      translate = Math.min(Math.max(translate, firstBound), lastBound);

      for (let tab of movingTabs) {
        tab.style.transform = `${translateAxis}(${translate}px)`;
      }

      dragData.translatePos = translate;

      // Determine what tab we're dragging over.
      // * Single tab dragging: Point of reference is the center of the dragged tab. If that
      //   point touches a background tab, the dragged tab would take that
      //   tab's position when dropped.
      // * Multiple tabs dragging: All dragged tabs are one "giant" tab with two
      //   points of reference (center of tabs on the extremities). When
      //   mouse is moving from top to bottom, the bottom reference gets activated,
      //   otherwise the top reference will be used. Everything else works the same
      //   as single tab dragging.

      tabs = tabs.filter(t => !movingTabs.includes(t) || t == draggedTab);
      let getTabShift = (tab, dropIndex) => {
        if (tab._tPos < draggedTab._tPos && tab._tPos >= dropIndex) {
          return this.#rtlMode ? -shiftSize : shiftSize;
        }
        if (tab._tPos > draggedTab._tPos && tab._tPos < dropIndex) {
          return this.#rtlMode ? shiftSize : -shiftSize;
        }
        return 0;
      };

      // We're doing a binary search in order to reduce the amount of
      // tabs we need to check.
      let oldIndex = dragData.animDropIndex ?? movingTabs[0]._tPos;
      let getDragOverIndex = tabSizeDragOverThreshold => {
        let point =
          (directionForward
            ? lastMovingTabScreen + tabSize * (1 - tabSizeDragOverThreshold)
            : firstMovingTabScreen + tabSize * tabSizeDragOverThreshold) +
          translate;
        let index = -1;
        let low = 0;
        let high = tabs.length - 1;
        while (low <= high) {
          let mid = Math.floor((low + high) / 2);
          if (tabs[mid] == draggedTab && ++mid > high) {
            break;
          }
          screen = tabs[mid][screenAxis] + getTabShift(tabs[mid], oldIndex);

          if (screen > point) {
            high = mid - 1;
          } else if (screen + tabs[mid].getBoundingClientRect()[size] < point) {
            low = mid + 1;
          } else {
            index = tabs[mid]._tPos;
            break;
          }
        }
        return index;
      };
      let moveOverThreshold = gBrowser._tabGroupsEnabled
        ? Services.prefs.getIntPref(
            "browser.tabs.dragdrop.moveOverThresholdPercent"
          ) / 100
        : 0.5;
      moveOverThreshold = Math.min(1, Math.max(0, moveOverThreshold));
      let newIndex = getDragOverIndex(moveOverThreshold);
      if (newIndex >= oldIndex) {
        newIndex++;
      }
      if (newIndex < 0) {
        newIndex = oldIndex;
      }
      dragData.animDropIndex = newIndex;

      if (gBrowser._tabGroupsEnabled && !pinned) {
        let dragOverGroupingThreshold =
          Services.prefs.getIntPref(
            "browser.tabs.groups.dragOverThresholdPercent"
          ) / 100;
        dragOverGroupingThreshold = Math.min(
          moveOverThreshold,
          Math.max(0, dragOverGroupingThreshold)
        );
        let groupDropIndex = getDragOverIndex(dragOverGroupingThreshold);
        if (
          "groupDropIndex" in dragData &&
          dragData.groupDropIndex != groupDropIndex
        ) {
          this.allTabs[dragData.groupDropIndex]?.removeAttribute(
            "dragover-createGroup"
          );
          delete dragData.groupDropIndex;
          delete dragData.groupDropAction;
        }
        // If dragging over an ungrouped tab, present the UI for creating a
        // new tab group on drop.
        this.#clearDragOverCreateGroupTimer();
        if (
          groupDropIndex in this.allTabs &&
          !this.allTabs[groupDropIndex].group
        ) {
          this.#dragOverCreateGroupTimer = setTimeout(
            () => this.#triggerDragOverCreateGroup(dragData, groupDropIndex),
            Services.prefs.getIntPref("browser.tabs.groups.dragOverDelayMS")
          );
        } else {
          this.removeAttribute("movingtab-createGroup");
        }

        // If dragging over the last tab in a group, differentiate between
        // dropping into the group vs. after the group.
        if (
          groupDropIndex in this.allTabs &&
          this.allTabs[groupDropIndex] ==
            this.allTabs[groupDropIndex].group?.tabs.at(-1)
        ) {
          dragData.groupDropIndex = groupDropIndex;
          dragData.groupDropAction = GROUP_DROP_ACTION_APPEND;
          let colorCode = this.allTabs[groupDropIndex].group.color;
          this.#setDragOverGroupColor(colorCode);
          this.removeAttribute("movingtab-ungroup");
        }
      }

      if (gBrowser._tabGroupsEnabled && !("groupDropIndex" in dragData)) {
        // Add tab group line to dragged tabs when dragging into a group, and
        // remove it when dragging outside a group.
        let colorCode = this.allTabs[dragData.animDropIndex]?.group?.color;
        this.#setDragOverGroupColor(colorCode);
        this.toggleAttribute("movingtab-ungroup", !colorCode);
      }

      if (newIndex == oldIndex) {
        return;
      }

      // Shift background tabs to leave a gap where the dragged tab
      // would currently be dropped.
      for (let tab of tabs) {
        if (tab == draggedTab) {
          continue;
        }
        let shift = getTabShift(tab, newIndex);
        let transform = shift ? `${translateAxis}(${shift}px)` : "";
        tab.style.transform = transform;
        if (tab.group?.tabs[0] == tab) {
          tab.group.style.setProperty(
            "--tabgroup-dragover-transform",
            transform
          );
        }
      }
    }

    #triggerDragOverCreateGroup(dragData, groupDropIndex) {
      this.#clearDragOverCreateGroupTimer();

      dragData.groupDropIndex = groupDropIndex;
      dragData.groupDropAction = GROUP_DROP_ACTION_CREATE;
      this.toggleAttribute("movingtab-createGroup", true);
      this.removeAttribute("movingtab-ungroup");
      this.allTabs[groupDropIndex].toggleAttribute(
        "dragover-createGroup",
        true
      );
      this.#setDragOverGroupColor(dragData.tabGroupCreationColor);
    }

    #clearDragOverCreateGroupTimer() {
      if (this.#dragOverCreateGroupTimer) {
        clearTimeout(this.#dragOverCreateGroupTimer);
        this.#dragOverCreateGroupTimer = 0;
      }
    }

    #setDragOverGroupColor(groupColorCode) {
      if (!groupColorCode) {
        this.style.removeProperty("--dragover-tab-group-color");
        this.style.removeProperty("--dragover-tab-group-color-invert");
        this.style.removeProperty("--dragover-tab-group-color-pale");
        return;
      }
      this.style.setProperty(
        "--dragover-tab-group-color",
        `var(--tab-group-color-${groupColorCode})`
      );
      this.style.setProperty(
        "--dragover-tab-group-color-invert",
        `var(--tab-group-color-${groupColorCode}-invert)`
      );
      this.style.setProperty(
        "--dragover-tab-group-color-pale",
        `var(--tab-group-color-${groupColorCode}-pale)`
      );
    }

    _finishAnimateTabMove() {
      if (!this.hasAttribute("movingtab")) {
        return;
      }

      this.removeAttribute("movingtab");
      gNavToolbox.removeAttribute("movingtab");

      for (let tab of this.visibleTabs) {
        tab.style.transform = "";
        if (tab.group) {
          tab.group.style.removeProperty("--tabgroup-dragover-transform");
        }
        tab.removeAttribute("dragover-createGroup");
      }
      this.removeAttribute("movingtab-createGroup");
      this.removeAttribute("movingtab-ungroup");
      this.#setDragOverGroupColor(null);
      this.#clearDragOverCreateGroupTimer();

      this._handleTabSelect();
    }

    /**
     * Move together all selected tabs around the tab in param.
     */
    #moveTogetherSelectedTabs(tab) {
      let draggedTabPos = tab._tPos;
      let selectedTabs = gBrowser.selectedTabs;
      let animate = !gReduceMotion;

      tab._moveTogetherSelectedTabsData = {
        finished: !animate,
      };

      let addAnimationData = (
        movingTab,
        movingTabNewIndex,
        isBeforeSelectedTab = true
      ) => {
        let movingTabOldIndex = movingTab._tPos;

        if (movingTabOldIndex == movingTabNewIndex) {
          // movingTab is already at the right position
          // and thus don't need to be animated.
          return;
        }

        let movingTabSize =
          movingTab.getBoundingClientRect()[
            this.verticalMode ? "height" : "width"
          ];
        let shift = (movingTabNewIndex - movingTabOldIndex) * movingTabSize;

        movingTab._moveTogetherSelectedTabsData.animate = true;
        movingTab.toggleAttribute("multiselected-move-together", true);

        movingTab._moveTogetherSelectedTabsData.translatePos = shift;

        let postTransitionCleanup = () => {
          movingTab._moveTogetherSelectedTabsData.newIndex = movingTabNewIndex;
          movingTab._moveTogetherSelectedTabsData.animate = false;
        };
        if (gReduceMotion) {
          postTransitionCleanup();
        } else {
          let onTransitionEnd = transitionendEvent => {
            if (
              transitionendEvent.propertyName != "transform" ||
              transitionendEvent.originalTarget != movingTab
            ) {
              return;
            }
            movingTab.removeEventListener("transitionend", onTransitionEnd);
            postTransitionCleanup();
          };

          movingTab.addEventListener("transitionend", onTransitionEnd);
        }

        // Add animation data for tabs between movingTab (selected
        // tab moving towards the dragged tab) and draggedTab.
        // Those tabs in the middle should move in
        // the opposite direction of movingTab.

        let lowerIndex = Math.min(movingTabOldIndex, draggedTabPos);
        let higherIndex = Math.max(movingTabOldIndex, draggedTabPos);

        for (let i = lowerIndex + 1; i < higherIndex; i++) {
          let middleTab = gBrowser.visibleTabs[i];

          if (middleTab.pinned != movingTab.pinned) {
            // Don't mix pinned and unpinned tabs
            break;
          }

          if (middleTab.multiselected) {
            // Skip because this selected tab should
            // be shifted towards the dragged Tab.
            continue;
          }

          if (!middleTab._moveTogetherSelectedTabsData?.translatePos) {
            middleTab._moveTogetherSelectedTabsData = { translatePos: 0 };
          }
          if (isBeforeSelectedTab) {
            middleTab._moveTogetherSelectedTabsData.translatePos -=
              movingTabSize;
          } else {
            middleTab._moveTogetherSelectedTabsData.translatePos +=
              movingTabSize;
          }

          middleTab.toggleAttribute("multiselected-move-together", true);
        }
      };

      // Animate left or top selected tabs
      let insertAtPos = draggedTabPos - 1;
      for (let i = selectedTabs.indexOf(tab) - 1; i > -1; i--) {
        let movingTab = selectedTabs[i];
        insertAtPos = newIndex(movingTab, insertAtPos);

        if (animate) {
          movingTab._moveTogetherSelectedTabsData = {};
          addAnimationData(movingTab, insertAtPos, true);
        } else {
          gBrowser.moveTabTo(movingTab, insertAtPos);
        }
        insertAtPos--;
      }

      // Animate right or bottom selected tabs
      insertAtPos = draggedTabPos + 1;
      for (
        let i = selectedTabs.indexOf(tab) + 1;
        i < selectedTabs.length;
        i++
      ) {
        let movingTab = selectedTabs[i];
        insertAtPos = newIndex(movingTab, insertAtPos);

        if (animate) {
          movingTab._moveTogetherSelectedTabsData = {};
          addAnimationData(movingTab, insertAtPos, false);
        } else {
          gBrowser.moveTabTo(movingTab, insertAtPos);
        }
        insertAtPos++;
      }

      // Slide the relevant tabs to their new position.
      for (let t of this.visibleTabs) {
        if (t._moveTogetherSelectedTabsData?.translatePos) {
          let translatePos =
            (this.#rtlMode ? -1 : 1) *
            t._moveTogetherSelectedTabsData.translatePos;
          t.style.transform = `translate${
            this.verticalMode ? "Y" : "X"
          }(${translatePos}px)`;
        }
      }

      function newIndex(aTab, index) {
        // Don't allow mixing pinned and unpinned tabs.
        if (aTab.pinned) {
          return Math.min(index, gBrowser.pinnedTabCount - 1);
        }
        return Math.max(index, gBrowser.pinnedTabCount);
      }
    }

    _finishMoveTogetherSelectedTabs(tab) {
      if (
        !tab._moveTogetherSelectedTabsData ||
        tab._moveTogetherSelectedTabsData.finished
      ) {
        return;
      }

      tab._moveTogetherSelectedTabsData.finished = true;

      let selectedTabs = gBrowser.selectedTabs;
      let tabIndex = selectedTabs.indexOf(tab);

      // Moving left or top tabs
      for (let i = tabIndex - 1; i > -1; i--) {
        let movingTab = selectedTabs[i];
        if (movingTab._moveTogetherSelectedTabsData.newIndex) {
          gBrowser.moveTabTo(
            movingTab,
            movingTab._moveTogetherSelectedTabsData.newIndex
          );
        }
      }

      // Moving right or bottom tabs
      for (let i = tabIndex + 1; i < selectedTabs.length; i++) {
        let movingTab = selectedTabs[i];
        if (movingTab._moveTogetherSelectedTabsData.newIndex) {
          gBrowser.moveTabTo(
            movingTab,
            movingTab._moveTogetherSelectedTabsData.newIndex
          );
        }
      }

      for (let t of this.visibleTabs) {
        t.style.transform = "";
        t.removeAttribute("multiselected-move-together");
        delete t._moveTogetherSelectedTabsData;
      }
    }

    #isAnimatingMoveTogetherSelectedTabs() {
      for (let tab of gBrowser.selectedTabs) {
        if (tab._moveTogetherSelectedTabsData?.animate) {
          return true;
        }
      }
      return false;
    }

    handleEvent(aEvent) {
      switch (aEvent.type) {
        case "mouseout":
          // If the "related target" (the node to which the pointer went) is not
          // a child of the current document, the mouse just left the window.
          let relatedTarget = aEvent.relatedTarget;
          if (relatedTarget && relatedTarget.ownerDocument == document) {
            break;
          }
        // fall through
        case "mousemove":
          if (document.getElementById("tabContextMenu").state != "open") {
            this._unlockTabSizing();
          }
          break;
        case "mouseleave":
          this.previewPanel?.deactivate();
          break;
        default:
          let methodName = `on_${aEvent.type}`;
          if (methodName in this) {
            this[methodName](aEvent);
          } else {
            throw new Error(`Unexpected event ${aEvent.type}`);
          }
      }
    }

    _notifyBackgroundTab(aTab) {
      if (aTab.pinned || !aTab.visible || !this.overflowing) {
        return;
      }

      this._lastTabToScrollIntoView = aTab;
      if (!this._backgroundTabScrollPromise) {
        this._backgroundTabScrollPromise = window
          .promiseDocumentFlushed(() => {
            let lastTabRect =
              this._lastTabToScrollIntoView.getBoundingClientRect();
            let selectedTab = this.selectedItem;
            if (selectedTab.pinned) {
              selectedTab = null;
            } else {
              selectedTab = selectedTab.getBoundingClientRect();
              selectedTab = {
                left: selectedTab.left,
                right: selectedTab.right,
                top: selectedTab.top,
                bottom: selectedTab.bottom,
              };
            }
            return [
              this._lastTabToScrollIntoView,
              this.arrowScrollbox.scrollClientRect,
              lastTabRect,
              selectedTab,
            ];
          })
          .then(([tabToScrollIntoView, scrollRect, tabRect, selectedRect]) => {
            // First off, remove the promise so we can re-enter if necessary.
            delete this._backgroundTabScrollPromise;
            // Then, if the layout info isn't for the last-scrolled-to-tab, re-run
            // the code above to get layout info for *that* tab, and don't do
            // anything here, as we really just want to run this for the last-opened tab.
            if (this._lastTabToScrollIntoView != tabToScrollIntoView) {
              this._notifyBackgroundTab(this._lastTabToScrollIntoView);
              return;
            }
            delete this._lastTabToScrollIntoView;
            // Is the new tab already completely visible?
            if (
              this.verticalMode
                ? scrollRect.top <= tabRect.top &&
                  tabRect.bottom <= scrollRect.bottom
                : scrollRect.left <= tabRect.left &&
                  tabRect.right <= scrollRect.right
            ) {
              return;
            }

            if (this.arrowScrollbox.smoothScroll) {
              // Can we make both the new tab and the selected tab completely visible?
              if (
                !selectedRect || this.verticalMode
                  ? Math.max(
                      tabRect.bottom - selectedRect.top,
                      selectedRect.bottom - tabRect.top
                    ) <= scrollRect.height
                  : Math.max(
                      tabRect.right - selectedRect.left,
                      selectedRect.right - tabRect.left
                    ) <= scrollRect.width
              ) {
                this.arrowScrollbox.ensureElementIsVisible(tabToScrollIntoView);
                return;
              }

              let scrollPixels;
              if (this.verticalMode) {
                scrollPixels = tabRect.top - selectedRect.top;
              } else if (this.#rtlMode) {
                scrollPixels = selectedRect.right - scrollRect.right;
              } else {
                scrollPixels = selectedRect.left - scrollRect.left;
              }
              this.arrowScrollbox.scrollByPixels(scrollPixels);
            }

            if (!this._animateElement.hasAttribute("highlight")) {
              this._animateElement.toggleAttribute("highlight", true);
              setTimeout(
                function (ele) {
                  ele.removeAttribute("highlight");
                },
                150,
                this._animateElement
              );
            }
          });
      }
    }

    /**
     * Returns the tab where an event happened, or null if it didn't occur on a tab.
     *
     * @param {Event} event
     *   The event for which we want to know on which tab it happened.
     * @param {object} options
     * @param {boolean} options.ignoreTabSides
     *   If set to true: events will only be associated with a tab if they happened
     *   on its central part (from 25% to 75%); if they happened on the left or right
     *   sides of the tab, the method will return null.
     */
    _getDragTargetTab(event, { ignoreTabSides = false } = {}) {
      let { target } = event;
      if (target.nodeType != Node.ELEMENT_NODE) {
        target = target.parentElement;
      }
      let tab = target?.closest("tab");
      if (tab && ignoreTabSides) {
        let { width, height } = tab.getBoundingClientRect();
        if (
          event.screenX < tab.screenX + width * 0.25 ||
          event.screenX > tab.screenX + width * 0.75 ||
          ((event.screenY < tab.screenY + height * 0.25 ||
            event.screenY > tab.screenY + height * 0.75) &&
            this.verticalMode)
        ) {
          return null;
        }
      }
      return tab;
    }

    _getDropIndex(event) {
      let tab = this._getDragTargetTab(event);
      if (!tab) {
        return this.allTabs.length;
      }
      let isBeforeMiddle;
      if (this.verticalMode) {
        let middle = tab.screenY + tab.getBoundingClientRect().height / 2;
        isBeforeMiddle = event.screenY < middle;
      } else {
        let middle = tab.screenX + tab.getBoundingClientRect().width / 2;
        isBeforeMiddle = this.#rtlMode
          ? event.screenX > middle
          : event.screenX < middle;
      }
      return tab._tPos + (isBeforeMiddle ? 0 : 1);
    }

    getDropEffectForTabDrag(event) {
      var dt = event.dataTransfer;

      let isMovingTabs = dt.mozItemCount > 0;
      for (let i = 0; i < dt.mozItemCount; i++) {
        // tabs are always added as the first type
        let types = dt.mozTypesAt(0);
        if (types[0] != TAB_DROP_TYPE) {
          isMovingTabs = false;
          break;
        }
      }

      if (isMovingTabs) {
        let sourceNode = dt.mozGetDataAt(TAB_DROP_TYPE, 0);
        if (
          XULElement.isInstance(sourceNode) &&
          sourceNode.localName == "tab" &&
          sourceNode.ownerGlobal.isChromeWindow &&
          sourceNode.ownerDocument.documentElement.getAttribute("windowtype") ==
            "navigator:browser" &&
          sourceNode.ownerGlobal.gBrowser.tabContainer == sourceNode.container
        ) {
          // Do not allow transfering a private tab to a non-private window
          // and vice versa.
          if (
            PrivateBrowsingUtils.isWindowPrivate(window) !=
            PrivateBrowsingUtils.isWindowPrivate(sourceNode.ownerGlobal)
          ) {
            return "none";
          }

          if (
            window.gMultiProcessBrowser !=
            sourceNode.ownerGlobal.gMultiProcessBrowser
          ) {
            return "none";
          }

          if (
            window.gFissionBrowser != sourceNode.ownerGlobal.gFissionBrowser
          ) {
            return "none";
          }

          return dt.dropEffect == "copy" ? "copy" : "move";
        }
      }

      if (browserDragAndDrop.canDropLink(event)) {
        return "link";
      }
      return "none";
    }

    _handleNewTab(tab) {
      if (tab.container != this) {
        return;
      }
      tab._fullyOpen = true;
      gBrowser.tabAnimationsInProgress--;

      this._updateCloseButtons();

      if (tab.hasAttribute("selected")) {
        this._handleTabSelect();
      } else if (!tab.hasAttribute("skipbackgroundnotify")) {
        this._notifyBackgroundTab(tab);
      }

      // If this browser isn't lazy (indicating it's probably created by
      // session restore), preload the next about:newtab if we don't
      // already have a preloaded browser.
      if (tab.linkedPanel) {
        NewTabPagePreloading.maybeCreatePreloadedBrowser(window);
      }

      if (UserInteraction.running("browser.tabs.opening", window)) {
        UserInteraction.finish("browser.tabs.opening", window);
      }
    }

    _canAdvanceToTab(aTab) {
      return !aTab.closing;
    }

    /**
     * Returns the panel associated with a tab if it has a connected browser
     * and/or it is the selected tab.
     * For background lazy browsers, this will return null.
     */
    getRelatedElement(aTab) {
      if (!aTab) {
        return null;
      }

      // Cannot access gBrowser before it's initialized.
      if (!gBrowser._initialized) {
        return this.tabbox.tabpanels.firstElementChild;
      }

      // If the tab's browser is lazy, we need to `_insertBrowser` in order
      // to have a linkedPanel.  This will also serve to bind the browser
      // and make it ready to use. We only do this if the tab is selected
      // because otherwise, callers might end up unintentionally binding the
      // browser for lazy background tabs.
      if (!aTab.linkedPanel) {
        if (!aTab.selected) {
          return null;
        }
        gBrowser._insertBrowser(aTab);
      }
      return document.getElementById(aTab.linkedPanel);
    }

    _updateNewTabVisibility() {
      // Helper functions to help deal with customize mode wrapping some items
      let wrap = n =>
        n.parentNode.localName == "toolbarpaletteitem" ? n.parentNode : n;
      let unwrap = n =>
        n && n.localName == "toolbarpaletteitem" ? n.firstElementChild : n;

      // Starting from the tabs element, find the next sibling that:
      // - isn't hidden; and
      // - isn't the all-tabs button.
      // If it's the new tab button, consider the new tab button adjacent to the tabs.
      // If the new tab button is marked as adjacent and the tabstrip doesn't
      // overflow, we'll display the 'new tab' button inline in the tabstrip.
      // In all other cases, the separate new tab button is displayed in its
      // customized location.
      let sib = this;
      do {
        sib = unwrap(wrap(sib).nextElementSibling);
      } while (sib && (sib.hidden || sib.id == "alltabs-button"));

      this.toggleAttribute(
        "hasadjacentnewtabbutton",
        sib && sib.id == "new-tab-button"
      );
    }

    onWidgetAfterDOMChange(aNode, aNextNode, aContainer) {
      if (
        aContainer.ownerDocument == document &&
        aContainer.id == "TabsToolbar-customization-target"
      ) {
        this._updateNewTabVisibility();
      }
    }

    onAreaNodeRegistered(aArea, aContainer) {
      if (aContainer.ownerDocument == document && aArea == "TabsToolbar") {
        this._updateNewTabVisibility();
      }
    }

    onAreaReset(aArea, aContainer) {
      this.onAreaNodeRegistered(aArea, aContainer);
    }

    _hiddenSoundPlayingStatusChanged(tab, opts) {
      let closed = opts && opts.closed;
      if (!closed && tab.soundPlaying && !tab.visible) {
        this._hiddenSoundPlayingTabs.add(tab);
        this.toggleAttribute("hiddensoundplaying", true);
      } else {
        this._hiddenSoundPlayingTabs.delete(tab);
        if (this._hiddenSoundPlayingTabs.size == 0) {
          this.removeAttribute("hiddensoundplaying");
        }
      }
    }

    destroy() {
      if (this.boundObserve) {
        Services.prefs.removeObserver("privacy.userContext", this.boundObserve);
      }
      CustomizableUI.removeListener(this);
    }

    updateTabIndicatorAttr(tab) {
      const theseAttributes = ["soundplaying", "muted", "activemedia-blocked"];
      const notTheseAttributes = ["pinned", "sharing", "crashed"];

      if (
        this.verticalMode ||
        notTheseAttributes.some(attr => tab.hasAttribute(attr))
      ) {
        tab.removeAttribute("indicator-replaces-favicon");
        return;
      }

      tab.toggleAttribute(
        "indicator-replaces-favicon",
        theseAttributes.some(attr => tab.hasAttribute(attr))
      );
    }
  }

  customElements.define("tabbrowser-tabs", MozTabbrowserTabs, {
    extends: "tabs",
  });
}
