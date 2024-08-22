/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { OriginControls } = ChromeUtils.importESModule(
  "resource://gre/modules/ExtensionPermissions.sys.mjs"
);

const FAKE_TABS = {
  EXAMPLE_COM: { url: "http://example.com" },
};
const ORIGIN_CONTROLS_STATES = {
  NO_ACCESS: { noAccess: true },
  ALL_DOMAINS: { allDomains: true },
  HAS_ACCESS: { hasAccess: true },
  SHOW_OPTIONS_WHEN_CLICKED_AND_ALWAYS_ON: {
    whenClicked: true,
    alwaysOn: true,
  },
};

const ALL_URLS_CASES = [
  ["<all_urls>"],
  ["*://*/*"],
  ["http://*/*", "https://*/*"],
];

const runOriginControlTests = async ({
  tabInfo: { url, hasActiveTabPermission = false },
  manifest,
  initialAllowedOrigins,
  initialOriginControlsState,
  originControlsTests,
  expectPrivileged = false,
  expectRestrictSchemes = true,
}) => {
  const testExtension = ExtensionTestUtils.loadExtension({
    manifest: {
      name: `Test Extension ${JSON.stringify(manifest)}`,
      manifest_version: 3,
      ...manifest,
    },
  });
  await testExtension.startup();
  const testURI = Services.io.newURI(url);
  const policy = WebExtensionPolicy.getByID(testExtension.id);
  const expectedMethodNames = new Set([
    "getState",
    "setAlwaysOn",
    "setWhenClicked",
  ]);
  const assertAllowedOrigins = (expectedAllowedOrigins, msg) => {
    Assert.deepEqual(
      policy.allowedOrigins.patterns
        .filter(
          // Exclude the moz-extension host permission implicitly added internally.
          mp => !mp.pattern.startsWith("moz-extension://")
        )
        .map(mp => mp.pattern),
      expectedAllowedOrigins,
      msg
    );
  };
  const assertOriginControlState = (
    expectedOriginControlsState,
    tabHasActiveTabPermission,
    msg
  ) => {
    Assert.deepEqual(
      OriginControls._getStateInternal(policy, {
        uri: testURI,
        tabHasActiveTabPermission,
      }),
      typeof expectedOriginControlsState === "function"
        ? expectedOriginControlsState(initialOriginControlsState)
        : expectedOriginControlsState,
      msg
    );
  };
  Assert.equal(
    policy.isPrivileged,
    expectPrivileged,
    `Expect isPrivileged is ${expectPrivileged} for ${policy.name}`
  );
  Assert.equal(
    policy.extension.restrictSchemes,
    expectRestrictSchemes,
    `Expect restrictSchemes is ${expectRestrictSchemes} for ${policy.name}`
  );
  assertAllowedOrigins(
    initialAllowedOrigins,
    `Got the expected granted initial host permissions for "${policy.name}"`
  );
  assertOriginControlState(
    initialOriginControlsState,
    hasActiveTabPermission,
    `Got the expected initial OriginControls state for ${policy.name} on ${url}`
  );
  for (const testCase of originControlsTests) {
    const {
      methodCall,
      expectedAllowedOrigins,
      expectedOriginControlsState = initialOriginControlsState,
    } = testCase;
    if (!expectedMethodNames.has(methodCall)) {
      ok(false, `Got unxpected methodCall: ${methodCall}`);
      continue;
    }
    await OriginControls[methodCall](policy, testURI);
    assertAllowedOrigins(
      expectedAllowedOrigins,
      `Got the expected granted host permissions for "${policy.name}" after ${methodCall} on url ${url}`
    );
    assertOriginControlState(
      expectedOriginControlsState,
      hasActiveTabPermission,
      `Got the expected OriginControls state for ${policy.name} after ${methodCall} on url ${url}`
    );
  }
  await testExtension.unload();
};

