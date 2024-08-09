// This is shared by image/test/mochitest/test_animated_css_image.html
// and image/test/browser/browser_animated_css_image.js
// Make sure any referenced files/images exist in both of those directories.

const kTests = [
  // Sanity test: background-image on a regular element.
  {
    html: `
      <!doctype html>
      <style>
        div {
          width: 100px;
          height: 100px;
          background-image: url(animated1.gif);
        }
      </style>
      <div></div>
    `,
    element(doc) {
      return doc.querySelector("div");
    },
  },

  // bug 1627585: content: url()
  {
    html: `
      <!doctype html>
      <style>
        div::before {
          content: url(animated1.gif);
        }
      </style>
      <div></div>
    `,
    element(doc) {
      return doc.querySelector("div");
    },
  },

  // bug 1627585: content: url() (on an element directly)
  {
    html: `
      <!doctype html>
      <style>
        div {
          content: url(animated1.gif);
        }
      </style>
      <div></div>
    `,
    element(doc) {
      return doc.querySelector("div");
    },
  },

  // bug 1625571: background propagated to canvas.
  {
    html: `
      <!doctype html>
      <style>
        body {
          background-image: url(animated1.gif);
        }
      </style>
    `,
    element(doc) {
      return doc.documentElement;
    },
  },

  // bug 1910297: background propagated to canvas with display: table
  {
    html: `
      <!doctype html>
      <style>
        html {
          display: table;
          background-image: url(animated1.gif);
        }
      </style>
    `,
    element(doc) {
      return doc.documentElement;
    },
  },

  // bug 1719375: CSS animation in SVG image.
  {
    html: `
      <!doctype html>
      <style>
        div {
          width: 100px;
          height: 100px;
          background-image: url(animated1.svg);
        }
      </style>
      <div></div>
    `,
    element(doc) {
      return doc.querySelector("div");
    },
  },

  // bug 1730834: stopped window.
  {
    html: `
      <!doctype html>
      <style>
        div {
          width: 100px;
          height: 100px;
        }
      </style>
      <body onload="window.stop(); document.querySelector('div').style.backgroundImage = 'url(animated1.gif)';">
        <div></div>
      </body>
    `,
    element(doc) {
      return doc.querySelector("div");
    },
  },

  // bug 1731138: Animated mask
  {
    html: `
      <!doctype html>
      <style>
        div {
          width: 100px;
          height: 100px;
          background-color: lime;
          mask-clip: border-box;
          mask-size: 100% 100%;
          mask-image: url(animatedMask.gif);
      }
      </style>
      <div></div>
    `,
    element(doc) {
      return doc.querySelector("div");
    },
  },
];
