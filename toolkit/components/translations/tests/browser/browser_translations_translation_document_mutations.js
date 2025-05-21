/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Request 2x longer timeout for this test.
 * There are lot of test cases in this file, but they are all of the same nature,
 * and it makes the most sense to have them all in this single test file.
 */
requestLongerTimeout(2);

/**
 * @typedef {{
 *   engineStatusCount: number,
 *   cancelCount: number,
 *   passthroughCount: number,
 *   cachedCount: number,
 *   requestCount: number,
 * }} PortData
 */

/**
 * Creates a TranslationsDocument with a mocked port that "translates"
 * by adding diacritical marks above text, in addition to appending the
 * translationId for the request at the end of the "translated" text.
 *
 * In this way, you can tell A) how many times the text has been translated
 * based on how many diacritical marks are above each letter, and B) which
 * translationId fulfilled each translation request.
 *
 * The mocked port used by this function also has special hooks to help
 * control the flow of translation requests and also assert information
 * about the requests that were sent over the port.
 *
 * No translation requests will be fulfilled until the `resolveRequests`
 * function is invoked.
 *
 * The `collectPortData` function will return a set of counters for each
 * type of request that was sent over the report @see PortData, and then
 * reset the state of the counters. This way if you are expecting 5 requests
 * and 3 cancellations, you can assert exactly that.
 *
 * @param {string} html
 */
async function setupMutationsTest(html) {
  const { mockedTranslatorPort, resolveRequests, collectPortData } =
    createControlledTranslatorPort();
  const translationsDoc = await createTranslationsDoc(html, {
    mockedTranslatorPort,
  });
  return { resolveRequests, collectPortData, ...translationsDoc };
}

/**
 * Assert that collected port-request counters meet the expectations.
 *
 * If an expectation is a number then the count must match exactly.
 * If an expectation is a function then the returned boolean must be true.
 *
 * @param {() => PortData} collectPortData
 * @param {object} portDataExpectations
 * @param {number | (number) => boolean} [portDataExpectations.expectedEngineStatusCount=0]
 * @param {number | (number) => boolean} [portDataExpectations.expectedCancelCount=0]
 * @param {number | (number) => boolean} [portDataExpectations.expectedPassthroughCount=0]
 * @param {number | (number) => boolean} [portDataExpectations.expectedCachedCount=0]
 * @param {number | (number) => boolean} [portDataExpectations.expectedRequestCount=0]
 * @param {string} [infoMessage]
 */
function assertPortData(
  collectPortData,
  {
    expectedEngineStatusCount = 0,
    expectedCancelCount = 0,
    expectedPassthroughCount = 0,
    expectedCachedCount = 0,
    expectedRequestCount = 0,
  } = {},
  infoMessage
) {
  const {
    engineStatusCount,
    cancelCount,
    passthroughCount,
    cachedCount,
    requestCount,
  } = collectPortData();

  if (infoMessage) {
    info(infoMessage);
  }

  const assertCount = (actual, expected, description) => {
    const message = `The count of ${description} should match the expectation.`;
    if (typeof expected === "function") {
      ok(expected(actual), message);
    } else {
      is(actual, expected, message);
    }
  };

  assertCount(
    engineStatusCount,
    expectedEngineStatusCount,
    "engine status requests"
  );
  assertCount(cancelCount, expectedCancelCount, "cancel requests");
  assertCount(
    passthroughCount,
    expectedPassthroughCount,
    "passthrough requests"
  );
  assertCount(cachedCount, expectedCachedCount, "cached translation requests");
  assertCount(requestCount, expectedRequestCount, "translation requests");
}

function isInRange(lower, upper, number) {
  return lower <= number && number <= upper;
}

/**
 * This test case ensures that translating a node, then rapidly mutating its
 * content N times will not necessarily produce N translation requests, rather
 * the cumulative mutations will be batched and produce only a single new translation.
 */
add_task(async function test_rapid_mutation_after_initial_translation() {
  const {
    translate,
    htmlMatches,
    cleanup,
    document,
    resolveRequests,
    collectPortData,
  } = await setupMutationsTest(/* html */ `
      <div title="This is a simple title translation">
        This is a simple content translation.
      </div>
    `);

  translate();

  await htmlMatches(
    "It translates.",
    /* html */ `
      <div title="T̅h̅i̅s̅ i̅s̅ a̅ s̅i̅m̅p̅l̅e̅ t̅i̅t̅l̅e̅ t̅r̅a̅n̅s̅l̅a̅t̅i̅o̅n̅ (id:2)">
        T̅h̅i̅s̅ i̅s̅ a̅ s̅i̅m̅p̅l̅e̅ c̅o̅n̅t̅e̅n̅t̅ t̅r̅a̅n̅s̅l̅a̅t̅i̅o̅n̅. (id:1)
      </div>
    `,
    document,
    resolveRequests
  );

  assertPortData(collectPortData, {
    expectedEngineStatusCount: 1,
    expectedRequestCount: 2,
  });

  const divElement = document.querySelector("div");
  const textNode = divElement.firstChild;

  info("Mutating the DOM node 5 times");
  for (let i = 1; i <= 5; i++) {
    textNode.nodeValue = `Mutation ${i} on element`;
  }

  info("Mutating the DOM node's title attribute 5 times");
  for (let i = 1; i <= 5; i++) {
    divElement.setAttribute("title", `Mutation ${i} on title`);
  }

  await htmlMatches(
    "The changed node gets translated",
    /* html */ `
      <div title="M̅u̅t̅a̅t̅i̅o̅n̅ 5 o̅n̅ t̅i̅t̅l̅e̅ (id:{{ [4-9]|1[0-4]}})">
        M̅u̅t̅a̅t̅i̅o̅n̅ 5 o̅n̅ e̅l̅e̅m̅e̅n̅t̅ (id:{{ [3-9]|1[0-3] }})
      </div>
    `,
    document,
    resolveRequests
  );

  assertPortData(
    collectPortData,
    {
      expectedRequestCount: 2,
    },
    "The 5 mutations are batched, and only 1 is sent for translation."
  );

  cleanup();
});

/**
 * This test case ensures that triggering a translation and then then rapidly mutating
 * a node's content N times will not necessarily produce N translation requests, rather
 * the cumulative mutations will be batched and produce only a single new translation.
 */
add_task(async function test_rapid_mutation_before_initial_translation() {
  const {
    translate,
    htmlMatches,
    cleanup,
    document,
    resolveRequests,
    collectPortData,
  } = await setupMutationsTest(/* html */ `
      <div title="This is a simple title translation">
        This is a simple content translation.
      </div>
    `);

  translate();

  const divElement = document.querySelector("div");
  const textNode = divElement.firstChild;

  info("Mutating the DOM node 5 times");
  for (let i = 1; i <= 5; i++) {
    textNode.nodeValue = `Mutation ${i} on element`;
  }

  info("Mutating the DOM node's title attribute 5 times");
  for (let i = 1; i <= 5; i++) {
    divElement.setAttribute("title", `Mutation ${i} on title`);
  }

  await htmlMatches(
    "The changed node gets translated",
    /* html */ `
      <div title="M̅u̅t̅a̅t̅i̅o̅n̅ 5 o̅n̅ t̅i̅t̅l̅e̅ (id:{{ [2-9]|1[0-4]}})">
        M̅u̅t̅a̅t̅i̅o̅n̅ 5 o̅n̅ e̅l̅e̅m̅e̅n̅t̅ (id:{{ [1-9]|1[0-3] }})
      </div>
    `,
    document,
    resolveRequests
  );

  assertPortData(
    collectPortData,
    {
      expectedEngineStatusCount: 1,
      expectedRequestCount: 2,
    },
    "The 5 mutations are batched, and only 1 is sent for translation."
  );

  cleanup();
});