add_task(async function test_granted_all_urls() {
  // For extensions with granted all urls host permissions, setWhenClicked
  // and setAlwaysOn are expected to be no-op.
  for (const granted_host_permissions of ALL_URLS_CASES) {
    await runOriginControlTests({
      tabInfo: FAKE_TABS.EXAMPLE_COM,
      manifest: { host_permissions: granted_host_permissions },
      initialAllowedOrigins: granted_host_permissions,
      initialOriginControlsState: {
        ...ORIGIN_CONTROLS_STATES.HAS_ACCESS,
        ...ORIGIN_CONTROLS_STATES.ALL_DOMAINS,
      },
      originControlsTests: [
        {
          methodCall: "setWhenClicked",
          expectedAllowedOrigins: granted_host_permissions,
        },
        {
          methodCall: "setAlwaysOn",
          expectedAllowedOrigins: granted_host_permissions,
        },
      ],
    });
  }
});

add_task(async function test_optional_all_urls() {
  // Not granted single all urls host permission.
  for (const allUrls_permission of ["<all_urls>", "*://*/*"]) {
    await runOriginControlTests({
      tabInfo: FAKE_TABS.EXAMPLE_COM,
      manifest: { optional_host_permissions: [allUrls_permission] },
      initialAllowedOrigins: [],
      initialOriginControlsState: {
        ...ORIGIN_CONTROLS_STATES.SHOW_OPTIONS_WHEN_CLICKED_AND_ALWAYS_ON,
        hasAccess: false,
        temporaryAccess: false,
      },
      originControlsTests: [
        {
          methodCall: "setAlwaysOn",
          expectedAllowedOrigins: ["*://example.com/*"],
          expectedOriginControlsState(initialState) {
            return { ...initialState, hasAccess: true };
          },
        },
        {
          methodCall: "setWhenClicked",
          expectedAllowedOrigins: [],
        },
      ],
    });
  }
});

add_task(async function test_optional_all_urls_separate_web_schemes() {
  // Two not granted separate schema-specific wildcard domain host permission.
  await runOriginControlTests({
    tabInfo: FAKE_TABS.EXAMPLE_COM,
    manifest: { optional_host_permissions: ["http://*/*", "https://*/*"] },
    initialAllowedOrigins: [],
    initialOriginControlsState: {
      ...ORIGIN_CONTROLS_STATES.SHOW_OPTIONS_WHEN_CLICKED_AND_ALWAYS_ON,
      hasAccess: false,
      temporaryAccess: false,
    },
    originControlsTests: [
      {
        methodCall: "setAlwaysOn",
        expectedAllowedOrigins: [
          "http://example.com/*",
          "https://example.com/*",
        ],
        expectedOriginControlsState(initialState) {
          return { ...initialState, hasAccess: true };
        },
      },
      {
        methodCall: "setWhenClicked",
        expectedAllowedOrigins: [],
      },
    ],
  });
});

add_task(async function test_wildcard_host_one_web_scheme_match() {
  // Single not granted schema-specific wildcard domain host permission
  // with schema matching the current url schema.
  await runOriginControlTests({
    tabInfo: FAKE_TABS.EXAMPLE_COM,
    manifest: { optional_host_permissions: ["http://*/*"] },
    initialAllowedOrigins: [],
    initialOriginControlsState: {
      ...ORIGIN_CONTROLS_STATES.SHOW_OPTIONS_WHEN_CLICKED_AND_ALWAYS_ON,
      hasAccess: false,
      temporaryAccess: false,
    },
    originControlsTests: [
      {
        methodCall: "setAlwaysOn",
        expectedAllowedOrigins: ["http://example.com/*"],
        expectedOriginControlsState(initialState) {
          return { ...initialState, hasAccess: true };
        },
      },
      {
        methodCall: "setWhenClicked",
        expectedAllowedOrigins: [],
      },
    ],
  });
});

