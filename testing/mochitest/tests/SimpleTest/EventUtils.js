/**
 * EventUtils provides some utility methods for creating and sending DOM events.
 * Current methods:
 *  sendMouseEvent
 *  sendDragEvent
 *  sendChar
 *  sendString
 *  sendKey
 *  sendWheelAndPaint
 *  sendWheelAndPaintNoFlush
 *  synthesizeMouse
 *  synthesizeMouseAtCenter
 *  synthesizeWheel
 *  synthesizeWheelAtPoint
 *  synthesizeKey
 *  synthesizeNativeKey
 *  synthesizeMouseExpectEvent
 *  synthesizeKeyExpectEvent
 *  synthesizeNativeOSXClick
 *  synthesizeDragOver
 *  synthesizeDropAfterDragOver
 *  synthesizeDrop
 *  synthesizePlainDragAndDrop
 *
 *  When adding methods to this file, please add a performance test for it.
 */

// This file is used both in privileged and unprivileged contexts, so we have to
// be careful about our access to Components.interfaces. We also want to avoid
// naming collisions with anything that might be defined in the scope that imports
// this script.
//
// Even if the real |Components| doesn't exist, we might shim in a simple JS
// placebo for compat. An easy way to differentiate this from the real thing
// is whether the property is read-only or not.  The real |Components| property
// is read-only.
window.__defineGetter__('_EU_Ci', function() {
  var c = Object.getOwnPropertyDescriptor(window, 'Components');
  return c && c.value && !c.writable ? Ci : SpecialPowers.Ci;
});

window.__defineGetter__('_EU_Cc', function() {
  var c = Object.getOwnPropertyDescriptor(window, 'Components');
  return c && c.value && !c.writable ? Cc : SpecialPowers.Cc;
});

window.__defineGetter__('_EU_Cu', function() {
  var c = Object.getOwnPropertyDescriptor(window, 'Components');
  return c && c.value && !c.writable ? Cu : SpecialPowers.Cu;
});

window.__defineGetter__("_EU_OS", function() {
  delete this._EU_OS;
  try {
    this._EU_OS = this._EU_Cu.import("resource://gre/modules/AppConstants.jsm", {}).platform;
  } catch (ex) {
    this._EU_OS = null;
  }
  return this._EU_OS;
});

function _EU_isMac(aWindow = window) {
  if (window._EU_OS) {
    return window._EU_OS == "macosx";
  }
  if (aWindow) {
    try {
      return aWindow.navigator.platform.indexOf("Mac") > -1;
    } catch (ex) {}
  }
  return navigator.platform.indexOf("Mac") > -1;
}

function _EU_isWin(aWindow = window) {
  if (window._EU_OS) {
    return window._EU_OS == "win";
  }
  if (aWindow) {
    try {
      return aWindow.navigator.platform.indexOf("Win") > -1;
    } catch (ex) {}
  }
  return navigator.platform.indexOf("Win") > -1;
}

function _EU_isLinux(aWindow = window) {
  if (window._EU_OS) {
    return window._EU_OS == "linux";
  }
  if (aWindow) {
    try {
      return aWindow.navigator.platform.startsWith("Linux");
    } catch (ex) {}
  }
  return navigator.platform.startsWith("Linux");
}

function _EU_isAndroid(aWindow = window) {
  if (window._EU_OS) {
    return window._EU_OS == "android";
  }
  if (aWindow) {
    try {
      return aWindow.navigator.userAgent.includes("Android");
    } catch (ex) {}
  }
  return navigator.userAgent.includes("Android");
}

function _EU_maybeWrap(o) {
  // We're used in some contexts where there is no SpecialPowers and also in
  // some where it exists but has no wrap() method.  And this is somewhat
  // independent of whether window.Components is a thing...
  var haveWrap = false;
  try {
    haveWrap = SpecialPowers.wrap != undefined;
  } catch (e) {
    // Just leave it false.
  }
  if (!haveWrap) {
    // Not much we can do here.
    return o;
  }
  var c = Object.getOwnPropertyDescriptor(window, 'Components');
  return c && c.value && !c.writable ? o : SpecialPowers.wrap(o);
}

function _EU_maybeUnwrap(o) {
  var c = Object.getOwnPropertyDescriptor(window, 'Components');
  return c && c.value && !c.writable ? o : SpecialPowers.unwrap(o);
}

/**
 * Send a mouse event to the node aTarget (aTarget can be an id, or an
 * actual node) . The "event" passed in to aEvent is just a JavaScript
 * object with the properties set that the real mouse event object should
 * have. This includes the type of the mouse event.
 * E.g. to send an click event to the node with id 'node' you might do this:
 *
 * sendMouseEvent({type:'click'}, 'node');
 */
function getElement(id) {
  return ((typeof(id) == "string") ?
    document.getElementById(id) : id); 
};   

this.$ = this.getElement;

function computeButton(aEvent) {
  if (typeof aEvent.button != 'undefined') {
    return aEvent.button;
  }
  return aEvent.type == 'contextmenu' ? 2 : 0;
}

function sendMouseEvent(aEvent, aTarget, aWindow) {
  if (!['click', 'contextmenu', 'dblclick', 'mousedown', 'mouseup', 'mouseover', 'mouseout'].includes(aEvent.type)) {
    throw new Error("sendMouseEvent doesn't know about event type '" + aEvent.type + "'");
  }

  if (!aWindow) {
    aWindow = window;
  }

  if (typeof aTarget == "string") {
    aTarget = aWindow.document.getElementById(aTarget);
  }

  var event = aWindow.document.createEvent('MouseEvent');

  var typeArg          = aEvent.type;
  var canBubbleArg     = true;
  var cancelableArg    = true;
  var viewArg          = aWindow;
  var detailArg        = aEvent.detail        || (aEvent.type == 'click'     ||
                                                  aEvent.type == 'mousedown' ||
                                                  aEvent.type == 'mouseup' ? 1 :
                                                  aEvent.type == 'dblclick'? 2 : 0);
  var screenXArg       = aEvent.screenX       || 0;
  var screenYArg       = aEvent.screenY       || 0;
  var clientXArg       = aEvent.clientX       || 0;
  var clientYArg       = aEvent.clientY       || 0;
  var ctrlKeyArg       = aEvent.ctrlKey       || false;
  var altKeyArg        = aEvent.altKey        || false;
  var shiftKeyArg      = aEvent.shiftKey      || false;
  var metaKeyArg       = aEvent.metaKey       || false;
  var buttonArg        = computeButton(aEvent);
  var relatedTargetArg = aEvent.relatedTarget || null;

  event.initMouseEvent(typeArg, canBubbleArg, cancelableArg, viewArg, detailArg,
                       screenXArg, screenYArg, clientXArg, clientYArg,
                       ctrlKeyArg, altKeyArg, shiftKeyArg, metaKeyArg,
                       buttonArg, relatedTargetArg);

  // If documentURIObject exists or `window` is a stub object, we're in
  // a chrome scope, so don't bother trying to go through SpecialPowers.
  if (!window.document || window.document.documentURIObject)
    return aTarget.dispatchEvent(event);
  return SpecialPowers.dispatchEvent(aWindow, aTarget, event);
}

function isHidden(aElement) {
  var box = aElement.getBoundingClientRect();
  return box.width == 0 && box.height == 0;
}

/**
 * Send a drag event to the node aTarget (aTarget can be an id, or an
 * actual node) . The "event" passed in to aEvent is just a JavaScript
 * object with the properties set that the real drag event object should
 * have. This includes the type of the drag event.
 */
function sendDragEvent(aEvent, aTarget, aWindow = window) {
  if (!['drag', 'dragstart', 'dragend', 'dragover', 'dragenter', 'dragleave', 'drop'].includes(aEvent.type)) {
    throw new Error("sendDragEvent doesn't know about event type '" + aEvent.type + "'");
  }

  if (typeof aTarget == "string") {
    aTarget = aWindow.document.getElementById(aTarget);
  }

  /*
   * Drag event cannot be performed if the element is hidden, except 'dragend'
   * event where the element can becomes hidden after start dragging.
   */
  if (aEvent.type != 'dragend' && isHidden(aTarget)) {
    var targetName = aTarget.nodeName;
    if ("id" in aTarget && aTarget.id) {
      targetName += "#" + aTarget.id;
    }
    throw new Error(`${aEvent.type} event target ${targetName} is hidden`);
  }

  var event = aWindow.document.createEvent('DragEvent');

  var typeArg          = aEvent.type;
  var canBubbleArg     = true;
  var cancelableArg    = true;
  var viewArg          = aWindow;
  var detailArg        = aEvent.detail        || 0;
  var screenXArg       = aEvent.screenX       || 0;
  var screenYArg       = aEvent.screenY       || 0;
  var clientXArg       = aEvent.clientX       || 0;
  var clientYArg       = aEvent.clientY       || 0;
  var ctrlKeyArg       = aEvent.ctrlKey       || false;
  var altKeyArg        = aEvent.altKey        || false;
  var shiftKeyArg      = aEvent.shiftKey      || false;
  var metaKeyArg       = aEvent.metaKey       || false;
  var buttonArg        = computeButton(aEvent);
  var relatedTargetArg = aEvent.relatedTarget || null;
  var dataTransfer     = aEvent.dataTransfer  || null;

  event.initDragEvent(typeArg, canBubbleArg, cancelableArg, viewArg, detailArg,
                      screenXArg, screenYArg, clientXArg, clientYArg,
                      ctrlKeyArg, altKeyArg, shiftKeyArg, metaKeyArg,
                      buttonArg, relatedTargetArg, dataTransfer);

  if (aEvent._domDispatchOnly) {
    return aTarget.dispatchEvent(event);
  }

  var utils = _getDOMWindowUtils(aWindow);
  return utils.dispatchDOMEventViaPresShell(aTarget, event, true);
}

/**
 * Send the char aChar to the focused element.  This method handles casing of
 * chars (sends the right charcode, and sends a shift key for uppercase chars).
 * No other modifiers are handled at this point.
 *
 * For now this method only works for ASCII characters and emulates the shift
 * key state on US keyboard layout.
 */
function sendChar(aChar, aWindow) {
  var hasShift;
  // Emulate US keyboard layout for the shiftKey state.
  switch (aChar) {
    case "!":
    case "@":
    case "#":
    case "$":
    case "%":
    case "^":
    case "&":
    case "*":
    case "(":
    case ")":
    case "_":
    case "+":
    case "{":
    case "}":
    case ":":
    case "\"":
    case "|":
    case "<":
    case ">":
    case "?":
      hasShift = true;
      break;
    default:
      hasShift = aChar.toLowerCase() != aChar.toUpperCase() &&
                 aChar == aChar.toUpperCase();
      break;
  }
  synthesizeKey(aChar, { shiftKey: hasShift }, aWindow);
}

/**
 * Send the string aStr to the focused element.
 *
 * For now this method only works for ASCII characters and emulates the shift
 * key state on US keyboard layout.
 */
function sendString(aStr, aWindow) {
  for (var i = 0; i < aStr.length; ++i) {
    sendChar(aStr.charAt(i), aWindow);
  }
}

/**
 * Send the non-character key aKey to the focused node.
 * The name of the key should be the part that comes after "DOM_VK_" in the
 *   KeyEvent constant name for this key.
 * No modifiers are handled at this point.
 */
function sendKey(aKey, aWindow) {
  var keyName = "VK_" + aKey.toUpperCase();
  synthesizeKey(keyName, { shiftKey: false }, aWindow);
}

/**
 * Parse the key modifier flags from aEvent. Used to share code between
 * synthesizeMouse and synthesizeKey.
 */