/**
 * This test case rapidly mutates a node while fulfilling and cancelling
 * requests as they come, to ensure that the final state matches the final
 * mutation no matter how many requests were fulfilled or cancelled.
 */
add_task(async function test_intermittent_mutations_during_pending_requests() {
  const {
    translate,
    htmlMatches,
    cleanup,
    document,
    resolveRequests,
    collectPortData,
  } = await setupMutationsTest(/* html */ `
      <div title="This is a simple title translation">
        This is a simple content translation.
      </div>
    `);

  const translationsDoc = translate();

  await htmlMatches(
    "It translates.",
    /* html */ `
      <div title="T̅h̅i̅s̅ i̅s̅ a̅ s̅i̅m̅p̅l̅e̅ t̅i̅t̅l̅e̅ t̅r̅a̅n̅s̅l̅a̅t̅i̅o̅n̅ (id:2)">
        T̅h̅i̅s̅ i̅s̅ a̅ s̅i̅m̅p̅l̅e̅ c̅o̅n̅t̅e̅n̅t̅ t̅r̅a̅n̅s̅l̅a̅t̅i̅o̅n̅. (id:1)
      </div>
    `,
    document,
    resolveRequests
  );

  assertPortData(collectPortData, {
    expectedEngineStatusCount: 1,
    expectedRequestCount: 2,
  });

  info("Mutating the DOM node and title attribute 15 times");
  for (let i = 1; i <= 15; i++) {
    const divElement = document.querySelector("div");
    const textNode = divElement.firstChild;

    textNode.nodeValue = `Mutation ${i} on element`;
    divElement.setAttribute("title", `Mutation ${i} on title`);

    for (let j = 0; j < Math.floor(Math.random() * 2); j++) {
      await doubleRaf(document);
    }

    translationsDoc.simulateIntersectionObservationForNonPendingNodes();

    for (let j = 0; j < Math.floor(Math.random() * 2); j++) {
      await doubleRaf(document);
    }

    resolveRequests();
  }

  await htmlMatches(
    "The final translated mutations' ids are between 3 to 33",
    /* html */ `
      <div title="M̅u̅t̅a̅t̅i̅o̅n̅ 15 o̅n̅ t̅i̅t̅l̅e̅ (id:{{ ([3-9]|[1-2][0-9]|3[0-3]) }})">
        M̅u̅t̅a̅t̅i̅o̅n̅ 15 o̅n̅ e̅l̅e̅m̅e̅n̅t̅ (id:{{ ([3-9]|[1-2][0-9]|3[0-3]) }})
      </div>
    `,
    document,
    resolveRequests
  );

  const { requestCount, cancelCount } = collectPortData(
    /* resetCounters */ false
  );

  Assert.greater(
    requestCount,
    cancelCount,
    "There should be more requests than cancellations."
  );

  assertPortData(
    collectPortData,
    {
      expectedRequestCount: count => isInRange(2, 30, count),
      expectedCancelCount: count => isInRange(0, 28, count),
    },
    "The request and cancel counts are within the expected ranges."
  );

  cleanup();
});

/**
 * This test case ensures that sequential mutations that are each given the
 * time to fully complete the translation request that they generate will
 * each update the node as expected.
 */
add_task(async function test_sequential_mutations() {
  const {
    translate,
    htmlMatches,
    cleanup,
    document,
    resolveRequests,
    collectPortData,
  } = await setupMutationsTest(/* html */ `
      <div title="This is a simple title translation">
        This is a simple content translation.
      </div>
    `);

  translate();

  await htmlMatches(
    "It translates.",
    /* html */ `
      <div title="T̅h̅i̅s̅ i̅s̅ a̅ s̅i̅m̅p̅l̅e̅ t̅i̅t̅l̅e̅ t̅r̅a̅n̅s̅l̅a̅t̅i̅o̅n̅ (id:2)">
        T̅h̅i̅s̅ i̅s̅ a̅ s̅i̅m̅p̅l̅e̅ c̅o̅n̅t̅e̅n̅t̅ t̅r̅a̅n̅s̅l̅a̅t̅i̅o̅n̅. (id:1)
      </div>
    `,
    document,
    resolveRequests
  );

  assertPortData(collectPortData, {
    expectedEngineStatusCount: 1,
    expectedRequestCount: 2,
  });

  const divElement = document.querySelector("div");
  const textNode = document.querySelector("div").firstChild;

  info("Mutating the DOM node 5 times");
  for (let i = 1; i <= 5; i++) {
    textNode.nodeValue = `Mutation ${i} on element`;
    await htmlMatches(
      "The changed node gets translated",
      /* html */ `
        <div title="T̅h̅i̅s̅ i̅s̅ a̅ s̅i̅m̅p̅l̅e̅ t̅i̅t̅l̅e̅ t̅r̅a̅n̅s̅l̅a̅t̅i̅o̅n̅ (id:2)">
          M̅u̅t̅a̅t̅i̅o̅n̅ ${i} o̅n̅ e̅l̅e̅m̅e̅n̅t̅ (id:${i + 2})
        </div>
      `,
      document,
      resolveRequests
    );
  }

  assertPortData(
    collectPortData,
    {
      expectedRequestCount: 5,
    },
    "There should be exactly one request for each mutation."
  );

  info("Mutating the DOM node's title attribute 5 times");
  for (let i = 1; i <= 5; i++) {
    divElement.setAttribute("title", `Mutation ${i} on title`);
    await htmlMatches(
      "The changed node gets translated",
      /* html */ `
        <div title="M̅u̅t̅a̅t̅i̅o̅n̅ ${i} o̅n̅ t̅i̅t̅l̅e̅ (id:${i + 7})">
          M̅u̅t̅a̅t̅i̅o̅n̅ 5 o̅n̅ e̅l̅e̅m̅e̅n̅t̅ (id:7)
        </div>
      `,
      document,
      resolveRequests
    );
  }

  assertPortData(
    collectPortData,
    {
      expectedRequestCount: 5,
    },
    "There should be exactly one request for each mutation."
  );

  cleanup();
});

/**
 * Test what happens when an inline element is mutated inside of a block element.
 */
