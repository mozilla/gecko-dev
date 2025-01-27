onmessage = function (e) {
  const message = e.data;

  if (message === "IsETPAllowListed") {
    sendMessage(!isOffscreenAPlaceHolder());
  }
};

function isOffscreenAPlaceHolder() {
  const canvas = new OffscreenCanvas(4, 4);
  const context = canvas.getContext("2d");
  // An empty canvas would return 0,0,0,0 for the pixel data.
  return !context.getImageData(0, 0, 4, 4).data.every(el => el === 0);
}

function sendMessage(message) {
  self.clients.matchAll({ includeUncontrolled: true }).then(function (res) {
    if (!res.length) {
      dump("Error: no clients are available.\n");
      return;
    }
    res[0].postMessage(message);
  });
}
