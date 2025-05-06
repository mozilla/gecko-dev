// |reftest| skip-if(!Error.prototype.toSource)

/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/ */

var err = evaluate(`
var e = new AggregateError([]);
e.toSource();
`, { fileName: "test.js" });
assertEq(err, `(new AggregateError([], "", "test.js", 2))`);

err = evaluate(`
var e = new AggregateError([new Error("another")], "msg here");
e.toSource();
`, { fileName: "test.js" });
assertEq(err, `(new AggregateError([(new Error("another", "test.js", 2))], "msg here", "test.js", 2))`);

err = evaluate(`
var e = new AggregateError(new Set([new Error("some error"), new Error("another error")]),"Multiple errors thrown");
e.toSource();
`, { fileName: "test.js" });
assertEq(err, `(new AggregateError([(new Error("some error", "test.js", 2)), (new Error("another error", "test.js", 2))], "Multiple errors thrown", "test.js", 2))`);

err = evaluate(`
var e = new AggregateError([]);
e.errors.push(e);
e.toSource();
`, { fileName: "test.js" });
assertEq(err, `(new AggregateError([{}], "", "test.js", 2))`);

err = evaluate(`
var e = new AggregateError([]);
e.errors = e;
e.toSource();
`, { fileName: "test.js" });
assertEq(err, `(new AggregateError({}, "", "test.js", 2))`);

err = evaluate(`
var e = new AggregateError([]);
e.errors = undefined;
e.toSource();
`, { fileName: "test.js" });
assertEq(err, `(new AggregateError((void 0), "", "test.js", 2))`);

err = evaluate(`
var e = new AggregateError([]);
e.errors = null;
e.toSource();
`, { fileName: "test.js" });
assertEq(err, `(new AggregateError(null, "", "test.js", 2))`);

reportCompare(true, true);
