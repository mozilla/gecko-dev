/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_remote_settings_versioning() {
  const testCases = [
    {
      minSupportedMajorVersion: 1,
      maxSupportedMajorVersion: 3,
      currentBestVersion: null,
      contendingVersion: "1.0a1",
      expectContendingIsBetter: true,
      description:
        "Contending pre-release version aligned with minimum supported major version without current best version",
    },
    {
      minSupportedMajorVersion: 1,
      maxSupportedMajorVersion: 3,
      currentBestVersion: null,
      contendingVersion: "1.0",
      expectContendingIsBetter: true,
      description:
        "Contending release version aligned with minimum supported major version without current best version",
    },
    {
      minSupportedMajorVersion: 1,
      maxSupportedMajorVersion: 3,
      currentBestVersion: null,
      contendingVersion: "2.0a1",
      expectContendingIsBetter: true,
      description:
        "Contending pre-release version between supported major versions without current best version",
    },
    {
      minSupportedMajorVersion: 1,
      maxSupportedMajorVersion: 3,
      currentBestVersion: null,
      contendingVersion: "2.0",
      expectContendingIsBetter: true,
      description:
        "Contending release version between supported major versions without current best version",
    },
    {
      minSupportedMajorVersion: 1,
      maxSupportedMajorVersion: 3,
      currentBestVersion: null,
      contendingVersion: "3.0a1",
      expectContendingIsBetter: true,
      description:
        "Contending pre-release version with maximum supported major version without current best version",
    },
    {
      minSupportedMajorVersion: 1,
      maxSupportedMajorVersion: 3,
      currentBestVersion: null,
      contendingVersion: "3.0",
      expectContendingIsBetter: true,
      description:
        "Contending release version aligned with maximum supported major version without current best version",
    },
    {
      minSupportedMajorVersion: 1,
      maxSupportedMajorVersion: 1,
      currentBestVersion: null,
      contendingVersion: "1.0a1",
      expectContendingIsBetter: true,
      description:
        "Contending pre-release version aligned with minimum and maximum supported major versions without current best version",
    },
    {
      minSupportedMajorVersion: 1,
      maxSupportedMajorVersion: 1,
      currentBestVersion: null,
      contendingVersion: "1.0",
      expectContendingIsBetter: true,
      description:
        "Contending release version aligned with minimum and maximum supported major versions without current best version",
    },
    {
      minSupportedMajorVersion: 1,
      maxSupportedMajorVersion: 2,
      currentBestVersion: "1.0a1",
      contendingVersion: "1.0a2",
      expectContendingIsBetter: true,
      description:
        "Contending pre-release tag version is larger than current pre-release tag version",
    },
    {
      minSupportedMajorVersion: 1,
      maxSupportedMajorVersion: 2,
      currentBestVersion: "1.0a2",
      contendingVersion: "1.1a1",
      expectContendingIsBetter: true,
      description:
        "Contending pre-release minor version is larger than current pre-release version",
    },
    {
      minSupportedMajorVersion: 1,
      maxSupportedMajorVersion: 2,
      currentBestVersion: "1.0a2",
      contendingVersion: "2.0a1",
      expectContendingIsBetter: true,
      description:
        "Contending pre-release major version is larger than current pre-release version",
    },
    {
      minSupportedMajorVersion: 1,
      maxSupportedMajorVersion: 2,
      currentBestVersion: "1.1",
      contendingVersion: "2.0a1",
      expectContendingIsBetter: true,
      description:
        "Contending pre-release major version is larger than current release major version",
    },
    {
      minSupportedMajorVersion: 1,
      maxSupportedMajorVersion: 2,
      currentBestVersion: "1.0",
      contendingVersion: "1.1a1",
      expectContendingIsBetter: true,
      description:
        "Contending pre-release minor version is larger than the current release minor version within the same major version",
    },
    {
      minSupportedMajorVersion: 1,
      maxSupportedMajorVersion: 2,
      currentBestVersion: "1.0",
      contendingVersion: "1.1",
      expectContendingIsBetter: true,
      description:
        "Contending release minor version is larger than the current release minor version within the same major version",
    },
    {
      minSupportedMajorVersion: 1,
      maxSupportedMajorVersion: 2,
      currentBestVersion: "1.0a1",
      contendingVersion: "1.0",
      expectContendingIsBetter: true,
      description:
        "Contending release major version is equal to pre-release major version",
    },
    {
      minSupportedMajorVersion: 1,
      maxSupportedMajorVersion: 2,
      currentBestVersion: "1.0a2",
      contendingVersion: "1.0a1",
      expectContendingIsBetter: false,
      description:
        "Contending pre-release tag version is smaller than current pre-release version",
    },
    {
      minSupportedMajorVersion: 1,
      maxSupportedMajorVersion: 2,
      currentBestVersion: "1.1a1",
      contendingVersion: "1.0a1",
      expectContendingIsBetter: false,
      description:
        "Contending pre-release minor version is smaller than current pre-release minor version",
    },
    {
      minSupportedMajorVersion: 1,
      maxSupportedMajorVersion: 2,
      currentBestVersion: "2.0a1",
      contendingVersion: "1.0a1",
      expectContendingIsBetter: false,
      description:
        "Contending pre-release major version is smaller than current pre-release major version",
    },
    {
      minSupportedMajorVersion: 1,
      maxSupportedMajorVersion: 2,
      currentBestVersion: "1.1a1",
      contendingVersion: "1.0",
      expectContendingIsBetter: false,
      description:
        "Contending release minor version is smaller than current pre-release minor version",
    },
    {
      minSupportedMajorVersion: 1,
      maxSupportedMajorVersion: 2,
      currentBestVersion: "1.0",
      contendingVersion: "1.0a1",
      expectContendingIsBetter: false,
      description:
        "Contending pre-release version is smaller than the current release version of the same major version",
    },
    {
      minSupportedMajorVersion: 1,
      maxSupportedMajorVersion: 2,
      currentBestVersion: "1.1",
      contendingVersion: "1.1a1",
      expectContendingIsBetter: false,
      description:
        "Contending pre-release version is of the same minor version as current best release version",
    },
    {
      minSupportedMajorVersion: 1,
      maxSupportedMajorVersion: 2,
      currentBestVersion: "1.1",
      contendingVersion: "1.0",
      expectContendingIsBetter: false,
      description:
        "Contending release minor version is smaller than the current release minor version within the same major version",
    },
    {
      minSupportedMajorVersion: 1,
      maxSupportedMajorVersion: 2,
      currentBestVersion: "2.0a1",
      contendingVersion: "1.0",
      expectContendingIsBetter: false,
      description:
        "Contending release major version is smaller than current pre-release major version",
    },
    {
      minSupportedMajorVersion: 2,
      maxSupportedMajorVersion: 4,
      currentBestVersion: null,
      contendingVersion: "1.0a1",
      expectContendingIsBetter: false,
      description:
        "Contending pre-release version is smaller than the minimum supported major version without current best version",
    },
    {
      minSupportedMajorVersion: 2,
      maxSupportedMajorVersion: 4,
      currentBestVersion: "3.0a1",
      contendingVersion: "1.0a1",
      expectContendingIsBetter: false,
      description:
        "Contending pre-release version is smaller than the minimum supported major version with current best version",
    },
    {
      minSupportedMajorVersion: 2,
      maxSupportedMajorVersion: 4,
      currentBestVersion: null,
      contendingVersion: "1.0",
      expectContendingIsBetter: false,
      description:
        "Contending release version is smaller than the minimum supported major version without current best version",
    },
    {
      minSupportedMajorVersion: 2,
      maxSupportedMajorVersion: 4,
      currentBestVersion: "3.0a1",
      contendingVersion: "1.0",
      expectContendingIsBetter: false,
      description:
        "Contending release version is smaller than the minimum supported major version with current best version",
    },
    {
      minSupportedMajorVersion: 2,
      maxSupportedMajorVersion: 4,
      currentBestVersion: null,
      contendingVersion: "5.0a1",
      expectContendingIsBetter: false,
      description:
        "Contending pre-release version is larger than the maximum supported major version without current best version",
    },
    {
      minSupportedMajorVersion: 2,
      maxSupportedMajorVersion: 4,
      currentBestVersion: "3.0a1",
      contendingVersion: "5.0a1",
      expectContendingIsBetter: false,
      description:
        "Contending pre-release version is larger than the maximum supported major version with current best version",
    },
    {
      minSupportedMajorVersion: 2,
      maxSupportedMajorVersion: 4,
      currentBestVersion: null,
      contendingVersion: "5.0",
      expectContendingIsBetter: false,
      description:
        "Contending release version is larger than the maximum supported major version without current best version",
    },
    {
      minSupportedMajorVersion: 2,
      maxSupportedMajorVersion: 4,
      currentBestVersion: "3.0a1",
      contendingVersion: "5.0",
      expectContendingIsBetter: false,
      description:
        "Contending release version is larger than the maximum supported major version with current best version",
    },
  ];

  for (const {
    minSupportedMajorVersion,
    maxSupportedMajorVersion,
    currentBestVersion,
    contendingVersion,
    expectContendingIsBetter,
    description,
  } of testCases) {
    is(
      TranslationsParent.isBetterRecordVersion(
        minSupportedMajorVersion,
        maxSupportedMajorVersion,
        contendingVersion,
        currentBestVersion
      ),
      expectContendingIsBetter,
      `
        ${description}:

        Given a supported major version range from min(${minSupportedMajorVersion}) to max(${maxSupportedMajorVersion}),
        a current best supported version of ${currentBestVersion}, with a contending version of ${contendingVersion},
        the contending version (${contendingVersion}) is ${expectContendingIsBetter ? "" : "not "} better than the current best version (${currentBestVersion}).
      `
    );
  }
});
