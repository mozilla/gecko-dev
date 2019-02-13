Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");

let Cc = Components.classes;
let Ci = Components.interfaces;

const PREF_DISABLE_OPEN_NEW_WINDOW = "browser.link.open_newwindow.disabled_in_fullscreen";
const isOSX = (Services.appinfo.OS === "Darwin");

const TEST_FILE = "file_fullscreen-window-open.html";
const gHttpTestRoot = getRootDirectory(gTestPath).replace("chrome://mochitests/content/",
                                                          "http://127.0.0.1:8888/");

function test () {
  waitForExplicitFinish();

  Services.prefs.setBoolPref(PREF_DISABLE_OPEN_NEW_WINDOW, true);

  let newTab = gBrowser.addTab();
  gBrowser.selectedTab = newTab;

  let gTestBrowser = gBrowser.selectedBrowser;
  gTestBrowser.addEventListener("load", function onLoad(){
    gTestBrowser.removeEventListener("load", onLoad, true, true);

    // Enter browser fullscreen mode.
    BrowserFullScreen();

    runNextTest();
  }, true, true);
  gTestBrowser.contentWindow.location.href = gHttpTestRoot + TEST_FILE;
}

registerCleanupFunction(function(){
  // Exit browser fullscreen mode.
  BrowserFullScreen();

  gBrowser.removeCurrentTab();

  Services.prefs.clearUserPref(PREF_DISABLE_OPEN_NEW_WINDOW);
});

let gTests = [
  test_open,
  test_open_with_size,
  test_open_with_pos,
  test_open_with_outerSize,
  test_open_with_innerSize,
  test_open_with_dialog,
  test_open_when_open_new_window_by_pref,
  test_open_with_pref_to_disable_in_fullscreen,
  test_open_from_chrome,
];

function runNextTest () {
  let test = gTests.shift();
  if (test) {
    executeSoon(test);
  }
  else {
    finish();
  }
}


// Test for window.open() with no feature.
function test_open() {
  waitForTabOpen({
    message: {
      title: "test_open",
      param: "",
    },
    finalizeFn: function () {},
  });
}

// Test for window.open() with width/height.
function test_open_with_size() {
  waitForTabOpen({
    message: {
      title: "test_open_with_size",
      param: "width=400,height=400",
    },
    finalizeFn: function () {},
  });
}

// Test for window.open() with top/left.
function test_open_with_pos() {
  waitForTabOpen({
    message: {
      title: "test_open_with_pos",
      param: "top=200,left=200",
    },
    finalizeFn: function () {},
  });
}

// Test for window.open() with outerWidth/Height.
function test_open_with_outerSize() {
  let [outerWidth, outerHeight] = [window.outerWidth, window.outerHeight];
  waitForTabOpen({
    message: {
      title: "test_open_with_outerSize",
      param: "outerWidth=200,outerHeight=200",
    },
    successFn: function () {
      is(window.outerWidth, outerWidth, "Don't change window.outerWidth.");
      is(window.outerHeight, outerHeight, "Don't change window.outerHeight.");
    },
    finalizeFn: function () {},
  });
}

// Test for window.open() with innerWidth/Height.
function test_open_with_innerSize() {
  let [innerWidth, innerHeight] = [window.innerWidth, window.innerHeight];
  waitForTabOpen({
    message: {
      title: "test_open_with_innerSize",
      param: "innerWidth=200,innerHeight=200",
    },
    successFn: function () {
      is(window.innerWidth, innerWidth, "Don't change window.innerWidth.");
      is(window.innerHeight, innerHeight, "Don't change window.innerHeight.");
    },
    finalizeFn: function () {},
  });
}

// Test for window.open() with dialog.
function test_open_with_dialog() {
  waitForTabOpen({
    message: {
      title: "test_open_with_dialog",
      param: "dialog=yes",
    },
    finalizeFn: function () {},
  });
}

// Test for window.open()
// when "browser.link.open_newwindow" is nsIBrowserDOMWindow.OPEN_NEWWINDOW
function test_open_when_open_new_window_by_pref() {
  const PREF_NAME = "browser.link.open_newwindow";
  Services.prefs.setIntPref(PREF_NAME, Ci.nsIBrowserDOMWindow.OPEN_NEWWINDOW);
  is(Services.prefs.getIntPref(PREF_NAME), Ci.nsIBrowserDOMWindow.OPEN_NEWWINDOW,
     PREF_NAME + " is nsIBrowserDOMWindow.OPEN_NEWWINDOW at this time");

  waitForTabOpen({
    message: {
      title: "test_open_when_open_new_window_by_pref",
      param: "width=400,height=400",
    },
    finalizeFn: function () {
      Services.prefs.clearUserPref(PREF_NAME);
    },
  });
}

// Test for the pref, "browser.link.open_newwindow.disabled_in_fullscreen"
function test_open_with_pref_to_disable_in_fullscreen() {
  Services.prefs.setBoolPref(PREF_DISABLE_OPEN_NEW_WINDOW, false);

  waitForWindowOpen({
    message: {
      title: "test_open_with_pref_disabled_in_fullscreen",
      param: "width=400,height=400",
    },
    finalizeFn: function () {
      Services.prefs.setBoolPref(PREF_DISABLE_OPEN_NEW_WINDOW, true);
    },
  });
}


