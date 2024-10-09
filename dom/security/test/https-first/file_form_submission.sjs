const CC = Components.Constructor;
const BinaryInputStream = CC(
  "@mozilla.org/binaryinputstream;1",
  "nsIBinaryInputStream",
  "setInputStream"
);

function makeResponse(success) {
  let res = `
  <html>
    <body>
      send message, downgraded
    <script type="application/javascript">
      let scheme = document.location.protocol;
      const loc = document.location.href;
      window.opener.postMessage({location: loc, scheme: scheme, form:"test=${
        success ? "success" : "failure"
      }" }, '*');
    </script>
    </body>
  </html>`;
  return res;
}

function makeForm(method, testID) {
  return `
  <html>
      <body>
          <form action="http://example.com/tests/dom/security/test/https-first/file_form_submission.sjs" method="${method}" id="testform">
              <div>
              <label id="submit">Submit</label>
              <input name="test" id="form" value="${testID}">
              <input name="result" id="form" value="success">
              </div>
          </form>
          <script class="testbody" type="text/javascript">
              document.getElementById("testform").submit();
          </script>
      </body>
  </html>
  `;
}

function handleRequest(request, response) {
  // avoid confusing cache behaviors
  response.setHeader("Cache-Control", "no-cache", false);
  let queryString = request.queryString;
  // Endpoints to return a form
  if (
    (request.scheme === "https" && queryString === "test=1") ||
    (request.scheme === "http" && queryString === "test=2")
  ) {
    response.write(makeForm("GET", queryString.substr(-1, 1)));
    return;
  }
  if (queryString === "test=3" || queryString === "test=4") {
    response.write(makeForm("POST", queryString.substr(-1, 1)));
    return;
  }
  // Endpoints to trigger downgrades because of timeouts
  if (
    request.scheme === "https" &&
    (queryString === "test=2" || queryString === "test=4")
  ) {
    response.processAsync();
    return;
  }
  // Endpoints for receiving the form data
  if (
    request.method == "GET" &&
    ((queryString.includes("test=1") && request.scheme === "https") ||
      queryString.includes("test=2")) &&
    queryString.includes("result=success")
  ) {
    response.write(makeResponse(true));
    return;
  }
  if (request.method == "POST" && request.scheme === "http") {
    // extract form parameters
    let body = new BinaryInputStream(request.bodyInputStream);
    let avail;
    let bytes = [];
    while ((avail = body.available()) > 0) {
      Array.prototype.push.apply(bytes, body.readByteArray(avail));
    }
    let requestBodyContents = String.fromCharCode.apply(null, bytes);

    response.write(
      makeResponse(
        (requestBodyContents.includes("test=3") ||
          requestBodyContents.includes("test=4")) &&
          requestBodyContents.includes("result=success")
      )
    );
    return;
  }
  // we should never get here; just in case, return something unexpected
  response.write(makeResponse(false));
}
