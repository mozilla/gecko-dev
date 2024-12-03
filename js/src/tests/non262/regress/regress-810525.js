/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */
var expect = '';
var actual = '';

function test(s) {
    assertThrowsInstanceOfWithMessageCheck(
        () => eval(s),
        Error,
        message => message.indexOf('(intermediate value)') === -1,
        `error message for ${s} should not contain '(intermediate value)'`);
}

test("({p:1, q:2}).m()");
test("[].m()");
test("[1,2,3].m()");
test("/hi/.m()");

reportCompare(0, 0, "ok");
