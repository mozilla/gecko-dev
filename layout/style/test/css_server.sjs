function handleRequest(request, response) {
  const flavours = new Set(request.queryString.split(","));

  response.setHeader("Content-Type", "text/css", false);

  if (flavours.has("cacheable")) {
    response.setHeader("Cache-Control", "max-age=10000", false);
  } else {
    response.setHeader("Cache-Control", "no-cache", false);
  }

  response.setHeader(
    "Server-Timing",
    `name1, name2;dur=20, name3;desc="desc3";dur=30`,
    false
  );

  if (flavours.has("tao")) {
    response.setHeader("Timing-Allow-Origin", "*", false);
  }

  if (flavours.has("cors")) {
    response.setHeader("Access-Control-Allow-Origin", "*", false);
  }

  if (flavours.has("slow")) {
    response.processAsync();

    const timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    timer.init(
      function () {
        response.write("body:before { content: 'slow'; }");
        response.finish();
      },
      1000,
      Ci.nsITimer.TYPE_ONE_SHOT
    );
  } else {
    response.write("body:before { content: 'non-slow'; }");
  }
}
