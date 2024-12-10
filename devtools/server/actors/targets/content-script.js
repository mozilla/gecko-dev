/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/*
 * Target actor for all Web Extension Content Scripts running against matching
 * web pages defined in the extension manifest.
 * They are running the same thread as the page (i.e. the main thread).
 *
 * See devtools/docs/contributor/backend/actor-hierarchy.md for more details about all the targets.
 */

const { ThreadActor } = require("resource://devtools/server/actors/thread.js");
const {
  WebConsoleActor,
} = require("resource://devtools/server/actors/webconsole.js");
const makeDebugger = require("resource://devtools/server/actors/utils/make-debugger.js");
const { assert } = require("resource://devtools/shared/DevToolsUtils.js");
const {
  SourcesManager,
} = require("resource://devtools/server/actors/utils/sources-manager.js");
const {
  contentScriptTargetSpec,
} = require("resource://devtools/shared/specs/targets/content-script.js");
const Targets = require("resource://devtools/server/actors/targets/index.js");
const Resources = require("resource://devtools/server/actors/resources/index.js");
const {
  BaseTargetActor,
} = require("resource://devtools/server/actors/targets/base-target-actor.js");

loader.lazyRequireGetter(
  this,
  "TracerActor",
  "resource://devtools/server/actors/tracer.js",
  true
);

class WebExtensionContentScriptTargetActor extends BaseTargetActor {
  constructor(conn, { sessionContext, contentScriptSandbox } = {}) {
    super(conn, Targets.TYPES.CONTENT_SCRIPT, contentScriptTargetSpec);

    this.threadActor = null;
    this.sessionContext = sessionContext;
    this.contentScriptSandbox = contentScriptSandbox;
    const metadata = Cu.getSandboxMetadata(contentScriptSandbox);
    this.addonId = metadata.addonId;

    // Use a debugger against a unique global
    this.makeDebugger = makeDebugger.bind(null, {
      findDebuggees: _dbg => [this.contentScriptSandbox],
      shouldAddNewGlobalAsDebuggee: () => true,
    });
  }

  get isRootActor() {
    return false;
  }

  // This will be used by the Console actor for evaluations
  get targetGlobal() {
    return this.contentScriptSandbox;
  }

  get sourcesManager() {
    if (!this._sourcesManager) {
      assert(
        this.threadActor,
        "threadActor should exist when creating SourcesManager."
      );
      this._sourcesManager = new SourcesManager(this.threadActor);
    }
    return this._sourcesManager;
  }

  /*
   * Return a Debugger instance or create one if there is none yet
   */
  get dbg() {
    if (!this._dbg) {
      this._dbg = this.makeDebugger();
    }
    return this._dbg;
  }

  form() {
    if (!this._consoleActor) {
      this._consoleActor = new WebConsoleActor(this.conn, this);
      this.manage(this._consoleActor);
    }

    if (!this.threadActor) {
      this.threadActor = new ThreadActor(this);
      this.manage(this.threadActor);
    }
    if (!this.tracerActor) {
      this.tracerActor = new TracerActor(this.conn, this);
      this.manage(this.tracerActor);
    }

    const policy = WebExtensionPolicy.getByID(this.addonId);

    return {
      actor: this.actorID,

      // Use the related extension as content script title
      // as content scripts have no name, they are just a group of JS files
      // running against a web page.
      title: policy.name,

      consoleActor: this._consoleActor.actorID,
      threadActor: this.threadActor.actorID,
      tracerActor: this.tracerActor.actorID,

      traits: {
        networkMonitor: false,
        // See trait description in browsing-context.js
        supportsTopLevelTargetFlag: false,
      },
    };
  }

  destroy({ isModeSwitching } = {}) {
    // Avoid reentrancy. We will destroy the Transport when emitting "destroyed",
    // which will force destroying all actors.
    if (this.destroying) {
      return;
    }
    this.destroying = true;

    // Unregistering watchers first is important
    // otherwise you might have leaks reported when running browser_browser_toolbox_netmonitor.js in debug builds
    Resources.unwatchAllResources(this);

    this.emit("destroyed", { isModeSwitching });

    super.destroy();

    if (this.threadActor) {
      this.threadActor = null;
    }

    if (this._sourcesManager) {
      this._sourcesManager.destroy();
      this._sourcesManager = null;
    }

    if (this._dbg) {
      this._dbg.disable();
      this._dbg = null;
    }
  }
}

exports.WebExtensionContentScriptTargetActor =
  WebExtensionContentScriptTargetActor;
