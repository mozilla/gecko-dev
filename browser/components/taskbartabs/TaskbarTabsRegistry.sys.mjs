/* vim: se cin sw=2 ts=2 et filetype=javascript :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const kStorageVersion = 1;

let lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  AsyncShutdown: "resource://gre/modules/AsyncShutdown.sys.mjs",
  EventEmitter: "resource://gre/modules/EventEmitter.sys.mjs",
  JsonSchema: "resource://gre/modules/JsonSchema.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "logConsole", () => {
  return console.createInstance({
    prefix: "TaskbarTabs",
    maxLogLevel: "All",
  });
});

/**
 * Returns a JSON schema validator for Taskbar Tabs persistent storage.
 *
 * @returns {Promise<Validator>} Resolves to JSON schema validator for Taskbar Tab's persistent storage.
 */
async function getJsonSchema() {
  const kJsonSchema =
    "chrome://browser/content/taskbartabs/TaskbarTabs.1.schema.json";
  let res = await fetch(kJsonSchema);
  let obj = await res.json();
  return new lazy.JsonSchema.Validator(obj);
}

/**
 * Storage class for a single Taskbar Tab's persistent storage.
 */
class TaskbarTab {
  // Unique identifier for the Taskbar Tab.
  #id;
  // List of hosts associated with this Taskbar tab.
  #scopes = [];
  // Container the Taskbar Tab is opened in when opened from the Taskbar.
  #userContextId;
  // URL opened when a Taskbar Tab is opened from the Taskbar.
  #startUrl;

  constructor({ id, scopes, startUrl, userContextId }) {
    this.#id = id;
    this.#scopes = scopes;
    this.#userContextId = userContextId;
    this.#startUrl = startUrl;
  }

  get id() {
    return this.#id;
  }

  get scopes() {
    return [...this.#scopes];
  }

  get userContextId() {
    return this.#userContextId;
  }

  get startUrl() {
    return this.#startUrl;
  }

  /**
   * Whether the provided URL is navigable from the Taskbar Tab.
   *
   * @param {nsIURL} aUrl - The URL to navigate to.
   * @returns {boolean} `true` if the URL is navigable from the Taskbar Tab associated to the ID.
   * @throws {Error} If `aId` is not a valid Taskbar Tabs ID.
   */
  isScopeNavigable(aUrl) {
    let baseDomain = Services.eTLD.getBaseDomain(aUrl);

    for (const scope of this.#scopes) {
      let scopeBaseDomain = Services.eTLD.getBaseDomainFromHost(scope.hostname);

      // Domains in the same base domain are valid navigation targets.
      if (baseDomain === scopeBaseDomain) {
        lazy.logConsole.info(`${aUrl} is navigable for scope ${scope}.`);
        return true;
      }
    }

    lazy.logConsole.info(
      `${aUrl} is not navigable for Taskbar Tab ID ${this.#id}.`
    );
    return false;
  }

  toJSON() {
    return {
      id: this.id,
      scopes: this.scopes,
      userContextId: this.userContextId,
      startUrl: this.startUrl,
    };
  }
}

export const kTaskbarTabsRegistryEvents = Object.freeze({
  created: "created",
  removed: "removed",
});

/**
 * Storage class for Taskbar Tabs feature's persistent storage.
 */
export class TaskbarTabsRegistry {
  // List of registered Taskbar Tabs.
  #taskbarTabs = [];
  // Signals when Taskbar Tabs have been created or removed.
  #emitter = new lazy.EventEmitter();

  /**
   * Initializes a Taskbar Tabs Registry, optionally loading from a file.
   *
   * @param {object} [init] - Initialization context.
   * @param {nsIFile} [init.loadFile] - Optional file to load.
   */
  static async create({ loadFile } = {}) {
    let registry = new TaskbarTabsRegistry();
    if (loadFile) {
      await registry.#load(loadFile);
    }

    return registry;
  }

  /**
   * Loads the stored Taskbar Tabs.
   *
   * @param {nsIFile} aFile - File to load from.
   */
  async #load(aFile) {
    if (!aFile.exists()) {
      lazy.logConsole.error(`File ${aFile.path} does not exist.`);
      return;
    }

    lazy.logConsole.info(`Loading file ${aFile.path} for Taskbar Tabs.`);

    const [schema, jsonObject] = await Promise.all([
      getJsonSchema(),
      IOUtils.readJSON(aFile.path),
    ]);

    if (!schema.validate(jsonObject).valid) {
      throw new Error(
        `JSON from file ${aFile.path} is invalid for the Taskbar Tabs Schema.`
      );
    }
    if (jsonObject.version > kStorageVersion) {
      throw new Error(`File ${aFile.path} has an unrecognized version.
          Current Version: ${kStorageVersion}
          File Version: ${jsonObject.version}`);
    }
    this.#taskbarTabs = jsonObject.taskbarTabs.map(tt => new TaskbarTab(tt));
  }

