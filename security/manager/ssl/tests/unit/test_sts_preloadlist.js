// This test attempts to use only domains that are likely to remain on the
// preload list for a long time. Currently this includes bugzilla.mozilla.org
// and login.persona.org because they are Mozilla properties and we are
// invested in HSTS. Additionally, www.torproject.org was deemed likely to
// continue to use HSTS.

var Cc = Components.classes;
var Ci = Components.interfaces;

Components.utils.import("resource://gre/modules/Services.jsm");

var _pbsvc;

// lazy getter, added for bug 785860 to avoid testing private browsing when the
// private browsing service doesn't exist
function getPBSvc() {
  if (_pbsvc)
    return _pbsvc;

  try {
    _pbsvc = Cc["@mozilla.org/privatebrowsing;1"]
               .getService(Ci.nsIPrivateBrowsingService);
    return _pbsvc;
  } catch (e) {}

  return null;
}

var gSTSService = Cc["@mozilla.org/stsservice;1"]
                  .getService(Ci.nsIStrictTransportSecurityService);

function Observer() {}
Observer.prototype = {
  observe: function(subject, topic, data) {
    run_next_test();
  }
};

var gObserver = new Observer();

// nsIStrictTransportSecurityService.removeStsState removes a given domain's
// HSTS status. This means that a domain on the preload list will be
// considered not HSTS if this is called. So, to reset everything to its
// original state, we have to reach into the permission manager and clear
// any HSTS-related state manually.
function clearStsState() {
  var permissionManager = Cc["@mozilla.org/permissionmanager;1"]
                            .getService(Ci.nsIPermissionManager);
  // This is a list of every host we call processStsHeader with
  // (so we can remove any state added to the sts service)
  var hosts = ["bugzilla.mozilla.org", "login.persona.org",
               "subdomain.www.torproject.org",
               "subdomain.bugzilla.mozilla.org" ];
  for (var host of hosts) {
    permissionManager.remove(host, "sts/use");
    permissionManager.remove(host, "sts/subd");
  }
}

function cleanup() {
  Services.obs.removeObserver(gObserver, "private-browsing-transition-complete");
  if (getPBSvc())
    getPBSvc().privateBrowsingEnabled = false;
  clearStsState();
}

function run_test() {
  do_register_cleanup(cleanup);
  Services.obs.addObserver(gObserver, "private-browsing-transition-complete", false);
  Services.prefs.setBoolPref("browser.privatebrowsing.keep_current_session", true);

  add_test(test_part1);
  if (getPBSvc()) {
    add_test(test_private_browsing1);
    add_test(test_private_browsing2);
  }

  run_next_test();
}

function test_part1() {
  // check that a host not in the list is not identified as an sts host
  do_check_false(gSTSService.isStsHost("nonexistent.mozilla.com"));

  // check that an ancestor domain is not identified as an sts host
  do_check_false(gSTSService.isStsHost("com"));

  // check that a subdomain is an sts host (includeSubdomains is set)
  do_check_true(gSTSService.isStsHost("subdomain.bugzilla.mozilla.org"));

  // check that another subdomain is an sts host (includeSubdomains is set)
  do_check_true(gSTSService.isStsHost("a.b.c.def.bugzilla.mozilla.org"));

  // check that a subdomain is not an sts host (includeSubdomains is not set)
  do_check_false(gSTSService.isStsHost("subdomain.www.torproject.org"));

  // check that a host with a dot on the end won't break anything
  do_check_false(gSTSService.isStsHost("notsts.nonexistent.mozilla.com."));

  // check that processing a header with max-age: 0 will remove a preloaded
  // site from the list
  var uri = Services.io.newURI("http://bugzilla.mozilla.org", null, null);
  gSTSService.processStsHeader(uri, "max-age=0");
  do_check_false(gSTSService.isStsHost("bugzilla.mozilla.org"));
  do_check_false(gSTSService.isStsHost("subdomain.bugzilla.mozilla.org"));
  // check that processing another header (with max-age non-zero) will
  // re-enable a site's sts status
  gSTSService.processStsHeader(uri, "max-age=1000");
  do_check_true(gSTSService.isStsHost("bugzilla.mozilla.org"));
  // but this time include subdomains was not set, so test for that
  do_check_false(gSTSService.isStsHost("subdomain.bugzilla.mozilla.org"));
  clearStsState();

  // check that processing a header with max-age: 0 from a subdomain of a site
  // will not remove that (ancestor) site from the list
  var uri = Services.io.newURI("http://subdomain.www.torproject.org", null, null);
  gSTSService.processStsHeader(uri, "max-age=0");
  do_check_true(gSTSService.isStsHost("www.torproject.org"));
  do_check_false(gSTSService.isStsHost("subdomain.www.torproject.org"));

  var uri = Services.io.newURI("http://subdomain.bugzilla.mozilla.org", null, null);
  gSTSService.processStsHeader(uri, "max-age=0");
  // we received a header with "max-age=0", so we have "no information"
  // regarding the sts state of subdomain.bugzilla.mozilla.org specifically,
  // but it is actually still an STS host, because of the preloaded
  // bugzilla.mozilla.org including subdomains.
  // Here's a drawing:
  // |-- bugzilla.mozilla.org (in preload list, includes subdomains) IS sts host
  //     |-- subdomain.bugzilla.mozilla.org                          IS sts host
  //     |   `-- another.subdomain.bugzilla.mozilla.org              IS sts host
  //     `-- sibling.bugzilla.mozilla.org                            IS sts host
  do_check_true(gSTSService.isStsHost("bugzilla.mozilla.org"));
  do_check_true(gSTSService.isStsHost("subdomain.bugzilla.mozilla.org"));
  do_check_true(gSTSService.isStsHost("sibling.bugzilla.mozilla.org"));
  do_check_true(gSTSService.isStsHost("another.subdomain.bugzilla.mozilla.org"));


  gSTSService.processStsHeader(uri, "max-age=1000");
  // Here's what we have now:
  // |-- bugzilla.mozilla.org (in preload list, includes subdomains) IS sts host
  //     |-- subdomain.bugzilla.mozilla.org (include subdomains is false) IS sts host
  //     |   `-- another.subdomain.bugzilla.mozilla.org              IS NOT sts host
  //     `-- sibling.bugzilla.mozilla.org                            IS sts host
  do_check_true(gSTSService.isStsHost("subdomain.bugzilla.mozilla.org"));
  do_check_true(gSTSService.isStsHost("sibling.bugzilla.mozilla.org"));
  do_check_false(gSTSService.isStsHost("another.subdomain.bugzilla.mozilla.org"));

  // Test private browsing correctly interacts with removing preloaded sites.
  // If we don't have the private browsing service, don't run those tests
  // (which means we have to manually call run_next_test() instead of relying
  // on our observer to call it).
  if (getPBSvc()) {
    getPBSvc().privateBrowsingEnabled = true;
  } else {
    run_next_test();
  }
}

