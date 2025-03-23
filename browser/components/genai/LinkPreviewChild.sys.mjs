/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  NetUtil: "resource://gre/modules/NetUtil.sys.mjs",
  ReaderMode: "moz-src:///toolkit/components/reader/ReaderMode.sys.mjs",
});

/**
 * Represents a child actor for handling link previews in the browser.
 * Interacts with content windows and handles events related to link previews.
 *
 * @class LinkPreviewChild
 * @augments {JSWindowActorChild}
 */
export class LinkPreviewChild extends JSWindowActorChild {
  /**
   * Handles incoming messages from the parent actor.
   *
   * @param {object} message - The message object containing name and data.
   * @param {string} message.name - The name of the message.
   * @param {object} message.data - Data associated with the message.
   * @returns {Promise<object>|undefined} The result of fetchPageData if applicable.
   */
  async receiveMessage({ name, data }) {
    if (name === "LinkPreview:FetchPageData") {
      return this.fetchPageData(data.url);
    }
    //expected a return value.  consistent-return (eslint)
    return undefined;
  }

  /**
   * Fetches the HTML content from the given URL.
   *
   * @param {string} url - The URL to fetch.
   * @returns {Promise<string>} The HTML content as a string.
   * @throws {Error} If the fetch fails or the content type is invalid.
   */
  fetchHTML(url) {
    const uri = lazy.NetUtil.newURI(url);
    if (!uri.schemeIs("https")) {
      throw Components.Exception(
        "Only handling https",
        Cr.NS_ERROR_UNKNOWN_PROTOCOL
      );
    }

    // Make requests with a channel to automatically get safe browsing checks.
    // Use null principals in combination with anonymous for now ahead of
    // fetching content with cookies to handle sites requiring login.
    const principal = Services.scriptSecurityManager.createNullPrincipal({});
    const channel = lazy.NetUtil.newChannel({
      contentPolicyType: Ci.nsIContentPolicy.TYPE_DOCUMENT,
      loadingPrincipal: principal,
      securityFlags: Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_INHERITS_SEC_CONTEXT,
      triggeringPrincipal: principal,
      uri,
    }).QueryInterface(Ci.nsIHttpChannel);
    channel.loadFlags = Ci.nsIRequest.LOAD_ANONYMOUS;

    // Specially identify this request, e.g., for publishers to opt out
    channel.setRequestHeader("x-firefox-ai", "1", false);

    const { promise, resolve, reject } = Promise.withResolvers();
    const MAX_CONTENT_LENGTH = 2 * 1024 * 1024; // 2 MB limit

    let charset = "utf-8";
    const byteChunks = [];
    let totalLength = 0;
    channel.asyncOpen({
      onDataAvailable(request, stream, offset, count) {
        totalLength += count;
        if (totalLength > MAX_CONTENT_LENGTH) {
          request.cancel(Cr.NS_ERROR_FILE_TOO_BIG);
        } else {
          byteChunks.push(lazy.NetUtil.readInputStream(stream, count));
        }
      },
      onStartRequest(request) {
        const http = request.QueryInterface(Ci.nsIHttpChannel);

        // Enforce text/html if provided by server
        let contentType = "";
        try {
          contentType = http.getResponseHeader("content-type");
        } catch (ex) {}
        if (contentType && !contentType.startsWith("text/html")) {
          request.cancel(Cr.NS_ERROR_FILE_UNKNOWN_TYPE);
        }

        // Save charset for later decoding
        const match = contentType.match(/charset=([^;]+)/i);
        if (match) {
          charset = match[1];
        }

        // Enforce max length if provided by server
        try {
          if (http.getResponseHeader("content-length") > MAX_CONTENT_LENGTH) {
            request.cancel(Cr.NS_ERROR_FILE_TOO_BIG);
          }
        } catch (ex) {}
      },
      onStopRequest(_request, status) {
        if (Components.isSuccessCode(status)) {
          const bytes = new Uint8Array(totalLength);
          let offset = 0;
          for (const chunk of byteChunks) {
            bytes.set(new Uint8Array(chunk), offset);
            offset += chunk.byteLength;
          }

          const decoder = new TextDecoder(charset);
          resolve(decoder.decode(bytes));
        } else {
          reject(Components.Exception("Failed to fetch HTML", status));
        }
      },
    });
    return promise;
  }

  /**
   * Fetches HTML content from a URL and parses its meta tags and page text.
   *
   * @param {string} url - The URL to fetch and parse.
   * @returns {Promise<object>} An object containing meta information, page text, and HTML code.
   */
  async fetchPageData(url) {
    const ret = {
      article: {},
      metaInfo: {},
      url,
    };
    try {
      const htmlCode = await this.fetchHTML(url);

      const parser = new DOMParser();
      const doc = parser.parseFromString(htmlCode, "text/html");

      ret.metaInfo = this.parseMetaTagsFromDoc(doc);
      ret.article = await this.getArticleDataFromDoc(doc);
    } catch (error) {
      console.error(`Failed to fetch and parse page data: ${error}`);
      ret.error = { message: error.message, result: error.result };
    }
    return ret;
  }

  /**
   * Parses meta tags from the provided Document into a key-value object.
   * Also extracts the title if available.
   *
   * @param {Document} doc - The parsed HTML document.
   * @returns {object} An object containing meta tag key-value pairs.
   */
  parseMetaTagsFromDoc(doc) {
    const metaTags = doc.querySelectorAll("meta");
    const metaInfo = {};

    // TODO: Define the meta tags we are interested in
    const desiredMetaNames = [
      "description",
      "og:image",
      "title",
      "og:title",
      "twitter:title",
      "og:description",
      "twitter:description",
    ];

    metaTags.forEach(tag => {
      const name = tag.getAttribute("name") || tag.getAttribute("property");
      const content = tag.getAttribute("content");
      if (name && content) {
        if (desiredMetaNames.includes(name.toLowerCase())) {
          metaInfo[name] = content;
        }
      }
    });

    const title = doc.querySelector("title")?.textContent;
    if (title) {
      metaInfo["html:title"] = title;
    }

    return metaInfo;
  }

  /**
   * Extracts article data from the provided Document using ReaderMode.
   *
   * @param {Document} doc - The parsed HTML document.
   * @returns {Promise<object>} The extracted article data including specified fields.
   */
  async getArticleDataFromDoc(doc) {
    try {
      const article = await lazy.ReaderMode.parseDocument(doc);
      if (article) {
        const {
          title,
          byline,
          textContent,
          length,
          siteName,
          excerpt,
          readingTimeMinsSlow,
          readingTimeMinsFast,
        } = article;

        return {
          title,
          byline,
          textContent,
          length,
          siteName,
          excerpt,
          readingTimeMinsFast,
          readingTimeMinsSlow,
        };
      }
    } catch (error) {
      console.error("Error parsing document with ReaderMode:", error);
    }

    return {};
  }
}
