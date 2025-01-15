/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/  */

function run_test() {
  if (Error.isError) {
    let complete = false;
    try {
      Services.tm.dispatchToMainThread(null);
    } catch (e) {
      Assert.ok(
        Error.isError(e),
        "Error.isError is true for Exception instances"
      );
      complete = true;
    }
    Assert.ok(complete, "Exception was thrown as expected");
  }
}