add_task(async function test_wildcard_host_one_web_scheme_nomatch() {
  // Single not granted schema-specific wildcard domain host permission
  // with schema NOT matching the current url schema.
  await runOriginControlTests({
    tabInfo: FAKE_TABS.EXAMPLE_COM,
    manifest: { optional_host_permissions: ["https://*/*"] },
    initialAllowedOrigins: [],
    initialOriginControlsState: {
      ...ORIGIN_CONTROLS_STATES.NO_ACCESS,
      quarantined: false,
    },
    originControlsTests: [
      {
        methodCall: "setAlwaysOn",
        expectedAllowedOrigins: [],
      },
      {
        methodCall: "setWhenClicked",
        expectedAllowedOrigins: [],
      },
    ],
  });
});

add_task(async function test_host_specific_wildcard_domain_match() {
  // Single granted wildcard-scheme domain-specific host permission.
  await runOriginControlTests({
    tabInfo: FAKE_TABS.EXAMPLE_COM,
    manifest: { host_permissions: ["*://example.com/*"] },
    initialAllowedOrigins: ["*://example.com/*"],
    initialOriginControlsState: {
      ...ORIGIN_CONTROLS_STATES.SHOW_OPTIONS_WHEN_CLICKED_AND_ALWAYS_ON,
      hasAccess: true,
      temporaryAccess: false,
    },
    originControlsTests: [
      {
        methodCall: "setWhenClicked",
        expectedAllowedOrigins: [],
        expectedOriginControlsState(initialState) {
          return { ...initialState, hasAccess: false };
        },
      },
      {
        methodCall: "setAlwaysOn",
        expectedAllowedOrigins: ["*://example.com/*"],
      },
    ],
  });
});

add_task(async function test_host_and_schema_specific_match() {
  // Single granted scheme-specific domain-specific host permission
  // matching current url scheme.
  await runOriginControlTests({
    tabInfo: FAKE_TABS.EXAMPLE_COM,
    manifest: { host_permissions: ["http://example.com/*"] },
    initialAllowedOrigins: ["http://example.com/*"],
    initialOriginControlsState: {
      ...ORIGIN_CONTROLS_STATES.SHOW_OPTIONS_WHEN_CLICKED_AND_ALWAYS_ON,
      hasAccess: true,
      temporaryAccess: false,
    },
    originControlsTests: [
      {
        methodCall: "setWhenClicked",
        expectedAllowedOrigins: [],
        expectedOriginControlsState(initialState) {
          return { ...initialState, hasAccess: false };
        },
      },
      {
        methodCall: "setAlwaysOn",
        expectedAllowedOrigins: ["http://example.com/*"],
      },
    ],
  });
});

add_task(async function test_host_and_schema_specific_nomatch() {
  // Single granted scheme-specific domain-specific host permission
  // NOT matching current url scheme.
  // NOTE: granted host permissions stays unchanged because:
  // - setWhenClicked: the extension already do not have access to the current url
  // - setAlwaysOn: the extension does not have any optional host permissions
  //   that could subsume an host permission pattern that matches the current
  //   url scheme
  await runOriginControlTests({
    tabInfo: FAKE_TABS.EXAMPLE_COM,
    manifest: { host_permissions: ["https://example.com/*"] },
    initialAllowedOrigins: ["https://example.com/*"],
    initialOriginControlsState: {
      ...ORIGIN_CONTROLS_STATES.NO_ACCESS,
      quarantined: false,
    },
    originControlsTests: [
      {
        methodCall: "setWhenClicked",
        expectedAllowedOrigins: ["https://example.com/*"],
      },
      {
        methodCall: "setAlwaysOn",
        expectedAllowedOrigins: ["https://example.com/*"],
      },
    ],
  });
});

