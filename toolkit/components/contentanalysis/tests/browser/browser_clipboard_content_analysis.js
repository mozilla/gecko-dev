/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// This test is used to check copy and paste in editable areas to ensure that non-text
// types (html and images) are copied to and pasted from the clipboard properly.

var testPage =
  "<body style='margin: 0'>" +
  "  <img id='img' tabindex='1' src='http://example.org/browser/browser/base/content/test/general/moz.png'>" +
  "  <div id='main' contenteditable='true'>Test <b>Bold</b> After Text</div>" +
  "</body>";

let mockCA = makeMockContentAnalysis();

add_setup(async function test_setup() {
  mockCA = await mockContentAnalysisService(mockCA);
});

function setClipboardHTMLData(htmlString) {
  const trans = Cc["@mozilla.org/widget/transferable;1"].createInstance(
    Ci.nsITransferable
  );
  trans.init(null);
  trans.addDataFlavor("text/html");
  const str = Cc["@mozilla.org/supports-string;1"].createInstance(
    Ci.nsISupportsString
  );
  str.data = htmlString;
  trans.setTransferData("text/html", str);

  // Write to clipboard.
  Services.clipboard.setData(trans, null, Ci.nsIClipboard.kGlobalClipboard);
}

async function testClipboardWithContentAnalysis(allowPaste) {
  mockCA.setupForTest(allowPaste);
  let tab = BrowserTestUtils.addTab(gBrowser);
  let browser = gBrowser.getBrowserForTab(tab);

  gBrowser.selectedTab = tab;

  await promiseTabLoadEvent(tab, "data:text/html," + escape(testPage));
  await SimpleTest.promiseFocus(browser);

  function sendKey(key, code) {
    return BrowserTestUtils.synthesizeKey(
      key,
      { code, accelKey: true },
      browser
    );
  }

  // On windows, HTML clipboard includes extra data.
  // The values are from widget/windows/nsDataObj.cpp.
  const htmlPrefix = navigator.platform.includes("Win")
    ? "<html><body>\n<!--StartFragment-->"
    : "";
  const htmlPostfix = navigator.platform.includes("Win")
    ? "<!--EndFragment-->\n</body>\n</html>"
    : "";

  await SpecialPowers.spawn(browser, [], () => {
    var doc = content.document;
    var main = doc.getElementById("main");
    main.focus();

    // Select an area of the text.
    let selection = doc.getSelection();
    selection.modify("move", "left", "line");
    selection.modify("move", "right", "character");
    selection.modify("move", "right", "character");
    selection.modify("move", "right", "character");
    selection.modify("extend", "right", "word");
    selection.modify("extend", "right", "word");
  });

  // The data is empty as the selection was copied during the event default phase.
  let copyEventPromise = BrowserTestUtils.waitForContentEvent(
    browser,
    "copy",
    false,
    event => {
      return event.clipboardData.mozItemCount == 0;
    }
  );
  await SpecialPowers.spawn(browser, [], () => {});
  await sendKey("c");
  await copyEventPromise;

  let pastePromise = SpecialPowers.spawn(
    browser,
    [htmlPrefix, htmlPostfix, allowPaste],
    (htmlPrefixChild, htmlPostfixChild, allowPaste) => {
      let selection = content.document.getSelection();
      selection.modify("move", "right", "line");

      return new Promise((resolve, _reject) => {
        content.addEventListener(
          "paste",
          event => {
            let clipboardData = event.clipboardData;
            Assert.equal(
              clipboardData.mozItemCount,
              allowPaste ? 1 : 0,
              "Items on clipboard"
            );
            Assert.equal(
              clipboardData.types.length,
              allowPaste ? 2 : 0,
              "Types on clipboard"
            );
            if (allowPaste) {
              Assert.equal(
                clipboardData.types[0],
                "text/html",
                "text/html on clipboard"
              );
              Assert.equal(
                clipboardData.types[1],
                "text/plain",
                "text/plain on clipboard"
              );
            }
            Assert.equal(
              clipboardData.getData("text/html"),
              allowPaste
                ? htmlPrefixChild + "t <b>Bold</b>" + htmlPostfixChild
                : "",
              "text/html value"
            );
            Assert.equal(
              clipboardData.getData("text/plain"),
              allowPaste ? "t Bold" : "",
              "text/plain value"
            );
            resolve();
          },
          { capture: true, once: true }
        );
      });
    }
  );

  await SpecialPowers.spawn(browser, [], () => {});

  await sendKey("v");
  await pastePromise;
  // Check that the number of calls matches the number of
  // kKnownClipboardTypes on the clipboard.
  is(mockCA.calls.length, 2, "Correct number of calls to Content Analysis");
  assertContentAnalysisRequest(mockCA.calls[0], "t Bold");
  assertContentAnalysisRequest(
    mockCA.calls[1],
    htmlPrefix + "t <b>Bold</b>" + htmlPostfix
  );
  mockCA.clearCalls();

  let copyPromise = SpecialPowers.spawn(browser, [allowPaste], allowPaste => {
    var main = content.document.getElementById("main");

    Assert.equal(
      main.innerHTML,
      allowPaste
        ? "Test <b>Bold</b> After Textt <b>Bold</b>"
        : "Test <b>Bold</b> After Text",
      "Copy and paste html"
    );

    let selection = content.document.getSelection();
    selection.modify("extend", "left", "word");
    selection.modify("extend", "left", "word");
    selection.modify("extend", "left", "character");

    return new Promise((resolve, _reject) => {
      content.addEventListener(
        "cut",
        event => {
          event.clipboardData.setData("text/plain", "Some text");
          event.clipboardData.setData("text/html", "<i>Italic</i> ");
          selection.deleteFromDocument();
          event.preventDefault();
          resolve();
        },
        { capture: true, once: true }
      );
    });
  });

  await SpecialPowers.spawn(browser, [], () => {});

  await sendKey("x");
  await copyPromise;

  pastePromise = SpecialPowers.spawn(
    browser,
    [htmlPrefix, htmlPostfix, allowPaste],
    (htmlPrefixChild, htmlPostfixChild, allowPaste) => {
      let selection = content.document.getSelection();
      selection.modify("move", "left", "line");

      return new Promise((resolve, _reject) => {
        content.addEventListener(
          "paste",
          event => {
            let clipboardData = event.clipboardData;
            Assert.equal(
              clipboardData.mozItemCount,
              allowPaste ? 1 : 0,
              "Items on clipboard 2"
            );
            Assert.equal(
              clipboardData.types.length,
              allowPaste ? 2 : 0,
              "Types on clipboard 2"
            );
            if (allowPaste) {
              Assert.equal(
                clipboardData.types[0],
                "text/html",
                "text/html on clipboard 2"
              );
              Assert.equal(
                clipboardData.types[1],
                "text/plain",
                "text/plain on clipboard 2"
              );
            }
            Assert.equal(
              clipboardData.getData("text/html"),
              allowPaste
                ? htmlPrefixChild + "<i>Italic</i> " + htmlPostfixChild
                : "",
              "text/html value 2"
            );
            Assert.equal(
              clipboardData.getData("text/plain"),
              allowPaste ? "Some text" : "",
              "text/plain value 2"
            );
            resolve();
          },
          { capture: true, once: true }
        );
      });
    }
  );

  await SpecialPowers.spawn(browser, [], () => {});

  await sendKey("v");
  await pastePromise;
  // 2 calls because there are two formats on the clipboard
  is(mockCA.calls.length, 2, "Correct number of calls to Content Analysis");
  assertContentAnalysisRequest(mockCA.calls[0], "Some text");
  assertContentAnalysisRequest(
    mockCA.calls[1],
    htmlPrefix + "<i>Italic</i> " + htmlPostfix
  );
  mockCA.clearCalls();

  await SpecialPowers.spawn(browser, [allowPaste], allowPaste => {
    var main = content.document.getElementById("main");
    Assert.equal(
      main.innerHTML,
      allowPaste
        ? "<i>Italic</i> Test <b>Bold</b> After<b></b>"
        : "Test <b>Bold</b>",
      "Copy and paste html 2"
    );
  });

  // Next, put some HTML data on the clipboard
  setClipboardHTMLData(
    '<img id="img" tabindex="1" src="http://example.org/browser/browser/base/content/test/general/moz.png">'
  );

  // Focus the content again
  await SimpleTest.promiseFocus(browser);

  pastePromise = SpecialPowers.spawn(
    browser,
    [htmlPrefix, htmlPostfix, allowPaste],
    (htmlPrefixChild, htmlPostfixChild, allowPaste) => {
      var doc = content.document;
      var main = doc.getElementById("main");
      main.focus();

      return new Promise((resolve, reject) => {
        content.addEventListener(
          "paste",
          event => {
            let clipboardData = event.clipboardData;

            // DataTransfer doesn't support the image types yet, so only text/html
            // will be present.
            let clipboardText = clipboardData.getData("text/html");
            if (allowPaste) {
              if (
                clipboardText !==
                htmlPrefixChild +
                  '<img id="img" tabindex="1" src="http://example.org/browser/browser/base/content/test/general/moz.png">' +
                  htmlPostfixChild
              ) {
                reject(
                  "Clipboard Data did not contain an image, was " +
                    clipboardText
                );
              }
            } else if (clipboardText !== "") {
              reject("Clipboard Data should be empty, was " + clipboardText);
            }
            resolve();
          },
          { capture: true, once: true }
        );
      });
    }
  );

  await SpecialPowers.spawn(browser, [], () => {});
  await sendKey("v");
  await pastePromise;
  is(mockCA.calls.length, 1, "Correct number of calls to Content Analysis");
  assertContentAnalysisRequest(
    mockCA.calls[0],
    htmlPrefix +
      '<img id="img" tabindex="1" src="http://example.org/browser/browser/base/content/test/general/moz.png">' +
      htmlPostfix
  );
  mockCA.clearCalls();

  // The new content should now include an image.
  await SpecialPowers.spawn(browser, [allowPaste], allowPaste => {
    var main = content.document.getElementById("main");
    Assert.equal(
      main.innerHTML,
      allowPaste
        ? '<i>Italic</i> <img id="img" tabindex="1" ' +
            'src="http://example.org/browser/browser/base/content/test/general/moz.png">' +
            "Test <b>Bold</b> After<b></b>"
        : "Test <b>Bold</b>",
      "Paste after copy image"
    );
  });

  gBrowser.removeCurrentTab();
}

function assertContentAnalysisRequest(request, expectedText) {
  // This page is loaded via a data: URL which has a null principal,
  // so the URL will reflect this.
  ok(
    request.url.spec.startsWith("moz-nullprincipal:"),
    "request has correct moz-nullprincipal URL, got " + request.url.spec
  );
  is(
    request.analysisType,
    Ci.nsIContentAnalysisRequest.eBulkDataEntry,
    "request has correct analysisType"
  );
  is(
    request.operationTypeForDisplay,
    Ci.nsIContentAnalysisRequest.eClipboard,
    "request has correct operationTypeForDisplay"
  );
  is(request.filePath, "", "request filePath should be empty");
  if (expectedText !== null) {
    is(request.textContent, expectedText, "request textContent should match");
  }
  is(request.printDataHandle, 0, "request printDataHandle should not be 0");
  is(request.printDataSize, 0, "request printDataSize should not be 0");
  ok(!!request.requestToken.length, "request requestToken should not be empty");
}
add_task(async function testClipboardWithContentAnalysisAllow() {
  await testClipboardWithContentAnalysis(true);
});

add_task(async function testClipboardWithContentAnalysisBlock() {
  await testClipboardWithContentAnalysis(false);
});
