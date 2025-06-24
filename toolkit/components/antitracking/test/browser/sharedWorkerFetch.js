self.addEventListener("connect", function (e) {
  const port = e.ports[0];
  port.start();

  port.addEventListener("message", async function (e) {
    switch (e.data.type) {
      case "Fetch":
        try {
          let response = await fetch(e.data.url);
          if (response.ok) {
            port.postMessage({ type: "FetchResult", success: true });
          } else {
            port.postMessage({ type: "FetchResult", success: false });
          }
        } catch (error) {
          port.postMessage({ type: "FetchResult", success: false });
        }
    }
  });

  port.start(); // Start the port to begin receiving messages
});