function _parseModifiers(aEvent, aWindow = window)
{
  var navigator = _getNavigator(aWindow);
  var nsIDOMWindowUtils = _EU_Ci.nsIDOMWindowUtils;
  var mval = 0;
  if (aEvent.shiftKey) {
    mval |= nsIDOMWindowUtils.MODIFIER_SHIFT;
  }
  if (aEvent.ctrlKey) {
    mval |= nsIDOMWindowUtils.MODIFIER_CONTROL;
  }
  if (aEvent.altKey) {
    mval |= nsIDOMWindowUtils.MODIFIER_ALT;
  }
  if (aEvent.metaKey) {
    mval |= nsIDOMWindowUtils.MODIFIER_META;
  }
  if (aEvent.accelKey) {
    mval |= _EU_isMac(aWindow) ?
      nsIDOMWindowUtils.MODIFIER_META : nsIDOMWindowUtils.MODIFIER_CONTROL;
  }
  if (aEvent.altGrKey) {
    mval |= nsIDOMWindowUtils.MODIFIER_ALTGRAPH;
  }
  if (aEvent.capsLockKey) {
    mval |= nsIDOMWindowUtils.MODIFIER_CAPSLOCK;
  }
  if (aEvent.fnKey) {
    mval |= nsIDOMWindowUtils.MODIFIER_FN;
  }
  if (aEvent.fnLockKey) {
    mval |= nsIDOMWindowUtils.MODIFIER_FNLOCK;
  }
  if (aEvent.numLockKey) {
    mval |= nsIDOMWindowUtils.MODIFIER_NUMLOCK;
  }
  if (aEvent.scrollLockKey) {
    mval |= nsIDOMWindowUtils.MODIFIER_SCROLLLOCK;
  }
  if (aEvent.symbolKey) {
    mval |= nsIDOMWindowUtils.MODIFIER_SYMBOL;
  }
  if (aEvent.symbolLockKey) {
    mval |= nsIDOMWindowUtils.MODIFIER_SYMBOLLOCK;
  }
  if (aEvent.osKey) {
    mval |= nsIDOMWindowUtils.MODIFIER_OS;
  }

  return mval;
}

/**
 * Synthesize a mouse event on a target. The actual client point is determined
 * by taking the aTarget's client box and offseting it by aOffsetX and
 * aOffsetY. This allows mouse clicks to be simulated by calling this method.
 *
 * aEvent is an object which may contain the properties:
 *   shiftKey, ctrlKey, altKey, metaKey, accessKey, clickCount, button, type
 *
 * If the type is specified, an mouse event of that type is fired. Otherwise,
 * a mousedown followed by a mouse up is performed.
 *
 * aWindow is optional, and defaults to the current window object.
 *
 * Returns whether the event had preventDefault() called on it.
 */
function synthesizeMouse(aTarget, aOffsetX, aOffsetY, aEvent, aWindow)
{
  var rect = aTarget.getBoundingClientRect();
  return synthesizeMouseAtPoint(rect.left + aOffsetX, rect.top + aOffsetY,
       aEvent, aWindow);
}
function synthesizeTouch(aTarget, aOffsetX, aOffsetY, aEvent, aWindow)
{
  var rect = aTarget.getBoundingClientRect();
  return synthesizeTouchAtPoint(rect.left + aOffsetX, rect.top + aOffsetY,
       aEvent, aWindow);
}

/*
 * Synthesize a mouse event at a particular point in aWindow.
 *
 * aEvent is an object which may contain the properties:
 *   shiftKey, ctrlKey, altKey, metaKey, accessKey, clickCount, button, type
 *
 * If the type is specified, an mouse event of that type is fired. Otherwise,
 * a mousedown followed by a mouse up is performed.
 *
 * aWindow is optional, and defaults to the current window object.
 */
function synthesizeMouseAtPoint(left, top, aEvent, aWindow = window)
{
  var utils = _getDOMWindowUtils(aWindow);
  var defaultPrevented = false;

  if (utils) {
    var button = computeButton(aEvent);
    var clickCount = aEvent.clickCount || 1;
    var modifiers = _parseModifiers(aEvent, aWindow);
    var pressure = ("pressure" in aEvent) ? aEvent.pressure : 0;

    // aWindow might be cross-origin from us.
    var MouseEvent = _EU_maybeWrap(aWindow).MouseEvent;

    // Default source to mouse.
    var inputSource = ("inputSource" in aEvent) ? aEvent.inputSource :
                                                  MouseEvent.MOZ_SOURCE_MOUSE;
    // Compute a pointerId if needed.
    var id;
    if ("id" in aEvent) {
      id = aEvent.id;
    } else {
      var isFromPen = inputSource === MouseEvent.MOZ_SOURCE_PEN;
      id = isFromPen ? utils.DEFAULT_PEN_POINTER_ID :
                       utils.DEFAULT_MOUSE_POINTER_ID;
    }

    var isDOMEventSynthesized =
      ("isSynthesized" in aEvent) ? aEvent.isSynthesized : true;
    var isWidgetEventSynthesized =
      ("isWidgetEventSynthesized" in aEvent) ? aEvent.isWidgetEventSynthesized : false;
    var buttons = ("buttons" in aEvent) ? aEvent.buttons :
                                          utils.MOUSE_BUTTONS_NOT_SPECIFIED;
    if (("type" in aEvent) && aEvent.type) {
      defaultPrevented = utils.sendMouseEvent(aEvent.type, left, top, button,
                                              clickCount, modifiers, false,
                                              pressure, inputSource,
                                              isDOMEventSynthesized,
                                              isWidgetEventSynthesized,
                                              buttons, id);
    }
    else {
      utils.sendMouseEvent("mousedown", left, top, button, clickCount, modifiers,
                           false, pressure, inputSource, isDOMEventSynthesized,
                           isWidgetEventSynthesized, buttons, id);
      utils.sendMouseEvent("mouseup", left, top, button, clickCount, modifiers,
                           false, pressure, inputSource, isDOMEventSynthesized,
                           isWidgetEventSynthesized, buttons, id);
    }
  }

  return defaultPrevented;
}

function synthesizeTouchAtPoint(left, top, aEvent, aWindow = window)
{
  var utils = _getDOMWindowUtils(aWindow);
  let defaultPrevented = false;

  if (utils) {
    var id = aEvent.id || utils.DEFAULT_TOUCH_POINTER_ID;
    var rx = aEvent.rx || 1;
    var ry = aEvent.ry || 1;
    var angle = aEvent.angle || 0;
    var force = aEvent.force || 1;
    var modifiers = _parseModifiers(aEvent, aWindow);

    if (("type" in aEvent) && aEvent.type) {
      defaultPrevented = utils.sendTouchEvent(aEvent.type, [id], [left], [top], [rx], [ry], [angle], [force], 1, modifiers);
    }
    else {
      utils.sendTouchEvent("touchstart", [id], [left], [top], [rx], [ry], [angle], [force], 1, modifiers);
      utils.sendTouchEvent("touchend", [id], [left], [top], [rx], [ry], [angle], [force], 1, modifiers);
    }
  }
  return defaultPrevented;
}

// Call synthesizeMouse with coordinates at the center of aTarget.
function synthesizeMouseAtCenter(aTarget, aEvent, aWindow)
{
  var rect = aTarget.getBoundingClientRect();
  return synthesizeMouse(aTarget, rect.width / 2, rect.height / 2, aEvent,
                         aWindow);
}
function synthesizeTouchAtCenter(aTarget, aEvent, aWindow)
{
  var rect = aTarget.getBoundingClientRect();
  synthesizeTouch(aTarget, rect.width / 2, rect.height / 2, aEvent,
                  aWindow);
}

/**
 * Synthesize a wheel event without flush layout at a particular point in
 * aWindow.
 *
 * aEvent is an object which may contain the properties:
 *   shiftKey, ctrlKey, altKey, metaKey, accessKey, deltaX, deltaY, deltaZ,
 *   deltaMode, lineOrPageDeltaX, lineOrPageDeltaY, isMomentum,
 *   isNoLineOrPageDelta, isCustomizedByPrefs, expectedOverflowDeltaX,
 *   expectedOverflowDeltaY
 *
 * deltaMode must be defined, others are ok even if undefined.
 *
 * expectedOverflowDeltaX and expectedOverflowDeltaY take integer value.  The
 * value is just checked as 0 or positive or negative.
 *
 * aWindow is optional, and defaults to the current window object.
 */
function synthesizeWheelAtPoint(aLeft, aTop, aEvent, aWindow = window)
{
  var utils = _getDOMWindowUtils(aWindow);
  if (!utils) {
    return;
  }

  var modifiers = _parseModifiers(aEvent, aWindow);
  var options = 0;
  if (aEvent.isNoLineOrPageDelta) {
    options |= utils.WHEEL_EVENT_CAUSED_BY_NO_LINE_OR_PAGE_DELTA_DEVICE;
  }
  if (aEvent.isMomentum) {
    options |= utils.WHEEL_EVENT_CAUSED_BY_MOMENTUM;
  }
  if (aEvent.isCustomizedByPrefs) {
    options |= utils.WHEEL_EVENT_CUSTOMIZED_BY_USER_PREFS;
  }
  if (typeof aEvent.expectedOverflowDeltaX !== "undefined") {
    if (aEvent.expectedOverflowDeltaX === 0) {
      options |= utils.WHEEL_EVENT_EXPECTED_OVERFLOW_DELTA_X_ZERO;
    } else if (aEvent.expectedOverflowDeltaX > 0) {
      options |= utils.WHEEL_EVENT_EXPECTED_OVERFLOW_DELTA_X_POSITIVE;
    } else {
      options |= utils.WHEEL_EVENT_EXPECTED_OVERFLOW_DELTA_X_NEGATIVE;
    }
  }
  if (typeof aEvent.expectedOverflowDeltaY !== "undefined") {
    if (aEvent.expectedOverflowDeltaY === 0) {
      options |= utils.WHEEL_EVENT_EXPECTED_OVERFLOW_DELTA_Y_ZERO;
    } else if (aEvent.expectedOverflowDeltaY > 0) {
      options |= utils.WHEEL_EVENT_EXPECTED_OVERFLOW_DELTA_Y_POSITIVE;
    } else {
      options |= utils.WHEEL_EVENT_EXPECTED_OVERFLOW_DELTA_Y_NEGATIVE;
    }
  }
  var isNoLineOrPageDelta = aEvent.isNoLineOrPageDelta;

  // Avoid the JS warnings "reference to undefined property"
  if (!aEvent.deltaX) {
    aEvent.deltaX = 0;
  }
  if (!aEvent.deltaY) {
    aEvent.deltaY = 0;
  }
  if (!aEvent.deltaZ) {
    aEvent.deltaZ = 0;
  }

  var lineOrPageDeltaX =
    aEvent.lineOrPageDeltaX != null ? aEvent.lineOrPageDeltaX :
                  aEvent.deltaX > 0 ? Math.floor(aEvent.deltaX) :
                                      Math.ceil(aEvent.deltaX);
  var lineOrPageDeltaY =
    aEvent.lineOrPageDeltaY != null ? aEvent.lineOrPageDeltaY :
                  aEvent.deltaY > 0 ? Math.floor(aEvent.deltaY) :
                                      Math.ceil(aEvent.deltaY);
  utils.sendWheelEvent(aLeft, aTop,
                       aEvent.deltaX, aEvent.deltaY, aEvent.deltaZ,
                       aEvent.deltaMode, modifiers,
                       lineOrPageDeltaX, lineOrPageDeltaY, options);
}

/**
 * Synthesize a wheel event on a target. The actual client point is determined
 * by taking the aTarget's client box and offseting it by aOffsetX and
 * aOffsetY.
 *
 * aEvent is an object which may contain the properties:
 *   shiftKey, ctrlKey, altKey, metaKey, accessKey, deltaX, deltaY, deltaZ,
 *   deltaMode, lineOrPageDeltaX, lineOrPageDeltaY, isMomentum,
 *   isNoLineOrPageDelta, isCustomizedByPrefs, expectedOverflowDeltaX,
 *   expectedOverflowDeltaY
 *
 * deltaMode must be defined, others are ok even if undefined.
 *
 * expectedOverflowDeltaX and expectedOverflowDeltaY take integer value.  The
 * value is just checked as 0 or positive or negative.
 *
 * aWindow is optional, and defaults to the current window object.
 */
function synthesizeWheel(aTarget, aOffsetX, aOffsetY, aEvent, aWindow)
{
  var rect = aTarget.getBoundingClientRect();
  synthesizeWheelAtPoint(rect.left + aOffsetX, rect.top + aOffsetY,
                         aEvent, aWindow);
}

const _FlushModes = {
  FLUSH: 0,
  NOFLUSH: 1
};