add_task(async function test_host_match_separate_schema_specific_perms() {
  // Two granted scheme-specific domain-specific host permissions.
  await runOriginControlTests({
    tabInfo: FAKE_TABS.EXAMPLE_COM,
    manifest: {
      host_permissions: ["http://example.com/*", "https://example.com/*"],
    },
    initialAllowedOrigins: ["http://example.com/*", "https://example.com/*"],
    initialOriginControlsState: {
      ...ORIGIN_CONTROLS_STATES.SHOW_OPTIONS_WHEN_CLICKED_AND_ALWAYS_ON,
      hasAccess: true,
      temporaryAccess: false,
    },
    originControlsTests: [
      {
        methodCall: "setWhenClicked",
        expectedAllowedOrigins: [],
        expectedOriginControlsState(initialState) {
          return { ...initialState, hasAccess: false };
        },
      },
      {
        methodCall: "setAlwaysOn",
        expectedAllowedOrigins: [
          "http://example.com/*",
          "https://example.com/*",
        ],
      },
    ],
  });

  // Two NOT granted scheme-specific domain-specific host permissions.
  await runOriginControlTests({
    tabInfo: FAKE_TABS.EXAMPLE_COM,
    manifest: {
      optional_host_permissions: [
        "http://example.com/*",
        "https://example.com/*",
      ],
    },
    initialAllowedOrigins: [],
    initialOriginControlsState: {
      ...ORIGIN_CONTROLS_STATES.SHOW_OPTIONS_WHEN_CLICKED_AND_ALWAYS_ON,
      hasAccess: false,
      temporaryAccess: false,
    },
    originControlsTests: [
      {
        methodCall: "setAlwaysOn",
        expectedAllowedOrigins: [
          "http://example.com/*",
          "https://example.com/*",
        ],
        expectedOriginControlsState(initialState) {
          return { ...initialState, hasAccess: true };
        },
      },
      {
        methodCall: "setWhenClicked",
        expectedAllowedOrigins: [],
      },
    ],
  });
});

add_task(async function test_host_match_schema_nomatch() {
  // Scheme-specific domain-specific host permission
  // NOT matching current url scheme.
  // NOTE: granted host permissions stays unchanged because:
  // - setAlwaysOn: the optional host permissions available wouldn't still
  //   grant access to the current url.
  // - setWhenClicked: the extension already do not have access to the current url
  await runOriginControlTests({
    tabInfo: FAKE_TABS.EXAMPLE_COM,
    manifest: {
      optional_host_permissions: [
        "https://example.com/*",
        "https://*.example.com/*",
      ],
    },
    initialAllowedOrigins: [],
    initialOriginControlsState: {
      ...ORIGIN_CONTROLS_STATES.NO_ACCESS,
      quarantined: false,
    },
    originControlsTests: [
      {
        methodCall: "setAlwaysOn",
        expectedAllowedOrigins: [],
      },
      {
        methodCall: "setWhenClicked",
        expectedAllowedOrigins: [],
      },
    ],
  });
});

