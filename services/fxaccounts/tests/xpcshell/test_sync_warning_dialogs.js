/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  FxAccountsWebChannelHelpers:
    "resource://gre/modules/FxAccountsWebChannel.sys.mjs",
});

// Set up mocked profiles
const mockedProfiles = [
  {
    name: "Profile1",
    path: PathUtils.join(PathUtils.tempDir, "current-profile"),
    email: "testuser1@test.com",
  },
  {
    name: "Profile2",
    path: PathUtils.join(PathUtils.tempDir, "other-profile"),
    email: "testuser2@test.com",
  },
];

// Emulates the response from the user
let gResponse = 1;
(function replacePromptService() {
  let originalPromptService = Services.prompt;
  Services.prompt = {
    QueryInterface: ChromeUtils.generateQI([Ci.nsIPromptService]),
    confirmEx: () => gResponse,
  };
  registerCleanupFunction(() => {
    Services.prompt = originalPromptService;
  });
})();

add_setup(function setup() {
  // FOG needs a profile directory to put its data in.
  do_get_profile();
  // FOG needs to be initialized in order for data to flow.
  Services.fog.initializeFOG();
});

const dialogVariants = [
  {
    description: "A previous account was signed into this profile",
    prefs: {
      "browser.profiles.enabled": true,
      "browser.profiles.sync.allow-danger-merge": false,
    },
    expectedResponses: [
      {
        responseVal: 0,
        expectedResult: { action: "create-profile" },
        expectedTelemetry: {
          variant_shown: "merge-warning",
          option_clicked: "create-profile",
        },
      },
      {
        responseVal: 1,
        expectedResult: { action: "cancel" },
        expectedTelemetry: {
          variant_shown: "merge-warning",
          option_clicked: "cancel",
        },
      },
    ],
  },
  {
    description:
      "A previous account was signed into this profile, with merge allowed",
    prefs: {
      "browser.profiles.enabled": true,
      "browser.profiles.sync.allow-danger-merge": true,
    },
    expectedResponses: [
      {
        responseVal: 0,
        expectedResult: { action: "continue" },
        expectedTelemetry: {
          variant_shown: "merge-warning-allow-merge",
          option_clicked: "continue",
        },
      },
      {
        responseVal: 1,
        expectedResult: { action: "create-profile" },
        expectedTelemetry: {
          variant_shown: "merge-warning-allow-merge",
          option_clicked: "create-profile",
        },
      },
      {
        responseVal: 2,
        expectedResult: { action: "cancel" },
        expectedTelemetry: {
          option_clicked: "cancel",
          variant_shown: "merge-warning-allow-merge",
        },
      },
    ],
  },
];

add_task(
  async function test_previously_signed_in_dialog_variants_result_and_telemetry() {
    // Create a helper instance
    let helpers = new FxAccountsWebChannelHelpers();

    // We "pretend" there was another account previously logged in
    helpers.setPreviousAccountNameHashPref("testuser@testuser.com");

    // Mock methods
    helpers._getAllProfiles = async () => mockedProfiles;
    helpers._getCurrentProfileName = () => mockedProfiles[0].name;
    helpers._readJSONFileAsync = async function (_filePath) {
      return null;
    };

    for (let variant of dialogVariants) {
      info(`Testing variant: ${variant.description}`);
      // Set the preferences for this variant
      for (let [prefName, prefValue] of Object.entries(variant.prefs)) {
        Services.prefs.setBoolPref(prefName, prefValue);
      }

      for (let i = 0; i < variant.expectedResponses.length; i++) {
        let { responseVal, expectedResult, expectedTelemetry } =
          variant.expectedResponses[i];

        gResponse = responseVal;
        let result =
          await helpers.promptProfileSyncWarningIfNeeded("testuser2@test.com");
        //Verify we returned the expected result
        Assert.deepEqual(result, expectedResult);

        let gleanValue = Glean.syncMergeDialog.clicked.testGetValue();
        // Verify the telemetry is shaped as expected
        Assert.equal(
          gleanValue[i].extra.variant_shown,
          expectedTelemetry.variant_shown,
          "Correctly logged which dialog variant was shown to the user"
        );
        Assert.equal(
          gleanValue[i].extra.option_clicked,
          expectedTelemetry.option_clicked,
          "Correctly logged which option the user selected"
        );
      }
      // Reset Glean for next iteration
      Services.fog.testResetFOG();
    }

    // Clean up preferences
    Services.prefs.clearUserPref("browser.profiles.enabled");
    Services.prefs.clearUserPref("browser.profiles.sync.allow-danger-merge");
  }
);

