/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/browser-window */

"use strict";

// This is loaded into all browser windows. Wrap in a block to prevent
// leaking to window scope.
{
  const TAB_PREVIEW_PREF = "browser.tabs.hoverPreview.enabled";

  class MozTabbrowserTabs extends MozElements.TabsBase {
    static observedAttributes = ["orient"];
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
      this.addEventListener("keydown", this, { mozSystemGroup: true });
      this.addEventListener("dragstart", this);
      this.addEventListener("dragover", this);
      this.addEventListener("drop", this);
      this.addEventListener("dragend", this);
      this.addEventListener("dragleave", this);
      this.addEventListener("mouseleave", this);
    }

    init() {
      this.startupTime = Services.startup.getStartupInfo().start.getTime();

      this.arrowScrollbox = this.querySelector("arrowscrollbox");
      this.arrowScrollbox.addEventListener("wheel", this, true);
      this.arrowScrollbox.addEventListener("underflow", this);
      this.arrowScrollbox.addEventListener("overflow", this);
      // Override arrowscrollbox.js method, since our scrollbox's children are
      // inherited from the scrollbox binding parent (this).
      this.arrowScrollbox._getScrollableElements = () => {
        return this.allTabs.filter(this.arrowScrollbox._canScrollToElement);
      };
      this.arrowScrollbox._canScrollToElement = tab => {
        return !tab._pinnedUnscrollable && !tab.hidden;
      };

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
      this._allTabs = null;
      this._visibleTabs = null;
      this._previewPanel = null;

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
      this._previewPanel = null;
    }

    attributeChangedCallback(name, oldValue, newValue) {
      if (name != "orient") {
        return;
      }

      if (oldValue == "vertical" && newValue == "horizontal") {
        this._resetVerticalPinnedTabs();
      }
      this._positionPinnedTabs();

      this.#updateTabMinWidth();

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
        event.target.hidden
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
      if (!this._previewPanel) {
        // load the tab preview component
        const TabHoverPreviewPanel = ChromeUtils.importESModule(
          "chrome://browser/content/tabbrowser/tab-hover-preview.mjs"
        ).default;
        this._previewPanel = new TabHoverPreviewPanel(
          document.getElementById("tab-preview-panel")
        );
      }
      this._previewPanel.activate(event.target);
    }

    on_TabHoverEnd(event) {
      this._previewPanel?.deactivate(event.target);
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
        let tabToSelect = gBrowser._findTabToBlurTo(
          gBrowser.selectedTab,
          group.tabs
        );

        if (tabToSelect) {
          gBrowser.selectedTab = tabToSelect;
        } else {
          gBrowser.addAdjacentNewTab(group.tabs.at(-1));
        }
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
      if (TabsInTitlebar.enabled && !this.verticalMode) {
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
          let visibleTabs = this._getVisibleTabs();
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

      let keyComboForMove = accel && shiftKey && !altKey && !nonAccel;
      let keyComboForFocus = accel && !shiftKey && !altKey && !nonAccel;

      if (!keyComboForMove && !keyComboForFocus) {
        return;
      }

      // Don't check if the event was already consumed because tab navigation
      // should work always for better user experience.
      let { visibleTabs, selectedTab } = gBrowser;
      let { arrowKeysShouldWrap } = this;
      let focusedTabIndex = this.ariaFocusedIndex;
      if (focusedTabIndex == -1) {
        focusedTabIndex = visibleTabs.indexOf(selectedTab);
      }
      let lastFocusedTabIndex = focusedTabIndex;
      switch (event.keyCode) {
        case KeyEvent.DOM_VK_UP:
          if (keyComboForMove) {
            gBrowser.moveTabBackward();
          } else {
            focusedTabIndex--;
          }
          break;
        case KeyEvent.DOM_VK_DOWN:
          if (keyComboForMove) {
            gBrowser.moveTabForward();
          } else {
            focusedTabIndex++;
          }
          break;
        case KeyEvent.DOM_VK_RIGHT:
        case KeyEvent.DOM_VK_LEFT:
          if (keyComboForMove) {
            gBrowser.moveTabOver(event);
          } else if (
            (!this.#rtlMode && event.keyCode == KeyEvent.DOM_VK_RIGHT) ||
            (this.#rtlMode && event.keyCode == KeyEvent.DOM_VK_LEFT)
          ) {
            focusedTabIndex++;
          } else {
            focusedTabIndex--;
          }
          break;
        case KeyEvent.DOM_VK_HOME:
          if (keyComboForMove) {
            gBrowser.moveTabToStart();
          } else {
            focusedTabIndex = 0;
          }
          break;
        case KeyEvent.DOM_VK_END:
          if (keyComboForMove) {
            gBrowser.moveTabToEnd();
          } else {
            focusedTabIndex = visibleTabs.length - 1;
          }
          break;
        case KeyEvent.DOM_VK_SPACE:
          if (visibleTabs[lastFocusedTabIndex].multiselected) {
            gBrowser.removeFromMultiSelectedTabs(
              visibleTabs[lastFocusedTabIndex]
            );
          } else {
            gBrowser.addToMultiSelectedTabs(visibleTabs[lastFocusedTabIndex]);
          }
          break;
        default:
          // Consume the keydown event for the above keyboard
          // shortcuts only.
          return;
      }

      if (arrowKeysShouldWrap) {
        if (focusedTabIndex >= visibleTabs.length) {
          focusedTabIndex = 0;
        } else if (focusedTabIndex < 0) {
          focusedTabIndex = visibleTabs.length - 1;
        }
      } else {
        focusedTabIndex = Math.min(
          visibleTabs.length - 1,
          Math.max(0, focusedTabIndex)
        );
      }

      if (keyComboForFocus && focusedTabIndex != lastFocusedTabIndex) {
        this.ariaFocusedItem = visibleTabs[focusedTabIndex];
      }

      event.preventDefault();
    }

    on_dragstart(event) {
      var tab = this._getDragTargetTab(event);
      if (!tab || this._isCustomizing) {
        return;
      }

      this._previewPanel?.deactivate();
      this.startTabDrag(event, tab);
    }

    startTabDrag(event, tab, { fromTabList = false } = {}) {
      let selectedTabs = gBrowser.selectedTabs;
      let otherSelectedTabs = selectedTabs.filter(
        selectedTab => selectedTab != tab
      );
      let dataTransferOrderedTabs = [tab].concat(otherSelectedTabs);

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
        this._groupSelectedTabs(tab);
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
        scrollPos: this.arrowScrollbox.scrollPosition,
        screenX: event.screenX,
        screenY: event.screenY,
        movingTabs: (tab.multiselected ? gBrowser.selectedTabs : [tab]).filter(
          t => t.pinned == tab.pinned
        ),
        fromTabList,
      };

      event.stopPropagation();

      if (fromTabList) {
        Services.telemetry.scalarAdd(
          "browser.ui.interaction.all_tabs_panel_dragstart_tab_event_count",
          1
        );
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

        if (!this._isGroupTabsAnimationOver()) {
          // Wait for grouping tabs animation to finish
          return;
        }
        this._finishGroupSelectedTabs(draggedTab);

        if (effects == "move") {
          // Pinned tabs in expanded vertical mode are on a grid format and require
          // different logic to drag and drop
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
          let tabRect = this._getVisibleTabs().at(-1).getBoundingClientRect();
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
        draggedTab.container._finishGroupSelectedTabs(draggedTab);
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

        let dropIndex;
        if (draggedTab._dragData.fromTabList) {
          dropIndex = this._getDropIndex(event);
        } else {
          dropIndex =
            "animDropIndex" in draggedTab._dragData &&
            draggedTab._dragData.animDropIndex;
        }
        let incrementDropIndex = true;
        if (dropIndex && dropIndex > movingTabs[0]._tPos) {
          dropIndex--;
          incrementDropIndex = false;
        }

        let shouldTranslate;
        if (this.#isContainerVerticalPinnedExpanded(draggedTab)) {
          shouldTranslate =
            ((oldTranslateX && oldTranslateX != newTranslateX) ||
              (oldTranslateY && oldTranslateY != newTranslateY)) &&
            !gReduceMotion;
        } else if (this.verticalMode) {
          shouldTranslate =
            oldTranslateY && oldTranslateY != newTranslateY && !gReduceMotion;
        } else {
          shouldTranslate =
            oldTranslateX && oldTranslateX != newTranslateX && !gReduceMotion;
        }

        if (shouldTranslate) {
          for (let tab of movingTabs) {
            tab.toggleAttribute("tabdrop-samewindow", true);
            tab.style.transform = `translate(${newTranslateX}px, ${newTranslateY}px)`;
            let postTransitionCleanup = () => {
              tab.removeAttribute("tabdrop-samewindow");

              this._finishAnimateTabMove();
              if (dropIndex !== false) {
                gBrowser.moveTabTo(tab, dropIndex);
                if (incrementDropIndex) {
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
        } else {
          this._finishAnimateTabMove();
          if (dropIndex !== false) {
            for (let tab of movingTabs) {
              gBrowser.moveTabTo(tab, dropIndex);
              if (incrementDropIndex) {
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

      this._finishGroupSelectedTabs(draggedTab);
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

    // Accessor for tabs.  arrowScrollbox has a container for non-tab elements
    // at the end, everything else is <tab>s.
    get allTabs() {
      if (this._allTabs) {
        return this._allTabs;
      }
      let verticalPinnedTabsContainer = document.getElementById(
        "vertical-pinned-tabs-container"
      );
      let children = Array.from(this.arrowScrollbox.children);
      // remove arrowScrollbox periphery element
      children.pop();

      // explode tab groups
      Array.from(children).forEach((node, index) => {
        if (node.tagName == "tab-group") {
          children.splice(index, 1, ...node.tabs);
        }
      });

      let allChildren = [...verticalPinnedTabsContainer.children, ...children];
      this._allTabs = allChildren;
      return allChildren;
    }

    get allGroups() {
      let children = Array.from(this.arrowScrollbox.children);
      return children.filter(node => node.tagName == "tab-group");
    }

    get previewPanel() {
      return this._previewPanel;
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

    _getVisibleTabs() {
      if (!this._visibleTabs) {
        this._visibleTabs = Array.prototype.filter.call(
          this.allTabs,
          tab => !tab.hidden && !tab.closing && !tab.group?.collapsed
        );
      }
      return this._visibleTabs;
    }

    _invalidateCachedTabs() {
      this._allTabs = null;
      this._visibleTabs = null;
    }

    _invalidateCachedVisibleTabs() {
      this._visibleTabs = null;
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

      return node.before(tab);
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

          // There are separate "new tab" buttons for when the tab strip
          // is overflowed and when it is not.  Attach the long click
          // popup to both of them.
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
          let tab = this._getVisibleTabs()[gBrowser._numPinnedTabs];
          if (tab && rect(tab).width <= this._tabClipWidth) {
            this.setAttribute("closebuttons", "activetab");
          } else {
            this.removeAttribute("closebuttons");
          }
        });
      });
    }

    _updateHiddenTabsStatus() {
      this.toggleAttribute(
        "hashiddentabs",
        gBrowser.visibleTabs.length < gBrowser.tabs.length
      );
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

      let tabs = this._getVisibleTabs();
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
        let numPinned = gBrowser._numPinnedTabs;
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
        let tabs = this._getVisibleTabs();
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
      this._handleTabSelect(true);
    }

    _updateVerticalPinnedTabs() {
      // Move pinned tabs to another container when the tabstrip is toggled to vertical
      // and when session restore code calls _positionPinnedTabs; update styling whenever
      // the number of pinned tabs changes.
      let verticalTabsContainer = document.getElementById(
        "vertical-pinned-tabs-container"
      );
      let numPinned = gBrowser._numPinnedTabs;

      if (gBrowser._numPinnedTabs !== verticalTabsContainer.children.length) {
        let tabs = this._getVisibleTabs();
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
      let tabs = this._getVisibleTabs();
      let numPinned = gBrowser._numPinnedTabs;
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
          tab._pinnedUnscrollable = true;
        }
        this.style.setProperty(
          "--tab-overflow-pinned-tabs-width",
          width + "px"
        );
      } else {
        for (let i = 0; i < numPinned; i++) {
          let tab = tabs[i];
          tab.style.marginInlineStart = "";
          tab._pinnedUnscrollable = false;
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
      let movingTabs = draggedTab._dragData.movingTabs;

      if (!this.hasAttribute("movingtab")) {
        this.toggleAttribute("movingtab", true);
        gNavToolbox.toggleAttribute("movingtab", true);
        if (!draggedTab.multiselected) {
          this.selectedItem = draggedTab;
        }
      }

      if (!("animLastScreenX" in draggedTab._dragData)) {
        draggedTab._dragData.animLastScreenX = draggedTab._dragData.screenX;
      }
      if (!("animLastScreenY" in draggedTab._dragData)) {
        draggedTab._dragData.animLastScreenY = draggedTab._dragData.screenY;
      }

      let screenX = event.screenX;
      let screenY = event.screenY;

      if (
        screenY == draggedTab._dragData.animLastScreenY &&
        screenX == draggedTab._dragData.animLastScreenX
      ) {
        return;
      }

      let tabs = this._getVisibleTabs().slice(0, gBrowser._numPinnedTabs);

      let directionX = screenX > draggedTab._dragData.animLastScreenX;
      let directionY = screenY > draggedTab._dragData.animLastScreenY;
      draggedTab._dragData.animLastScreenY = screenY;
      draggedTab._dragData.animLastScreenX = screenX;

      let tabWidth = draggedTab.getBoundingClientRect().width;
      let tabHeight = draggedTab.getBoundingClientRect().height;
      let shiftSizeX = tabWidth * movingTabs.length;
      let shiftSizeY = tabHeight;
      draggedTab._dragData.tabWidth = tabWidth;
      draggedTab._dragData.tabHeight = tabHeight;

      // In expanded vertical mode, 6 is the max number of pinned tabs per row
      const maxTabsPerRow = 6;

      // Move the dragged tab based on the mouse position.
      let firstTabInRow;
      let lastTabInRow;
      if (RTL_UI) {
        firstTabInRow =
          tabs.length >= maxTabsPerRow ? tabs[maxTabsPerRow - 1] : tabs.at(-1);
        lastTabInRow = tabs[0];
      } else {
        firstTabInRow = tabs[0];
        lastTabInRow =
          tabs.length >= maxTabsPerRow ? tabs[maxTabsPerRow - 1] : tabs.at(-1);
      }
      let firstMovingTabScreenX = movingTabs.at(-1).screenX;
      let firstMovingTabScreenY = movingTabs.at(-1).screenY;
      let lastMovingTabScreenX = movingTabs[0].screenX;
      let lastMovingTabScreenY = movingTabs[0].screenY;
      let translateX = screenX - draggedTab._dragData.screenX;
      let translateY = screenY - draggedTab._dragData.screenY;
      let firstBoundX = firstTabInRow.screenX - lastMovingTabScreenX;
      let firstBoundY = firstTabInRow.screenY - lastMovingTabScreenY;
      let lastBoundX =
        lastTabInRow.screenX +
        lastTabInRow.getBoundingClientRect().width -
        (firstMovingTabScreenX + tabWidth);
      let lastBoundY =
        tabs.at(-1).screenY +
        lastTabInRow.getBoundingClientRect().height -
        (firstMovingTabScreenY + tabHeight);
      translateX = Math.min(Math.max(translateX, firstBoundX), lastBoundX);
      translateY = Math.min(Math.max(translateY, firstBoundY), lastBoundY);

      for (let tab of movingTabs) {
        tab.style.transform = `translate(${translateX}px, ${translateY}px)`;
      }

      draggedTab._dragData.translateX = translateX;
      draggedTab._dragData.translateY = translateY;

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
      let firstTabCenterX = lastMovingTabScreenX + translateX + tabWidth / 2;
      let lastTabCenterX = firstMovingTabScreenX + translateX + tabWidth / 2;
      let tabCenterX = directionX ? lastTabCenterX : firstTabCenterX;
      let firstTabCenterY = lastMovingTabScreenY + translateY + tabWidth / 2;
      let lastTabCenterY = firstMovingTabScreenY + translateY + tabWidth / 2;
      let tabCenterY = directionY ? lastTabCenterY : firstTabCenterY;

      let newIndex = -1;
      let oldIndex =
        "animDropIndex" in draggedTab._dragData
          ? draggedTab._dragData.animDropIndex
          : movingTabs[0]._tPos;

      let low = 0;
      let high = tabs.length - 1;
      let shiftNumber = maxTabsPerRow - movingTabs.length;

      let getTabShift = (tab, dropIndex) => {
        if (tab._tPos < draggedTab._tPos && tab._tPos >= dropIndex) {
          // If tab is at the end of a row, shift back and down
          let tabRow = Math.ceil((tab._tPos + 1) / maxTabsPerRow);
          let shiftedTabRow = Math.ceil(
            (tab._tPos + 1 + movingTabs.length) / maxTabsPerRow
          );
          if (tab._tPos && tabRow != shiftedTabRow) {
            return [
              RTL_UI
                ? tabWidth * shiftNumber + tabWidth / 2
                : -tabWidth * shiftNumber - tabWidth / 2,
              shiftSizeY,
            ];
          }
          return [RTL_UI ? -shiftSizeX : shiftSizeX, 0];
        }
        if (tab._tPos > draggedTab._tPos && tab._tPos < dropIndex) {
          // If tab is not index 0 and at the start of a row, shift across and up
          let tabRow = Math.floor(tab._tPos / maxTabsPerRow);
          let shiftedTabRow = Math.floor(
            (tab._tPos - movingTabs.length) / maxTabsPerRow
          );
          if (tab._tPos && tabRow != shiftedTabRow) {
            return [
              RTL_UI
                ? -tabWidth * shiftNumber - tabWidth / 2
                : tabWidth * shiftNumber + tabWidth / 2,
              -shiftSizeY,
            ];
          }
          return [RTL_UI ? shiftSizeX : -shiftSizeX, 0];
        }
        return [0, 0];
      };

      while (low <= high) {
        let mid = Math.floor((low + high) / 2);
        if (tabs[mid] == draggedTab && ++mid > high) {
          break;
        }
        let shift = getTabShift(tabs[mid], oldIndex);
        screenX = tabs[mid].screenX + shift[0];
        screenY = tabs[mid].screenY + shift[1];

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
      draggedTab._dragData.animDropIndex = newIndex;

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
      let movingTabs = draggedTab._dragData.movingTabs;

      if (!this.hasAttribute("movingtab")) {
        this.toggleAttribute("movingtab", true);
        gNavToolbox.toggleAttribute("movingtab", true);
        if (!draggedTab.multiselected) {
          this.selectedItem = draggedTab;
        }
      }

      if (!("animLastScreenPos" in draggedTab._dragData)) {
        draggedTab._dragData.animLastScreenPos = this.verticalMode
          ? draggedTab._dragData.screenY
          : draggedTab._dragData.screenX;
      }

      let screen = this.verticalMode ? event.screenY : event.screenX;
      if (screen == draggedTab._dragData.animLastScreenPos) {
        return;
      }

      let pinned = draggedTab.pinned;
      let numPinned = gBrowser._numPinnedTabs;
      let tabs = this._getVisibleTabs().slice(
        pinned ? 0 : numPinned,
        pinned ? numPinned : undefined
      );

      if (this.#rtlMode) {
        tabs.reverse();
        // Copy moving tabs array to avoid infinite reversing.
        movingTabs = [...movingTabs].reverse();
      }

      let directionMove = screen > draggedTab._dragData.animLastScreenPos;
      draggedTab._dragData.animLastScreenPos = screen;

      let screenAxis = this.verticalMode ? "screenY" : "screenX";
      let size = this.verticalMode ? "height" : "width";
      let translateAxis = this.verticalMode ? "translateY" : "translateX";
      let scrollDirection = this.verticalMode ? "scrollTop" : "scrollLeft";

      let tabSize = draggedTab.getBoundingClientRect()[size];
      let shiftSize = tabSize * movingTabs.length;
      draggedTab._dragData.tabSize = tabSize;

      // Move the dragged tab based on the mouse position.
      let firstTab = tabs[0];
      let lastTab = tabs.at(-1);
      let firstMovingTabScreen = movingTabs.at(-1)[screenAxis];
      let lastMovingTabScreen = movingTabs[0][screenAxis];
      let translate = screen - draggedTab._dragData[screenAxis];
      if (!pinned) {
        translate +=
          this.arrowScrollbox.scrollbox[scrollDirection] -
          draggedTab._dragData.scrollPos;
      }
      let firstBound = firstTab[screenAxis] - lastMovingTabScreen;
      let lastBound =
        lastTab[screenAxis] +
        lastTab.getBoundingClientRect()[size] -
        (firstMovingTabScreen + tabSize);
      translate = Math.min(Math.max(translate, firstBound), lastBound);

      for (let tab of movingTabs) {
        tab.style.transform = `${translateAxis}(${translate}px)`;
      }

      draggedTab._dragData.translatePos = translate;

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
      let firstTabCenter = lastMovingTabScreen + translate + tabSize / 2;
      let lastTabCenter = firstMovingTabScreen + translate + tabSize / 2;
      let tabCenter = directionMove ? lastTabCenter : firstTabCenter;
      let newIndex = -1;
      let oldIndex =
        "animDropIndex" in draggedTab._dragData
          ? draggedTab._dragData.animDropIndex
          : movingTabs[0]._tPos;
      let low = 0;
      let high = tabs.length - 1;
      let getTabShift = (tab, dropIndex) => {
        if (tab._tPos < draggedTab._tPos && tab._tPos >= dropIndex) {
          return this.#rtlMode ? -shiftSize : shiftSize;
        }
        if (tab._tPos > draggedTab._tPos && tab._tPos < dropIndex) {
          return this.#rtlMode ? shiftSize : -shiftSize;
        }
        return 0;
      };

      while (low <= high) {
        let mid = Math.floor((low + high) / 2);
        if (tabs[mid] == draggedTab && ++mid > high) {
          break;
        }
        screen = tabs[mid][screenAxis] + getTabShift(tabs[mid], oldIndex);

        if (screen > tabCenter) {
          high = mid - 1;
        } else if (
          screen + tabs[mid].getBoundingClientRect()[size] <
          tabCenter
        ) {
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
      draggedTab._dragData.animDropIndex = newIndex;

      // Shift background tabs to leave a gap where the dragged tab
      // would currently be dropped.
      for (let tab of tabs) {
        if (tab != draggedTab) {
          let shift = getTabShift(tab, newIndex);
          tab.style.transform = shift ? `${translateAxis}(${shift}px)` : "";
        }
      }
    }

    _finishAnimateTabMove() {
      if (!this.hasAttribute("movingtab")) {
        return;
      }

      for (let tab of this._getVisibleTabs()) {
        tab.style.transform = "";
      }

      this.removeAttribute("movingtab");
      gNavToolbox.removeAttribute("movingtab");

      this._handleTabSelect();
    }

    /**
     * Regroup all selected tabs around the
     * tab in param
     */
    _groupSelectedTabs(tab) {
      let draggedTabPos = tab._tPos;
      let selectedTabs = gBrowser.selectedTabs;
      let animate = !gReduceMotion;

      tab.groupingTabsData = {
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

        movingTab.groupingTabsData.animate = true;
        movingTab.toggleAttribute("tab-grouping", true);

        movingTab.groupingTabsData.translatePos = shift;

        let postTransitionCleanup = () => {
          movingTab.groupingTabsData.newIndex = movingTabNewIndex;
          movingTab.groupingTabsData.animate = false;
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

          if (
            !middleTab.groupingTabsData ||
            !middleTab.groupingTabsData.translatePos
          ) {
            middleTab.groupingTabsData = { translatePos: 0 };
          }
          if (isBeforeSelectedTab) {
            middleTab.groupingTabsData.translatePos -= movingTabSize;
          } else {
            middleTab.groupingTabsData.translatePos += movingTabSize;
          }

          middleTab.toggleAttribute("tab-grouping", true);
        }
      };

      // Animate left or top selected tabs
      let insertAtPos = draggedTabPos - 1;
      for (let i = selectedTabs.indexOf(tab) - 1; i > -1; i--) {
        let movingTab = selectedTabs[i];
        insertAtPos = newIndex(movingTab, insertAtPos);

        if (animate) {
          movingTab.groupingTabsData = {};
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
          movingTab.groupingTabsData = {};
          addAnimationData(movingTab, insertAtPos, false);
        } else {
          gBrowser.moveTabTo(movingTab, insertAtPos);
        }
        insertAtPos++;
      }

      // Slide the relevant tabs to their new position.
      for (let t of this._getVisibleTabs()) {
        if (t.groupingTabsData && t.groupingTabsData.translatePos) {
          let translatePos =
            (this.#rtlMode ? -1 : 1) * t.groupingTabsData.translatePos;
          t.style.transform = `translate${
            this.verticalMode ? "Y" : "X"
          }(${translatePos}px)`;
        }
      }

      function newIndex(aTab, index) {
        // Don't allow mixing pinned and unpinned tabs.
        if (aTab.pinned) {
          return Math.min(index, gBrowser._numPinnedTabs - 1);
        }
        return Math.max(index, gBrowser._numPinnedTabs);
      }
    }

    _finishGroupSelectedTabs(tab) {
      if (!tab.groupingTabsData || tab.groupingTabsData.finished) {
        return;
      }

      tab.groupingTabsData.finished = true;

      let selectedTabs = gBrowser.selectedTabs;
      let tabIndex = selectedTabs.indexOf(tab);

      // Moving left or top tabs
      for (let i = tabIndex - 1; i > -1; i--) {
        let movingTab = selectedTabs[i];
        if (movingTab.groupingTabsData.newIndex) {
          gBrowser.moveTabTo(movingTab, movingTab.groupingTabsData.newIndex);
        }
      }

      // Moving right or bottom tabs
      for (let i = tabIndex + 1; i < selectedTabs.length; i++) {
        let movingTab = selectedTabs[i];
        if (movingTab.groupingTabsData.newIndex) {
          gBrowser.moveTabTo(movingTab, movingTab.groupingTabsData.newIndex);
        }
      }

      for (let t of this._getVisibleTabs()) {
        t.style.transform = "";
        t.removeAttribute("tab-grouping");
        delete t.groupingTabsData;
      }
    }

    _isGroupTabsAnimationOver() {
      for (let tab of gBrowser.selectedTabs) {
        if (tab.groupingTabsData && tab.groupingTabsData.animate) {
          return false;
        }
      }
      return true;
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
          this._previewPanel?.deactivate();
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
      if (aTab.pinned || aTab.hidden || !this.overflowing) {
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
              };
            }
            return [
              this._lastTabToScrollIntoView,
              this.arrowScrollbox.scrollClientRect,
              { left: lastTabRect.left, right: lastTabRect.right },
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
              scrollRect.left <= tabRect.left &&
              tabRect.right <= scrollRect.right
            ) {
              return;
            }

            if (this.arrowScrollbox.smoothScroll) {
              // Can we make both the new tab and the selected tab completely visible?
              if (
                !selectedRect ||
                Math.max(
                  tabRect.right - selectedRect.left,
                  selectedRect.right - tabRect.left
                ) <= scrollRect.width
              ) {
                this.arrowScrollbox.ensureElementIsVisible(tabToScrollIntoView);
                return;
              }

              this.arrowScrollbox.scrollByPixels(
                this.#rtlMode
                  ? selectedRect.right - scrollRect.right
                  : selectedRect.left - scrollRect.left
              );
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
      if (!closed && tab.soundPlaying && tab.hidden) {
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
