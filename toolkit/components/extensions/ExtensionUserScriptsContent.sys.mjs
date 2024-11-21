/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This file handles logic related to user script code execution in content.
 * It complements ExtensionContent.sys.mjs and is a separate file because this
 * module only needs to be loaded when an extension runs user scripts.
 */

import { ExtensionUtils } from "resource://gre/modules/ExtensionUtils.sys.mjs";

const { DefaultMap, DefaultWeakMap } = ExtensionUtils;

class WorldConfigHolder {
  /** @type {Map<ExtensionChild,WorldConfigHolder>} */
  static allMaps = new DefaultWeakMap(ext => new WorldConfigHolder(ext));

  constructor(extension) {
    this.defaultCSP = extension.policy.baseCSP;
    this.configs = new Map(extension.getSharedData("userScriptsWorldConfigs"));
  }

  configureWorld(properties) {
    this.configs.set(properties.worldId, properties);
  }

  resetWorldConfiguration(worldId) {
    this.configs.delete(worldId);
  }

  getCSPForWorldId(worldId) {
    return (
      this.configs.get(worldId)?.csp ??
      this.configs.get("")?.csp ??
      this.defaultCSP
    );
  }
}

class WorldCollection {
  /** @type {Map<ContentScriptContextChild,WorldCollection>} */
  static allByContext = new DefaultMap(context => new WorldCollection(context));

  /** @type {Map<string,Sandbox>} */
  sandboxes = new DefaultMap(worldId => this.newSandbox(worldId));

  /**
   * Retrieve a Sandbox for the given context and worldId. May throw if the
   * context has unloaded.
   *
   * @param {ContentScriptContextChild} context
   *        Context that wraps the document where the scripts should execute.
   *        The context keeps track of the document & extension lifetime.
   * @param {string} worldId
   *        The identifier of the userScript world.
   * @returns {Sandbox}
   */
  static sandboxFor(context, worldId) {
    return WorldCollection.allByContext.get(context).sandboxes.get(worldId);
  }

  constructor(context) {
    if (context.unloaded) {
      throw new Error("Cannot create user script world after context unloaded");
    }
    this.context = context;
    this.configHolder = WorldConfigHolder.allMaps.get(context.extension);
    context.callOnClose(this);
  }

  close() {
    WorldCollection.allByContext.delete(this.context);
    for (let sandbox of this.sandboxes.values()) {
      Cu.nukeSandbox(sandbox);
    }
    this.sandboxes.clear();
  }

  newSandbox(worldId) {
    let contentWindow = this.context.contentWindow;
    let docPrincipal = contentWindow.document.nodePrincipal;
    let policy = this.context.extension.policy;

    if (docPrincipal.isSystemPrincipal) {
      throw new Error("User scripts are not supported in system principals");
    }

    let sandbox = Cu.Sandbox([docPrincipal], {
      metadata: {
        "inner-window-id": this.context.innerWindowID,
        addonId: policy.id,
      },
      sandboxName: `User script world ${worldId} for ${policy.debugName}`,
      sandboxPrototype: contentWindow,
      sandboxContentSecurityPolicy: this.configHolder.getCSPForWorldId(worldId),
      sameZoneAs: contentWindow,
      wantXrays: true,
      isWebExtensionContentScript: true,
      wantExportHelpers: true,
      originAttributes: docPrincipal.originAttributes,
    });

    // TODO bug 1911836: Expose APIs when messaging is true.

    return sandbox;
  }
}

export const ExtensionUserScriptsContent = {
  sandboxFor(context, worldId) {
    return WorldCollection.sandboxFor(context, worldId);
  },
  updateWorldConfig(extension, reset, update) {
    let configHolder = WorldConfigHolder.allMaps.get(extension);
    if (reset) {
      for (let worldId of reset) {
        configHolder.resetWorldConfiguration(worldId);
      }
    }
    if (update) {
      for (let properties of update) {
        configHolder.configureWorld(properties);
      }
    }
  },
};
