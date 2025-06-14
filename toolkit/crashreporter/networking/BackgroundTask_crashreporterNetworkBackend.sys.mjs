/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

import { EXIT_CODE } from "resource://gre/modules/BackgroundTasksManager.sys.mjs";

/*
 * IMPORTANT! Keep the deserialized JSON format compatible with
 * toolkit/crashreporter/client/app/src/net/http.rs
 */

async function createRequestInit(requestBuilder) {
  switch (requestBuilder.type) {
    case "MimePost": {
      const formData = new FormData();
      for (const part of requestBuilder.parts) {
        let content = part.content;
        const options = { type: part.mime_type ?? "" };
        switch (content.type) {
          case "File":
            content = await File.createFromFileName(content.value, options);
            break;
          case "String":
            content = new Blob([content.value], options);
            break;
        }
        formData.append(part.name, content, part.filename);
      }
      return {
        method: "POST",
        body: formData,
      };
    }
    case "Post": {
      const body = requestBuilder.body;
      const headers = requestBuilder.headers;
      return {
        method: "POST",
        headers: Object.fromEntries(headers),
        body: new Uint8Array(body),
      };
    }
  }

  throw new Error("invalid request builder format");
}

export async function runBackgroundTask(commandLine) {
  const requestUrl = commandLine.getArgument(0);
  const requestUserAgent = commandLine.getArgument(1);
  const requestBuilderFilePath = commandLine.getArgument(2);

  const requestBuilderFile = await File.createFromFileName(
    requestBuilderFilePath
  );
  const requestBuilder = JSON.parse(await requestBuilderFile.text());

  const requestInit = await createRequestInit(requestBuilder);
  (requestInit.headers ??= {})["User-Agent"] = requestUserAgent;
  const request = new Request(requestUrl, requestInit);
  const response = await fetch(request);
  if (!response.ok) {
    console.error(
      `Request failed: ${response.status} ${response.statusText}\n${await response.text()}`
    );
    return 1;
  }

  await IOUtils.write(requestBuilderFilePath, await response.bytes());

  return EXIT_CODE.SUCCESS;
}
