// META: timeout=long
// META: global=worker
// META: script=/common/utils.js
// META: script=/common/get-host-info.sub.js
// META: script=/fetch/api/request/request-error.js

// this adopted from fetch keepalive abort test for windows from /fetch/api/abort/keepalive.html
async function fetchJson(url) {
  const response = await fetch(url);
  assert_true(response.ok, 'response should be ok');
  return response.json();
}

promise_test(async () => {
const stateKey = token();
const controller = new AbortController();
// infinites-slow-response writes a response to the client until client closes connection
// or it receives the abortKey. In our case the we expect the connection to be closed from client,
// i.e. the fetch keep-alive request is aborted.

await fetch(`/fetch/api/resources/infinite-slow-response.py?stateKey=${stateKey}`,
            {
              signal: controller.signal,
              keepalive: true
            });
const before = await fetchJson(`/fetch/api/resources/stash-take.py?key=${stateKey}`);
assert_equals(before, 'open', 'connection should be open');

controller.abort();

// Spin until the abort completes.
while (true) {
  const after = await fetchJson(`/fetch/api/resources/stash-take.py?key=${stateKey}`);
  if (after) {
    // stateKey='open' was removed from the dictionary by the first fetch of
    // stash-take.py, so we should only ever see the value 'closed' here.
    assert_equals(after, 'closed', 'connection should have closed');
    break;
  }
}
}, 'aborting a keepalive worker should work');
