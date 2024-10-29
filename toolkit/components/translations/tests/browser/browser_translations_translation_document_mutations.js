/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * @param {string} html
 */
async function setupMutationsTest(html) {
  const { mockedTranslatorPort, resolveRequests } =
    createControlledTranslatorPort();
  const translationsDoc = await createTranslationsDoc(html, {
    mockedTranslatorPort,
  });
  return { resolveRequests, ...translationsDoc };
}

/**
 * Test basic mutations discarding behavior, where a page is trans
 */
add_task(async function test_discarding() {
  const { translate, htmlMatches, cleanup, document, resolveRequests } =
    await setupMutationsTest(/* html */ `
      <div>
        This is a simple translation.
      </div>
    `);

  translate();
  let translationsCount = await resolveRequests();
  is(translationsCount, 1, "There was just the initial translation.");

  await htmlMatches(
    "It translates.",
    /* html */ `
      <div>
        T̅h̅i̅s̅ i̅s̅ a̅ s̅i̅m̅p̅l̅e̅ t̅r̅a̅n̅s̅l̅a̅t̅i̅o̅n̅. (id:1)
      </div>
    `
  );

  info("Mutating the DOM node 5 times");
  const textNode = document.querySelector("div").firstChild;
  for (let i = 1; i <= 5; i++) {
    textNode.nodeValue = `Mutation ${i} on element`;
  }

  await doubleRaf(document);
  translationsCount = await resolveRequests();
  is(
    translationsCount,
    1,
    "The 5 mutations are batched, and only 1 is sent for translation."
  );

  await htmlMatches(
    "The changed node gets translated",
    /* html */ `
      <div>
        M̅u̅t̅a̅t̅i̅o̅n̅ 5 o̅n̅ e̅l̅e̅m̅e̅n̅t̅ (id:2)
      </div>
    `
  );
  cleanup();
});

/**
 * Test the case where mutations happens before the initial translation.
 */
add_task(async function test_before_initial_translation() {
  const { translate, htmlMatches, cleanup, document, resolveRequests } =
    await setupMutationsTest(/* html */ `
      <div>
        This is a simple translation.
      </div>
    `);

  translate();
  // Unlike `test_discarding`, do NOT resolve translations here.

  info("Mutating the DOM node 5 times");
  const textNode = document.querySelector("div").firstChild;
  for (let i = 1; i <= 5; i++) {
    textNode.nodeValue = `Mutation ${i} on element`;
  }

  await doubleRaf(document);
  const translationsCount = await resolveRequests();
  is(
    translationsCount,
    1,
    "Only one of the mutations was actually translated."
  );

  await htmlMatches(
    "The changed node gets translated",
    /* html */ `
      <div>
        M̅u̅t̅a̅t̅i̅o̅n̅ 5 o̅n̅ e̅l̅e̅m̅e̅n̅t̅ (id:2)
      </div>
    `
  );
  cleanup();
});

/**
 * Test what happens when an inline element is mutated inside of a block element.
 */
add_task(async function test_inline_elements() {
  const { translate, htmlMatches, cleanup, document, resolveRequests } =
    await setupMutationsTest(/* html */ `
      <div>
        <span>inline one</span>
        <span title="Title attribute">inline two</span>
        <span>inline three</span>
      </div>
    `);

  translate();

  await doubleRaf(document);
  let translationsCount = await resolveRequests();
  is(
    translationsCount,
    2,
    "The whole block is sent as one translation and the title attribute was sent separately"
  );

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
    `
  );

  info("Mutating the text of span 2");
  /** @type {HTMLSpanElement} */
  const secondSpan = document.querySelectorAll("span")[1];
  secondSpan.innerText =
    "setting the innerText hits the childList mutation type";

  await doubleRaf(document);
  translationsCount = await resolveRequests();
  is(translationsCount, 1, "There were an expected number of translations");

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
    `
  );

  secondSpan.firstChild.nodeValue =
    "Change the character data for a specific node";

  await doubleRaf(document);
  translationsCount = await resolveRequests();
  is(translationsCount, 1, "There were an expected number of translations");

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
    `
  );

  secondSpan.setAttribute("title", "Mutate the title attribute");

  await doubleRaf(document);
  translationsCount = await resolveRequests();
  is(translationsCount, 1, "There were an expected number of translations");

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
    `
  );

  cleanup();
});

/**
 * Test the same behavior as `test_inline_elements` but with individual block
 * elements.
 */
add_task(async function test_block_elements() {
  const { translate, htmlMatches, cleanup, document, resolveRequests } =
    await setupMutationsTest(/* html */ `
      <section>
        <div>block one</div>
        <div title="Title attribute">block two</div>
        <div>block three</div>
      </section>
    `);

  translate();

  await doubleRaf(document);
  let translationsCount = await resolveRequests();
  is(translationsCount, 4, "The whole block is sent and the title attribute");

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
    `
  );

  info("Mutating the text of div 2");
  /** @type {HTMLSpanElement} */
  const secondDiv = document.querySelectorAll("div")[1];
  secondDiv.innerText =
    "setting the innerText hits the childList mutation type";

  await doubleRaf(document);
  translationsCount = await resolveRequests();
  is(translationsCount, 1, "There were an expected number of translations");

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
    `
  );

  secondDiv.firstChild.nodeValue =
    "Change the character data for a specific node";

  await doubleRaf(document);
  translationsCount = await resolveRequests();
  is(translationsCount, 1, "There were an expected number of translations");

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
    `
  );

  secondDiv.setAttribute("title", "Mutate the title attribute");

  await doubleRaf(document);
  translationsCount = await resolveRequests();
  is(translationsCount, 1, "There were an expected number of translations");

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
    `
  );

  cleanup();
});

