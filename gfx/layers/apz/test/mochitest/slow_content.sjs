// Make sure our timer stays alive.
let gTimer;

function handleRequest(request, response) {
  response.processAsync();

  response.setHeader("Content-Type", "text/html", false);
  response.setStatusLine(request.httpVersion, 200, "OK");

  // The id to be used for `getLastContentDisplayportFor`,
  // `scrollbar-width: none` is to avoid the scrollbar area is excluded from
  // the displayport.
  response.write(
    "<html id='iframe-html' style='scrollbar-width: none; height: 200vh;'>"
  );
  response.write("<script src='/tests/SimpleTest/SimpleTest.js'></script>");
  response.write("<script src='apz_test_utils.js'></script>");
  response.write("<body style='margin: 0; padding: 0;'>");

  // Send a bunch of block elements to make the content vertically scrollable.
  for (let i = 0; i < 100; ++i) {
    response.write("<p>Some text.</p>");
  }

  // Flush above changes first.
  response.write(
    "<script>document.documentElement.getBoundingClientRect();</script>"
  );

  // Now it's time to start the test.
  response.write("<script>window.parent.postMessage('ready', '*');</script>");

  gTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
  gTimer.init(
    () => {
      response.write("</body>");
      response.write("</html>");
      response.finish();
    },
    3000,
    Ci.nsITimer.TYPE_ONE_SHOT
  );
}
