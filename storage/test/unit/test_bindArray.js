/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This file tests binding arrays to statements.
 */

add_task(async function test_errors() {
  let db = Services.storage.openSpecialDatabase("memory");
  let stmt = db.createStatement("SELECT * FROM carray(?1)");

  Assert.throws(
    () => stmt.bindArrayOfIntegersByIndex(0, 1),
    /NS_ERROR_XPC_CANT_CONVERT_PRIMITIVE_TO_ARRAY/,
    "Not an array"
  );
  Assert.throws(
    () => stmt.bindArrayOfIntegersByIndex(0, "string"),
    /NS_ERROR_XPC_CANT_CONVERT_PRIMITIVE_TO_ARRAY/,
    "Not an array"
  );
  Assert.throws(
    () => stmt.bindArrayOfIntegersByIndex(0, null),
    /NS_ERROR_XPC_CANT_CONVERT_PRIMITIVE_TO_ARRAY/,
    "Not an array"
  );
  Assert.throws(
    () => stmt.bindArrayOfUTF8StringsByIndex(0, null),
    /NS_ERROR_XPC_CANT_CONVERT_PRIMITIVE_TO_ARRAY/,
    "Not an array"
  );

  stmt.finalize();
  db.close();
});

add_task(async function test_bind_empty_array() {
  let db = Services.storage.openSpecialDatabase("memory");
  let stmt = db.createStatement("SELECT * FROM carray(?1)");
  stmt.bindArrayOfIntegersByIndex(0, []);
  Assert.ok(!stmt.executeStep(), "Execution succeeds with no results");
  stmt.finalize();
  db.close();
});

add_task(async function test_bind() {
  let db = getOpenedDatabase();
  db.executeSimpleSQL(`
    CREATE TABLE test (
    id INTEGER,
    value BLOB /* no affinity */
    )
  `);

  db.executeSimpleSQL(`
    INSERT INTO test (value)
    VALUES
    (1),
    (2),
    (1.1),
    (2.2),
    ("test1"),
    ("test2")
  `);

  function bindStatement(stmt, results) {
    if (Number.isInteger(results[0])) {
      stmt.bindArrayOfIntegersByIndex(0, results);
      stmt.bindArrayOfIntegersByName("values", results);
    } else if (typeof results[0] == "number") {
      stmt.bindArrayOfDoublesByIndex(0, results);
      stmt.bindArrayOfDoublesByName("values", results);
    } else if (typeof results[0] == "string") {
      stmt.bindArrayOfStringsByIndex(0, results);
      stmt.bindArrayOfStringsByName("values", results);
    }
  }

  for (let results of [[1, 2], [1.1, 2.2], ["test1", "test2"], []]) {
    info("sync statement");
    let query = `
      SELECT value FROM test
      WHERE value IN carray(?1)
        AND value IN carray(:values)
    `;
    let stmt = db.createStatement(query);
    bindStatement(stmt, results);
    for (let result of results) {
      Assert.ok(stmt.executeStep());
      Assert.equal(stmt.row.value, result);
    }
    stmt.finalize();

    info("async statement");
    stmt = db.createAsyncStatement(query);
    bindStatement(stmt, results);
    let rv = await new Promise((resolve, reject) => {
      let rows = [];
      stmt.executeAsync({
        handleResult(resultSet) {
          let row = null;
          do {
            row = resultSet.getNextRow();
            if (row) {
              rows.push(row);
            }
          } while (row);
        },
        handleError(error) {
          reject(new Error(`Failed to execute statement: ${error.message}`));
        },
        handleCompletion(reason) {
          if (reason == Ci.mozIStorageStatementCallback.REASON_FINISHED) {
            resolve(rows.map(r => r.getResultByIndex(0)));
          } else {
            reject(new Error("Statement failed to execute or was cancelled"));
          }
        },
      });
    });
    Assert.deepEqual(rv, results);
    stmt.finalize();
  }
  // we are the last test using this connection and since it has gone async
  // we *must* call asyncClose on it.
  await asyncClose(db);
});
