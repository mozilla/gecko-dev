/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React, { useEffect, useRef } from "react";

const BROWSER_STYLES = [
  "height",
  "width",
  "border",
  "border-radius",
  "flex",
  "margin",
  "padding",
];

function applyValidStyles(element, style, validStyles) {
  Object.keys(style).forEach(key => {
    if (validStyles.includes(key)) {
      element.style.setProperty(key, style[key]);
    }
  });
}

export const EmbeddedBrowser = props => {
  // Conditionally render the component only if the environment supports XULElements (such as in Spotlight modals)
  return document.createXULElement ? <EmbeddedBrowserInner {...props} /> : null;
};

const EmbeddedBrowserInner = ({ url, style }) => {
  const ref = useRef(null);

  useEffect(() => {
    const browser = document.createXULElement("browser");
    browser.setAttribute("disableglobalhistory", "true");
    browser.setAttribute("type", "content");
    browser.setAttribute("remote", "true");

    ref.current.appendChild(browser);
  }, []);

  useEffect(() => {
    const browser = ref.current.querySelector("browser");
    if (browser) {
      if (style) {
        applyValidStyles(browser, style, BROWSER_STYLES);
      }
      browser.fixupAndLoadURIString(url, {
        triggeringPrincipal: Services.scriptSecurityManager.createNullPrincipal(
          {}
        ),
      });
    }
  }, [url, style]);

  return <div className="embedded-browser-container" ref={ref}></div>;
};

export default EmbeddedBrowser;
