onmessage = async ev => {
  if (ev.data !== "show") {
    return;
  }
  const shown = await self.registration.showNotification("title").then(
    () => true,
    () => false
  );
  const clients = await self.clients.matchAll({ includeUncontrolled: true });
  for (let client of clients) {
    client.postMessage({ shown });
  }
};