function test_private_browsing1() {
  clearStsState();
  // sanity - bugzilla.mozilla.org is preloaded, includeSubdomains set
  do_check_true(gSTSService.isStsHost("bugzilla.mozilla.org");
  do_check_true(gSTSService.isStsHost("a.b.c.subdomain.bugzilla.mozilla.org"));

  var uri = Services.io.newURI("http://bugzilla.mozilla.org", null, null);
  gSTSService.processStsHeader(uri, "max-age=0");
  do_check_false(gSTSService.isStsHost("bugzilla.mozilla.org"));
  do_check_false(gSTSService.isStsHost("a.b.subdomain.bugzilla.mozilla.org"));

  // check adding it back in
  gSTSService.processStsHeader(uri, "max-age=1000");
  do_check_true(gSTSService.isStsHost("bugzilla.mozilla.org"));
  // but no includeSubdomains this time
  do_check_false(gSTSService.isStsHost("b.subdomain.bugzilla.mozilla.org"));

  // do the hokey-pokey...
  gSTSService.processStsHeader(uri, "max-age=0");
  do_check_false(gSTSService.isStsHost("bugzilla.mozilla.org"));
  do_check_false(gSTSService.isStsHost("subdomain.bugzilla.mozilla.org"));  
  
  // TODO unfortunately we don't have a good way to know when an entry
  // has expired in the permission manager, so we can't yet extend this test
  // to that case.
  // Test that an expired private browsing entry results in correctly
  // identifying a host that is on the preload list as no longer sts.
  // (This happens when we're in private browsing mode, we get a header from
  // a site on the preload list, and that header later expires. We need to
  // then treat that host as no longer an sts host.)
  // (sanity check first - this should be in the preload list)
  do_check_true(gSTSService.isStsHost("login.persona.org"));
  var uri = Services.io.newURI("http://login.persona.org", null, null);
  // according to the rfc, max-age can't be negative, but this is a great
  // way to test an expired entry
  gSTSService.processStsHeader(uri, "max-age=-1000");
  do_check_false(gSTSService.isStsHost("login.persona.org"));

  // if this test gets this far, it means there's a private browsing service
  getPBSvc().privateBrowsingEnabled = false;
}

function test_private_browsing2() {
  // if this test gets this far, it means there's a private browsing service
  do_check_true(gSTSService.isStsHost("bugzilla.mozilla.org"));
  // the bugzilla.mozilla.org entry has includeSubdomains set
  do_check_true(gSTSService.isStsHost("subdomain.bugzilla.mozilla.org"));

  // Now that we're out of private browsing mode, we need to make sure
  // we've "forgotten" that we "forgot" this site's sts status.
  do_check_true(gSTSService.isStsHost("login.persona.org"));

  run_next_test();
}
