let { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs"
);

function generateResponse(secure) {
  return `
    <!DOCTYPE html>
    <html>
    <body>
    <h1 id="welcome" style="color: ${secure == "SECURE" ? "green" : "red"}">Welcome to our ${secure} site!</h1>
    <script type="application/javascript">
    </script>
    </body>
    </html>`;
}

function handleRequest(request, response) {
  response.setHeader("Cache-Control", "no-cache", false);
  response.processAsync();

  if (request.scheme === "https") {
    setTimeout(function () {
      response.write(generateResponse("SECURE"));
      response.finish();
    }, 5000);
  } else {
    response.write(generateResponse("INsecure"));
    response.finish();
  }
}
