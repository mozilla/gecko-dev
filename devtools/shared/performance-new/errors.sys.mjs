/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
// @ts-check

export class UnhandledCaseError extends Error {
  /**
   * @param {never} value - Check that
   * @param {string} typeName - A friendly type name.
   */
  constructor(value, typeName) {
    super(`There was an unhandled case for "${typeName}": ${value}`);
    this.name = "UnhandledCaseError";
  }
}
