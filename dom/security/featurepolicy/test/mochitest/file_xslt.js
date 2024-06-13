let features = [
  "camera",
  "fullscreen",
  "gamepad",
  "geolocation",
  "microphone",
  "storage-access",
].map(f => [f, document.featurePolicy.getAllowlistForFeature(f)]);

document.documentElement.requestFullscreen().finally(() => {
  let fullscreen = document.fullscreenElement == document.documentElement;
  document.exitFullscreen().finally(() => {
    parent.postMessage({ fullscreen, features }, "*");
  });
});
