/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { setTimeout, clearTimeout } from "resource://gre/modules/Timer.sys.mjs";
import { loader } from "resource://devtools/shared/loader/Loader.sys.mjs";
import { EventEmitter } from "resource://gre/modules/EventEmitter.sys.mjs";

const { ParentProcessWatcherRegistry } = ChromeUtils.importESModule(
  "resource://devtools/server/actors/watcher/ParentProcessWatcherRegistry.sys.mjs",
  // ParentProcessWatcherRegistry needs to be a true singleton and loads ActorManagerParent
  // which also has to be a true singleton.
  { global: "shared" }
);

const lazy = {};
loader.lazyRequireGetter(
  lazy,
  "JsWindowActorTransport",
  "devtools/shared/transport/js-window-actor-transport",
  true
);

export class DevToolsProcessParent extends JSProcessActorParent {
  constructor() {
    super();

    EventEmitter.decorate(this);
  }

  // Boolean to indicate if the related content process is slow to respond
  // and may be hanging or frozen. When true, we should avoid waiting for its replies.
  #frozen = false;

  #destroyed = false;

  // Map of various data specific to each active Watcher Actor.
  // A Watcher will become "active" once we receive a first target actor
  // notified by the content process, which happens only once
  // the watcher starts watching for some target types.
  //
  // This Map is keyed by "watcher connection prefix".
  // This is a unique prefix, per watcher actor, which will
  // be used in the "forwardingPrefix" documented below.
  //
  // Note that We may have multiple Watcher actors,
  // which will resuse the same DevToolsServerConnection (watcher.conn)
  // if they are instantiated from the same client connection.
  // Or be using distinct DevToolsServerConnection, if they
  // are instantiated via distinct client connections.
  //
  // The Map's values are objects containing the following properties:
  // - watcher:
  //     The watcher actor instance.
  // - targetActorForms:
  //     The list of all active target actor forms
  //     related to this watcher actor.
  // - transport:
  //     The JsWindowActorTransport which will receive and emit
  //     the RDP packets from the current JS Process Actor's content process.
  //     We spawn one transpart per Watcher and Content process pair.
  // - forwardingPrefix:
  //     The forwarding prefix is specific to the transport.
  //     It helps redirect RDP packets between this "transport" and
  //     the DevToolsServerConnection (watcher.conn), in the parent process,
  //     which receives and emits RDP packet from/to the client.

  #watchers = new Map();

  /**
   * Request the content process to create all the targets currently watched
   * and start observing for new ones to be created later.
   */
  watchTargets({ watcherActorID, targetType }) {
    return this.sendQuery("DevToolsProcessParent:watchTargets", {
      watcherActorID,
      targetType,
    });
  }

  /**
   * Request the content process to stop observing for currently watched targets
   * and destroy all the currently active ones.
   */
  unwatchTargets({ watcherActorID, targetType, options }) {
    this.sendAsyncMessage("DevToolsProcessParent:unwatchTargets", {
      watcherActorID,
      targetType,
      options,
    });
  }

  /**
   * Communicate to the content process that some data have been added or set.
   */
  addOrSetSessionDataEntry({ watcherActorID, type, entries, updateType }) {
    return this.sendQuery("DevToolsProcessParent:addOrSetSessionDataEntry", {
      watcherActorID,
      type,
      entries,
      updateType,
    });
  }

  /**
   * Communicate to the content process that some data have been removed.
   */
  removeSessionDataEntry({ watcherActorID, type, entries }) {
    this.sendAsyncMessage("DevToolsProcessParent:removeSessionDataEntry", {
      watcherActorID,
      type,
      entries,
    });
  }

  destroyWatcher({ watcherActorID }) {
    return this.sendAsyncMessage("DevToolsProcessParent:destroyWatcher", {
      watcherActorID,
    });
  }

  /**
   * Called when the content process notified us about a new target actor
   */
  #onTargetAvailable({ watcherActorID, forwardingPrefix, targetActorForm }) {
    const watcher = ParentProcessWatcherRegistry.getWatcher(watcherActorID);

    if (!watcher) {
      throw new Error(
        `Watcher Actor with ID '${watcherActorID}' can't be found.`
      );
    }
    const connection = watcher.conn;

