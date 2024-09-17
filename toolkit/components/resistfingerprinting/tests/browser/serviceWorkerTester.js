onmessage = e => {
  let runnableStr = `(() => {return (${e.data.callback});})();`;
  let runnable = eval(runnableStr); // eslint-disable-line no-eval

  runnable.call(this).then(async result => {
    self.clients.matchAll({ includeUncontrolled: true }).then(clients => {
      if (clients && clients.length) {
        clients[0].postMessage({ result });
      }
    });
  });
};
