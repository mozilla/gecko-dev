/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export var ForgetAboutSite = {
  /**
   * Clear data associated with a base domain. This includes partitioned storage
   * associated with the domain.
   *
   * @param {string} aDomainOrHost - Domain or host to clear data for. Will be
   * converted to base domain if needed.
   * @returns {Promise} - Resolves once all matching data has been cleared.
   * Throws if any of the internal cleaners fail.
   */
  async removeDataFromBaseDomain(aDomainOrHost) {
    if (!aDomainOrHost) {
      throw new Error("aDomainOrHost can not be empty.");
    }

    let schemelessSite = Services.eTLD.getSchemelessSiteFromHost(aDomainOrHost);
    let errorCount = await new Promise(resolve => {
      Services.clearData.deleteDataFromSite(
        schemelessSite,
        {},
        true /* user request */,
        Ci.nsIClearDataService.CLEAR_FORGET_ABOUT_SITE,
        errorCode => resolve(bitCounting(errorCode))
      );
    });

    if (errorCount !== 0) {
      throw new Error(
        `There were a total of ${errorCount} errors during removal`
      );
    }
  },
};

function bitCounting(value) {
  // To know more about how to count bits set to 1 in a numeric value, see this
  // interesting article:
  // https://blogs.msdn.microsoft.com/jeuge/2005/06/08/bit-fiddling-3/
  const count =
    value - ((value >> 1) & 0o33333333333) - ((value >> 2) & 0o11111111111);
  return ((count + (count >> 3)) & 0o30707070707) % 63;
}
