/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { ContentProcessWatcherRegistry } from "resource://devtools/server/connectors/js-process-actor/ContentProcessWatcherRegistry.sys.mjs";

import { addDebuggerToGlobal } from "resource://gre/modules/jsdebugger.sys.mjs";
// This will inject `Debugger` in the global scope
// eslint-disable-next-line mozilla/reject-globalThis-modification
addDebuggerToGlobal(globalThis);

const lazy = {};

const EXTENSION_CONTENT_SYS_MJS =
  "resource://gre/modules/ExtensionContent.sys.mjs";

// Do not import Targets/index.js to prevent having to load DevTools module loader.
const CONTENT_SCRIPT = "content_script";

ChromeUtils.defineESModuleGetters(
  lazy,
  {
    // ExtensionContent.sys.mjs is a singleton and must be loaded through the
    // main loader. Note that the user of lazy.ExtensionContent elsewhere in
    // this file (at webextensionsContentScriptGlobals) looks up the module
    // via Cu.isESModuleLoaded, which also uses the main loader as desired.
    ExtensionContent: EXTENSION_CONTENT_SYS_MJS,
  },
  { global: "shared" }
);

let gDbg = null;

function watch() {
  // Listen for new globals via Spidermonkey Debugger API
  // in order to catch new Content Script sandboxes created later on
  gDbg = new Debugger();
  gDbg.onNewGlobalObject = onNewGlobal;

  // Also listen to this event emitted by ExtensionContent to know
  // when some Content Script sandboxes are destroyed.
  // It happens when a page navigates/reload and also on add-on disabling.
  Services.obs.addObserver(observe, "content-script-destroyed");
}

function unwatch() {
  gDbg.onNewGlobalObject = undefined;
  gDbg = null;

  Services.obs.removeObserver(observe, "content-script-destroyed");
}

/**
 * Called whenever a new global is instantiated in the current process
 *
 * @param {Debugger.Object} global
 */
function onNewGlobal(global) {
  // Content scripts are only using Sandboxes as global.
  if (global.class != "Sandbox") {
    return;
  }

  // WebExtension codebase will flag its sandboxes with a browser-id attribute
  // in order to inform which tab they are injected against.
  const contentScriptSandbox = global.unsafeDereference();
  const metadata = Cu.getSandboxMetadata(contentScriptSandbox);
  if (!metadata) {
    return;
  }
  const sandboxBrowserId = metadata["browser-id"];
  if (!sandboxBrowserId) {
    return;
  }

  for (const watcherDataObject of ContentProcessWatcherRegistry.getAllWatchersDataObjects(
    CONTENT_SCRIPT
  )) {
    const { sessionContext } = watcherDataObject;
    const { browserId } = sessionContext;
    if (browserId != sandboxBrowserId) {
      continue;
    }
    createContentScriptTargetActor(watcherDataObject, {
      sessionContext,
      contentScriptSandbox,
    });
  }
}

/**
 * Listen to an Observer Service notification emitted by ExtensionContent
 * when any content script sandbox is destroyed.
 */
function observe(sandbox, topic) {
  if (topic != "content-script-destroyed") {
    return;
  }

  for (const watcherDataObject of ContentProcessWatcherRegistry.getAllWatchersDataObjects(
    CONTENT_SCRIPT
  )) {
    const targetActor = watcherDataObject.actors.find(
      actor => actor.contentScriptSandbox === sandbox
    );
    if (!targetActor) {
      continue;
    }

    ContentProcessWatcherRegistry.destroyTargetActor(
      watcherDataObject,
      targetActor,
      {}
    );
  }
}

function createTargetsForWatcher(watcherDataObject, _isProcessActorStartup) {
  // Ignore this process if there is no extension activity in it
  if (!Cu.isESModuleLoaded(EXTENSION_CONTENT_SYS_MJS)) {
    return;
  }

  const { sessionContext } = watcherDataObject;
  // Only try spawning Content Script targets when debugging tabs.
  if (sessionContext.type != "browser-element") {
    return;
  }
  const { browserId } = sessionContext;

  const sandboxes = lazy.ExtensionContent.getAllContentScriptGlobals();
  for (const contentScriptSandbox of sandboxes) {
    const metadata = Cu.getSandboxMetadata(contentScriptSandbox);
    if (metadata["browser-id"] != browserId) {
      continue;
    }
    createContentScriptTargetActor(watcherDataObject, {
      sessionContext,
      contentScriptSandbox,
    });
  }
}

function destroyTargetsForWatcher(watcherDataObject, options) {
  // Unregister and destroy the existing target actors for this target type
  const actorsToDestroy = watcherDataObject.actors.filter(
    actor => actor.targetType == CONTENT_SCRIPT
  );
  watcherDataObject.actors = watcherDataObject.actors.filter(
    actor => actor.targetType != CONTENT_SCRIPT
  );

  for (const actor of actorsToDestroy) {
    ContentProcessWatcherRegistry.destroyTargetActor(
      watcherDataObject,
      actor,
      options
    );
  }
}

/**
 * Instantiate a ContentScript target actor for a given content script sandbox
 * and a given watcher actor.
 *
 * @param {Object} watcherDataObject
 * @param {Sandbox} sandbox
 */
function createContentScriptTargetActor(watcherDataObject, sandbox) {
  const { connection, loader } =
    ContentProcessWatcherRegistry.getOrCreateConnectionForWatcher(
      watcherDataObject.watcherActorID,
      false
    );

  const { WebExtensionContentScriptTargetActor } = loader.require(
    "devtools/server/actors/targets/content-script"
  );

  // Create the actual target actor.
  const targetActor = new WebExtensionContentScriptTargetActor(
    connection,
    sandbox
  );

  ContentProcessWatcherRegistry.onNewTargetActor(
    watcherDataObject,
    targetActor,
    false
  );
}

export const ContentScriptTargetWatcher = {
  watch,
  unwatch,
  createTargetsForWatcher,
  destroyTargetsForWatcher,
};
