onmessage = async ev => {
  if (ev.data !== "show") {
    return;
  }
  const reg = await navigator.serviceWorker.getRegistration();
  const shown = await reg.showNotification("title").then(
    () => true,
    () => false
  );
  self.postMessage({ shown });
};
