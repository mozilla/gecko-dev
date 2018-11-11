/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function run_test() {
  const mozIntl = Components.classes["@mozilla.org/mozintl;1"]
                            .getService(Components.interfaces.mozIMozIntl);

  test_this_global(mozIntl);
  test_cross_global(mozIntl);

  ok(true);
}

function test_this_global(mozIntl) {
  let x = {};

  mozIntl.addGetCalendarInfo(x);
  equal(x.getCalendarInfo instanceof Function, true);
  equal(x.getCalendarInfo() instanceof Object, true);
}

function test_cross_global(mozIntl) {
  var global = new Components.utils.Sandbox("https://example.com/");
  var x = global.Object();

  mozIntl.addGetCalendarInfo(x);
  var waivedX = Components.utils.waiveXrays(x);
  equal(waivedX.getCalendarInfo instanceof Function, false);
  equal(waivedX.getCalendarInfo instanceof global.Function, true);
  equal(waivedX.getCalendarInfo() instanceof Object, false);
  equal(waivedX.getCalendarInfo() instanceof global.Object, true);
}
