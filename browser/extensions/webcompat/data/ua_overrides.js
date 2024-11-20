/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* globals browser, module, require */

// This is a hack for the tests.
if (typeof InterventionHelpers === "undefined") {
  var InterventionHelpers = require("../lib/intervention_helpers");
}
if (typeof UAHelpers === "undefined") {
  var UAHelpers = require("../lib/ua_helpers");
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
     * Bug 1577519 - directv.com - Create a UA override for directv.com for playback on desktop
     * WebCompat issue #3846 - https://webcompat.com/issues/3846
     *
     * directv.com is blocking Firefox via UA sniffing. Spoofing as Chrome allows
     * to access the site and playback works fine. This is former directvnow.com
     */
    id: "bug1577519",
    platform: "desktop",
    domain: "directv.com",
    bug: "1577519",
    config: {
      matches: [
        "*://*.directv.com.ec/*", // bug 1827706
        "*://*.directv.com/*",
      ],
      uaTransformer: () => {
        return UAHelpers.getDeviceAppropriateChromeUA();
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
     * Bug 1582582 - watch.sling.com - UA override for watch.sling.com
     * WebCompat issue #17804 - https://webcompat.com/issues/17804
     *
     * watch.sling.com blocks Firefox users showing unsupported browser message.
     * When spoofing as Chrome playing content works fine
     */
    id: "bug1582582",
    platform: "desktop",
    domain: "watch.sling.com",
    bug: "1582582",
    config: {
      matches: ["https://watch.sling.com/*"],
      uaTransformer: originalUA => {
        return (
          UAHelpers.getPrefix(originalUA) +
          " AppleWebKit/537.36 (KHTML, like Gecko) Chrome/108.0.0.0 Safari/537.36"
        );
      },
    },
  },
  {
    /*
     * Bug 1610026 - www.mobilesuica.com - UA override for www.mobilesuica.com
     * WebCompat issue #4608 - https://webcompat.com/issues/4608
     *
     * mobilesuica.com showing unsupported message for Firefox users
     * Spoofing as Chrome allows to access the page
     */
    id: "bug1610026",
    platform: "all",
    domain: "www.mobilesuica.com",
    bug: "1610026",
    config: {
      matches: ["https://www.mobilesuica.com/*"],
      uaTransformer: originalUA => {
        return (
          UAHelpers.getPrefix(originalUA) +
          " AppleWebKit/537.36 (KHTML, like Gecko) Chrome/79.0.3945.88 Safari/537.36"
        );
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
     * mobile.de sends the desktop site to Firefox Mobile.
     * Spoofing as Chrome works fine.
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
     * Bug 1509873 - zmags.com - Add UA override for secure.viewer.zmags.com
     * WebCompat issue #21576 - https://webcompat.com/issues/21576
     *
     * The zmags viewer locks out Firefox Mobile with a "Browser unsupported"
     * message, but tests showed that it works just fine with a Chrome UA.
     * Outreach attempts were unsuccessful, and as the site has a relatively
     * high rank, we alter the UA.
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
     * and Flash for Firefox Mobile. Since Mobile has no Flash, the video
     * doesn't work. Spoofing as Chrome makes the video play
     */
    id: "bug1574564",
    platform: "android",
    domain: "ceskatelevize.cz",
    bug: "1574564",
    config: {
      matches: ["*://*.ceskatelevize.cz/*"],
      uaTransformer: () => {
        return UAHelpers.getDeviceAppropriateChromeUA();
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
  {
    /*
     * Bug 1598198 - User Agent extension for Samsung's galaxy.store URLs
     *
     * Samsung's galaxy.store shortlinks are supposed to redirect to a Samsung
     * intent:// URL on Samsung devices, but to an error page on other brands.
     * As we do not provide device info in our user agent string, this check
     * fails, and even Samsung users land on an error page if they use Firefox
     * for Android.
     * This intervention adds a simple "Samsung" identifier to the User Agent
     * on only the Galaxy Store URLs if the device happens to be a Samsung.
     */
    id: "bug1598198",
    platform: "android",
    domain: "galaxy.store",
    bug: "1598198",
    config: {
      matches: [
        "*://galaxy.store/*",
        "*://dev.galaxy.store/*",
        "*://stg.galaxy.store/*",
      ],
      uaTransformer: originalUA => {
        if (!browser.systemManufacturer) {
          return originalUA;
        }

        const manufacturer = browser.systemManufacturer.getManufacturer();
        if (manufacturer && manufacturer.toLowerCase() === "samsung") {
          return originalUA.replace("Mobile;", "Mobile; Samsung;");
        }

        return originalUA;
      },
    },
  },
  {
    /*
     * Bug 1622063 - UA override for wp1-ext.usps.gov
     * Webcompat issue #29867 - https://webcompat.com/issues/29867
     *
     * The Job Search site for USPS does not work for Firefox Mobile
     * browsers (a 500 is returned).
     */
    id: "bug1622063",
    platform: "android",
    domain: "wp1-ext.usps.gov",
    bug: "1622063",
    config: {
      matches: ["*://wp1-ext.usps.gov/*"],
      uaTransformer: () => {
        return UAHelpers.getDeviceAppropriateChromeUA();
      },
    },
  },
  {
    /*
     * Bug 1646791 - bancosantander.es - Re-add UA override.
     * Bug 1665129 - *.gruposantander.es - Add wildcard domains.
     * WebCompat issue #33462 - https://webcompat.com/issues/33462
     * SuMo request - https://support.mozilla.org/es/questions/1291085
     *
     * santanderbank expects UA to have 'like Gecko', otherwise it runs
     * xmlDoc.onload whose support has been dropped. It results in missing labels in forms
     * and some other issues.  Adding 'like Gecko' fixes those issues.
     */
    id: "bug1646791",
    platform: "all",
    domain: "santanderbank.com",
    bug: "1646791",
    config: {
      matches: [
        "*://*.bancosantander.es/*",
        "*://*.gruposantander.es/*",
        "*://*.santander.co.uk/*",
      ],
      uaTransformer: originalUA => {
        // The first line related to Firefox 100 is for Bug 1743445.
        // [TODO]: Remove when bug 1743429 gets backed out.
        return UAHelpers.capVersionTo99(originalUA).replace(
          "Gecko",
          "like Gecko"
        );
      },
    },
  },
  {
    /*
     * Bug 1651292 - UA override for www.jp.square-enix.com
     * Webcompat issue #53018 - https://webcompat.com/issues/53018
     *
     * Unless the UA string contains "Chrome 66+", a section of
     * www.jp.square-enix.com will show a never ending LOADING
     * page.
     */
    id: "bug1651292",
    platform: "android",
    domain: "www.jp.square-enix.com",
    bug: "1651292",
    config: {
      matches: ["*://www.jp.square-enix.com/music/sem/page/FF7R/ost/*"],
      uaTransformer: originalUA => {
        return originalUA + " Chrome/83";
      },
    },
  },
  {
    /*
     * Bug 1666754 - Mobile UA override for lffl.org
     * Bug 1665720 - lffl.org article page takes 2x as much time to load on Moto G
     *
     * This site returns desktop site based on server side UA detection.
     * Spoofing as Chrome allows to get mobile experience
     */
    id: "bug1666754",
    platform: "android",
    domain: "lffl.org",
    bug: "1666754",
    config: {
      matches: ["*://*.lffl.org/*"],
      uaTransformer: () => {
        return UAHelpers.getDeviceAppropriateChromeUA();
      },
    },
  },
  {
    /*
     * Bug 1704673 - Add UA override for app.xiaomi.com
     * Webcompat issue #66163 - https://webcompat.com/issues/66163
     *
     * The page isn’t redirecting properly error message received.
     * Spoofing as Chrome makes the page load
     */
    id: "bug1704673",
    platform: "android",
    domain: "app.xiaomi.com",
    bug: "1704673",
    config: {
      matches: ["*://app.xiaomi.com/*"],
      uaTransformer: () => {
        return UAHelpers.getDeviceAppropriateChromeUA();
      },
    },
  },
  {
    /*
     * Bug 1712807 - Add UA override for www.dealnews.com
     * Webcompat issue #39341 - https://webcompat.com/issues/39341
     *
     * The sites shows Firefox a different layout compared to Chrome.
     * Spoofing as Chrome fixes this.
     */
    id: "bug1712807",
    platform: "android",
    domain: "www.dealnews.com",
    bug: "1712807",
    config: {
      matches: ["*://www.dealnews.com/*"],
      uaTransformer: () => {
        return UAHelpers.getDeviceAppropriateChromeUA();
      },
    },
  },
  {
    /*
     * Bug 1719859 - Add UA override for saxoinvestor.fr
     * Webcompat issue #74678 - https://webcompat.com/issues/74678
     *
     * The site blocks Firefox with a server-side UA sniffer. Appending a
     * Chrome version segment to the UA makes it work.
     */
    id: "bug1719859",
    platform: "android",
    domain: "saxoinvestor.fr",
    bug: "1719859",
    config: {
      matches: ["*://*.saxoinvestor.fr/*"],
      uaTransformer: () => {
        return UAHelpers.getDeviceAppropriateChromeUA();
      },
    },
  },
  {
    /*
     * Bug 1743627 - Add UA override for renaud-bray.com
     * Webcompat issue #55276 - https://github.com/webcompat/web-bugs/issues/55276
     *
     * Firefox for Android depends on "Version/" being there in the UA string,
     * or it'll throw a runtime error.
     */
    id: "bug1743627",
    platform: "android",
    domain: "renaud-bray.com",
    bug: "1743627",
    config: {
      matches: ["*://*.renaud-bray.com/*"],
      uaTransformer: originalUA => {
        return originalUA + " Version/0";
      },
    },
  },
  {
    /*
     * Bug 1743751 - Add UA override for slrclub.com
     * Webcompat issue #91373 - https://github.com/webcompat/web-bugs/issues/91373
     *
     * On Firefox Android, the browser is receiving the desktop layout.
     * Spoofing as Chrome works fine.
     */
    id: "bug1743751",
    platform: "android",
    domain: "slrclub.com",
    bug: "1743751",
    config: {
      matches: ["*://*.slrclub.com/*"],
      uaTransformer: () => {
        return UAHelpers.getDeviceAppropriateChromeUA();
      },
    },
  },
  {
    /*
     * Bug 1743754 - Add UA override for slrclub.com
     * Webcompat issue #86839 - https://github.com/webcompat/web-bugs/issues/86839
     *
     * On Firefox Android, the browser is failing a UA parsing on Firefox UA.
     */
    id: "bug1743754",
    platform: "android",
    domain: "workflow.base.vn",
    bug: "1743754",
    config: {
      matches: ["*://workflow.base.vn/*"],
      uaTransformer: () => {
        return UAHelpers.getDeviceAppropriateChromeUA();
      },
    },
  },
  {
    /*
     * Bug 1743429 - Add UA override for sites broken with the Version 100 User Agent
     *
     * Some sites have issues with a UA string with Firefox version 100 or higher,
     * so present as version 99 for now.
     */
    id: "bug1743429",
    platform: "all",
    domain: "Sites with known Version 100 User Agent breakage",
    bug: "1743429",
    config: {
      matches: [
        "*://411.ca/", // #121332
        "*://*.mms.telekom.de/*", // #1800241
        "*://ubank.com.au/*", // #104099
        "*://wifi.sncf/*", // #100194
      ],
      uaTransformer: originalUA => {
        return UAHelpers.capVersionTo99(originalUA);
      },
    },
  },
  {
    /*
     * Bug 1753461 - UA override for serieson.naver.com
     * Webcompat issue #99993 - https://webcompat.com/issues/97298
     *
     * The site locks out Firefox users unless a Chrome UA is given,
     * and locks out Linux users as well (so we use Windows+Chrome).
     */
    id: "bug1753461",
    platform: "desktop",
    domain: "serieson.naver.com",
    bug: "1753461",
    config: {
      matches: ["*://serieson.naver.com/*"],
      uaTransformer: () => {
        return "Mozilla/5.0 (Windows NT 10.0; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/97.0.4692.99 Safari/537.36";
      },
    },
  },
  {
    /*
     * Bug 1771200 - UA override for animalplanet.com
     * Webcompat issue #99993 - https://webcompat.com/issues/103727
     *
     * The videos are not playing and an error message is displayed
     * in Firefox for Android, but work with Chrome UA
     */
    id: "bug1771200",
    platform: "android",
    domain: "animalplanet.com",
    bug: "1771200",
    config: {
      matches: ["*://*.animalplanet.com/video/*"],
      uaTransformer: () => {
        return UAHelpers.getDeviceAppropriateChromeUA();
      },
    },
  },
  {
    /*
     * Bug 1771200 - UA override for lazada.co.id
     * Webcompat issue #106229 - https://webcompat.com/issues/106229
     *
     * The map is not playing and an error message is displayed
     * in Firefox for Android, but work with Chrome UA
     */
    id: "bug1779059",
    platform: "android",
    domain: "lazada.co.id",
    bug: "1779059",
    config: {
      matches: ["*://member-m.lazada.co.id/address/*"],
      uaTransformer: () => {
        return UAHelpers.getDeviceAppropriateChromeUA();
      },
    },
  },
  {
    /*
     * Bug 1778168 - UA override for watch.antennaplus.gr
     * Webcompat issue #106529 - https://webcompat.com/issues/106529
     *
     * The site's content is not loaded unless a Chrome UA is used,
     * and breaks on Linux (so we claim Windows instead in that case).
     */
    id: "bug1778168",
    platform: "desktop",
    domain: "watch.antennaplus.gr",
    bug: "1778168",
    config: {
      matches: ["*://watch.antennaplus.gr/*"],
      uaTransformer: () => {
        return UAHelpers.getDeviceAppropriateChromeUA({
          desktopOS: "nonLinux",
        });
      },
    },
  },
  {
    /*
     * Bug 1776897 - UA override for www.edencast.fr
     * Webcompat issue #106545 - https://webcompat.com/issues/106545
     *
     * The site's podcast audio player does not load unless a Chrome UA is used.
     */
    id: "bug1776897",
    platform: "all",
    domain: "www.edencast.fr",
    bug: "1776897",
    config: {
      matches: ["*://www.edencast.fr/zoomcast*"],
      uaTransformer: () => {
        return UAHelpers.getDeviceAppropriateChromeUA();
      },
    },
  },
  {
    /*
     * Bug 1784361 - UA override for coldwellbankerhomes.com
     * Webcompat issue #108535 - https://webcompat.com/issues/108535
     *
     * An error is thrown due to missing element, unless Chrome UA is used
     */
    id: "bug1784361",
    platform: "android",
    domain: "coldwellbankerhomes.com",
    bug: "1784361",
    config: {
      matches: ["*://*.coldwellbankerhomes.com/*"],
      uaTransformer: () => {
        return UAHelpers.getDeviceAppropriateChromeUA();
      },
    },
  },
  {
    /*
     * Bug 1786404 - UA override for business.help.royalmail.com
     * Webcompat issue #109070 - https://webcompat.com/issues/109070
     *
     * Replacing `Firefox` with `FireFox` to evade one of their UA tests...
     */
    id: "bug1786404",
    platform: "all",
    domain: "business.help.royalmail.com",
    bug: "1786404",
    config: {
      matches: ["*://business.help.royalmail.com/app/webforms/*"],
      uaTransformer: originalUA => {
        return originalUA.replace("Firefox", "FireFox");
      },
    },
  },
  {
    /*
     * Bug 1823966 - UA override for elearning.dmv.ca.gov
     * Original report: https://bugzilla.mozilla.org/show_bug.cgi?id=1823785
     */
    id: "bug1823966",
    platform: "all",
    domain: "elearning.dmv.ca.gov",
    bug: "1823966",
    config: {
      matches: ["*://*.elearning.dmv.ca.gov/*"],
      uaTransformer: () => {
        return UAHelpers.getDeviceAppropriateChromeUA();
      },
    },
  },
  {
    /*
     * Bug 1827678 - UA override for admissions.nid.edu
     * Webcompat issue #65753 - https://webcompat.com/issues/65753
     */
    id: "bug1827678-webc65753",
    platform: "android",
    domain: "admissions.nid.edu",
    bug: "1827678",
    config: {
      matches: ["*://*.admissions.nid.edu/*"],
      uaTransformer: () => {
        return UAHelpers.getDeviceAppropriateChromeUA();
      },
    },
  },
  {
    /*
     * Bug 1827678 - UA override for bankmandiri.co.id
     * Webcompat issue #67924 - https://webcompat.com/issues/67924
     */
    id: "bug1827678-webc67924",
    platform: "all",
    domain: "bankmandiri.co.id",
    bug: "1827678",
    config: {
      matches: ["*://*.bankmandiri.co.id/*"],
      uaTransformer: () => {
        return UAHelpers.getDeviceAppropriateChromeUA();
      },
    },
  },
  {
    /*
     * Bug 1827678 - UA override for frankfred.com
     * Webcompat issue #68007 - https://webcompat.com/issues/68007
     */
    id: "bug1827678-webc68007",
    platform: "android",
    domain: "frankfred.com",
    bug: "1827678",
    config: {
      matches: ["*://*.frankfred.com/*"],
      uaTransformer: () => {
        return UAHelpers.getDeviceAppropriateChromeUA();
      },
    },
  },
  {
    /*
     * Bug 1827678 - UA override for onvue.com
     * Webcompat issue #68520 - https://webcompat.com/issues/68520
     */
    id: "bug1827678-webc68520",
    platform: "all",
    domain: "onvue.com",
    bug: "1827678",
    config: {
      matches: ["*://*.onvue.com/*"],
      uaTransformer: () => {
        return UAHelpers.getDeviceAppropriateChromeUA();
      },
    },
  },
  {
    /*
     * Bug 1827678 - UA override for avizia.com
     * Webcompat issue #68635 - https://webcompat.com/issues/68635
     */
    id: "bug1827678-webc68635",
    platform: "all",
    domain: "avizia.com",
    bug: "1827678",
    config: {
      matches: ["*://*.avizia.com/*"],
      uaTransformer: () => {
        return UAHelpers.getDeviceAppropriateChromeUA({ noFxQuantum: true });
      },
    },
  },
  {
    /*
     * Bug 1827678 - UA override for www.yourtexasbenefits.com
     * Webcompat issue #76785 - https://webcompat.com/issues/76785
     */
    id: "bug1827678-webc76785",
    platform: "android",
    domain: "www.yourtexasbenefits.com",
    bug: "1827678",
    config: {
      matches: ["*://www.yourtexasbenefits.com/*"],
      uaTransformer: () => {
        return UAHelpers.getDeviceAppropriateChromeUA();
      },
    },
  },
  {
    /*
     * Bug 1827678 - UA override for www.free4talk.com
     * Webcompat issue #77727 - https://webcompat.com/issues/77727
     */
    id: "bug1827678-webc77727-ua",
    platform: "android",
    domain: "www.free4talk.com",
    bug: "1827678",
    config: {
      matches: ["*://www.free4talk.com/*"],
      uaTransformer: () => {
        return UAHelpers.getDeviceAppropriateChromeUA();
      },
    },
  },
  {
    /*
     * Bug 1827678 - UA override for watch.indee.tv
     * Webcompat issue #77912 - https://webcompat.com/issues/77912
     */
    id: "bug1827678-webc77912",
    platform: "all",
    domain: "watch.indee.tv",
    bug: "1827678",
    config: {
      matches: ["*://watch.indee.tv/*"],
      uaTransformer: () => {
        return UAHelpers.getDeviceAppropriateChromeUA();
      },
    },
  },
  {
    /*
     * Bug 1827678 - UA override for viewer-ebook.books.com.tw
     * Webcompat issue #80180 - https://webcompat.com/issues/80180
     */
    id: "bug1827678-webc80180",
    platform: "all",
    domain: "viewer-ebook.books.com.tw",
    bug: "1827678",
    config: {
      matches: ["*://viewer-ebook.books.com.tw/*"],
      uaTransformer: () => {
        return UAHelpers.getDeviceAppropriateChromeUA();
      },
    },
  },
  {
    /*
     * Bug 1827678 - UA override for jelly.jd.com
     * Webcompat issue #83269 - https://webcompat.com/issues/83269
     */
    id: "bug1827678-webc83269",
    platform: "all",
    domain: "jelly.jd.com",
    bug: "1827678",
    config: {
      matches: ["*://jelly.jd.com/*"],
      uaTransformer: () => {
        return UAHelpers.getDeviceAppropriateChromeUA();
      },
    },
  },
  {
    /*
     * Bug 1827678 - UA override for kt.com
     * Webcompat issue #119012 - https://webcompat.com/issues/119012
     */
    id: "bug1827678-webc119012",
    platform: "desktop",
    domain: "kt.com",
    bug: "1827678",
    config: {
      matches: ["*://*.kt.com/*"],
      uaTransformer: () => {
        return UAHelpers.getDeviceAppropriateChromeUA();
      },
    },
  },
  {
    /*
     * Bug 1827678 - UA override for oirsa.org
     * Webcompat issue #119402 - https://webcompat.com/issues/119402
     */
    id: "bug1827678-webc119402",
    platform: "all",
    domain: "oirsa.org",
    bug: "1827678",
    config: {
      matches: ["*://*.oirsa.org/*"],
      uaTransformer: () => {
        return UAHelpers.getDeviceAppropriateChromeUA();
      },
    },
  },
  {
    /*
     * Bug 1827678 - UA override for onp.cloud.waterloo.ca
     * Webcompat issue #120450 - https://webcompat.com/issues/120450
     */
    id: "bug1827678-webc120450",
    platform: "all",
    domain: "onp.cloud.waterloo.ca",
    bug: "1827678",
    config: {
      matches: ["*://onp.cloud.waterloo.ca/*"],
      uaTransformer: () => {
        return UAHelpers.getDeviceAppropriateChromeUA();
      },
    },
  },
  {
    /*
     * Bug 1830739 - UA override for casino sites
     *
     * The sites are showing unsupported message with the same UI
     */
    id: "bug1830739",
    platform: "android",
    domain: "casino sites",
    bug: "1830739",
    config: {
      matches: [
        "*://*.captainjackcasino.com/*", // 79490
        "*://*.casinoextreme.eu/*", // 118175
        "*://*.cryptoloko.com/*", // 117911
        "*://*.123lobbygames.com/*", // 120027
        "*://*.planet7casino.com/*", // 120609
        "*://*.yebocasino.co.za/*", // 88409
        "*://*.yabbycasino.com/*", // 108025
      ],
      uaTransformer: () => {
        return UAHelpers.getDeviceAppropriateChromeUA();
      },
    },
  },
  {
    /*
     * Bug 1830821 - UA override for webcartop.jp
     * Webcompat issue #113663 - https://webcompat.com/issues/113663
     */
    id: "bug1830821-webc113663",
    platform: "android",
    domain: "webcartop.jp",
    bug: "1830821",
    config: {
      matches: ["*://*.webcartop.jp/*"],
      uaTransformer: () => {
        return UAHelpers.getDeviceAppropriateChromeUA();
      },
    },
  },
  {
    /*
     * Bug 1830821 - UA override for enjoy.point.auone.jp
     * Webcompat issue #90981 - https://webcompat.com/issues/90981
     */
    id: "bug1830821-webc90981",
    platform: "android",
    domain: "enjoy.point.auone.jp",
    bug: "1830821",
    config: {
      matches: ["*://enjoy.point.auone.jp/*"],
      uaTransformer: () => {
        return UAHelpers.getDeviceAppropriateChromeUA();
      },
    },
  },
  {
    /*
     * Bug 1836109 - UA override for watch.tonton.com.my
     *
     * The site's content is not loaded unless a Chrome UA is used.
     */
    id: "bug1836109",
    platform: "all",
    domain: "watch.tonton.com.my",
    bug: "1836109",
    config: {
      matches: ["*://watch.tonton.com.my/*"],
      uaTransformer: () => {
        return UAHelpers.getDeviceAppropriateChromeUA();
      },
    },
  },
  {
    /*
     * Bug 1836135 - UA override for gts-pro.sdimedia.com
     *
     * The site's content is not loaded without a Chrome UA spoof.
     */
    id: "bug1836135",
    platform: "all",
    domain: "gts-pro.sdimedia.com",
    bug: "1836135",
    config: {
      matches: ["*://gts-pro.sdimedia.com/*"],
      uaTransformer: originalUA => {
        return originalUA.replace("Firefox/", "Fx/") + " Chrome/113.0.0.0";
      },
    },
  },
  {
    /*
     * Bug 1836140 - UA override for indices.iriworldwide.com
     *
     * The site's content is not loaded without a UA spoof.
     */
    id: "bug1836140",
    platform: "all",
    domain: "indices.iriworldwide.com",
    bug: "1836140",
    config: {
      matches: ["*://indices.iriworldwide.com/covid19/*"],
      uaTransformer: originalUA => {
        return originalUA.replace("Firefox/", "Fx/");
      },
    },
  },
  {
    /*
     * Bug 1836178 - UA override for atracker.pro
     *
     * The site's content is not loaded without a Chrome UA spoof.
     */
    id: "bug1836178",
    platform: "all",
    domain: "atracker.pro",
    bug: "1836178",
    config: {
      matches: ["*://atracker.pro/*"],
      uaTransformer: originalUA => {
        return originalUA + " Chrome/113.0.0.0";
      },
    },
  },
  {
    /*
     * Bug 1850455 - UA override for frontgate.com
     * Webcompat issue #36277 - https://webcompat.com/issues/36277
     *
     * The site is showing a desktop view to Firefox mobile user-agents
     */
    id: "bug1850455",
    platform: "android",
    domain: "frontgate.com",
    bug: "1850455",
    config: {
      matches: ["*://*.frontgate.com/*"],
      uaTransformer: () => {
        return UAHelpers.getDeviceAppropriateChromeUA();
      },
    },
  },
  {
    /*
     * Bug 1855088 - UA override for hrmis2.eghrmis.gov.my
     * Webcompat issue #125039 - https://webcompat.com/issues/125039
     *
     * hrmis2.eghrmis.gov.my showing unsupported message for Firefox users
     * Spoofing as Chrome allows to access the page
     */
    id: "bug1855088",
    platform: "all",
    domain: "hrmis2.eghrmis.gov.my",
    bug: "1855088",
    config: {
      matches: ["*://hrmis2.eghrmis.gov.my/*"],
      uaTransformer: () => {
        return UAHelpers.getDeviceAppropriateChromeUA();
      },
    },
  },
  {
    /*
     * Bug 1855102 - UA override for my.southerncross.co.nz
     * Webcompat issue #121877 - https://webcompat.com/issues/121877
     *
     * Spoofing as Chrome for Android allows to access the page
     */
    id: "bug1855102",
    platform: "android",
    domain: "my.southerncross.co.nz",
    bug: "1855102",
    config: {
      matches: ["*://my.southerncross.co.nz/*"],
      uaTransformer: () => {
        return UAHelpers.getDeviceAppropriateChromeUA();
      },
    },
  },
  {
    /*
     * Bug 1864999 - UA override for autotrader.ca
     * Webcompat issue #126822 - https://webcompat.com/issues/126822
     *
     * Spoofing as Chrome for Android makes filters work on the site
     */
    id: "bug1864999",
    platform: "android",
    domain: "autotrader.ca",
    bug: "1864999",
    config: {
      matches: ["*://*.autotrader.ca/*"],
      uaTransformer: () => {
        return UAHelpers.getDeviceAppropriateChromeUA();
      },
    },
  },
  {
    /*
     * Bug 1865000 - UA override for bmo.com
     * Webcompat issue #127620 - https://webcompat.com/issues/127620
     *
     * Spoofing as Chrome removes the unsupported message and allows
     * to proceed with application
     */
    id: "bug1865000",
    platform: "all",
    domain: "bmo.com",
    bug: "1865000",
    config: {
      matches: ["*://*.bmo.com/main/personal/*/getting-started/*"],
      uaTransformer: () => {
        return UAHelpers.getDeviceAppropriateChromeUA();
      },
    },
  },
  {
    /*
     * Bug 1865004 - UA override for digimart.net
     * Webcompat issue #126647 - https://webcompat.com/issues/126647
     *
     * The site returns desktop layout on Firefox for Android
     */
    id: "bug1865004",
    platform: "android",
    domain: "digimart.net",
    bug: "1865004",
    config: {
      matches: ["*://*.digimart.net/*"],
      uaTransformer: () => {
        return UAHelpers.getDeviceAppropriateChromeUA();
      },
    },
  },
  {
    /*
     * Bug 1865007 - UA override for portal.circle.ms
     * Webcompat issue #127739 - https://webcompat.com/issues/127739
     *
     * The site returns desktop layout on Firefox for Android
     */
    id: "bug1865007",
    platform: "android",
    domain: "portal.circle.ms",
    bug: "1865007",
    config: {
      matches: ["*://*.circle.ms/*"],
      uaTransformer: () => {
        return UAHelpers.getDeviceAppropriateChromeUA();
      },
    },
  },
  {
    /*
     * Bug 1884779 - UA override for
     * Webcompat issue #134287 - https://webcompat.com/issues/134287
     *
     * The site returns desktop layout on Firefox for Android
     */
    id: "bug1884779",
    platform: "android",
    domain: "memurlar.net",
    bug: "1884779",
    config: {
      matches: ["*://*.memurlar.net/*"],
      uaTransformer: () => {
        return UAHelpers.getDeviceAppropriateChromeUA();
      },
    },
  },
  {
    /*
     * Bug 1898531 - UA override for www.cwb.digital
     * Webcompat issue #137285 - https://webcompat.com/issues/137285
     *
     * Spoofing as Chrome makes the login form work
     */
    id: "bug1898531",
    platform: "all",
    domain: "cwb.digital",
    bug: "1898531",
    config: {
      matches: ["*://*.cwb.digital/*"],
      uaTransformer: () => {
        return UAHelpers.getDeviceAppropriateChromeUA();
      },
    },
  },
  {
    /*
     * Bug 1842767 - UA override for passport.bilibili.com
     *
     * Spoofing as Chrome makes the login page use a mobile layout.
     */
    id: "bug1842767",
    platform: "android",
    domain: "passport.bilibili.com",
    bug: "1842767",
    config: {
      matches: ["*://*.passport.bilibili.com/*"],
      uaTransformer: () => {
        return UAHelpers.getDeviceAppropriateChromeUA();
      },
    },
  },
  {
    /*
     * Bug 1896354 - UA override for my.rhinoshield.fr
     *
     * Sites only supports Chrome, but seems to work with a UA spoof.
     */
    id: "bug1896354",
    platform: "all",
    domain: "my.rhinoshield.fr",
    bug: "1896354",
    config: {
      matches: ["*://my.rhinoshield.fr/*"],
      uaTransformer: () => {
        return UAHelpers.getDeviceAppropriateChromeUA();
      },
    },
  },
  {
    /*
     * Bug 1813177 - UA override for rbi.org.in
     *
     * The site endlessly redirects for a Firefox mobile UA string.
     */
    id: "bug1813177",
    platform: "android",
    domain: "m.rbi.org.in",
    bug: "1813177",
    config: {
      matches: ["*://m.rbi.org.in/*"],
      uaTransformer: () => {
        return UAHelpers.getDeviceAppropriateChromeUA();
      },
    },
  },
  {
    /*
     * Bug 1903970 - UA override for unimedlitoral.com.br
     * Webcompat issue #138342 - https://webcompat.com/issues/138342
     *
     * Site blocks Firefox, but seems to work with a UA spoof.
     */
    id: "bug1903970",
    platform: "all",
    domain: "www.unimedlitoral.com.br",
    bug: "1903970",
    config: {
      matches: ["*://www.unimedlitoral.com.br/agendaonline/*"],
      uaTransformer: () => {
        return UAHelpers.getDeviceAppropriateChromeUA();
      },
    },
  },
  {
    /*
     * Bug 1902474 - UA override for lg.jio.com
     * Webcompat issue #124681 - https://webcompat.com/issues/124681
     *
     * Site incorrectly blocks Firefox on Android. A desktop UA works.
     */
    id: "bug1902474",
    platform: "android",
    domain: "lg.jio.com",
    bug: "1902474",
    config: {
      matches: ["*://lg.jio.com/*"],
      uaTransformer: originalUA => {
        return originalUA.replace(/ (Mobile|Tablet);/, "");
      },
    },
  },
  {
    /*
     * Bug 1909448 - UA override for fire.honeywell.com
     *
     * Site doesn't load on Firefox, but works fine with a UA spoof.
     */
    id: "bug1909448",
    platform: "all",
    domain: "fire.honeywell.com",
    bug: "1909448",
    config: {
      matches: ["*://fire.honeywell.com/*"],
      uaTransformer: () => {
        return UAHelpers.getDeviceAppropriateChromeUA();
      },
    },
  },
  {
    /*
     * Bug 1899937 - UA override for plus.nhk.jp
     * Webcompat issue #103463 - https://webcompat.com/issues/103463
     *
     * Site blocks Firefox, so a UA spoof and an intervention is needed.
     */
    id: "bug1899937-ua",
    platform: "all",
    domain: "plus.nhk.jp",
    bug: "1899937",
    config: {
      matches: ["*://plus.nhk.jp/*"],
      uaTransformer: () => {
        return UAHelpers.getDeviceAppropriateChromeUA();
      },
    },
  },
  {
    /*
     * Bug 1897724 - UA override for app.homewyse.com
     * Webcompat issue #137164 - https://webcompat.com/issues/137164
     *
     * Site blocks Firefox, but seems to work with a UA spoof.
     */
    id: "bug1897724",
    platform: "all",
    domain: "app.homewyse.com",
    bug: "1897724",
    config: {
      matches: ["*://app.homewyse.com/*"],
      uaTransformer: () => {
        return UAHelpers.getDeviceAppropriateChromeUA();
      },
    },
  },
  {
    /*
     * Bug 1898923 - UA override for trade-in.vodafone.com
     * Webcompat issue #127158 - https://webcompat.com/issues/127158
     *
     * Site blocks Firefox, but seems to work with a UA spoof.
     */
    id: "bug1898923",
    platform: "all",
    domain: "trade-in.vodafone.com",
    bug: "1898923",
    config: {
      matches: ["*://trade-in.vodafone.com/*"],
      uaTransformer: () => {
        return UAHelpers.getDeviceAppropriateChromeUA();
      },
    },
  },
  {
    /*
     * Bug 1899067 - UA override for game.granbluefantasy.jp
     * Webcompat issue #43155 - https://webcompat.com/issues/43155
     *
     * Site blocks Firefox, but seems to work with a UA spoof.
     */
    id: "bug1899067",
    platform: "desktop",
    domain: "game.granbluefantasy.jp",
    bug: "1899067",
    config: {
      matches: ["*://game.granbluefantasy.jp/*"],
      uaTransformer: () => {
        return UAHelpers.getDeviceAppropriateChromeUA();
      },
    },
  },
  {
    /*
     * Bug 1898941 - UA override for events.webinar.ru
     * Webcompat issue #121871 - https://webcompat.com/issues/121871
     *
     * Site blocks Firefox, but seems to work with a UA spoof.
     */
    id: "bug1898941",
    platform: "all",
    domain: "events.webinar.ru",
    bug: "1898941",
    config: {
      matches: ["*://events.webinar.ru/*"],
      uaTransformer: () => {
        return UAHelpers.getDeviceAppropriateChromeUA();
      },
    },
  },
  {
    /*
     * Bug 1919004 - UA override for www.editoracontexto.com.br
     * Webcompat issue #141641 - https://webcompat.com/issues/141641
     *
     * Site presents its desktop CSS to Firefox on Android.
     */
    id: "bug1919004",
    platform: "android",
    domain: "www.editoracontexto.com.br",
    bug: "1919004",
    config: {
      matches: ["*://www.editoracontexto.com.br/*"],
      uaTransformer: () => {
        return UAHelpers.getDeviceAppropriateChromeUA();
      },
    },
  },
  {
    /*
     * Bug 1921410 - UA override for beta.maps.apple.com
     * Webcompat issue #140205 - https://webcompat.com/issues/140205
     *
     * Apple Maps beta artifically blocks Linux.
     */
    id: "bug1921410",
    platform: "linux",
    domain: "beta.maps.apple.com",
    bug: "1921410",
    config: {
      matches: ["*://beta.maps.apple.com/*"],
      uaTransformer: originalUA => {
        return UAHelpers.getWindowsUA(originalUA);
      },
    },
  },
  {
    /*
     * Bug 1899948 - UA override for tv.partner.co.il
     * Webcompat issue #64703 - https://webcompat.com/issues/64703
     *
     * Site blocks Firefox, but seems to work with a UA spoof.
     */
    id: "1899948",
    platform: "desktop",
    domain: "tv.partner.co.il",
    bug: "1899948",
    config: {
      matches: ["*://tv.partner.co.il/*"],
      uaTransformer: () => {
        return UAHelpers.getDeviceAppropriateChromeUA();
      },
    },
  },
  {
    /*
     * Bug 1844503 - UA override for nicochannel.jp and similar video sites
     * Webcompat issue #124511 - https://webcompat.com/issues/124511
     * Webcompat issue #124463 - https://webcompat.com/issues/124463
     *
     * These sites' video APIs block Firefox, yet they allow a Chrome UA.
     * (Note that some are NSFW).
     */
    id: "1844503",
    platform: "all",
    domain: "nicochannel.jp",
    bug: "1844503",
    config: {
      matches: [
        "*://api.ado-dokidokihimitsukichi-daigakuimo.com/fc/video_pages/*",
        "*://api.canan8181.com/fc/video_pages/*",
        "*://api.gs-ch.com/fc/video_pages/*", // 124511
        "*://api.keisuke-ueda.jp/fc/video_pages/*",
        "*://api.kemomimirefle.net/fc/video_pages/*",
        "*://api.nicochannel.jp/fc/video_pages/*", // 124463
        "*://api.p-jinriki-fc.com/fc/video_pages/*",
        "*://api.pizzaradio.jp/fc/video_pages/*",
        "*://api.rnqq.jp/fc/video_pages/*",
        "*://api.ryogomatsumaru.com/fc/video_pages/*",
        "*://api.takahashifumiya.com/fc/video_pages/*",
        "*://api.yamingfc.net/fc/video_pages/*",
      ],
      uaTransformer: () => {
        return UAHelpers.getDeviceAppropriateChromeUA();
      },
    },
  },
  {
    /*
     * Bug 1912923 - UA override for livelesson.class.com/class
     *
     * Site blocks Firefox, but seems to work with a UA spoof.
     */
    id: "1912923",
    platform: "all",
    domain: "livelesson.class.com",
    bug: "1912923",
    config: {
      matches: ["*://livelesson.class.com/class/*"],
      uaTransformer: () => {
        return UAHelpers.getDeviceAppropriateChromeUA();
      },
    },
  },
  {
    /*
     * Bug 1899945 - UA override for nytimes.com Modern Love Questions
     * Webcompat issue #41961 - https://webcompat.com/issues/41961
     *
     * Site blocks Firefox on Android, but seems to work with a UA spoof.
     */
    id: "1899945",
    platform: "android",
    domain: "nytimes.com Modern Love Questions",
    bug: "1899945",
    config: {
      matches: ["*://www.nytimes.com/interactive/projects/modern-love/*"],
      uaTransformer: () => {
        return UAHelpers.getDeviceAppropriateChromeUA();
      },
    },
  },
  {
    /*
     * Bug 1898992 - UA override for asp.attenix.co.il
     * Webcompat issue #101497 - https://webcompat.com/issues/101497
     *
     * Site blocks Firefox, but seems to work with a UA spoof.
     */
    id: "1898992",
    platform: "all",
    domain: "asp.attenix.co.il",
    bug: "1898992",
    config: {
      matches: ["*://asp.attenix.co.il/*"],
      uaTransformer: () => {
        return UAHelpers.getDeviceAppropriateChromeUA();
      },
    },
  },
  {
    /*
     * Bug 1899060 - UA override for wbte.drcedirect.com
     * Webcompat issue #66885 - https://webcompat.com/issues/66885
     *
     * Site blocks Firefox, but seems to work with a UA spoof.
     */
    id: "1899060",
    platform: "desktop",
    domain: "wbte.drcedirect.com",
    bug: "1899060",
    config: {
      matches: ["*://wbte.drcedirect.com/*"],
      uaTransformer: () => {
        return UAHelpers.getDeviceAppropriateChromeUA();
      },
    },
  },
  {
    /*
     * Bug 1898988 - UA override for prudential.com.hk
     * Webcompat issue #105184 - https://webcompat.com/issues/105184
     *
     * Site blocks Firefox, but seems to work with a UA spoof. The site also
     * has flawed UA detection for Linux, causing its page to not load correctly
     * unless we spoof as a different OS.
     */
    id: "1898988",
    platform: "all",
    domain: "prudential.com.hk",
    bug: "1898988",
    config: {
      matches: ["*://*.prudential.com.hk/*"],
      uaTransformer: originalUA => {
        const override = UAHelpers.getDeviceAppropriateChromeUA();
        if (originalUA.includes("Linux") && !originalUA.includes("Android")) {
          return override.replace(
            /\(.*Linux.*\)/,
            "(Macintosh; Intel Mac OS X 10.15)"
          );
        }
        return override;
      },
    },
  },
  {
    /*
     * Bug 1898994 - UA override for eportal.uestc.edu.cn
     * Webcompat issue #100234 - https://webcompat.com/issues/100234
     *
     * Site blocks Firefox, but seems to work with a UA spoof.
     */
    id: "1898994",
    platform: "all",
    domain: "eportal.uestc.edu.cn",
    bug: "1898994",
    config: {
      matches: ["*://eportal.uestc.edu.cn/*"],
      uaTransformer: () => {
        return UAHelpers.getDeviceAppropriateChromeUA();
      },
    },
  },
  {
    /*
     * Bug 1898960 - UA override for hrm.online.comarch.pl
     * Webcompat issue #117868 - https://webcompat.com/issues/117868
     *
     * Site blocks Firefox, but seems to work with a UA spoof.
     */
    id: "1898960",
    platform: "android",
    domain: "hrm.online.comarch.pl",
    bug: "1898960",
    config: {
      matches: ["*://hrm.online.comarch.pl/*"],
      uaTransformer: () => {
        return UAHelpers.getDeviceAppropriateChromeUA();
      },
    },
  },
  {
    /*
     * Bug 1898938 - UA override for conference.amwell.com
     * Webcompat issue #122754 - https://webcompat.com/issues/122754
     *
     * The site requires a Chrome UA or it will block the browser.
     */
    id: "bug1898938",
    platform: "all",
    domain: "conference.amwell.com",
    bug: "1898938",
    config: {
      matches: ["*://conference.amwell.com/*"],
      uaTransformer: () => {
        return UAHelpers.getDeviceAppropriateChromeUA({ noFxQuantum: true });
      },
    },
  },
  {
    /*
     * Bug 1913759 - UA override for playblackdesert.com
     * Webcompat issue #140522 - https://webcompat.com/issues/140522
     *
     * The site says Firefox is unsupported, yet seems to work with a Chrome UA.
     * (The issue does not seem to exist on Android).
     */
    id: "bug1913759",
    platform: "desktop",
    domain: "playblackdesert.com",
    bug: "1913759",
    config: {
      matches: ["*://eu-trade.naeu.playblackdesert.com/*"],
      uaTransformer: () => {
        return UAHelpers.getDeviceAppropriateChromeUA();
      },
    },
  },
  {
    /*
     * Bug 1899066 - UA override for app.bookcreator.com
     * Webcompat issue #52462 - https://webcompat.com/issues/52462
     *
     * The site says Firefox is unsupported, yet seems to work with a Chrome UA.
     */
    id: "bug1899066",
    platform: "all",
    domain: "bookcreator.com",
    bug: "1899066",
    config: {
      matches: ["*://app.bookcreator.com/*"],
      uaTransformer: () => {
        return UAHelpers.getDeviceAppropriateChromeUA();
      },
    },
  },
  {
    /*
     * Bug 1899072 - UA override for passpoint.boingo.com
     * Webcompat issue #29373 - https://webcompat.com/issues/29373
     *
     * The site supports HotSpot 2.0 devices, but only allows MacOSX
     * Mavericks, iOS 7, or Android Nexus 6 devices without a UA override.
     */
    id: "bug1899072",
    platform: "all",
    domain: "passpoint.boingo.com",
    bug: "1899072",
    config: {
      matches: ["*://passpoint.boingo.com/*"],
      uaTransformer: (originalUA, platform) => {
        if (platform == "android") {
          return "Mozilla/5.0 (Linux; Android 10; K) AppleWebKit/537.36 (KHTML, like Gecko) FxQuantum Chrome/130.0.6723.102 Mobile Safari/537.36";
        } else if (platform == "macos") {
          return UAHelpers.getDeviceAppropriateChromeUA().replace(
            / Chrome\/[0-9.]*/,
            ""
          );
        }
        return "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15 (KHTML, like Gecko) FxQuantum Version/18.1 Safari/605.1.15";
      },
    },
  },
  {
    /*
     * Bug 1899929 - UA override for selecionases.saude.pe.gov.br
     * Webcompat issue #127079 - https://webcompat.com/issues/127079
     *
     * The site says Firefox is unsupported, yet seems to work with a Chrome UA.
     */
    id: "bug1899929",
    platform: "all",
    domain: "selecionases.saude.pe.gov.br",
    bug: "1899929",
    config: {
      matches: ["*://selecionases.saude.pe.gov.br/*"],
      uaTransformer: () => {
        return UAHelpers.getDeviceAppropriateChromeUA();
      },
    },
  },
  {
    /*
     * Bug 1930632 - UA override for support.hp.com
     *
     * The site presents Firefox with a version of the page which
     * does not load properly. A Chrome UA gets a working version.
     */
    id: "bug1930632",
    platform: "desktop",
    domain: "support.hp.com",
    bug: "1930632",
    config: {
      matches: ["*://support.hp.com/*"],
      uaTransformer: () => {
        return UAHelpers.getDeviceAppropriateChromeUA();
      },
    },
  },
];

module.exports = AVAILABLE_UA_OVERRIDES;
