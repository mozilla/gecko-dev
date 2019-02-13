/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

module.metadata = {
  'stability': 'unstable'
};

const { Cc, Ci, Cu } = require('chrome');
const { Unknown } = require('../platform/xpcom');
const { Class } = require('../core/heritage');
const { ns } = require('../core/namespace');
const { addObserver, removeObserver, notifyObservers } = 
  Cc['@mozilla.org/observer-service;1'].getService(Ci.nsIObserverService);
const unloadSubject = require('@loader/unload');

const Subject = Class({
  extends: Unknown,
  initialize: function initialize(object) {
    // Double-wrap the object and set a property identifying the
    // wrappedJSObject as one of our wrappers to distinguish between
    // subjects that are one of our wrappers (which we should unwrap
    // when notifying our observers) and those that are real JS XPCOM
    // components (which we should pass through unaltered).
    this.wrappedJSObject = {
      observersModuleSubjectWrapper: true,
      object: object
    };
  },
  getScriptableHelper: function() {},
  getInterfaces: function() {}
});

function emit(type, event) {
  // From bug 910599
  // We must test to see if 'subject' or 'data' is a defined property
  // of the event object, but also allow primitives to be passed in,
  // which the `in` operator breaks, yet `null` is an object, hence
  // the long conditional
  let subject = event && typeof event === 'object' && 'subject' in event ?
    Subject(event.subject) :
    null;
  let data = event && typeof event === 'object' ?
    // An object either returns its `data` property or null
    ('data' in event ? event.data : null) :
    // All other types return themselves (and cast to strings/null
    // via observer service)
    event;
  notifyObservers(subject, type, data);
}
exports.emit = emit;

const Observer = Class({
  extends: Unknown,
  initialize: function initialize(listener) {
    this.listener = listener;
  },
  interfaces: [ 'nsIObserver', 'nsISupportsWeakReference' ],
  observe: function(subject, topic, data) {
    // Extract the wrapped object for subjects that are one of our
    // wrappers around a JS object.  This way we support both wrapped
    // subjects created using this module and those that are real
    // XPCOM components.
    if (subject && typeof(subject) == 'object' &&
        ('wrappedJSObject' in subject) &&
        ('observersModuleSubjectWrapper' in subject.wrappedJSObject))
      subject = subject.wrappedJSObject.object;

    try {
      this.listener({
        type: topic,
        subject: subject,
        data: data
      });
    }
    catch (error) {
      console.exception(error);
    }
  }
});

const subscribers = ns();

function on(type, listener, strong) {
  // Unless last optional argument is `true` we use a weak reference to a
  // listener.
  let weak = !strong;
  // Take list of observers associated with given `listener` function.
  let observers = subscribers(listener);
  // If `observer` for the given `type` is not registered yet, then
  // associate an `observer` and register it.
  if (!(type in observers)) {
    let observer = Observer(listener);
    observers[type] = observer;
    addObserver(observer, type, weak);
    // WeakRef gymnastics to remove all alive observers on unload
    let ref = Cu.getWeakReference(observer);
    weakRefs.set(observer, ref);
    stillAlive.set(ref, type);
  }
}
exports.on = on;

function once(type, listener) {
  // Note: this code assumes order in which listeners are called, which is fine
  // as long as dispatch happens in same order as listener registration which
  // is the case now. That being said we should be aware that this may break
  // in a future if order will change.
  on(type, listener);
  on(type, function cleanup() {
    off(type, listener);
    off(type, cleanup);
  }, true);
}
exports.once = once;

function off(type, listener) {
  // Take list of observers as with the given `listener`.
  let observers = subscribers(listener);
  // If `observer` for the given `type` is registered, then
  // remove it & unregister.
  if (type in observers) {
    let observer = observers[type];
    delete observers[type];
    removeObserver(observer, type);
    stillAlive.delete(weakRefs.get(observer));
  }
}
exports.off = off;

// must use WeakMap to keep reference to all the WeakRefs (!), see bug 986115
let weakRefs = new WeakMap();

// and we're out of beta, we're releasing on time!
let stillAlive = new Map();   

on('sdk:loader:destroy', function onunload({ subject, data: reason }) {
  // using logic from ./unload, to avoid a circular module reference
  if (subject.wrappedJSObject === unloadSubject) {
    off('sdk:loader:destroy', onunload);

    // don't bother
    if (reason === 'shutdown') 
      return;

    stillAlive.forEach( (type, ref) => {
      let observer = ref.get();
      if (observer) 
        removeObserver(observer, type);
    })
  }
  // a strong reference
}, true);
