/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/*
 * Target actor for any of the various kinds of workers.
 *
 * See devtools/docs/backend/actor-hierarchy.md for more details.
 */

const { Ci } = require("chrome");
const ChromeUtils = require("ChromeUtils");
const { DebuggerServer } = require("devtools/server/main");
const { XPCOMUtils } = require("resource://gre/modules/XPCOMUtils.jsm");
const protocol = require("devtools/shared/protocol");
const { workerTargetSpec } = require("devtools/shared/specs/targets/worker");

loader.lazyRequireGetter(this, "ChromeUtils");

XPCOMUtils.defineLazyServiceGetter(
  this, "swm",
  "@mozilla.org/serviceworkers/manager;1",
  "nsIServiceWorkerManager"
);

const WorkerTargetActor = protocol.ActorClassWithSpec(workerTargetSpec, {
  initialize(conn, dbg) {
    protocol.Actor.prototype.initialize.call(this, conn);
    this._dbg = dbg;
    this._attached = false;
    this._threadActor = null;
    this._transport = null;
  },

  form(detail) {
    if (detail === "actorid") {
      return this.actorID;
    }
    const form = {
      actor: this.actorID,
      consoleActor: this._consoleActor,
      url: this._dbg.url,
      type: this._dbg.type,
    };
    if (this._dbg.type === Ci.nsIWorkerDebugger.TYPE_SERVICE) {
      const registration = this._getServiceWorkerRegistrationInfo();
      form.scope = registration.scope;
      const newestWorker = (registration.activeWorker ||
                          registration.waitingWorker ||
                          registration.installingWorker);
      form.fetch = newestWorker && newestWorker.handlesFetchEvents;
    }
    return form;
  },

  attach() {
    if (this._dbg.isClosed) {
      return { error: "closed" };
    }

    if (!this._attached) {
      // Automatically disable their internal timeout that shut them down
      // Should be refactored by having actors specific to service workers
      if (this._dbg.type == Ci.nsIWorkerDebugger.TYPE_SERVICE) {
        const worker = this._getServiceWorkerInfo();
        if (worker) {
          worker.attachDebugger();
        }
      }
      this._dbg.addListener(this);
      this._attached = true;
    }

    return {
      type: "attached",
      url: this._dbg.url,
    };
  },

  detach() {
    if (!this._attached) {
      return { error: "wrongState" };
    }

    this._detach();

    return { type: "detached" };
  },

  destroy() {
    if (this._attached) {
      this._detach();
    }
    protocol.Actor.prototype.destroy.call(this);
  },

  connect(options) {
    if (!this._attached) {
      return { error: "wrongState" };
    }

    if (this._threadActor !== null) {
      return {
        type: "connected",
        threadActor: this._threadActor,
      };
    }

    return DebuggerServer.connectToWorker(
      this.conn, this._dbg, this.actorID, options
    ).then(({ threadActor, transport, consoleActor }) => {
      this._threadActor = threadActor;
      this._transport = transport;
      this._consoleActor = consoleActor;

      return {
        type: "connected",
        threadActor: this._threadActor,
        consoleActor: this._consoleActor,
      };
    }, (error) => {
      return { error: error.toString() };
    });
  },

  push() {
    if (this._dbg.type !== Ci.nsIWorkerDebugger.TYPE_SERVICE) {
      return { error: "wrongType" };
    }
    const registration = this._getServiceWorkerRegistrationInfo();
    const originAttributes = ChromeUtils.originAttributesToSuffix(
      this._dbg.principal.originAttributes);
    swm.sendPushEvent(originAttributes, registration.scope);
    return { type: "pushed" };
  },

  onClose() {
    if (this._attached) {
      this._detach();
    }

    this.conn.sendActorEvent(this.actorID, "close");
  },

  onError(filename, lineno, message) {
    reportError("ERROR:" + filename + ":" + lineno + ":" + message + "\n");
  },

  _getServiceWorkerRegistrationInfo() {
    return swm.getRegistrationByPrincipal(this._dbg.principal, this._dbg.url);
  },

  _getServiceWorkerInfo() {
    const registration = this._getServiceWorkerRegistrationInfo();
    return registration.getWorkerByID(this._dbg.serviceWorkerID);
  },

  _detach() {
    if (this._threadActor !== null) {
      this._transport.close();
      this._transport = null;
      this._threadActor = null;
    }

    // If the worker is already destroyed, nsIWorkerDebugger.type throws
    // (_dbg.closed appears to be false when it throws)
    let type;
    try {
      type = this._dbg.type;
    } catch (e) {
      // nothing
    }

    if (type == Ci.nsIWorkerDebugger.TYPE_SERVICE) {
      const worker = this._getServiceWorkerInfo();
      if (worker) {
        worker.detachDebugger();
      }
    }

    this._dbg.removeListener(this);
    this._attached = false;
  },
});

exports.WorkerTargetActor = WorkerTargetActor;
