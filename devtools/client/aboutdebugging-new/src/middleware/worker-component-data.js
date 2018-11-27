/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  DEBUG_TARGETS,
  REQUEST_WORKERS_SUCCESS,
  SERVICE_WORKER_FETCH_STATES,
  SERVICE_WORKER_STATUSES,
} = require("../constants");

/**
 * This middleware converts workers object that get from DebuggerClient.listAllWorkers()
 * to data which is used in DebugTargetItem.
 */
const workerComponentDataMiddleware = store => next => action => {
  switch (action.type) {
    case REQUEST_WORKERS_SUCCESS: {
      action.otherWorkers = toComponentData(action.otherWorkers);
      action.serviceWorkers = toComponentData(action.serviceWorkers, true);
      action.sharedWorkers = toComponentData(action.sharedWorkers);
      break;
    }
  }

  return next(action);
};

function getServiceWorkerStatus(isActive, isRunning) {
  if (isActive && isRunning) {
    return SERVICE_WORKER_STATUSES.RUNNING;
  } else if (isActive) {
    return SERVICE_WORKER_STATUSES.STOPPED;
  }
  // We cannot get service worker registrations unless the registration is in
  // ACTIVE state. Unable to know the actual state ("installing", "waiting"), we
  // display a custom state "registering" for now. See Bug 1153292.
  return SERVICE_WORKER_STATUSES.REGISTERING;
}

function toComponentData(workers, isServiceWorker) {
  return workers.map(worker => {
    // Here `worker` is the worker object created by RootFront.listAllWorkers
    const type = DEBUG_TARGETS.WORKER;
    const icon = "chrome://devtools/skin/images/debugging-workers.svg";
    let { fetch, name, registrationActor, scope, workerTargetFront } = worker;

    // For registering service workers, workerTargetFront will not be available.
    // The only valid identifier we can use at that point is the actorID for the
    // service worker registration.
    const id = workerTargetFront ? workerTargetFront.actorID : registrationActor;

    let isActive = false;
    let isRunning = false;
    let status = null;

    if (isServiceWorker) {
      fetch = fetch ? SERVICE_WORKER_FETCH_STATES.LISTENING
                    : SERVICE_WORKER_FETCH_STATES.NOT_LISTENING;
      isActive = worker.active;
      isRunning = !!worker.workerTargetFront;
      status = getServiceWorkerStatus(isActive, isRunning);
    }

    return {
      details: {
        fetch,
        isActive,
        isRunning,
        registrationActor,
        scope,
        status,
      },
      icon,
      id,
      name,
      type,
    };
  });
}

module.exports = workerComponentDataMiddleware;
