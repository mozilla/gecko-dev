/* import-globals-from head_psm.js */
"use strict";

function run_test() {
  let SSService = Cc["@mozilla.org/ssservice;1"]
                    .getService(Ci.nsISiteSecurityService);

  ok(!SSService.isSecureHost(Ci.nsISiteSecurityService.HEADER_HSTS,
                             "expired.example.com", 0));
  ok(SSService.isSecureHost(Ci.nsISiteSecurityService.HEADER_HSTS,
                            "notexpired.example.com", 0));
  ok(SSService.isSecureHost(Ci.nsISiteSecurityService.HEADER_HSTS,
                            "includesubdomains.preloaded.test", 0));
  ok(!SSService.isSecureHost(Ci.nsISiteSecurityService.HEADER_HSTS,
                             "sub.includesubdomains.preloaded.test", 0));
  ok(SSService.isSecureHost(Ci.nsISiteSecurityService.HEADER_HSTS,
                            "incsubdomain.example.com", 0));
  ok(SSService.isSecureHost(Ci.nsISiteSecurityService.HEADER_HSTS,
                            "sub.incsubdomain.example.com", 0));
  ok(!SSService.isSecureHost(Ci.nsISiteSecurityService.HEADER_HSTS,
                             "includesubdomains2.preloaded.test", 0));
  ok(!SSService.isSecureHost(Ci.nsISiteSecurityService.HEADER_HSTS,
                             "sub.includesubdomains2.preloaded.test", 0));
  do_test_finished();
}
