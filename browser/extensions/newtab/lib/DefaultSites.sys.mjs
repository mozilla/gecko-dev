/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const DEFAULT_SITES_MAP = new Map([
  // This first item is the global list fallback for any unexpected geos
  [
    "",
    "https://www.wikipedia.org/,https://www.youtube.com/,https://www.reddit.com/,https://addons.mozilla.org",
  ],
  [
    "US",
    "https://www.wikipedia.org/,https://www.youtube.com/,https://apnews.com/,https://www.reddit.com/",
  ],
  [
    "CA",
    "https://www.wikipedia.org/,https://www.youtube.com/,https://www.reddit.com/,https://addons.mozilla.org",
  ],
  [
    "DE",
    "https://www.wikipedia.org/,https://www.youtube.com/,https://www.tagesschau.de/,https://www.reddit.com/",
  ],
  [
    "PL",
    "https://www.wikipedia.org/,https://www.youtube.com/,https://www.reddit.com/,https://addons.mozilla.org",
  ],
  [
    "RU",
    "https://www.wikipedia.org/,https://www.youtube.com/,https://www.reddit.com/,https://addons.mozilla.org",
  ],
  [
    "GB",
    "https://www.wikipedia.org/,https://www.youtube.com/,https://www.bbc.co.uk/,https://www.reddit.com/",
  ],
  [
    "FR",
    "https://www.wikipedia.org/,https://www.youtube.com/,https://www.lemonde.fr/,https://www.reddit.com/",
  ],
  [
    "CN",
    "https://www.baidu.com/,https://www.zhihu.com/,https://www.ifeng.com/,https://weibo.com/,https://www.ctrip.com/,https://www.iqiyi.com/",
  ],
]);

// Immutable for export.
export const DEFAULT_SITES = Object.freeze(DEFAULT_SITES_MAP);