add_task(async function test_inline_elements() {
  const {
    translate,
    htmlMatches,
    cleanup,
    document,
    resolveRequests,
    collectPortData,
  } = await setupMutationsTest(/* html */ `
      <div>
        <span>inline one</span>
        <span title="Title attribute">inline two</span>
        <span>inline three</span>
      </div>
    `);

  translate();

  await htmlMatches(
    "The block element gets translated as one logical unit.",
    /* html */ `
     <div>
       <span>
         i̅n̅l̅i̅n̅e̅ o̅n̅e̅
       </span>
       <span title="T̅i̅t̅l̅e̅ a̅t̅t̅r̅i̅b̅u̅t̅e̅ (id:2)">
         i̅n̅l̅i̅n̅e̅ t̅w̅o̅
       </span>
       <span>
         i̅n̅l̅i̅n̅e̅ t̅h̅r̅e̅e̅
       </span>
       (id:1)
     </div>
    `,
    document,
    resolveRequests
  );

  assertPortData(
    collectPortData,
    {
      expectedEngineStatusCount: 1,
      expectedRequestCount: 2,
    },
    "The whole block is sent as one translation and the title attribute was sent separately"
  );

  info("Mutating the text of span 2");

  /** @type {HTMLSpanElement} */
  const secondSpan = document.querySelectorAll("span")[1];
  secondSpan.innerText =
    "setting the innerText hits the childList mutation type";

  await htmlMatches(
    "The changed node gets translated",
    /* html */ `
      <div>
        <span>
          i̅n̅l̅i̅n̅e̅ o̅n̅e̅
        </span>
        <span title="T̅i̅t̅l̅e̅ a̅t̅t̅r̅i̅b̅u̅t̅e̅ (id:2)">
          s̅e̅t̅t̅i̅n̅g̅ t̅h̅e̅ i̅n̅n̅e̅r̅T̅e̅x̅t̅ h̅i̅t̅s̅ t̅h̅e̅ c̅h̅i̅l̅d̅L̅i̅s̅t̅ m̅u̅t̅a̅t̅i̅o̅n̅ t̅y̅p̅e̅ (id:3)
        </span>
        <span>
          i̅n̅l̅i̅n̅e̅ t̅h̅r̅e̅e̅
        </span>
        (id:1)
      </div>
    `,
    document,
    resolveRequests
  );

  assertPortData(collectPortData, {
    expectedRequestCount: 1,
  });

  secondSpan.firstChild.nodeValue =
    "Change the character data for a specific node";

  await htmlMatches(
    "The changed node gets translated",
    /* html */ `
      <div>
        <span>
          i̅n̅l̅i̅n̅e̅ o̅n̅e̅
        </span>
        <span title="T̅i̅t̅l̅e̅ a̅t̅t̅r̅i̅b̅u̅t̅e̅ (id:2)">
          C̅h̅a̅n̅g̅e̅ t̅h̅e̅ c̅h̅a̅r̅a̅c̅t̅e̅r̅ d̅a̅t̅a̅ f̅o̅r̅ a̅ s̅p̅e̅c̅i̅f̅i̅c̅ n̅o̅d̅e̅ (id:4)
        </span>
        <span>
          i̅n̅l̅i̅n̅e̅ t̅h̅r̅e̅e̅
        </span>
        (id:1)
      </div>
    `,
    document,
    resolveRequests
  );

  assertPortData(collectPortData, {
    expectedRequestCount: 1,
  });

  secondSpan.setAttribute("title", "Mutate the title attribute");

  await htmlMatches(
    "The changed node gets translated",
    /* html */ `
      <div>
        <span>
          i̅n̅l̅i̅n̅e̅ o̅n̅e̅
        </span>
        <span title="M̅u̅t̅a̅t̅e̅ t̅h̅e̅ t̅i̅t̅l̅e̅ a̅t̅t̅r̅i̅b̅u̅t̅e̅ (id:5)">
          C̅h̅a̅n̅g̅e̅ t̅h̅e̅ c̅h̅a̅r̅a̅c̅t̅e̅r̅ d̅a̅t̅a̅ f̅o̅r̅ a̅ s̅p̅e̅c̅i̅f̅i̅c̅ n̅o̅d̅e̅ (id:4)
        </span>
        <span>
          i̅n̅l̅i̅n̅e̅ t̅h̅r̅e̅e̅
        </span>
        (id:1)
      </div>
    `,
    document,
    resolveRequests
  );

  assertPortData(collectPortData, {
    expectedRequestCount: 1,
  });

  cleanup();
});

/**
 * Test the same behavior as `test_inline_elements` but with individual block
 * elements.
 */
add_task(async function test_block_elements() {
  const {
    translate,
    htmlMatches,
    cleanup,
    document,
    resolveRequests,
    collectPortData,
  } = await setupMutationsTest(/* html */ `
      <section>
        <div>block one</div>
        <div title="Title attribute">block two</div>
        <div>block three</div>
      </section>
    `);

  translate();

  await htmlMatches(
    "Each div block gets translated separately",
    /* html */ `
      <section>
        <div>
          b̅l̅o̅c̅k̅ o̅n̅e̅ (id:1)
        </div>
        <div title="T̅i̅t̅l̅e̅ a̅t̅t̅r̅i̅b̅u̅t̅e̅ (id:4)">
          b̅l̅o̅c̅k̅ t̅w̅o̅ (id:2)
        </div>
        <div>
          b̅l̅o̅c̅k̅ t̅h̅r̅e̅e̅ (id:3)
        </div>
      </section>
    `,
    document,
    resolveRequests
  );

  assertPortData(
    collectPortData,
    {
      expectedEngineStatusCount: 1,
      expectedRequestCount: 4,
    },
    "The whole block is translated, including the title attribute."
  );

  info("Mutating the text of div 2");
  /** @type {HTMLSpanElement} */
  const secondDiv = document.querySelectorAll("div")[1];
  secondDiv.innerText =
    "setting the innerText hits the childList mutation type";

  await htmlMatches(
    "The changed node gets translated",
    /* html */ `
      <section>
        <div>
          b̅l̅o̅c̅k̅ o̅n̅e̅ (id:1)
        </div>
        <div title="T̅i̅t̅l̅e̅ a̅t̅t̅r̅i̅b̅u̅t̅e̅ (id:4)">
          s̅e̅t̅t̅i̅n̅g̅ t̅h̅e̅ i̅n̅n̅e̅r̅T̅e̅x̅t̅ h̅i̅t̅s̅ t̅h̅e̅ c̅h̅i̅l̅d̅L̅i̅s̅t̅ m̅u̅t̅a̅t̅i̅o̅n̅ t̅y̅p̅e̅ (id:5)
        </div>
        <div>
          b̅l̅o̅c̅k̅ t̅h̅r̅e̅e̅ (id:3)
        </div>
      </section>
    `,
    document,
    resolveRequests
  );

  assertPortData(collectPortData, {
    expectedRequestCount: 1,
  });

  secondDiv.firstChild.nodeValue =
    "Change the character data for a specific node";

  await htmlMatches(
    "The changed node gets translated",
    /* html */ `
      <section>
        <div>
          b̅l̅o̅c̅k̅ o̅n̅e̅ (id:1)
        </div>
        <div title="T̅i̅t̅l̅e̅ a̅t̅t̅r̅i̅b̅u̅t̅e̅ (id:4)">
          C̅h̅a̅n̅g̅e̅ t̅h̅e̅ c̅h̅a̅r̅a̅c̅t̅e̅r̅ d̅a̅t̅a̅ f̅o̅r̅ a̅ s̅p̅e̅c̅i̅f̅i̅c̅ n̅o̅d̅e̅ (id:6)
        </div>
        <div>
          b̅l̅o̅c̅k̅ t̅h̅r̅e̅e̅ (id:3)
        </div>
      </section>
    `,
    document,
    resolveRequests
  );

  assertPortData(collectPortData, {
    expectedRequestCount: 1,
  });

  secondDiv.setAttribute("title", "Mutate the title attribute");

  await htmlMatches(
    "The changed node gets translated",
    /* html */ `
      <section>
        <div>
          b̅l̅o̅c̅k̅ o̅n̅e̅ (id:1)
        </div>
        <div title="M̅u̅t̅a̅t̅e̅ t̅h̅e̅ t̅i̅t̅l̅e̅ a̅t̅t̅r̅i̅b̅u̅t̅e̅ (id:7)">
          C̅h̅a̅n̅g̅e̅ t̅h̅e̅ c̅h̅a̅r̅a̅c̅t̅e̅r̅ d̅a̅t̅a̅ f̅o̅r̅ a̅ s̅p̅e̅c̅i̅f̅i̅c̅ n̅o̅d̅e̅ (id:6)
        </div>
        <div>
          b̅l̅o̅c̅k̅ t̅h̅r̅e̅e̅ (id:3)
        </div>
      </section>
    `,
    document,
    resolveRequests
  );

  assertPortData(collectPortData, {
    expectedRequestCount: 1,
  });

  cleanup();
});

