async function clearAllCache() {
  await new Promise(function (resolve) {
    Services.clearData.deleteData(
      Ci.nsIClearDataService.CLEAR_ALL_CACHES,
      resolve
    );
  });
}

const URL_BASE = "https://example.com/browser/layout/style/test/";
const URL_BASE2 = "https://example.net/browser/layout/style/test/";

function testFields(
  entry,
  { hasBodyAccess, hasTimingAccess, isCacheOf },
  desc
) {
  Assert.equal(entry.entryType, "resource", "entryType should be available");
  Assert.equal(
    entry.initiatorType,
    "link",
    "initiatorType should be available"
  );

  if (hasTimingAccess) {
    Assert.equal(
      entry.nextHopProtocol,
      "http/1.1",
      `nextHopProtocol should be available for ${desc}`
    );
  } else {
    Assert.equal(
      entry.nextHopProtocol,
      "",
      `nextHopProtocol should be hidden for ${desc}`
    );
  }

  if (hasBodyAccess) {
    Assert.equal(
      entry.responseStatus,
      200,
      `responseStatus should be available for ${desc}`
    );
  } else {
    Assert.equal(
      entry.responseStatus,
      0,
      `responseStatus should be hidden for ${desc}`
    );
  }

  if (hasBodyAccess) {
    Assert.equal(
      entry.contentType,
      "text/css",
      `contentType should be available for ${desc}`
    );
  } else {
    Assert.equal(
      entry.contentType,
      "",
      `contentType should be hidden for ${desc}`
    );
  }

  Assert.greater(
    entry.startTime,
    0,
    `startTime should be non-zero for ${desc}`
  );
  Assert.greater(
    entry.responseEnd,
    0,
    `responseEnd should be non-zero for ${desc}`
  );
  Assert.lessOrEqual(
    entry.startTime,
    entry.responseEnd,
    `startTime <= responseEnd for ${desc}`
  );

  if (hasTimingAccess) {
    Assert.deepEqual(
      entry.serverTiming,
      [
        { name: "name1", duration: 0, description: "" },
        { name: "name2", duration: 20, description: "" },
        { name: "name3", duration: 30, description: "desc3" },
      ],
      `serverTiming should be available for ${desc}`
    );
  } else {
    Assert.deepEqual(
      entry.serverTiming,
      [],
      `serverTiming should be hidden for ${desc}`
    );
  }

  if (hasBodyAccess) {
    Assert.greater(
      entry.encodedBodySize,
      0,
      `encodedBodySize should be available for ${desc}`
    );
  } else {
    Assert.equal(
      entry.encodedBodySize,
      0,
      `encodedBodySize should be hidden for ${desc}`
    );
  }

  if (isCacheOf) {
    Assert.equal(
      entry.encodedBodySize,
      isCacheOf.encodedBodySize,
      `encodedBodySize should equal to non-cache case for ${desc}`
    );
  }

  if (hasBodyAccess) {
    Assert.greater(
      entry.decodedBodySize,
      0,
      `decodedBodySize should be available for ${desc}`
    );
  } else {
    Assert.equal(
      entry.decodedBodySize,
      0,
      `decodedBodySize should be hidden for ${desc}`
    );
  }

  if (isCacheOf) {
    Assert.equal(
      entry.decodedBodySize,
      isCacheOf.decodedBodySize,
      `decodedBodySize should equal to non-cache case for ${desc}`
    );
  }

  if (hasTimingAccess) {
    if (isCacheOf) {
      Assert.equal(
        entry.transferSize,
        0,
        `transferSize should be zero for ${desc}`
      );
    } else if (hasBodyAccess) {
      Assert.greater(
        entry.transferSize,
        300,
        `transferSize should be non-zero +300 for ${desc}`
      );
    } else {
      Assert.equal(
        entry.transferSize,
        300,
        `transferSize should be zero +300 for ${desc}`
      );
    }
  } else {
    Assert.equal(
      entry.transferSize,
      0,
      `transferSize should be hidden for ${desc}`
    );
  }
}

