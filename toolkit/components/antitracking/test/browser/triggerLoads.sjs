function handleRequest(aRequest, aResponse) {
  aResponse.setStatusLine(aRequest.httpVersion, 200);
  aResponse.setHeader("Content-Type", "text/javascript", false);
  aResponse.setHeader("Access-Control-Allow-Origin", "*");

  let params = new URLSearchParams(aRequest.queryString);
  let type = params.get("type");
  let url = params.get("url");

  switch (type) {
    case "XHR":
      aResponse.write(`
        let xhr = new XMLHttpRequest();
        xhr.open("GET", "${url}");
        xhr.send();
      `);
      break;

    case "Fetch":
      aResponse.write(`
        fetch("${url}");
      `);
      break;

    case "Image":
      aResponse.write(`
        let img = document.createElement("img");
        img.src = "${url}";
        document.body.appendChild(img);
      `);
      break;

    case "CSS":
      aResponse.write(`
        let link = document.createElement("link");
        link.rel = "stylesheet";
        link.href = "${url}";
        document.head.appendChild(link);
      `);
      break;

    case "Video":
      aResponse.write(`
        let video = document.createElement("video");
        video.src = "${url}";
        document.body.appendChild(video);
      `);
      break;

    case "Audio":
      aResponse.write(`
        let audio = document.createElement("audio");
        audio.src = "${url}";
        document.body.appendChild(audio);
      `);
      break;

    case "Iframe":
      aResponse.write(`
        let iframe = document.createElement("iframe");
        iframe.src = "${url}";
        document.body.appendChild(iframe);
      `);
      break;

    case "Script":
      aResponse.write(`
        let script = document.createElement("script");
        script.src = "${url}";
        document.body.appendChild(script);
      `);
      break;

    case "WebSocket":
      aResponse.write(`
        let ws = new WebSocket(
          "ws://mochi.test:8888/browser/toolkit/components/antitracking/test/browser/file_ws_handshake_delay",
          ["test"]
        );
      `);
      break;

    default:
      throw new Error("Unsupported load type: " + aRequest.queryString);
  }
}