    // If this is the first target actor for this watcher,
    // hook up the DevToolsServerConnection which will bridge
    // communication between the parent process DevToolsServer
    // and the content process.
    if (!this.#watchers.get(watcher.watcherConnectionPrefix)) {
      connection.on("closed", this.#onConnectionClosed);

      // Create a js-window-actor based transport.
      const transport = new lazy.JsWindowActorTransport(
        this,
        forwardingPrefix,
        "DevToolsProcessParent:packet"
      );
      transport.hooks = {
        onPacket: connection.send.bind(connection),
        onClosed() {},
      };
      transport.ready();

      connection.setForwarding(forwardingPrefix, transport);

      this.#watchers.set(watcher.watcherConnectionPrefix, {
        watcher,
        // This prefix is the prefix of the DevToolsServerConnection, running
        // in the content process, for which we should forward packets to, based on its prefix.
        // While `watcher.connection` is also a DevToolsServerConnection, but from this process,
        // the parent process. It is the one receiving Client packets and the one, from which
        // we should forward packets from.
        forwardingPrefix,
        transport,
        targetActorForms: [],
      });
    }

    this.#watchers
      .get(watcher.watcherConnectionPrefix)
      .targetActorForms.push(targetActorForm);

    watcher.notifyTargetAvailable(targetActorForm);
  }

  /**
   * Called when the content process notified us about a target actor that has been destroyed.
   */
  #onTargetDestroyed({ actors, options }) {
    for (const { watcherActorID, targetActorForm } of actors) {
      const watcher = ParentProcessWatcherRegistry.getWatcher(watcherActorID);
      // As we instruct to destroy all targets when the watcher is destroyed,
      // we may easily receive the target destruction notification *after*
      // the watcher has been removed from the registry.
      if (!watcher || watcher.isDestroyed()) {
        continue;
      }
      watcher.notifyTargetDestroyed(targetActorForm, options);
      const watcherInfo = this.#watchers.get(watcher.watcherConnectionPrefix);
      if (watcherInfo) {
        const idx = watcherInfo.targetActorForms.findIndex(
          form => form.actor == targetActorForm.actor
        );
        if (idx != -1) {
          watcherInfo.targetActorForms.splice(idx, 1);
        }
        // Once the last active target is removed, disconnect the DevTools transport
        // and cleanup everything bound to this DOM Process. We will re-instantiate
        // a new connection/transport on the next reported target actor.
        if (!watcherInfo.targetActorForms.length) {
          this.#unregisterWatcher(watcherInfo.watcher, options);
        }
      }
    }
  }

  #onConnectionClosed = (status, prefix) => {
    for (const watcherInfo of this.#watchers.values()) {
      if (watcherInfo.watcher.conn.prefix == prefix) {
        this.#unregisterWatcher(watcherInfo.watcher);
      }
    }
  };

  /**
   * Unregister a given watcher.
   * This will also close and unregister the related given DevToolsServerConnection,
   * if no other watcher is active on the same, possibly shared, connection.
   * (when remote debugging many tabs on the same connection)
   *
   * @param {WatcherActor} watcher
   * @param {object} options
   * @param {boolean} options.isModeSwitching
   *        true when this is called as the result of a change to the devtools.browsertoolbox.scope pref
   */
  #unregisterWatcher(watcher, options = {}) {
    const watcherInfo = this.#watchers.get(watcher.watcherConnectionPrefix);
    if (!watcherInfo) {
      return;
    }
    this.#watchers.delete(watcher.watcherConnectionPrefix);

    for (const actor of watcherInfo.targetActorForms) {
      watcherInfo.watcher.notifyTargetDestroyed(actor, options);
    }

    let connectionUsedByAnotherWatcher = false;
    for (const info of this.#watchers.values()) {
      if (info.watcher.conn == watcherInfo.watcher.conn) {
        connectionUsedByAnotherWatcher = true;
        break;
      }
    }

    if (!connectionUsedByAnotherWatcher) {
      const { forwardingPrefix, transport } = watcherInfo;
      if (transport) {
        // If we have a child transport, the actor has already
        // been created. We need to stop using this transport.
        transport.close(options);
      }
      // When cancelling the forwarding, one RDP event is sent to the client to purge all requests
      // and actors related to a given prefix.
      // Be careful that any late RDP event would be ignored by the client passed this call.
      watcherInfo.watcher.conn.cancelForwarding(forwardingPrefix);
    }

    if (!this.#watchers.size) {
      this.#destroy(options);
    }
  }

  /**
   * Destroy and cleanup everything for this DOM Process.
   *
   * @param {object} options
   * @param {boolean} options.isModeSwitching
   *        true when this is called as the result of a change to the devtools.browsertoolbox.scope pref
   */
  #destroy(options) {
    if (this.#destroyed) {
      return;
    }
    this.#destroyed = true;

    for (const watcherInfo of this.#watchers.values()) {
      this.#unregisterWatcher(watcherInfo.watcher, options);
    }
  }

  /**
   * Used by DevTools Transport to send packets to the content process.
   */

  sendPacket(packet, prefix) {
    this.sendAsyncMessage("DevToolsProcessParent:packet", { packet, prefix });
  }

  /**
   * JsProcessActor API
   */

  /**
   * JS Actor override of `sendQuery` method, whose main goal is the ignore possibly freezing processes.
   * This also prints a warning when the query failed to be sent, or when a process hangs.
   *
   * @param String msg
   * @param Array<json> args
   * @return Promise<undefined>
   *   We only use sendQuery for two queries ("watchTargets" and "addOrSetSessionDataEntry") and
   *   none of them use any returned value (except a promise to know when their processing is done).
   */
  async sendQuery(msg, args) {
    // If any preview query timed out and did not reply yet, the process is considered frozen
    // and are no longer waiting for the process response.
    if (this.#frozen) {
      this.sendAsyncMessage(msg, args);
      return Promise.resolve();
    }

    // Cache `osPid` and avoid querying `this.manager` attribute later as it may result into
    // a `AssertReturnTypeMatchesJitinfo` assertion crash into GenericGetter .
    const { osPid } = this.manager;

    return new Promise((resolve, reject) => {
      // The process may be slow to resolve the query, or even be completely frozen.
      // Use a timeout to detect when it happens.
      const timeout = setTimeout(() => {
        this.#frozen = true;
        console.error(
          `Content process ${osPid} isn't responsive while sending "${msg}" request. DevTools will ignore this process for now.`
        );
        // Do not consider timeout as an error as it may easily break the frontend.
        resolve();
      }, 1000);

      super.sendQuery(msg, args).then(
        () => {
          if (this.#frozen && !this.#destroyed) {
            console.error(
              `Content process ${osPid} is responsive again. DevTools resumes operations against it.`
            );
          }
          clearTimeout(timeout);
          // If any of the ongoing query resolved, consider the process as responsive again
          this.#frozen = false;

          resolve();
        },
        async e => {
          // Ignore frozen processes when the JS Process Actor is destroyed.
          // Either the process was shut down or DevTools unregistered the Actor.
          if (this.#frozen && !this.#destroyed) {
            console.error(
              `Content process ${osPid} is responsive again. DevTools resumes operations against it.`
            );
          }
          clearTimeout(timeout);
          // If any of the ongoing query resolved, consider the process as responsive again
          this.#frozen = false;

          console.error("Failed to sendQuery in DevToolsProcessParent", msg);
          console.error(e.toString());
          reject(e);
        }
      );
    });
  }

  /**
   * Called by the JSProcessActor API when the content process sent us a message
   */
  receiveMessage(message) {
    switch (message.name) {
      case "DevToolsProcessChild:targetAvailable":
        return this.#onTargetAvailable(message.data);
      case "DevToolsProcessChild:packet":
        return this.emit("packet-received", message);
      case "DevToolsProcessChild:targetDestroyed":
        return this.#onTargetDestroyed(message.data);
      case "DevToolsProcessChild:bf-cache-navigation-pageshow": {
        const browsingContext = BrowsingContext.get(
          message.data.browsingContextId
        );
        for (const watcherActor of ParentProcessWatcherRegistry.getWatchersForBrowserId(
          browsingContext.browserId
        )) {
          watcherActor.emit("bf-cache-navigation-pageshow", {
            windowGlobal: browsingContext.currentWindowGlobal,
          });
        }
        return null;
      }
      case "DevToolsProcessChild:bf-cache-navigation-pagehide": {
        const browsingContext = BrowsingContext.get(
          message.data.browsingContextId
        );
        for (const watcherActor of ParentProcessWatcherRegistry.getWatchersForBrowserId(
          browsingContext.browserId
        )) {
          watcherActor.emit("bf-cache-navigation-pagehide", {
            windowGlobal: browsingContext.currentWindowGlobal,
          });
        }
        return null;
      }
      default:
        throw new Error(
          "Unsupported message in DevToolsProcessParent: " + message.name
        );
    }
  }

  /**
   * Called by the JSProcessActor API when this content process is destroyed.
   */
  didDestroy() {
    this.#destroy();
  }
}

export class BrowserToolboxDevToolsProcessParent extends DevToolsProcessParent {}