function _sendWheelAndPaint(aTarget, aOffsetX, aOffsetY,
                            aEvent, aCallback,
                            aFlushMode = _FlushModes.FLUSH,
                            aWindow = window) {
  var utils = _getDOMWindowUtils(aWindow);
  if (!utils)
    return;

  if (utils.isMozAfterPaintPending) {
    // If a paint is pending, then APZ may be waiting for a scroll acknowledgement
    // from the content thread. If we send a wheel event now, it could be ignored
    // by APZ (or its scroll offset could be overridden). To avoid problems we
    // just wait for the paint to complete.
    aWindow.waitForAllPaintsFlushed(function() {
      _sendWheelAndPaint(aTarget, aOffsetX, aOffsetY,
                         aEvent, aCallback,
                         aFlushMode,
                         aWindow);
    });
    return;
  }

  var onwheel = function() {
    SpecialPowers.removeSystemEventListener(window, "wheel", onwheel);

    // Wait one frame since the wheel event has not caused a refresh observer
    // to be added yet.
    setTimeout(function() {
      utils.advanceTimeAndRefresh(1000);

      if (!aCallback) {
        utils.advanceTimeAndRefresh(0);
        return;
      }

      var waitForPaints = function () {
        SpecialPowers.Services.obs.removeObserver(waitForPaints, "apz-repaints-flushed");
        aWindow.waitForAllPaintsFlushed(function() {
          utils.restoreNormalRefresh();
          aCallback();
        });
      }

      SpecialPowers.Services.obs.addObserver(waitForPaints, "apz-repaints-flushed");
      if (!utils.flushApzRepaints(aWindow)) {
        waitForPaints();
      }
    }, 0);
  };

  // Listen for the system wheel event, because it happens after all of
  // the other wheel events, including legacy events.
  SpecialPowers.addSystemEventListener(aWindow, "wheel", onwheel);
  if (aFlushMode === _FlushModes.FLUSH) {
    synthesizeWheel(aTarget, aOffsetX, aOffsetY, aEvent, aWindow);
  } else {
    synthesizeWheelAtPoint(aOffsetX, aOffsetY, aEvent, aWindow);
  }
}

/**
 * This is a wrapper around synthesizeWheel that waits for the wheel event
 * to be dispatched and for the subsequent layout/paints to be flushed.
 *
 * This requires including paint_listener.js. Tests must call
 * DOMWindowUtils.restoreNormalRefresh() before finishing, if they use this
 * function.
 *
 * If no callback is provided, the caller is assumed to have its own method of
 * determining scroll completion and the refresh driver is not automatically
 * restored.
 */
function sendWheelAndPaint(aTarget, aOffsetX, aOffsetY,
                           aEvent, aCallback,
                           aWindow = window) {
  _sendWheelAndPaint(aTarget, aOffsetX, aOffsetY,
                     aEvent, aCallback,
                     _FlushModes.FLUSH,
                     aWindow);
}

/**
 * Similar to sendWheelAndPaint but without flushing layout for obtaining
 * |aTarget| position in |aWindow| before sending the wheel event.
 * |aOffsetX| and |aOffsetY| should be offsets against aWindow.
 */
function sendWheelAndPaintNoFlush(aTarget, aOffsetX, aOffsetY,
                                  aEvent, aCallback,
                                  aWindow = window) {
  _sendWheelAndPaint(aTarget, aOffsetX, aOffsetY,
                     aEvent, aCallback,
                     _FlushModes.NOFLUSH,
                     aWindow);
}

function synthesizeNativeTapAtCenter(aTarget, aLongTap = false, aCallback = null, aWindow = window) {
  let rect = aTarget.getBoundingClientRect();
  return synthesizeNativeTap(aTarget, rect.width / 2, rect.height / 2, aLongTap, aCallback, aWindow);
}

function synthesizeNativeTap(aTarget, aOffsetX, aOffsetY, aLongTap = false, aCallback = null, aWindow = window) {
  let utils = _getDOMWindowUtils(aWindow);
  if (!utils)
    return;

  let scale = utils.screenPixelsPerCSSPixel;
  let rect = aTarget.getBoundingClientRect();
  let x = (aWindow.mozInnerScreenX + rect.left + aOffsetX) * scale;
  let y = (aWindow.mozInnerScreenY + rect.top + aOffsetY) * scale;

  let observer = {
    observe: (subject, topic, data) => {
      if (aCallback && topic == "mouseevent") {
        aCallback(data);
      }
    }
  };
  utils.sendNativeTouchTap(x, y, aLongTap, observer);
}

function synthesizeNativeMouseMove(aTarget, aOffsetX, aOffsetY, aCallback, aWindow = window) {
  var utils = _getDOMWindowUtils(aWindow);
  if (!utils)
    return;

  var rect = aTarget.getBoundingClientRect();
  var x = aOffsetX + window.mozInnerScreenX + rect.left;
  var y = aOffsetY + window.mozInnerScreenY + rect.top;
  var scale = utils.screenPixelsPerCSSPixel;

  var observer = {
    observe: (subject, topic, data) => {
      if (aCallback && topic == "mouseevent") {
        aCallback(data);
      }
    }
  };
  utils.sendNativeMouseMove(x * scale, y * scale, null, observer);
}

/**
 * This is a wrapper around synthesizeNativeMouseMove that waits for the mouse
 * event to be dispatched to the target content.
 *
 * This API is supposed to be used in those test cases that synthesize some
 * input events to chrome process and have some checks in content.
 */
function synthesizeAndWaitNativeMouseMove(aTarget, aOffsetX, aOffsetY,
                                          aCallback, aWindow = window) {
  let browser = gBrowser.selectedTab.linkedBrowser;
  let mm = browser.messageManager;
  let ContentTask =
    _EU_Cu.import("resource://testing-common/ContentTask.jsm", null).ContentTask;

  let eventRegisteredPromise = new Promise(resolve => {
    mm.addMessageListener("Test:MouseMoveRegistered", function processed(message) {
      mm.removeMessageListener("Test:MouseMoveRegistered", processed);
      resolve();
    });
  });
  let eventReceivedPromise = ContentTask.spawn(browser, [aOffsetX, aOffsetY],
                                               ([clientX, clientY]) => {
    return new Promise(resolve => {
      addEventListener("mousemove", function onMouseMoveEvent(e) {
        if (e.clientX == clientX && e.clientY == clientY) {
          removeEventListener("mousemove", onMouseMoveEvent);
          resolve();
        }
      });
      sendAsyncMessage("Test:MouseMoveRegistered");
    });
  });
  eventRegisteredPromise.then(() => {
    synthesizeNativeMouseMove(aTarget, aOffsetX, aOffsetY, null, aWindow);
  });
  return eventReceivedPromise;
}

/**
 * Synthesize a key event. It is targeted at whatever would be targeted by an
 * actual keypress by the user, typically the focused element.
 *
 * aKey should be:
 *  - key value (recommended).  If you specify a non-printable key name,
 *    append "KEY_" prefix.  Otherwise, specifying a printable key, the
 *    key value should be specified.
 *  - keyCode name starting with "VK_" (e.g., VK_RETURN).  This is available
 *    only for compatibility with legacy API.  Don't use this with new tests.
 *
 * aEvent is an object which may contain the properties:
 *  - code: If you don't specify this explicitly, it'll be guessed from aKey
 *          of US keyboard layout.  Note that this value may be different
 *          between browsers.  For example, "Insert" is never set only on
 *          macOS since actual key operation won't cause this code value.
 *          In such case, the value becomes empty string.
 *          If you need to emulate non-US keyboard layout or virtual keyboard
 *          which doesn't emulate hardware key input, you should set this value
 *          to empty string explicitly.
 *  - repeat: If you emulates auto-repeat, you should set the count of repeat.
 *            This method will automatically synthesize keydown (and keypress).
 *  - location: If you want to specify this, you can specify this explicitly.
 *              However, if you don't specify this value, it will be computed
 *              from code value.
 *  - type: Basically, you shouldn't specify this.  Then, this function will
 *          synthesize keydown (, keypress) and keyup.
 *          If keydown is specified, this only fires keydown (and keypress if
 *          it should be fired).
 *          If keyup is specified, this only fires keyup.
 *  - altKey, altGraphKey, ctrlKey, capsLockKey, fnKey, fnLockKey, numLockKey,
 *    metaKey, osKey, scrollLockKey, shiftKey, symbolKey, symbolLockKey:
 *        Basically, you shouldn't use these attributes.  nsITextInputProcessor
 *        manages modifier key state when you synthesize modifier key events.
 *        However, if some of these attributes are true, this function activates
 *        the modifiers only during dispatching the key events.
 *        Note that if some of these values are false, they are ignored (i.e.,
 *        not inactivated with this function).
 *  - keyCode: Must be 0 - 255 (0xFF). If this is specified explicitly,
 *             .keyCode value is initialized with this value.
 *
 * aWindow is optional, and defaults to the current window object.
 * aCallback is optional, use the callback for receiving notifications of TIP.
 */
function synthesizeKey(aKey, aEvent = undefined, aWindow = window, aCallback)
{
  var event = aEvent === undefined || aEvent === null ? {} : aEvent;

  var TIP = _getTIP(aWindow, aCallback);
  if (!TIP) {
    return;
  }
  var KeyboardEvent = _getKeyboardEvent(aWindow);
  var modifiers = _emulateToActivateModifiers(TIP, event, aWindow);
  var keyEventDict = _createKeyboardEventDictionary(aKey, event, TIP, aWindow);
  var keyEvent = new KeyboardEvent("", keyEventDict.dictionary);
  var dispatchKeydown =
    !("type" in event) || event.type === "keydown" || !event.type;
  var dispatchKeyup =
    !("type" in event) || event.type === "keyup"   || !event.type;

  try {
    if (dispatchKeydown) {
      TIP.keydown(keyEvent, keyEventDict.flags);
      if ("repeat" in event && event.repeat > 1) {
        keyEventDict.dictionary.repeat = true;
        var repeatedKeyEvent = new KeyboardEvent("", keyEventDict.dictionary);
        for (var i = 1; i < event.repeat; i++) {
          TIP.keydown(repeatedKeyEvent, keyEventDict.flags);
        }
      }
    }
    if (dispatchKeyup) {
      TIP.keyup(keyEvent, keyEventDict.flags);
    }
  } finally {
    _emulateToInactivateModifiers(TIP, modifiers, aWindow);
  }
}

/**
 * This is a wrapper around synthesizeKey that waits for the key event to be
 * dispatched to the target content. It returns a promise which is resolved
 * when the content receives the key event.
 *
 * This API is supposed to be used in those test cases that synthesize some
 * input events to chrome process and have some checks in content.
 */
function synthesizeAndWaitKey(aKey, aEvent, aWindow = window,
                              checkBeforeSynthesize, checkAfterSynthesize)
{
  let browser = gBrowser.selectedTab.linkedBrowser;
  let mm = browser.messageManager;
  let keyCode = _createKeyboardEventDictionary(aKey, aEvent, null, aWindow).dictionary.keyCode;
  let ContentTask = _EU_Cu.import("resource://testing-common/ContentTask.jsm", null).ContentTask;

  let keyRegisteredPromise = new Promise(resolve => {
    mm.addMessageListener("Test:KeyRegistered", function processed(message) {
      mm.removeMessageListener("Test:KeyRegistered", processed);
      resolve();
    });
  });
  let keyReceivedPromise = ContentTask.spawn(browser, keyCode, (keyCode) => {
    return new Promise(resolve => {
      addEventListener("keyup", function onKeyEvent(e) {
        if (e.keyCode == keyCode) {
          removeEventListener("keyup", onKeyEvent);
          resolve();
        }
      });
      sendAsyncMessage("Test:KeyRegistered");
    });
  });
  keyRegisteredPromise.then(() => {
    if (checkBeforeSynthesize) {
      checkBeforeSynthesize();
    }
    synthesizeKey(aKey, aEvent, aWindow);
    if (checkAfterSynthesize) {
      checkAfterSynthesize();
    }
  });
  return keyReceivedPromise;
}

function _parseNativeModifiers(aModifiers, aWindow = window)
{
  var navigator = _getNavigator(aWindow);
  var modifiers;
  if (aModifiers.capsLockKey) {
    modifiers |= 0x00000001;
  }
  if (aModifiers.numLockKey) {
    modifiers |= 0x00000002;
  }
  if (aModifiers.shiftKey) {
    modifiers |= 0x00000100;
  }
  if (aModifiers.shiftRightKey) {
    modifiers |= 0x00000200;
  }
  if (aModifiers.ctrlKey) {
    modifiers |= 0x00000400;
  }
  if (aModifiers.ctrlRightKey) {
    modifiers |= 0x00000800;
  }
  if (aModifiers.altKey) {
    modifiers |= 0x00001000;
  }
  if (aModifiers.altRightKey) {
    modifiers |= 0x00002000;
  }
  if (aModifiers.metaKey) {
    modifiers |= 0x00004000;
  }
  if (aModifiers.metaRightKey) {
    modifiers |= 0x00008000;
  }
  if (aModifiers.helpKey) {
    modifiers |= 0x00010000;
  }
  if (aModifiers.fnKey) {
    modifiers |= 0x00100000;
  }
  if (aModifiers.numericKeyPadKey) {
    modifiers |= 0x01000000;
  }

  if (aModifiers.accelKey) {
    modifiers |= _EU_isMac(aWindow) ? 0x00004000 : 0x00000400;
  }
  if (aModifiers.accelRightKey) {
    modifiers |= _EU_isMac(aWindow) ? 0x00008000 : 0x00000800;
  }
  if (aModifiers.altGrKey) {
    modifiers |= _EU_isWin(aWindow) ? 0x00020000 : 0x00001000;
  }
  return modifiers;
}

