/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// check that the correct default bookmarks are imported with each build type
// browser/base/content/default-bookmarks.html

async function fetchMenuTree() {
  function nodeToInfo({ uri, children }) {
    return {
      ...(uri ? { uri } : {}),
      ...(children ? { children: children.map(nodeToInfo) } : {}),
    };
  }
  return nodeToInfo(
    await PlacesUtils.promiseBookmarksTree(PlacesUtils.bookmarks.menuGuid)
  );
}

add_task(async function checkDefaultBookmarks() {
  if (AppConstants.NIGHTLY_BUILD) {
    Assert.deepEqual(await fetchMenuTree(), {
      children: [
        {
          children: [
            {
              uri: "https://www.mozilla.org/contribute/?utm_medium=firefox-desktop&utm_source=bookmarks-toolbar&utm_campaign=new-users-nightly&utm_content=-global",
            },
            {
              uri: "https://blog.nightly.mozilla.org/",
            },
            {
              uri: "https://bugzilla.mozilla.org/",
            },
            {
              uri: "https://developer.mozilla.org/",
            },
            {
              uri: "https://addons.mozilla.org/firefox/addon/nightly-tester-tools/",
            },
            {
              uri: "about:crashes",
            },
            {
              uri: "https://planet.mozilla.org/",
            },
          ],
        },
      ],
    });
  } else if (AppConstants.EARLY_BETA_OR_EARLIER) {
    Assert.deepEqual(await fetchMenuTree(), {
      children: [
        {
          children: [
            {
              uri: "https://support.mozilla.org/products/firefox",
            },
            {
              uri: "https://support.mozilla.org/kb/customize-firefox-controls-buttons-and-toolbars?utm_source=firefox-browser&utm_medium=default-bookmarks&utm_campaign=customize",
            },
            {
              uri: "https://www.mozilla.org/contribute/?utm_medium=firefox-desktop&utm_source=bookmarks-toolbar&utm_campaign=new-users-beta&utm_content=-global",
            },
            {
              uri: "https://www.mozilla.org/about/",
            },
          ],
        },
      ],
    });
  } else {
    // Release
    Assert.deepEqual(await fetchMenuTree(), {
      children: [
        {
          children: [
            {
              uri: "https://support.mozilla.org/products/firefox",
            },
            {
              uri: "https://support.mozilla.org/kb/customize-firefox-controls-buttons-and-toolbars?utm_source=firefox-browser&utm_medium=default-bookmarks&utm_campaign=customize",
            },
            {
              uri: "https://www.mozilla.org/contribute/",
            },
            {
              uri: "https://www.mozilla.org/about/",
            },
          ],
        },
      ],
    });
  }
});
