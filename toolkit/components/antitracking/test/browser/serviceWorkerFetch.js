self.addEventListener("activate", e => {
  e.waitUntil(self.clients.claim());
});

self.addEventListener("message", e => {
  switch (e.data.type) {
    case "Fetch":
      e.waitUntil(
        fetch(e.data.url)
          .then(() => {
            // Send success message back to source client
            e.source.postMessage({ type: "FetchResult", success: true });
          })
          .catch(_ => {
            // Send error message back to source client
            e.source.postMessage({
              type: "FetchResult",
              success: false,
            });
          })
      );
      break;
  }
});
