let ROOT = getRootDirectory(gTestPath).replace("chrome://mochitests/content", "http://example.com");
const searchBundle = Services.strings.createBundle("chrome://global/locale/search/search.properties");
const brandBundle = Services.strings.createBundle("chrome://branding/locale/brand.properties");
const brandName = brandBundle.GetStringFromName("brandShortName");

function getString(key, ...params) {
  return searchBundle.formatStringFromName(key, params, params.length);
}

function AddSearchProvider(...args) {
  return gBrowser.addTab(ROOT + "webapi.html?AddSearchProvider:" + encodeURIComponent(JSON.stringify(args)));
}

function addSearchEngine(...args) {
  return gBrowser.addTab(ROOT + "webapi.html?addSearchEngine:" + encodeURIComponent(JSON.stringify(args)));
}

function promiseDialogOpened() {
  return new Promise((resolve, reject) => {
    Services.wm.addListener({
      onOpenWindow: function(xulWin) {
        Services.wm.removeListener(this);

        let win = xulWin.QueryInterface(Ci.nsIInterfaceRequestor)
                        .getInterface(Ci.nsIDOMWindow);
        waitForFocus(() => {
          if (win.location == "chrome://global/content/commonDialog.xul")
            resolve(win)
          else
            reject();
        }, win);
      }
    });
  });
}

add_task(function* test_working_AddSearchProvider() {
  gBrowser.selectedTab = AddSearchProvider(ROOT + "testEngine.xml");

  let dialog = yield promiseDialogOpened();
  is(dialog.args.promptType, "confirmEx", "Should see the confirmation dialog.");
  is(dialog.args.text, getString("addEngineConfirmation", "Foo", "example.com"),
     "Should have seen the right install message");
  dialog.document.documentElement.cancelDialog();

  gBrowser.removeCurrentTab();
});

add_task(function* test_HTTP_AddSearchProvider() {
  gBrowser.selectedTab = AddSearchProvider(ROOT.replace("http:", "HTTP:") + "testEngine.xml");

  let dialog = yield promiseDialogOpened();
  is(dialog.args.promptType, "confirmEx", "Should see the confirmation dialog.");
  is(dialog.args.text, getString("addEngineConfirmation", "Foo", "example.com"),
     "Should have seen the right install message");
  dialog.document.documentElement.cancelDialog();

  gBrowser.removeCurrentTab();
});

add_task(function* test_relative_AddSearchProvider() {
  gBrowser.selectedTab = AddSearchProvider("testEngine.xml");

  let dialog = yield promiseDialogOpened();
  is(dialog.args.promptType, "confirmEx", "Should see the confirmation dialog.");
  is(dialog.args.text, getString("addEngineConfirmation", "Foo", "example.com"),
     "Should have seen the right install message");
  dialog.document.documentElement.cancelDialog();

  gBrowser.removeCurrentTab();
});

add_task(function* test_invalid_AddSearchProvider() {
  gBrowser.selectedTab = AddSearchProvider("z://foobar");

  let dialog = yield promiseDialogOpened();
  is(dialog.args.promptType, "alert", "Should see the alert dialog.");
  is(dialog.args.text, getString("error_invalid_engine_msg", brandName),
     "Should have seen the right error message")
  dialog.document.documentElement.acceptDialog();

  gBrowser.removeCurrentTab();
});

add_task(function* test_missing_AddSearchProvider() {
  let url = ROOT + "foobar.xml";
  gBrowser.selectedTab = AddSearchProvider(url);

  let dialog = yield promiseDialogOpened();
  is(dialog.args.promptType, "alert", "Should see the alert dialog.");
  is(dialog.args.text, getString("error_loading_engine_msg2", brandName, url),
     "Should have seen the right error message")
  dialog.document.documentElement.acceptDialog();

  gBrowser.removeCurrentTab();
});

add_task(function* test_working_addSearchEngine_xml() {
  gBrowser.selectedTab = addSearchEngine(ROOT + "testEngine.xml", "", "", "");

  let dialog = yield promiseDialogOpened();
  is(dialog.args.promptType, "confirmEx", "Should see the confirmation dialog.");
  is(dialog.args.text, getString("addEngineConfirmation", "Foo", "example.com"),
     "Should have seen the right install message");
  dialog.document.documentElement.cancelDialog();

  gBrowser.removeCurrentTab();
});

add_task(function* test_working_addSearchEngine_src() {
  gBrowser.selectedTab = addSearchEngine(ROOT + "testEngine.src", "", "", "");

  let dialog = yield promiseDialogOpened();
  is(dialog.args.promptType, "confirmEx", "Should see the confirmation dialog.");
  is(dialog.args.text, getString("addEngineConfirmation", "Test Sherlock", "example.com"),
     "Should have seen the right install message");
  dialog.document.documentElement.cancelDialog();

  gBrowser.removeCurrentTab();
});

add_task(function* test_relative_addSearchEngine_xml() {
  gBrowser.selectedTab = addSearchEngine("testEngine.xml", "", "", "");

  let dialog = yield promiseDialogOpened();
  is(dialog.args.promptType, "confirmEx", "Should see the confirmation dialog.");
  is(dialog.args.text, getString("addEngineConfirmation", "Foo", "example.com"),
     "Should have seen the right install message");
  dialog.document.documentElement.cancelDialog();

  gBrowser.removeCurrentTab();
});

add_task(function* test_relative_addSearchEngine_src() {
  gBrowser.selectedTab = addSearchEngine("testEngine.src", "", "", "");

  let dialog = yield promiseDialogOpened();
  is(dialog.args.promptType, "confirmEx", "Should see the confirmation dialog.");
  is(dialog.args.text, getString("addEngineConfirmation", "Test Sherlock", "example.com"),
     "Should have seen the right install message");
  dialog.document.documentElement.cancelDialog();

  gBrowser.removeCurrentTab();
});

add_task(function* test_invalid_addSearchEngine() {
  gBrowser.selectedTab = addSearchEngine("z://foobar", "", "", "");

  let dialog = yield promiseDialogOpened();
  is(dialog.args.promptType, "alert", "Should see the alert dialog.");
  is(dialog.args.text, getString("error_invalid_engine_msg", brandName),
     "Should have seen the right error message")
  dialog.document.documentElement.acceptDialog();

  gBrowser.removeCurrentTab();
});

add_task(function* test_invalid_icon_addSearchEngine() {
  gBrowser.selectedTab = addSearchEngine(ROOT + "testEngine.src", "z://foobar", "", "");

  let dialog = yield promiseDialogOpened();
  is(dialog.args.promptType, "alert", "Should see the alert dialog.");
  is(dialog.args.text, getString("error_invalid_engine_msg", brandName),
     "Should have seen the right error message")
  dialog.document.documentElement.acceptDialog();

  gBrowser.removeCurrentTab();
});

add_task(function* test_missing_addSearchEngine() {
  let url = ROOT + "foobar.xml";
  gBrowser.selectedTab = addSearchEngine(url, "", "", "");

  let dialog = yield promiseDialogOpened();
  is(dialog.args.promptType, "alert", "Should see the alert dialog.");
  is(dialog.args.text, getString("error_loading_engine_msg2", brandName, url),
     "Should have seen the right error message")
  dialog.document.documentElement.acceptDialog();

  gBrowser.removeCurrentTab();
});
