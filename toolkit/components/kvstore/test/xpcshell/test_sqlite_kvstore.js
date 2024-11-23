/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { KeyValueService, SQLiteKeyValueService, KeyValueImporter } =
  ChromeUtils.importESModule("resource://gre/modules/kvstore.sys.mjs");

add_setup(async function setup() {
  do_get_profile();
});

async function makeDatabaseDir(name) {
  const databaseDir = PathUtils.join(PathUtils.profileDir, name);
  await IOUtils.makeDirectory(databaseDir);
  return databaseDir;
}

async function allEntries(db) {
  return Array.from(await db.enumerate(), ({ key, value }) => ({ key, value }));
}

async function allKeys(db) {
  let entries = await allEntries(db);
  return entries.map(({ key }) => key);
}

add_task(async function getOrCreate() {
  const databaseDir = await makeDatabaseDir("getOrCreate");
  const database = await SQLiteKeyValueService.getOrCreate(databaseDir, "db");
  Assert.ok(database);
  await database.close();

  // Test creating a database with a nonexistent path.
  const nonexistentDir = PathUtils.join(PathUtils.profileDir, "nonexistent");
  await Assert.rejects(
    SQLiteKeyValueService.getOrCreate(nonexistentDir),
    /storage dir/
  );

  // Test creating a database with a non-normalized but fully-qualified path.
  let nonNormalizedDir = await makeDatabaseDir("non-normalized");
  nonNormalizedDir = [nonNormalizedDir, "..", ".", "non-normalized"].join(
    Services.appinfo.OS === "WINNT" ? "\\" : "/"
  );
  const nonNormalizedRepository = await SQLiteKeyValueService.getOrCreate(
    nonNormalizedDir,
    "db"
  );
  Assert.ok(nonNormalizedRepository);
  await nonNormalizedRepository.close();
});

add_task(async function putGetHasDelete() {
  const databaseDir = await makeDatabaseDir("putGetHasDelete");
  const database = await SQLiteKeyValueService.getOrCreate(databaseDir, "db");

  // Getting key/value pairs that don't exist (yet) returns default values
  // or null, depending on whether you specify a default value.
  Assert.strictEqual(await database.get("int-key", 1), 1);
  Assert.strictEqual(await database.get("double-key", 1.1), 1.1);
  Assert.strictEqual(await database.get("string-key", ""), "");
  Assert.strictEqual(await database.get("bool-key", false), false);
  Assert.strictEqual(await database.get("int-key"), undefined);
  Assert.strictEqual(await database.get("double-key"), undefined);
  Assert.strictEqual(await database.get("string-key"), undefined);
  Assert.strictEqual(await database.get("bool-key"), undefined);

  // The put method succeeds without returning a value.
  Assert.strictEqual(await database.put("int-key", 1234), undefined);
  Assert.strictEqual(await database.put("double-key", 56.78), undefined);
  Assert.strictEqual(
    await database.put("string-key", "Héllo, wőrld!"),
    undefined
  );
  Assert.strictEqual(await database.put("bool-key", true), undefined);

  // Getting key/value pairs that exist returns the expected values.
  Assert.strictEqual(await database.get("int-key", 1), 1234);
  Assert.strictEqual(await database.get("double-key", 1.1), 56.78);
  Assert.strictEqual(await database.get("string-key", ""), "Héllo, wőrld!");
  Assert.strictEqual(await database.get("bool-key", false), true);
  Assert.strictEqual(await database.get("int-key"), 1234);
  Assert.strictEqual(await database.get("double-key"), 56.78);
  Assert.strictEqual(await database.get("string-key"), "Héllo, wőrld!");
  Assert.strictEqual(await database.get("bool-key"), true);

  // The has() method works as expected for both existing and non-existent keys.
  Assert.strictEqual(await database.has("int-key"), true);
  Assert.strictEqual(await database.has("double-key"), true);
  Assert.strictEqual(await database.has("string-key"), true);
  Assert.strictEqual(await database.has("bool-key"), true);
  Assert.strictEqual(await database.has("nonexistent-key"), false);

  // The delete() method succeeds without returning a value.
  Assert.strictEqual(await database.delete("int-key"), undefined);
  Assert.strictEqual(await database.delete("double-key"), undefined);
  Assert.strictEqual(await database.delete("string-key"), undefined);
  Assert.strictEqual(await database.delete("bool-key"), undefined);

  // The has() method works as expected for a deleted key.
  Assert.strictEqual(await database.has("int-key"), false);
  Assert.strictEqual(await database.has("double-key"), false);
  Assert.strictEqual(await database.has("string-key"), false);
  Assert.strictEqual(await database.has("bool-key"), false);

  // Getting key/value pairs that were deleted returns default values.
  Assert.strictEqual(await database.get("int-key", 1), 1);
  Assert.strictEqual(await database.get("double-key", 1.1), 1.1);
  Assert.strictEqual(await database.get("string-key", ""), "");
  Assert.strictEqual(await database.get("bool-key", false), false);
  Assert.strictEqual(await database.get("int-key"), undefined);
  Assert.strictEqual(await database.get("double-key"), undefined);
  Assert.strictEqual(await database.get("string-key"), undefined);
  Assert.strictEqual(await database.get("bool-key"), undefined);
});