// Mac: Any unused number is okay for adding new keyboard layout.
//      When you add new keyboard layout here, you need to modify
//      TISInputSourceWrapper::InitByLayoutID().
// Win: These constants can be found by inspecting registry keys under
//      HKEY_LOCAL_MACHINE\System\CurrentControlSet\Control\Keyboard Layouts

const KEYBOARD_LAYOUT_ARABIC =
  { name: "Arabic",             Mac: 6,
                                Win: 0x00000401, hasAltGrOnWin: false };
const KEYBOARD_LAYOUT_ARABIC_PC =
  { name: "Arabic - PC",        Mac: 7,
                                Win: null,       hasAltGrOnWin: false };
const KEYBOARD_LAYOUT_BRAZILIAN_ABNT =
  { name: "Brazilian ABNT",     Mac: null,
                                Win: 0x00000416, hasAltGrOnWin: true  };
const KEYBOARD_LAYOUT_DVORAK_QWERTY =
  { name: "Dvorak-QWERTY",      Mac: 4,
                                Win: null,       hasAltGrOnWin: false };
const KEYBOARD_LAYOUT_EN_US =
  { name: "US",                 Mac: 0,
                                Win: 0x00000409, hasAltGrOnWin: false };
const KEYBOARD_LAYOUT_FRENCH =
  { name: "French",             Mac: 8,
                                Win: 0x0000040C, hasAltGrOnWin: true  };
const KEYBOARD_LAYOUT_GREEK =
  { name: "Greek",              Mac: 1,
                                Win: 0x00000408, hasAltGrOnWin: true  };
const KEYBOARD_LAYOUT_GERMAN =
  { name: "German",             Mac: 2,
                                Win: 0x00000407, hasAltGrOnWin: true  };
const KEYBOARD_LAYOUT_HEBREW =
  { name: "Hebrew",             Mac: 9,
                                Win: 0x0000040D, hasAltGrOnWin: true  };
const KEYBOARD_LAYOUT_JAPANESE =
  { name: "Japanese",           Mac: null,
                                Win: 0x00000411, hasAltGrOnWin: false };
const KEYBOARD_LAYOUT_KHMER =
  { name: "Khmer",              Mac: null,
                                Win: 0x00000453, hasAltGrOnWin: true  }; // available on Win7 or later.
const KEYBOARD_LAYOUT_LITHUANIAN =
  { name: "Lithuanian",         Mac: 10,
                                Win: 0x00010427, hasAltGrOnWin: true  };
const KEYBOARD_LAYOUT_NORWEGIAN =
  { name: "Norwegian",          Mac: 11,
                                Win: 0x00000414, hasAltGrOnWin: true  };
const KEYBOARD_LAYOUT_RUSSIAN_MNEMONIC =
  { name: "Russian - Mnemonic", Mac: null,
                                Win: 0x00020419, hasAltGrOnWin: true  }; // available on Win8 or later.
const KEYBOARD_LAYOUT_SPANISH =
  { name: "Spanish",            Mac: 12,
                                Win: 0x0000040A, hasAltGrOnWin: true  };
const KEYBOARD_LAYOUT_SWEDISH =
  { name: "Swedish",            Mac: 3,
                                Win: 0x0000041D, hasAltGrOnWin: true  };
const KEYBOARD_LAYOUT_THAI =
  { name: "Thai",               Mac: 5,
                                Win: 0x0002041E, hasAltGrOnWin: false };

/**
 * synthesizeNativeKey() dispatches native key event on active window.
 * This is implemented only on Windows and Mac. Note that this function
 * dispatches the key event asynchronously and returns immediately. If a
 * callback function is provided, the callback will be called upon
 * completion of the key dispatch.
 *
 * @param aKeyboardLayout       One of KEYBOARD_LAYOUT_* defined above.
 * @param aNativeKeyCode        A native keycode value defined in
 *                              NativeKeyCodes.js.
 * @param aModifiers            Modifier keys.  If no modifire key is pressed,
 *                              this must be {}.  Otherwise, one or more items
 *                              referred in _parseNativeModifiers() must be
 *                              true.
 * @param aChars                Specify characters which should be generated
 *                              by the key event.
 * @param aUnmodifiedChars      Specify characters of unmodified (except Shift)
 *                              aChar value.
 * @param aCallback             If provided, this callback will be invoked
 *                              once the native keys have been processed
 *                              by Gecko. Will never be called if this
 *                              function returns false.
 * @return                      True if this function succeed dispatching
 *                              native key event.  Otherwise, false.
 */

function synthesizeNativeKey(aKeyboardLayout, aNativeKeyCode, aModifiers,
                             aChars, aUnmodifiedChars, aCallback, aWindow = window)
{
  var utils = _getDOMWindowUtils(aWindow);
  if (!utils) {
    return false;
  }
  var navigator = _getNavigator(aWindow);
  var nativeKeyboardLayout = null;
  if (_EU_isMac(aWindow)) {
    nativeKeyboardLayout = aKeyboardLayout.Mac;
  } else if (_EU_isWin(aWindow)) {
    nativeKeyboardLayout = aKeyboardLayout.Win;
  }
  if (nativeKeyboardLayout === null) {
    return false;
  }

  var observer = {
    observe: function(aSubject, aTopic, aData) {
      if (aCallback && aTopic == "keyevent") {
        aCallback(aData);
      }
    }
  };
  utils.sendNativeKeyEvent(nativeKeyboardLayout, aNativeKeyCode,
                           _parseNativeModifiers(aModifiers, aWindow),
                           aChars, aUnmodifiedChars, observer);
  return true;
}

var _gSeenEvent = false;

/**
 * Indicate that an event with an original target of aExpectedTarget and
 * a type of aExpectedEvent is expected to be fired, or not expected to
 * be fired.
 */
function _expectEvent(aExpectedTarget, aExpectedEvent, aTestName)
{
  if (!aExpectedTarget || !aExpectedEvent)
    return null;

  _gSeenEvent = false;

  var type = (aExpectedEvent.charAt(0) == "!") ?
             aExpectedEvent.substring(1) : aExpectedEvent;
  var eventHandler = function(event) {
    var epassed = (!_gSeenEvent && event.originalTarget == aExpectedTarget &&
                   event.type == type);
    is(epassed, true, aTestName + " " + type + " event target " + (_gSeenEvent ? "twice" : ""));
    _gSeenEvent = true;
  };

  aExpectedTarget.addEventListener(type, eventHandler);
  return eventHandler;
}

/**
 * Check if the event was fired or not. The event handler aEventHandler
 * will be removed.
 */
function _checkExpectedEvent(aExpectedTarget, aExpectedEvent, aEventHandler, aTestName)
{
  if (aEventHandler) {
    var expectEvent = (aExpectedEvent.charAt(0) != "!");
    var type = expectEvent ? aExpectedEvent : aExpectedEvent.substring(1);
    aExpectedTarget.removeEventListener(type, aEventHandler);
    var desc = type + " event";
    if (!expectEvent)
      desc += " not";
    is(_gSeenEvent, expectEvent, aTestName + " " + desc + " fired");
  }

  _gSeenEvent = false;
}

/**
 * Similar to synthesizeMouse except that a test is performed to see if an
 * event is fired at the right target as a result.
 *
 * aExpectedTarget - the expected originalTarget of the event.
 * aExpectedEvent - the expected type of the event, such as 'select'.
 * aTestName - the test name when outputing results
 *
 * To test that an event is not fired, use an expected type preceded by an
 * exclamation mark, such as '!select'. This might be used to test that a
 * click on a disabled element doesn't fire certain events for instance.
 *
 * aWindow is optional, and defaults to the current window object.
 */
function synthesizeMouseExpectEvent(aTarget, aOffsetX, aOffsetY, aEvent,
                                    aExpectedTarget, aExpectedEvent, aTestName,
                                    aWindow)
{
  var eventHandler = _expectEvent(aExpectedTarget, aExpectedEvent, aTestName);
  synthesizeMouse(aTarget, aOffsetX, aOffsetY, aEvent, aWindow);
  _checkExpectedEvent(aExpectedTarget, aExpectedEvent, eventHandler, aTestName);
}

/**
 * Similar to synthesizeKey except that a test is performed to see if an
 * event is fired at the right target as a result.
 *
 * aExpectedTarget - the expected originalTarget of the event.
 * aExpectedEvent - the expected type of the event, such as 'select'.
 * aTestName - the test name when outputing results
 *
 * To test that an event is not fired, use an expected type preceded by an
 * exclamation mark, such as '!select'.
 *
 * aWindow is optional, and defaults to the current window object.
 */
function synthesizeKeyExpectEvent(key, aEvent, aExpectedTarget, aExpectedEvent,
                                  aTestName, aWindow)
{
  var eventHandler = _expectEvent(aExpectedTarget, aExpectedEvent, aTestName);
  synthesizeKey(key, aEvent, aWindow);
  _checkExpectedEvent(aExpectedTarget, aExpectedEvent, eventHandler, aTestName);
}

function disableNonTestMouseEvents(aDisable)
{
  var domutils = _getDOMWindowUtils();
  domutils.disableNonTestMouseEvents(aDisable);
}

function _getDOMWindowUtils(aWindow = window)
{
  // Leave this here as something, somewhere, passes a falsy argument
  // to this, causing the |window| default argument not to get picked up.
  if (!aWindow) {
    aWindow = window;
  }

  // If documentURIObject exists or `window` is a stub object, we're in
  // a chrome scope, so don't bother trying to go through SpecialPowers.
  if (!window.document || window.document.documentURIObject) {
    return aWindow.windowUtils;
  }

  // we need parent.SpecialPowers for:
  //  layout/base/tests/test_reftests_with_caret.html
  //  chrome: toolkit/content/tests/chrome/test_findbar.xul
  //  chrome: toolkit/content/tests/chrome/test_popup_anchor.xul
  if ("SpecialPowers" in window && window.SpecialPowers != undefined) {
    return SpecialPowers.getDOMWindowUtils(aWindow);
  }
  if ("SpecialPowers" in parent && parent.SpecialPowers != undefined) {
    return parent.SpecialPowers.getDOMWindowUtils(aWindow);
  }

  // TODO: this is assuming we are in chrome space
  return aWindow.windowUtils;
}

function _defineConstant(name, value) {
  Object.defineProperty(this, name, {
    value: value,
    enumerable: true,
    writable: false
  });
}

const COMPOSITION_ATTR_RAW_CLAUSE =
  _EU_Ci.nsITextInputProcessor.ATTR_RAW_CLAUSE;
_defineConstant("COMPOSITION_ATTR_RAW_CLAUSE", COMPOSITION_ATTR_RAW_CLAUSE);
const COMPOSITION_ATTR_SELECTED_RAW_CLAUSE =
  _EU_Ci.nsITextInputProcessor.ATTR_SELECTED_RAW_CLAUSE;
_defineConstant("COMPOSITION_ATTR_SELECTED_RAW_CLAUSE", COMPOSITION_ATTR_SELECTED_RAW_CLAUSE);
const COMPOSITION_ATTR_CONVERTED_CLAUSE =
  _EU_Ci.nsITextInputProcessor.ATTR_CONVERTED_CLAUSE;
_defineConstant("COMPOSITION_ATTR_CONVERTED_CLAUSE", COMPOSITION_ATTR_CONVERTED_CLAUSE);
const COMPOSITION_ATTR_SELECTED_CLAUSE =
  _EU_Ci.nsITextInputProcessor.ATTR_SELECTED_CLAUSE;
_defineConstant("COMPOSITION_ATTR_SELECTED_CLAUSE", COMPOSITION_ATTR_SELECTED_CLAUSE);

var TIPMap = new WeakMap();

