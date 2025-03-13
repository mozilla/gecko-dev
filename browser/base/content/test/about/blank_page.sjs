/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function decode(str) {
  return decodeURIComponent(str.replace(/\+/g, encodeURIComponent(" ")));
}

function handleRequest(request, response) {
  const queryString = request.queryString;
  let params = queryString.split("&").reduce((memo, pair) => {
    let [key, val] = pair.split("=");
    if (!val) {
      val = key;
      key = "query";
    }

    try {
      memo[decode(key)] = decode(val);
    } catch (e) {
      memo[key] = val;
    }

    return memo;
  }, {});

  const status = parseInt(params.status);
  const message = params.message;
  const header = params.header;

  // Set default if missing parameters
  if (!status || !message) {
    response.setStatusLine(request.httpVersion, 400, "Bad Request");
    response.setHeader("Content-Length", "0", false);
    return;
  }

  if (header === "hide") {
    response.setStatusLine(request.httpVersion, status, message);
    return;
  }
  if (header === "lie") {
    response.setStatusLine(request.httpVersion, status, message);
    response.setHeader("Content-Length", "100", false);
    return;
  }
  response.setStatusLine(request.httpVersion, status, message);
  response.setHeader("Content-Length", "0", false);
}
