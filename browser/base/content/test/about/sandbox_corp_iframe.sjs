/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

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

  response.setHeader("Content-Type", "text/html", false);

  let destination = "dummy_page";

  switch (params.error) {
    case "coop": {
      destination = "sandbox_corp_popup";
      response.setHeader("Cross-Origin-Opener-Policy", "same-origin", false);
      break;
    }
    case "coep": {
      response.setHeader("Cross-Origin-Opener-Policy", "same-origin", false);
      response.setHeader("Cross-Origin-Embedder-Policy", "require-corp", false);
      break;
    }
    case "inner_coop": // Only called from popup.html
      response.setHeader("Cross-Origin-Opener-Policy", "same-origin", false);
      return;
    default:
      return;
  }

  let txt = `<html><body><iframe src="https://example.com/browser/browser/base/content/test/about/${destination}.html" width=100% height=100% sandbox="allow-popups allow-scripts allow-same-origin"></iframe></body></html>`;
  response.write(txt);
}