function _getTIP(aWindow, aCallback)
{
  if (!aWindow) {
    aWindow = window;
  }
  var tip;
  if (TIPMap.has(aWindow)) {
    tip = TIPMap.get(aWindow);
  } else {
    tip =
      _EU_Cc["@mozilla.org/text-input-processor;1"].
        createInstance(_EU_Ci.nsITextInputProcessor);
    TIPMap.set(aWindow, tip);
  }
  if (!tip.beginInputTransactionForTests(aWindow, aCallback)) {
    tip = null;
    TIPMap.delete(aWindow);
  }
  return tip;
}

function _getKeyboardEvent(aWindow = window)
{
  if (typeof KeyboardEvent != "undefined") {
    try {
      // See if the object can be instantiated; sometimes this yields
      // 'TypeError: can't access dead object' or 'KeyboardEvent is not a constructor'.
      new KeyboardEvent("", {});
      return KeyboardEvent;
    } catch (ex) {}
  }
  if (typeof content != "undefined" && ("KeyboardEvent" in content)) {
    return content.KeyboardEvent;
  }
  return aWindow.KeyboardEvent;
}

function _getNavigator(aWindow = window)
{
  if (typeof navigator != "undefined") {
    return navigator;
  }
  return aWindow.navigator;
}

function _guessKeyNameFromKeyCode(aKeyCode, aWindow = window)
{
  var KeyboardEvent = _getKeyboardEvent(aWindow);
  switch (aKeyCode) {
    case KeyboardEvent.DOM_VK_CANCEL:
      return "Cancel";
    case KeyboardEvent.DOM_VK_HELP:
      return "Help";
    case KeyboardEvent.DOM_VK_BACK_SPACE:
      return "Backspace";
    case KeyboardEvent.DOM_VK_TAB:
      return "Tab";
    case KeyboardEvent.DOM_VK_CLEAR:
      return "Clear";
    case KeyboardEvent.DOM_VK_RETURN:
      return "Enter";
    case KeyboardEvent.DOM_VK_SHIFT:
      return "Shift";
    case KeyboardEvent.DOM_VK_CONTROL:
      return "Control";
    case KeyboardEvent.DOM_VK_ALT:
      return "Alt";
    case KeyboardEvent.DOM_VK_PAUSE:
      return "Pause";
    case KeyboardEvent.DOM_VK_EISU:
      return "Eisu";
    case KeyboardEvent.DOM_VK_ESCAPE:
      return "Escape";
    case KeyboardEvent.DOM_VK_CONVERT:
      return "Convert";
    case KeyboardEvent.DOM_VK_NONCONVERT:
      return "NonConvert";
    case KeyboardEvent.DOM_VK_ACCEPT:
      return "Accept";
    case KeyboardEvent.DOM_VK_MODECHANGE:
      return "ModeChange";
    case KeyboardEvent.DOM_VK_PAGE_UP:
      return "PageUp";
    case KeyboardEvent.DOM_VK_PAGE_DOWN:
      return "PageDown";
    case KeyboardEvent.DOM_VK_END:
      return "End";
    case KeyboardEvent.DOM_VK_HOME:
      return "Home";
    case KeyboardEvent.DOM_VK_LEFT:
      return "ArrowLeft";
    case KeyboardEvent.DOM_VK_UP:
      return "ArrowUp";
    case KeyboardEvent.DOM_VK_RIGHT:
      return "ArrowRight";
    case KeyboardEvent.DOM_VK_DOWN:
      return "ArrowDown";
    case KeyboardEvent.DOM_VK_SELECT:
      return "Select";
    case KeyboardEvent.DOM_VK_PRINT:
      return "Print";
    case KeyboardEvent.DOM_VK_EXECUTE:
      return "Execute";
    case KeyboardEvent.DOM_VK_PRINTSCREEN:
      return "PrintScreen";
    case KeyboardEvent.DOM_VK_INSERT:
      return "Insert";
    case KeyboardEvent.DOM_VK_DELETE:
      return "Delete";
    case KeyboardEvent.DOM_VK_WIN:
      return "OS";
    case KeyboardEvent.DOM_VK_CONTEXT_MENU:
      return "ContextMenu";
    case KeyboardEvent.DOM_VK_SLEEP:
      return "Standby";
    case KeyboardEvent.DOM_VK_F1:
      return "F1";
    case KeyboardEvent.DOM_VK_F2:
      return "F2";
    case KeyboardEvent.DOM_VK_F3:
      return "F3";
    case KeyboardEvent.DOM_VK_F4:
      return "F4";
    case KeyboardEvent.DOM_VK_F5:
      return "F5";
    case KeyboardEvent.DOM_VK_F6:
      return "F6";
    case KeyboardEvent.DOM_VK_F7:
      return "F7";
    case KeyboardEvent.DOM_VK_F8:
      return "F8";
    case KeyboardEvent.DOM_VK_F9:
      return "F9";
    case KeyboardEvent.DOM_VK_F10:
      return "F10";
    case KeyboardEvent.DOM_VK_F11:
      return "F11";
    case KeyboardEvent.DOM_VK_F12:
      return "F12";
    case KeyboardEvent.DOM_VK_F13:
      return "F13";
    case KeyboardEvent.DOM_VK_F14:
      return "F14";
    case KeyboardEvent.DOM_VK_F15:
      return "F15";
    case KeyboardEvent.DOM_VK_F16:
      return "F16";
    case KeyboardEvent.DOM_VK_F17:
      return "F17";
    case KeyboardEvent.DOM_VK_F18:
      return "F18";
    case KeyboardEvent.DOM_VK_F19:
      return "F19";
    case KeyboardEvent.DOM_VK_F20:
      return "F20";
    case KeyboardEvent.DOM_VK_F21:
      return "F21";
    case KeyboardEvent.DOM_VK_F22:
      return "F22";
    case KeyboardEvent.DOM_VK_F23:
      return "F23";
    case KeyboardEvent.DOM_VK_F24:
      return "F24";
    case KeyboardEvent.DOM_VK_NUM_LOCK:
      return "NumLock";
    case KeyboardEvent.DOM_VK_SCROLL_LOCK:
      return "ScrollLock";
    case KeyboardEvent.DOM_VK_VOLUME_MUTE:
      return "AudioVolumeMute";
    case KeyboardEvent.DOM_VK_VOLUME_DOWN:
      return "AudioVolumeDown";
    case KeyboardEvent.DOM_VK_VOLUME_UP:
      return "AudioVolumeUp";
    case KeyboardEvent.DOM_VK_META:
      return "Meta";
    case KeyboardEvent.DOM_VK_ALTGR:
      return "AltGraph";
    case KeyboardEvent.DOM_VK_PROCESSKEY:
      return "Process";
    case KeyboardEvent.DOM_VK_ATTN:
      return "Attn";
    case KeyboardEvent.DOM_VK_CRSEL:
      return "CrSel";
    case KeyboardEvent.DOM_VK_EXSEL:
      return "ExSel";
    case KeyboardEvent.DOM_VK_EREOF:
      return "EraseEof";
    case KeyboardEvent.DOM_VK_PLAY:
      return "Play";
    default:
      return "Unidentified";
  }
}

function _createKeyboardEventDictionary(aKey, aKeyEvent,
                                        aTIP = null,
                                        aWindow = window) {
  var result = { dictionary: null, flags: 0 };
  var keyCodeIsDefined = "keyCode" in aKeyEvent;
  var keyCode =
    (keyCodeIsDefined && aKeyEvent.keyCode >= 0 && aKeyEvent.keyCode <= 255) ?
      aKeyEvent.keyCode : 0;
  var keyName = "Unidentified";
  var code = aKeyEvent.code;
  if (!aTIP) {
    aTIP = _getTIP(aWindow);
  }
  if (aKey.indexOf("KEY_") == 0) {
    keyName = aKey.substr("KEY_".length);
    result.flags |= _EU_Ci.nsITextInputProcessor.KEY_NON_PRINTABLE_KEY;
    if (code === undefined) {
      code =
        aTIP.computeCodeValueOfNonPrintableKey(keyName, aKeyEvent.location);
    }
  } else if (aKey.indexOf("VK_") == 0) {
    keyCode = _getKeyboardEvent(aWindow)["DOM_" + aKey];
    if (!keyCode) {
      throw "Unknown key: " + aKey;
    }
    keyName = _guessKeyNameFromKeyCode(keyCode, aWindow);
    result.flags |= _EU_Ci.nsITextInputProcessor.KEY_NON_PRINTABLE_KEY;
    if (code === undefined) {
      code =
        aTIP.computeCodeValueOfNonPrintableKey(keyName, aKeyEvent.location);
    }
  } else if (aKey != "") {
    keyName = aKey;
    if (!keyCodeIsDefined) {
      keyCode =
        aTIP.guessKeyCodeValueOfPrintableKeyInUSEnglishKeyboardLayout(
               aKey, aKeyEvent.location);
    }
    if (!keyCode) {
      result.flags |= _EU_Ci.nsITextInputProcessor.KEY_KEEP_KEYCODE_ZERO;
    }
    result.flags |= _EU_Ci.nsITextInputProcessor.KEY_FORCE_PRINTABLE_KEY;
    if (code === undefined) {
      code = aTIP.guessCodeValueOfPrintableKeyInUSEnglishKeyboardLayout(
               keyName, aKeyEvent.location);
    }
  }
  var locationIsDefined = "location" in aKeyEvent;
  if (locationIsDefined && aKeyEvent.location === 0) {
    result.flags |= _EU_Ci.nsITextInputProcessor.KEY_KEEP_KEY_LOCATION_STANDARD;
  }
  if (aKeyEvent.doNotMarkKeydownAsProcessed) {
    result.flags |=
      _EU_Ci.nsITextInputProcessor.KEY_DONT_MARK_KEYDOWN_AS_PROCESSED;
  }
  if (aKeyEvent.markKeyupAsProcessed) {
    result.flags |= _EU_Ci.nsITextInputProcessor.KEY_MARK_KEYUP_AS_PROCESSED;
  }
  result.dictionary = {
    key: keyName,
    code: code,
    location: locationIsDefined ? aKeyEvent.location : 0,
    repeat: "repeat" in aKeyEvent ? aKeyEvent.repeat === true : false,
    keyCode: keyCode,
  };
  return result;
}

function _emulateToActivateModifiers(aTIP, aKeyEvent, aWindow = window)
{
  if (!aKeyEvent) {
    return null;
  }
  var KeyboardEvent = _getKeyboardEvent(aWindow);
  var navigator = _getNavigator(aWindow);

  var modifiers = {
    normal: [
      { key: "Alt",        attr: "altKey" },
      { key: "AltGraph",   attr: "altGraphKey" },
      { key: "Control",    attr: "ctrlKey" },
      { key: "Fn",         attr: "fnKey" },
      { key: "Meta",       attr: "metaKey" },
      { key: "OS",         attr: "osKey" },
      { key: "Shift",      attr: "shiftKey" },
      { key: "Symbol",     attr: "symbolKey" },
      { key: _EU_isMac(aWindow) ? "Meta" : "Control",
                           attr: "accelKey" },
    ],
    lockable: [
      { key: "CapsLock",   attr: "capsLockKey" },
      { key: "FnLock",     attr: "fnLockKey" },
      { key: "NumLock",    attr: "numLockKey" },
      { key: "ScrollLock", attr: "scrollLockKey" },
      { key: "SymbolLock", attr: "symbolLockKey" },
    ]
  }

  for (var i = 0; i < modifiers.normal.length; i++) {
    if (!aKeyEvent[modifiers.normal[i].attr]) {
      continue;
    }
    if (aTIP.getModifierState(modifiers.normal[i].key)) {
      continue; // already activated.
    }
    var event = new KeyboardEvent("", { key: modifiers.normal[i].key });
    aTIP.keydown(event,
      aTIP.KEY_NON_PRINTABLE_KEY | aTIP.KEY_DONT_DISPATCH_MODIFIER_KEY_EVENT);
    modifiers.normal[i].activated = true;
  }
  for (var i = 0; i < modifiers.lockable.length; i++) {
    if (!aKeyEvent[modifiers.lockable[i].attr]) {
      continue;
    }
    if (aTIP.getModifierState(modifiers.lockable[i].key)) {
      continue; // already activated.
    }
    var event = new KeyboardEvent("", { key: modifiers.lockable[i].key });
    aTIP.keydown(event,
      aTIP.KEY_NON_PRINTABLE_KEY | aTIP.KEY_DONT_DISPATCH_MODIFIER_KEY_EVENT);
    aTIP.keyup(event,
      aTIP.KEY_NON_PRINTABLE_KEY | aTIP.KEY_DONT_DISPATCH_MODIFIER_KEY_EVENT);
    modifiers.lockable[i].activated = true;
  }
  return modifiers;
}