  toJSON() {
    return {
      version: kStorageVersion,
      taskbarTabs: this.#taskbarTabs.map(tt => {
        return tt.toJSON();
      }),
    };
  }

  /**
   * Finds or creates a Taskbar Tab based on the provided URL and container.
   *
   * @param {nsIURL} aUrl - The URL to match or derive the scope and start URL from.
   * @param {number} aUserContextId - The container to start a Taskbar Tab in.
   * @returns {TaskbarTab} The matching or created Taskbar Tab.
   */
  findOrCreateTaskbarTab(aUrl, aUserContextId) {
    let taskbarTab = this.findTaskbarTab(aUrl, aUserContextId);
    if (taskbarTab) {
      return taskbarTab;
    }

    let id = Services.uuid.generateUUID().toString().slice(1, -1);
    taskbarTab = new TaskbarTab({
      id,
      scopes: [{ hostname: aUrl.host }],
      userContextId: aUserContextId,
      startUrl: aUrl.prePath,
    });
    this.#taskbarTabs.push(taskbarTab);

    lazy.logConsole.info(`Created Taskbar Tab with ID ${id}`);

    this.#emitter.emit(kTaskbarTabsRegistryEvents.created, taskbarTab);

    return taskbarTab;
  }

  /**
   * Removes a Taskbar Tab.
   *
   * @param {string} aId - The ID of the TaskbarTab to remove.
   */
  removeTaskbarTab(aId) {
    let tts = this.#taskbarTabs;
    const i = tts.findIndex(tt => {
      return tt.id === aId;
    });

    if (i > -1) {
      lazy.logConsole.info(`Removing Taskbar Tab Id ${tts[i].id}`);
      let removed = tts.splice(i, 1);

      this.#emitter.emit(kTaskbarTabsRegistryEvents.removed, removed[0]);
    } else {
      lazy.logConsole.error(`Taskbar Tab ID ${aId} not found.`);
    }
  }

  /**
   * Searches for an existing Taskbar Tab matching the URL and Container.
   *
   * @param {nsIURL} aUrl - The URL to match.
   * @param {number} aUserContextId - The container to match.
   * @returns {TaskbarTab|null} The matching Taskbar Tab, or null if none match.
   */
  findTaskbarTab(aUrl, aUserContextId) {
    for (const tt of this.#taskbarTabs) {
      for (const scope of tt.scopes) {
        if (aUrl.host === scope.hostname) {
          if (aUserContextId !== tt.userContextId) {
            lazy.logConsole.info(
              `Matched TaskbarTab for URL ${aUrl.host} to ${scope.hostname}, but container ${aUserContextId} mismatched ${tt.userContextId}.`
            );
          } else {
            lazy.logConsole.info(
              `Matched TaskbarTab for URL ${aUrl.host} to ${scope.hostname} with container ${aUserContextId}.`
            );
            return tt;
          }
        }
      }
    }

    lazy.logConsole.info(
      `No matching TaskbarTab found for URL ${aUrl.host} and container ${aUserContextId}.`
    );
    return null;
  }

  /**
   * Retrieves the Taskbar Tab matching the ID.
   *
   * @param {string} aId - The ID of the Taskbar Tab.
   * @returns {TaskbarTab} The matching Taskbar Tab.
   * @throws {Error} If `aId` is not a valid Taskbar Tab ID.
   */
  getTaskbarTab(aId) {
    const tt = this.#taskbarTabs.find(aTaskbarTab => {
      return aTaskbarTab.id === aId;
    });
    if (!tt) {
      lazy.logConsole.error(`Taskbar Tab Id ${aId} not found.`);
      throw new Error(`Taskbar Tab Id ${aId} is invalid.`);
    }

    return tt;
  }

  /**
   * Passthrough to `EventEmitter.on`.
   *
   * @param  {...any} args - Same as `EventEmitter.on`.
   */
  on(...args) {
    return this.#emitter.on(...args);
  }

  /**
   * Passthrough to `EventEmitter.off`
   *
   * @param  {...any} args - Same as `EventEmitter.off`
   */
  off(...args) {
    return this.#emitter.off(...args);
  }
}

/**
 * Monitor for the Taskbar Tabs Registry that updates the save file as it
 * changes.
 *
 * Note: this intentionally does not save on schema updates to allow for
 * gracefall rollback to an earlier version of Firefox where possible. This is
 * desirable in cases where a user has unintentioally opened a profile on a
 * newer version of Firefox, or has reverted an update.
 */
export class TaskbarTabsRegistryStorage {
  // The registry to save.
  #registry;
  // The file saved to.
  #saveFile;
  // Promise queue to ensure that async writes don't occur out of order.
  #saveQueue = Promise.resolve();

  /**
   * @param {TaskbarTabsRegistry} aRegistry - The registry to serialize.
   * @param {nsIFile} aSaveFile - The save file to update.
   */
  constructor(aRegistry, aSaveFile) {
    this.#registry = aRegistry;
    this.#saveFile = aSaveFile;
  }

  /**
   * Serializes the Taskbar Tabs Registry into a JSON file.
   *
   * Note: file writes are strictly ordered, ensuring the sequence of serialized
   * object writes reflects the latest state even if any individual write
   * serializes the registry in a newer state than when it's associated event
   * was emitted.
   *
   * @returns {Promise} Resolves once the current save operation completes.
   */
  save() {
    this.#saveQueue = this.#saveQueue
      .finally(async () => {
        lazy.logConsole.info(`Updating Taskbar Tabs storage file.`);

        const schema = await getJsonSchema();

        // Copy the JSON object to prevent awaits after validation risking
        // TOCTOU if the registry changes..
        let json = this.#registry.toJSON();

        let result = schema.validate(json);
        if (!result.valid) {
          throw new Error(
            "Generated invalid JSON for the Taskbar Tabs Schema:\n" +
              JSON.stringify(result.errors)
          );
        }

        await IOUtils.makeDirectory(this.#saveFile.parent.path);
        await IOUtils.writeJSON(this.#saveFile.path, json);

        lazy.logConsole.info(`Tasbkar Tabs storage file updated.`);
      })
      .catch(e => {
        lazy.logConsole.error(`Error writing Taskbar Tabs file: ${e}`);
      });

    lazy.AsyncShutdown.profileBeforeChange.addBlocker(
      "Taskbar Tabs: finalizing registry serialization to disk.",
      this.#saveQueue
    );

    return this.#saveQueue;
  }
}