add_task(async function testCompleteCacheAfterReload() {
  await clearAllCache();

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: URL_BASE + "empty.html",
    },
    async function (browser) {
      const CSS_URL = URL_BASE + "css_server.sjs?cacheable";

      const task = async url => {
        await new Promise(resolve => {
          const link = content.document.createElement("link");
          link.rel = "stylesheet";
          link.href = url;
          link.addEventListener("load", resolve);
          content.document.head.append(link);
        });

        const entries = content.performance
          .getEntriesByType("resource")
          .filter(entry => entry.name.includes("css_server.sjs"));
        if (entries.length != 1) {
          throw new Error(`Expect one entry, got ${entries.length} entries`);
        }
        // NOTE: entries[0].toJSON() doesn't convert serverTiming items.
        return JSON.parse(JSON.stringify(entries[0]));
      };

      const entry = await SpecialPowers.spawn(browser, [CSS_URL], task);
      Assert.equal(entry.name, CSS_URL);
      testFields(
        entry,
        {
          hasBodyAccess: true,
          hasTimingAccess: true,
        },
        "same origin (non-cached)"
      );

      await BrowserTestUtils.reloadTab(gBrowser.selectedTab);

      const cacheEntry = await SpecialPowers.spawn(browser, [CSS_URL], task);
      Assert.equal(cacheEntry.name, CSS_URL);
      testFields(
        cacheEntry,
        {
          hasBodyAccess: true,
          hasTimingAccess: true,
          isCacheOf: entry,
        },
        "same origin (cached)"
      );
    }
  );
});

add_task(async function testCompleteCacheInSameDocument() {
  await clearAllCache();

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: URL_BASE + "empty.html",
    },
    async function (browser) {
      const CSS_URL = URL_BASE + "css_server.sjs?cacheable";

      const task = async url => {
        // Before reload:
        //   * The first load is not cache
        //   * The second load is complete cache
        // After reload:
        //   * Both loads are complete cache

        for (let i = 0; i < 2; i++) {
          await new Promise(resolve => {
            const link = content.document.createElement("link");
            link.rel = "stylesheet";
            link.href = url;
            link.addEventListener("load", () => {
              resolve();
            });
            content.document.head.append(link);
          });
        }

        const entries = content.performance
          .getEntriesByType("resource")
          .filter(entry => entry.name.includes("css_server.sjs"));
        if (entries.length != 1) {
          throw new Error(`Expect one entry, got ${entries.length} entries`);
        }
        return JSON.parse(JSON.stringify(entries[0]));
      };

      const entry = await SpecialPowers.spawn(browser, [CSS_URL], task);
      Assert.equal(entry.name, CSS_URL);
      testFields(
        entry,
        {
          hasBodyAccess: true,
          hasTimingAccess: true,
        },
        "same origin (non-cached)"
      );

      await BrowserTestUtils.reloadTab(gBrowser.selectedTab);

      const cacheEntry = await SpecialPowers.spawn(browser, [CSS_URL], task);
      Assert.equal(cacheEntry.name, CSS_URL);
      testFields(
        cacheEntry,
        {
          hasBodyAccess: true,
          hasTimingAccess: true,
          isCacheOf: entry,
        },
        "same origin (cached)"
      );
    }
  );
});

add_task(async function testIncompleteCacheInSameDocument() {
  await clearAllCache();

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: URL_BASE + "empty.html",
    },
    async function (browser) {
      const CSS_URL = URL_BASE + "css_server.sjs?cacheable,slow";

      const task = async url => {
        const promises = [];
        for (let i = 0; i < 2; i++) {
          // The first load is not cache.
          // The load load uses pending or loading cache, which is
          // created by the first load.

          promises.push(
            new Promise(resolve => {
              const link = content.document.createElement("link");
              link.rel = "stylesheet";
              link.href = url;
              link.addEventListener("load", () => {
                resolve();
              });
              content.document.head.append(link);
            })
          );
        }

        await Promise.all(promises);

        const entries = content.performance
          .getEntriesByType("resource")
          .filter(entry => entry.name.includes("css_server.sjs"));
        if (entries.length != 1) {
          throw new Error(`Expect one entry, got ${entries.length} entries`);
        }
        return JSON.parse(JSON.stringify(entries[0]));
      };

      const entry = await SpecialPowers.spawn(browser, [CSS_URL], task);
      Assert.equal(entry.name, CSS_URL);
      testFields(
        entry,
        {
          hasBodyAccess: true,
          hasTimingAccess: true,
        },
        "same origin (non-cached)"
      );
    }
  );
});