function _emulateToInactivateModifiers(aTIP, aModifiers, aWindow = window)
{
  if (!aModifiers) {
    return;
  }
  var KeyboardEvent = _getKeyboardEvent(aWindow);
  for (var i = 0; i < aModifiers.normal.length; i++) {
    if (!aModifiers.normal[i].activated) {
      continue;
    }
    var event = new KeyboardEvent("", { key: aModifiers.normal[i].key });
    aTIP.keyup(event,
      aTIP.KEY_NON_PRINTABLE_KEY | aTIP.KEY_DONT_DISPATCH_MODIFIER_KEY_EVENT);
  }
  for (var i = 0; i < aModifiers.lockable.length; i++) {
    if (!aModifiers.lockable[i].activated) {
      continue;
    }
    if (!aTIP.getModifierState(aModifiers.lockable[i].key)) {
      continue; // who already inactivated this?
    }
    var event = new KeyboardEvent("", { key: aModifiers.lockable[i].key });
    aTIP.keydown(event,
      aTIP.KEY_NON_PRINTABLE_KEY | aTIP.KEY_DONT_DISPATCH_MODIFIER_KEY_EVENT);
    aTIP.keyup(event,
      aTIP.KEY_NON_PRINTABLE_KEY | aTIP.KEY_DONT_DISPATCH_MODIFIER_KEY_EVENT);
  }
}

/**
 * Synthesize a composition event and keydown event and keyup events unless
 * you prevent to dispatch them explicitly (see aEvent.key's explanation).
 *
 * Note that you shouldn't call this with "compositionstart" unless you need to
 * test compositionstart event which is NOT followed by compositionupdate
 * event immediately.  Typically, native IME starts composition with
 * a pair of keydown and keyup event and dispatch compositionstart and
 * compositionupdate (and non-standard text event) between them.  So, in most
 * cases, you should call synthesizeCompositionChange() directly.
 * If you call this with compositionstart, keyup event will be fired
 * immediately after compositionstart.  In other words, you should use
 * "compositionstart" only when you need to emulate IME which just starts
 * composition with compositionstart event but does not send composing text to
 * us until committing the composition.  This is behavior of some Chinese IMEs.
 *
 * @param aEvent               The composition event information.  This must
 *                             have |type| member.  The value must be
 *                             "compositionstart", "compositionend",
 *                             "compositioncommitasis" or "compositioncommit".
 *
 *                             And also this may have |data| and |locale| which
 *                             would be used for the value of each property of
 *                             the composition event.  Note that the |data| is
 *                             ignored if the event type is "compositionstart"
 *                             or "compositioncommitasis".
 *
 *                             If |key| is undefined, "keydown" and "keyup"
 *                             events which are marked as "processed by IME"
 *                             are dispatched.  If |key| is not null, "keydown"
 *                             and/or "keyup" events are dispatched (if the
 *                             |key.type| is specified as "keydown", only
 *                             "keydown" event is dispatched).  Otherwise,
 *                             i.e., if |key| is null, neither "keydown" nor
 *                             "keyup" event is dispatched.
 *
 *                             If |key.doNotMarkKeydownAsProcessed| is not true,
 *                             key value and keyCode value of "keydown" event
 *                             will be set to "Process" and DOM_VK_PROCESSKEY.
 *                             If |key.markKeyupAsProcessed| is true,
 *                             key value and keyCode value of "keyup" event
 *                             will be set to "Process" and DOM_VK_PROCESSKEY.
 * @param aWindow              Optional (If null, current |window| will be used)
 * @param aCallback            Optional (If non-null, use the callback for
 *                             receiving notifications to IME)
 */
function synthesizeComposition(aEvent, aWindow = window, aCallback)
{
  var TIP = _getTIP(aWindow, aCallback);
  if (!TIP) {
    return false;
  }
  var KeyboardEvent = _getKeyboardEvent(aWindow);
  var modifiers = _emulateToActivateModifiers(TIP, aEvent.key, aWindow);
  var ret = false;
  var keyEventDict = {dictionary: null, flags: 0};
  var keyEvent = null;
  if (aEvent.key && typeof aEvent.key.key === "string") {
    keyEventDict =
      _createKeyboardEventDictionary(aEvent.key.key, aEvent.key, TIP, aWindow);
    keyEvent = new KeyboardEvent(aEvent.key.type === "keydown" ?
                                   "keydown" :
                                   aEvent.key.type === "keyup" ?
                                     "keyup" : "",
                                 keyEventDict.dictionary)
  } else if (aEvent.key === undefined) {
    keyEventDict =
      _createKeyboardEventDictionary("KEY_Process", {}, TIP, aWindow);
    keyEvent = new KeyboardEvent("", keyEventDict.dictionary)
  }
  try {
    switch (aEvent.type) {
      case "compositionstart":
        ret = TIP.startComposition(keyEvent, keyEventDict.flags);
        break;
      case "compositioncommitasis":
        ret = TIP.commitComposition(keyEvent, keyEventDict.flags);
        break;
      case "compositioncommit":
        ret = TIP.commitCompositionWith(aEvent.data, keyEvent,
                                        keyEventDict.flags);
        break;
    }
  } finally {
    _emulateToInactivateModifiers(TIP, modifiers, aWindow);
  }
}
/**
 * Synthesize eCompositionChange event which causes a DOM text event, may
 * cause compositionupdate event, and causes keydown event and keyup event
 * unless you prevent to dispatch them explicitly (see aEvent.key's
 * explanation).
 *
 * Note that if you call this when there is no composition, compositionstart
 * event will be fired automatically.  This is better than you use
 * synthesizeComposition("compositionstart") in most cases.  See the
 * explanation of synthesizeComposition().
 *
 * @param aEvent   The compositionchange event's information, this has
 *                 |composition| and |caret| members.  |composition| has
 *                 |string| and |clauses| members.  |clauses| must be array
 *                 object.  Each object has |length| and |attr|.  And |caret|
 *                 has |start| and |length|.  See the following tree image.
 *
 *                 aEvent
 *                   +-- composition
 *                   |     +-- string
 *                   |     +-- clauses[]
 *                   |           +-- length
 *                   |           +-- attr
 *                   +-- caret
 *                   |     +-- start
 *                   |     +-- length
 *                   +-- key
 *
 *                 Set the composition string to |composition.string|.  Set its
 *                 clauses information to the |clauses| array.
 *
 *                 When it's composing, set the each clauses' length to the
 *                 |composition.clauses[n].length|.  The sum of the all length
 *                 values must be same as the length of |composition.string|.
 *                 Set nsICompositionStringSynthesizer.ATTR_* to the
 *                 |composition.clauses[n].attr|.
 *
 *                 When it's not composing, set 0 to the
 *                 |composition.clauses[0].length| and
 *                 |composition.clauses[0].attr|.
 *
 *                 Set caret position to the |caret.start|. It's offset from
 *                 the start of the composition string.  Set caret length to
 *                 |caret.length|.  If it's larger than 0, it should be wide
 *                 caret.  However, current nsEditor doesn't support wide
 *                 caret, therefore, you should always set 0 now.
 *
 *                 If |key| is undefined, "keydown" and "keyup" events which
 *                 are marked as "processed by IME" are dispatched.  If |key|
 *                 is not null, "keydown" and/or "keyup" events are dispatched
 *                 (if the |key.type| is specified as "keydown", only "keydown"
 *                 event is dispatched).  Otherwise, i.e., if |key| is null,
 *                 neither "keydown" nor "keyup" event is dispatched.
 *                 If |key.doNotMarkKeydownAsProcessed| is not true, key value
 *                 and keyCode value of "keydown" event will be set to
 *                 "Process" and DOM_VK_PROCESSKEY.
 *                 If |key.markKeyupAsProcessed| is true key value and keyCode
 *                 value of "keyup" event will be set to "Process" and
 *                 DOM_VK_PROCESSKEY.
 *
 * @param aWindow  Optional (If null, current |window| will be used)
 * @param aCallback     Optional (If non-null, use the callback for receiving
 *                      notifications to IME)
 */
function synthesizeCompositionChange(aEvent, aWindow = window, aCallback)
{
  var TIP = _getTIP(aWindow, aCallback);
  if (!TIP) {
    return;
  }
  var KeyboardEvent = _getKeyboardEvent(aWindow);

  if (!aEvent.composition || !aEvent.composition.clauses ||
      !aEvent.composition.clauses[0]) {
    return;
  }

  TIP.setPendingCompositionString(aEvent.composition.string);
  if (aEvent.composition.clauses[0].length) {
    for (var i = 0; i < aEvent.composition.clauses.length; i++) {
      switch (aEvent.composition.clauses[i].attr) {
        case TIP.ATTR_RAW_CLAUSE:
        case TIP.ATTR_SELECTED_RAW_CLAUSE:
        case TIP.ATTR_CONVERTED_CLAUSE:
        case TIP.ATTR_SELECTED_CLAUSE:
          TIP.appendClauseToPendingComposition(
                aEvent.composition.clauses[i].length,
                aEvent.composition.clauses[i].attr);
          break;
        case 0:
          // Ignore dummy clause for the argument.
          break;
        default:
          throw new Error("invalid clause attribute specified");
          break;
      }
    }
  }

  if (aEvent.caret) {
    TIP.setCaretInPendingComposition(aEvent.caret.start);
  }

  var modifiers = _emulateToActivateModifiers(TIP, aEvent.key, aWindow);
  try {
    var keyEventDict = {dictionary: null, flags: 0};
    var keyEvent = null;
    if (aEvent.key && typeof aEvent.key.key === "string") {
      keyEventDict =
        _createKeyboardEventDictionary(aEvent.key.key, aEvent.key,
                                       TIP, aWindow);
      keyEvent = new KeyboardEvent(aEvent.key.type === "keydown" ?
                                     "keydown" :
                                     aEvent.key.type === "keyup" ?
                                       "keyup" : "",
                                   keyEventDict.dictionary)
    } else if (aEvent.key === undefined) {
      keyEventDict =
        _createKeyboardEventDictionary("KEY_Process", {}, TIP, aWindow);
      keyEvent = new KeyboardEvent("", keyEventDict.dictionary)
    }
    TIP.flushPendingComposition(keyEvent, keyEventDict.flags);
  } finally {
    _emulateToInactivateModifiers(TIP, modifiers, aWindow);
  }
}

// Must be synchronized with nsIDOMWindowUtils.
const QUERY_CONTENT_FLAG_USE_NATIVE_LINE_BREAK          = 0x0000;
const QUERY_CONTENT_FLAG_USE_XP_LINE_BREAK              = 0x0001;

const QUERY_CONTENT_FLAG_SELECTION_NORMAL                    = 0x0000;
const QUERY_CONTENT_FLAG_SELECTION_SPELLCHECK                = 0x0002;
const QUERY_CONTENT_FLAG_SELECTION_IME_RAWINPUT              = 0x0004;
const QUERY_CONTENT_FLAG_SELECTION_IME_SELECTEDRAWTEXT       = 0x0008;
const QUERY_CONTENT_FLAG_SELECTION_IME_CONVERTEDTEXT         = 0x0010;
const QUERY_CONTENT_FLAG_SELECTION_IME_SELECTEDCONVERTEDTEXT = 0x0020;
const QUERY_CONTENT_FLAG_SELECTION_ACCESSIBILITY             = 0x0040;
const QUERY_CONTENT_FLAG_SELECTION_FIND                      = 0x0080;
const QUERY_CONTENT_FLAG_SELECTION_URLSECONDARY              = 0x0100;
const QUERY_CONTENT_FLAG_SELECTION_URLSTRIKEOUT              = 0x0200;

const QUERY_CONTENT_FLAG_OFFSET_RELATIVE_TO_INSERTION_POINT  = 0x0400;

const SELECTION_SET_FLAG_USE_NATIVE_LINE_BREAK          = 0x0000;
const SELECTION_SET_FLAG_USE_XP_LINE_BREAK              = 0x0001;
const SELECTION_SET_FLAG_REVERSE                        = 0x0002;

/**
 * Synthesize a query text content event.
 *
 * @param aOffset  The character offset.  0 means the first character in the
 *                 selection root.
 * @param aLength  The length of getting text.  If the length is too long,
 *                 the extra length is ignored.
 * @param aIsRelative   Optional (If true, aOffset is relative to start of
 *                      composition if there is, or start of selection.)
 * @param aWindow  Optional (If null, current |window| will be used)
 * @return         An nsIQueryContentEventResult object.  If this failed,
 *                 the result might be null.
 */
