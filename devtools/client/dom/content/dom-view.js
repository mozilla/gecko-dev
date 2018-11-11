/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// React & Redux
const React = require("devtools/client/shared/vendor/react");
const ReactDOM = require("devtools/client/shared/vendor/react-dom");
const { Provider } = require("devtools/client/shared/vendor/react-redux");
const { combineReducers } = require("devtools/client/shared/vendor/redux");

// DOM Panel
const MainFrame = React.createFactory(require("./components/main-frame"));

// Store
const createStore = require("devtools/client/shared/redux/create-store")({
  log: false
});

const { reducers } = require("./reducers/index");
const store = createStore(combineReducers(reducers));

/**
 * This object represents view of the DOM panel and is responsible
 * for rendering the content. It renders the top level ReactJS
 * component: the MainFrame.
 */
function DomView(localStore) {
  addEventListener("devtools/chrome/message",
    this.onMessage.bind(this), true);

  // Make it local so, tests can access it.
  this.store = localStore;
}

DomView.prototype = {
  initialize: function (rootGrip) {
    let content = document.querySelector("#content");
    let mainFrame = MainFrame({
      object: rootGrip,
    });

    // Render top level component
    let provider = React.createElement(Provider, {
      store: this.store
    }, mainFrame);

    this.mainFrame = ReactDOM.render(provider, content);
  },

  onMessage: function (event) {
    let data = event.data;
    let method = data.type;

    if (typeof this[method] == "function") {
      this[method](data.args);
    }
  },
};

// Construct DOM panel view object and expose it to tests.
// Tests can access it throught: |panel.panelWin.view|
window.view = new DomView(store);
