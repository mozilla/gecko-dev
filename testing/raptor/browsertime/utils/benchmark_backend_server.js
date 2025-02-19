/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* globals require, process, Buffer */

const http = require("http");

const html = `
<!DOCTYPE html>
<html>
  <head>
    <title>Upload/Download test</title>
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
      Upload/Download test
    </section>
    <button id="fileUpload">Upload Test</button>
    <p id="upload_status"> </p>
    <button id="downloadBtn">Download Test</button>
    <p id="download_status"></p>
    <script>
      let upload_status = "";
      let download_status = "";

      function set_status(id, status) {
        if (id === "upload_status") {
          upload_status = status;
        } else if (id === "download_status") {
          download_status = status;
        }
        console.log(id + ":" + status);
        document.getElementById(id).innerHTML = status;
      }

      set_status("upload_status", "not_started");
      set_status("download_status", "not_started");

      const BLOB_SIZE = 8 * 1024 * 1024;

      // Function to generate a Blob of specified size
      function generateBlob(sizeInBytes) {
        const array = new Uint8Array(sizeInBytes).fill(0);
        return new Blob([array], { type: 'application/octet-stream' });
      }

      // Function to upload the entire Blob
      async function uploadBlob(blob) {
        const formData = new FormData();
        formData.append('blob', blob);

        const response = await fetch('/saveFile', {
          method: 'POST',
          body: formData,
        });

        if (!response.ok) {
          throw new Error("Failed to upload Blob");
        }

        return response.json();
      }

      const handleUpload = async () => {
        const blob = generateBlob(BLOB_SIZE);
        let totalBytesUploaded = 0;
        set_status("upload_status", "started");
        const startTime = performance.now();
        const uploadDuration = 60 * 1000;
        const endTime = startTime + uploadDuration;

        try {
          while (true) {
            const currentTime = performance.now();
            if (currentTime >= endTime) {
              break;
            }

            await uploadBlob(blob);

            totalBytesUploaded += blob.size;
            set_status("upload_status", "uploaded:" + totalBytesUploaded);
          }

          const actualEndTime = performance.now();
          const totalTimeSeconds = (actualEndTime - startTime) / 1000;

          // Calculate goodput in Mbps
          const goodputMbps = (totalBytesUploaded * 8) / (totalTimeSeconds * 1_000_000);
          set_status("upload_status", "success goodput:" + goodputMbps);
        } catch (error) {
          set_status("upload_status", "error:" + error);
        }
      };

      document.querySelector('#fileUpload').addEventListener('click', handleUpload);

      const handleDownloadTest = () => {
            set_status("download_status", "started");
            const startTime = performance.now();
            fetch('/downloadTest')
              .then(response => response.blob())
              .then(blob => {
                const endTime = performance.now();
                const downloadTime = endTime - startTime;
                set_status("download_status", "success time:" + downloadTime);
              })
              .catch(error => {
                console.error(error);
                set_download_status("error");
              });
          }
      document.querySelector('#downloadBtn').addEventListener('click', handleDownloadTest);
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
  } else if (
    req.url === "/downloadTest" &&
    req.method.toLowerCase() === "get"
  ) {
    const contentLength = 32 * 1024 * 1024; // 32 MB
    res.writeHead(200, {
      "Content-Type": "application/octet-stream",
      "Content-Length": contentLength,
      "Content-Disposition": "attachment; filename=testfile.bin",
      "Cache-Control": "no-store, no-cache, must-revalidate",
      Pragma: "no-cache",
    });

    const chunkSize = 1024 * 1024; // 1MB chunks
    for (let i = 0; i < contentLength / chunkSize; i++) {
      const chunk = Buffer.alloc(chunkSize, "0"); // Fill the chunk with zeros
      res.write(chunk);
    }
    res.end();
  } else if (req.url === "/" && req.method.toLowerCase() === "get") {
    res.writeHead(200, {
      "Content-Type": "text/html",
      "Cache-Control": "no-store, no-cache, must-revalidate",
      Pragma: "no-cache",
    });
    res.end(html);
  } else {
    res.writeHead(404, { "Content-Type": "text/plain" });
    res.end("Not Found");
  }
});

server.listen(() => {
  console.log(`Server is running on http://localhost:${server.address().port}`);
});