add_task(async function putWithResizing() {
  const databaseDir = await makeDatabaseDir("putWithResizing");
  const database = await SQLiteKeyValueService.getOrCreate(databaseDir, "db");

  // The default store size is 1MB, putting key/value pairs bigger than that
  // would trigger auto resizing.
  const base = "A humongous string in 32 bytes!!";
  const val1M = base.repeat(32768);
  const val2M = val1M.repeat(2);
  Assert.strictEqual(await database.put("A-1M-value", val1M), undefined);
  Assert.strictEqual(await database.put("A-2M-value", val2M), undefined);
  Assert.strictEqual(await database.put("A-32B-value", base), undefined);

  Assert.strictEqual(await database.get("A-1M-value"), val1M);
  Assert.strictEqual(await database.get("A-2M-value"), val2M);
  Assert.strictEqual(await database.get("A-32B-value"), base);
});

add_task(async function largeNumbers() {
  const databaseDir = await makeDatabaseDir("largeNumbers");
  const database = await SQLiteKeyValueService.getOrCreate(databaseDir, "db");

  const MAX_INT_VARIANT = Math.pow(2, 31) - 1;
  const MIN_DOUBLE_VARIANT = Math.pow(2, 31);

  await database.put("max-int-variant", MAX_INT_VARIANT);
  await database.put("min-double-variant", MIN_DOUBLE_VARIANT);
  await database.put("max-safe-integer", Number.MAX_SAFE_INTEGER);
  await database.put("min-safe-integer", Number.MIN_SAFE_INTEGER);
  await database.put("max-value", Number.MAX_VALUE);
  await database.put("min-value", Number.MIN_VALUE);

  Assert.strictEqual(await database.get("max-int-variant"), MAX_INT_VARIANT);
  Assert.strictEqual(
    await database.get("min-double-variant"),
    MIN_DOUBLE_VARIANT
  );
  Assert.strictEqual(
    await database.get("max-safe-integer"),
    Number.MAX_SAFE_INTEGER
  );
  Assert.strictEqual(
    await database.get("min-safe-integer"),
    Number.MIN_SAFE_INTEGER
  );
  Assert.strictEqual(await database.get("max-value"), Number.MAX_VALUE);
  Assert.strictEqual(await database.get("min-value"), Number.MIN_VALUE);
});

add_task(async function extendedCharacterKey() {
  const databaseDir = await makeDatabaseDir("extendedCharacterKey");
  const database = await SQLiteKeyValueService.getOrCreate(databaseDir, "db");

  // Ensure that we can use extended character (i.e. non-ASCII) strings as keys.

  await database.put("Héllo, wőrld!", 1);
  Assert.strictEqual(await database.has("Héllo, wőrld!"), true);
  Assert.strictEqual(await database.get("Héllo, wőrld!"), 1);

  const enumerator = await database.enumerate();
  const { key } = enumerator.getNext();
  Assert.strictEqual(key, "Héllo, wőrld!");

  await database.delete("Héllo, wőrld!");
});

add_task(async function deleteRange() {
  const databaseDir = await makeDatabaseDir("deleteRange");
  const database = await SQLiteKeyValueService.getOrCreate(databaseDir, "db");

  await database.writeMany({
    "bool/a": true,
    "bool/b": true,
    "double/a": 12.34,
    "double/b": 56.78,
    "int/a": 12,
    "int/b": 34,
    "int/c": 56,
    "int/d": 78,
    "string/a": "Héllo",
    "string/b": "Wőrld",
  });

  await database.deleteRange(null, "double/a");
  Assert.deepEqual(await allKeys(database), [
    "double/a",
    "double/b",
    "int/a",
    "int/b",
    "int/c",
    "int/d",
    "string/a",
    "string/b",
  ]);

  await database.deleteRange("int/a", "string");
  Assert.deepEqual(await allKeys(database), [
    "double/a",
    "double/b",
    "string/a",
    "string/b",
  ]);

  await database.deleteRange("string/b", "string/a");
  Assert.deepEqual(await allKeys(database), [
    "double/a",
    "double/b",
    "string/a",
    "string/b",
  ]);

  await database.deleteRange("string");
  Assert.deepEqual(await allKeys(database), ["double/a", "double/b"]);

  await database.deleteRange("string");
  Assert.deepEqual(await allKeys(database), ["double/a", "double/b"]);

  await database.deleteRange();
  Assert.deepEqual(await allKeys(database), []);
});

add_task(async function clear() {
  const databaseDir = await makeDatabaseDir("clear");
  const database = await SQLiteKeyValueService.getOrCreate(databaseDir, "db");

  await database.put("int-key", 1234);
  await database.put("double-key", 56.78);
  await database.put("string-key", "Héllo, wőrld!");
  await database.put("bool-key", true);

  Assert.strictEqual(await database.clear(), undefined);
  Assert.strictEqual(await database.has("int-key"), false);
  Assert.strictEqual(await database.has("double-key"), false);
  Assert.strictEqual(await database.has("string-key"), false);
  Assert.strictEqual(await database.has("bool-key"), false);
});

add_task(async function writeManyFailureCases() {
  const databaseDir = await makeDatabaseDir("writeManyFailureCases");
  const database = await SQLiteKeyValueService.getOrCreate(databaseDir, "db");

  Assert.throws(() => database.writeMany(), /unexpected argument/);
  Assert.throws(() => database.writeMany("foo"), /unexpected argument/);
  Assert.throws(() => database.writeMany(["foo"]), /unexpected argument/);
});

