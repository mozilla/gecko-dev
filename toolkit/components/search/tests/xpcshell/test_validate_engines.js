/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Ensure all the engines defined in the configuration are valid by
// creating a refined configuration that includes all the engines
// with all the variants and subvariants enabled everywhere.

"use strict";

const IDS = new Set();

function uniqueId(id) {
  while (IDS.has(id)) {
    id += "_";
  }
  IDS.add(id);
  return id;
}

/**
 * For each subVariant, yields an array containing only that subVariant
 * but with environment set to allRegionsAndLocales and optional to undefined.
 * If subVariants is undefined, yields undefined once.
 *
 * @param {Array|undefined} subVariants
 *    The original subVariants array.
 * @yields {Array|undefined}
 *    An array containing only one subVariant.
 */
function* generateSubvariants(subVariants) {
  if (!subVariants) {
    yield undefined;
    return;
  }

  for (let subVariant of subVariants) {
    yield [
      {
        ...subVariant,
        optional: undefined,
        environment: { allRegionsAndLocales: true },
      },
    ];
  }
}

/**
 * For each variant and subVariant, yields an array containing only that variant
 * and subVariant but with environment set to allRegionsAndLocales and optional
 * to undefined in both the variant and subvariant.
 *
 * @param {Array|undefined} variants
 *    The original variants array.
 * @yields {Array|undefined}
 *     An array containing only one variant.
 */
function* generateVariants(variants) {
  for (let variant of variants) {
    for (let subVariants of generateSubvariants(variant.subVariants)) {
      yield [
        {
          ...variant,
          optional: undefined,
          environment: { allRegionsAndLocales: true },
          subVariants,
        },
      ];
    }
  }
}

/**
 * For each variant and subVariant of a given engine, yields an engine with
 * only that Variant and subVariant but enabled everywhere. Also makes sure
 * no identifiers and names of yielded engines are duplicated and works when
 * there are no variants or subVariants.
 *
 * @param {object} engine
 *    The engine with (potentially) multiple variants.
 * @yields {object}
 *    The same engine but with a single variant that is enabled everywhere.
 */
function* generateEngineVariants(engine) {
  for (let variants of generateVariants(engine.variants)) {
    let id = uniqueId(engine.identifier);
    yield {
      ...engine,
      base: {
        ...engine.base,
        // Reuse identifier as name to avoid duplicated names.
        name: id,
      },
      identifier: id,
      variants,
    };
  }
}

add_task(async function test_validate_all_engines_and_variants() {
  let settings = RemoteSettings(SearchUtils.SETTINGS_KEY);
  let config = await settings.get();
  config = config.flatMap(obj => {
    if (obj.recordType == "engine") {
      return [...generateEngineVariants(obj)];
    }
    return obj;
  });

  sinon.stub(settings, "get").returns(config);
  await Services.search.init();

  for (let id of IDS) {
    Assert.ok(
      !!Services.search.getEngineById(id),
      `Engine with id '${id}' was found.`
    );
  }

  sinon.restore();
});