add_task(async function test_removing_elements() {
  const {
    translate,
    htmlMatches,
    cleanup,
    document,
    resolveRequests,
    collectPortData,
  } = await setupMutationsTest(/* html */ `
      <section>
        <div>block one</div>
        <div title="Title attribute">block two</div>
        <div>block three</div>
      </section>
    `);

  translate();

  info("Removing two divs");
  const elements = document.querySelectorAll("div");
  elements[0].remove();
  elements[1].remove();

  await htmlMatches(
    "Only one element is translated",
    /* html */ `
      <section>
        <div>
          b̅l̅o̅c̅k̅ t̅h̅r̅e̅e̅ (id:{{ [1-4] }})
        </div>
      </section>
    `,
    document,
    resolveRequests
  );

  assertPortData(collectPortData, {
    expectedEngineStatusCount: 1,
    expectedRequestCount: 1,
  });

  cleanup();
});

add_task(async function test_mixed_block_inline() {
  const {
    translate,
    htmlMatches,
    cleanup,
    document,
    resolveRequests,
    collectPortData,
  } = await setupMutationsTest(/* html */ `
      <section>
        first text node
        <div>block one</div>
        second text node <span>with inline element</span>
        <div>block two</div>
        third text node
        <div>block three</div>
      </section>
    `);

  translate();

  await htmlMatches(
    "The algorithm to chop of the nodes runs.",
    /* html */ `
      <section>
        f̅i̅r̅s̅t̅ t̅e̅x̅t̅ n̅o̅d̅e̅ (id:1)
        <div>
          b̅l̅o̅c̅k̅ o̅n̅e̅ (id:4)
        </div>
        s̅e̅c̅o̅n̅d̅ t̅e̅x̅t̅ n̅o̅d̅e̅ (id:2)
        <span>
          w̅i̅t̅h̅ i̅n̅l̅i̅n̅e̅ e̅l̅e̅m̅e̅n̅t̅ (id:5)
        </span>
        <div>
          b̅l̅o̅c̅k̅ t̅w̅o̅ (id:6)
        </div>
        t̅h̅i̅r̅d̅ t̅e̅x̅t̅ n̅o̅d̅e̅ (id:3)
        <div>
          b̅l̅o̅c̅k̅ t̅h̅r̅e̅e̅ (id:7)
        </div>
      </section>
    `,
    document,
    resolveRequests
  );

  assertPortData(collectPortData, {
    expectedEngineStatusCount: 1,
    expectedRequestCount: 7,
  });

  info("Mutating the <section>'s text nodes");
  const section = document.querySelector("section");
  for (let i = 0; i < section.childNodes.length; i++) {
    const node = section.childNodes[i];
    if (node.nodeType === Node.TEXT_NODE && node.nodeValue.trim()) {
      node.nodeValue = `Mutating ${i} text node`;
    }
  }

  await htmlMatches(
    "",
    /* html */ `
      <section>
        M̅u̅t̅a̅t̅i̅n̅g̅ 0 t̅e̅x̅t̅ n̅o̅d̅e̅ (id:8)
        <div>
          b̅l̅o̅c̅k̅ o̅n̅e̅ (id:4)
        </div>
        M̅u̅t̅a̅t̅i̅n̅g̅ 2 t̅e̅x̅t̅ n̅o̅d̅e̅ (id:9)
        <span>
          w̅i̅t̅h̅ i̅n̅l̅i̅n̅e̅ e̅l̅e̅m̅e̅n̅t̅ (id:5)
        </span>
        <div>
          b̅l̅o̅c̅k̅ t̅w̅o̅ (id:6)
        </div>
        M̅u̅t̅a̅t̅i̅n̅g̅ 6 t̅e̅x̅t̅ n̅o̅d̅e̅ (id:10)
        <div>
          b̅l̅o̅c̅k̅ t̅h̅r̅e̅e̅ (id:7)
        </div>
      </section>
    `,
    document,
    resolveRequests
  );

  assertPortData(collectPortData, {
    expectedRequestCount: 3,
  });

  cleanup();
});

