/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* globals require, process */

const http = require("http");

const html = `
<!DOCTYPE html>
<html>
  <head>
    <title>Upload test</title>
    <style>
      html {
        font-family: neo-sans;
        font-weight: 700;
        font-size: calc(42rem / 16);
      }
      body {
        background: white;
      }
      section {
        border-radius: 1em;
        padding: 1em;
        position: absolute;
        top: 50%;
        left: 50%;
        margin-right: -50%;
        transform: translate(-50%, -50%);
      }
    </style>
  </head>
  <body>
    <section>
      Upload test
    </section>
    <input type="file" id="fileUpload" />
    <p id="upload_status"> </p>

   <script>

      function set_upload_status(status) {
        upload_status = status;
        console.log("upload_status: " + status);
        document.getElementById('upload_status').innerHTML = status;
      }

      let upload_status = "";
      set_upload_status("not_started");

      const handleFileUpload = event => {
      const files = event.target.files
      const formData = new FormData()
      formData.append('myFile', files[0])

      set_upload_status("started");
      const startTime = performance.now();
      fetch('/saveFile', {
        method: 'POST',
        body: formData
      })
      .then(response => response.json())
      .then(data => {
        console.log("status: " + data.status + " " + data.path + " size: " + data.size);
        const endTime = performance.now();
        const uploadTime = (endTime - startTime)
        set_upload_status("success time:" + uploadTime);
      })
      .catch(error => {
        console.error(error);
        set_upload_status("error");
      })
    }

    document.querySelector('#fileUpload').addEventListener('change', event => {
      handleFileUpload(event)
    })
   </script>

  </body>
</html>
`;

const server = http.createServer((req, res) => {
  if (req.url === "/saveFile" && req.method.toLowerCase() === "post") {
    let totalSize = 0;
    req.on("data", chunk => {
      totalSize += chunk.length;
    });

    req.on("end", () => {
      res.writeHead(200, { "Content-Type": "application/json" });
      res.end(JSON.stringify({ status: "success", size: totalSize }));
    });
  } else if (req.url === "/" && req.method.toLowerCase() === "get") {
    res.writeHead(200, { "Content-Type": "text/html" });
    res.end(html);
  } else {
    res.writeHead(404, { "Content-Type": "text/plain" });
    res.end("Not Found");
  }
});

server.listen(() => {
  console.log(`Server is running on http://localhost:${server.address().port}`);
});