add_task(async function writeManyPutOnly() {
  const databaseDir = await makeDatabaseDir("writeMany");
  const database = await SQLiteKeyValueService.getOrCreate(databaseDir, "db");

  async function test_helper(pairs) {
    Assert.strictEqual(await database.writeMany(pairs), undefined);
    Assert.strictEqual(await database.get("int-key"), 1234);
    Assert.strictEqual(await database.get("double-key"), 56.78);
    Assert.strictEqual(await database.get("string-key"), "Héllo, wőrld!");
    Assert.strictEqual(await database.get("bool-key"), true);
    await database.clear();
  }

  // writeMany with an empty object is OK
  Assert.strictEqual(await database.writeMany({}), undefined);

  // writeMany with an object
  const pairs = {
    "int-key": 1234,
    "double-key": 56.78,
    "string-key": "Héllo, wőrld!",
    "bool-key": true,
  };
  await test_helper(pairs);

  // writeMany with an array of pairs
  const arrayPairs = [
    ["int-key", 1234],
    ["double-key", 56.78],
    ["string-key", "Héllo, wőrld!"],
    ["bool-key", true],
  ];
  await test_helper(arrayPairs);

  // writeMany with a key/value generator
  function* pairMaker() {
    yield ["int-key", 1234];
    yield ["double-key", 56.78];
    yield ["string-key", "Héllo, wőrld!"];
    yield ["bool-key", true];
  }
  await test_helper(pairMaker());

  // writeMany with a map
  const mapPairs = new Map(arrayPairs);
  await test_helper(mapPairs);
});

add_task(async function writeManyLargePairsWithResizing() {
  const databaseDir = await makeDatabaseDir("writeManyWithResizing");
  const database = await SQLiteKeyValueService.getOrCreate(databaseDir, "db");

  // The default store size is 1MB, putting key/value pairs bigger than that
  // would trigger auto resizing.
  const base = "A humongous string in 32 bytes!!";
  const val1M = base.repeat(32768);
  const val2M = val1M.repeat(2);

  // writeMany with an object
  const pairs = {
    "A-1M-value": val1M,
    "A-32B-value": base,
    "A-2M-value": val2M,
  };

  Assert.strictEqual(await database.writeMany(pairs), undefined);

  Assert.strictEqual(await database.get("A-1M-value"), val1M);
  Assert.strictEqual(await database.get("A-2M-value"), val2M);
  Assert.strictEqual(await database.get("A-32B-value"), base);
});

add_task(async function writeManySmallPairsWithResizing() {
  const databaseDir = await makeDatabaseDir("writeManyWithResizing");
  const database = await SQLiteKeyValueService.getOrCreate(databaseDir, "db");

  // The default store size is 1MB, putting key/value pairs bigger than that
  // would trigger auto resizing.
  const base = "A humongous string in 32 bytes!!";
  const val1K = base.repeat(32);
  // writeMany with a key/value generator
  function* pairMaker() {
    for (let i = 0; i < 1024; i++) {
      yield [`key-${i}`, val1K];
    }
  }

  Assert.strictEqual(await database.writeMany(pairMaker()), undefined);
  for (let i = 0; i < 1024; i++) {
    Assert.ok(await database.has(`key-${i}`));
  }
});

add_task(async function writeManyDeleteOnly() {
  const databaseDir = await makeDatabaseDir("writeManyDeletesOnly");
  const database = await SQLiteKeyValueService.getOrCreate(databaseDir, "db");

  // writeMany with an object
  const pairs = {
    "int-key": 1234,
    "double-key": 56.78,
    "string-key": "Héllo, wőrld!",
    "bool-key": true,
  };

  async function test_helper(deletes) {
    Assert.strictEqual(await database.writeMany(pairs), undefined);
    Assert.strictEqual(await database.writeMany(deletes), undefined);
    Assert.strictEqual(await database.get("int-key"), undefined);
    Assert.strictEqual(await database.get("double-key"), undefined);
    Assert.strictEqual(await database.get("string-key"), undefined);
    Assert.strictEqual(await database.get("bool-key"), undefined);
  }

  // writeMany with an empty object is OK
  Assert.strictEqual(await database.writeMany({}), undefined);

  // writeMany with an object
  await test_helper({
    "int-key": null,
    "double-key": null,
    "string-key": null,
    "bool-key": null,
  });

  // writeMany with an array of pairs
  const arrayPairs = [
    ["int-key", null],
    ["double-key", null],
    ["string-key", null],
    ["bool-key", null],
  ];
  await test_helper(arrayPairs);

  // writeMany with a key/value generator
  function* pairMaker() {
    yield ["int-key", null];
    yield ["double-key", null];
    yield ["string-key", null];
    yield ["bool-key", null];
  }
  await test_helper(pairMaker());

  // writeMany with a map
  const mapPairs = new Map(arrayPairs);
  await test_helper(mapPairs);
});

add_task(async function writeManyPutDelete() {
  const databaseDir = await makeDatabaseDir("writeManyPutDelete");
  const database = await SQLiteKeyValueService.getOrCreate(databaseDir, "db");

  await database.writeMany([
    ["key1", "val1"],
    ["key3", "val3"],
    ["key4", "val4"],
    ["key5", "val5"],
  ]);

  await database.writeMany([
    ["key2", "val2"],
    ["key4", null],
    ["key5", null],
  ]);

  Assert.strictEqual(await database.get("key1"), "val1");
  Assert.strictEqual(await database.get("key2"), "val2");
  Assert.strictEqual(await database.get("key3"), "val3");
  Assert.strictEqual(await database.get("key4"), undefined);
  Assert.strictEqual(await database.get("key5"), undefined);

  await database.clear();

  await database.writeMany([
    ["key1", "val1"],
    ["key1", null],
    ["key1", "val11"],
    ["key1", null],
    ["key2", null],
    ["key2", "val2"],
  ]);

  Assert.strictEqual(await database.get("key1"), undefined);
  Assert.strictEqual(await database.get("key2"), "val2");
});

