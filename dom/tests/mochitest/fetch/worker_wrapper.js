importScripts("utils.js");
var client;
var context;

function ok(a, msg) {
  client.postMessage({type: 'status', status: !!a,
                      msg: a + ": " + msg, context: context});
}

function is(a, b, msg) {
  client.postMessage({type: 'status', status: a === b,
                      msg: a + " === " + b + ": " + msg, context: context});
}

addEventListener('message', function workerWrapperOnMessage(e) {
  removeEventListener('message', workerWrapperOnMessage);
  var data = e.data;

  function loadTest() {
    var done = function() {
      client.postMessage({ type: 'finish', context: context });
    }

    try {
      importScripts(data.script);
      // runTest() is provided by the test.
      runTest().then(done, done);
    } catch(e) {
      client.postMessage({
        type: 'status',
        status: false,
        msg: 'worker failed to import ' + data.script + "; error: " + e.message,
        context: context
      });
      done();
    }
  }

  if ("ServiceWorker" in self) {
    self.clients.matchAll().then(function(clients) {
      for (var i = 0; i < clients.length; ++i) {
        if (clients[i].url.indexOf("message_receiver.html") > -1) {
          client = clients[i];
          break;
        }
      }
      if (!client) {
        dump("We couldn't find the message_receiver window, the test will fail\n");
      }
      context = "ServiceWorker";
      loadTest();
    });
  } else {
    client = self;
    context = "Worker";
    loadTest();
  }
});
