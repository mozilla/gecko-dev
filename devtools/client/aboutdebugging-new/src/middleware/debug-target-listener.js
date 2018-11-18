/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  DEBUG_TARGETS,
  UNWATCH_RUNTIME_START,
  WATCH_RUNTIME_SUCCESS,
} = require("../constants");
const Actions = require("../actions/index");
const { isSupportedDebugTarget } = require("../modules/debug-target-support");

function debugTargetListenerMiddleware(store) {
  const onExtensionsUpdated = () => {
    store.dispatch(Actions.requestExtensions());
  };

  const onTabsUpdated = () => {
    store.dispatch(Actions.requestTabs());
  };

  const onWorkersUpdated = () => {
    store.dispatch(Actions.requestWorkers());
  };

  return next => action => {
    switch (action.type) {
      case WATCH_RUNTIME_SUCCESS: {
        const { runtime } = action;
        const { client } = runtime.runtimeDetails;

        if (isSupportedDebugTarget(runtime.type, DEBUG_TARGETS.TAB)) {
          client.addListener("tabListChanged", onTabsUpdated);
        }

        if (isSupportedDebugTarget(runtime.type, DEBUG_TARGETS.EXTENSION)) {
          client.addListener("addonListChanged", onExtensionsUpdated);
        }

        if (isSupportedDebugTarget(runtime.type, DEBUG_TARGETS.WORKER)) {
          client.addListener("workerListChanged", onWorkersUpdated);
          client.addListener("serviceWorkerRegistrationListChanged", onWorkersUpdated);
          client.addListener("processListChanged", onWorkersUpdated);
          client.addListener("registration-changed", onWorkersUpdated);
          client.addListener("push-subscription-modified", onWorkersUpdated);
        }
        break;
      }
      case UNWATCH_RUNTIME_START: {
        const { runtime } = action;
        const { client } = runtime.runtimeDetails;

        if (isSupportedDebugTarget(runtime.type, DEBUG_TARGETS.TAB)) {
          client.removeListener("tabListChanged", onTabsUpdated);
        }

        if (isSupportedDebugTarget(runtime.type, DEBUG_TARGETS.EXTENSION)) {
          client.removeListener("addonListChanged", onExtensionsUpdated);
        }

        if (isSupportedDebugTarget(runtime.type, DEBUG_TARGETS.WORKER)) {
          client.removeListener("workerListChanged", onWorkersUpdated);
          client.removeListener("serviceWorkerRegistrationListChanged", onWorkersUpdated);
          client.removeListener("processListChanged", onWorkersUpdated);
          client.removeListener("registration-changed", onWorkersUpdated);
          client.removeListener("push-subscription-modified", onWorkersUpdated);
        }
        break;
      }
    }

    return next(action);
  };
}

module.exports = debugTargetListenerMiddleware;
