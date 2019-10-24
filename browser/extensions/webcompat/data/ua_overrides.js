/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* globals module, require */

// This is a hack for the tests.
if (typeof getMatchPatternsForGoogleURL === "undefined") {
  var getMatchPatternsForGoogleURL = require("../lib/google");
}

/**
 * For detailed information on our policies, and a documention on this format
 * and its possibilites, please check the Mozilla-Wiki at
 *
 * https://wiki.mozilla.org/Compatibility/Go_Faster_Addon/Override_Policies_and_Workflows#User_Agent_overrides
 */
const AVAILABLE_UA_OVERRIDES = [
  {
    id: "testbed-override",
    platform: "all",
    domain: "webcompat-addon-testbed.herokuapp.com",
    bug: "0000000",
    config: {
      hidden: true,
      matches: ["*://webcompat-addon-testbed.herokuapp.com/*"],
      uaTransformer: originalUA => {
        return (
          UAHelpers.getPrefix(originalUA) +
          " AppleWebKit/537.36 (KHTML, like Gecko) Chrome/54.0.2840.98 Safari/537.36 for WebCompat"
        );
      },
    },
  },
  {
    /*
     * Bug 1564594 - Create UA override for Enhanced Search on Firefox Android
     *
     * Enables the Chrome Google Search experience for Fennec users.
     */
    id: "bug1564594",
    platform: "android",
    domain: "Enhanced Search",
    bug: "1567945",
    config: {
      matches: [
        ...getMatchPatternsForGoogleURL("images.google"),
        ...getMatchPatternsForGoogleURL("maps.google"),
        ...getMatchPatternsForGoogleURL("news.google"),
        ...getMatchPatternsForGoogleURL("www.google"),
      ],
      blocks: [...getMatchPatternsForGoogleURL("www.google", "serviceworker")],
      permanentPref: "enable_enhanced_search",
      telemetryKey: "enhancedSearch",
      experiment: ["enhanced-search", "enhanced-search-control"],
      uaTransformer: originalUA => {
        return UAHelpers.getDeviceAppropriateChromeUA();
      },
    },
  },
  {
    /*
     * Bug 1563839 - rolb.santanderbank.com - Build UA override
     * WebCompat issue #33462 - https://webcompat.com/issues/33462
     *
     * santanderbank expects UA to have 'like Gecko', otherwise it runs
     * xmlDoc.onload whose support has been dropped. It results in missing labels in forms
     * and some other issues.  Adding 'like Gecko' fixes those issues.
     */
    id: "bug1563839",
    platform: "all",
    domain: "rolb.santanderbank.com",
    bug: "1563839",
    config: {
      matches: [
        "*://*.santander.co.uk/*",
        "*://bob.santanderbank.com/*",
        "*://rolb.santanderbank.com/*",
      ],
      uaTransformer: originalUA => {
        return originalUA.replace("Gecko", "like Gecko");
      },
    },
  },
  {
    /*
     * Bug 1577179 - UA override for supportforms.embarcadero.com
     * WebCompat issue #34682 - https://webcompat.com/issues/34682
     *
     * supportforms.embarcadero.com has a constant onchange event on a product selector
     * which makes it unusable. Spoofing as Chrome allows to stop event from firing
     */
    id: "bug1577179",
    platform: "all",
    domain: "supportforms.embarcadero.com",
    bug: "1577179",
    config: {
      matches: ["*://supportforms.embarcadero.com/*"],
      uaTransformer: originalUA => {
        return (
          UAHelpers.getPrefix(originalUA) +
          " AppleWebKit/537.36 (KHTML, like Gecko) Chrome/76.0.3809.132 Safari/537.36"
        );
      },
    },
  },
  {
    /*
     * Bug 1577519 - att.tv - Create a UA override for att.tv for playback on desktop
     * WebCompat issue #3846 - https://webcompat.com/issues/3846
     *
     * att.tv (atttvnow.com) is blocking Firefox via UA sniffing. Spoofing as Chrome allows
     * to access the site and playback works fine. This is former directvnow.com
     */
    id: "bug1577519",
    platform: "desktop",
    domain: "att.tv",
    bug: "1577519",
    config: {
      matches: ["*://*.att.tv/*"],
      uaTransformer: originalUA => {
        return (
          UAHelpers.getPrefix(originalUA) +
          " AppleWebKit/537.36 (KHTML, like Gecko) Chrome/76.0.3809.132 Safari/537.36"
        );
      },
    },
  },
  {
    /*
     * Bug 1570108 - steamcommunity.com - UA override for steamcommunity.com
     * WebCompat issue #34171 - https://webcompat.com/issues/34171
     *
     * steamcommunity.com blocks chat feature for Firefox users showing unsupported browser message.
     * When spoofing as Chrome the chat works fine
     */
    id: "bug1570108",
    platform: "desktop",
    domain: "steamcommunity.com",
    bug: "1570108",
    config: {
      matches: ["*://steamcommunity.com/chat*"],
      uaTransformer: originalUA => {
        return (
          UAHelpers.getPrefix(originalUA) +
          " AppleWebKit/537.36 (KHTML, like Gecko) Chrome/75.0.3770.142 Safari/537.36"
        );
      },
    },
  },
  {
    /*
     * Bug 1582582 - sling.com - UA override for sling.com
     * WebCompat issue #17804 - https://webcompat.com/issues/17804
     *
     * sling.com blocks Firefox users showing unsupported browser message.
     * When spoofing as Chrome playing content works fine
     */
    id: "bug1582582",
    platform: "desktop",
    domain: "sling.com",
    bug: "1582582",
    config: {
      matches: ["https://watch.sling.com/*", "https://www.sling.com/*"],
      uaTransformer: originalUA => {
        return (
          UAHelpers.getPrefix(originalUA) +
          " AppleWebKit/537.36 (KHTML, like Gecko) Chrome/76.0.3809.132 Safari/537.36"
        );
      },
    },
  },
  {
    /*
     * Bug 1480710 - m.imgur.com - Build UA override
     * WebCompat issue #13154 - https://webcompat.com/issues/13154
     *
     * imgur returns a 404 for requests to CSS and JS file if requested with a Fennec
     * User Agent. By removing the Fennec identifies and adding Chrome Mobile's, we
     * receive the correct CSS and JS files.
     */
    id: "bug1480710",
    platform: "android",
    domain: "m.imgur.com",
    bug: "1480710",
    config: {
      matches: ["*://m.imgur.com/*"],
      uaTransformer: originalUA => {
        return (
          UAHelpers.getPrefix(originalUA) +
          " AppleWebKit/537.36 (KHTML, like Gecko) Chrome/68.0.3440.85 Mobile Safari/537.36"
        );
      },
    },
  },
  {
    /*
     * Bug 945963 - tieba.baidu.com serves simplified mobile content to Firefox Android
     * WebCompat issue #18455 - https://webcompat.com/issues/18455
     *
     * tieba.baidu.com and tiebac.baidu.com serve a heavily simplified and less functional
     * mobile experience to Firefox for Android users. Adding the AppleWebKit indicator
     * to the User Agent gets us the same experience.
     */
    id: "bug945963",
    platform: "android",
    domain: "tieba.baidu.com",
    bug: "945963",
    config: {
      matches: [
        "*://tieba.baidu.com/*",
        "*://tiebac.baidu.com/*",
        "*://zhidao.baidu.com/*",
      ],
      uaTransformer: originalUA => {
        return originalUA + " AppleWebKit/537.36 (KHTML, like Gecko)";
      },
    },
  },
  {
    /*
     * Bug 1177298 - Write UA overrides for top Japanese Sites
     * (Imported from ua-update.json.in)
     *
     * To receive the proper mobile version instead of the desktop version or
     * a lower grade mobile experience, the UA is spoofed.
     */
    id: "bug1177298-2",
    platform: "android",
    domain: "lohaco.jp",
    bug: "1177298",
    config: {
      matches: ["*://*.lohaco.jp/*"],
      uaTransformer: _ => {
        return "Mozilla/5.0 (Linux; Android 5.0.2; Galaxy Nexus Build/IMM76B) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/43.0.2357.93 Mobile Safari/537.36";
      },
    },
  },
  {
    /*
     * Bug 1177298 - Write UA overrides for top Japanese Sites
     * (Imported from ua-update.json.in)
     *
     * To receive the proper mobile version instead of the desktop version or
     * a lower grade mobile experience, the UA is spoofed.
     */
    id: "bug1177298-3",
    platform: "android",
    domain: "nhk.or.jp",
    bug: "1177298",
    config: {
      matches: ["*://*.nhk.or.jp/*"],
      uaTransformer: originalUA => {
        return originalUA + " AppleWebKit";
      },
    },
  },
  {
    /*
     * Bug 1338260 - Add UA override for directTV
     * (Imported from ua-update.json.in)
     *
     * DirectTV has issues with scrolling and cut-off images. Pretending to be
     * Chrome for Android fixes those issues.
     */
    id: "bug1338260",
    platform: "android",
    domain: "directv.com",
    bug: "1338260",
    config: {
      matches: ["*://*.directv.com/*"],
      uaTransformer: _ => {
        return "Mozilla/5.0 (Linux; Android 6.0.1; SM-G920F Build/MMB29K) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/55.0.2883.91 Mobile Safari/537.36";
      },
    },
  },
  {
    /*
     * Bug 1385206 - Create UA override for rakuten.co.jp on Firefox Android
     * (Imported from ua-update.json.in)
     *
     * rakuten.co.jp serves a Desktop version if Firefox is included in the UA.
     */
    id: "bug1385206",
    platform: "android",
    domain: "rakuten.co.jp",
    bug: "1385206",
    config: {
      matches: ["*://*.rakuten.co.jp/*"],
      uaTransformer: originalUA => {
        return originalUA.replace(/Firefox.+$/, "");
      },
    },
  },
  {
    /*
     * Bug 969844 - mobile.de sends desktop site to Firefox on Android
     *
     * mobile.de sends the desktop site to Fennec. Spooing as Chrome works fine.
     */
    id: "bug969844",
    platform: "android",
    domain: "mobile.de",
    bug: "969844",
    config: {
      matches: ["*://*.mobile.de/*"],
      uaTransformer: _ => {
        return "Mozilla/5.0 (Linux; Android 6.0.1; SM-G920F Build/MMB29K) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/55.0.2883.91 Mobile Safari/537.36";
      },
    },
  },
  {
    /*
     * Bug 1509831 - cc.com - Add UA override for CC.com
     * WebCompat issue #329 - https://webcompat.com/issues/329
     *
     * ComedyCentral blocks Firefox for not being able to play HLS, which was
     * true in previous versions, but no longer is. With a spoofed Chrome UA,
     * the site works just fine.
     */
    id: "bug1509831",
    platform: "android",
    domain: "cc.com",
    bug: "1509831",
    config: {
      matches: ["*://*.cc.com/*"],
      uaTransformer: _ => {
        return "Mozilla/5.0 (Linux; Android 6.0.1; SM-G920F Build/MMB29K) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/55.0.2883.91 Mobile Safari/537.36";
      },
    },
  },
  {
    /*
     * Bug 1508516 - cineflix.com.br - Add UA override for cineflix.com.br/m/
     * WebCompat issue #21553 - https://webcompat.com/issues/21553
     *
     * The site renders a blank page with any Firefox snipped in the UA as it
     * is running into an exception. Spoofing as Chrome makes the site work
     * fine.
     */
    id: "bug1508516",
    platform: "android",
    domain: "cineflix.com.br",
    bug: "1508516",
    config: {
      matches: ["*://*.cineflix.com.br/m/*"],
      uaTransformer: originalUA => {
        return (
          UAHelpers.getPrefix(originalUA) +
          " AppleWebKit/537.36 (KHTML, like Gecko) Chrome/55.0.2883.91 Mobile Safari/537.36"
        );
      },
    },
  },
  {
    /*
     * Bug 1509852 - redbull.com - Add UA override for redbull.com
     * WebCompat issue #21439 - https://webcompat.com/issues/21439
     *
     * Redbull.com blocks some features, for example the live video player, for
     * Fennec. Spoofing as Chrome results in us rendering the video just fine,
     * and everything else works as well.
     */
    id: "bug1509852",
    platform: "android",
    domain: "redbull.com",
    bug: "1509852",
    config: {
      matches: ["*://*.redbull.com/*"],
      uaTransformer: originalUA => {
        return (
          UAHelpers.getPrefix(originalUA) +
          " AppleWebKit/537.36 (KHTML, like Gecko) Chrome/55.0.2883.91 Mobile Safari/537.36"
        );
      },
    },
  },
  {
    /*
     * Bug 1509873 - zmags.com - Add UA override for secure.viewer.zmags.com
     * WebCompat issue #21576 - https://webcompat.com/issues/21576
     *
     * The zmags viewer locks out Fennec with a "Browser unsupported" message,
     * but tests showed that it works just fine with a Chrome UA. Outreach
     * attempts were unsuccessful, and as the site has a relatively high rank,
     * we alter the UA.
     */
    id: "bug1509873",
    platform: "android",
    domain: "zmags.com",
    bug: "1509873",
    config: {
      matches: ["*://*.viewer.zmags.com/*"],
      uaTransformer: originalUA => {
        return (
          UAHelpers.getPrefix(originalUA) +
          " AppleWebKit/537.36 (KHTML, like Gecko) Chrome/55.0.2883.91 Mobile Safari/537.36"
        );
      },
    },
  },
  {
    /*
     * Bug 1566253 - posts.google.com - Add UA override for posts.google.com
     * WebCompat issue #17870 - https://webcompat.com/issues/17870
     *
     * posts.google.com displaying "Your browser doesn't support this page".
     * Spoofing as Chrome works fine.
     */
    id: "bug1566253",
    platform: "android",
    domain: "posts.google.com",
    bug: "1566253",
    config: {
      matches: ["*://posts.google.com/*"],
      uaTransformer: _ => {
        return "Mozilla/5.0 (Linux; Android 6.0.1; SM-G900M) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/75.0.3770.101 Mobile Safari/537.36";
      },
    },
  },
  {
    /*
     * Bug 1567945 - Create UA override for beeg.com on Firefox Android
     * WebCompat issue #16648 - https://webcompat.com/issues/16648
     *
     * beeg.com is hiding content of a page with video if Firefox exists in UA,
     * replacing "Firefox" with an empty string makes the page load
     */
    id: "bug1567945",
    platform: "android",
    domain: "beeg.com",
    bug: "1567945",
    config: {
      matches: ["*://beeg.com/*"],
      uaTransformer: originalUA => {
        return originalUA.replace(/Firefox.+$/, "");
      },
    },
  },
  {
    /*
     * Bug 1574522 - UA override for enuri.com on Firefox for Android
     * WebCompat issue #37139 - https://webcompat.com/issues/37139
     *
     * enuri.com returns a different template for Firefox on Android
     * based on server side UA detection. This results in page content cut offs.
     * Spoofing as Chrome fixes the issue
     */
    id: "bug1574522",
    platform: "android",
    domain: "enuri.com",
    bug: "1574522",
    config: {
      matches: ["*://enuri.com/*"],
      uaTransformer: _ => {
        return "Mozilla/5.0 (Linux; Android 6.0.1; SM-G900M) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/76.0.3809.111 Mobile Safari/537.36";
      },
    },
  },
  {
    /*
     * Bug 1574564 - UA override for ceskatelevize.cz on Firefox for Android
     * WebCompat issue #15467 - https://webcompat.com/issues/15467
     *
     * ceskatelevize sets streamingProtocol depending on the User-Agent it sees
     * in the request headers, returning DASH for Chrome, HLS for iOS,
     * and Flash for Fennec. Since Fennec has no Flash, the video doesn't work.
     * Spoofing as Chrome makes the video play
     */
    id: "bug1574564",
    platform: "android",
    domain: "ceskatelevize.cz",
    bug: "1574564",
    config: {
      matches: ["*://*.ceskatelevize.cz/*"],
      uaTransformer: originalUA => {
        return (
          UAHelpers.getPrefix(originalUA) +
          " AppleWebKit/537.36 (KHTML, like Gecko) Chrome/76.0.3809.111 Mobile Safari/537.36"
        );
      },
    },
  },
  {
    /*
     * Bug 1577240 - UA override for heb.com on Firefox for Android
     * WebCompat issue #33613 - https://webcompat.com/issues/33613
     *
     * heb.com shows desktop site on Firefox for Android for some pages based on
     * UA detection. Spoofing as Chrome allows to get mobile site.
     */
    id: "bug1577240",
    platform: "android",
    domain: "heb.com",
    bug: "1577240",
    config: {
      matches: ["*://*.heb.com/*"],
      uaTransformer: originalUA => {
        return (
          UAHelpers.getPrefix(originalUA) +
          " AppleWebKit/537.36 (KHTML, like Gecko) Chrome/76.0.3809.111 Mobile Safari/537.36"
        );
      },
    },
  },
  {
    /*
     * Bug 1577250 - UA override for homebook.pl on Firefox for Android
     * WebCompat issue #24044 - https://webcompat.com/issues/24044
     *
     * homebook.pl shows desktop site on Firefox for Android based on
     * UA detection. Spoofing as Chrome allows to get mobile site.
     */
    id: "bug1577250",
    platform: "android",
    domain: "homebook.pl",
    bug: "1577250",
    config: {
      matches: ["*://*.homebook.pl/*"],
      uaTransformer: originalUA => {
        return (
          UAHelpers.getPrefix(originalUA) +
          " AppleWebKit/537.36 (KHTML, like Gecko) Chrome/76.0.3809.111 Mobile Safari/537.36"
        );
      },
    },
  },
  {
    /*
     * Bug 1577267 - UA override for metfone.com.kh on Firefox for Android
     * WebCompat issue #16363 - https://webcompat.com/issues/16363
     *
     * metfone.com.kh has a server side UA detection which returns desktop site
     * for Firefox for Android. Spoofing as Chrome allows to receive mobile version
     */
    id: "bug1577267",
    platform: "android",
    domain: "metfone.com.kh",
    bug: "1577267",
    config: {
      matches: ["*://*.metfone.com.kh/*"],
      uaTransformer: originalUA => {
        return (
          UAHelpers.getPrefix(originalUA) +
          " AppleWebKit/537.36 (KHTML, like Gecko) Chrome/76.0.3809.111 Mobile Safari/537.36"
        );
      },
    },
  },
];

