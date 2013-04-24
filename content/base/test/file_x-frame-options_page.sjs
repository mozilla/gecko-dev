// SJS file for X-Frame-Options mochitests
function handleRequest(request, response)
{
  var query = {};
  var BOUNDARY = "BOUNDARYOMG3984";
  request.queryString.split('&').forEach(function (val) {
    var [name, value] = val.split('=');
    query[name] = unescape(value);
  });

  if (query['multipart'] == "1") {
    response.setHeader("Content-Type", "multipart/x-mixed-replace;boundary=" + BOUNDARY, false);
    response.setHeader("Cache-Control", "no-cache", false);
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.write("--" + BOUNDARY + "\r\n");
    response.write("Content-Type: text/html\r\n\r\n");
  } else {
    response.setHeader("Content-Type", "text/html", false);
    response.setHeader("Cache-Control", "no-cache", false);
  }

  // X-Frame-Options header value
  if (query['xfo'] == "deny") {
    response.setHeader("X-Frame-Options", "DENY", false);
  }
  else if (query['xfo'] == "sameorigin") {
    response.setHeader("X-Frame-Options", "SAMEORIGIN", false);
  }
  else if (query['xfo'] == "sameorigin2") {
    response.setHeader("X-Frame-Options", "SAMEORIGIN, SAMEORIGIN", false);
  }
  else if (query['xfo'] == "sameorigin3") {
    response.setHeader("X-Frame-Options", "SAMEORIGIN,SAMEORIGIN , SAMEORIGIN", false);
  }
  else if (query['xfo'] == "mixedpolicy") {
    response.setHeader("X-Frame-Options", "DENY,SAMEORIGIN", false);
  }
  else if (query['xfo'] == "afa") {
    response.setHeader("X-Frame-Options", "ALLOW-FROM http://mochi.test:8888/", false);
  }
  else if (query['xfo'] == "afd") {
    response.setHeader("X-Frame-Options", "ALLOW-FROM http://example.com/", false);
  }

  // from the test harness we'll be checking for the presence of this element
  // to test if the page loaded
  response.write("<h1 id=\"test\">" + query["testid"] + "</h1>");

  if (query['multipart'] == "1") {
    response.write("\r\n--" + BOUNDARY + "\r\n");
  }
}
