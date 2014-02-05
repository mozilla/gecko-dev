/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * selection management
 */

/*
 * Current monocle image:
 *  dimensions: 32 x 24
 *  circle center: 16 x 14
 *  padding top: 6
 */

// Y axis scroll distance that will disable this module and cancel selection
const kDisableOnScrollDistance = 25;

// Drag hysteresis programmed into monocle drag moves
const kDragHysteresisDistance = 10;

// selection layer id returned from SelectionHandlerUI's layerMode.
const kChromeLayer = 1;
const kContentLayer = 2;

/*
 * Markers
 */

function MarkerDragger(aMarker) {
  this._marker = aMarker;
}

MarkerDragger.prototype = {
  _selectionHelperUI: null,
  _marker: null,
  _shutdown: false,
  _dragging: false,

  get marker() {
    return this._marker;
  },

  set shutdown(aVal) {
    this._shutdown = aVal;
  },

  get shutdown() {
    return this._shutdown;
  },

  get dragging() {
    return this._dragging;
  },

  freeDrag: function freeDrag() {
    return true;
  },

  isDraggable: function isDraggable(aTarget, aContent) {
    return { x: true, y: true };
  },

  dragStart: function dragStart(aX, aY, aTarget, aScroller) {
    if (this._shutdown)
      return false;
    this._dragging = true;
    this.marker.dragStart(aX, aY);
    return true;
  },

  dragStop: function dragStop(aDx, aDy, aScroller) {
    if (this._shutdown)
      return false;
    this._dragging = false;
    this.marker.dragStop(aDx, aDy);
    return true;
  },

  dragMove: function dragMove(aDx, aDy, aScroller, aIsKenetic, aClientX, aClientY) {
    // Note if aIsKenetic is true this is synthetic movement,
    // we don't want that so return false.
    if (this._shutdown || aIsKenetic)
      return false;
    this.marker.moveBy(aDx, aDy, aClientX, aClientY);
    // return true if we moved, false otherwise. The result
    // is used in deciding if we should repaint between drags.
    return true;
  }
}

function Marker(aParent, aTag, aElementId, xPos, yPos) {
  this._xPos = xPos;
  this._yPos = yPos;
  this._selectionHelperUI = aParent;
  this._element = aParent.overlay.getMarker(aElementId);
  this._elementId = aElementId;
  // These get picked in input.js and receives drag input
  this._element.customDragger = new MarkerDragger(this);
  this.tag = aTag;
}

Marker.prototype = {
  _element: null,
  _elementId: "",
  _selectionHelperUI: null,
  _xPos: 0,
  _yPos: 0,
  _xDrag: 0,
  _yDrag: 0,
  _tag: "",
  _hPlane: 0,
  _vPlane: 0,

  // Tweak me if the monocle graphics change in any way
  _monocleRadius: 8,
  _monocleXHitTextAdjust: -2, 
  _monocleYHitTextAdjust: -10, 

  get xPos() {
    return this._xPos;
  },

  get yPos() {
    return this._yPos;
  },

  get tag() {
    return this._tag;
  },

  set tag(aVal) {
    this._tag = aVal;
  },

  get dragging() {
    return this._element.customDragger.dragging;
  },

  shutdown: function shutdown() {
    this._element.hidden = true;
    this._element.customDragger.shutdown = true;
    delete this._element.customDragger;
    this._selectionHelperUI = null;
    this._element = null;
  },

  setTrackBounds: function setTrackBounds(aVerticalPlane, aHorizontalPlane) {
    // monocle boundaries
    this._hPlane = aHorizontalPlane;
    this._vPlane = aVerticalPlane;
  },

  show: function show() {
    this._element.hidden = false;
  },

  hide: function hide() {
    this._element.hidden = true;
  },

  get visible() {
    return this._element.hidden == false;
  },

  position: function position(aX, aY) {
    this._xPos = aX;
    this._yPos = aY;
    this._setPosition();
  },

  _setPosition: function _setPosition() {
    this._element.left = this._xPos + "px";
    this._element.top = this._yPos + "px";
  },

  dragStart: function dragStart(aX, aY) {
    this._xDrag = 0;
    this._yDrag = 0;
    this._selectionHelperUI.markerDragStart(this);
  },

  dragStop: function dragStop(aDx, aDy) {
    this._selectionHelperUI.markerDragStop(this);
  },

  moveBy: function moveBy(aDx, aDy, aClientX, aClientY) {
    this._xPos -= aDx;
    this._yPos -= aDy;
    this._xDrag -= aDx;
    this._yDrag -= aDy;
    // Add a bit of hysteresis to our directional detection so "big fingers"
    // are detected accurately.
    let direction = "tbd";
    if (Math.abs(this._xDrag) > kDragHysteresisDistance ||
        Math.abs(this._yDrag) > kDragHysteresisDistance) {
      direction = (this._xDrag <= 0 && this._yDrag <= 0 ? "start" : "end");
    }
    // We may swap markers in markerDragMove. If markerDragMove
    // returns true keep processing, otherwise get out of here.
    if (this._selectionHelperUI.markerDragMove(this, direction)) {
      this._setPosition();
    }
  },

  hitTest: function hitTest(aX, aY) {
    // Gets the pointer of the arrow right in the middle of the
    // monocle.
    aY += this._monocleYHitTextAdjust;
    aX += this._monocleXHitTextAdjust;
    if (aX >= (this._xPos - this._monocleRadius) &&
        aX <= (this._xPos + this._monocleRadius) &&
        aY >= (this._yPos - this._monocleRadius) &&
        aY <= (this._yPos + this._monocleRadius))
      return true;
    return false;
  },

  swapMonocle: function swapMonocle(aCaret) {
    let targetElement = aCaret._element;
    let targetElementId = aCaret._elementId;

    aCaret._element = this._element;
    aCaret._element.customDragger._marker = aCaret;
    aCaret._elementId = this._elementId;

    this._xPos = aCaret._xPos;
    this._yPos = aCaret._yPos;
    this._element = targetElement;
    this._element.customDragger._marker = this;
    this._elementId = targetElementId;
    this._element.visible = true;
  },

};