add_task(async function testIncompleteCacheInAnotherTab() {
  await clearAllCache();

  const CSS_URL = URL_BASE + "css_server.sjs?cacheable,slow";

  // Prepare 2 tabs in the same process.
  const tab1 = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: URL_BASE + "empty.html",
  });
  const tab2Promise = BrowserTestUtils.waitForNewTab(gBrowser, null, true);
  SpecialPowers.spawn(tab1.linkedBrowser, [], () => {
    content.window.open("empty.html");
  });
  const tab2 = await tab2Promise;

  const task = async url => {
    await new Promise(resolve => {
      const link = content.document.createElement("link");
      link.rel = "stylesheet";
      link.href = url;
      link.addEventListener("load", () => {
        resolve();
      });
      content.document.head.append(link);
    });

    const entries = content.performance
      .getEntriesByType("resource")
      .filter(entry => entry.name.includes("css_server.sjs"));
    if (entries.length != 1) {
      throw new Error(`Expect one entry, got ${entries.length} entries`);
    }
    return JSON.parse(JSON.stringify(entries[0]));
  };

  // Tab1's load is not cache.
  // Tab2's load uses the pending or loading cache, which is created by the
  // tab1's load.
  const p1 = SpecialPowers.spawn(tab1.linkedBrowser, [CSS_URL], task);
  const p2 = SpecialPowers.spawn(tab2.linkedBrowser, [CSS_URL], task);

  const entry1 = await p1;

  Assert.equal(entry1.name, CSS_URL);
  testFields(
    entry1,
    {
      hasBodyAccess: true,
      hasTimingAccess: true,
    },
    "same origin (non-cached)"
  );

  const entry2 = await p2;

  Assert.equal(entry2.name, CSS_URL);
  testFields(
    entry2,
    {
      hasBodyAccess: true,
      hasTimingAccess: true,
      isCacheOf: entry1,
    },
    "same origin (cached)"
  );

  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
});

add_task(async function testNoCacheReload() {
  await clearAllCache();

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: URL_BASE + "empty.html",
    },
    async function (browser) {
      const CSS_URL = URL_BASE + "css_server.sjs?";

      const task = async url => {
        await new Promise(resolve => {
          const link = content.document.createElement("link");
          link.rel = "stylesheet";
          link.href = url;
          link.addEventListener("load", resolve);
          content.document.head.append(link);
        });

        const entries = content.performance
          .getEntriesByType("resource")
          .filter(entry => entry.name.includes("css_server.sjs"));
        if (entries.length != 1) {
          throw new Error(`Expect one entry, got ${entries.length} entries`);
        }
        return JSON.parse(JSON.stringify(entries[0]));
      };

      const entry = await SpecialPowers.spawn(browser, [CSS_URL], task);
      Assert.equal(entry.name, CSS_URL);
      testFields(
        entry,
        {
          hasBodyAccess: true,
          hasTimingAccess: true,
        },
        "same origin (non-cached)"
      );

      await BrowserTestUtils.reloadTab(gBrowser.selectedTab);

      // Reloading the CSS shouldn't hit any cache.

      const reloadEntry = await SpecialPowers.spawn(browser, [CSS_URL], task);
      Assert.equal(reloadEntry.name, CSS_URL);
      testFields(
        reloadEntry,
        {
          hasBodyAccess: true,
          hasTimingAccess: true,
        },
        "same origin (non-cached)"
      );
    }
  );
});

add_task(async function test_NoCORS() {
  await clearAllCache();

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: URL_BASE + "empty.html",
    },
    async function (browser) {
      const CSS_URL = URL_BASE2 + "css_server.sjs?cacheable";

      const task = async url => {
        await new Promise(resolve => {
          const link = content.document.createElement("link");
          link.rel = "stylesheet";
          link.href = url;
          link.addEventListener("load", resolve);
          content.document.head.append(link);
        });

        const entries = content.performance
          .getEntriesByType("resource")
          .filter(entry => entry.name.includes("css_server.sjs"));
        if (entries.length != 1) {
          throw new Error(`Expect one entry, got ${entries.length} entries`);
        }
        return JSON.parse(JSON.stringify(entries[0]));
      };

      const entry = await SpecialPowers.spawn(browser, [CSS_URL], task);
      Assert.equal(entry.name, CSS_URL);
      testFields(
        entry,
        {
          hasBodyAccess: false,
          hasTimingAccess: false,
        },
        "cross origin (non-cached)"
      );

      await BrowserTestUtils.reloadTab(gBrowser.selectedTab);

      const cacheEntry = await SpecialPowers.spawn(browser, [CSS_URL], task);
      Assert.equal(cacheEntry.name, CSS_URL);
      testFields(
        cacheEntry,
        {
          hasBodyAccess: false,
          hasTimingAccess: false,
          isCacheOf: entry,
        },
        "cross origin (cached)"
      );
    }
  );
});