add_task(async function getOrCreateNamedDatabases() {
  const databaseDir = await makeDatabaseDir("getOrCreateNamedDatabases");

  let fooDB = await SQLiteKeyValueService.getOrCreate(databaseDir, "foo");
  Assert.ok(fooDB, "retrieval of first named database works");

  let barDB = await SQLiteKeyValueService.getOrCreate(databaseDir, "bar");
  Assert.ok(barDB, "retrieval of second named database works");

  let bazDB = await SQLiteKeyValueService.getOrCreate(databaseDir, "baz");
  Assert.ok(bazDB, "retrieval of third named database works");

  // Key/value pairs that are put into a database don't exist in others.
  await bazDB.put("key", 1);
  Assert.ok(!(await fooDB.has("key")), "the foo DB still doesn't have the key");
  await fooDB.put("key", 2);
  Assert.ok(!(await barDB.has("key")), "the bar DB still doesn't have the key");
  await barDB.put("key", 3);
  Assert.strictEqual(
    await bazDB.get("key", 0),
    1,
    "the baz DB has its KV pair"
  );
  Assert.strictEqual(
    await fooDB.get("key", 0),
    2,
    "the foo DB has its KV pair"
  );
  Assert.strictEqual(
    await barDB.get("key", 0),
    3,
    "the bar DB has its KV pair"
  );

  // Key/value pairs that are deleted from a database still exist in other DBs.
  await bazDB.delete("key");
  Assert.strictEqual(
    await fooDB.get("key", 0),
    2,
    "the foo DB still has its KV pair"
  );
  await fooDB.delete("key");
  Assert.strictEqual(
    await barDB.get("key", 0),
    3,
    "the bar DB still has its KV pair"
  );
  await barDB.delete("key");
});

add_task(async function inMemory() {
  const database = await SQLiteKeyValueService.getOrCreate(":memory:", "db");
  await database.put("int-key", 1234);
  await database.put("double-key", 56.78);
  Assert.deepEqual(await allEntries(database), [
    {
      key: "double-key",
      value: 56.78,
    },
    {
      key: "int-key",
      value: 1234,
    },
  ]);

  const otherDatabase = await SQLiteKeyValueService.getOrCreate(
    ":memory:",
    "db"
  );
  await otherDatabase.put("string-key", "Héllo, wőrld!");
  await otherDatabase.put("bool-key", true);
  Assert.deepEqual(await allEntries(otherDatabase), [
    {
      key: "bool-key",
      value: true,
    },
    {
      key: "string-key",
      value: "Héllo, wőrld!",
    },
  ]);
});

add_task(async function enumeration() {
  const databaseDir = await makeDatabaseDir("enumeration");
  const database = await SQLiteKeyValueService.getOrCreate(databaseDir, "db");

  await database.put("int-key", 1234);
  await database.put("double-key", 56.78);
  await database.put("string-key", "Héllo, wőrld!");
  await database.put("bool-key", true);

  async function test(fromKey, toKey, pairs) {
    const enumerator = await database.enumerate(fromKey, toKey);

    for (const pair of pairs) {
      Assert.strictEqual(enumerator.hasMoreElements(), true);
      const element = enumerator.getNext();
      Assert.ok(element);
      Assert.strictEqual(element.key, pair[0]);
      Assert.strictEqual(element.value, pair[1]);
    }

    Assert.strictEqual(enumerator.hasMoreElements(), false);
    Assert.throws(() => enumerator.getNext(), /NS_ERROR_FAILURE/);
  }

  // Test enumeration without specifying "from" and "to" keys, which should
  // enumerate all of the pairs in the database.  This test does so explicitly
  // by passing "null", "undefined" or "" (empty string) arguments
  // for those parameters. The iterator test below also tests this implicitly
  // by not specifying arguments for those parameters.
  await test(null, null, [
    ["bool-key", true],
    ["double-key", 56.78],
    ["int-key", 1234],
    ["string-key", "Héllo, wőrld!"],
  ]);
  await test(undefined, undefined, [
    ["bool-key", true],
    ["double-key", 56.78],
    ["int-key", 1234],
    ["string-key", "Héllo, wőrld!"],
  ]);

  // The implementation doesn't distinguish between a null/undefined value
  // and an empty string, so enumerating pairs from "" to "" has the same effect
  // as enumerating pairs without specifying from/to keys: it enumerates
  // all of the pairs in the database.
  await test("", "", [
    ["bool-key", true],
    ["double-key", 56.78],
    ["int-key", 1234],
    ["string-key", "Héllo, wőrld!"],
  ]);

  // Test enumeration from a key that doesn't exist and is lexicographically
  // less than the least key in the database, which should enumerate
  // all of the pairs in the database.
  await test("aaaaa", null, [
    ["bool-key", true],
    ["double-key", 56.78],
    ["int-key", 1234],
    ["string-key", "Héllo, wőrld!"],
  ]);

  // Test enumeration from a key that doesn't exist and is lexicographically
  // greater than the first key in the database, which should enumerate pairs
  // whose key is greater than or equal to the specified key.
  await test("ccccc", null, [
    ["double-key", 56.78],
    ["int-key", 1234],
    ["string-key", "Héllo, wőrld!"],
  ]);

  // Test enumeration from a key that does exist, which should enumerate pairs
  // whose key is greater than or equal to that key.
  await test("int-key", null, [
    ["int-key", 1234],
    ["string-key", "Héllo, wőrld!"],
  ]);

  // Test enumeration from a key that doesn't exist and is lexicographically
  // greater than the greatest test key in the database, which should enumerate
  // none of the pairs in the database.
  await test("zzzzz", null, []);

  // Test enumeration to a key that doesn't exist and is lexicographically
  // greater than the greatest test key in the database, which should enumerate
  // all of the pairs in the database.
  await test(null, "zzzzz", [
    ["bool-key", true],
    ["double-key", 56.78],
    ["int-key", 1234],
    ["string-key", "Héllo, wőrld!"],
  ]);

  // Test enumeration to a key that doesn't exist and is lexicographically
  // less than the greatest test key in the database, which should enumerate
  // pairs whose key is less than the specified key.
  await test(null, "ppppp", [
    ["bool-key", true],
    ["double-key", 56.78],
    ["int-key", 1234],
  ]);

  // Test enumeration to a key that does exist, which should enumerate pairs
  // whose key is less than that key.
  await test(null, "int-key", [
    ["bool-key", true],
    ["double-key", 56.78],
  ]);

  // Test enumeration to a key that doesn't exist and is lexicographically
  // less than the least key in the database, which should enumerate
  // none of the pairs in the database.
  await test(null, "aaaaa", []);

  // Test enumeration between intermediate keys that don't exist, which should
  // enumerate the pairs whose keys lie in between them.
  await test("ggggg", "ppppp", [["int-key", 1234]]);

  // Test enumeration from a key that exists to the same key, which shouldn't
  // enumerate any pairs, because the "to" key is exclusive.
  await test("int-key", "int-key", []);

  // Test enumeration from a greater key to a lesser one, which should
  // enumerate none of the pairs in the database, even if the reverse ordering
  // would enumerate some pairs.  Consumers are responsible for ordering
  // the "from" and "to" keys such that "from" is less than or equal to "to".
  await test("ppppp", "ccccc", []);
  await test("int-key", "ccccc", []);
  await test("ppppp", "int-key", []);

  const actual = {};
  for (const { key, value } of await database.enumerate()) {
    actual[key] = value;
  }
  Assert.deepEqual(actual, {
    "bool-key": true,
    "double-key": 56.78,
    "int-key": 1234,
    "string-key": "Héllo, wőrld!",
  });

  await database.delete("int-key");
  await database.delete("double-key");
  await database.delete("string-key");
  await database.delete("bool-key");
});