add_task(async function test_block_within_inline() {
  const {
    translate,
    htmlMatches,
    cleanup,
    document,
    resolveRequests,
    collectPortData,
  } = await setupMutationsTest(/* html */ `
    <span>
      outer span text before div
      <div>
        inner div text 1
        <span>
          inner span text 1
          <span>
            innermost span text
          </span>
          inner span text 2
        </span>
        inner div text 2
      </div>
      outer span text after div
    </span>
  `);

  translate();

  await htmlMatches(
    "Nested block/inline structure is translated correctly.",
    /* html */ `
      <span>
        o̅u̅t̅e̅r̅ s̅p̅a̅n̅ t̅e̅x̅t̅ b̅e̅f̅o̅r̅e̅ d̅i̅v̅ (id:1)
        <div>
          i̅n̅n̅e̅r̅ d̅i̅v̅ t̅e̅x̅t̅ 1
          <span>
            i̅n̅n̅e̅r̅ s̅p̅a̅n̅ t̅e̅x̅t̅ 1
            <span>
              i̅n̅n̅e̅r̅m̅o̅s̅t̅ s̅p̅a̅n̅ t̅e̅x̅t̅
            </span>
            i̅n̅n̅e̅r̅ s̅p̅a̅n̅ t̅e̅x̅t̅ 2
          </span>
          i̅n̅n̅e̅r̅ d̅i̅v̅ t̅e̅x̅t̅ 2 (id:3)
        </div>
        o̅u̅t̅e̅r̅ s̅p̅a̅n̅ t̅e̅x̅t̅ a̅f̅t̅e̅r̅ d̅i̅v̅ (id:2)
      </span>
    `,
    document,
    resolveRequests
  );

  assertPortData(collectPortData, {
    expectedEngineStatusCount: 1,
    expectedRequestCount: 3,
  });

  info("Mutating the two outer span text nodes");
  const outerSpan = document.querySelector("span");
  for (const node of outerSpan.childNodes) {
    if (node.nodeType === Node.TEXT_NODE && node.nodeValue.trim()) {
      node.nodeValue = "Mutated outer span text";
    }
  }

  info("Mutating the two inner span text nodes");
  const innerSpan = outerSpan.querySelector("div span");
  for (const node of innerSpan.childNodes) {
    if (node.nodeType === Node.TEXT_NODE && node.nodeValue.trim()) {
      node.nodeValue = "Mutated inner span text";
    }
  }

  await htmlMatches(
    "Inline mutations are re-translated while unchanged blocks stay cached.",
    /* html */ `
      <span>
        M̅u̅t̅a̅t̅e̅d̅ o̅u̅t̅e̅r̅ s̅p̅a̅n̅ t̅e̅x̅t̅ (id:4)
        <div>
          i̅n̅n̅e̅r̅ d̅i̅v̅ t̅e̅x̅t̅ 1
          <span>
            M̅u̅t̅a̅t̅e̅d̅ i̅n̅n̅e̅r̅ s̅p̅a̅n̅ t̅e̅x̅t̅ (id:7)
            <span>
              i̅n̅n̅e̅r̅m̅o̅s̅t̅ s̅p̅a̅n̅ t̅e̅x̅t̅
            </span>
            M̅u̅t̅a̅t̅e̅d̅ i̅n̅n̅e̅r̅ s̅p̅a̅n̅ t̅e̅x̅t̅ (id:7)
          </span>
          i̅n̅n̅e̅r̅ d̅i̅v̅ t̅e̅x̅t̅ 2 (id:3)
        </div>
        M̅u̅t̅a̅t̅e̅d̅ o̅u̅t̅e̅r̅ s̅p̅a̅n̅ t̅e̅x̅t̅ (id:4)
      </span>
    `,
    document,
    resolveRequests
  );

  assertPortData(collectPortData, {
    expectedCachedCount: 2,
    expectedRequestCount: 2,
  });

  info("Mutating the two div text nodes");
  const innerDiv = outerSpan.querySelector("div");
  for (const node of innerDiv.childNodes) {
    if (node.nodeType === Node.TEXT_NODE && node.nodeValue.trim()) {
      node.nodeValue = "Mutated inner div text";
    }
  }

  info("Mutating the innermost span text node");
  const innermostSpan = innerSpan.querySelector("span span");
  innermostSpan.textContent = "Mutated innermost inline text";

  await htmlMatches(
    "Block mutations are re-translated while cached inline nodes remain.",
    /* html */ `
      <span>
        M̅u̅t̅a̅t̅e̅d̅ o̅u̅t̅e̅r̅ s̅p̅a̅n̅ t̅e̅x̅t̅ (id:4)
        <div>
          M̅u̅t̅a̅t̅e̅d̅ i̅n̅n̅e̅r̅ d̅i̅v̅ t̅e̅x̅t̅ (id:8)
          <span>
            M̅u̅t̅a̅t̅e̅d̅ i̅n̅n̅e̅r̅ s̅p̅a̅n̅ t̅e̅x̅t̅ (id:7)
            <span>
              M̅u̅t̅a̅t̅e̅d̅ i̅n̅n̅e̅r̅m̅o̅s̅t̅ i̅n̅l̅i̅n̅e̅ t̅e̅x̅t̅ (id:10)
            </span>
            M̅u̅t̅a̅t̅e̅d̅ i̅n̅n̅e̅r̅ s̅p̅a̅n̅ t̅e̅x̅t̅ (id:7)
          </span>
          M̅u̅t̅a̅t̅e̅d̅ i̅n̅n̅e̅r̅ d̅i̅v̅ t̅e̅x̅t̅ (id:8)
        </div>
        M̅u̅t̅a̅t̅e̅d̅ o̅u̅t̅e̅r̅ s̅p̅a̅n̅ t̅e̅x̅t̅ (id:4)
      </span>
    `,
    document,
    resolveRequests
  );

  assertPortData(collectPortData, {
    expectedRequestCount: 2,
    expectedCachedCount: 1,
  });

  cleanup();
});

add_task(async function test_appending_element() {
  const {
    translate,
    htmlMatches,
    cleanup,
    document,
    resolveRequests,
    collectPortData,
  } = await setupMutationsTest(/* html */ `
      <section>
        <div>block one</div>
        <div>block two</div>
        <div>block three</div>
      </section>
    `);

  translate();

  await htmlMatches(
    "The blocks are translated",
    /* html */ `
      <section>
        <div>
          b̅l̅o̅c̅k̅ o̅n̅e̅ (id:1)
        </div>
        <div>
          b̅l̅o̅c̅k̅ t̅w̅o̅ (id:2)
        </div>
        <div>
          b̅l̅o̅c̅k̅ t̅h̅r̅e̅e̅ (id:3)
        </div>
      </section>
    `,
    document,
    resolveRequests
  );

  assertPortData(collectPortData, {
    expectedEngineStatusCount: 1,
    expectedRequestCount: 3,
  });

  const fragment = document.createDocumentFragment();
  const subDiv1 = document.createElement("div");
  const subDiv2 = document.createElement("div");
  subDiv1.innerHTML = "Adding multiple elements at once";
  subDiv2.innerHTML = "<div>It even has <span>nested</span> elements</div>";
  fragment.append(subDiv1);
  fragment.append(subDiv2);

  const section = document.querySelector("section");
  const secondDiv = document.querySelectorAll("div")[1];
  section.insertBefore(fragment, secondDiv);

  await htmlMatches(
    "Multiple elements are inserted at once",
    /* html */ `
      <section>
        <div>
          b̅l̅o̅c̅k̅ o̅n̅e̅ (id:1)
        </div>
        <div>
          A̅d̅d̅i̅n̅g̅ m̅u̅l̅t̅i̅p̅l̅e̅ e̅l̅e̅m̅e̅n̅t̅s̅ a̅t̅ o̅n̅c̅e̅ (id:4)
        </div>
        <div>
          <div>
            I̅t̅ e̅v̅e̅n̅ h̅a̅s̅
            <span>
              n̅e̅s̅t̅e̅d̅
            </span>
            e̅l̅e̅m̅e̅n̅t̅s̅ (id:5)
          </div>
        </div>
        <div>
          b̅l̅o̅c̅k̅ t̅w̅o̅ (id:2)
        </div>
        <div>
          b̅l̅o̅c̅k̅ t̅h̅r̅e̅e̅ (id:3)
        </div>
      </section>
    `,
    document,
    resolveRequests
  );

  assertPortData(collectPortData, {
    expectedRequestCount: 2,
  });

  cleanup();
});

add_task(async function test_mutating_comments() {
  const {
    translate,
    htmlMatches,
    cleanup,
    document,
    resolveRequests,
    collectPortData,
  } = await setupMutationsTest(/* html */ `
      <section>
        <!-- this is a comment -->
        <div>block one</div>
      </section>
    `);

  translate();

  await htmlMatches(
    "The blocks are translated",
    /* html */ `
      <section>
        <!-- this is a comment -->
        <div>
          b̅l̅o̅c̅k̅ o̅n̅e̅ (id:1)
        </div>
      </section>
    `,
    document,
    resolveRequests
  );

  assertPortData(collectPortData, {
    expectedEngineStatusCount: 1,
    expectedRequestCount: 1,
  });

  // Mutate the comment's contents.
  const section = document.querySelector("section");
  const commentNode = [...section.childNodes].find(
    node => node.nodeType === Node.COMMENT_NODE
  );
  commentNode.nodeValue = "Change the comment";

  await htmlMatches(
    "The comment is not translated in a mutation",
    /* html */ `
      <section>
        <!--Change the comment-->
        <div>
          b̅l̅o̅c̅k̅ o̅n̅e̅ (id:1)
        </div>
      </section>
    `
  );

  assertPortData(collectPortData, {}, "No data is sent over the port.");

  cleanup();
});