add_task(async function test_NoCORS_TAO() {
  await clearAllCache();

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: URL_BASE + "empty.html",
    },
    async function (browser) {
      const CSS_URL = URL_BASE2 + "css_server.sjs?cacheable,tao";

      const task = async url => {
        await new Promise(resolve => {
          const link = content.document.createElement("link");
          link.rel = "stylesheet";
          link.href = url;
          link.addEventListener("load", resolve);
          content.document.head.append(link);
        });

        const entries = content.performance
          .getEntriesByType("resource")
          .filter(entry => entry.name.includes("css_server.sjs"));
        if (entries.length != 1) {
          throw new Error(`Expect one entry, got ${entries.length} entries`);
        }
        return JSON.parse(JSON.stringify(entries[0]));
      };

      const entry = await SpecialPowers.spawn(browser, [CSS_URL], task);
      Assert.equal(entry.name, CSS_URL);
      testFields(
        entry,
        {
          hasBodyAccess: false,
          hasTimingAccess: true,
        },
        "cross origin with Timing-Allow-Origin (non-cached)"
      );

      await BrowserTestUtils.reloadTab(gBrowser.selectedTab);

      const cacheEntry = await SpecialPowers.spawn(browser, [CSS_URL], task);
      Assert.equal(cacheEntry.name, CSS_URL);
      testFields(
        cacheEntry,
        {
          hasBodyAccess: false,
          hasTimingAccess: true,
          isCacheOf: entry,
        },
        "cross origin with Timing-Allow-Origin (cached)"
      );
    }
  );
});

add_task(async function test_CORS() {
  await clearAllCache();

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: URL_BASE + "empty.html",
    },
    async function (browser) {
      const CSS_URL = URL_BASE2 + "css_server.sjs?cacheable,cors";

      const task = async url => {
        await new Promise(resolve => {
          const link = content.document.createElement("link");
          link.rel = "stylesheet";
          link.setAttribute("crossorigin", "anonymous");
          link.href = url;
          link.addEventListener("load", resolve);
          content.document.head.append(link);
        });

        const entries = content.performance
          .getEntriesByType("resource")
          .filter(entry => entry.name.includes("css_server.sjs"));
        if (entries.length != 1) {
          throw new Error(`Expect one entry, got ${entries.length} entries`);
        }
        return JSON.parse(JSON.stringify(entries[0]));
      };

      const entry = await SpecialPowers.spawn(browser, [CSS_URL], task);
      Assert.equal(entry.name, CSS_URL);
      testFields(
        entry,
        {
          hasBodyAccess: true,
          hasTimingAccess: false,
        },
        "CORS (non-cached)"
      );

      await BrowserTestUtils.reloadTab(gBrowser.selectedTab);

      const cacheEntry = await SpecialPowers.spawn(browser, [CSS_URL], task);
      Assert.equal(cacheEntry.name, CSS_URL);
      testFields(
        cacheEntry,
        {
          hasBodyAccess: true,
          hasTimingAccess: false,
          isCacheOf: entry,
        },
        "cors-cached"
      );
    }
  );
});

add_task(async function test_CORS_TAO() {
  await clearAllCache();

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: URL_BASE + "empty.html",
    },
    async function (browser) {
      const CSS_URL = URL_BASE2 + "css_server.sjs?cacheable,cors,tao";

      const task = async url => {
        await new Promise(resolve => {
          const link = content.document.createElement("link");
          link.rel = "stylesheet";
          link.setAttribute("crossorigin", "anonymous");
          link.href = url;
          link.addEventListener("load", resolve);
          content.document.head.append(link);
        });

        const entries = content.performance
          .getEntriesByType("resource")
          .filter(entry => entry.name.includes("css_server.sjs"));
        if (entries.length != 1) {
          throw new Error(`Expect one entry, got ${entries.length} entries`);
        }
        return JSON.parse(JSON.stringify(entries[0]));
      };

      const entry = await SpecialPowers.spawn(browser, [CSS_URL], task);
      Assert.equal(entry.name, CSS_URL);
      testFields(
        entry,
        {
          hasBodyAccess: true,
          hasTimingAccess: true,
        },
        "CORS with Timing-Allow-Origin (non-cached)"
      );

      await BrowserTestUtils.reloadTab(gBrowser.selectedTab);

      const cacheEntry = await SpecialPowers.spawn(browser, [CSS_URL], task);
      Assert.equal(cacheEntry.name, CSS_URL);
      testFields(
        cacheEntry,
        {
          hasBodyAccess: true,
          hasTimingAccess: true,
          isCacheOf: entry,
        },
        "CORS with Timing-Allow-Origin (cached)"
      );
    }
  );
});