add_task(async function keysWithWhitespace() {
  const databaseDir = await makeDatabaseDir("keysWithWhitespace");
  const database = await SQLiteKeyValueService.getOrCreate(databaseDir, "db");

  await database.put(" leading", 1234);
  await database.writeMany({
    "trailing ": 5678,
    " both ": 9123,
  });

  Assert.strictEqual(await database.get(" leading"), 1234);
  Assert.strictEqual(await database.get("trailing "), 5678);
  Assert.strictEqual(await database.get(" both "), 9123);

  Assert.strictEqual(await database.has(" leading"), true);
  Assert.strictEqual(await database.has(" nonexistent "), false);

  Assert.deepEqual(await allEntries(database), [
    { key: " both ", value: 9123 },
    { key: " leading", value: 1234 },
    { key: "trailing ", value: 5678 },
  ]);

  Assert.strictEqual(await database.deleteRange(" ", "t"));

  Assert.deepEqual(await allEntries(database), [
    { key: "trailing ", value: 5678 },
  ]);

  await database.delete("trailing ");

  Assert.ok(await database.isEmpty());
});

add_task(async function importFromRkv() {
  const databaseDir = await makeDatabaseDir("importFromRkv");

  const oldFooDB = await KeyValueService.getOrCreate(databaseDir, "foo");
  await oldFooDB.put("bool-key", true);
  await oldFooDB.put("double-key", 56.78);

  const oldBarDB = await KeyValueService.getOrCreate(databaseDir, "bar");
  await oldBarDB.put("int-key", 1234);
  await oldBarDB.put("string-key", "Héllo, wőrld!");

  const newFooDB = await SQLiteKeyValueService.getOrCreate(databaseDir, "foo");
  const newBarDB = await SQLiteKeyValueService.getOrCreate(databaseDir, "bar");

  {
    const importer = SQLiteKeyValueService.createImporter(
      SQLiteKeyValueService.Importer.RKV_SAFE_MODE,
      databaseDir
    );
    Assert.strictEqual(importer.path, databaseDir);
    Assert.strictEqual(
      importer.type,
      SQLiteKeyValueService.Importer.RKV_SAFE_MODE
    );

    importer.addDatabase("foo");

    await importer.import();

    Assert.deepEqual(await allEntries(newFooDB), [
      { key: "bool-key", value: true },
      { key: "double-key", value: 56.78 },
    ]);
    Assert.strictEqual(await newBarDB.isEmpty(), true);
  }

  {
    const importer = SQLiteKeyValueService.createImporter(
      SQLiteKeyValueService.Importer.RKV_SAFE_MODE,
      databaseDir
    );

    importer.addDatabase("bar");

    await importer.import();

    Assert.deepEqual(await allEntries(newBarDB), [
      { key: "int-key", value: 1234 },
      { key: "string-key", value: "Héllo, wőrld!" },
    ]);
  }

  await newFooDB.close();
  await newBarDB.close();
});

