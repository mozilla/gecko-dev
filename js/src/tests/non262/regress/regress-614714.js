// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

// SKIP test262 export
// Not interesting and triggers export bug.

function f(reportCompare) {
    if (typeof clear === 'function')
        clear(this);
    return f;
}

// This must be called before clear().
reportCompare(0, 0, 'ok');
f();  // don't assert