/**
 * This test case tests the behavior of **cached translations** for both
 * attributes and text content. After translating some content, we re-insert
 * the *original* source-language text. Because the engine has already cached
 * translations for those strings, they should be re-translated from the cache
 * without sending any new translation requests.
 */
add_task(async function test_cache_within_document() {
  const {
    translate,
    htmlMatches,
    cleanup,
    document,
    resolveRequests,
    collectPortData,
  } = await setupMutationsTest(/* html */ `
       <div title="Title attribute 1">This is block 1</div>
       <div title="Title attribute 2">This is block 2</div>
       <div title="Title attribute 3">This is block 3</div>
       <div title="Title attribute 4">This is block 4</div>
     `);

  // Capture the original source-language strings so we can restore them later.
  const divs = Array.from(document.querySelectorAll("div"));
  const originalTitles = divs.map(div => div.getAttribute("title"));
  const originalTexts = divs.map(div => div.textContent);

  translate();

  await htmlMatches(
    "Each div block gets translated separately",
    /* html */ `
       <div title="T̅i̅t̅l̅e̅ a̅t̅t̅r̅i̅b̅u̅t̅e̅ 1 (id:5)">T̅h̅i̅s̅ i̅s̅ b̅l̅o̅c̅k̅ 1 (id:1)</div>
       <div title="T̅i̅t̅l̅e̅ a̅t̅t̅r̅i̅b̅u̅t̅e̅ 2 (id:6)">T̅h̅i̅s̅ i̅s̅ b̅l̅o̅c̅k̅ 2 (id:2)</div>
       <div title="T̅i̅t̅l̅e̅ a̅t̅t̅r̅i̅b̅u̅t̅e̅ 3 (id:7)">T̅h̅i̅s̅ i̅s̅ b̅l̅o̅c̅k̅ 3 (id:3)</div>
       <div title="T̅i̅t̅l̅e̅ a̅t̅t̅r̅i̅b̅u̅t̅e̅ 4 (id:8)">T̅h̅i̅s̅ i̅s̅ b̅l̅o̅c̅k̅ 4 (id:4)</div>
     `,
    document,
    resolveRequests
  );

  assertPortData(collectPortData, {
    expectedEngineStatusCount: 1,
    expectedRequestCount: 8,
  });

  info(
    "Restoring all div titles and text content back to their original source language"
  );
  divs.forEach((div, idx) => {
    div.setAttribute("title", originalTitles[idx]);
    div.textContent = originalTexts[idx];
  });

  // Add brand-new, uncached content and attribute.
  const newDiv = document.createElement("div");
  newDiv.setAttribute("title", "Title attribute 5");
  newDiv.textContent = "This is block 5";
  document.body.appendChild(newDiv);

  await htmlMatches(
    "Restored blocks are served from cache and the new block is translated with new IDs",
    /* html */ `
       <div title="T̅i̅t̅l̅e̅ a̅t̅t̅r̅i̅b̅u̅t̅e̅ 1 (id:5)">T̅h̅i̅s̅ i̅s̅ b̅l̅o̅c̅k̅ 1 (id:1)</div>
       <div title="T̅i̅t̅l̅e̅ a̅t̅t̅r̅i̅b̅u̅t̅e̅ 2 (id:6)">T̅h̅i̅s̅ i̅s̅ b̅l̅o̅c̅k̅ 2 (id:2)</div>
       <div title="T̅i̅t̅l̅e̅ a̅t̅t̅r̅i̅b̅u̅t̅e̅ 3 (id:7)">T̅h̅i̅s̅ i̅s̅ b̅l̅o̅c̅k̅ 3 (id:3)</div>
       <div title="T̅i̅t̅l̅e̅ a̅t̅t̅r̅i̅b̅u̅t̅e̅ 4 (id:8)">T̅h̅i̅s̅ i̅s̅ b̅l̅o̅c̅k̅ 4 (id:4)</div>
       <div title="T̅i̅t̅l̅e̅ a̅t̅t̅r̅i̅b̅u̅t̅e̅ 5 (id:14)">T̅h̅i̅s̅ i̅s̅ b̅l̅o̅c̅k̅ 5 (id:13)</div>
     `,
    document,
    resolveRequests
  );

  assertPortData(collectPortData, {
    expectedRequestCount: 2,
  });

  cleanup();
});

/**
 * This test case tests the behavior of translations cacheing, specifically
 * after all nodes have been submitted to the scheduler. In this scenario
 * the scheduler may realize that it has alredy translated this exact text
 * and skip a new CPU-bound translation request, resulting in the same id
 * showing in the result mock-translated text.
 */
