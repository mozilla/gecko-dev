/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * ESLint's flat configuration doesn't automatically read sub-configuration files.
 * Hence, we import them here, adjusting paths as we go.
 *
 * Over time we would like to reduce this list. Ideally, this file would not exist.
 * The aim is for our rules to be consistently applied across the code base.
 *
 * If you are seeking to add a new sub-file here, please talk to the "JavaScript
 * usage, tools, and style" team first (aka #frontend-codestyle-reviewers).
 */

import config0 from "./accessible/tests/browser/.eslintrc.mjs";
import config1 from "./accessible/tests/mochitest/.eslintrc.mjs";
import config2 from "./browser/.eslintrc.mjs";
import config3 from "./browser/base/content/test/webextensions/.eslintrc.mjs";
import config4 from "./browser/components/.eslintrc.mjs";
import config5 from "./browser/components/aboutlogins/tests/chrome/.eslintrc.mjs";
import config6 from "./browser/components/aboutwelcome/.eslintrc.mjs";
import config7 from "./browser/components/asrouter/.eslintrc.mjs";
import config8 from "./browser/components/customizableui/.eslintrc.mjs";
import config9 from "./browser/components/customizableui/content/.eslintrc.mjs";
import config10 from "./browser/components/enterprisepolicies/tests/xpcshell/.eslintrc.mjs";
import config11 from "./browser/components/extensions/.eslintrc.mjs";
import config12 from "./browser/components/extensions/child/.eslintrc.mjs";
import config13 from "./browser/components/extensions/parent/.eslintrc.mjs";
import config14 from "./browser/components/extensions/test/browser/.eslintrc.mjs";
import config15 from "./browser/components/extensions/test/mochitest/.eslintrc.mjs";
import config16 from "./browser/components/extensions/test/xpcshell/.eslintrc.mjs";
import config17 from "./browser/components/migration/.eslintrc.mjs";
import config18 from "./browser/components/pagedata/.eslintrc.mjs";
import config20 from "./browser/components/resistfingerprinting/test/mochitest/.eslintrc.mjs";
import config21 from "./browser/components/search/.eslintrc.mjs";
import config22 from "./browser/components/urlbar/.eslintrc.mjs";
import config23 from "./browser/extensions/newtab/.eslintrc.mjs";
import config24 from "./browser/extensions/pictureinpicture/tests/browser/.eslintrc.mjs";
import config25 from "./browser/extensions/search-detection/tests/browser/.eslintrc.mjs";
import config26 from "./devtools/.eslintrc.mjs";
import config28 from "./devtools/client/.eslintrc.mjs";
import config34 from "./devtools/client/debugger/src/.eslintrc.mjs";
import config37 from "./devtools/client/dom/.eslintrc.mjs";
import config39 from "./devtools/client/framework/test/reload/.eslintrc.mjs";
import config45 from "./devtools/client/jsonview/.eslintrc.mjs";
import config46 from "./devtools/client/memory/.eslintrc.mjs";
import config48 from "./devtools/client/netmonitor/test/.eslintrc.mjs";
import config50 from "./devtools/client/performance-new/.eslintrc.mjs";
import config53 from "./devtools/client/shared/components/.eslintrc.mjs";
import config63 from "./devtools/server/tests/xpcshell/.eslintrc.mjs";
import config64 from "./devtools/shared/.eslintrc.mjs";
import config74 from "./dom/base/test/jsmodules/.eslintrc.mjs";
import config75 from "./dom/fs/test/common/.eslintrc.mjs";
import config76 from "./dom/fs/test/mochitest/worker/.eslintrc.mjs";
import config77 from "./dom/fs/test/xpcshell/worker/.eslintrc.mjs";
import config78 from "./dom/media/mediasource/test/.eslintrc.mjs";
import config79 from "./dom/quota/test/modules/system/worker/.eslintrc.mjs";
import config80 from "./js/src/builtin/.eslintrc.mjs";
import config81 from "./mobile/android/.eslintrc.mjs";
import config82 from "./mobile/android/android-components/components/feature/webcompat-reporter/src/main/assets/extensions/webcompat-reporter/.eslintrc.mjs";
import config83 from "./mobile/android/examples/messaging_example/app/src/main/assets/messaging/.eslintrc.mjs";
import config84 from "./mobile/android/examples/port_messaging_example/app/src/main/assets/messaging/.eslintrc.mjs";
import config85 from "./mobile/android/fenix/app/src/androidTest/java/org/mozilla/fenix/syncintegration/.eslintrc.mjs";
import config86 from "./mobile/android/geckoview/src/androidTest/assets/web_extensions/.eslintrc.mjs";
import config87 from "./mobile/shared/.eslintrc.mjs";
import config88 from "./mobile/shared/components/extensions/.eslintrc.mjs";
import config89 from "./mobile/shared/components/extensions/test/mochitest/.eslintrc.mjs";
import config90 from "./mobile/shared/components/extensions/test/xpcshell/.eslintrc.mjs";
import config91 from "./netwerk/test/perf/.eslintrc.mjs";
import config92 from "./remote/marionette/.eslintrc.mjs";
import config93 from "./remote/marionette/test/xpcshell/.eslintrc.mjs";
import config94 from "./security/.eslintrc.mjs";
import config95 from "./security/manager/ssl/tests/.eslintrc.mjs";
import config96 from "./security/manager/tools/.eslintrc.mjs";
import config97 from "./services/sync/tests/tps/.eslintrc.mjs";
import config98 from "./taskcluster/docker/index-task/.eslintrc.mjs";
import config99 from "./taskcluster/docker/periodic-updates/.eslintrc.mjs";
import config100 from "./testing/talos/talos/tests/perf-reftest-singletons/.eslintrc.mjs";
import config101 from "./testing/mozbase/mozprofile/tests/files/dummy-profile/.eslintrc.mjs";
import config102 from "./testing/performance/.eslintrc.mjs";
import config103 from "./testing/raptor/.eslintrc.mjs";
import config104 from "./testing/talos/.eslintrc.mjs";
import config105 from "./testing/talos/talos/tests/devtools/addon/content/.eslintrc.mjs";
import config106 from "./testing/talos/talos/tests/perf-reftest/.eslintrc.mjs";
import config107 from "./toolkit/.eslintrc.mjs";
import config108 from "./toolkit/components/antitracking/test/browser/.eslintrc.mjs";
import config109 from "./toolkit/components/extensions/.eslintrc.mjs";
import config110 from "./toolkit/components/extensions/child/.eslintrc.mjs";
import config111 from "./toolkit/components/extensions/parent/.eslintrc.mjs";
import config112 from "./toolkit/components/extensions/test/browser/.eslintrc.mjs";
import config113 from "./toolkit/components/extensions/test/mochitest/.eslintrc.mjs";
import config114 from "./toolkit/components/extensions/test/xpcshell/.eslintrc.mjs";
import config115 from "./toolkit/components/extensions/test/xpcshell/webidl-api/.eslintrc.mjs";
import config116 from "./toolkit/components/narrate/.eslintrc.mjs";
import config117 from "./toolkit/components/normandy/test/.eslintrc.mjs";
import config118 from "./toolkit/components/passwordmgr/test/browser/.eslintrc.mjs";
import config119 from "./toolkit/components/passwordmgr/test/mochitest/.eslintrc.mjs";
import config120 from "./toolkit/components/prompts/test/.eslintrc.mjs";
import config121 from "./toolkit/components/reader/.eslintrc.mjs";
import config122 from "./toolkit/content/.eslintrc.mjs";
import config123 from "./toolkit/modules/subprocess/.eslintrc.mjs";
import config124 from "./toolkit/mozapps/extensions/.eslintrc.mjs";
import config125 from "./toolkit/mozapps/extensions/test/browser/.eslintrc.mjs";
import config126 from "./toolkit/mozapps/extensions/test/xpcshell/.eslintrc.mjs";
import config127 from "./tools/lint/eslint/.eslintrc.mjs";
import config130 from "./tools/tryselect/selectors/chooser/.eslintrc.mjs";
import config131 from "./tools/ts/.eslintrc.mjs";

