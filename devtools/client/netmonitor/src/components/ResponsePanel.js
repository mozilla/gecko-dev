/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Component, createFactory } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { L10N } = require("../utils/l10n");
const {
  decodeUnicodeBase64,
  fetchNetworkUpdatePacket,
  formDataURI,
  getUrlBaseName,
} = require("../utils/request-utils");
const { Filters } = require("../utils/filter-predicates");

// Components
const PropertiesView = createFactory(require("./PropertiesView"));

const { div, img } = dom;
const JSON_SCOPE_NAME = L10N.getStr("jsonScopeName");
const JSON_FILTER_TEXT = L10N.getStr("jsonFilterText");
const RESPONSE_IMG_NAME = L10N.getStr("netmonitor.response.name");
const RESPONSE_IMG_DIMENSIONS = L10N.getStr("netmonitor.response.dimensions");
const RESPONSE_IMG_MIMETYPE = L10N.getStr("netmonitor.response.mime");
const RESPONSE_PAYLOAD = L10N.getStr("responsePayload");
const RESPONSE_PREVIEW = L10N.getStr("responsePreview");

const JSON_VIEW_MIME_TYPE = "application/vnd.mozilla.json.view";

/**
 * Response panel component
 * Displays the GET parameters and POST data of a request
 */
class ResponsePanel extends Component {
  static get propTypes() {
    return {
      request: PropTypes.object.isRequired,
      openLink: PropTypes.func,
      connector: PropTypes.object.isRequired,
    };
  }

  constructor(props) {
    super(props);

    this.state = {
      imageDimensions: {
        width: 0,
        height: 0,
      },
    };

    this.updateImageDimensions = this.updateImageDimensions.bind(this);
  }

  componentDidMount() {
    const { request, connector } = this.props;
    fetchNetworkUpdatePacket(connector.requestData, request, ["responseContent"]);
  }

  componentWillReceiveProps(nextProps) {
    const { request, connector } = nextProps;
    fetchNetworkUpdatePacket(connector.requestData, request, ["responseContent"]);
  }

  updateImageDimensions({ target }) {
    this.setState({
      imageDimensions: {
        width: target.naturalWidth,
        height: target.naturalHeight,
      },
    });
  }

  /**
   * This method checks that the response is base64 encoded by
   * comparing these 2 values:
   * 1. The original response
   * 2. The value of doing a base64 decode on the
   * response and then base64 encoding the result.
   * If the values are different or an error is thrown,
   * the method will return false.
   */
  isBase64(response) {
    try {
      return btoa(atob(response)) == response;
    } catch (err) {
      return false;
    }
  }

  /**
   * Handle json, which we tentatively identify by checking the
   * MIME type for "json" after any word boundary. This works
   * for the standard "application/json", and also for custom
   * types like "x-bigcorp-json". Additionally, we also
   * directly parse the response text content to verify whether
   * it's json or not, to handle responses incorrectly labeled
   * as text/plain instead.
   */
  isJSON(mimeType, response) {
    let json, error;

    try {
      json = JSON.parse(response);
    } catch (err) {
      if (this.isBase64(response)) {
        try {
          json = JSON.parse(atob(response));
        } catch (err64) {
          error = err;
        }
      } else {
        error = err;
      }
    }

    if (/\bjson/.test(mimeType) || json) {
      // Extract the actual json substring in case this might be a "JSONP".
      // This regex basically parses a function call and captures the
      // function name and arguments in two separate groups.
      const jsonpRegex = /^\s*([\w$]+)\s*\(\s*([^]*)\s*\)\s*;?\s*$/;
      const [, jsonpCallback, jsonp] = response.match(jsonpRegex) || [];
      const result = {};

      // Make sure this is a valid JSON object first. If so, nicely display
      // the parsing results in a tree view.
      if (jsonpCallback && jsonp) {
        error = null;
        try {
          json = JSON.parse(jsonp);
        } catch (err) {
          error = err;
        }
      }

      // Valid JSON
      if (json) {
        result.json = json;
      }
      // Valid JSONP
      if (jsonpCallback) {
        result.jsonpCallback = jsonpCallback;
      }
      // Malformed JSON
      if (error) {
        result.error = "" + error;
      }

      return result;
    }

    return null;
  }

  render() {
    const { openLink, request } = this.props;
    const { responseContent, url } = request;

    if (!responseContent || typeof responseContent.content.text !== "string") {
      return null;
    }

    let { encoding, mimeType, text } = responseContent.content;

    if (mimeType.includes("image/")) {
      const { width, height } = this.state.imageDimensions;

      return (
        div({ className: "panel-container response-image-box devtools-monospace" },
          img({
            className: "response-image",
            src: formDataURI(mimeType, encoding, text),
            onLoad: this.updateImageDimensions,
          }),
          div({ className: "response-summary" },
            div({ className: "tabpanel-summary-label" }, RESPONSE_IMG_NAME),
            div({ className: "tabpanel-summary-value" }, getUrlBaseName(url)),
          ),
          div({ className: "response-summary" },
            div({ className: "tabpanel-summary-label" }, RESPONSE_IMG_DIMENSIONS),
            div({ className: "tabpanel-summary-value" }, `${width} × ${height}`),
          ),
          div({ className: "response-summary" },
            div({ className: "tabpanel-summary-label" }, RESPONSE_IMG_MIMETYPE),
            div({ className: "tabpanel-summary-value" }, mimeType),
          ),
        )
      );
    }

    // Decode response if it's coming from JSONView.
    if (mimeType.includes(JSON_VIEW_MIME_TYPE) && encoding === "base64") {
      text = decodeUnicodeBase64(text);
    }

    // Display Properties View
    const { json, jsonpCallback, error } = this.isJSON(mimeType, text) || {};
    const object = {};
    let sectionName;

    if (json) {
      if (jsonpCallback) {
        sectionName = L10N.getFormatStr("jsonpScopeName", jsonpCallback);
      } else {
        sectionName = JSON_SCOPE_NAME;
      }
      object[sectionName] = json;
    }

    // Display HTML under Properties View
    if (Filters.html(this.props.request)) {
      object[RESPONSE_PREVIEW] = {
        HTML_PREVIEW: { responseContent },
      };
    }

    // Others like text/html, text/plain, application/javascript
    object[RESPONSE_PAYLOAD] = {
      EDITOR_CONFIG: {
        text,
        mode: json ? "application/json" : mimeType.replace(/;.+/, ""),
      },
    };

    const classList = ["panel-container"];
    if (Filters.html(this.props.request)) {
      classList.push("contains-html-preview");
    }

    return (
      div({ className: classList.join(" ") },
        error && div({ className: "response-error-header", title: error },
          error
        ),
        PropertiesView({
          object,
          filterPlaceHolder: JSON_FILTER_TEXT,
          sectionNames: Object.keys(object),
          openLink,
        }),
      )
    );
  }
}

module.exports = ResponsePanel;
