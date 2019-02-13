/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 sw=2 sts=2 et: */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";
dump("############################### browserElementPanningAPZDisabled.js loaded\n");

let { classes: Cc, interfaces: Ci, results: Cr, utils: Cu }  = Components;
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Geometry.jsm");

var global = this;

const ContentPanningAPZDisabled = {
  // Are we listening to touch or mouse events?
  watchedEventsType: '',

  // Are mouse events being delivered to this content along with touch
  // events, in violation of spec?
  hybridEvents: false,

  init: function cp_init() {
    this._setupListenersForPanning();
  },

  _setupListenersForPanning: function cp_setupListenersForPanning() {
    let events;

    if (content.TouchEvent) {
      events = ['touchstart', 'touchend', 'touchmove'];
      this.watchedEventsType = 'touch';
#ifdef MOZ_WIDGET_GONK
      // The gonk widget backend does not deliver mouse events per
      // spec.  Third-party content isn't exposed to this behavior,
      // but that behavior creates some extra work for us here.
      let appInfo = Cc["@mozilla.org/xre/app-info;1"];
      let isParentProcess =
        !appInfo || appInfo.getService(Ci.nsIXULRuntime)
                           .processType == Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT;
      this.hybridEvents = isParentProcess;
#endif
    } else {
      // Touch events aren't supported, so fall back on mouse.
      events = ['mousedown', 'mouseup', 'mousemove'];
      this.watchedEventsType = 'mouse';
    }

    let els = Cc["@mozilla.org/eventlistenerservice;1"]
                .getService(Ci.nsIEventListenerService);

    events.forEach(function(type) {
      // Using the system group for mouse/touch events to avoid
      // missing events if .stopPropagation() has been called.
      els.addSystemEventListener(global, type,
                                 this.handleEvent.bind(this),
                                 /* useCapture = */ false);
    }.bind(this));
  },

  handleEvent: function cp_handleEvent(evt) {
    // Ignore events targeting an oop <iframe mozbrowser> since those will be
    // handle by the BrowserElementPanning.js instance in the child process.
    if (evt.target instanceof Ci.nsIMozBrowserFrame) {
      return;
    }

    // For in-process <iframe mozbrowser> the events are not targetting
    // directly the container iframe element, but some node of the document.
    // So, the BrowserElementPanning instance of the system app will receive
    // the sequence of touch events, as well as the BrowserElementPanning
    // instance in the targetted app.
    // As a result, multiple mozbrowser iframes will try to interpret the
    // sequence of touch events, which may results into multiple clicks.
    let targetWindow = evt.target.ownerDocument.defaultView;
    let frameElement = targetWindow.frameElement;
    while (frameElement) {
      targetWindow = frameElement.ownerDocument.defaultView;
      frameElement = targetWindow.frameElement;
    }

    if (content !== targetWindow) {
      return;
    }

    if (evt.defaultPrevented || evt.multipleActionsPrevented) {
      // clean up panning state even if touchend/mouseup has been preventDefault.
      if(evt.type === 'touchend' || evt.type === 'mouseup') {
        if (this.dragging &&
            (this.watchedEventsType === 'mouse' ||
             this.findPrimaryPointer(evt.changedTouches))) {
          this._finishPanning();
        }
      }
      return;
    }

    switch (evt.type) {
      case 'mousedown':
      case 'touchstart':
        this.onTouchStart(evt);
        break;
      case 'mousemove':
      case 'touchmove':
        this.onTouchMove(evt);
        break;
      case 'mouseup':
      case 'touchend':
        this.onTouchEnd(evt);
        break;
      case 'click':
        evt.stopPropagation();
        evt.preventDefault();

        let target = evt.target;
        let view = target.ownerDocument ? target.ownerDocument.defaultView
                                        : target;
        view.removeEventListener('click', this, true, true);
        break;
    }
  },

  position: new Point(0 , 0),

  findPrimaryPointer: function cp_findPrimaryPointer(touches) {
    if (!('primaryPointerId' in this))
      return null;

    for (let i = 0; i < touches.length; i++) {
      if (touches[i].identifier === this.primaryPointerId) {
        return touches[i];
      }
    }
    return null;
  },

  onTouchStart: function cp_onTouchStart(evt) {
    let screenX, screenY;
    if (this.watchedEventsType == 'touch') {
      if ('primaryPointerId' in this || evt.touches.length >= 2) {
        this._resetActive();
        return;
      }

      let firstTouch = evt.changedTouches[0];
      this.primaryPointerId = firstTouch.identifier;
      this.pointerDownTarget = firstTouch.target;
      screenX = firstTouch.screenX;
      screenY = firstTouch.screenY;
    } else {
      this.pointerDownTarget = evt.target;
      screenX = evt.screenX;
      screenY = evt.screenY;
    }
    this.dragging = true;
    this.panning = false;

    let oldTarget = this.target;
    [this.target, this.scrollCallback] = this.getPannable(this.pointerDownTarget);

    // If we have a pointer down target, we may need to fill in for EventStateManager
    // in setting the active state on the target element.  Set a timer to
    // ensure the pointer-down target is active.  (If it's already
    // active, the timer is a no-op.)
    if (this.pointerDownTarget !== null) {
      // If there's no possibility this is a drag/pan, activate now.
      // Otherwise wait a little bit to see if the gesture isn't a
      // tap.
      if (this.target === null) {
        this.notify(this._activationTimer);
      } else {
        this._activationTimer.initWithCallback(this,
                                               this._activationDelayMs,
                                               Ci.nsITimer.TYPE_ONE_SHOT);
      }
    }

    // If there is a pan animation running (from a previous pan gesture) and
    // the user touch back the screen, stop this animation immediatly and
    // prevent the possible click action if the touch happens on the same
    // target.
    this.preventNextClick = false;
    if (KineticPanning.active) {
      KineticPanning.stop();

      if (oldTarget && oldTarget == this.target)
        this.preventNextClick = true;
    }

    this.position.set(screenX, screenY);
    KineticPanning.reset();
    KineticPanning.record(new Point(0, 0), evt.timeStamp);

    // We prevent start events to avoid sending a focus event at the end of this
    // touch series. See bug 889717.
    if ((this.panning || this.preventNextClick)) {
      evt.preventDefault();
    }
  },

  onTouchEnd: function cp_onTouchEnd(evt) {
    let touch = null;
    if (!this.dragging ||
        (this.watchedEventsType == 'touch' &&
         !(touch = this.findPrimaryPointer(evt.changedTouches)))) {
      return;
    }

    // !isPan() and evt.detail should always give the same answer here
    // since they use the same heuristics, but use the native gecko
    // computation when possible.
    //
    // NB: when we're using touch events, then !KineticPanning.isPan()
    // => this.panning, so we'll never attempt to block the click
    // event.  That's OK however, because we won't fire a synthetic
    // click when we're using touch events and this touch series
    // wasn't a "tap" gesture.
    let click = (this.watchedEventsType == 'mouse') ?
      evt.detail : !KineticPanning.isPan();
    // Additionally, if we're seeing non-compliant hybrid events, a
    // "real" click will be generated if we started and ended on the
    // same element.
    if (this.hybridEvents) {
      let target =
        content.document.elementFromPoint(touch.clientX, touch.clientY);
      click |= (target === this.pointerDownTarget);
    }

    if (this.target && click && (this.panning || this.preventNextClick)) {
      if (this.hybridEvents) {
        let target = this.target;
        let view = target.ownerDocument ? target.ownerDocument.defaultView
                                        : target;
        view.addEventListener('click', this, true, true);
      } else {
        // We prevent end events to avoid sending a focus event. See bug 889717.
        evt.preventDefault();
      }
    } else if (this.target && click && !this.panning) {
      this.notify(this._activationTimer);
    }

    this._finishPanning();

    // Now that we're done, avoid entraining the thing we just panned.
    this.pointerDownTarget = null;
  },

  onTouchMove: function cp_onTouchMove(evt) {
    if (!this.dragging)
      return;

    let screenX, screenY;
    if (this.watchedEventsType == 'touch') {
      let primaryTouch = this.findPrimaryPointer(evt.changedTouches);
      if (evt.touches.length > 1 || !primaryTouch)
        return;
      screenX = primaryTouch.screenX;
      screenY = primaryTouch.screenY;
    } else {
      screenX = evt.screenX;
      screenY = evt.screenY;
    }

    let current = this.position;
    let delta = new Point(screenX - current.x, screenY - current.y);
    current.set(screenX, screenY);

    KineticPanning.record(delta, evt.timeStamp);

    let isPan = KineticPanning.isPan();

    // If we've detected a pan gesture, cancel the active state of the
    // current target.
    if (!this.panning && isPan) {
      this._resetActive();
    }

    // There's no possibility of us panning anything.
    if (!this.scrollCallback) {
      return;
    }

    // Scroll manually.
    this.scrollCallback(delta.scale(-1));

    if (!this.panning && isPan) {
      this.panning = true;
      this._activationTimer.cancel();
    }

    if (this.panning) {
      // Only do this when we're actually executing a pan gesture.
      // Otherwise synthetic mouse events will be canceled.
      evt.stopPropagation();
      evt.preventDefault();
    }
  },

  // nsITimerCallback
  notify: function cp_notify(timer) {
    this._setActive(this.pointerDownTarget);
  },

  onKineticBegin: function cp_onKineticBegin(evt) {
  },

  onKineticPan: function cp_onKineticPan(delta) {
    return !this.scrollCallback(delta);
  },

  onKineticEnd: function cp_onKineticEnd() {
    if (!this.dragging)
      this.scrollCallback = null;
  },

  getPannable: function cp_getPannable(node) {
    let pannableNode = this._findPannable(node);
    if (pannableNode) {
      return [pannableNode, this._generateCallback(pannableNode)];
    }

    return [null, null];
  },

  _findPannable: function cp_findPannable(node) {
    if (!(node instanceof Ci.nsIDOMHTMLElement) || node.tagName == 'HTML') {
      return null;
    }

    let nodeContent = node.ownerDocument.defaultView;
    while (!(node instanceof Ci.nsIDOMHTMLBodyElement)) {
      let style = nodeContent.getComputedStyle(node, null);

      let overflow = [style.getPropertyValue('overflow'),
                      style.getPropertyValue('overflow-x'),
                      style.getPropertyValue('overflow-y')];

      let rect = node.getBoundingClientRect();
      let isAuto = (overflow.indexOf('auto') != -1 &&
                   (rect.height < node.scrollHeight ||
                    rect.width < node.scrollWidth));

      let isScroll = (overflow.indexOf('scroll') != -1);

      let isScrollableTextarea = (node.tagName == 'TEXTAREA' &&
          (node.scrollHeight > node.clientHeight ||
           node.scrollWidth > node.clientWidth ||
           ('scrollLeftMax' in node && node.scrollLeftMax > 0) ||
           ('scrollTopMax' in node && node.scrollTopMax > 0)));
      if (isScroll || isAuto || isScrollableTextarea) {
        return node;
      }

      node = node.parentNode;
    }

    if (nodeContent.scrollMaxX || nodeContent.scrollMaxY) {
      return nodeContent;
    }

    if (nodeContent.frameElement) {
      return this._findPannable(nodeContent.frameElement);
    }

    return null;
  },

  _generateCallback: function cp_generateCallback(root) {
    let firstScroll = true;
    let target;
    let current;
    let win, doc, htmlNode, bodyNode;

    function doScroll(node, delta) {
      if (node instanceof Ci.nsIDOMHTMLElement) {
        return node.scrollByNoFlush(delta.x, delta.y);
      } else if (node instanceof Ci.nsIDOMWindow) {
        win = node;
        doc = win.document;

        // "overflow:hidden" on either the <html> or the <body> node should
        // prevent the user from scrolling the root viewport.
        if (doc instanceof Ci.nsIDOMHTMLDocument) {
          htmlNode = doc.documentElement;
          bodyNode = doc.body;
          if (win.getComputedStyle(htmlNode, null).overflowX == "hidden" ||
              win.getComputedStyle(bodyNode, null).overflowX == "hidden") {
            delta.x = 0;
          }
          if (win.getComputedStyle(htmlNode, null).overflowY == "hidden" ||
              win.getComputedStyle(bodyNode, null).overflowY == "hidden") {
            delta.y = 0;
          }
        }
        let oldX = node.scrollX;
        let oldY = node.scrollY;
        node.scrollBy(delta.x, delta.y);
        return (node.scrollX != oldX || node.scrollY != oldY);
      }
      // If we get here, |node| isn't an HTML element and it's not a window,
      // but findPannable apparently thought it was scrollable... What is it?
      return false;
    }

    function targetParent(node) {
      return node.parentNode || node.frameElement || null;
    }

    function scroll(delta) {
      current = root;
      firstScroll = true;
      while (current) {
        if (doScroll(current, delta)) {
          firstScroll = false;
          return true;
        }

        // TODO The current code looks for possible scrolling regions only if
        // this is the first scroll action but this should be more dynamic.
        if (!firstScroll) {
          return false;
        }

        current = ContentPanningAPZDisabled._findPannable(targetParent(current));
      }

      // There is nothing scrollable here.
      return false;
    }
    return scroll;
  },

  get _activationTimer() {
    delete this._activationTimer;
    return this._activationTimer = Cc["@mozilla.org/timer;1"]
                                     .createInstance(Ci.nsITimer);
  },

  get _activationDelayMs() {
    let delay = Services.prefs.getIntPref('ui.touch_activation.delay_ms');
    delete this._activationDelayMs;
    return this._activationDelayMs = delay;
  },

  get _domUtils() {
    delete this._domUtils;
    return this._domUtils = Cc['@mozilla.org/inspector/dom-utils;1']
                              .getService(Ci.inIDOMUtils);
  },

  _resetActive: function cp_resetActive() {
    let elt = this.pointerDownTarget || this.target;
    let root = elt.ownerDocument || elt.document;
    this._setActive(root.documentElement);
  },

  _setActive: function cp_setActive(elt) {
    const kStateActive = 0x00000001;
    this._domUtils.setContentState(elt, kStateActive);
  },

  _finishPanning: function() {
    this.dragging = false;
    delete this.primaryPointerId;
    this._activationTimer.cancel();

    // If there is a scroll action, let's do a manual kinetic panning action.
    if (this.panning) {
      KineticPanning.start(this);
    }
  },
};