add_task(async function test_wildcard_scheme_and_subdomain() {
  // Single granted "wildcard scheme + wildcard subdomain" permission.
  await runOriginControlTests({
    tabInfo: { url: "http://www.example.com" },
    manifest: {
      host_permissions: ["*://*.example.com/*"],
    },
    initialAllowedOrigins: ["*://*.example.com/*"],
    initialOriginControlsState: {
      ...ORIGIN_CONTROLS_STATES.SHOW_OPTIONS_WHEN_CLICKED_AND_ALWAYS_ON,
      hasAccess: true,
      temporaryAccess: false,
    },
    originControlsTests: [
      {
        methodCall: "setWhenClicked",
        expectedAllowedOrigins: [],
        expectedOriginControlsState(initialState) {
          return { ...initialState, hasAccess: false };
        },
      },
      {
        methodCall: "setAlwaysOn",
        expectedAllowedOrigins: ["*://www.example.com/*"],
      },
    ],
  });

  // Single NOT granted "wildcard scheme + wildcard subdomain" permission.
  await runOriginControlTests({
    tabInfo: { url: "http://www.example.com" },
    manifest: {
      optional_host_permissions: ["*://*.example.com/*"],
    },
    initialAllowedOrigins: [],
    initialOriginControlsState: {
      ...ORIGIN_CONTROLS_STATES.SHOW_OPTIONS_WHEN_CLICKED_AND_ALWAYS_ON,
      hasAccess: false,
      temporaryAccess: false,
    },
    originControlsTests: [
      {
        methodCall: "setAlwaysOn",
        expectedAllowedOrigins: ["*://www.example.com/*"],
        expectedOriginControlsState(initialState) {
          return { ...initialState, hasAccess: true };
        },
      },
      {
        methodCall: "setWhenClicked",
        expectedAllowedOrigins: [],
      },
    ],
  });

  // Two granted "scheme specific + wildcard subdomain" permissions.
  await runOriginControlTests({
    tabInfo: { url: "http://www.example.com" },
    manifest: {
      host_permissions: ["http://*.example.com/*", "https://*.example.com/*"],
    },
    initialAllowedOrigins: [
      "http://*.example.com/*",
      "https://*.example.com/*",
    ],
    initialOriginControlsState: {
      ...ORIGIN_CONTROLS_STATES.SHOW_OPTIONS_WHEN_CLICKED_AND_ALWAYS_ON,
      hasAccess: true,
      temporaryAccess: false,
    },
    originControlsTests: [
      {
        methodCall: "setWhenClicked",
        expectedAllowedOrigins: [],
        expectedOriginControlsState(initialState) {
          return { ...initialState, hasAccess: false };
        },
      },
      {
        methodCall: "setAlwaysOn",
        expectedAllowedOrigins: [
          "http://www.example.com/*",
          "https://www.example.com/*",
        ],
      },
    ],
  });
});

add_task(async function test_two_wildcard_domain_and_host_specific_match() {
  // This addon two host permissions:
  // - an already granted wildcard subdomain permission *://*.example.com
  // - and an additional not yet granted more restrictive specific
  //   domain permission not granted *://example.com
  //
  // both of them technically match the current url host,
  // which does not have any explicit subdomain.
  //
  // Then:
  //
  // 1. on the setWhenClicked call, the wildcard subdomain permission is
  //   revoked.
  //
  // 2. on a setAlwaysOn following that, the more restrictive subdomain
  //   permission is the one being granted (instead of the initial
  //   subdomain wildcard one).
  await runOriginControlTests({
    tabInfo: { url: "http://example.com" },
    manifest: {
      host_permissions: ["*://*.example.com/*"],
      optional_host_permissions: ["*://example.com/*"],
    },
    initialAllowedOrigins: ["*://*.example.com/*"],
    initialOriginControlsState: {
      ...ORIGIN_CONTROLS_STATES.SHOW_OPTIONS_WHEN_CLICKED_AND_ALWAYS_ON,
      hasAccess: true,
      temporaryAccess: false,
    },
    originControlsTests: [
      {
        methodCall: "setWhenClicked",
        expectedAllowedOrigins: [],
        expectedOriginControlsState(initialState) {
          return { ...initialState, hasAccess: false };
        },
      },
      {
        methodCall: "setAlwaysOn",
        expectedAllowedOrigins: ["*://example.com/*"],
      },
    ],
  });
});

add_task(async function test_file_url_and_file_scheme_host_perm() {
  // Granted file:///* host permission.
  // NOTE: setAlwaysOn is currently expected to be a no-op for non-http/https
  // urls, setWhenClicked does explictly avoid revoking file urls because they
  // are not currently visible in the addon manager permissions view and so
  // the user would not be able to re-grant it unless is the extension itself
  // re-requesting it through permissions.request API call.
  await runOriginControlTests({
    tabInfo: { url: "file:///some/subdir/filepath.txt" },
    manifest: {
      host_permissions: ["file:///*"],
    },
    initialAllowedOrigins: ["file:///*"],
    // TODO(Bug 1849765): the OriginControl state for file urls is currently
    // wrong and this assertion should be changed as part of that bugfix.
    initialOriginControlsState: {
      ...ORIGIN_CONTROLS_STATES.NO_ACCESS,
      quarantined: false,
    },
    originControlsTests: [
      {
        methodCall: "setWhenClicked",
        expectedAllowedOrigins: ["file:///*"],
      },
      {
        methodCall: "setAlwaysOn",
        expectedAllowedOrigins: ["file:///*"],
      },
    ],
  });
});

