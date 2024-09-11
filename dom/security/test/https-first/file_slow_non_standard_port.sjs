// Custom *.sjs file specifically for the needs of Bug 1709552
"use strict";

const RESPONSE_SUCCESS = `
  <html>
    <body>
      send message, downgraded
    <script type="application/javascript">
      let scheme = document.location.protocol;
      window.opener.postMessage({result: 'downgraded', scheme: scheme}, '*');
    </script>
    </body>
  </html>`;

const RESPONSE_UNEXPECTED = `
  <html>
    <body>
      send message, error
    <script type="application/javascript">
      let scheme = document.location.protocol;
      window.opener.postMessage({result: 'Error', scheme: scheme }, '*');
    </script>
    </body>
  </html>`;

function handleRequest(request, response) {
  // avoid confusing cache behaviour
  response.setHeader("Cache-Control", "no-cache", false);
  response.setHeader("Content-Type", "text/html", false);

  let query = request.queryString;
  // If the scheme is not https then we rather fall through and display unexpected content
  if (query.scheme === "https") {
    // We should never arrive here, just in case send something unexpected
    response.write(RESPONSE_UNEXPECTED);
    return;
  }
  // Use a busy loop and slow down the response by one second
  const delayMs = 1000;
  const start = Date.now();
  while (Date.now() < start + delayMs) {
    continue;
  }
  // We should arrive here when the redirection was downraded successful
  response.write(RESPONSE_SUCCESS);
}
