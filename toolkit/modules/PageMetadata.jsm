/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["PageMetadata"];

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "UnescapeService",
                                   "@mozilla.org/feed-unescapehtml;1",
                                   "nsIScriptableUnescapeHTML");


/**
 * Maximum number of images to discover in the document, when no preview images
 * are explicitly specified by the metadata.
 * @type {Number}
 */
const DISCOVER_IMAGES_MAX  = 5;


/**
 * Extract metadata and microdata from a HTML document.
 * @type {Object}
 */
this.PageMetadata = {
  /**
   * Get all metadata from an HTML document. This includes:
   * - URL
   * - title
   * - Metadata specified in <meta> tags, including OpenGraph data
   * - Links specified in <link> tags (short, canonical, preview images, alternative)
   * - Content that can be found in the page content that we consider useful metadata
   * - Microdata, as defined by the spec:
   *   http://www.whatwg.org/specs/web-apps/current-work/multipage/microdata-2.html
   *
   * @param {Document} document - Document to extract data from.
   * @returns {Object} Object containing the various metadata, normalized to
   *                   merge some common alternative names for metadata.
   */
  getData(document) {
    let result = {
      url: this._validateURL(document, document.documentURI),
      title: document.title,
      previews: [],
    };

    // if pushState was used to change the url, most likely all meta data is
    // invalid. This is the case with several major sites that rely on
    // pushState. In that case, we'll only return uri and title. If document is
    // via XHR or something, there is no view or history.
    if (document.defaultView) {
      let docshell = document.defaultView.QueryInterface(Ci.nsIInterfaceRequestor)
                                         .getInterface(Ci.nsIWebNavigation)
                                         .QueryInterface(Ci.nsIDocShell);
      let shentry = {};
      if (docshell.getCurrentSHEntry(shentry) &&
          shentry.value && shentry.value.URIWasModified) {
        return result;
      }
    }

    this._getMetaData(document, result);
    this._getLinkData(document, result);
    this._getPageData(document, result);
    result.microdata = this.getMicrodata(document);

    return result;
  },

  /**
   * Get all microdata from an HTML document, or from only a given element.
   *
   * Returns an object in the format:
   *   {
   *     "items": [
   *       {
   *         "type": [ "<TYPE>" ],
   *         "properties": {
   *            "<PROPERTY-NAME>": [ "<PROPERTY-VALUE>", ... ],
   *            ...,
   *         }
   *       },
   *       ...,
   *     ]
   *   }
   *
   * @note This is based on wg algorythm to convert microdata to json
   *      http://www.whatwg.org/specs/web-apps/current-work/multipage/microdata-2.html#json
   *
   * @param {Document} document - Document to extract data from.
   * @param {Element} [target] - Optional element to restrict microdata lookup to.
   * @return {Object} Object describing the found microdata.
   */
  getMicrodata(document, target = null) {
    function getObject(item) {
      let result = {};

      if (item.itemType.length) {
        result.types = [...item.itemType];
      }

      if (item.itemId) {
        result.itemId = item.itemId;
      }

      if (item.properties.length) {
        result.properties = {};
      }

      for (let elem of item.properties) {
        let value;
        if (elem.itemScope) {
          value = getObject(elem);
        } else if (elem.itemValue) {
          value = elem.itemValue;
        } else if (elem.hasAttribute("content")) {
          // Handle mis-formatted microdata.
          value = elem.getAttribute("content");
        }

        for (let prop of elem.itemProp) {
          if (!result.properties[prop]) {
            result.properties[prop] = [];
          }

          result.properties[prop].push(value);
        }
      }

      return result;
    }

    let result = { items: [] };
    let elements = target ? [target] : document.getItems();

    for (let element of elements) {
      if (element.itemScope) {
        result.items.push(getObject(element));
      }
    }

    return result;
  },

  /**
   * Get metadata as defined in <meta> tags.
   * This adds properties to an existing result object.
   *
   * @param {Document} document - Document to extract data from.
   * @param {Object}  result - Existing result object to add properties to.
   */
  _getMetaData(document, result) {
    // Query for standardized meta data.
    let elements = document.querySelectorAll("head > meta[property], head > meta[name]");
    if (elements.length < 1) {
      return;
    }

    for (let element of elements) {
      let value = element.getAttribute("content")
      if (!value) {
        continue;
      }
      value = UnescapeService.unescape(value.trim());

      let key = element.getAttribute("property") || element.getAttribute("name");
      if (!key) {
        continue;
      }

      // There are a wide array of possible meta tags, expressing articles,
      // products, etc. so all meta tags are passed through but we touch up the
      // most common attributes.
      result[key] = value;

      switch (key) {
        case "title":
        case "og:title": {
          result.title = value;
          break;
        }

        case "description":
        case "og:description": {
          result.description = value;
          break;
        }

        case "og:site_name": {
          result.siteName = value;
          break;
        }

        case "medium":
        case "og:type": {
          result.medium = value;
          break;
        }

        case "og:video": {
          let url = this._validateURL(document, value);
          if (url) {
            result.source = url;
          }
          break;
        }

        case "og:url": {
          let url = this._validateURL(document, value);
          if (url) {
            result.url = url;
          }
          break;
        }

        case "og:image": {
          let url = this._validateURL(document, value);
          if (url) {
            result.previews.push(url);
          }
          break;
        }
      }
    }
  },

  /**
   * Get metadata as defined in <link> tags.
   * This adds properties to an existing result object.
   *
   * @param {Document} document - Document to extract data from.
   * @param {Object}  result - Existing result object to add properties to.
   */
  _getLinkData: function(document, result) {
    let elements = document.querySelectorAll("head > link[rel], head > link[id]");

    for (let element of elements) {
      let url = element.getAttribute("href");
      if (!url) {
        continue;
      }
      url = this._validateURL(document, UnescapeService.unescape(url.trim()));

      let key = element.getAttribute("rel") || element.getAttribute("id");
      if (!key) {
        continue;
      }

      switch (key) {
        case "shorturl":
        case "shortlink": {
          result.shortUrl = url;
          break;
        }

        case "canonicalurl":
        case "canonical": {
          result.url = url;
          break;
        }

        case "image_src": {
          result.previews.push(url);
          break;
        }

        case "alternate": {
          // Expressly for oembed support but we're liberal here and will let
          // other alternate links through. oembed defines an href, supplied by
          // the site, where you can fetch additional meta data about a page.
          // We'll let the client fetch the oembed data themselves, but they
          // need the data from this link.
          if (!result.alternate) {
            result.alternate = [];
          }

          result.alternate.push({
            type: element.getAttribute("type"),
            href: element.getAttribute("href"),
            title: element.getAttribute("title")
          });
        }
      }
    }
  },

  /**
   * Scrape thought the page content for additional content that may be used to
   * suppliment explicitly defined metadata. This includes:
   * - First few images, when no preview image metadata is explicitly defined.
   *
   * This adds properties to an existing result object.
   *
   * @param {Document} document - Document to extract data from.
   * @param {Object}  result - Existing result object to add properties to.
   */
  _getPageData(document, result) {
    if (result.previews.length < 1) {
      result.previews = this._getImageUrls(document);
    }
  },

  /**
   * Find the first few images in a document, for use as preview images.
   * Will return upto DISCOVER_IMAGES_MAX number of images.
   *
   * @note This is not very clever. It does not (yet) check if any of the
   *       images may be appropriate as a preview image.
   *
   * @param {Document} document - Document to extract data from.
   * @return {[string]} Array of URLs.
   */
  _getImageUrls(document) {
    let result = [];
    let elements = document.querySelectorAll("img");

    for (let element of elements) {
      let src = element.getAttribute("src");
      if (src) {
        result.push(this._validateURL(document, UnescapeService.unescape(src)));

        // We don't want a billion images.
        // TODO: Move this magic number to a const.
        if (result.length > DISCOVER_IMAGES_MAX) {
          break;
        }
      }
    }

    return result;
  },

  /**
   * Validate a URL. This involves resolving the URL if it's relative to the
   * document location, ensuring it's using an expected scheme, and stripping
   * the userPass portion of the URL.
   *
   * @param {Document} document - Document to use as the root location for a relative URL.
   * @param {string} url - URL to validate.
   * @return {string} Result URL.
   */
  _validateURL(document, url) {
    let docURI = Services.io.newURI(document.documentURI, null, null);
    let uri = Services.io.newURI(docURI.resolve(url), null, null);

    if (["http", "https"].indexOf(uri.scheme) < 0) {
      return null;
    }

    uri.userPass = "";

    return uri.spec;
  },
};