add_task(async function test_file_url_and_all_urls_host_perm() {
  for (const granted_host_permissions of ALL_URLS_CASES) {
    await runOriginControlTests({
      tabInfo: { url: "file:///some/subdir/filepath.txt" },
      manifest: {
        host_permissions: granted_host_permissions,
      },
      initialAllowedOrigins: granted_host_permissions,
      initialOriginControlsState: {
        ...ORIGIN_CONTROLS_STATES.NO_ACCESS,
        quarantined: false,
      },
      originControlsTests: [
        {
          methodCall: "setWhenClicked",
          expectedAllowedOrigins: granted_host_permissions,
        },
        {
          methodCall: "setAlwaysOn",
          expectedAllowedOrigins: granted_host_permissions,
        },
      ],
    });
  }
});

add_task(async function test_about_reader_unprivileged_addons() {
  await runOriginControlTests({
    tabInfo: {
      url: "about:reader?url=https%3A%2F%2Fexample.com%2F",
    },
    manifest: {
      host_permissions: ["<all_urls>", "about:reader*"],
    },
    expectPrivileged: false,
    expectRestrictSchemes: true,
    initialAllowedOrigins: ["<all_urls>"],
    initialOriginControlsState: {
      ...ORIGIN_CONTROLS_STATES.NO_ACCESS,
      quarantined: false,
    },
    originControlsTests: [
      {
        methodCall: "setWhenClicked",
        expectedAllowedOrigins: ["<all_urls>"],
      },
      {
        methodCall: "setAlwaysOn",
        expectedAllowedOrigins: ["<all_urls>"],
      },
    ],
  });
  // TODO(Bug 1910965): privileged MV3 addons can't currently get granted host permissions
  // on restricted urls, add an additional test case to cover that once fixed.
});

add_task(async function test_resource_uri_tab_unprivileged_addons() {
  await runOriginControlTests({
    tabInfo: {
      url: "resource://gre/modules/XPCOMUtils.sys.mjs",
    },
    manifest: {
      host_permissions: ["<all_urls>", "*://*/*", "resource://gre/modules/*"],
    },
    expectPrivileged: false,
    expectRestrictSchemes: true,
    initialAllowedOrigins: ["<all_urls>", "*://*/*"],
    initialOriginControlsState: {
      ...ORIGIN_CONTROLS_STATES.NO_ACCESS,
      quarantined: false,
    },
    originControlsTests: [
      {
        methodCall: "setWhenClicked",
        expectedAllowedOrigins: ["<all_urls>", "*://*/*"],
      },
      {
        methodCall: "setAlwaysOn",
        expectedAllowedOrigins: ["<all_urls>", "*://*/*"],
      },
    ],
  });
  // TODO(Bug 1910965): privileged MV3 addons can't currently get granted host permissions
  // on restricted urls, add an additional test case to cover that once fixed.
});

add_task(async function test_viewsource_uri_tab_unprivileged_addons() {
  await runOriginControlTests({
    tabInfo: {
      url: "view-source:https://example.com/",
    },
    manifest: {
      host_permissions: ["<all_urls>", "*://*/*", "*://example.com/*"],
    },
    expectPrivileged: false,
    expectRestrictSchemes: true,
    initialAllowedOrigins: ["<all_urls>", "*://*/*", "*://example.com/*"],
    initialOriginControlsState: {
      ...ORIGIN_CONTROLS_STATES.NO_ACCESS,
      quarantined: false,
    },
    originControlsTests: [
      {
        methodCall: "setWhenClicked",
        expectedAllowedOrigins: ["<all_urls>", "*://*/*", "*://example.com/*"],
      },
      {
        methodCall: "setAlwaysOn",
        expectedAllowedOrigins: ["<all_urls>", "*://*/*", "*://example.com/*"],
      },
    ],
  });
});
