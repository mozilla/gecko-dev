/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function test_file_opening() {
  // Get a ref to the pdf we want to open.
  let dirFileObj = getChromeDir(getResolvedURI(gTestPath));
  dirFileObj.append("file_pdfjs_test.pdf");

  // Change the defaults.
  var oldAction = changeMimeHandler(Ci.nsIHandlerInfo.useSystemDefault, true);

  // Test: "Open with" dialog should not come up, despite pdf.js not being
  // the default - because files from disk should always use pdfjs, unless
  // it is forcibly disabled.
  let openedWindow = false;
  const windowOpenedPromise = new Promise(resolve => {
    addWindowListener(
      "chrome://mozapps/content/downloads/unknownContentType.xhtml",
      () => {
        openedWindow = true;
        resolve();
      }
    );
  });

  // Open the tab with a system principal:
  var tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    dirFileObj.path
  );
  await Promise.race([
    windowOpenedPromise,
    waitForSelector(
      tab.linkedBrowser,
      ".textLayer .endOfContent",
      "Wait for text layer."
    ),
  ]);
  ok(!openedWindow, "Shouldn't open an unknownContentType window!");
  await waitForPdfJSClose(tab.linkedBrowser, /* closeTab = */ true);

  // Now try opening it from the file directory:
  tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    dirFileObj.parent.path
  );
  const pdfjsLoadedPromise = BrowserTestUtils.browserLoaded(
    tab.linkedBrowser,
    false,
    url => url.endsWith("test.pdf")
  );
  await SpecialPowers.spawn(tab.linkedBrowser, [], () => {
    content.document.querySelector("a[href$='test.pdf']").click();
  });
  await Promise.race([windowOpenedPromise, pdfjsLoadedPromise]);
  ok(
    !openedWindow,
    "Shouldn't open an unknownContentType window for PDFs from file: links!"
  );

  await waitForSelector(
    tab.linkedBrowser,
    ".textLayer .endOfContent",
    "Wait for text layer."
  );

  registerCleanupFunction(async () => {
    if (listenerCleanup) {
      listenerCleanup();
    }
    changeMimeHandler(oldAction[0], oldAction[1]);
    await waitForPdfJSClose(tab.linkedBrowser, /* closeTab = */ true);
  });
});

let listenerCleanup;
function addWindowListener(aURL, aCallback) {
  let listener = {
    onOpenWindow(aXULWindow) {
      info("window opened, waiting for focus");
      listenerCleanup();
      listenerCleanup = null;

      var domwindow = aXULWindow.docShell.domWindow;
      waitForFocus(function () {
        is(
          domwindow.document.location.href,
          aURL,
          "should have seen the right window open"
        );
        domwindow.close();
        aCallback();
      }, domwindow);
    },
    onCloseWindow() {},
  };
  Services.wm.addListener(listener);
  listenerCleanup = () => Services.wm.removeListener(listener);
}
