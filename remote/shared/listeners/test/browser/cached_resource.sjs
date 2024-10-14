/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

function handleRequest(request, response) {
  response.setHeader(
    "Cache-Control",
    "no-transform,public,max-age=300,s-maxage=900"
  );
  response.setHeader("Expires", "Thu, 01 Dec 2100 20:00:00 GMT");

  const params = new Map(
    request.queryString
      .replace("?", "")
      .split("&")
      .map(s => s.split("="))
  );

  switch (params.get("type")) {
    case "stylesheet": {
      response.setHeader("Content-Type", "text/css", false);
      response.write("body { background-color: black; }");
      break;
    }
    case "script": {
      response.setHeader("Content-Type", "text/javascript", false);
      response.write("window.scriptLoaded = true;");
      break;
    }
    case "image": {
      response.setHeader("Content-Type", "image/png", false);
      response.write(
        atob(
          "iVBORw0KGgoAAAANSUhEUgAAAAUAAAAFCAYAAACNbyblAAAAHElEQVQI12" +
            "P4//8/w38GIAXDIBKE0DHxgljNBAAO9TXL0Y4OHwAAAABJRU5ErkJggg=="
        )
      );
      break;
    }
    default: {
      throw new Error(
        "Expecting type parameter to be one of script, stylesheet or image"
      );
    }
  }
}
