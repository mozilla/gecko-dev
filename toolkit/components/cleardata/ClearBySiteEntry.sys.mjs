/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
/* License, v. 2.0. If a copy of the MPL was not distributed with this file,
/* You can obtain one at http://mozilla.org/MPL/2.0/. */

export class ClearBySiteEntry {
  #schemelessSite;
  #patternJSON;

  constructor(site, patternJSON) {
    this.#schemelessSite = site;
    this.#patternJSON = patternJSON;
  }

  get schemelessSite() {
    return this.#schemelessSite;
  }

  set schemelessSite(value) {
    this.#schemelessSite = value;
  }

  get patternJSON() {
    return this.#patternJSON;
  }

  set patternJSON(value) {
    this.#patternJSON = value;
  }

  QueryInterface = ChromeUtils.generateQI(["nsIClearBySiteEntry"]);
}
