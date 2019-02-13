var gWidgetManifestURL = 'http://test/tests/dom/apps/tests/file_app.sjs?apptype=widget&getmanifest=true';
var gInvalidWidgetManifestURL = 'http://test/tests/dom/apps/tests/file_app.sjs?apptype=invalidWidget&getmanifest=true';
var gApp;
var gHasBrowserPermission;

function onError() {
  ok(false, "Error callback invoked");
  finish();
}

function installApp(path) {
  var request = navigator.mozApps.install(path);
  request.onerror = onError;
  request.onsuccess = function() {
    gApp = request.result;

    runTest();
  }
}

function uninstallApp() {
  // Uninstall the app.
  var request = navigator.mozApps.mgmt.uninstall(gApp);
  request.onerror = onError;
  request.onsuccess = function() {
    // All done.
    info("All done");

    runTest();
  }
}

function testApp(isValidWidget) {
  info("Test widget feature. IsValidWidget: " + isValidWidget);

  var ifr = document.createElement('iframe');
  ifr.setAttribute('mozbrowser', 'true');
  ifr.setAttribute('mozwidget', gApp.manifestURL);
  ifr.setAttribute('src', gApp.origin+gApp.manifest.launch_path);

  var domParent = document.getElementById('container');
  domParent.appendChild(ifr);

  var mm = SpecialPowers.getBrowserFrameMessageManager(ifr);
  mm.addMessageListener('OK', function(msg) {
    ok(isValidWidget, "Message from widget: " + SpecialPowers.wrap(msg).json);
  });
  mm.addMessageListener('KO', function(msg) {
    ok(!isValidWidget, "Message from widget: " + SpecialPowers.wrap(msg).json);
  });
  mm.addMessageListener('DONE', function(msg) {
    ok(true, "Message from widget complete: "+SpecialPowers.wrap(msg).json);
    domParent.removeChild(ifr);
    runTest();
  });

  ifr.addEventListener('mozbrowserloadend', function() {
    ok(true, "receive mozbrowserloadend");

    // Test limited browser API feature only for valid widget case
    if (isValidWidget) {
      testLimitedBrowserAPI(ifr);
    }
    SimpleTest.executeSoon(()=>loadFrameScript(mm));
  }, false);

  // Test limited browser API feature only for valid widget case
  if (!isValidWidget) {
    return;
  }

  [
    'mozbrowsertitlechange',
    'mozbrowseropenwindow',
    'mozbrowserscroll',
    'mozbrowserasyncscroll'
  ].forEach( function(topic) {
    ifr.addEventListener(topic, function() {
      ok(false, topic + " should be hidden");
    }, false);
  });
}

function testLimitedBrowserAPI(ifr) {
  var securitySensitiveCalls = [
    { api: 'sendMouseEvent'      , args: ['mousedown', 0, 0, 0, 0, 0] },
    { api: 'sendTouchEvent'      , args: ['touchstart', [0], [0], [0], [1], [1], [0], [1], 1, 0] },
    { api: 'goBack'              , args: [] },
    { api: 'goForward'           , args: [] },
    { api: 'reload'              , args: [] },
    { api: 'stop'                , args: [] },
    { api: 'download'            , args: ['http://example.org'] },
    { api: 'purgeHistory'        , args: [] },
    { api: 'getScreenshot'       , args: [0, 0] },
    { api: 'zoom'                , args: [0.1] },
    { api: 'getCanGoBack'        , args: [] },
    { api: 'getCanGoForward'     , args: [] },
    { api: 'getContentDimensions', args: [] }
  ];
  securitySensitiveCalls.forEach( function(call) {
    if (gHasBrowserPermission) {
      isnot(typeof ifr[call.api], "undefined", call.api + " should be defined");
      var didThrow;
      try {
        ifr[call.api].apply(ifr, call.args);
      } catch (e) {
        ok(e instanceof DOMException, "throw right exception type");
        didThrow = e.code;
      }
      is(didThrow, DOMException.INVALID_NODE_TYPE_ERR, "call " + call.api + " should throw exception");
    } else {
      is(typeof ifr[call.api], "undefined", call.api + " should be hidden for widget");
    }
  });
}

function loadFrameScript(mm) {
  var script = 'data:,\
  function ok(p, msg) { \
  if (p) { \
  sendAsyncMessage("OK", msg); \
} else { \
  sendAsyncMessage("KO", msg); \
} \
} \
  \
  function is(a, b, msg) { \
  if (a == b) { \
  sendAsyncMessage("OK", a + " == " + b + " - " + msg); \
} else { \
  sendAsyncMessage("KO", a + " != " + b + " - " + msg); \
} \
} \
  \
  function finish() { \
  sendAsyncMessage("DONE",""); \
} \
  \
  function onError() { \
  ok(false, "Error callback invoked"); \
  finish(); \
} \
  \
  function checkWidget(widget) { \
  /*For invalid widget case, ignore the following check*/\
  if (widget) { \
  var widgetName = "Really Rapid Release (APPTYPETOKEN)"; \
  is(widget.origin, "http://test", "Widget origin should be correct"); \
  is(widget.installOrigin, "http://mochi.test:8888", "Install origin should be correct"); \
} \
  finish(); \
} \
  \
  var request = content.window.navigator.mozApps.getSelf(); \
  request.onsuccess = function() { \
  var widget = request.result; \
  ok(widget,"Should be a widget"); \
  checkWidget(widget); \
}; \
  request.onerror = onError; \
  content.window.open("about:blank"); /*test mozbrowseropenwindow*/ \
  content.window.scrollTo(4000, 4000); /*test mozbrowser(async)scroll*/ \
  ';
  mm.loadFrameScript(script, /* allowDelayedLoad = */ false);
}

var tests = [
  // Permissions
  function() {
    SpecialPowers.pushPermissions(
      [{ "type": "browser", "allow": gHasBrowserPermission ? 1 : 0, "context": document },
       { "type": "embed-widgets", "allow": 1, "context": document },
       { "type": "webapps-manage", "allow": 1, "context": document }], runTest);
  },

  // Preferences
  function() {
    SpecialPowers.pushPrefEnv({"set": [["dom.mozBrowserFramesEnabled", true],
                                       ["dom.enable_widgets", true],
                                       ["dom.datastore.sysMsgOnChangeShortTimeoutSec", 1],
                                       ["dom.datastore.sysMsgOnChangeLongTimeoutSec", 3]]}, runTest);
  },

  function() {
    if (SpecialPowers.isMainProcess()) {
      SpecialPowers.Cu.import("resource://gre/modules/DataStoreChangeNotifier.jsm");
    }

    SpecialPowers.setAllAppsLaunchable(true);
    runTest();
  },

  // No confirmation needed when an app is installed
  function() {
    SpecialPowers.autoConfirmAppInstall(() => {
      SpecialPowers.autoConfirmAppUninstall(runTest);
    });
  },

  // Installing the app
  ()=>installApp(gWidgetManifestURL),

  // Run tests in app
  ()=>testApp(true),

  // Uninstall the app
  uninstallApp,

  // Installing the app for invalid widget case
  ()=>installApp(gInvalidWidgetManifestURL),

  // Run tests in app for invalid widget case
  ()=>testApp(false),

  // Uninstall the app
  uninstallApp
];

function runTest() {
  if (!tests.length) {
    finish();
    return;
  }

  var test = tests.shift();
  test();
}

function finish() {
  SimpleTest.finish();
}