function convertConfigurationFile(path, config) {
  let sectionId = 0;
  let newConfig = [];
  for (let section of config) {
    let newSection = { ...section };
    newSection.name = path + ".eslintrc.js-" + sectionId++;

    if (!newSection.files) {
      newSection.files = [path];
    } else if (Array.isArray(newSection.files)) {
      newSection.files = newSection.files.map(f => path + f);
    } else if (typeof newSection.files == "string") {
      newSection.files = [path + newSection.files];
    } else {
      throw new Error(
        "Unexpected type for the files property in configuration for",
        path
      );
    }
    newConfig.push(newSection);
  }
  return newConfig;
}

export default [
  ...convertConfigurationFile("accessible/tests/browser/", config0),
  ...convertConfigurationFile("accessible/tests/mochitest/", config1),
  ...convertConfigurationFile("browser/", config2),
  ...convertConfigurationFile(
    "browser/base/content/test/webextensions/",
    config3
  ),
  ...convertConfigurationFile("browser/components/", config4),
  ...convertConfigurationFile(
    "browser/components/aboutlogins/tests/chrome/",
    config5
  ),
  ...convertConfigurationFile("browser/components/aboutwelcome/", config6),
  ...convertConfigurationFile("browser/components/asrouter/", config7),
  ...convertConfigurationFile("browser/components/customizableui/", config8),
  ...convertConfigurationFile(
    "browser/components/customizableui/content/",
    config9
  ),
  ...convertConfigurationFile(
    "browser/components/enterprisepolicies/tests/xpcshell/",
    config10
  ),
  ...convertConfigurationFile("browser/components/extensions/", config11),
  ...convertConfigurationFile("browser/components/extensions/child/", config12),
  ...convertConfigurationFile(
    "browser/components/extensions/parent/",
    config13
  ),
  ...convertConfigurationFile(
    "browser/components/extensions/test/browser/",
    config14
  ),
  ...convertConfigurationFile(
    "browser/components/extensions/test/mochitest/",
    config15
  ),
  ...convertConfigurationFile(
    "browser/components/extensions/test/xpcshell/",
    config16
  ),
  ...convertConfigurationFile("browser/components/migration/", config17),
  ...convertConfigurationFile("browser/components/pagedata/", config18),
  ...convertConfigurationFile(
    "browser/components/resistfingerprinting/test/mochitest/",
    config20
  ),
  ...convertConfigurationFile("browser/components/search/", config21),
  ...convertConfigurationFile("browser/components/urlbar/", config22),
  ...convertConfigurationFile("browser/extensions/newtab/", config23),
  ...convertConfigurationFile(
    "browser/extensions/pictureinpicture/tests/browser/",
    config24
  ),
  ...convertConfigurationFile(
    "browser/extensions/search-detection/tests/browser/",
    config25
  ),
  ...convertConfigurationFile("devtools/", config26),
  ...convertConfigurationFile("devtools/client/", config28),
  ...convertConfigurationFile("devtools/client/debugger/src/", config34),
  ...convertConfigurationFile("devtools/client/dom/", config37),
  ...convertConfigurationFile(
    "devtools/client/framework/test/reload/",
    config39
  ),
  ...convertConfigurationFile("devtools/client/jsonview/", config45),
  ...convertConfigurationFile("devtools/client/memory/", config46),
  ...convertConfigurationFile("devtools/client/netmonitor/test/", config48),
  ...convertConfigurationFile("devtools/client/performance-new/", config50),
  ...convertConfigurationFile("devtools/client/shared/components/", config53),
  ...convertConfigurationFile("devtools/server/tests/xpcshell/", config63),
  ...convertConfigurationFile("devtools/shared/", config64),
  ...convertConfigurationFile("dom/base/test/jsmodules/", config74),
  ...convertConfigurationFile("dom/fs/test/common/", config75),
  ...convertConfigurationFile("dom/fs/test/mochitest/worker/", config76),
  ...convertConfigurationFile("dom/fs/test/xpcshell/worker/", config77),
  ...convertConfigurationFile("dom/media/mediasource/test/", config78),
  ...convertConfigurationFile(
    "dom/quota/test/modules/system/worker/",
    config79
  ),
  ...convertConfigurationFile("js/src/builtin/", config80),
  ...convertConfigurationFile("mobile/android/", config81),
  ...convertConfigurationFile(
    "mobile/android/android-components/components/feature/webcompat-reporter/src/main/assets/extensions/webcompat-reporter/",
    config82
  ),
  ...convertConfigurationFile(
    "mobile/android/examples/messaging_example/app/src/main/assets/messaging/",
    config83
  ),
  ...convertConfigurationFile(
    "mobile/android/examples/port_messaging_example/app/src/main/assets/messaging/",
    config84
  ),
  ...convertConfigurationFile(
    "mobile/android/fenix/app/src/androidTest/java/org/mozilla/fenix/syncintegration/",
    config85
  ),
  ...convertConfigurationFile(
    "mobile/android/geckoview/src/androidTest/assets/web_extensions/",
    config86
  ),
  ...convertConfigurationFile("mobile/shared/", config87),
  ...convertConfigurationFile("mobile/shared/components/extensions/", config88),
  ...convertConfigurationFile(
    "mobile/shared/components/extensions/test/mochitest/",
    config89
  ),
  ...convertConfigurationFile(
    "mobile/shared/components/extensions/test/xpcshell/",
    config90
  ),
  ...convertConfigurationFile("netwerk/test/perf/", config91),
  ...convertConfigurationFile("remote/marionette/", config92),
  ...convertConfigurationFile("remote/marionette/test/xpcshell/", config93),
  ...convertConfigurationFile("security/", config94),
  ...convertConfigurationFile("security/manager/ssl/tests/", config95),
  ...convertConfigurationFile("security/manager/tools/", config96),
  ...convertConfigurationFile("services/sync/tests/tps/", config97),
  ...convertConfigurationFile("taskcluster/docker/index-task/", config98),
  ...convertConfigurationFile("taskcluster/docker/periodic-updates/", config99),
  ...convertConfigurationFile(
    "testing/talos/talos/tests/perf-reftest-singletons/",
    config100
  ),
  ...convertConfigurationFile(
    "testing/mozbase/mozprofile/tests/files/dummy-profile/",
    config101
  ),
  ...convertConfigurationFile("testing/performance/", config102),
  ...convertConfigurationFile("testing/raptor/", config103),
  ...convertConfigurationFile("testing/talos/", config104),
  ...convertConfigurationFile(
    "testing/talos/talos/tests/devtools/addon/content/",
    config105
  ),
  ...convertConfigurationFile(
    "testing/talos/talos/tests/perf-reftest/",
    config106
  ),
  ...convertConfigurationFile("toolkit/", config107),
  ...convertConfigurationFile(
    "toolkit/components/antitracking/test/browser/",
    config108
  ),
  ...convertConfigurationFile("toolkit/components/extensions/", config109),
  ...convertConfigurationFile(
    "toolkit/components/extensions/child/",
    config110
  ),
  ...convertConfigurationFile(
    "toolkit/components/extensions/parent/",
    config111
  ),
  ...convertConfigurationFile(
    "toolkit/components/extensions/test/browser/",
    config112
  ),
  ...convertConfigurationFile(
    "toolkit/components/extensions/test/mochitest/",
    config113
  ),
  ...convertConfigurationFile(
    "toolkit/components/extensions/test/xpcshell/",
    config114
  ),
  ...convertConfigurationFile(
    "toolkit/components/extensions/test/xpcshell/webidl-api/",
    config115
  ),
  ...convertConfigurationFile("toolkit/components/narrate/", config116),
  ...convertConfigurationFile("toolkit/components/normandy/test/", config117),
  ...convertConfigurationFile(
    "toolkit/components/passwordmgr/test/browser/",
    config118
  ),
  ...convertConfigurationFile(
    "toolkit/components/passwordmgr/test/mochitest/",
    config119
  ),
  ...convertConfigurationFile("toolkit/components/prompts/test/", config120),
  ...convertConfigurationFile("toolkit/components/reader/", config121),
  ...convertConfigurationFile("toolkit/content/", config122),
  ...convertConfigurationFile("toolkit/modules/subprocess/", config123),
  ...convertConfigurationFile("toolkit/mozapps/extensions/", config124),
  ...convertConfigurationFile(
    "toolkit/mozapps/extensions/test/browser/",
    config125
  ),
  ...convertConfigurationFile(
    "toolkit/mozapps/extensions/test/xpcshell/",
    config126
  ),
  ...convertConfigurationFile("tools/lint/eslint/", config127),
  ...convertConfigurationFile("tools/tryselect/selectors/chooser/", config130),
  ...convertConfigurationFile("tools/ts/", config131),
];
