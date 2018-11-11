/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { BrowserLoader } = ChromeUtils.import("resource://devtools/client/shared/browser-loader.js", {});
const require = BrowserLoader({
  baseURI: "resource://devtools/client/application/",
  window,
}).require;

const { createFactory } = require("devtools/client/shared/vendor/react");
const { render, unmountComponentAtNode } = require("devtools/client/shared/vendor/react-dom");
const Provider = createFactory(require("devtools/client/shared/vendor/react-redux").Provider);
const { bindActionCreators } = require("devtools/client/shared/vendor/redux");
const { L10nRegistry } = require("resource://gre/modules/L10nRegistry.jsm");
const Services = require("Services");

const { configureStore } = require("./src/create-store");
const actions = require("./src/actions/index");

const App = createFactory(require("./src/components/App"));

/**
 * Global Application object in this panel. This object is expected by panel.js and is
 * called to start the UI for the panel.
 */
window.Application = {
  async bootstrap({ toolbox, panel }) {
    this.updateWorkers = this.updateWorkers.bind(this);
    this.updateDomain = this.updateDomain.bind(this);

    this.mount = document.querySelector("#mount");
    this.toolbox = toolbox;
    this.client = toolbox.target.client;

    this.store = configureStore();
    this.actions = bindActionCreators(actions, this.store.dispatch);

    const serviceContainer = {
      selectTool(toolId) {
        return toolbox.selectTool(toolId);
      },
    };
    this.toolbox.target.activeTab.on("workerListChanged", this.updateWorkers);
    this.client.mainRoot.on("serviceWorkerRegistrationListChanged", this.updateWorkers);
    this.client.addListener("registration-changed", this.updateWorkers);
    this.client.mainRoot.on("processListChanged", this.updateWorkers);
    this.toolbox.target.on("navigate", this.updateDomain);

    this.updateDomain();
    await this.updateWorkers();

    const fluentBundles = await this.createFluentBundles();

    // Render the root Application component.
    const app = App({ client: this.client, fluentBundles, serviceContainer });
    render(Provider({ store: this.store }, app), this.mount);
  },

  /**
   * Retrieve message contexts for the current locales, and return them as an array of
   * FluentBundles elements.
   */
  async createFluentBundles() {
    const locales = Services.locale.appLocalesAsBCP47;
    const generator =
      L10nRegistry.generateBundles(locales, ["devtools/application.ftl"]);

    // Return value of generateBundles is a generator and should be converted to
    // a sync iterable before using it with React.
    const contexts = [];
    for await (const message of generator) {
      contexts.push(message);
    }

    return contexts;
  },

  async updateWorkers() {
    const { service } = await this.client.mainRoot.listAllWorkers();
    this.actions.updateWorkers(service);
  },

  updateDomain() {
    this.actions.updateDomain(this.toolbox.target.url);
  },

  destroy() {
    this.toolbox.target.activeTab.off("workerListChanged", this.updateWorkers);
    this.client.mainRoot.off("serviceWorkerRegistrationListChanged",
      this.updateWorkers);
    this.client.removeListener("registration-changed", this.updateWorkers);
    this.client.mainRoot.off("processListChanged", this.updateWorkers);

    this.toolbox.target.off("navigate", this.updateDomain);

    unmountComponentAtNode(this.mount);
    this.mount = null;
    this.toolbox = null;
    this.client = null;
  },
};
