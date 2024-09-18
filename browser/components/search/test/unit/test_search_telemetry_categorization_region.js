/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests the region logic in DomainToCategoriesMap.
 */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  SearchSERPDomainToCategoriesMap:
    "resource:///modules/SearchSERPTelemetry.sys.mjs",
});

add_task(async function record_matches_region() {
  const TESTS = [
    {
      title: "Home region, blank include and exclude.",
      record: {
        includeRegions: [],
        excludeRegions: [],
      },
      region: "US",
      expectedResult: false,
    },
    {
      title: "Null region, blank include and exclude.",
      record: {
        includeRegions: [],
        excludeRegions: [],
      },
      region: null,
      expectedResult: false,
    },
    {
      title: "Null region, with include.",
      record: {
        includeRegions: ["US"],
        excludeRegions: [],
      },
      region: null,
      expectedResult: false,
    },
    {
      title: "Blank region, blank include and exclude.",
      record: {
        includeRegions: [],
        excludeRegions: [],
      },
      region: "",
      expectedResult: false,
    },
    {
      title: "Blank region, with include.",
      record: {
        includeRegions: ["US"],
        excludeRegions: [],
      },
      region: "",
      expectedResult: false,
    },
    {
      title: "Included region.",
      record: {
        includeRegions: ["US"],
        excludeRegions: [],
      },
      region: "US",
      expectedResult: true,
    },
    {
      title: "Region is not included.",
      record: {
        includeRegions: ["DE"],
        excludeRegions: [],
      },
      region: "US",
      expectedResult: false,
    },
    {
      title: "Multiple included regions.",
      record: {
        includeRegions: ["DE", "CA", "US"],
        excludeRegions: [],
      },
      region: "US",
      expectedResult: true,
    },
    {
      title: "Excluded region.",
      record: {
        includeRegions: [],
        excludeRegions: ["US"],
      },
      region: "US",
      expectedResult: false,
    },
    {
      title: "Multiple excluded regions.",
      record: {
        includeRegions: [],
        excludeRegions: ["DE", "CA", "US"],
      },
      region: "US",
      expectedResult: false,
    },
  ];

  for (let { title, record, expectedResult, region } of TESTS) {
    info(title);
    let result = SearchSERPDomainToCategoriesMap.recordMatchesRegion(
      record,
      region
    );
    Assert.equal(result, expectedResult, "Result should match.");
  }
});

add_task(async function find_records_for_region() {
  const TESTS = [
    {
      title: "Region matches custom region.",
      record: [
        { includeRegions: ["US"], excludeRegions: [], isDefault: false },
        { includeRegions: [], excludeRegions: [], isDefault: true },
      ],
      region: "US",
      expectedResult: {
        isDefault: false,
        records: [
          {
            includeRegions: ["US"],
            excludeRegions: [],
            isDefault: false,
          },
        ],
      },
    },
    {
      title: "Region matches multiple custom regions.",
      record: [
        { includeRegions: ["US"], excludeRegions: [], isDefault: false },
        { includeRegions: ["DE"], excludeRegions: [], isDefault: false },
        { includeRegions: [], excludeRegions: [], isDefault: true },
        { includeRegions: ["US"], excludeRegions: [], isDefault: false },
      ],
      region: "US",
      expectedResult: {
        isDefault: false,
        records: [
          {
            includeRegions: ["US"],
            excludeRegions: [],
            isDefault: false,
          },
          {
            includeRegions: ["US"],
            excludeRegions: [],
            isDefault: false,
          },
        ],
      },
    },
    {
      title: "Region matches default record.",
      record: [
        { includeRegions: ["US"], excludeRegions: [], isDefault: false },
        { includeRegions: [], excludeRegions: [], isDefault: true },
      ],
      region: "CA",
      expectedResult: {
        isDefault: true,
        records: [
          {
            includeRegions: [],
            excludeRegions: [],
            isDefault: true,
          },
        ],
      },
    },
    {
      title: "Region matches multiple default records.",
      record: [
        { includeRegions: [], excludeRegions: [], isDefault: true },
        { includeRegions: ["US"], excludeRegions: [], isDefault: false },
        { includeRegions: ["DE"], excludeRegions: [], isDefault: false },
        { includeRegions: [], excludeRegions: [], isDefault: true },
      ],
      region: "CA",
      expectedResult: {
        isDefault: true,
        records: [
          {
            includeRegions: [],
            excludeRegions: [],
            isDefault: true,
          },
          {
            includeRegions: [],
            excludeRegions: [],
            isDefault: true,
          },
        ],
      },
    },
    {
      title: "Region doesn't match default record.",
      record: [{ includeRegions: [], excludeRegions: ["CA"], isDefault: true }],
      region: "CA",
      expectedResult: null,
    },
  ];

  for (let { title, record, expectedResult, region } of TESTS) {
    info(title);
    let result = SearchSERPDomainToCategoriesMap.findRecordsForRegion(
      record,
      region
    );
    Assert.deepEqual(result, expectedResult, "Result should match.");
  }
});
