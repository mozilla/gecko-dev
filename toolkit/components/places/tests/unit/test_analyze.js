/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests sqlite_sta1 table exists, it should be created by analyze.
// Note that this version of SQLite puts null rows for empty tables, while next
// versions will just omit the rows.

function run_test() {
  do_test_pending();

  let stmt = DBConn().createAsyncStatement(
    "SELECT ROWID FROM sqlite_stat1"
  );
  stmt.executeAsync({
    _gotResult: false,
    handleResult: function(aResultSet) {
      this._gotResult = true;
    },
    handleError: function(aError) {
      do_throw("Unexpected error (" + aError.result + "): " + aError.message);
    },
    handleCompletion: function(aReason) {
      do_check_true(this._gotResult);
      do_test_finished();
    }
  });
  stmt.finalize();
}