add_task(async function test_removing_elements() {
  const { translate, htmlMatches, cleanup, document, resolveRequests } =
    await setupMutationsTest(/* html */ `
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

  await doubleRaf(document);
  let translationsCount = await resolveRequests();
  is(translationsCount, 1, "Only one block should have been translated.");

  await htmlMatches(
    "Only one element is translated",
    /* html */ `
      <section>
        <div>
          b̅l̅o̅c̅k̅ t̅h̅r̅e̅e̅ (id:3)
        </div>
      </section>
    `
  );

  cleanup();
});

add_task(async function test_mixed_block_inline() {
  const { translate, htmlMatches, cleanup, document, resolveRequests } =
    await setupMutationsTest(/* html */ `
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
  await resolveRequests();

  await htmlMatches(
    "The algorithm to chop of the nodes runs.",
    /* html */ `
      <section>
        f̅i̅r̅s̅t̅ t̅e̅x̅t̅ n̅o̅d̅e̅ (id:1)
        <div>
          b̅l̅o̅c̅k̅ o̅n̅e̅ (id:2)
        </div>
        s̅e̅c̅o̅n̅d̅ t̅e̅x̅t̅ n̅o̅d̅e̅ (id:3)
        <span>
          w̅i̅t̅h̅ i̅n̅l̅i̅n̅e̅ e̅l̅e̅m̅e̅n̅t̅ (id:4)
        </span>
        <div>
          b̅l̅o̅c̅k̅ t̅w̅o̅ (id:5)
        </div>
        t̅h̅i̅r̅d̅ t̅e̅x̅t̅ n̅o̅d̅e̅ (id:6)
        <div>
          b̅l̅o̅c̅k̅ t̅h̅r̅e̅e̅ (id:7)
        </div>
      </section>
    `
  );

  info("Mutating the <section>'s text nodes");
  const section = document.querySelector("section");
  for (let i = 0; i < section.childNodes.length; i++) {
    const node = section.childNodes[i];
    if (node.nodeType === Node.TEXT_NODE && node.nodeValue.trim()) {
      node.nodeValue = `Mutating ${i} text node`;
    }
  }

  await doubleRaf(document);
  let translationsCount = await resolveRequests();
  is(translationsCount, 3, "There were 3 text nodes");

  await htmlMatches(
    "",
    /* html */ `
      <section>
        M̅u̅t̅a̅t̅i̅n̅g̅ 0 t̅e̅x̅t̅ n̅o̅d̅e̅ (id:8)
        <div>
          b̅l̅o̅c̅k̅ o̅n̅e̅ (id:2)
        </div>
        M̅u̅t̅a̅t̅i̅n̅g̅ 2 t̅e̅x̅t̅ n̅o̅d̅e̅ (id:9)
        <span>
          w̅i̅t̅h̅ i̅n̅l̅i̅n̅e̅ e̅l̅e̅m̅e̅n̅t̅ (id:4)
        </span>
        <div>
          b̅l̅o̅c̅k̅ t̅w̅o̅ (id:5)
        </div>
        M̅u̅t̅a̅t̅i̅n̅g̅ 6 t̅e̅x̅t̅ n̅o̅d̅e̅ (id:10)
        <div>
          b̅l̅o̅c̅k̅ t̅h̅r̅e̅e̅ (id:7)
        </div>
      </section>
    `
  );

  cleanup();
});

add_task(async function test_appending_element() {
  const { translate, htmlMatches, cleanup, document, resolveRequests } =
    await setupMutationsTest(/* html */ `
      <section>
        <div>block one</div>
        <div>block two</div>
        <div>block three</div>
      </section>
    `);

  translate();
  await resolveRequests();

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
    `
  );

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
          Adding multiple elements at once
        </div>
        <div>
          <div>
            It even has
            <span data-moz-translations-id="0">
              nested
            </span>
            elements
          </div>
        </div>
        <div>
          b̅l̅o̅c̅k̅ t̅w̅o̅ (id:2)
        </div>
        <div>
          b̅l̅o̅c̅k̅ t̅h̅r̅e̅e̅ (id:3)
        </div>
      </section>
    `
  );

  await doubleRaf(document);
  let translationsCount = await resolveRequests();
  is(translationsCount, 2, "The two block elements were translated");

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
    `
  );

  cleanup();
});

add_task(async function test_mutating_comments() {
  const { translate, htmlMatches, cleanup, document, resolveRequests } =
    await setupMutationsTest(/* html */ `
      <section>
        <!-- this is a comment -->
        <div>block one</div>
      </section>
    `);

  translate();
  await resolveRequests();

  await htmlMatches(
    "The blocks are translated",
    /* html */ `
      <section>
        <!-- this is a comment -->
        <div>
          b̅l̅o̅c̅k̅ o̅n̅e̅ (id:1)
        </div>
      </section>
    `
  );

  // Mutate the comment's contents.
  const section = document.querySelector("section");
  const commentNode = [...section.childNodes].find(
    node => node.nodeType === Node.COMMENT_NODE
  );
  commentNode.nodeValue = "Change the comment";

  await doubleRaf(document);
  let translationsCount = await resolveRequests();
  is(translationsCount, 0, "Nothing was translated.");

  await htmlMatches(
    "The comment is untranslated in a mutation",
    /* html */ `
      <section>
        <!--Change the comment-->
        <div>
          b̅l̅o̅c̅k̅ o̅n̅e̅ (id:1)
        </div>
      </section>
    `
  );
  cleanup();
});