function synthesizeQueryTextContent(aOffset, aLength, aIsRelative, aWindow)
{
  var utils = _getDOMWindowUtils(aWindow);
  if (!utils) {
    return nullptr;
  }
  var flags = QUERY_CONTENT_FLAG_USE_NATIVE_LINE_BREAK;
  if (aIsRelative === true) {
    flags |= QUERY_CONTENT_FLAG_OFFSET_RELATIVE_TO_INSERTION_POINT;
  }
  return utils.sendQueryContentEvent(utils.QUERY_TEXT_CONTENT,
                                     aOffset, aLength, 0, 0, flags);
}

/**
 * Synthesize a query selected text event.
 *
 * @param aSelectionType    Optional, one of QUERY_CONTENT_FLAG_SELECTION_*.
 *                          If null, QUERY_CONTENT_FLAG_SELECTION_NORMAL will
 *                          be used.
 * @param aWindow  Optional (If null, current |window| will be used)
 * @return         An nsIQueryContentEventResult object.  If this failed,
 *                 the result might be null.
 */
function synthesizeQuerySelectedText(aSelectionType, aWindow)
{
  var utils = _getDOMWindowUtils(aWindow);
  if (!utils) {
    return null;
  }

  var flags = QUERY_CONTENT_FLAG_USE_NATIVE_LINE_BREAK;
  if (aSelectionType) {
    flags |= aSelectionType;
  }

  return utils.sendQueryContentEvent(utils.QUERY_SELECTED_TEXT, 0, 0, 0, 0,
                                     flags);
}

/**
 * Synthesize a query caret rect event.
 *
 * @param aOffset  The caret offset.  0 means left side of the first character
 *                 in the selection root.
 * @param aWindow  Optional (If null, current |window| will be used)
 * @return         An nsIQueryContentEventResult object.  If this failed,
 *                 the result might be null.
 */
function synthesizeQueryCaretRect(aOffset, aWindow)
{
  var utils = _getDOMWindowUtils(aWindow);
  if (!utils) {
    return null;
  }
  return utils.sendQueryContentEvent(utils.QUERY_CARET_RECT,
                                     aOffset, 0, 0, 0,
                                     QUERY_CONTENT_FLAG_USE_NATIVE_LINE_BREAK);
}

/**
 * Synthesize a selection set event.
 *
 * @param aOffset  The character offset.  0 means the first character in the
 *                 selection root.
 * @param aLength  The length of the text.  If the length is too long,
 *                 the extra length is ignored.
 * @param aReverse If true, the selection is from |aOffset + aLength| to
 *                 |aOffset|.  Otherwise, from |aOffset| to |aOffset + aLength|.
 * @param aWindow  Optional (If null, current |window| will be used)
 * @return         True, if succeeded.  Otherwise false.
 */
function synthesizeSelectionSet(aOffset, aLength, aReverse, aWindow)
{
  var utils = _getDOMWindowUtils(aWindow);
  if (!utils) {
    return false;
  }
  var flags = aReverse ? SELECTION_SET_FLAG_REVERSE : 0;
  return utils.sendSelectionSetEvent(aOffset, aLength, flags);
}

/*
 * Synthesize a native mouse click event at a particular point in screen.
 * This function should be used only for testing native event loop.
 * Use synthesizeMouse instead for most case.
 *
 * This works only on OS X.  Throws an error on other OS.  Also throws an error
 * when the library or any of function are not found, or something goes wrong
 * in native functions.
 */
function synthesizeNativeOSXClick(x, y)
{
  var { ctypes } = _EU_Cu.import("resource://gre/modules/ctypes.jsm", {});

  // Library
  var CoreFoundation = ctypes.open("/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation");
  var CoreGraphics = ctypes.open("/System/Library/Frameworks/CoreGraphics.framework/CoreGraphics");

  // Contants
  var kCGEventLeftMouseDown = 1;
  var kCGEventLeftMouseUp = 2;
  var kCGEventSourceStateHIDSystemState = 1;
  var kCGHIDEventTap = 0;
  var kCGMouseButtonLeft = 0;
  var kCGMouseEventClickState = 1;

  // Types
  var CGEventField = ctypes.uint32_t;
  var CGEventRef = ctypes.voidptr_t;
  var CGEventSourceRef = ctypes.voidptr_t;
  var CGEventSourceStateID = ctypes.uint32_t;
  var CGEventTapLocation = ctypes.uint32_t;
  var CGEventType = ctypes.uint32_t;
  var CGFloat = ctypes.voidptr_t.size == 4 ? ctypes.float : ctypes.double;
  var CGMouseButton = ctypes.uint32_t;

  var CGPoint = new ctypes.StructType(
    "CGPoint",
    [ { "x" : CGFloat },
      { "y" : CGFloat } ]);

  // Functions
  var CGEventSourceCreate = CoreGraphics.declare(
    "CGEventSourceCreate",
    ctypes.default_abi,
    CGEventSourceRef, CGEventSourceStateID);
  var CGEventCreateMouseEvent = CoreGraphics.declare(
    "CGEventCreateMouseEvent",
    ctypes.default_abi,
    CGEventRef,
    CGEventSourceRef, CGEventType, CGPoint, CGMouseButton);
  var CGEventSetIntegerValueField = CoreGraphics.declare(
    "CGEventSetIntegerValueField",
    ctypes.default_abi,
    ctypes.void_t,
    CGEventRef, CGEventField, ctypes.int64_t);
  var CGEventPost = CoreGraphics.declare(
    "CGEventPost",
    ctypes.default_abi,
    ctypes.void_t,
    CGEventTapLocation, CGEventRef);
  var CFRelease = CoreFoundation.declare(
    "CFRelease",
    ctypes.default_abi,
    ctypes.void_t,
    CGEventRef);

  var source = CGEventSourceCreate(kCGEventSourceStateHIDSystemState);
  if (!source) {
    throw new Error("CGEventSourceCreate returns null");
  }

  var loc = new CGPoint({ x: x, y: y });
  var event = CGEventCreateMouseEvent(source, kCGEventLeftMouseDown, loc,
                                      kCGMouseButtonLeft);
  if (!event) {
    throw new Error("CGEventCreateMouseEvent returns null");
  }
  CGEventSetIntegerValueField(event, kCGMouseEventClickState,
                              new ctypes.Int64(1));
  CGEventPost(kCGHIDEventTap, event);
  CFRelease(event);

  event = CGEventCreateMouseEvent(source, kCGEventLeftMouseUp, loc,
                                      kCGMouseButtonLeft);
  if (!event) {
    throw new Error("CGEventCreateMouseEvent returns null");
  }
  CGEventSetIntegerValueField(event, kCGMouseEventClickState,
                              new ctypes.Int64(1));
  CGEventPost(kCGHIDEventTap, event);
  CFRelease(event);

  CFRelease(source);

  CoreFoundation.close();
  CoreGraphics.close();
}

/**
 * Emulate a dragstart event.
 *  element - element to fire the dragstart event on
 *  expectedDragData - the data you expect the data transfer to contain afterwards
 *                      This data is in the format:
 *                         [ [ {type: value, data: value, test: function}, ... ], ... ]
 *                     can be null
 *  aWindow - optional; defaults to the current window object.
 *  x - optional; initial x coordinate
 *  y - optional; initial y coordinate
 * Returns null if data matches.
 * Returns the event.dataTransfer if data does not match
 *
 * eqTest is an optional function if comparison can't be done with x == y;
 *   function (actualData, expectedData) {return boolean}
 *   @param actualData from dataTransfer
 *   @param expectedData from expectedDragData
 * see bug 462172 for example of use
 *
 */
function synthesizeDragStart(element, expectedDragData, aWindow, x, y)
{
  if (!aWindow)
    aWindow = window;
  x = x || 2;
  y = y || 2;
  const step = 9;

  var result = "trapDrag was not called";
  var trapDrag = function(event) {
    try {
      // We must wrap only in plain mochitests, not chrome
      var dataTransfer = _EU_maybeWrap(event.dataTransfer);
      result = null;
      if (!dataTransfer)
        throw "no dataTransfer";
      if (expectedDragData == null ||
          dataTransfer.mozItemCount != expectedDragData.length)
        throw dataTransfer;
      for (var i = 0; i < dataTransfer.mozItemCount; i++) {
        var dtTypes = dataTransfer.mozTypesAt(i);
        if (dtTypes.length != expectedDragData[i].length)
          throw dataTransfer;
        for (var j = 0; j < dtTypes.length; j++) {
          if (dtTypes[j] != expectedDragData[i][j].type)
            throw dataTransfer;
          var dtData = dataTransfer.mozGetDataAt(dtTypes[j],i);
          if (expectedDragData[i][j].eqTest) {
            if (!expectedDragData[i][j].eqTest(dtData, expectedDragData[i][j].data))
              throw dataTransfer;
          }
          else if (expectedDragData[i][j].data != dtData)
            throw dataTransfer;
        }
      }
    } catch(ex) {
      result = ex;
    }
    event.preventDefault();
    event.stopPropagation();
  }
  aWindow.addEventListener("dragstart", trapDrag);
  synthesizeMouse(element, x, y, { type: "mousedown" }, aWindow);
  x += step; y += step;
  synthesizeMouse(element, x, y, { type: "mousemove" }, aWindow);
  x += step; y += step;
  synthesizeMouse(element, x, y, { type: "mousemove" }, aWindow);
  aWindow.removeEventListener("dragstart", trapDrag);
  synthesizeMouse(element, x, y, { type: "mouseup" }, aWindow);
  return result;
}

/**
 * Synthesize a query text rect event.
 *
 * @param aOffset  The character offset.  0 means the first character in the
 *                 selection root.
 * @param aLength  The length of the text.  If the length is too long,
 *                 the extra length is ignored.
 * @param aWindow  Optional (If null, current |window| will be used)
 * @return         An nsIQueryContentEventResult object.  If this failed,
 *                 the result might be null.
 */
function synthesizeQueryTextRect(aOffset, aLength, aWindow)
{
  var utils = _getDOMWindowUtils(aWindow);
  if (!utils) {
    return nullptr;
  }
  return utils.sendQueryContentEvent(utils.QUERY_TEXT_RECT,
                                     aOffset, aLength, 0, 0,
                                     QUERY_CONTENT_FLAG_USE_NATIVE_LINE_BREAK);
}

/**
 * Synthesize a query text rect array event.
 *
 * @param aOffset  The character offset.  0 means the first character in the
 *                 selection root.
 * @param aLength  The length of the text.  If the length is too long,
 *                 the extra length is ignored.
 * @param aWindow  Optional (If null, current |window| will be used)
 * @return         An nsIQueryContentEventResult object.  If this failed,
 *                 the result might be null.
 */
function synthesizeQueryTextRectArray(aOffset, aLength, aWindow)
{
  var utils = _getDOMWindowUtils(aWindow);
  if (!utils) {
    return nullptr;
  }
  return utils.sendQueryContentEvent(utils.QUERY_TEXT_RECT_ARRAY,
                                     aOffset, aLength, 0, 0,
                                     QUERY_CONTENT_FLAG_USE_NATIVE_LINE_BREAK);
}

/**
 * Synthesize a query editor rect event.
 *
 * @param aWindow  Optional (If null, current |window| will be used)
 * @return         An nsIQueryContentEventResult object.  If this failed,
 *                 the result might be null.
 */
function synthesizeQueryEditorRect(aWindow)
{
  var utils = _getDOMWindowUtils(aWindow);
  if (!utils) {
    return nullptr;
  }
  return utils.sendQueryContentEvent(utils.QUERY_EDITOR_RECT, 0, 0, 0, 0,
                                     QUERY_CONTENT_FLAG_USE_NATIVE_LINE_BREAK);
}

/**
 * Synthesize a character at point event.
 *
 * @param aX, aY   The offset in the client area of the DOM window.
 * @param aWindow  Optional (If null, current |window| will be used)
 * @return         An nsIQueryContentEventResult object.  If this failed,
 *                 the result might be null.
 */
function synthesizeCharAtPoint(aX, aY, aWindow)
{
  var utils = _getDOMWindowUtils(aWindow);
  if (!utils) {
    return nullptr;
  }
  return utils.sendQueryContentEvent(utils.QUERY_CHARACTER_AT_POINT,
                                     0, 0, aX, aY,
                                     QUERY_CONTENT_FLAG_USE_NATIVE_LINE_BREAK);
}

