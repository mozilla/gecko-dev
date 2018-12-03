/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {Ci} = require("chrome");
const {rootSpec} = require("devtools/shared/specs/root");
const protocol = require("devtools/shared/protocol");
const {custom} = protocol;

loader.lazyRequireGetter(this, "getFront", "devtools/shared/protocol", true);
loader.lazyRequireGetter(this, "BrowsingContextTargetFront", "devtools/shared/fronts/targets/browsing-context", true);
loader.lazyRequireGetter(this, "ContentProcessTargetFront", "devtools/shared/fronts/targets/content-process", true);

const RootFront = protocol.FrontClassWithSpec(rootSpec, {
  initialize: function(client, form) {
    protocol.Front.prototype.initialize.call(this, client, { actor: form.from });

    this.applicationType = form.applicationType;
    this.traits = form.traits;

    // Cache root form as this will always be the same value.
    Object.defineProperty(this, "rootForm", {
      get() {
        delete this.rootForm;
        this.rootForm = this.getRoot();
        return this.rootForm;
      },
      configurable: true,
    });

    // Cache of already created global scoped fronts
    // [typeName:string => Front instance]
    this.fronts = new Map();

    this._client = client;
  },

  /**
   * Retrieve all service worker registrations as well as workers from the parent and
   * content processes. Listing service workers involves merging information coming from
   * registrations and workers, this method will combine this information to present a
   * unified array of serviceWorkers. If you are only interested in other workers, use
   * listWorkers.
   *
   * @return {Object}
   *         - {Array} service
   *           array of form-like objects for serviceworkers
   *         - {Array} shared
   *           Array of WorkerTargetActor forms, containing shared workers.
   *         - {Array} other
   *           Array of WorkerTargetActor forms, containing other workers.
   */
  listAllWorkers: async function() {
    let registrations = [];
    let workers = [];

    try {
      // List service worker registrations
      ({ registrations } = await this.listServiceWorkerRegistrations());

      // List workers from the Parent process
      ({ workers } = await this.listWorkers());

      // And then from the Child processes
      const { processes } = await this.listProcesses();
      for (const process of processes) {
        // Ignore parent process
        if (process.parent) {
          continue;
        }
        const front = await this.getProcess(process.id);
        const response = await front.listWorkers();
        workers = workers.concat(response.workers);
      }
    } catch (e) {
      // Something went wrong, maybe our client is disconnected?
    }

    const result = {
      service: [],
      shared: [],
      other: [],
    };

    registrations.forEach(form => {
      result.service.push({
        name: form.url,
        url: form.url,
        scope: form.scope,
        fetch: form.fetch,
        registrationActor: form.actor,
        active: form.active,
        lastUpdateTime: form.lastUpdateTime,
      });
    });

    workers.forEach(front => {
      const worker = {
        name: front.url,
        url: front.url,
        workerTargetFront: front,
      };
      switch (front.type) {
        case Ci.nsIWorkerDebugger.TYPE_SERVICE:
          const registration = result.service.find(r => r.scope === front.scope);
          if (registration) {
            // XXX: Race, sometimes a ServiceWorkerRegistrationInfo doesn't
            // have a scriptSpec, but its associated WorkerDebugger does.
            if (!registration.url) {
              registration.name = registration.url = front.url;
            }
            registration.workerTargetFront = front;
          } else {
            worker.fetch = front.fetch;

            // If a service worker registration could not be found, this means we are in
            // e10s, and registrations are not forwarded to other processes until they
            // reach the activated state. Augment the worker as a registration worker to
            // display it in aboutdebugging.
            worker.scope = front.scope;
            worker.active = false;
            result.service.push(worker);
          }
          break;
        case Ci.nsIWorkerDebugger.TYPE_SHARED:
          result.shared.push(worker);
          break;
        default:
          result.other.push(worker);
      }
    });

    return result;
  },

  /**
   * Fetch the ParentProcessTargetActor for the main process.
   *
   * `getProcess` requests allows to fetch the target actor for any process
   * and the main process is having the process ID zero.
   */
  getMainProcess() {
    return this.getProcess(0);
  },

  getProcess: custom(async function(id) {
    // Do not use specification automatic marshalling as getProcess may return
    // two different type: ParentProcessTargetActor or ContentProcessTargetActor.
    // Also, we do want to memoize the fronts and return already existing ones.
    const { form } = await this._getProcess(id);
    let front = this.actor(form.actor);
    if (front) {
      return front;
    }
    // getProcess may return a ContentProcessTargetActor or a ParentProcessTargetActor
    // In most cases getProcess(0) will return the main process target actor,
    // which is a ParentProcessTargetActor, but not in xpcshell, which uses a
    // ContentProcessTargetActor. So select the right front based on the actor ID.
    if (form.actor.includes("contentProcessTarget")) {
      front = new ContentProcessTargetFront(this._client, form);
    } else {
      // ParentProcessTargetActor doesn't have a specific front, instead it uses
      // BrowsingContextTargetFront on the client side.
      front = new BrowsingContextTargetFront(this._client, form);
    }
    this.manage(front);

    return front;
  }, {
    impl: "_getProcess",
  }),

  /**
   * Fetch the target actor for the currently selected tab, or for a specific
   * tab given as first parameter.
   *
   * @param [optional] object filter
   *        A dictionary object with following optional attributes:
   *         - outerWindowID: used to match tabs in parent process
   *         - tabId: used to match tabs in child processes
   *         - tab: a reference to xul:tab element
   *        If nothing is specified, returns the actor for the currently
   *        selected tab.
   */
  getTab: custom(async function(filter) {
    const packet = {};
    if (filter) {
      if (typeof (filter.outerWindowID) == "number") {
        packet.outerWindowID = filter.outerWindowID;
      } else if (typeof (filter.tabId) == "number") {
        packet.tabId = filter.tabId;
      } else if ("tab" in filter) {
        const browser = filter.tab.linkedBrowser;
        if (browser.frameLoader.tabParent) {
          // Tabs in child process
          packet.tabId = browser.frameLoader.tabParent.tabId;
        } else if (browser.outerWindowID) {
          // <xul:browser> tabs in parent process
          packet.outerWindowID = browser.outerWindowID;
        } else {
          // <iframe mozbrowser> tabs in parent process
          const windowUtils = browser.contentWindow.windowUtils;
          packet.outerWindowID = windowUtils.outerWindowID;
        }
      } else {
        // Throw if a filter object have been passed but without
        // any clearly idenfified filter.
        throw new Error("Unsupported argument given to getTab request");
      }
    }

    return this._getTab(packet);
  }, {
    impl: "_getTab",
  }),

  /**
   * Fetch the target front for a given add-on.
   * This is just an helper on top of `listAddons` request.
   *
   * @param object filter
   *        A dictionary object with following attribute:
   *         - id: used to match the add-on to connect to.
   */
  async getAddon({ id }) {
    const addons = await this.listAddons();
    const addonTargetFront = addons.find(addon => addon.id === id);
    return addonTargetFront;
  },

  /**
   * Test request that returns the object passed as first argument.
   *
   * `echo` is special as all the property of the given object have to be passed
   * on the packet object. That's not something that can be achieve by requester helper.
   */

  echo(packet) {
    packet.type = "echo";
    return this.request(packet);
  },

  /*
   * This function returns a protocol.js Front for any root actor.
   * i.e. the one directly served from RootActor.listTabs or getRoot.
   *
   * @param String typeName
   *        The type name used in protocol.js's spec for this actor.
   */
  async getFront(typeName) {
    let front = this.fronts.get(typeName);
    if (front) {
      return front;
    }
    const rootForm = await this.rootForm;
    front = getFront(this._client, typeName, rootForm);
    this.fronts.set(typeName, front);
    return front;
  },
});
exports.RootFront = RootFront;
