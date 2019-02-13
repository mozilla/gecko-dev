const Cu = Components.utils;
const READWRITE = "readwrite";
const UNKNOWN = "foobar";

var gData = [
// test normal expansion
{
  permission: "contacts",
  access: READWRITE,
  expected: ["contacts-read", "contacts-create",
             "contacts-write"]
},
// test additional expansion and access not having read+create+write
{
  permission: "settings",
  access: READWRITE,
  expected: ["settings-read", "settings-write",
             "settings-api-read", "settings-api-write",
             "indexedDB-chrome-settings-read",
             "indexedDB-chrome-settings-write"]
},
// test unknown access
{
  permission: "contacts",
  access: UNKNOWN,
  expected: []
},
// test unknown permission
{
  permission: UNKNOWN,
  access: READWRITE,
  expected: []
}
];

// check if 2 arrays contain the same elements
function do_check_set_eq(a1, a2) {
  do_check_eq(a1.length, a2.length)

  Array.sort(a1);
  Array.sort(a2);

  for (let i = 0; i < a1.length; ++i) {
    do_check_eq(a1[i], a2[i])
  }
}

function test_substitute_does_not_break_substituted(scope) {
  const Ci = Components.interfaces;

  // geolocation-noprompt substitutes for geolocation ...
  do_check_eq(scope.PermissionsTable["geolocation-noprompt"].substitute[0],
              "geolocation");
  // ... and sets silent allow ...
  do_check_eq(scope.PermissionsTable["geolocation-noprompt"].certified,
              Ci.nsIPermissionManager.ALLOW_ACTION)
  // ... which works ...
  do_check_false(scope.isExplicitInPermissionsTable("geolocation-noprompt", Ci.nsIPrincipal.APP_STATUS_CERTIFIED));
  // ... but does not interfere with geolocation's PROMPT value
  do_check_true(scope.isExplicitInPermissionsTable("geolocation", Ci.nsIPrincipal.APP_STATUS_CERTIFIED));
}

function run_test() {
  var scope = {};
  Cu.import("resource://gre/modules/PermissionsTable.jsm", scope);

  for (var i = 0; i < gData.length; i++) {
    var perms = scope.expandPermissions(gData[i].permission,
                                        gData[i].access);
    do_check_set_eq(perms, gData[i].expected);
  }
  test_substitute_does_not_break_substituted(scope);
}