const UAHelpers = {
  getDeviceAppropriateChromeUA() {
    if (!UAHelpers._deviceAppropriateChromeUA) {
      const userAgent =
        typeof navigator !== "undefined" ? navigator.userAgent : "";
      const RunningFirefoxVersion = (userAgent.match(/Firefox\/([0-9.]+)/) || [
        "",
        "58.0",
      ])[1];
      const RunningAndroidVersion =
        userAgent.match(/Android\/[0-9.]+/) || "Android 6.0";
      const ChromeVersionToMimic = "76.0.3809.111";
      const ChromePhoneUA = `Mozilla/5.0 (Linux; ${RunningAndroidVersion}; Nexus 5 Build/MRA58N) FxQuantum/${RunningFirefoxVersion} AppleWebKit/537.36 (KHTML, like Gecko) Chrome/${ChromeVersionToMimic} Mobile Safari/537.36`;
      const ChromeTabletUA = `Mozilla/5.0 (Linux; ${RunningAndroidVersion}; Nexus 7 Build/JSS15Q) FxQuantum/${RunningFirefoxVersion} AppleWebKit/537.36 (KHTML, like Gecko) Chrome/${ChromeVersionToMimic} Safari/537.36`;
      const IsPhone = userAgent.includes("Mobile");
      UAHelpers._deviceAppropriateChromeUA = IsPhone
        ? ChromePhoneUA
        : ChromeTabletUA;
    }
    return UAHelpers._deviceAppropriateChromeUA;
  },
  getPrefix(originalUA) {
    return originalUA.substr(0, originalUA.indexOf(")") + 1);
  },
};

module.exports = AVAILABLE_UA_OVERRIDES;
