"use strict";

add_task(_ => {
  // Check that create instance and the service getter all return the same object.
  let etld = Cc["@mozilla.org/network/effective-tld-service;1"].createInstance(
    Ci.nsIEffectiveTLDService
  );

  Assert.equal(etld, Services.eTLD);

  // eslint-disable-next-line mozilla/use-services
  let etld2 = Cc["@mozilla.org/network/effective-tld-service;1"].getService(
    Ci.nsIEffectiveTLDService
  );

  Assert.equal(etld, etld2);
});
