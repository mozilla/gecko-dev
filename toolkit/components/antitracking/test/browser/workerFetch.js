self.addEventListener("message", async function (e) {
  switch (e.data.type) {
    case "Fetch":
      try {
        let response = await fetch(e.data.url);
        if (response.ok) {
          self.postMessage({ type: "FetchResult", success: true });
        } else {
          self.postMessage({ type: "FetchResult", success: false });
        }
      } catch (error) {
        self.postMessage({ type: "FetchResult", success: false });
      }
  }
});