// Test for window.open() called from chrome context.
function test_open_from_chrome() {
  waitForWindowOpenFromChrome({
    message: {
      title: "test_open_from_chrome",
      param: "",
    },
    finalizeFn: function () {}
  });
}

function waitForTabOpen(aOptions) {
  let start = Date.now();
  let message = aOptions.message;

  if (!message.title) {
    ok(false, "Can't get message.title.");
    aOptions.finalizeFn();
    runNextTest();
    return;
  }

  info("Running test: " + message.title);

  let onTabOpen = function onTabOpen(aEvent) {
    gBrowser.tabContainer.removeEventListener("TabOpen", onTabOpen, true);

    let tab = aEvent.target;
    tab.linkedBrowser.addEventListener("load", function onLoad(ev){
      let browser = ev.currentTarget;
      browser.removeEventListener("load", onLoad, true, true);

      is(browser.contentWindow.document.title, message.title,
         "Opened Tab is expected: " + message.title);

      if (aOptions.successFn) {
        aOptions.successFn();
      }

      gBrowser.removeTab(tab);
      finalize();
    }, true, true);
  }
  gBrowser.tabContainer.addEventListener("TabOpen", onTabOpen, true);

  let finalize = function () {
    aOptions.finalizeFn();
    info("Finished: " + message.title);
    runNextTest();
  };

  const URI = "data:text/html;charset=utf-8,<!DOCTYPE html><html><head><title>"+
              message.title +
              "<%2Ftitle><%2Fhead><body><%2Fbody><%2Fhtml>";

  executeWindowOpenInContent({
    uri: URI,
    title: message.title,
    option: message.param,
  });
}


function waitForWindowOpen(aOptions) {
  let start = Date.now();
  let message = aOptions.message;
  let url = aOptions.url || getBrowserURL();

  if (!message.title) {
    ok(false, "Can't get message.title");
    aOptions.finalizeFn();
    runNextTest();
    return;
  }

  info("Running test: " + message.title);

  let onFinalize = function () {
    aOptions.finalizeFn();

    info("Finished: " + message.title);
    runNextTest();
  };

  let listener = new WindowListener(message.title, url, {
    onSuccess: aOptions.successFn,
    onFinalize: onFinalize,
  });
  Services.wm.addListener(listener);

  const URI = aOptions.url || "about:blank";

  executeWindowOpenInContent({
    uri: URI,
    title: message.title,
    option: message.param,
  });
}

function executeWindowOpenInContent(aParam) {
  var testWindow = gBrowser.selectedBrowser.contentWindow;
  var testElm = testWindow.document.getElementById("test");

  testElm.setAttribute("data-test-param", JSON.stringify(aParam));
  EventUtils.synthesizeMouseAtCenter(testElm, {}, testWindow);
}

function waitForWindowOpenFromChrome(aOptions) {
  let start = Date.now();
  let message = aOptions.message;
  let url = aOptions.url || getBrowserURL();

  if (!message.title) {
    ok(false, "Can't get message.title");
    aOptions.finalizeFn();
    runNextTest();
    return;
  }

  info("Running test: " + message.title);

  let onFinalize = function () {
    aOptions.finalizeFn();

    info("Finished: " + message.title);
    runNextTest();
  };

  let listener = new WindowListener(message.title, url, {
    onSuccess: aOptions.successFn,
    onFinalize: onFinalize,
  });
  Services.wm.addListener(listener);


  const URI = aOptions.url || "about:blank";

  let testWindow = window.open(URI, message.title, message.option);
}

function WindowListener(aTitle, aUrl, aCallBackObj) {
  this.test_title = aTitle;
  this.test_url = aUrl;
  this.callback_onSuccess = aCallBackObj.onSuccess;
  this.callBack_onFinalize = aCallBackObj.onFinalize;
}
WindowListener.prototype = {

  test_title: null,
  test_url: null,
  callback_onSuccess: null,
  callBack_onFinalize: null,

  onOpenWindow: function(aXULWindow) {
    Services.wm.removeListener(this);

    let domwindow = aXULWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                    .getInterface(Ci.nsIDOMWindow);
    domwindow.addEventListener("load", function onLoad(aEvent) {
      is(domwindow.document.location.href, this.test_url,
        "Opened Window is expected: "+ this.test_title);
      if (this.callback_onSuccess) {
        this.callback_onSuccess();
      }

      domwindow.removeEventListener("load", onLoad, true);

      // wait for trasition to fullscreen on OSX Lion later
      if (isOSX) {
        setTimeout(function(){
          domwindow.close();
          executeSoon(this.callBack_onFinalize);
        }.bind(this), 3000);
      }
      else {
        domwindow.close();
        executeSoon(this.callBack_onFinalize);
      }
    }.bind(this), true);
  },
  onCloseWindow: function(aXULWindow) {},
  onWindowTitleChange: function(aXULWindow, aNewTitle) {},
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIWindowMediatorListener,
                                         Ci.nsISupports]),
};
