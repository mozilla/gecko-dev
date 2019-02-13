function sendMessage(msg) {
  alert(msg);
}

function ok(p, msg) {
  if (p)
    sendMessage("OK: " + msg);
  else
    sendMessage("KO: " + msg);
}

function is(a, b, msg) {
  if (a == b)
    sendMessage("OK: " + a + " == " + b + " - " + msg);
  else
    sendMessage("KO: " + a + " != " + b + " - " + msg);
}

function installed(p) {
  if (p)
    sendMessage("IS_INSTALLED");
  else
    sendMessage("NOT_INSTALLED");
}

function finish() {
  sendMessage("VERSION: MyWebApp vVERSIONTOKEN");
  sendMessage("DONE");
}

function cbError() {
  ok(false, "Error callback invoked");
  finish();
}

function go() {
  ok(true, "Launched APPTYPETOKEN app");
  var request = window.navigator.mozApps.getSelf();
  request.onsuccess = function() {
    var app = request.result;
    checkApp(app);
  }
  request.onerror = cbError;
}

function checkApp(app) {
  // If the app is installed, |app| will be non-null. If it is, verify its state.
  installed(!!app);
  if (app) {
    var appName = "Really Rapid Release (APPTYPETOKEN)";
    var manifest = SpecialPowers.wrap(app.manifest);
    is(manifest.name, appName, "Manifest name should be correct");
    is(app.origin, "http://test", "App origin should be correct");
    is(app.installOrigin, "http://mochi.test:8888", "Install origin should be correct");
  }
  finish();
}

go();
