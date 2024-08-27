//https://bugzilla.mozilla.org/show_bug.cgi?id=1706351

// Step 1. Send request with redirect queryString (eg. file_redirect.sjs?302)
// Step 2. Server responds with corresponding redirect code to http://example.com/../file_redirect.sjs?check
// Step 3. Response from ?check indicates whether the redirected request was secure or not.

const RESPONSE_ERROR = "unexpected-query";

// An onload postmessage to window opener
const RESPONSE_SECURE = `
  <html>
  <body>
  send onload message...
  <script type="application/javascript">
    window.opener.postMessage({result: 'secure'}, '*');
  </script>
  </body>
  </html>`;

const RESPONSE_INSECURE = `
  <html>
  <body>
  send onload message...
  <script type="application/javascript">
    window.opener.postMessage({result: 'insecure'}, '*');
  </script>
  </body>
  </html>`;

function redirectMeta(targetUri) {
  return `<html>
<head>
  <meta http-equiv="refresh" content="0; url='${targetUri}'">
</head>
<body>
  META REDIRECT
</body>
</html>`;
}

function redirectJs(targetUri) {
  return `<html>
<body>
  JS REDIRECT
  <script>
    let url= "${targetUri}";
    window.location = url;
  </script>
</body>
</html>`;
}

const CROSS_ORIGIN_REDIRECT =
  "https://example.net/tests/dom/security/test/https-first/file_redirect_error.sjs?check";
const SAME_ORIGIN_REDIRECT =
  "https://example.com/tests/dom/security/test/https-first/file_redirect_error.sjs?check";
const DOWNGRADE_SECURE =
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  "http://example.com/tests/dom/security/test/https-first/file_redirect_error.sjs?downgrade-302";
const START_TEST =
  "https://example.com/tests/dom/security/test/https-first/file_redirect_error.sjs?cross-302";

function handleRequest(request, response) {
  response.setHeader("Cache-Control", "no-cache", false);

  const secure = request.scheme == "https";

  const query = request.queryString.split("-");
  // allow specifying different target uris
  let targetUri = null;
  switch (query[0]) {
    case "cross":
      if (secure) {
        targetUri = CROSS_ORIGIN_REDIRECT;
      }
      break;
    case "same":
      if (secure) {
        targetUri = SAME_ORIGIN_REDIRECT;
      }
      break;
    case "downgrade":
      if (secure) {
        targetUri = DOWNGRADE_SECURE;
      } else {
        targetUri = START_TEST;
      }
      dump("request:" + request.scheme + "\n");
      dump("redirect:" + targetUri + "\n");
      break;
    case "check":
      break;
    default:
      // This should not happen
      response.setStatusLine(request.httpVersion, 500, "OK");
      response.write(RESPONSE_ERROR);
      return;
  }
  let method = query[1];

  // send redirect if requested
  if (targetUri != null && method == "302") {
    response.setStatusLine(request.httpVersion, 302, "Found");
    response.setHeader("Location", targetUri, false);
    return;
  }
  if (targetUri != null && method == "js") {
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.write(redirectJs(targetUri));
    return;
  }
  if (targetUri != null && method == "meta") {
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.write(redirectMeta(targetUri));
    return;
  }

  // Check if scheme is http:// or https://
  if (query == "check") {
    response.setStatusLine(request.httpVersion, 400, "Error");
    response.write(secure ? RESPONSE_SECURE : RESPONSE_INSECURE);
    return;
  }

  // This should not happen
  response.setStatusLine(request.httpVersion, 500, "OK");
  response.write(RESPONSE_ERROR);
}