/**
 * INTERNAL USE ONLY
 * Create an event object to pass to sendDragEvent.
 *
 * @param aType          The string represents drag event type.
 * @param aDestElement   The element to fire the drag event, used to calculate
 *                       screenX/Y and clientX/Y.
 * @param aDestWindow    Optional; Defaults to the current window object.
 * @param aDataTransfer  dataTransfer for current drag session.
 * @param aDragEvent     The object contains properties to override the event
 *                       object
 * @return               An object to pass to sendDragEvent.
 */
function createDragEventObject(aType, aDestElement, aDestWindow, aDataTransfer,
                               aDragEvent)
{
  var destRect = aDestElement.getBoundingClientRect();
  var destClientX = destRect.left + destRect.width / 2;
  var destClientY = destRect.top + destRect.height / 2;
  var destScreenX = aDestWindow.mozInnerScreenX + destClientX;
  var destScreenY = aDestWindow.mozInnerScreenY + destClientY;
  if ("clientX" in aDragEvent && !("screenX" in aDragEvent)) {
    aDragEvent.screenX = aDestWindow.mozInnerScreenX + aDragEvent.clientX;
  }
  if ("clientY" in aDragEvent && !("screenY" in aDragEvent)) {
    aDragEvent.screenY = aDestWindow.mozInnerScreenY + aDragEvent.clientY;
  }

  // Wrap only in plain mochitests
  let dataTransfer = _EU_maybeUnwrap(_EU_maybeWrap(aDataTransfer).mozCloneForEvent(aType));

  // Copy over the drop effect. This isn't copied over by Clone, as it uses more
  // complex logic in the actual implementation (see
  // nsContentUtils::SetDataTransferInEvent for actual impl).
  dataTransfer.dropEffect = aDataTransfer.dropEffect;

  return Object.assign({
    type: aType,
    screenX: destScreenX, screenY: destScreenY,
    clientX: destClientX, clientY: destClientY,
    dataTransfer: dataTransfer,
    _domDispatchOnly: aDragEvent._domDispatchOnly,
  }, aDragEvent);
}

/**
 * Emulate a event sequence of dragstart, dragenter, and dragover.
 *
 * @param aSrcElement   The element to use to start the drag.
 * @param aDestElement  The element to fire the dragover, dragenter events
 * @param aDragData     The data to supply for the data transfer.
 *                      This data is in the format:
 *                        [ [ {type: value, data: value}, ...], ... ]
 *                      Pass null to avoid modifying dataTransfer.
 * @param aDropEffect   The drop effect to set during the dragstart event, or
 *                      'move' if null.
 * @param aWindow       Optional; Defaults to the current window object.
 * @param aDestWindow   Optional; Defaults to aWindow.
 *                      Used when aDestElement is in a different window than
 *                      aSrcElement.
 * @param aDragEvent    Optional; Defaults to empty object. Overwrites an object
 *                      passed to sendDragEvent.
 * @return              A two element array, where the first element is the
 *                      value returned from sendDragEvent for
 *                      dragover event, and the second element is the
 *                      dataTransfer for the current drag session.
 */
function synthesizeDragOver(aSrcElement, aDestElement, aDragData, aDropEffect, aWindow, aDestWindow, aDragEvent={})
{
  if (!aWindow) {
    aWindow = window;
  }
  if (!aDestWindow) {
    aDestWindow = aWindow;
  }

  const obs = _EU_Cc["@mozilla.org/observer-service;1"].getService(_EU_Ci.nsIObserverService);
  const ds = _EU_Cc["@mozilla.org/widget/dragservice;1"].getService(_EU_Ci.nsIDragService);
  var sess = ds.getCurrentSession();

  // This method runs before other callbacks, and acts as a way to inject the
  // initial drag data into the DataTransfer.
  function fillDrag(event) {
    if (aDragData) {
      for (var i = 0; i < aDragData.length; i++) {
        var item = aDragData[i];
        for (var j = 0; j < item.length; j++) {
          _EU_maybeWrap(event.dataTransfer).mozSetDataAt(item[j].type, item[j].data, i);
        }
      }
    }
    event.dataTransfer.dropEffect = aDropEffect || "move";
    event.preventDefault();
  }

  function trapDrag(subject, topic) {
    if (topic == "on-datatransfer-available") {
      sess.dataTransfer = _EU_maybeUnwrap(_EU_maybeWrap(subject).mozCloneForEvent("drop"));
      sess.dataTransfer.dropEffect = subject.dropEffect;
    }
  }

  // need to use real mouse action
  aWindow.addEventListener("dragstart", fillDrag, true);
  obs.addObserver(trapDrag, "on-datatransfer-available");
  synthesizeMouseAtCenter(aSrcElement, { type: "mousedown" }, aWindow);

  var rect = aSrcElement.getBoundingClientRect();
  var x = rect.width / 2;
  var y = rect.height / 2;
  synthesizeMouse(aSrcElement, x, y, { type: "mousemove" }, aWindow);
  synthesizeMouse(aSrcElement, x+10, y+10, { type: "mousemove" }, aWindow);
  aWindow.removeEventListener("dragstart", fillDrag, true);
  obs.removeObserver(trapDrag, "on-datatransfer-available");

  var dataTransfer = sess.dataTransfer;
  if (!dataTransfer) {
    throw new Error("No data transfer object after synthesizing the mouse!");
  }

  // The EventStateManager will fire our dragenter event if it needs to.
  var event = createDragEventObject("dragover", aDestElement, aDestWindow,
                                    dataTransfer, aDragEvent);
  var result = sendDragEvent(event, aDestElement, aDestWindow);

  return [result, dataTransfer];
}

/**
 * Emulate the drop event and mouseup event.
 * This should be called after synthesizeDragOver.
 *
 * @param aResult        The first element of the array returned from
 *                       synthesizeDragOver.
 * @param aDataTransfer  The second element of the array returned from
 *                       synthesizeDragOver.
 * @param aDestElement   The element to fire the drop event.
 * @param aDestWindow    Optional; Defaults to the current window object.
 * @param aDragEvent     Optional; Defaults to empty object. Overwrites an
 *                       object passed to sendDragEvent.
 * @return               "none" if aResult is true,
 *                       aDataTransfer.dropEffect otherwise.
 */
function synthesizeDropAfterDragOver(aResult, aDataTransfer, aDestElement, aDestWindow, aDragEvent={})
{
  if (!aDestWindow) {
    aDestWindow = window;
  }

  var effect = aDataTransfer.dropEffect;
  var event;

  if (aResult) {
    effect = "none";
  } else if (effect != "none") {
    event = createDragEventObject("drop", aDestElement, aDestWindow,
                                  aDataTransfer, aDragEvent);
    sendDragEvent(event, aDestElement, aDestWindow);
  }

  return effect;
}

/**
 * Emulate a drag and drop by emulating a dragstart and firing events dragenter,
 * dragover, and drop.
 *
 * @param aSrcElement   The element to use to start the drag.
 * @param aDestElement  The element to fire the dragover, dragenter events
 * @param aDragData     The data to supply for the data transfer.
 *                      This data is in the format:
 *                        [ [ {type: value, data: value}, ...], ... ]
 *                      Pass null to avoid modifying dataTransfer.
 * @param aDropEffect   The drop effect to set during the dragstart event, or
 *                      'move' if null.
 * @param aWindow       Optional; Defaults to the current window object.
 * @param aDestWindow   Optional; Defaults to aWindow.
 *                      Used when aDestElement is in a different window than
 *                      aSrcElement.
 * @param aDragEvent    Optional; Defaults to empty object. Overwrites an object
 *                      passed to sendDragEvent.
 * @return              The drop effect that was desired.
 */
function synthesizeDrop(aSrcElement, aDestElement, aDragData, aDropEffect, aWindow, aDestWindow, aDragEvent={})
{
  if (!aWindow) {
    aWindow = window;
  }
  if (!aDestWindow) {
    aDestWindow = aWindow;
  }

  var ds = _EU_Cc["@mozilla.org/widget/dragservice;1"]
           .getService(_EU_Ci.nsIDragService);

  ds.startDragSession();

  try {
    var [result, dataTransfer] = synthesizeDragOver(aSrcElement, aDestElement,
                                                    aDragData, aDropEffect,
                                                    aWindow, aDestWindow,
                                                    aDragEvent);
    return synthesizeDropAfterDragOver(result, dataTransfer, aDestElement,
                                       aDestWindow, aDragEvent);
  } finally {
    ds.endDragSession(true, _parseModifiers(aDragEvent));
  }
}

/**
 * Emulate a drag and drop by emulating a dragstart by mousedown and mousemove,
 * and firing events dragenter, dragover, drop, and dragend.
 * This does not modify dataTransfer and tries to emulate the plain drag and
 * drop as much as possible, compared to synthesizeDrop.
 *
 * @param aParams
 *        {
 *          srcElement:   The element to start dragging
 *          destElement:  The element to drop on. Pass null to emulate
 *                        a drop on an invalid target.
 *          srcX:         The initial x coordinate inside srcElement
 *          srcY:         The initial y coordinate inside srcElement
 *          stepX:        The x-axis step for mousemove inside srcElement
 *          stepY:        The y-axis step for mousemove inside srcElement
 *          finalX:       The final x coordinate inside srcElement
 *          finalY:       The final x coordinate inside srcElement
 *          srcWindow:    The window for dispatching event on srcElement,
 *                        defaults to the current window object
 *          destWindow:   The window for dispatching event on destElement,
 *                        defaults to the current window object
 *        }
 */
async function synthesizePlainDragAndDrop(aParams)
{
  let {
    srcElement,
    destElement,
    srcX = 2,
    srcY = 2,
    stepX = 9,
    stepY = 9,
    finalX = srcX + stepX * 2,
    finalY = srcY + stepY * 2,
    srcWindow = window,
    destWindow = window,
  } = aParams;

  const ds = _EU_Cc["@mozilla.org/widget/dragservice;1"]
        .getService(_EU_Ci.nsIDragService);
  ds.startDragSession();

  try {
    let dataTransfer = null;
    function trapDrag(aEvent) {
      dataTransfer = aEvent.dataTransfer;
    }
    srcElement.addEventListener("dragstart", trapDrag, true);
    synthesizeMouse(srcElement, srcX, srcY, { type: "mousedown" }, srcWindow);

    // Wait for the next event tick after each event dispatch, so that UI elements
    // (e.g. menu) work like the real user input.
    await new Promise(r => setTimeout(r, 0));

    srcX += stepX; srcY += stepY;
    synthesizeMouse(srcElement, srcX, srcY, { type: "mousemove" }, srcWindow);

    await new Promise(r => setTimeout(r, 0));

    srcX += stepX; srcY += stepY;
    synthesizeMouse(srcElement, srcX, srcY, { type: "mousemove" }, srcWindow);

    await new Promise(r => setTimeout(r, 0));

    srcElement.removeEventListener("dragstart", trapDrag, true);

    await new Promise(r => setTimeout(r, 0));

    let event;
    if (destElement) {
      // dragover and drop are only fired to a valid drop target. If the
      // destElement parameter is null, this function is being used to
      // simulate a drag'n'drop over an invalid drop target.
      event = createDragEventObject("dragover", destElement, destWindow,
                                        dataTransfer, {});
      sendDragEvent(event, destElement, destWindow);

      await new Promise(r => setTimeout(r, 0));

      event = createDragEventObject("drop", destElement, destWindow,
                                    dataTransfer, {});
      sendDragEvent(event, destElement, destWindow);
    }

    // dragend is fired, by definition, on the srcElement
    event = createDragEventObject("dragend", srcElement, srcWindow,
                                  dataTransfer, {clientX: finalX, clientY: finalY});
    sendDragEvent(event, srcElement, srcWindow);

    await new Promise(r => setTimeout(r, 0));

  } finally {
    ds.endDragSession(true, 0);
  }
}

var PluginUtils =
{
  withTestPlugin : function(callback)
  {
    var ph = _EU_Cc["@mozilla.org/plugin/host;1"]
             .getService(_EU_Ci.nsIPluginHost);
    var tags = ph.getPluginTags();

    // Find the test plugin
    for (var i = 0; i < tags.length; i++) {
      if (tags[i].name == "Test Plug-in") {
        callback(tags[i]);
        return true;
      }
    }
    todo(false, "Need a test plugin on this platform");
    return false;
  }
};
