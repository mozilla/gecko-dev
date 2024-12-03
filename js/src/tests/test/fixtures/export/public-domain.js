/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

async function f() {
    let
    await 0;
}

assert.sameValue(true, f instanceof Function);
