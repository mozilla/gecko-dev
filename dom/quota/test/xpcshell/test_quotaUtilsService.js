/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/* exported testSteps */
async function testSteps() {
  const quotaUtilsService = Cc[
    "@mozilla.org/dom/quota-utils-service;1"
  ].getService(Ci.nsIQuotaUtilsService);

  {
    const id = quotaUtilsService.getPrivateIdentityId("");
    Assert.equal(id, 0, "Correct id");
  }

  {
    const id = quotaUtilsService.getPrivateIdentityId(
      "userContextIdInternal.thumbnail"
    );
    Assert.greater(id, 4, "Correct id");
  }
}
