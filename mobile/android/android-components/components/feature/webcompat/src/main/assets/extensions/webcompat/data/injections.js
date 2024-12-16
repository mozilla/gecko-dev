/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* globals module, require */

// This is a hack for the tests.
if (typeof InterventionHelpers === "undefined") {
  var InterventionHelpers = require("../lib/intervention_helpers");
}

/**
 * For detailed information on our policies, and a documention on this format
 * and its possibilites, please check the Mozilla-Wiki at
 *
 * https://wiki.mozilla.org/Compatibility/Go_Faster_Addon/Override_Policies_and_Workflows#User_Agent_overrides
 */
const AVAILABLE_INJECTIONS = [
  {
    id: "testbed-injection",
    platform: "all",
    domain: "webcompat-addon-testbed.herokuapp.com",
    bug: "0000000",
    hidden: true,
    contentScripts: {
      matches: ["*://webcompat-addon-testbed.herokuapp.com/*"],
      css: [
        {
          file: "injections/css/bug0000000-testbed-css-injection.css",
        },
      ],
      js: [
        {
          file: "injections/js/bug0000000-testbed-js-injection.js",
        },
      ],
    },
  },
  {
    id: "bug1452707",
    platform: "all",
    domain: "ib.absa.co.za",
    bug: "1452707",
    contentScripts: {
      matches: ["https://ib.absa.co.za/*"],
      js: [
        {
          file: "injections/js/bug1452707-window.controllers-shim-ib.absa.co.za.js",
        },
      ],
    },
  },
  {
    id: "bug1457335",
    platform: "desktop",
    domain: "histography.io",
    bug: "1457335",
    contentScripts: {
      matches: ["*://histography.io/*"],
      js: [
        {
          file: "injections/js/bug1457335-histography.io-ua-change.js",
        },
      ],
    },
  },
  {
    id: "bug1472075",
    platform: "desktop",
    domain: "bankofamerica.com",
    bug: "1472075",
    contentScripts: {
      matches: [
        "*://*.bankofamerica.com/*",
        "*://*.ml.com/*", // #120104
      ],
      js: [
        {
          file: "injections/js/bug1472075-bankofamerica.com-ua-change.js",
        },
      ],
    },
  },
  {
    id: "bug1579159",
    platform: "android",
    domain: "m.tailieu.vn",
    bug: "1579159",
    contentScripts: {
      matches: ["*://m.tailieu.vn/*", "*://m.elib.vn/*"],
      js: [
        {
          file: "injections/js/bug1579159-m.tailieu.vn-pdfjs-worker-disable.js",
        },
      ],
      allFrames: true,
    },
  },
  {
    id: "bug1575000",
    platform: "all",
    domain: "apply.lloydsbank.co.uk",
    bug: "1575000",
    contentScripts: {
      matches: ["*://apply.lloydsbank.co.uk/*"],
      css: [
        {
          file: "injections/css/bug1575000-apply.lloydsbank.co.uk-radio-buttons-fix.css",
        },
      ],
    },
  },
  {
    id: "bug1610344",
    platform: "all",
    domain: "directv.com.co",
    bug: "1610344",
    contentScripts: {
      matches: [
        "https://*.directv.com.co/*",
        "https://*.directv.com.ec/*", // bug 1827706
      ],
      css: [
        {
          file: "injections/css/bug1610344-directv.com.co-hide-unsupported-message.css",
        },
      ],
    },
  },
  {
    id: "bug1644830",
    platform: "desktop",
    domain: "usps.com",
    bug: "1644830",
    contentScripts: {
      matches: ["https://*.usps.com/*"],
      css: [
        {
          file: "injections/css/bug1644830-missingmail.usps.com-checkboxes-not-visible.css",
        },
      ],
    },
  },
  {
    id: "bug1886293",
    platform: "desktop",
    domain: "Future PLC websites",
    bug: "1886293",
    contentScripts: {
      matches: [
        "*://*.androidcentral.com/*",
        "*://*.creativebloq.com/*",
        "*://*.cyclingnews.com/*",
        "*://*.gamesradar.com/*",
        "*://*.imore.com/*",
        "*://*.itpro.com/*",
        "*://*.laptopmag.com/*",
        "*://*.livescience.com/*",
        "*://*.loudersound.com/*",
        "*://*.musicradar.com/*",
        "*://*.pcgamer.com/*",
        "*://*.space.com/*",
        "*://*.techradar.com/*",
        "*://*.tomshardware.com/*",
        "*://*.windowscentral.com/*",
      ],
      css: [
        {
          file: "injections/css/bug1886293-futurePLC-sites-trending_scrollbars.css",
        },
      ],
    },
  },
  {
    id: "bug1694470",
    platform: "android",
    domain: "m.myvidster.com",
    bug: "1694470",
    contentScripts: {
      matches: ["https://m.myvidster.com/*"],
      css: [
        {
          file: "injections/css/bug1694470-myvidster.com-content-not-shown.css",
        },
      ],
    },
  },
  {
    id: "bug1707795",
    platform: "desktop",
    domain: "Office Excel spreadsheets",
    bug: "1707795",
    contentScripts: {
      matches: [
        "*://*.live.com/*",
        "*://*.office.com/*",
        "*://*.sharepoint.com/*",
      ],
      css: [
        {
          file: "injections/css/bug1707795-office365-sheets-overscroll-disable.css",
        },
      ],
      allFrames: true,
    },
  },
  {
    id: "bug1722955",
    platform: "android",
    domain: "frontgate.com",
    bug: "1722955",
    contentScripts: {
      matches: ["*://*.frontgate.com/*"],
      js: [
        {
          file: "lib/ua_helpers.js",
        },
        {
          file: "injections/js/bug1722955-frontgate.com-ua-override.js",
        },
      ],
      allFrames: true,
    },
  },
  {
    id: "bug1724868",
    platform: "android",
    domain: "news.yahoo.co.jp",
    bug: "1724868",
    contentScripts: {
      matches: ["*://news.yahoo.co.jp/articles/*", "*://s.yimg.jp/*"],
      js: [
        {
          file: "injections/js/bug1724868-news.yahoo.co.jp-ua-override.js",
        },
      ],
      allFrames: true,
    },
  },
  {
    id: "bug1741234",
    platform: "all",
    domain: "patient.alphalabs.ca",
    bug: "1741234",
    contentScripts: {
      matches: ["*://patient.alphalabs.ca/*"],
      css: [
        {
          file: "injections/css/bug1741234-patient.alphalabs.ca-height-fix.css",
        },
      ],
    },
  },
  {
    id: "bug1739489",
    platform: "desktop",
    domain: "Sites using draft.js",
    bug: "1739489",
    checkIfNeeded() {
      return !window.TextEvent;
    },
    contentScripts: {
      matches: [
        "*://draftjs.org/*", // Bug 1739489
        "*://www.facebook.com/*", // Bug 1739489
        "*://twitter.com/*", // Bug 1776229
        "*://mobile.twitter.com/*", // Bug 1776229
        "*://x.com/*", // Bug 1776229
        "*://mobile.x.com/*", // Bug 1776229
      ],
      js: [
        {
          file: "injections/js/bug1739489-draftjs-beforeinput.js",
        },
      ],
    },
  },
  {
    id: "bug11769762",
    platform: "all",
    domain: "tiktok.com",
    bug: "1769762",
    contentScripts: {
      matches: ["https://www.tiktok.com/*"],
      js: [
        {
          file: "injections/js/bug1769762-tiktok.com-plugins-shim.js",
        },
      ],
    },
  },
  {
    id: "bug1770962",
    platform: "all",
    domain: "coldwellbankerhomes.com",
    bug: "1770962",
    contentScripts: {
      matches: ["*://*.coldwellbankerhomes.com/*"],
      css: [
        {
          file: "injections/css/bug1770962-coldwellbankerhomes.com-image-height.css",
        },
      ],
    },
  },
  {
    id: "bug1774005",
    platform: "all",
    domain: "Sites relying on window.InstallTrigger",
    bug: "1774005",
    contentScripts: {
      matches: [
        "*://*.ambrahealth.com/*", // Bug 1930429
        "*://*.webex.com/*", // Bug 1788934
        "*://ifcinema.institutfrancais.com/*", // Bug 1806423
        "*://islamionline.islamicbank.ps/*", // Bug 1821439
        "*://www.schoolnutritionandfitness.com/*", // Bug 1793761
      ],
      js: [
        {
          file: "injections/js/bug1774005-installtrigger-shim.js",
        },
      ],
      allFrames: true,
    },
  },
  {
    id: "bug1859617",
    platform: "all",
    domain: "Sites relying on there being no window.InstallTrigger",
    bug: "1859617",
    contentScripts: {
      matches: [
        "*://*.stallionexpress.ca/*", // Bug 1859617
      ],
      js: [
        {
          file: "injections/js/bug1859617-installtrigger-removal-shim.js",
        },
      ],
      allFrames: true,
    },
  },
  {
    id: "bug1784199",
    platform: "all",
    domain: "Sites based on Entrata Platform",
    bug: "1784199",
    contentScripts: {
      matches: [
        "*://*.7streetbrownstones.com/*", // #129553
        "*://*.aptsovation.com/*", // #100131
        "*://*.arboretumapartments.com/*", // bugzilla 1894682
        "*://*.avanabayview.com/*", // #118617
        "*://*.breakpointeandcoronado.com/*", // #117735
        "*://*.courtsatspringmill.com/*", // #128404
        "*://*.fieldstoneamherst.com/*", // #132974
        "*://*.flatsatshadowglen.com/*", // #121799
        "*://*.gslbriarcreek.com/*", // #126401
        "*://*.hpixeniatrails.com/*", // #131703
        "*://*.liveobserverpark.com/*", // #105244
        "*://*.liveupark.com/*", // #121083
        "*://*.midwayurban.com/*", // #116523
        "*://*.nhcalaska.com/*", // #107833
        "*://*.prospectportal.com/*", // #115206
        "*://*.securityproperties.com/*", // #107969
        "*://*.thefoundryat41st.com/*", // #128994
        "*://*.theloftsorlando.com/*", // #101496
        "*://*.thepointatkingston.com/*", // #139030
        "*://*.vanallenapartments.com/*", // #120056
      ],
      css: [
        {
          file: "injections/css/bug1784199-entrata-platform-unsupported.css",
        },
      ],
      allFrames: true,
    },
  },
  {
    id: "bug1799968",
    platform: "linux",
    domain: "www.samsung.com",
    bug: "1799968",
    contentScripts: {
      matches: ["*://www.samsung.com/*"],
      js: [
        {
          file: "injections/js/bug1799968-www.samsung.com-appVersion-linux-fix.js",
        },
      ],
    },
  },
  {
    id: "bug1860417",
    platform: "android",
    domain: "www.samsung.com",
    bug: "1860417",
    contentScripts: {
      matches: ["*://www.samsung.com/*"],
      js: [
        {
          file: "injections/js/bug1799968-www.samsung.com-appVersion-linux-fix.js",
        },
      ],
    },
  },
  {
    id: "bug1799980",
    platform: "all",
    domain: "healow.com",
    bug: "1799980",
    contentScripts: {
      matches: ["*://healow.com/*"],
      js: [
        {
          file: "injections/js/bug1799980-healow.com-infinite-loop-fix.js",
        },
      ],
    },
  },
  {
    id: "bug1448747",
    platform: "android",
    domain: "FastClick breakage",
    bug: "1448747",
    contentScripts: {
      matches: [
        "*://*.co2meter.com/*", // 10959
        "*://*.franmar.com/*", // 27273
        "*://*.themusiclab.org/*", // 49667
        "*://*.oregonfoodbank.org/*", // 53203
        "*://bathpublishing.com/*", // 100145
        "*://dylantalkstone.com/*", // 101356
        "*://renewd.com.au/*", // 104998
        "*://*.lamudi.co.id/*", // 106767
        "*://weaversofireland.com/*", // 116816
        "*://*.iledefrance-mobilites.fr/*", // 117344
        "*://*.lawnmowerpartsworld.com/*", // 117577
        "*://*.discountcoffee.co.uk/*", // 118757
        "*://*.arcsivr.com/*", // 120716
        "*://drafthouse.com/*", // 126385
        "*://*.lafoodbank.org/*", // 127006
        "*://rutamayacoffee.com/*", // 129353
        "*://give.umrelief.org/give/*", // bugzilla 1916407
      ],
      js: [
        {
          file: "injections/js/bug1448747-fastclick-shim.js",
        },
      ],
    },
  },
  {
    id: "bug1818818",
    platform: "android",
    domain: "FastClick breakage - legacy",
    bug: "1818818",
    contentScripts: {
      matches: [
        "*://*.chatiw.com/*", // 5544
        "*://*.wellcare.com/*", // 116595
      ],
      js: [
        {
          file: "injections/js/bug1818818-fastclick-legacy-shim.js",
        },
      ],
    },
  },
  {
    id: "bug1819476",
    platform: "all",
    domain: "axisbank.com",
    bug: "1819476",
    contentScripts: {
      matches: ["*://*.axisbank.com/*"],
      js: [
        {
          file: "injections/js/bug1819476-axisbank.com-webkitSpeechRecognition-shim.js",
        },
      ],
    },
  },
  {
    id: "bug1819450",
    platform: "android",
    domain: "cmbchina.com",
    bug: "1819450",
    contentScripts: {
      matches: ["*://www.cmbchina.com/*", "*://cmbchina.com/*"],
      js: [
        {
          file: "injections/js/bug1819450-cmbchina.com-ua-change.js",
        },
      ],
    },
  },
  {
    id: "bug1827678-webc77727-js",
    platform: "android",
    domain: "free4talk.com",
    bug: "1827678",
    contentScripts: {
      matches: ["*://www.free4talk.com/*"],
      js: [
        {
          file: "injections/js/bug1819678-free4talk.com-window-chrome-shim.js",
        },
      ],
    },
  },
  {
    id: "bug1827678-webc119017",
    platform: "all",
    domain: "nppes.cms.hhs.gov",
    bug: "1827678",
    contentScripts: {
      matches: ["*://nppes.cms.hhs.gov/*"],
      css: [
        {
          file: "injections/css/bug1819678-nppes.cms.hhs.gov-unsupported-banner.css",
        },
      ],
    },
  },
  {
    id: "bug1830752",
    platform: "all",
    domain: "afisha.ru",
    bug: "1830752",
    contentScripts: {
      matches: ["*://*.afisha.ru/*"],
      css: [
        {
          file: "injections/css/bug1830752-afisha.ru-slider-pointer-events.css",
        },
      ],
    },
  },
  {
    id: "bug1830761",
    platform: "all",
    domain: "91mobiles.com",
    bug: "1830761",
    contentScripts: {
      matches: ["*://*.91mobiles.com/*"],
      css: [
        {
          file: "injections/css/bug1830761-91mobiles.com-content-height.css",
        },
      ],
    },
  },
  {
    id: "bug1830796",
    platform: "android",
    domain: "copyleaks.com",
    bug: "1830796",
    contentScripts: {
      matches: ["*://*.copyleaks.com/*"],
      css: [
        {
          file: "injections/css/bug1830796-copyleaks.com-hide-unsupported.css",
        },
      ],
      allFrames: true,
    },
  },
  {
    id: "bug1830813",
    platform: "desktop",
    domain: "onstove.com",
    bug: "1830813",
    contentScripts: {
      matches: ["*://*.onstove.com/*"],
      css: [
        {
          file: "injections/css/bug1830813-page.onstove.com-hide-unsupported.css",
        },
      ],
    },
  },
  {
    id: "bug1831007",
    platform: "all",
    domain: "All international Nintendo domains",
    bug: "1831007",
    contentScripts: {
      matches: [
        "*://*.mojenintendo.cz/*",
        "*://*.nintendo-europe.com/*",
        "*://*.nintendo.at/*",
        "*://*.nintendo.be/*",
        "*://*.nintendo.ch/*",
        "*://*.nintendo.co.il/*",
        "*://*.nintendo.co.jp/*",
        "*://*.nintendo.co.kr/*",
        "*://*.nintendo.co.nz/*",
        "*://*.nintendo.co.uk/*",
        "*://*.nintendo.co.za/*",
        "*://*.nintendo.com.au/*",
        "*://*.nintendo.com.hk/*",
        "*://*.nintendo.com/*",
        "*://*.nintendo.de/*",
        "*://*.nintendo.dk/*",
        "*://*.nintendo.es/*",
        "*://*.nintendo.fi/*",
        "*://*.nintendo.fr/*",
        "*://*.nintendo.gr/*",
        "*://*.nintendo.hu/*",
        "*://*.nintendo.it/*",
        "*://*.nintendo.nl/*",
        "*://*.nintendo.no/*",
        "*://*.nintendo.pt/*",
        "*://*.nintendo.ru/*",
        "*://*.nintendo.se/*",
        "*://*.nintendo.sk/*",
        "*://*.nintendo.tw/*",
        "*://*.nintendoswitch.com.cn/*",
      ],
      js: [
        {
          file: "injections/js/bug1831007-nintendo-window-OnetrustActiveGroups.js",
        },
      ],
    },
  },
  {
    id: "bug1836157",
    platform: "android",
    domain: "thai-masszazs.net",
    bug: "1836157",
    contentScripts: {
      matches: ["*://*.thai-masszazs.net/*"],
      js: [
        {
          file: "injections/js/bug1836157-thai-masszazs-niceScroll-disable.js",
        },
      ],
    },
  },
  {
    id: "bug1836103",
    platform: "all",
    domain: "autostar-novoross.ru",
    bug: "1836103",
    contentScripts: {
      matches: ["*://autostar-novoross.ru/*"],
      css: [
        {
          file: "injections/css/bug1836103-autostar-novoross.ru-make-map-taller.css",
        },
      ],
    },
  },
  {
    id: "bug1836105",
    platform: "all",
    domain: "cnn.com",
    bug: "1836105",
    contentScripts: {
      matches: ["*://*.cnn.com/*"],
      css: [
        {
          file: "injections/css/bug1836105-cnn.com-fix-blank-pages-when-printing.css",
        },
      ],
    },
  },
  {
    id: "bug1842437",
    platform: "desktop",
    domain: "www.youtube.com",
    bug: "1842437",
    contentScripts: {
      matches: ["*://www.youtube.com/*"],
      js: [
        {
          file: "injections/js/bug1842437-www.youtube.com-performance-now-precision.js",
        },
      ],
    },
  },
  {
    id: "bug1848713",
    platform: "all",
    domain: "cleanrider.com",
    bug: "1848713",
    contentScripts: {
      matches: ["*://*.cleanrider.com/*"],
      css: [
        {
          file: "injections/css/bug1848713-cleanrider.com-slider.css",
        },
      ],
    },
  },
  {
    id: "bug1848849",
    platform: "all",
    domain: "theaa.com",
    bug: "1848849",
    contentScripts: {
      matches: ["*://*.theaa.com/route-planner/*"],
      css: [
        {
          file: "injections/css/bug1848849-theaa.com-printing-mode-fix.css",
        },
      ],
    },
  },
  {
    id: "bug1849058",
    platform: "all",
    domain: "nicochannel.jp",
    bug: "1849058",
    contentScripts: {
      matches: [
        "*://ado-dokidokihimitsukichi-daigakuimo.com/*",
        "*://canan8181.com/*",
        "*://gs-ch.com/*", // 124511
        "*://keisuke-ueda.jp/*",
        "*://kemomimirefle.net/*",
        "*://nicochannel.jp/*", // 124463
        "*://p-jinriki-fc.com/*",
        "*://pizzaradio.jp/*",
        "*://rnqq.jp/*",
        "*://ryogomatsumaru.com/*",
        "*://takahashifumiya.com/*",
        "*://yamingfc.net/*",
      ],
      js: [
        {
          file: "injections/js/bug1849058-nicochannel.jp-picture-in-picture-shim.js",
        },
      ],
    },
  },
  {
    id: "bug1855014",
    platform: "android",
    domain: "eksiseyler.com",
    bug: "1855014",
    contentScripts: {
      matches: ["*://eksiseyler.com/*"],
      js: [
        {
          file: "injections/js/bug1855014-eksiseyler.com.js",
        },
      ],
    },
  },
  {
    id: "bug1868345",
    platform: "all",
    domain: "tvmovie.de",
    bug: "1868345",
    contentScripts: {
      matches: [
        "*://www.tvmovie.de/tv/fernsehprogramm",
        "*://www.tvmovie.de/tv/fernsehprogramm*",
        "*://www.goodcarbadcar.net/*",
      ],
      css: [
        {
          file: "injections/css/bug1868345-tvmovie.de-scroll-fix.css",
        },
      ],
    },
  },
  {
    id: "bug1897120",
    platform: "desktop",
    domain: "turn.js breakage",
    bug: "1897120",
    contentScripts: {
      matches: ["*://flipbook.se.com/*", "*://*.flipbookpdf.net/*"],
      js: [
        {
          file: "injections/js/bug1897120-turnjs-zoom-fix.js",
        },
      ],
      allFrames: true,
    },
  },
  {
    id: "bug1889326",
    platform: "desktop",
    domain: "Office 365 email handling prompt",
    bug: "1889326",
    contentScripts: {
      matches: [
        "*://*.live.com/*",
        "*://*.office.com/*",
        "*://*.office365.com/*",
        "*://*.office365.us/*",
        "*://*.outlook.cn/*",
        "*://*.outlook.com/*",
        "*://*.sharepoint.com/*",
      ],
      js: [
        {
          file: "injections/js/bug1889326-office365-email-handling-prompt-autohide.js",
        },
      ],
      allFrames: true,
    },
  },
  {
    id: "bug1881922",
    platform: "all",
    domain: "helpdeskgeek.com",
    bug: "1881922",
    contentScripts: {
      matches: ["*://helpdeskgeek.com/*"],
      js: [
        {
          file: "injections/js/bug1881922-disable-legacy-mutation-events.js",
        },
      ],
    },
  },
  {
    id: "bug1901780",
    platform: "all",
    domain: "vanbreda-health.be",
    bug: "1901780",
    contentScripts: {
      matches: ["*://www.vanbreda-health.be/*"],
      js: [
        {
          file: "injections/js/bug1881922-disable-legacy-mutation-events.js",
        },
      ],
    },
  },
  {
    id: "bug1896571",
    platform: "all",
    domain: "gracobaby.ca",
    bug: "1896571",
    contentScripts: {
      matches: ["*://www.gracobaby.ca/*"],
      css: [
        {
          file: "injections/css/bug1896571-gracobaby.ca-unlock-scrolling.css",
        },
      ],
    },
  },
  {
    id: "bug1895994",
    platform: "android",
    domain: "www.softrans.ro",
    bug: "1895994",
    contentScripts: {
      matches: ["*://*.softrans.ro/*"],
      css: [
        {
          file: "injections/css/bug1895994-softtrans.ro-unlock-scrolling.css",
        },
      ],
    },
  },
  {
    id: "bug1898952",
    platform: "desktop",
    domain: "digits.t-mobile.com",
    bug: "1898952",
    contentScripts: {
      matches: ["*://digits.t-mobile.com/*"],
      js: [
        {
          file: "injections/js/bug1898952-digits.t-mobile.com.js",
        },
      ],
    },
  },
  {
    id: "bug1815733",
    platform: "desktop",
    domain: "Office 365 Outlook locations",
    bug: "1815733",
    contentScripts: {
      matches: [
        "*://outlook.live.com/*",
        "*://outlook.office.com/*",
        "*://outlook.office365.com/*",
        "*://outlook.office365.us/*",
        "*://*.outlook.cn/*",
        "*://*.outlook.com/*",
      ],
      js: [
        {
          file: "injections/js/bug1815733-outlook365-clipboard-read-noop.js",
        },
      ],
      allFrames: true,
    },
  },
  {
    id: "bug1899937",
    platform: "all",
    domain: "plus.nhk.jp",
    bug: "1899937",
    contentScripts: {
      matches: ["*://plus.nhk.jp/*"],
      css: [
        {
          file: "injections/css/bug1899937-plus.nhk.jp-hide-unsupported.css",
        },
      ],
      js: [
        {
          file: "injections/js/bug1899937-plus.nhk.jp-request-picture-in-picture.js",
        },
      ],
    },
  },
  {
    id: "bug1886616",
    platform: "all",
    domain: "www.six-group.com",
    bug: "1886616",
    contentScripts: {
      matches: ["*://www.six-group.com/*/market-data/etf/etf-explorer.html*"],
      css: [
        {
          file: "injections/css/bug1886616-www.six-group.com-select-fix.css",
        },
      ],
    },
  },
  {
    id: "bug1896349",
    platform: "all",
    domain: "vivaldi.com",
    bug: "1896349",
    contentScripts: {
      matches: ["*://vivaldi.com/*"],
      css: [
        {
          file: "injections/css/bug1896349-vivaldi.com-selected-text-fix.css",
        },
      ],
    },
  },
  {
    id: "bug1836872",
    platform: "desktop",
    domain: "docs.google.com",
    bug: "1836872",
    contentScripts: {
      matches: ["*://docs.google.com/document/*"],
      css: [
        {
          file: "injections/css/bug1836872-docs.google.com-font-submenus-inaccessible.css",
        },
      ],
    },
  },
  {
    id: "bug1779908",
    platform: "desktop",
    domain: "play.google.com",
    bug: "1779908",
    contentScripts: {
      matches: ["*://play.google.com/store/*"],
      css: [
        {
          file: "injections/css/bug1779908-play.google.com-scrollbar-fix.css",
        },
      ],
    },
  },
  {
    id: "bug1879879",
    platform: "all",
    domain: "developers.pinterest.com",
    bug: "1879879",
    contentScripts: {
      matches: ["*://developers.pinterest.com/docs/*"],
      css: [
        {
          file: "injections/css/bug1879879-developers.pinterest.com-list-alignment-fix.css",
        },
      ],
    },
  },
  {
    id: "bug1856915",
    platform: "android",
    domain: "login.yahoo.com",
    bug: "1856915",
    contentScripts: {
      matches: ["*://login.yahoo.com/account/*"],
      css: [
        {
          file: "injections/css/bug1856915-login.yahoo.com-unhide-password-button-fix.css",
        },
      ],
    },
  },
  {
    id: "bug1841730",
    platform: "desktop",
    domain: "www.korg.com",
    bug: "1841730",
    contentScripts: {
      matches: ["*://www.korg.com/*/support/download/product/*"],
      js: [
        {
          file: "injections/js/bug1841730-www.korg.com-fix-broken-page-loads.js",
        },
      ],
    },
  },
  {
    id: "bug1895051",
    platform: "all",
    domain: "www.zhihu.com",
    bug: "1895051",
    contentScripts: {
      matches: ["*://www.zhihu.com/question/*"],
      css: [
        {
          file: "injections/css/bug1895051-www.zhihu.com-broken-button-fix.css",
        },
      ],
    },
  },
  {
    id: "bug1924500",
    platform: "desktop",
    domain: "www.tiktok.com",
    bug: "1924500",
    contentScripts: {
      matches: ["*://www.tiktok.com/*"],
      js: [
        {
          file: "injections/js/bug1924500-www.tiktok.com-fix-captcha-slider.js",
        },
      ],
    },
  },
  {
    id: "bug1901000",
    platform: "desktop",
    domain: "eyebuydirect.ca",
    bug: "1901000",
    contentScripts: {
      matches: ["*://*.eyebuydirect.ca/*"],
      css: [
        {
          file: "injections/css/bug1901000-eyebuydirect.ca-fix-paypal-button.css",
        },
      ],
    },
  },
  {
    id: "bug1889505",
    platform: "android",
    domain: "bankmandiri.co.id",
    bug: "1889505",
    contentScripts: {
      matches: ["*://*.bankmandiri.co.id/*"],
      js: [
        {
          file: "injections/js/bug1889505-bankmandiri.co.id-window.chrome.js",
        },
      ],
    },
  },
  {
    id: "bug1925508",
    platform: "android",
    domain: "developer.apple.com",
    bug: "1925508",
    contentScripts: {
      matches: ["*://developer.apple.com/*"],
      css: [
        {
          file: "injections/css/bug1925508-developer-apple.com-transform-scale.css",
        },
      ],
    },
  },
  {
    id: "bug1928216",
    platform: "desktop",
    domain: "voice.google.com",
    bug: "1928216",
    contentScripts: {
      matches: ["*://voice.google.com/*"],
      js: [
        {
          file: "injections/js/bug1928216-voice.google.com-permissions.query.js",
        },
      ],
    },
  },
  {
    id: "bug1873166",
    platform: "android",
    domain: "nsandi.com",
    bug: "1873166",
    contentScripts: {
      matches: ["*://*.nsandi.com/*"],
      css: [
        {
          file: "injections/css/bug1873166-nsandi.com-hide-unsupported-message.css",
        },
      ],
    },
  },
  {
    id: "1875540",
    platform: "all",
    domain: "allstate.com",
    bug: "1875540",
    contentScripts: {
      matches: ["*://*.allstate.com/*"],
      css: [
        {
          file: "injections/css/bug1875540-allstate.com-hide-unsupported-message.css",
        },
      ],
    },
  },
  {
    id: "1886566",
    platform: "all",
    domain: "quezoncity.gov.ph",
    bug: "1886566",
    contentScripts: {
      matches: ["*://qceservices.quezoncity.gov.ph/qcvaxeasy*"],
      css: [
        {
          file: "injections/css/bug1886566-quezoncity.gov.ph-iframe-height.css",
        },
      ],
    },
  },
  {
    id: "1846742",
    platform: "desktop",
    domain: "microsoft.com",
    bug: "1846742",
    contentScripts: {
      matches: ["*://www.microsoft.com/*"],
      js: [
        {
          file: "injections/js/bug1846742-microsoft.com-search-key-fix.js",
        },
      ],
    },
  },
  {
    id: "1886591",
    platform: "all",
    domain: "la-vache-noire.com",
    bug: "1886591",
    contentScripts: {
      matches: ["*://la-vache-noire.com/*"],
      css: [
        {
          file: "injections/css/bug1886591-la-vache-noire.com-cookie-banner-fix.css",
        },
      ],
    },
  },
  {
    id: "1923286",
    platform: "desktop",
    domain: "bing.com",
    bug: "1923286",
    contentScripts: {
      matches: ["*://www.bing.com/images/search*"],
      js: [
        {
          file: "injections/js/bug1923286-bing.com-image-click-fix.js",
        },
      ],
    },
  },
  {
    id: "1930440",
    platform: "all",
    domain: "online.singaporepools.com",
    bug: "1930440",
    contentScripts: {
      matches: ["*://online.singaporepools.com/*"],
      js: [
        {
          file: "injections/js/bug1930440-online.singaporepools.com-prevent-unsupported-alert.js",
        },
      ],
    },
  },
  {
    id: "1934567",
    platform: "android",
    domain: "www.port8.fi",
    bug: "1934567",
    contentScripts: {
      matches: ["*://www.port8.fi/bokning/*"],
      css: [
        {
          file: "injections/css/bug1934567-www.port8.fi-scrolling-fix.css",
        },
      ],
    },
  },
];

module.exports = AVAILABLE_INJECTIONS;