// Min/max velocity of kinetic panning. This is in pixels/millisecond.
const kMinVelocity = 0.2;
const kMaxVelocity = 6;

// Constants that affect the "friction" of the scroll pane.
const kExponentialC = 1000;
const kPolynomialC = 100 / 1000000;

// How often do we change the position of the scroll pane?
// Too often and panning may jerk near the end.
// Too little and panning will be choppy. In milliseconds.
const kUpdateInterval = 16;

// The numbers of momentums to use for calculating the velocity of the pan.
// Those are taken from the end of the action
const kSamples = 5;

const KineticPanning = {
  _position: new Point(0, 0),
  _velocity: new Point(0, 0),
  _acceleration: new Point(0, 0),

  get active() {
    return this.target !== null;
  },

  target: null,
  start: function kp_start(target) {
    this.target = target;

    // Calculate the initial velocity of the movement based on user input
    let momentums = this.momentums;
    let flick = momentums[momentums.length - 1].time - momentums[0].time < 300;

    let distance = new Point(0, 0);
    momentums.forEach(function(momentum) {
      distance.add(momentum.dx, momentum.dy);
    });

    function clampFromZero(x, min, max) {
      if (x >= 0)
        return Math.max(min, Math.min(max, x));
      return Math.min(-min, Math.max(-max, x));
    }

    let elapsed = momentums[momentums.length - 1].time - momentums[0].time;
    let velocityX = clampFromZero(distance.x / elapsed, 0, kMaxVelocity);
    let velocityY = clampFromZero(distance.y / elapsed, 0, kMaxVelocity);

    let velocity = this._velocity;
    if (flick) {
      // Very fast pan action that does not generate a click are very often pan
      // action. If this is a small gesture then it will not move the view a lot
      // and so it will be above the minimun threshold and not generate any
      // kinetic panning. This does not look on a device since this is often
      // a real gesture, so let's lower the velocity threshold for such moves.
      velocity.set(velocityX, velocityY);
    } else {
      velocity.set(Math.abs(velocityX) < kMinVelocity ? 0 : velocityX,
                   Math.abs(velocityY) < kMinVelocity ? 0 : velocityY);
    }
    this.momentums = [];

    // Set acceleration vector to opposite signs of velocity
    function sign(x) {
      return x ? (x > 0 ? 1 : -1) : 0;
    }

    this._acceleration.set(velocity.clone().map(sign).scale(-kPolynomialC));

    // Reset the position
    this._position.set(0, 0);

    this._startAnimation();

    this.target.onKineticBegin();
  },

  stop: function kp_stop() {
    this.reset();

    if (!this.target)
      return;

    this.target.onKineticEnd();
    this.target = null;
  },

  reset: function kp_reset() {
    this.momentums = [];
    this.distance.set(0, 0);
  },

  momentums: [],
  record: function kp_record(delta, timestamp) {
    this.momentums.push({ 'time': this._getTime(timestamp),
                          'dx' : delta.x, 'dy' : delta.y });

    // We only need to keep kSamples in this.momentums.
    if (this.momentums.length > kSamples) {
      this.momentums.shift();
    }

    this.distance.add(delta.x, delta.y);
  },

  _getTime: function kp_getTime(time) {
    // Touch events generated by the platform or hand-made are defined in
    // microseconds instead of milliseconds. Bug 77992 will fix this at the
    // platform level.
    if (time > Date.now()) {
      return Math.floor(time / 1000);
    } else {
      return time;
    }
  },

  get threshold() {
    let dpi = content.QueryInterface(Ci.nsIInterfaceRequestor)
                     .getInterface(Ci.nsIDOMWindowUtils)
                     .displayDPI;

    let threshold = Services.prefs.getIntPref('ui.dragThresholdX') / 240 * dpi;

    delete this.threshold;
    return this.threshold = threshold;
  },

  distance: new Point(0, 0),
  isPan: function cp_isPan() {
    return (Math.abs(this.distance.x) > this.threshold ||
            Math.abs(this.distance.y) > this.threshold);
  },

  _startAnimation: function kp_startAnimation() {
    let c = kExponentialC;
    function getNextPosition(position, v, a, t) {
      // Important traits for this function:
      //   p(t=0) is 0
      //   p'(t=0) is v0
      //
      // We use exponential to get a smoother stop, but by itself exponential
      // is too smooth at the end. Adding a polynomial with the appropriate
      // weight helps to balance
      position.set(v.x * Math.exp(-t / c) * -c + a.x * t * t + v.x * c,
                   v.y * Math.exp(-t / c) * -c + a.y * t * t + v.y * c);
    }

    let startTime = content.mozAnimationStartTime;
    let elapsedTime = 0, targetedTime = 0, averageTime = 0;

    let velocity = this._velocity;
    let acceleration = this._acceleration;

    let position = this._position;
    let nextPosition = new Point(0, 0);
    let delta = new Point(0, 0);

    let callback = (function(timestamp) {
      if (!this.target)
        return;

      // To make animation end fast enough but to keep smoothness, average the
      // ideal time frame (smooth animation) with the actual time lapse
      // (end fast enough).
      // Animation will never take longer than 2 times the ideal length of time.
      elapsedTime = timestamp - startTime;
      targetedTime += kUpdateInterval;
      averageTime = (targetedTime + elapsedTime) / 2;

      // Calculate new position.
      getNextPosition(nextPosition, velocity, acceleration, averageTime);
      delta.set(Math.round(nextPosition.x - position.x),
                Math.round(nextPosition.y - position.y));

      // Test to see if movement is finished for each component.
      if (delta.x * acceleration.x > 0)
        delta.x = position.x = velocity.x = acceleration.x = 0;

      if (delta.y * acceleration.y > 0)
        delta.y = position.y = velocity.y = acceleration.y = 0;

      if (velocity.equals(0, 0) || delta.equals(0, 0)) {
        this.stop();
        return;
      }

      position.add(delta);
      if (this.target.onKineticPan(delta.scale(-1))) {
        this.stop();
        return;
      }

      content.mozRequestAnimationFrame(callback);
    }).bind(this);

    content.mozRequestAnimationFrame(callback);
  }
};