/*
 * SelectionHelperUI
 */

var SelectionHelperUI = {
  _debugEvents: false,
  _msgTarget: null,
  _startMark: null,
  _endMark: null,
  _caretMark: null,
  _target: null,
  _showAfterUpdate: false,
  _activeSelectionRect: null,
  _selectionMarkIds: [],
  _targetIsEditable: false,

  /*
   * Properties
   */

  get startMark() {
    if (this._startMark == null) {
      this._startMark = new Marker(this, "start", this._selectionMarkIds.pop(), 0, 0);
    }
    return this._startMark;
  },

  get endMark() {
    if (this._endMark == null) {
      this._endMark = new Marker(this, "end", this._selectionMarkIds.pop(), 0, 0);
    }
    return this._endMark;
  },

  get caretMark() {
    if (this._caretMark == null) {
      this._caretMark = new Marker(this, "caret", this._selectionMarkIds.pop(), 0, 0);
    }
    return this._caretMark;
  },

  get overlay() {
    return document.getElementById(this.layerMode == kChromeLayer ?
      "chrome-selection-overlay" : "content-selection-overlay");
  },

  get layerMode() {
    if (this._msgTarget && this._msgTarget instanceof SelectionPrototype)
      return kChromeLayer;
    return kContentLayer;
  },

  /*
   * isActive (prop)
   *
   * Determines if a selection edit session is currently active.
   */
  get isActive() {
    return this._msgTarget ? true : false;
  },

  /*
   * isSelectionUIVisible (prop)
   *
   * Determines if edit session monocles are visible. Useful
   * in checking if selection handler is setup for tests.
   */
  get isSelectionUIVisible() {
    if (!this._msgTarget || !this._startMark)
      return false;
    return this._startMark.visible;
  },

  /*
   * isCaretUIVisible (prop)
   *
   * Determines if caret browsing monocle is visible. Useful
   * in checking if selection handler is setup for tests.
   */
  get isCaretUIVisible() {
    if (!this._msgTarget || !this._caretMark)
      return false;
    return this._caretMark.visible;
  },

  /*
   * hasActiveDrag (prop)
   *
   * Determines if a marker is actively being dragged (missing call
   * to markerDragStop). Useful in checking if selection handler is
   * setup for tests.
   */
  get hasActiveDrag() {
    if (!this._msgTarget)
      return false;
    if ((this._caretMark && this._caretMark.dragging) ||
        (this._startMark && this._startMark.dragging) ||
        (this._endMark && this._endMark.dragging))
      return true;
    return false;
  },


  /*
   * Observers
   */

  observe: function (aSubject, aTopic, aData) {
    switch (aTopic) {
      case "attach_edit_session_to_content":
        // We receive this from text input bindings when this module
        // isn't accessible.
        this.chromeTextboxClick(aSubject);
        break;

      case "apzc-transform-begin":
        if (this.isActive && this.layerMode == kContentLayer) {
          this._hideMonocles();
        }
        break;

      case "apzc-transform-end":
        // The selection range callback will check to see if the new
        // position is off the screen, in which case it shuts down and
        // clears the selection.
        if (this.isActive && this.layerMode == kContentLayer) {
          this._showAfterUpdate = true;
          this._sendAsyncMessage("Browser:SelectionUpdate", {
            isInitiatedByAPZC: true
          });
        }
        break;
      }
  },

  /*
   * Public apis
   */

  /*
   * pingSelectionHandler
   * 
   * Ping the SelectionHandler and wait for the right response. Insures
   * all previous messages have been received. Useful in checking if
   * selection handler is setup for tests.
   *
   * @return a promise
   */
  pingSelectionHandler: function pingSelectionHandler() {
    if (!this.isActive)
      return null;

    if (this._pingCount == undefined) {
      this._pingCount = 0;
      this._pingArray = [];
    }

    this._pingCount++;

    let deferred = Promise.defer();
    this._pingArray.push({
      id: this._pingCount,
      deferred: deferred
    });

    this._sendAsyncMessage("Browser:SelectionHandlerPing", { id: this._pingCount });
    return deferred.promise;
  },

  /*
   * openEditSession
   *
   * Attempts to select underlying text at a point and begins editing
   * the section.
   *
   * @param aMsgTarget - Browser or chrome message target
   * @param aX, aY - Browser relative client coordinates.
   * @param aSetFocus - (optional) For form inputs, requests that the focus
   * be set to the element.
   */
  openEditSession: function openEditSession(aMsgTarget, aX, aY, aSetFocus) {
    if (!aMsgTarget || this.isActive)
      return;
    this._init(aMsgTarget);
    this._setupDebugOptions();
    let setFocus = aSetFocus || false;
    // Send this over to SelectionHandler in content, they'll message us
    // back with information on the current selection. SelectionStart
    // takes client coordinates.
    this._sendAsyncMessage("Browser:SelectionStart", {
      setFocus: setFocus,
      xPos: aX,
      yPos: aY
    });
  },

  /*
   * attachEditSession
   *
   * Attaches to existing selection and begins editing.
   *
   * @param aMsgTarget - Browser or chrome message target
   * @param aX, aY - Browser relative client coordinates.
   */
  attachEditSession: function attachEditSession(aMsgTarget, aX, aY) {
    if (!aMsgTarget || this.isActive)
      return;
    this._init(aMsgTarget);
    this._setupDebugOptions();

    // Send this over to SelectionHandler in content, they'll message us
    // back with information on the current selection. SelectionAttach
    // takes client coordinates.
    this._sendAsyncMessage("Browser:SelectionAttach", {
      xPos: aX,
      yPos: aY
    });
  },

  /*
   * attachToCaret
   * 
   * Initiates a touch caret selection session for a text input.
   * Can be called multiple times to move the caret marker around.
   *
   * Note the caret marker is pretty limited in functionality. The
   * only thing is can do is be displayed at the caret position.
   * Once the user starts a drag, the caret marker is hidden, and
   * the start and end markers take over.
   *
   * @param aMsgTarget - Browser or chrome message target
   * @param aX, aY - Browser relative client coordinates of the tap
   * that initiated the session.
   */
  attachToCaret: function attachToCaret(aMsgTarget, aX, aY) {
    if (!this.isActive) {
      this._init(aMsgTarget);
      this._setupDebugOptions();
    } else {
      this._hideMonocles();
    }

    this._lastPoint = { xPos: aX, yPos: aY };

    this._sendAsyncMessage("Browser:CaretAttach", {
      xPos: aX,
      yPos: aY
    });
  },

  /*
   * canHandleContextMenuMsg
   *
   * Determines if we can handle a ContextMenuHandler message.
   */
  canHandleContextMenuMsg: function canHandleContextMenuMsg(aMessage) {
    if (aMessage.json.types.indexOf("content-text") != -1)
      return true;
    return false;
  },

  /*
   * closeEditSession(aClearSelection)
   *
   * Closes an active edit session and shuts down. Does not clear existing
   * selection regions if they exist.
   *
   * @param aClearSelection bool indicating if the selection handler should also
   * clear any selection. optional, the default is false.
   */
  closeEditSession: function closeEditSession(aClearSelection) {
    if (!this.isActive) {
      return;
    }
    // This will callback in _selectionHandlerShutdown in
    // which we will call _shutdown().
    let clearSelection = aClearSelection || false;
    this._sendAsyncMessage("Browser:SelectionClose", {
      clearSelection: clearSelection
    });
  },

  /*
   * Event handler on the navbar text input. Called from navbar bindings
   * when focus is applied to the edit.
   */
  urlbarTextboxClick: function(aEdit) {
    // workaround for bug 925457: taping browser chrome resets last tap
    // co-ordinates to 'undefined' so that we know not to shift the browser
    // when the keyboard is up in SelectionHandler's _calcNewContentPosition().
    Browser.selectedTab.browser.messageManager.sendAsyncMessage("Browser:ResetLastPos", {
      xPos: null,
      yPos: null
    });

    if (InputSourceHelper.isPrecise || !aEdit.textLength) {
      return;
    }

    // Enable selection when there's text in the control
    let innerRect = aEdit.inputField.getBoundingClientRect();
    this.attachEditSession(ChromeSelectionHandler,
                           innerRect.left,
                           innerRect.top);
  },

  /*
   * Click handler for chrome pages loaded into the browser (about:config).
   * Called from the text input bindings via the attach_edit_session_to_content
   * observer.
   */
  chromeTextboxClick: function (aEvent) {
    this.attachEditSession(Browser.selectedTab.browser,
                           aEvent.clientX, aEvent.clientY);
  },

  /*
   * Handy debug routines that work independent of selection. They
   * make use of the selection overlay for drawing points.
   */

  debugDisplayDebugPoint: function (aLeft, aTop, aSize, aCssColorStr, aFill) {
    this.overlay.enabled = true;
    this.overlay.displayDebugLayer = true;
    this.overlay.addDebugRect(aLeft, aTop, aLeft + aSize, aTop + aSize,
                              aCssColorStr, aFill);
  },

  debugClearDebugPoints: function () {
    this.overlay.displayDebugLayer = false;
    if (!this._msgTarget) {
      this.overlay.enabled = false;
    }
  },

  /*
   * Init and shutdown
   */

  init: function () {
    let os = Services.obs;
    os.addObserver(this, "attach_edit_session_to_content", false);
    os.addObserver(this, "apzc-transform-begin", false);
    os.addObserver(this, "apzc-transform-end", false);
  },

  _init: function _init(aMsgTarget) {
    // store the target message manager
    this._msgTarget = aMsgTarget;

    // Init our list of available monocle ids
    this._setupMonocleIdArray();

    // Init selection rect info
    this._activeSelectionRect = Util.getCleanRect();
    this._targetElementRect = Util.getCleanRect();

    // SelectionHandler messages
    messageManager.addMessageListener("Content:SelectionRange", this);
    messageManager.addMessageListener("Content:SelectionCopied", this);
    messageManager.addMessageListener("Content:SelectionFail", this);
    messageManager.addMessageListener("Content:SelectionDebugRect", this);
    messageManager.addMessageListener("Content:HandlerShutdown", this);
    messageManager.addMessageListener("Content:SelectionHandlerPong", this);

    // capture phase
    window.addEventListener("keypress", this, true);
    window.addEventListener("MozPrecisePointer", this, true);
    window.addEventListener("MozDeckOffsetChanging", this, true);
    window.addEventListener("MozDeckOffsetChanged", this, true);
    window.addEventListener("KeyboardChanged", this, true);

    // bubble phase
    window.addEventListener("click", this, false);
    window.addEventListener("touchstart", this, false);

    Elements.browsers.addEventListener("URLChanged", this, true);
    Elements.browsers.addEventListener("SizeChanged", this, true);

    Elements.tabList.addEventListener("TabSelect", this, true);

    Elements.navbar.addEventListener("transitionend", this, true);

    this.overlay.enabled = true;
  },

  _shutdown: function _shutdown() {
    messageManager.removeMessageListener("Content:SelectionRange", this);
    messageManager.removeMessageListener("Content:SelectionCopied", this);
    messageManager.removeMessageListener("Content:SelectionFail", this);
    messageManager.removeMessageListener("Content:SelectionDebugRect", this);
    messageManager.removeMessageListener("Content:HandlerShutdown", this);
    messageManager.removeMessageListener("Content:SelectionHandlerPong", this);

    window.removeEventListener("keypress", this, true);
    window.removeEventListener("MozPrecisePointer", this, true);
    window.removeEventListener("MozDeckOffsetChanging", this, true);
    window.removeEventListener("MozDeckOffsetChanged", this, true);
    window.removeEventListener("KeyboardChanged", this, true);

    window.removeEventListener("click", this, false);
    window.removeEventListener("touchstart", this, false);

    Elements.browsers.removeEventListener("URLChanged", this, true);
    Elements.browsers.removeEventListener("SizeChanged", this, true);

    Elements.tabList.removeEventListener("TabSelect", this, true);

    Elements.navbar.removeEventListener("transitionend", this, true);

    this._shutdownAllMarkers();

    this._selectionMarkIds = [];
    this._msgTarget = null;
    this._activeSelectionRect = null;

    this.overlay.displayDebugLayer = false;
    this.overlay.enabled = false;
  },

  /*
   * Utilities
   */

  /*
   * _swapCaretMarker
   *
   * Swap two drag markers - used when transitioning from caret mode
   * to selection mode. We take the current caret marker (which is in a
   * drag state) and swap it out with one of the selection markers.
   */
  _swapCaretMarker: function _swapCaretMarker(aDirection) {
    let targetMark = null;
    if (aDirection == "start")
      targetMark = this.startMark;
    else
      targetMark = this.endMark;
    let caret = this.caretMark;
    targetMark.swapMonocle(caret);
    let id = caret._elementId;
    caret.shutdown();
    this._caretMark = null;
    this._selectionMarkIds.push(id);
  },

  /*
   * _transitionFromCaretToSelection
   *
   * Transitions from caret mode to text selection mode.
   */
  _transitionFromCaretToSelection: function _transitionFromCaretToSelection(aDirection) {
    // Get selection markers initialized if they aren't already
    { let mark = this.startMark; mark = this.endMark; }

    // Swap the caret marker out for the start or end marker depending
    // on direction.
    this._swapCaretMarker(aDirection);

    let targetMark = null;
    if (aDirection == "start")
      targetMark = this.startMark;
    else
      targetMark = this.endMark;

    // Position both in the same starting location.
    this.startMark.position(targetMark.xPos, targetMark.yPos);
    this.endMark.position(targetMark.xPos, targetMark.yPos);

    // We delay transitioning until we know which direction the user is dragging
    // based on a hysteresis value in the drag marker code. Down in our caller, we
    // cache the first drag position in _cachedCaretPos so we can select from the
    // initial caret drag position. Use those values if we have them. (Note
    // _cachedCaretPos has already been translated in _getMarkerBaseMessage.)
    let xpos = this._cachedCaretPos ? this._cachedCaretPos.xPos :
      this._msgTarget.ctobx(targetMark.xPos, true);
    let ypos = this._cachedCaretPos ? this._cachedCaretPos.yPos :
      this._msgTarget.ctoby(targetMark.yPos, true);

    // Start the selection monocle drag. SelectionHandler relies on this
    // for getting initialized. This will also trigger a message back for
    // monocle positioning. Note, markerDragMove is still on the stack in
    // this call!
    this._sendAsyncMessage("Browser:SelectionSwitchMode", {
      newMode: "selection",
      change: targetMark.tag,
      xPos: xpos,
      yPos: ypos,
    });
  },

  /*
   * _setupDebugOptions
   *
   * Sends a message over to content instructing it to
   * turn on various debug features.
   */
  _setupDebugOptions: function _setupDebugOptions() {
    // Debug options for selection
    let debugOpts = { dumpRanges: false, displayRanges: false, dumpEvents: false };
    try {
      if (Services.prefs.getBoolPref(kDebugSelectionDumpPref))
        debugOpts.displayRanges = true;
    } catch (ex) {}
    try {
      if (Services.prefs.getBoolPref(kDebugSelectionDisplayPref))
        debugOpts.displayRanges = true;
    } catch (ex) {}
    try {
      if (Services.prefs.getBoolPref(kDebugSelectionDumpEvents)) {
        debugOpts.dumpEvents = true;
        this._debugEvents = true;
      }
    } catch (ex) {}

    if (debugOpts.displayRanges || debugOpts.dumpRanges || debugOpts.dumpEvents) {
      // Turn on the debug layer
      this.overlay.displayDebugLayer = true;
      // Tell SelectionHandler what to do
      this._sendAsyncMessage("Browser:SelectionDebug", debugOpts);
    }
  },

  /*
   * _sendAsyncMessage
   *
   * Helper for sending a message to SelectionHandler.
   */
  _sendAsyncMessage: function _sendAsyncMessage(aMsg, aJson) {
    if (!this._msgTarget) {
      if (this._debugEvents)
        Util.dumpLn("SelectionHelperUI sendAsyncMessage could not send", aMsg);
      return;
    }
    if (this._msgTarget && this._msgTarget instanceof SelectionPrototype) {
      this._msgTarget.msgHandler(aMsg, aJson);
    } else {
      this._msgTarget.messageManager.sendAsyncMessage(aMsg, aJson);
    }
  },

  _checkForActiveDrag: function _checkForActiveDrag() {
    return (this.startMark.dragging || this.endMark.dragging ||
            this.caretMark.dragging);
  },

  _hitTestSelection: function _hitTestSelection(aEvent) {
    // Ignore if the double tap isn't on our active selection rect.
    if (this._activeSelectionRect &&
        Util.pointWithinRect(aEvent.clientX, aEvent.clientY, this._activeSelectionRect)) {
      return true;
    }
    return false;
  },

  /*
   * _setCaretPositionAtPoint - sets the current caret position.
   *
   * @param aX, aY - browser relative client coordinates
   */
  _setCaretPositionAtPoint: function _setCaretPositionAtPoint(aX, aY) {
    let json = this._getMarkerBaseMessage("caret");
    json.caret.xPos = aX;
    json.caret.yPos = aY;
    this._sendAsyncMessage("Browser:CaretUpdate", json);
  },

  /*
   * _shutdownAllMarkers
   *
   * Helper for shutting down all markers and
   * freeing the objects associated with them.
   */
  _shutdownAllMarkers: function _shutdownAllMarkers() {
    if (this._startMark)
      this._startMark.shutdown();
    if (this._endMark)
      this._endMark.shutdown();
    if (this._caretMark)
      this._caretMark.shutdown();

    this._startMark = null;
    this._endMark = null;
    this._caretMark = null;
  },

  /*
   * _setupMonocleIdArray
   *
   * Helper for initing the array of monocle anon ids.
   */
  _setupMonocleIdArray: function _setupMonocleIdArray() {
    this._selectionMarkIds = ["selectionhandle-mark1",
                              "selectionhandle-mark2",
                              "selectionhandle-mark3"];
  },

  _hideMonocles: function _hideMonocles() {
    if (this._startMark) {
      this.startMark.hide();
    }
    if (this._endMark) {
      this.endMark.hide();
    }
    if (this._caretMark) {
      this.caretMark.hide();
    }
  },

  _showMonocles: function _showMonocles(aSelection) {
    if (!aSelection) {
      if (this._checkMonocleVisibility(this.caretMark.xPos, this.caretMark.yPos)) {
        this.caretMark.show();
      }
    } else {
      if (this._checkMonocleVisibility(this.endMark.xPos, this.endMark.yPos)) {
        this.endMark.show();
      }
      if (this._checkMonocleVisibility(this.startMark.xPos, this.startMark.yPos)) {
        this.startMark.show();
      }
    }
  },

  _checkMonocleVisibility: function(aX, aY) {
    let viewport = Browser.selectedBrowser.contentViewportBounds;
    aX = this._msgTarget.ctobx(aX);
    aY = this._msgTarget.ctoby(aY);
    if (aX < viewport.x || aY < viewport.y ||
        aX > (viewport.x + viewport.width) ||
        aY > (viewport.y + viewport.height)) {
      return false;
    }
    return true;
  },

  /*
   * Event handlers for document events
   */

  /*
   * Handles taps that move the current caret around in text edits,
   * clear active selection and focus when neccessary, or change
   * modes. Only active afer SelectionHandlerUI is initialized.
   */
  _onClick: function(aEvent) {
    if (this.layerMode == kChromeLayer && this._targetIsEditable) {
      this.attachToCaret(this._msgTarget, aEvent.clientX, aEvent.clientY);
    }
  },

  _onKeypress: function _onKeypress() {
    this.closeEditSession();
  },

  _onResize: function _onResize() {
    this._sendAsyncMessage("Browser:SelectionUpdate", {});
  },

  /*
   * _onDeckOffsetChanging - fired by ContentAreaObserver before the browser
   * deck is shifted for form input access in response to a soft keyboard
   * display.
   */
  _onDeckOffsetChanging: function _onDeckOffsetChanging(aEvent) {
    // Hide the monocles temporarily
    this._hideMonocles();
  },

  /*
   * _onDeckOffsetChanged - fired by ContentAreaObserver after the browser
   * deck is shifted for form input access in response to a soft keyboard
   * display.
   */
  _onDeckOffsetChanged: function _onDeckOffsetChanged(aEvent) {
    // Update the monocle position and display
    this.attachToCaret(null, this._lastPoint.xPos, this._lastPoint.yPos);
  },

  /*
   * Detects when the nav bar transitions, so we can enable selection at the
   * appropriate location once the transition is complete, or shutdown
   * selection down when the nav bar is hidden.
   */
  _onNavBarTransitionEvent: function _onNavBarTransitionEvent(aEvent) {
    // Ignore when selection is in content
    if (this.layerMode == kContentLayer) {
      return;
    }

    // After tansitioning up, show the monocles
    if (Elements.navbar.isShowing) {
      this._showAfterUpdate = true;
      this._sendAsyncMessage("Browser:SelectionUpdate", {});
    }
  },

  _onKeyboardChangedEvent: function _onKeyboardChangedEvent() {
    if (!this.isActive || this.layerMode == kContentLayer) {
      return;
    }
    this._sendAsyncMessage("Browser:SelectionUpdate", {});
  },

  /*
   * Event handlers for message manager
   */

  _onDebugRectRequest: function _onDebugRectRequest(aMsg) {
    this.overlay.addDebugRect(aMsg.left, aMsg.top, aMsg.right, aMsg.bottom,
                              aMsg.color, aMsg.fill, aMsg.id);
  },

  _selectionHandlerShutdown: function _selectionHandlerShutdown() {
    this._shutdown();
  },

  /*
   * Message handlers
   */

  _onSelectionCopied: function _onSelectionCopied(json) {
    this.closeEditSession(true);
  },

  _onSelectionRangeChange: function _onSelectionRangeChange(json) {
    let haveSelectionRect = true;

    if (json.updateStart) {
      let x = this._msgTarget.btocx(json.start.xPos, true);
      let y = this._msgTarget.btocy(json.start.yPos, true);
      this.startMark.position(x, y);
    }

    if (json.updateEnd) {
      let x = this._msgTarget.btocx(json.end.xPos, true);
      let y = this._msgTarget.btocy(json.end.yPos, true);
      this.endMark.position(x, y);
    }

    if (json.updateCaret) {
      let x = this._msgTarget.btocx(json.caret.xPos, true);
      let y = this._msgTarget.btocy(json.caret.yPos, true);
      // If selectionRangeFound is set SelectionHelper found a range we can
      // attach to. If not, there's no text in the control, and hence no caret
      // position information we can use.
      haveSelectionRect = json.selectionRangeFound;
      if (json.selectionRangeFound) {
        this.caretMark.position(x, y);
        this._showMonocles(false);
      }
    }

    if (this._showAfterUpdate) {
      this._showAfterUpdate = false;
      this._showMonocles(!json.updateCaret);
    }

    this._targetIsEditable = json.targetIsEditable;
    this._activeSelectionRect = haveSelectionRect ?
      this._msgTarget.rectBrowserToClient(json.selection, true) :
      this._activeSelectionRect = Util.getCleanRect();
    this._targetElementRect =
      this._msgTarget.rectBrowserToClient(json.element, true);

    // If this is the end of a selection move show the appropriate
    // monocle images. src=(start, update, end, caret)
    if (json.src == "start" || json.src == "end") {
      this._showMonocles(true);
    }
  },

  _onSelectionFail: function _onSelectionFail() {
    Util.dumpLn("failed to get a selection.");
    this.closeEditSession();
  },

  /*
   * _onPong
   *
   * Handles the closure of promise we return when we send a ping
   * to SelectionHandler in pingSelectionHandler. Testing use.
   */
  _onPong: function _onPong(aId) {
    let ping = this._pingArray.pop();
    if (ping.id != aId) {
      ping.deferred.reject(
        new Error("Selection module's pong doesn't match our last ping."));
    }
    ping.deferred.resolve();
   },

  /*
   * Events
   */

  handleEvent: function handleEvent(aEvent) {
    if (this._debugEvents && aEvent.type != "touchmove") {
      Util.dumpLn("SelectionHelperUI:", aEvent.type);
    }
    switch (aEvent.type) {
      case "click":
        this._onClick(aEvent);
        break;

      case "touchstart": {
        if (aEvent.touches.length != 1)
          break;
        // Only prevent default if we're dragging so that
        // APZC doesn't scroll.
        if (this._checkForActiveDrag()) {
          aEvent.preventDefault();
        }
        break;
      }

      case "keypress":
        this._onKeypress(aEvent);
        break;

      case "SizeChanged":
        this._onResize(aEvent);
        break;

      case "URLChanged":
      case "TabSelect":
        this._shutdown();
        break;

      case "MozPrecisePointer":
        this.closeEditSession(true);
        break;

      case "MozDeckOffsetChanging":
        this._onDeckOffsetChanging(aEvent);
        break;

      case "MozDeckOffsetChanged":
        this._onDeckOffsetChanged(aEvent);
        break;

      case "transitionend":
        this._onNavBarTransitionEvent(aEvent);
        break;

      case "KeyboardChanged":
        this._onKeyboardChangedEvent();
        break;
    }
  },

  receiveMessage: function sh_receiveMessage(aMessage) {
    if (this._debugEvents) Util.dumpLn("SelectionHelperUI:", aMessage.name);
    let json = aMessage.json;
    switch (aMessage.name) {
      case "Content:SelectionFail":
        this._onSelectionFail();
        break;
      case "Content:SelectionRange":
        this._onSelectionRangeChange(json);
        break;
      case "Content:SelectionCopied":
        this._onSelectionCopied(json);
        break;
      case "Content:SelectionDebugRect":
        this._onDebugRectRequest(json);
        break;
      case "Content:HandlerShutdown":
        this._selectionHandlerShutdown();
        break;
      case "Content:SelectionHandlerPong":
        this._onPong(json.id);
        break;
    }
  },

  /*
   * Callbacks from markers
   */

  _getMarkerBaseMessage: function _getMarkerBaseMessage(aMarkerTag) {
    return {
      change: aMarkerTag,
      start: {
        xPos: this._msgTarget.ctobx(this.startMark.xPos, true),
        yPos: this._msgTarget.ctoby(this.startMark.yPos, true)
      },
      end: {
        xPos: this._msgTarget.ctobx(this.endMark.xPos, true),
        yPos: this._msgTarget.ctoby(this.endMark.yPos, true)
      },
      caret: {
        xPos: this._msgTarget.ctobx(this.caretMark.xPos, true),
        yPos: this._msgTarget.ctoby(this.caretMark.yPos, true)
      },
    };
  },

  markerDragStart: function markerDragStart(aMarker) {
    let json = this._getMarkerBaseMessage(aMarker.tag);
    if (aMarker.tag == "caret") {
      this._cachedCaretPos = null;
      this._sendAsyncMessage("Browser:CaretMove", json);
      return;
    }
    this._sendAsyncMessage("Browser:SelectionMoveStart", json);
  },

  markerDragStop: function markerDragStop(aMarker) {
    let json = this._getMarkerBaseMessage(aMarker.tag);
    if (aMarker.tag == "caret") {
      this._sendAsyncMessage("Browser:CaretUpdate", json);
      return;
    }
    this._sendAsyncMessage("Browser:SelectionMoveEnd", json);
  },

  markerDragMove: function markerDragMove(aMarker, aDirection) {
    if (aMarker.tag == "caret") {
      // If direction is "tbd" the drag monocle hasn't determined which
      // direction the user is dragging.
      if (aDirection != "tbd") {
        // We are going to transition from caret browsing mode to selection
        // mode on drag. So swap the caret monocle for a start or end monocle
        // depending on the direction of the drag, and start selecting text.
        this._transitionFromCaretToSelection(aDirection);
        return false;
      }
      // Cache for when we start the drag in _transitionFromCaretToSelection.
      if (!this._cachedCaretPos) {
        this._cachedCaretPos = this._getMarkerBaseMessage(aMarker.tag).caret;
      }
      return true;
    }
    this._cachedCaretPos = null;

    // We'll re-display these after the drag is complete.
    this._hideMonocles();

    let json = this._getMarkerBaseMessage(aMarker.tag);
    this._sendAsyncMessage("Browser:SelectionMove", json);
    return true;
  },
};
