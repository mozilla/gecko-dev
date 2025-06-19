onnotificationclick = ev => {
  ev.waitUntil(
    (async () => {
      const client = await clients.openWindow("file_openwindow.html");
      client.postMessage("HELLO");
    })()
  );
};