add_task(async function test_cache_within_scheduler() {
  const {
    translate,
    htmlMatches,
    cleanup,
    document,
    resolveRequests,
    collectPortData,
  } = await setupMutationsTest(/* html */ `
      <div title="Title attribute 1">This is block 1</div>
      <div title="Title attribute 2">This is block 2</div>
      <div title="Title attribute 3">This is block 3</div>
      <div title="Title attribute 4">This is block 4</div>
      <div title="Title attribute 5">This is block 5</div>

      <div title="Title attribute 1">This is block 1</div>
      <div title="Title attribute 2">This is block 2</div>
      <div title="Title attribute 3">This is block 3</div>
      <div title="Title attribute 4">This is block 4</div>
      <div title="Title attribute 5">This is block 5</div>

      <div title="Title attribute 1">This is block 1</div>
      <div title="Title attribute 2">This is block 2</div>
      <div title="Title attribute 3">This is block 3</div>
      <div title="Title attribute 4">This is block 4</div>
      <div title="Title attribute 5">This is block 5</div>
    `);

  translate();

  await htmlMatches(
    "Each div block gets translated separately",
    /* html */ `
      <div title="T̅i̅t̅l̅e̅ a̅t̅t̅r̅i̅b̅u̅t̅e̅ 1 (id:26)">T̅h̅i̅s̅ i̅s̅ b̅l̅o̅c̅k̅ 1 (id:11)</div>
      <div title="T̅i̅t̅l̅e̅ a̅t̅t̅r̅i̅b̅u̅t̅e̅ 2 (id:27)">T̅h̅i̅s̅ i̅s̅ b̅l̅o̅c̅k̅ 2 (id:12)</div>
      <div title="T̅i̅t̅l̅e̅ a̅t̅t̅r̅i̅b̅u̅t̅e̅ 3 (id:28)">T̅h̅i̅s̅ i̅s̅ b̅l̅o̅c̅k̅ 3 (id:13)</div>
      <div title="T̅i̅t̅l̅e̅ a̅t̅t̅r̅i̅b̅u̅t̅e̅ 4 (id:29)">T̅h̅i̅s̅ i̅s̅ b̅l̅o̅c̅k̅ 4 (id:14)</div>
      <div title="T̅i̅t̅l̅e̅ a̅t̅t̅r̅i̅b̅u̅t̅e̅ 5 (id:30)">T̅h̅i̅s̅ i̅s̅ b̅l̅o̅c̅k̅ 5 (id:15)</div>

      <div title="T̅i̅t̅l̅e̅ a̅t̅t̅r̅i̅b̅u̅t̅e̅ 1 (id:26)">T̅h̅i̅s̅ i̅s̅ b̅l̅o̅c̅k̅ 1 (id:11)</div>
      <div title="T̅i̅t̅l̅e̅ a̅t̅t̅r̅i̅b̅u̅t̅e̅ 2 (id:27)">T̅h̅i̅s̅ i̅s̅ b̅l̅o̅c̅k̅ 2 (id:12)</div>
      <div title="T̅i̅t̅l̅e̅ a̅t̅t̅r̅i̅b̅u̅t̅e̅ 3 (id:28)">T̅h̅i̅s̅ i̅s̅ b̅l̅o̅c̅k̅ 3 (id:13)</div>
      <div title="T̅i̅t̅l̅e̅ a̅t̅t̅r̅i̅b̅u̅t̅e̅ 4 (id:29)">T̅h̅i̅s̅ i̅s̅ b̅l̅o̅c̅k̅ 4 (id:14)</div>
      <div title="T̅i̅t̅l̅e̅ a̅t̅t̅r̅i̅b̅u̅t̅e̅ 5 (id:30)">T̅h̅i̅s̅ i̅s̅ b̅l̅o̅c̅k̅ 5 (id:15)</div>

      <div title="T̅i̅t̅l̅e̅ a̅t̅t̅r̅i̅b̅u̅t̅e̅ 1 (id:26)">T̅h̅i̅s̅ i̅s̅ b̅l̅o̅c̅k̅ 1 (id:11)</div>
      <div title="T̅i̅t̅l̅e̅ a̅t̅t̅r̅i̅b̅u̅t̅e̅ 2 (id:27)">T̅h̅i̅s̅ i̅s̅ b̅l̅o̅c̅k̅ 2 (id:12)</div>
      <div title="T̅i̅t̅l̅e̅ a̅t̅t̅r̅i̅b̅u̅t̅e̅ 3 (id:28)">T̅h̅i̅s̅ i̅s̅ b̅l̅o̅c̅k̅ 3 (id:13)</div>
      <div title="T̅i̅t̅l̅e̅ a̅t̅t̅r̅i̅b̅u̅t̅e̅ 4 (id:29)">T̅h̅i̅s̅ i̅s̅ b̅l̅o̅c̅k̅ 4 (id:14)</div>
      <div title="T̅i̅t̅l̅e̅ a̅t̅t̅r̅i̅b̅u̅t̅e̅ 5 (id:30)">T̅h̅i̅s̅ i̅s̅ b̅l̅o̅c̅k̅ 5 (id:15)</div>
    `,
    document,
    resolveRequests
  );

  assertPortData(collectPortData, {
    expectedEngineStatusCount: 1,
    expectedRequestCount: 10,
    expectedCachedCount: 20,
  });

  cleanup();
});

/**
 * This test case tests the behavior of passthrough translations
 * both for attributes and text content, where a node is mutated
 * with text that we know is already in the target language because
 * it is hot in the cache. In such a case, it should not be re-translated.
 */
add_task(async function test_passthrough_within_document() {
  const {
    translate,
    htmlMatches,
    cleanup,
    document,
    resolveRequests,
    collectPortData,
  } = await setupMutationsTest(/* html */ `
       <div title="Title attribute 1">This is block 1</div>
       <div title="Title attribute 2">This is block 2</div>
       <div title="Title attribute 3">This is block 3</div>
       <div title="Title attribute 4">This is block 4</div>
     `);

  translate();

  await htmlMatches(
    "Each div block gets translated separately",
    /* html */ `
       <div title="T̅i̅t̅l̅e̅ a̅t̅t̅r̅i̅b̅u̅t̅e̅ 1 (id:5)">T̅h̅i̅s̅ i̅s̅ b̅l̅o̅c̅k̅ 1 (id:1)</div>
       <div title="T̅i̅t̅l̅e̅ a̅t̅t̅r̅i̅b̅u̅t̅e̅ 2 (id:6)">T̅h̅i̅s̅ i̅s̅ b̅l̅o̅c̅k̅ 2 (id:2)</div>
       <div title="T̅i̅t̅l̅e̅ a̅t̅t̅r̅i̅b̅u̅t̅e̅ 3 (id:7)">T̅h̅i̅s̅ i̅s̅ b̅l̅o̅c̅k̅ 3 (id:3)</div>
       <div title="T̅i̅t̅l̅e̅ a̅t̅t̅r̅i̅b̅u̅t̅e̅ 4 (id:8)">T̅h̅i̅s̅ i̅s̅ b̅l̅o̅c̅k̅ 4 (id:4)</div>
     `,
    document,
    resolveRequests
  );

  assertPortData(collectPortData, {
    expectedEngineStatusCount: 1,
    expectedRequestCount: 8,
  });

  info("Circularly rotating all divs and titles");
  const divs = Array.from(document.querySelectorAll("div"));
  const titles = divs.map(div => div.getAttribute("title"));
  const texts = divs.map(div => div.textContent);

  divs.forEach((div, idx) => {
    const nextIdx = (idx + 1) % divs.length;
    div.setAttribute("title", titles[nextIdx]);
    div.textContent = texts[nextIdx];
  });

  // Add brand-new, uncached content and attribute.
  const newDiv = document.createElement("div");
  newDiv.setAttribute("title", "Title attribute 5");
  newDiv.textContent = "This is block 5";
  document.body.appendChild(newDiv);

  await htmlMatches(
    "Rotated blocks are still translated from the cache and the new block is translated with new IDs",
    /* html */ `
       <div title="T̅i̅t̅l̅e̅ a̅t̅t̅r̅i̅b̅u̅t̅e̅ 2 (id:6)">T̅h̅i̅s̅ i̅s̅ b̅l̅o̅c̅k̅ 2 (id:2)</div>
       <div title="T̅i̅t̅l̅e̅ a̅t̅t̅r̅i̅b̅u̅t̅e̅ 3 (id:7)">T̅h̅i̅s̅ i̅s̅ b̅l̅o̅c̅k̅ 3 (id:3)</div>
       <div title="T̅i̅t̅l̅e̅ a̅t̅t̅r̅i̅b̅u̅t̅e̅ 4 (id:8)">T̅h̅i̅s̅ i̅s̅ b̅l̅o̅c̅k̅ 4 (id:4)</div>
       <div title="T̅i̅t̅l̅e̅ a̅t̅t̅r̅i̅b̅u̅t̅e̅ 1 (id:5)">T̅h̅i̅s̅ i̅s̅ b̅l̅o̅c̅k̅ 1 (id:1)</div>
       <div title="T̅i̅t̅l̅e̅ a̅t̅t̅r̅i̅b̅u̅t̅e̅ 5 (id:14)">T̅h̅i̅s̅ i̅s̅ b̅l̅o̅c̅k̅ 5 (id:13)</div>
     `,
    document,
    resolveRequests
  );

  assertPortData(collectPortData, {
    expectedRequestCount: 2,
  });

  cleanup();
});

