self.addEventListener("install", function () {
  console.log("install");
  self.skipWaiting();
});

self.addEventListener("activate", function (e) {
  console.log("activate");
  e.waitUntil(self.clients.claim());
});

async function postAll(data) {
  const clients = await self.clients.matchAll({ includeUncontrolled: true });
  for (const client of clients) {
    client.postMessage(data);
  }
}

self.onnotificationclick = function (event) {
  console.log("onnotificationclick");
  postAll(event.action);
  event.notification.close();
};
