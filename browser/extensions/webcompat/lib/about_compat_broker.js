/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* global browser, module, onMessageFromTab */

class AboutCompatBroker {
  constructor(bindings) {
    this._interventions = bindings.interventions;
    this._shims = bindings.shims;

    if (!this._interventions && !this._shims) {
      throw new Error(
        "No interventions or shims; about:compat broker is not needed"
      );
    }

    this.portsToAboutCompatTabs = this.buildPorts();
    this._interventions?.bindAboutCompatBroker(this);
    this._shims?.bindAboutCompatBroker(this);
  }

  buildPorts() {
    const ports = new Set();

    browser.runtime.onConnect.addListener(port => {
      ports.add(port);
      port.onDisconnect.addListener(function () {
        ports.delete(port);
      });
    });

    async function broadcast(message) {
      for (const port of ports) {
        port.postMessage(message);
      }
    }

    return { broadcast };
  }

  filterInterventions(interventions) {
    return interventions
      .filter(intervention => intervention.availableOnPlatform)
      .map(intervention => {
        const { id, active, bugs, hidden } = intervention;
        let domain = intervention.label;
        let bug = Object.keys(bugs)[0];
        return { id, active, bug, domain, hidden };
      });
  }

  getInterventionById(id) {
    for (const [type, things] of Object.entries({
      interventions: this._interventions?.getAvailableInterventions() || [],
      shims: this._shims?.getAvailableShims() || [],
    })) {
      for (const what of things) {
        if (what.id === id) {
          return { type, what };
        }
      }
    }
    return {};
  }

  bootup() {
    onMessageFromTab(msg => {
      switch (msg.command || msg) {
        case "toggle": {
          const id = msg.id;
          const { type, what } = this.getInterventionById(id);
          if (!what) {
            return Promise.reject(`No such intervention to toggle: ${id}`);
          }
          const active = type === "shims" ? !what.disabledReason : what.active;
          this.portsToAboutCompatTabs
            .broadcast({ toggling: id, active })
            .then(async () => {
              switch (type) {
                case "interventions": {
                  if (active) {
                    await this._interventions?.disableIntervention(what);
                  } else {
                    await this._interventions?.enableIntervention(what);
                  }
                  break;
                }
                case "shims": {
                  if (active) {
                    await this._shims?.disableShimForSession(id);
                  } else {
                    await this._shims?.enableShimForSession(id);
                  }
                  // no need to broadcast the "toggled" signal for shims, as
                  // they send a shimsUpdated message themselves instead
                  return;
                }
              }
              this.portsToAboutCompatTabs.broadcast({
                toggled: id,
                active: !active,
              });
            });
          break;
        }
        case "getAllInterventions": {
          return Promise.resolve({
            interventions:
              (this._interventions?.isEnabled() &&
                this.filterInterventions(
                  this._interventions?.getAvailableInterventions()
                )) ||
              false,
            shims: this._shims?.getAvailableShims() || false,
          });
        }
      }
      return undefined;
    });
  }
}

module.exports = AboutCompatBroker;