add_task(async function importAllFromRkv() {
  const databaseDir = await makeDatabaseDir("importAllFromRkv");

  const oldFooDB = await KeyValueService.getOrCreate(databaseDir, "foo");
  await oldFooDB.put("bool-key", true);
  await oldFooDB.put("double-key", 56.78);

  const oldBarDB = await KeyValueService.getOrCreate(databaseDir, "bar");
  await oldBarDB.put("int-key", 1234);
  await oldBarDB.put("string-key", "Héllo, wőrld!");

  const importer = SQLiteKeyValueService.createImporter(
    SQLiteKeyValueService.Importer.RKV_SAFE_MODE,
    databaseDir
  );

  importer.addAllDatabases();

  await importer.import();

  const newFooDB = await SQLiteKeyValueService.getOrCreate(databaseDir, "foo");
  Assert.deepEqual(await allEntries(newFooDB), [
    { key: "bool-key", value: true },
    { key: "double-key", value: 56.78 },
  ]);

  const newBarDB = await SQLiteKeyValueService.getOrCreate(databaseDir, "bar");
  Assert.deepEqual(await allEntries(newBarDB), [
    { key: "int-key", value: 1234 },
    { key: "string-key", value: "Héllo, wőrld!" },
  ]);

  await newFooDB.close();
  await newBarDB.close();
});

add_task(async function importOrErrorFromRkv() {
  const databaseDir = await makeDatabaseDir("importOrErrorFromRkv");

  const oldFooDB = await KeyValueService.getOrCreate(databaseDir, "foo");
  await oldFooDB.put("bool-key", true);
  await oldFooDB.put("int-key", 1234);

  const newFooDB = await SQLiteKeyValueService.getOrCreate(databaseDir, "foo");
  await oldFooDB.put("bool-key", true);
  await oldFooDB.put("int-key", 1234);

  const importer = SQLiteKeyValueService.createImporter(
    SQLiteKeyValueService.Importer.RKV_SAFE_MODE,
    databaseDir
  );

  importer
    .addDatabase("foo")
    .setConflictPolicy(KeyValueImporter.ConflictPolicy.ERROR);

  // Both databases contain identical values, so this import should succeed.
  await importer.import();

  await newFooDB.put("int-key", 5678);

  // The values are different now, so this import should fail.
  await Assert.rejects(importer.import(), /conflict: 'int-key'/);

  await newFooDB.close();
});

add_task(async function importOrIgnoreFromRkv() {
  const databaseDir = await makeDatabaseDir("importOrIgnoreFromRkv");

  const oldFooDB = await KeyValueService.getOrCreate(databaseDir, "foo");
  await oldFooDB.put("int-key", 1234);

  const newFooDB = await SQLiteKeyValueService.getOrCreate(databaseDir, "foo");
  await newFooDB.put("int-key", 5678);

  const importer = SQLiteKeyValueService.createImporter(
    SQLiteKeyValueService.Importer.RKV_SAFE_MODE,
    databaseDir
  );

  importer
    .addDatabase("foo")
    .setConflictPolicy(KeyValueImporter.ConflictPolicy.IGNORE);

  await importer.import();

  Assert.deepEqual(await allEntries(newFooDB), [
    { key: "int-key", value: 5678 },
  ]);

  await newFooDB.close();
});

add_task(async function importOrReplaceFromRkv() {
  const databaseDir = await makeDatabaseDir("importOrReplaceFromRkv");

  const oldFooDB = await KeyValueService.getOrCreate(databaseDir, "foo");
  await oldFooDB.put("int-key", 1234);

  const newFooDB = await SQLiteKeyValueService.getOrCreate(databaseDir, "foo");
  await newFooDB.put("int-key", 5678);

  const importer = SQLiteKeyValueService.createImporter(
    SQLiteKeyValueService.Importer.RKV_SAFE_MODE,
    databaseDir
  );

  importer
    .addDatabase("foo")
    .setConflictPolicy(KeyValueImporter.ConflictPolicy.REPLACE);

  await importer.import();

  Assert.deepEqual(await allEntries(newFooDB), [
    { key: "int-key", value: 1234 },
  ]);

  await newFooDB.close();
});

add_task(async function importFromRkvAndKeep() {
  const databaseDir = await makeDatabaseDir("importFromRkvAndKeep");

  const oldFooDB = await KeyValueService.getOrCreate(databaseDir, "foo");
  await oldFooDB.put("bool-key", true);
  await oldFooDB.put("double-key", 56.78);

  const oldBarDB = await KeyValueService.getOrCreate(databaseDir, "bar");
  await oldBarDB.put("int-key", 1234);
  await oldBarDB.put("string-key", "Héllo, wőrld!");

  const importer = SQLiteKeyValueService.createImporter(
    SQLiteKeyValueService.Importer.RKV_SAFE_MODE,
    databaseDir
  );

  importer.addAllDatabases();

  await importer.import();

  Assert.deepEqual(await allEntries(oldFooDB), [
    { key: "bool-key", value: true },
    { key: "double-key", value: 56.78 },
  ]);

  Assert.deepEqual(await allEntries(oldBarDB), [
    { key: "int-key", value: 1234 },
    { key: "string-key", value: "Héllo, wőrld!" },
  ]);
});

add_task(async function importFromRkvAndDelete() {
  const databaseDir = await makeDatabaseDir("importFromRkvAndDelete");

  const oldFooDB = await KeyValueService.getOrCreate(databaseDir, "foo");
  await oldFooDB.put("bool-key", true);
  await oldFooDB.put("double-key", 56.78);

  const oldBarDB = await KeyValueService.getOrCreate(databaseDir, "bar");
  await oldBarDB.put("int-key", 1234);
  await oldBarDB.put("string-key", "Héllo, wőrld!");

  const importer = SQLiteKeyValueService.createImporter(
    SQLiteKeyValueService.Importer.RKV_SAFE_MODE,
    databaseDir
  );

  importer
    .addAllDatabases()
    .setCleanupPolicy(KeyValueImporter.CleanupPolicy.DELETE);

  await importer.import();

  Assert.deepEqual(await allEntries(oldFooDB), []);
  Assert.deepEqual(await allEntries(oldBarDB), []);
});

