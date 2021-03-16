/* global chrome */

'use strict';

let backendDisconnected = false;
let backendInitialized = false;

let window;

function sayHelloToBackend() {
  window.postMessage(
    {
      source: 'react-devtools-content-script',
      hello: true,
    },
    '*',
  );
}

function handleMessageFromDevtools(message) {
  window.postMessage(
    {
      source: 'react-devtools-content-script',
      payload: message,
    },
    '*',
  );
}

function handleMessageFromPage(evt) {
  if (
    evt.source === window &&
    evt.data &&
    evt.data.source === 'react-devtools-bridge'
  ) {
    backendInitialized = true;

    port.postMessage(evt.data.payload);
  }
}

function handleDisconnect() {
  backendDisconnected = true;

  window.removeEventListener('message', handleMessageFromPage);

  window.postMessage(
    {
      source: 'react-devtools-content-script',
      payload: {
        type: 'event',
        event: 'shutdown',
      },
    },
    '*',
  );
}

// proxy from main page to devtools (via the background page)
/*
const port = chrome.runtime.connect({
  name: 'content-script',
});
port.onMessage.addListener(handleMessageFromDevtools);
port.onDisconnect.addListener(handleDisconnect);
*/
const port = null;

function initialize(dbgWindow) {
  window = dbgWindow.unsafeDereference();

  const { installHook } = require("devtools/server/actors/replay/react-devtools/hook");
  dbgWindow.executeInGlobal(`(${installHook}(window))`);

  const {
    initialize: initBackend,
  } = require("devtools/server/actors/replay/react-devtools/react_devtools_backend");
  initBackend(window);

  window.addEventListener('message', handleMessageFromPage);
  sayHelloToBackend();
}

exports.initialize = initialize;
