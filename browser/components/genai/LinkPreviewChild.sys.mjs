/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
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
  async fetchHTML(url) {
    // Perform a HEAD request to check content type and length
    const headResponse = await fetch(url, {
      method: "HEAD",
      mode: "cors",
      headers: {
        "x-firefox-ai": "true",
      },
    });

    if (!headResponse.ok) {
      throw new Error(
        `Failed to fetch: ${headResponse.status} ${headResponse.statusText}`
      );
    }

    const contentType = headResponse.headers.get("content-type") || "";
    if (!contentType.startsWith("text/html")) {
      throw new Error(`Invalid content-type: ${contentType}`);
    }

    const contentLength = parseInt(
      headResponse.headers.get("content-length"),
      10
    );
    const MAX_CONTENT_LENGTH = 2 * 1024 * 1024; // 2 MB limit

    if (contentLength && contentLength > MAX_CONTENT_LENGTH) {
      throw new Error(`Content length exceeds limit: ${contentLength} bytes`);
    }

    // Proceed with GET request to fetch the HTML content
    const response = await fetch(url, {
      method: "GET",
      mode: "cors",
      headers: {
        "x-firefox-ai": "true",
      },
    });

    if (!response.ok) {
      throw new Error(
        `Failed to fetch: ${response.status} ${response.statusText}`
      );
    }

    const html = await response.text();
    return html;
  }

  /**
   * Fetches HTML content from a URL and parses its meta tags and page text.
   *
   * @param {string} url - The URL to fetch and parse.
   * @returns {Promise<object>} An object containing meta information, page text, and HTML code.
   */
  async fetchPageData(url) {
    try {
      const htmlCode = await this.fetchHTML(url);

      const parser = new DOMParser();
      const doc = parser.parseFromString(htmlCode, "text/html");

      const metaInfo = this.parseMetaTagsFromDoc(doc);
      const articleData = await this.getArticleDataFromDoc(doc);

      return {
        metaInfo,
        article: articleData,
      };
    } catch (error) {
      console.error(`Failed to fetch and parse page data: ${error.message}`);
      return {
        metaInfo: {},
        article: {},
      };
    }
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
    const desiredMetaNames = ["description", "og:image", "title"];

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
        //TODO include other fields like sitename from article
        const { title, byline, textContent, length, siteName } = article;

        //TODO getReadingTime from Readermode
        return {
          title,
          byline,
          textContent,
          length,
          siteName,
        };
      }
    } catch (error) {
      console.error("Error parsing document with ReaderMode:", error);
    }

    return {};
  }
}