add_task(async function test_passthrough_within_scheduler() {
  const {
    translate,
    htmlMatches,
    cleanup,
    document,
    resolveRequests,
    collectPortData,
  } = await setupMutationsTest(/* html */ `
      <div title="T̅i̅t̅l̅e̅ a̅t̅t̅r̅i̅b̅u̅t̅e̅ 1 (id:26)">T̅h̅i̅s̅ i̅s̅ b̅l̅o̅c̅k̅ 1 (id:11)</div>
      <div title="T̅i̅t̅l̅e̅ a̅t̅t̅r̅i̅b̅u̅t̅e̅ 2 (id:27)">T̅h̅i̅s̅ i̅s̅ b̅l̅o̅c̅k̅ 2 (id:12)</div>
      <div title="T̅i̅t̅l̅e̅ a̅t̅t̅r̅i̅b̅u̅t̅e̅ 3 (id:28)">T̅h̅i̅s̅ i̅s̅ b̅l̅o̅c̅k̅ 3 (id:13)</div>
      <div title="T̅i̅t̅l̅e̅ a̅t̅t̅r̅i̅b̅u̅t̅e̅ 4 (id:29)">T̅h̅i̅s̅ i̅s̅ b̅l̅o̅c̅k̅ 4 (id:14)</div>
      <div title="T̅i̅t̅l̅e̅ a̅t̅t̅r̅i̅b̅u̅t̅e̅ 5 (id:30)">T̅h̅i̅s̅ i̅s̅ b̅l̅o̅c̅k̅ 5 (id:15)</div>

      <div title="T̅i̅t̅l̅e̅ a̅t̅t̅r̅i̅b̅u̅t̅e̅ 1 (id:26)">T̅h̅i̅s̅ i̅s̅ b̅l̅o̅c̅k̅ 1 (id:11)</div>
      <div title="T̅i̅t̅l̅e̅ a̅t̅t̅r̅i̅b̅u̅t̅e̅ 2 (id:27)">T̅h̅i̅s̅ i̅s̅ b̅l̅o̅c̅k̅ 2 (id:12)</div>
      <div title="T̅i̅t̅l̅e̅ a̅t̅t̅r̅i̅b̅u̅t̅e̅ 3 (id:28)">T̅h̅i̅s̅ i̅s̅ b̅l̅o̅c̅k̅ 3 (id:13)</div>
      <div title="T̅i̅t̅l̅e̅ a̅t̅t̅r̅i̅b̅u̅t̅e̅ 4 (id:29)">T̅h̅i̅s̅ i̅s̅ b̅l̅o̅c̅k̅ 4 (id:14)</div>
      <div title="T̅i̅t̅l̅e̅ a̅t̅t̅r̅i̅b̅u̅t̅e̅ 5 (id:30)">T̅h̅i̅s̅ i̅s̅ b̅l̅o̅c̅k̅ 5 (id:15)</div>

      <div title="Title attribute 1">This is block 1</div>
      <div title="Title attribute 2">This is block 2</div>
      <div title="Title attribute 3">This is block 3</div>
      <div title="Title attribute 4">This is block 4</div>
      <div title="Title attribute 5">This is block 5</div>
    `);

  translate();

  await htmlMatches(
    "Each div block gets translated separately",
    /* html */ `
      <div title="T̅i̅t̅l̅e̅ a̅t̅t̅r̅i̅b̅u̅t̅e̅ 1 (id:26)">T̅h̅i̅s̅ i̅s̅ b̅l̅o̅c̅k̅ 1 (id:11)</div>
      <div title="T̅i̅t̅l̅e̅ a̅t̅t̅r̅i̅b̅u̅t̅e̅ 2 (id:27)">T̅h̅i̅s̅ i̅s̅ b̅l̅o̅c̅k̅ 2 (id:12)</div>
      <div title="T̅i̅t̅l̅e̅ a̅t̅t̅r̅i̅b̅u̅t̅e̅ 3 (id:28)">T̅h̅i̅s̅ i̅s̅ b̅l̅o̅c̅k̅ 3 (id:13)</div>
      <div title="T̅i̅t̅l̅e̅ a̅t̅t̅r̅i̅b̅u̅t̅e̅ 4 (id:29)">T̅h̅i̅s̅ i̅s̅ b̅l̅o̅c̅k̅ 4 (id:14)</div>
      <div title="T̅i̅t̅l̅e̅ a̅t̅t̅r̅i̅b̅u̅t̅e̅ 5 (id:30)">T̅h̅i̅s̅ i̅s̅ b̅l̅o̅c̅k̅ 5 (id:15)</div>

      <div title="T̅i̅t̅l̅e̅ a̅t̅t̅r̅i̅b̅u̅t̅e̅ 1 (id:26)">T̅h̅i̅s̅ i̅s̅ b̅l̅o̅c̅k̅ 1 (id:11)</div>
      <div title="T̅i̅t̅l̅e̅ a̅t̅t̅r̅i̅b̅u̅t̅e̅ 2 (id:27)">T̅h̅i̅s̅ i̅s̅ b̅l̅o̅c̅k̅ 2 (id:12)</div>
      <div title="T̅i̅t̅l̅e̅ a̅t̅t̅r̅i̅b̅u̅t̅e̅ 3 (id:28)">T̅h̅i̅s̅ i̅s̅ b̅l̅o̅c̅k̅ 3 (id:13)</div>
      <div title="T̅i̅t̅l̅e̅ a̅t̅t̅r̅i̅b̅u̅t̅e̅ 4 (id:29)">T̅h̅i̅s̅ i̅s̅ b̅l̅o̅c̅k̅ 4 (id:14)</div>
      <div title="T̅i̅t̅l̅e̅ a̅t̅t̅r̅i̅b̅u̅t̅e̅ 5 (id:30)">T̅h̅i̅s̅ i̅s̅ b̅l̅o̅c̅k̅ 5 (id:15)</div>

      <div title="T̅i̅t̅l̅e̅ a̅t̅t̅r̅i̅b̅u̅t̅e̅ 1 (id:26)">T̅h̅i̅s̅ i̅s̅ b̅l̅o̅c̅k̅ 1 (id:11)</div>
      <div title="T̅i̅t̅l̅e̅ a̅t̅t̅r̅i̅b̅u̅t̅e̅ 2 (id:27)">T̅h̅i̅s̅ i̅s̅ b̅l̅o̅c̅k̅ 2 (id:12)</div>
      <div title="T̅i̅t̅l̅e̅ a̅t̅t̅r̅i̅b̅u̅t̅e̅ 3 (id:28)">T̅h̅i̅s̅ i̅s̅ b̅l̅o̅c̅k̅ 3 (id:13)</div>
      <div title="T̅i̅t̅l̅e̅ a̅t̅t̅r̅i̅b̅u̅t̅e̅ 4 (id:29)">T̅h̅i̅s̅ i̅s̅ b̅l̅o̅c̅k̅ 4 (id:14)</div>
      <div title="T̅i̅t̅l̅e̅ a̅t̅t̅r̅i̅b̅u̅t̅e̅ 5 (id:30)">T̅h̅i̅s̅ i̅s̅ b̅l̅o̅c̅k̅ 5 (id:15)</div>
    `,
    document,
    resolveRequests
  );

  assertPortData(collectPortData, {
    expectedEngineStatusCount: 1,
    expectedRequestCount: 10,
    expectedPassthroughCount: 20,
  });

  cleanup();
});
