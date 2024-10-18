// This file is a simple "server" for test_webvtt_resistfingerprinting.html

// You can request a VTT file by setting the `request` parameter to `vtt` and
// providing an `id` parameter. Id parameter is used to track how many times
// a specific test has requested the file. Please don't use the same id for
// multiple tests.

// You can also request the number of times a specific test has requested the
// file by setting the `request` parameter to `count` and providing an `id`
// parameter.

const vtt = `WEBVTT
REGION
id:testOne lines:2 width:30%
REGION
id:testTwo lines:4 width:20%

1
00:00.500 --> 00:00.700 region:testOne
This

2
00:01.200 --> 00:02.400 region:testTwo
Is

2.5
00:02.000 --> 00:03.500 region:testOne
(Over here?!)

3
00:02.710 --> 00:02.910
A

4
00:03.217 --> 00:03.989
Test

5
00:03.217 --> 00:03.989
And more!
`;

// stolen from server-stream-download.sjs# and they stole it from file_blocked_script.sjs
function setGlobalState(data, key) {
  const x = {
    data,
    QueryInterface(_iid) {
      return this;
    },
  };
  x.wrappedJSObject = x;
  setObjectState(key, x);
}

function getGlobalState(key) {
  let data;
  getObjectState(key, function (x) {
    data = x && x.wrappedJSObject.data;
  });
  return data;
}

const requestCounter = (() => {
  const keyPrefix = "vtt-request-counter-";

  return {
    recordRequest(id) {
      const key = keyPrefix + id;
      const count = getGlobalState(key) || 0;
      setGlobalState(count + 1, key);
    },
    getRequestCount(id) {
      const key = keyPrefix + id;
      return getGlobalState(key) || 0;
    },
  };
})();

// We need this for test-verify jobs. It runs the test
// multiple times and we need to know how many times
// the test has been run because global state is not
// reset between runs.
const iterationCounter = (() => {
  const keyPrefix = "vtt-request-iteration-counter-";

  return {
    recordIteration() {
      const count = getGlobalState(keyPrefix) || 0;
      setGlobalState(count + 1, keyPrefix);
    },
    getIterationCount() {
      return getGlobalState(keyPrefix) || 0;
    },
  };
})();

function handleRequest(aRequest, aResponse) {
  aResponse.setHeader("Access-Control-Allow-Origin", "*", false);

  const params = aRequest.queryString
    .split("&")
    .map(command => command.split("="))
    .reduce((acc, [key, value]) => {
      acc[key] = value;
      return acc;
    }, {});

  if (params.request === "vtt") {
    requestCounter.recordRequest(params.id);
    aResponse.setStatusLine(aRequest.httpVersion, 200);
    aResponse.setHeader("Content-Type", "text/vtt", false);
    aResponse.write(vtt);
  } else if (params.request === "count") {
    aResponse.setStatusLine(aRequest.httpVersion, 200);
    aResponse.write(requestCounter.getRequestCount(params.id));
  } else if (params.request === "newIteration") {
    iterationCounter.recordIteration();
    aResponse.setStatusLine(aRequest.httpVersion, 200);
    aResponse.write(iterationCounter.getIterationCount());
  } else {
    aResponse.setStatusLine(aRequest.httpVersion, 400);
    aResponse.write(aRequest.queryString);
  }
}
