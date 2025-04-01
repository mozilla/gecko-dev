/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* global ExtensionAPI, ExtensionCommon, Services */

this.testUtils = class extends ExtensionAPI {
  getAPI(context) {
    const { EventManager } = ExtensionCommon;
    let messageListener;
    return {
      testUtils: {
        onMessage: new EventManager({
          context,
          name: "testUtils.onMessage",
          register: fire => {
            if (!messageListener) {
              const obs = {
                receiveMessage({ data }) {
                  fire
                    .async(data)
                    .then(retval => {
                      Services.ppmm.broadcastAsyncMessage(
                        `WebCompat:${data.name}:Done`,
                        retval
                      );
                    })
                    .catch(() => {}); // ignore Message Manager disconnects
                },
              };
              Services.cpmm.addMessageListener("WebCompat", obs);
              messageListener = () => {
                Services.cpmm.removeMessageListener("WebCompat", obs);
              };
            }
            return messageListener;
          },
        }).api(),
        async interventionsActive() {
          Services.ppmm.sharedData.set(
            "WebCompatTests:InterventionsStatus",
            "active"
          );
        },
        async interventionsInactive() {
          Services.ppmm.sharedData.set(
            "WebCompatTests:InterventionsStatus",
            "inactive"
          );
        },
        async shimsActive() {
          Services.ppmm.sharedData.set("WebCompatTests:ShimsStatus", "active");
        },
        async shimsInactive() {
          Services.ppmm.sharedData.set(
            "WebCompatTests:ShimsStatus",
            "inactive"
          );
        },
      },
    };
  }
};