add_task(async function importFromMultipleRkvDirs() {
  const destinationDir = await makeDatabaseDir("importFromMultipleRkvDirs");

  const fooDatabaseDir = PathUtils.join(destinationDir, "foo");
  await IOUtils.makeDirectory(fooDatabaseDir);
  const barDatabaseDir = PathUtils.join(destinationDir, "bar");
  await IOUtils.makeDirectory(barDatabaseDir);

  const oldBazDB = await KeyValueService.getOrCreate(destinationDir, "baz");
  await oldBazDB.put("bool-key", true);

  const oldQuxDB = await KeyValueService.getOrCreate(fooDatabaseDir, "qux");
  await oldQuxDB.put("double-key", 56.78);

  const oldBlargDB = await KeyValueService.getOrCreate(fooDatabaseDir, "blarg");
  await oldBlargDB.put("int-key", 1234);

  const oldBorkDB = await KeyValueService.getOrCreate(barDatabaseDir, "bork");
  await oldBorkDB.put("string-key", "Héllo, wőrld!");

  const importer = SQLiteKeyValueService.createImporter(
    SQLiteKeyValueService.Importer.RKV_SAFE_MODE,
    destinationDir
  );

  // Configure the importer to import four databases from three different
  // directories. Delete the imported keys from the old `qux` and `bork
  // databases, but keep them in the old `baz` and `blarg` databases.

  importer.addDatabase("baz");

  let fooSpec = importer.addPath(fooDatabaseDir);
  fooSpec
    .addDatabase("qux")
    .setCleanupPolicy(KeyValueImporter.CleanupPolicy.DELETE);
  fooSpec.addDatabase("blarg");

  let barSpec = importer.addPath(barDatabaseDir);
  barSpec
    .addAllDatabases()
    .setCleanupPolicy(KeyValueImporter.CleanupPolicy.DELETE);

  await importer.import();

  const newBazDB = await SQLiteKeyValueService.getOrCreate(
    destinationDir,
    "baz"
  );
  const newQuxDB = await SQLiteKeyValueService.getOrCreate(
    destinationDir,
    "qux"
  );
  const newBlargDB = await SQLiteKeyValueService.getOrCreate(
    destinationDir,
    "blarg"
  );
  const newBorkDB = await SQLiteKeyValueService.getOrCreate(
    destinationDir,
    "bork"
  );

  Assert.deepEqual(await allEntries(newBazDB), [
    { key: "bool-key", value: true },
  ]);
  Assert.deepEqual(await allEntries(newQuxDB), [
    { key: "double-key", value: 56.78 },
  ]);
  Assert.deepEqual(await allEntries(newBlargDB), [
    { key: "int-key", value: 1234 },
  ]);
  Assert.deepEqual(await allEntries(newBorkDB), [
    { key: "string-key", value: "Héllo, wőrld!" },
  ]);

  Assert.deepEqual(await allKeys(oldBazDB), ["bool-key"]);
  Assert.deepEqual(await allKeys(oldQuxDB), []);
  Assert.deepEqual(await allKeys(oldBlargDB), ["int-key"]);
  Assert.deepEqual(await allKeys(oldBorkDB), []);

  await newBazDB.close();
  await newQuxDB.close();
  await newBlargDB.close();
  await newBorkDB.close();
});

add_task(async function importOrErrorFromMultipleRkvDirs() {
  const destinationDir = await makeDatabaseDir(
    "importOrReplaceFromMultipleRkvDirs"
  );

  const fooDatabaseDir = PathUtils.join(destinationDir, "foo");
  await IOUtils.makeDirectory(fooDatabaseDir);
  const barDatabaseDir = PathUtils.join(destinationDir, "bar");
  await IOUtils.makeDirectory(barDatabaseDir);

  const oldFooBazDB = await KeyValueService.getOrCreate(fooDatabaseDir, "baz");
  await oldFooBazDB.put("bool-key", true);
  await oldFooBazDB.put("double-key", 56.78);

  const oldBarBazDB = await KeyValueService.getOrCreate(barDatabaseDir, "baz");
  await oldBarBazDB.put("double-key", 12.34);
  await oldBarBazDB.put("string-key", "Héllo, wőrld!");

  const importer = SQLiteKeyValueService.createImporter(
    SQLiteKeyValueService.Importer.RKV_SAFE_MODE,
    destinationDir
  );

  // `fooDatabaseDir` and `barDatabaseDir` both contain databases named `baz`,
  // with conflicting values for `double-key`. Since we add `fooDatabaseDir`
  // first, we'll import its `baz` first, then fail when we try to import
  // `baz` from `barDatabaseDir`.

  importer.addPath(fooDatabaseDir).addAllDatabases();
  importer
    .addPath(barDatabaseDir)
    .addAllDatabases()
    .setConflictPolicy(KeyValueImporter.ConflictPolicy.ERROR);

  await Assert.rejects(importer.import(), /conflict: 'double-key'/);

  const newBazDB = await SQLiteKeyValueService.getOrCreate(
    destinationDir,
    "baz"
  );
  Assert.deepEqual(await allEntries(newBazDB), [
    { key: "bool-key", value: true },
    { key: "double-key", value: 56.78 },
  ]);

  await newBazDB.close();
});