/**
 *  Testing the dialog variants where another profile is signed into the account
 *  we're trying to sign into
 */
const anotherProfileDialogVariants = [
  {
    description:
      "Another profile is logged into the account we're trying to sign into",
    prefs: {
      "browser.profiles.enabled": true,
      "browser.profiles.sync.allow-danger-merge": false,
    },
    expectedResponses: [
      {
        responseVal: 0,
        // switch-profile also returns what the profile we switch to
        expectedResult: {
          action: "switch-profile",
          data: {
            name: "Profile2",
            path: PathUtils.join(PathUtils.tempDir, "other-profile"),
            email: "testuser2@test.com",
          },
        },
        expectedTelemetry: {
          option_clicked: "switch-profile",
          variant_shown: "sync-warning",
        },
      },
      {
        responseVal: 1,
        expectedResult: { action: "cancel" },
        expectedTelemetry: {
          option_clicked: "cancel",
          variant_shown: "sync-warning",
        },
      },
    ],
  },
  {
    description:
      "Another profile is logged into the account we're trying to sign into, with merge allowed",
    prefs: {
      "browser.profiles.enabled": true,
      "browser.profiles.sync.allow-danger-merge": true,
    },
    expectedResponses: [
      {
        responseVal: 0,
        expectedResult: { action: "continue" },
        expectedTelemetry: {
          option_clicked: "continue",
          variant_shown: "sync-warning-allow-merge",
        },
      },
      {
        responseVal: 1,
        // switch-profile also returns what the profile we switch to
        expectedResult: {
          action: "switch-profile",
          data: {
            name: "Profile2",
            path: PathUtils.join(PathUtils.tempDir, "other-profile"),
            email: "testuser2@test.com",
          },
        },
        expectedTelemetry: {
          option_clicked: "switch-profile",
          variant_shown: "sync-warning-allow-merge",
        },
      },
      {
        responseVal: 2,
        expectedResult: { action: "cancel" },
        expectedTelemetry: {
          option_clicked: "cancel",
          variant_shown: "sync-warning-allow-merge",
        },
      },
    ],
  },
];

add_task(
  async function test_another_profile_signed_in_variants_result_and_telemetry() {
    // Create a helper instance
    let helpers = new FxAccountsWebChannelHelpers();

    // Mock methods
    helpers._getAllProfiles = async () => mockedProfiles;
    helpers._getCurrentProfileName = () => mockedProfiles[0].name;
    // Mock the file reading to simulate the account being signed into the other profile
    helpers._readJSONFileAsync = async function (filePath) {
      if (filePath.includes("current-profile")) {
        // No signed-in user in the current profile
        return null;
      } else if (filePath.includes("other-profile")) {
        // The account is signed into the other profile
        return {
          version: 1,
          accountData: { email: "testuser2@test.com" },
        };
      }
      return null;
    };

    for (let variant of anotherProfileDialogVariants) {
      info(`Testing variant: ${variant.description}`);
      // Set the preferences for this variant
      for (let [prefName, prefValue] of Object.entries(variant.prefs)) {
        Services.prefs.setBoolPref(prefName, prefValue);
      }

      for (let i = 0; i < variant.expectedResponses.length; i++) {
        let { responseVal, expectedResult, expectedTelemetry } =
          variant.expectedResponses[i];

        gResponse = responseVal;
        let result =
          await helpers.promptProfileSyncWarningIfNeeded("testuser2@test.com");
        //Verify we returned the expected result
        Assert.deepEqual(result, expectedResult);

        let gleanValue = Glean.syncMergeDialog.clicked.testGetValue();
        // Verify the telemetry is shaped as expected
        Assert.equal(
          gleanValue[i].extra.variant_shown,
          expectedTelemetry.variant_shown,
          "Correctly logged which dialog variant was shown to the user"
        );
        Assert.equal(
          gleanValue[i].extra.option_clicked,
          expectedTelemetry.option_clicked,
          "Correctly logged which option the user selected"
        );
      }
      // Reset Glean for next iteration
      Services.fog.testResetFOG();
    }

    // Clean up preferences
    Services.prefs.clearUserPref("browser.profiles.enabled");
    Services.prefs.clearUserPref("browser.profiles.sync.allow-danger-merge");
  }
);
