/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { WindowGlobalBiDiModule } from "chrome://remote/content/webdriver-bidi/modules/WindowGlobalBiDiModule.sys.mjs";

class EmulationModule extends WindowGlobalBiDiModule {
  constructor(messageHandler) {
    super(messageHandler);
  }

  destroy() {}

  /**
   * Internal commands
   */

  _applySessionData() {}

  /**
   * Set the geolocation override to the navigable.
   *
   * @param {object=} params
   * @param {(GeolocationCoordinates|null)} params.coordinates
   *     Geolocation coordinates which have to override
   *     the return result of geolocation APIs.
   *     Null value resets the override.
   */
  async _setGeolocationOverride(params = {}) {
    const { coordinates } = params;

    if (coordinates === null) {
      this.messageHandler.context.setGeolocationServiceOverride();
    } else {
      this.messageHandler.context.setGeolocationServiceOverride({
        coords: coordinates,
        // The timestamp attribute represents the time
        // when the geographic position of the device was acquired.
        timestamp: Date.now(),
      });
    }
  }
}

export const emulation = EmulationModule;
