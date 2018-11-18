/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const SUGGESTIONS_ENGINE_NAME = "engine-suggestions.xml";

// Basic test that uses two engines, a GET engine and a POST engine, neither
// providing search suggestions.
add_task(async function basicGetAndPost() {
  // Note that head_autocomplete.js has already added a MozSearch engine.
  // Here we add another engine with a search alias.
  Services.search.addEngineWithDetails("AliasedGETMozSearch", "", "get", "",
                                       "GET", "http://s.example.com/search");
  Services.search.addEngineWithDetails("AliasedPOSTMozSearch", "", "post", "",
                                       "POST", "http://s.example.com/search");

  await PlacesTestUtils.addVisits("http://s.example.com/search?q=firefox");
  let historyMatch = {
    value: "http://s.example.com/search?q=firefox",
    comment: "test visit for http://s.example.com/search?q=firefox",
  };

  for (let alias of ["get", "post"]) {
    await check_autocomplete({
      search: alias,
      searchParam: "enable-actions",
      matches: [
        makeSearchMatch(`${alias} `, {
          engineName: `Aliased${alias.toUpperCase()}MozSearch`,
          searchQuery: "",
          alias,
          heuristic: true,
        }),
        historyMatch,
      ],
    });

    await check_autocomplete({
      search: `${alias} `,
      searchParam: "enable-actions",
      matches: [
        makeSearchMatch(`${alias} `, {
          engineName: `Aliased${alias.toUpperCase()}MozSearch`,
          searchQuery: "",
          alias,
          heuristic: true,
        }),
        historyMatch,
      ],
    });

    await check_autocomplete({
      search: `${alias} fire`,
      searchParam: "enable-actions",
      matches: [
        makeSearchMatch(`${alias} fire`, {
          engineName: `Aliased${alias.toUpperCase()}MozSearch`,
          searchQuery: "fire",
          alias,
          heuristic: true,
        }),
        historyMatch,
      ],
    });

    await check_autocomplete({
      search: `${alias} mozilla`,
      searchParam: "enable-actions",
      matches: [
        makeSearchMatch(`${alias} mozilla`, {
          engineName: `Aliased${alias.toUpperCase()}MozSearch`,
          searchQuery: "mozilla",
          alias,
          heuristic: true,
        }),
      ],
    });

    await check_autocomplete({
      search: `${alias} MoZiLlA`,
      searchParam: "enable-actions",
      matches: [
        makeSearchMatch(`${alias} MoZiLlA`, {
          engineName: `Aliased${alias.toUpperCase()}MozSearch`,
          searchQuery: "MoZiLlA",
          alias,
          heuristic: true,
        }),
      ],
    });

    await check_autocomplete({
      search: `${alias} mozzarella mozilla`,
      searchParam: "enable-actions",
      matches: [
        makeSearchMatch(`${alias} mozzarella mozilla`, {
          engineName: `Aliased${alias.toUpperCase()}MozSearch`,
          searchQuery: "mozzarella mozilla",
          alias,
          heuristic: true,
        }),
      ],
    });

    // When a restriction token is used before the alias, the alias should *not*
    // be recognized.  It should be treated as part of the search string.  Try
    // all the restriction tokens to test that.  We should get a single "search
    // with" heuristic result without an alias.
    for (let restrictToken in UrlbarTokenizer.RESTRICT) {
      let search = `${restrictToken} ${alias} query string`;
      await check_autocomplete({
        search,
        searchParam: "enable-actions",
        matches: [
          makeSearchMatch(search, {
            engineName: "MozSearch",
            searchQuery: search,
            heuristic: true,
          }),
        ],
      });
    }
  }

  await cleanup();
});


// Uses an engine that provides search suggestions.
add_task(async function engineWithSuggestions() {
  let engine = await addTestSuggestionsEngine();

  await PlacesTestUtils.addVisits(engine.searchForm);
  let historyMatch = {
    value: "http://localhost:9000/search",
    comment: "test visit for http://localhost:9000/search",
  };

  // Use a normal alias and then one with an "@".  For the @ alias, the only
  // matches should be the search suggestions -- no history matches.
  for (let alias of ["moz", "@moz"]) {
    engine.alias = alias;
    Assert.equal(engine.alias, alias);

    let expectedMatches = [
      makeSearchMatch(`${alias} `, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        alias,
        searchQuery: "",
        heuristic: true,
      }),
    ];
    if (alias[0] != "@") {
      expectedMatches.push(historyMatch);
    }
    await check_autocomplete({
      search: alias,
      searchParam: "enable-actions",
      matches: expectedMatches,
    });

    expectedMatches = [
      makeSearchMatch(`${alias} `, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        alias,
        searchQuery: "",
        heuristic: true,
      }),
    ];
    if (alias[0] != "@") {
      expectedMatches.push(historyMatch);
    }
    await check_autocomplete({
      search: `${alias} `,
      searchParam: "enable-actions",
      matches: expectedMatches,
    });

    await check_autocomplete({
      search: `${alias} fire`,
      searchParam: "enable-actions",
      matches: [
        makeSearchMatch(`${alias} fire`, {
          engineName: SUGGESTIONS_ENGINE_NAME,
          alias,
          searchQuery: "fire",
          heuristic: true,
        }),
        makeSearchMatch(`${alias} fire foo`, {
          engineName: SUGGESTIONS_ENGINE_NAME,
          alias,
          searchQuery: "fire",
          searchSuggestion: "fire foo",
        }),
        makeSearchMatch(`${alias} fire bar`, {
          engineName: SUGGESTIONS_ENGINE_NAME,
          alias,
          searchQuery: "fire",
          searchSuggestion: "fire bar",
        }),
      ],
    });
  }

  engine.alias = "";
  await cleanup();
});


// When the search is simply "@", the results should be a list of all the "@"
// alias engines.
add_task(async function tokenAliasEngines() {
  let tokenEngines = [];
  for (let engine of Services.search.getEngines()) {
    let aliases = [];
    if (engine.alias) {
      aliases.push(engine.alias);
    }
    aliases.push(...engine.wrappedJSObject._internalAliases);
    let tokenAliases = aliases.filter(a => a.startsWith("@"));
    if (tokenAliases.length) {
      tokenEngines.push({ engine, tokenAliases });
    }
  }
  if (!tokenEngines.length) {
    Assert.ok(true, "No token alias engines, skipping task.");
    return;
  }
  info("Got token alias engines: " +
       tokenEngines.map(({ engine }) => engine.name));

  await check_autocomplete({
    search: "@",
    searchParam: "enable-actions",
    matches: tokenEngines.map(({ engine, tokenAliases }) => {
      let alias = tokenAliases[0];
      return makeSearchMatch(alias + " ", {
        engineName: engine.name,
        alias,
        searchQuery: "",
      });
    }),
  });

  await cleanup();
});
