/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";
import { FilterAdultComponent } from "moz-src:///toolkit/components/uniffi-bindgen-gecko-js/components/generated/RustFilterAdult.sys.mjs";

const lazy = {};

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "gFilterAdultEnabled",
  "browser.newtabpage.activity-stream.filterAdult",
  true
);

export class _FilterAdult {
  #comp = null;

  constructor() {
    this.#comp = FilterAdultComponent.init();
  }

  /**
   * Filter out any link objects that have a url with an adult base domain.
   *
   * @param {string[]} links
   *   An array of links to test.
   * @returns {string[]}
   *   A filtered array without adult links.
   */
  filter(links) {
    if (!lazy.gFilterAdultEnabled) {
      return links;
    }

    return links.filter(({ url }) => {
      try {
        const uri = Services.io.newURI(url);
        return !this.#comp.contains(Services.eTLD.getBaseDomain(uri));
      } catch (ex) {
        return true;
      }
    });
  }

  /**
   * Determine if the supplied url is an adult url or not.
   *
   * @param {string} url
   *   The url to test.
   * @returns {boolean}
   *   True if it is an adult url.
   */
  isAdultUrl(url) {
    if (!lazy.gFilterAdultEnabled) {
      return false;
    }
    try {
      const uri = Services.io.newURI(url);
      return this.#comp.contains(Services.eTLD.getBaseDomain(uri));
    } catch (ex) {
      return false;
    }
  }
}

export const FilterAdult = new _FilterAdult();