add_task(async function importOrIgnoreFromMultipleRkvDirs() {
  const destinationDir = await makeDatabaseDir(
    "importOrReplaceFromMultipleRkvDirs"
  );

  const fooDatabaseDir = PathUtils.join(destinationDir, "foo");
  await IOUtils.makeDirectory(fooDatabaseDir);
  const barDatabaseDir = PathUtils.join(destinationDir, "bar");
  await IOUtils.makeDirectory(barDatabaseDir);

  const oldFooBazDB = await KeyValueService.getOrCreate(fooDatabaseDir, "baz");
  await oldFooBazDB.put("bool-key", true);
  await oldFooBazDB.put("double-key", 56.78);
  await oldFooBazDB.put("int-key", 1234);

  const oldBarBazDB = await KeyValueService.getOrCreate(barDatabaseDir, "baz");
  await oldBarBazDB.put("double-key", 12.34);
  await oldBarBazDB.put("int-key", 5678);
  await oldBarBazDB.put("string-key", "Héllo, wőrld!");

  const importer = SQLiteKeyValueService.createImporter(
    SQLiteKeyValueService.Importer.RKV_SAFE_MODE,
    destinationDir
  );

  // `fooDatabaseDir` and `barDatabaseDir` both contain databases named `baz`,
  // with conflicting values for `double-key` and `int-key`. We'll import
  // from `fooDatabaseDir` first, then `barDatabaseDir`; ignoring the
  // conflicting pairs from the latter.

  importer.addPath(fooDatabaseDir).addAllDatabases();
  importer
    .addPath(barDatabaseDir)
    .addAllDatabases()
    .setConflictPolicy(KeyValueImporter.ConflictPolicy.IGNORE);

  await importer.import();

  const newBazDB = await SQLiteKeyValueService.getOrCreate(
    destinationDir,
    "baz"
  );
  Assert.deepEqual(await allEntries(newBazDB), [
    { key: "bool-key", value: true },
    { key: "double-key", value: 56.78 },
    { key: "int-key", value: 1234 },
    { key: "string-key", value: "Héllo, wőrld!" },
  ]);

  await newBazDB.close();
});

add_task(async function importOrReplaceFromMultipleRkvDirs() {
  const destinationDir = await makeDatabaseDir(
    "importOrReplaceFromMultipleRkvDirs"
  );

  const fooDatabaseDir = PathUtils.join(destinationDir, "foo");
  await IOUtils.makeDirectory(fooDatabaseDir);
  const barDatabaseDir = PathUtils.join(destinationDir, "bar");
  await IOUtils.makeDirectory(barDatabaseDir);

  const oldFooBazDB = await KeyValueService.getOrCreate(fooDatabaseDir, "baz");
  await oldFooBazDB.put("bool-key", true);
  await oldFooBazDB.put("double-key", 56.78);
  await oldFooBazDB.put("int-key", 1234);

  const oldBarBazDB = await KeyValueService.getOrCreate(barDatabaseDir, "baz");
  await oldBarBazDB.put("double-key", 12.34);
  await oldBarBazDB.put("int-key", 5678);
  await oldBarBazDB.put("string-key", "Héllo, wőrld!");

  const importer = SQLiteKeyValueService.createImporter(
    SQLiteKeyValueService.Importer.RKV_SAFE_MODE,
    destinationDir
  );

  // `fooDatabaseDir` and `barDatabaseDir` both contain databases named `baz`,
  // with conflicting values for `double-key` and `int-key`. We'll import
  // from `fooDatabaseDir` first, then `barDatabaseDir`; replacing the
  // conflicting pairs with the pairs from `barDatabaseDir`.

  importer.addPath(fooDatabaseDir).addAllDatabases();
  importer
    .addPath(barDatabaseDir)
    .addAllDatabases()
    .setConflictPolicy(KeyValueImporter.ConflictPolicy.REPLACE);

  await importer.import();

  const newBazDB = await SQLiteKeyValueService.getOrCreate(
    destinationDir,
    "baz"
  );
  Assert.deepEqual(await allEntries(newBazDB), [
    { key: "bool-key", value: true },
    { key: "double-key", value: 12.34 },
    { key: "int-key", value: 5678 },
    { key: "string-key", value: "Héllo, wőrld!" },
  ]);

  await newBazDB.close();
});

add_task(async function stats() {
  const database = await SQLiteKeyValueService.getOrCreate(":memory:", "db");
  Assert.strictEqual(await database.isEmpty(), true);
  Assert.strictEqual(await database.count(), 0);
  Assert.strictEqual(await database.size(), 0);

  await database.put("int-key", 1234); // 12 (7 + 5) bytes.
  Assert.strictEqual(await database.isEmpty(), false);
  Assert.strictEqual(await database.count(), 1);
  Assert.strictEqual(await database.size(), 12);

  await database.put("double-key", 56.78); // 16 (10 + 6) bytes.
  Assert.strictEqual(await database.isEmpty(), false);
  Assert.strictEqual(await database.count(), 2);
  Assert.strictEqual(await database.size(), 28);

  await database.put("string-key", "Héllo, wőrld!"); // 27 (10 + 17) bytes.
  Assert.strictEqual(await database.isEmpty(), false);
  Assert.strictEqual(await database.count(), 3);
  Assert.strictEqual(await database.size(), 55);

  await database.put("bool-key", true); // 9 (8 + 1) bytes.
  Assert.strictEqual(await database.isEmpty(), false);
  Assert.strictEqual(await database.count(), 4);
  Assert.strictEqual(await database.size(), 64);
});
