importScripts('/resources/testharness.js');
importScripts('/resources/test262harness.js');

onmessage = function(e) {
    let script = e.data[0];
    try {
        eval(script);
        postMessage(["ok"]);
    } catch (e) {
        throw Error(e.message);
    }
}
