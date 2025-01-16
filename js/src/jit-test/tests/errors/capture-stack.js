// |jit-test| --setpref=experimental.error_capture_stack_trace;
load(libdir + "asserts.js");

assertEq('captureStackTrace' in Error, true);
assertEq(Error.captureStackTrace.length, 2);

let x = Error.captureStackTrace({});
assertEq(x, undefined);

assertThrowsInstanceOf(() => Error.captureStackTrace(), TypeError);
assertThrowsInstanceOf(() => Error.captureStackTrace(2), TypeError);

Error.captureStackTrace({}, 2);
Error.captureStackTrace({}, null);
Error.captureStackTrace({}, {});

function caller(f) {
    return f();
}

function fill() {
    let x = {}
    Error.captureStackTrace(x, caller);
    return x;
}

function test_elision() {
    let x = caller(fill);
    let { stack } = x;
    assertEq(stack.includes("caller"), false);
    assertEq(stack.includes("fill"), false);


    ({ stack } = caller(() => caller(fill)))
    print(stack);
    assertEq(stack.includes("caller"), true); // Only elide the first caller!
    assertEq(stack.includes("fill"), false);
}

test_elision();

function nestedLambda(f) {
    (() => {
        (() => {
            (() => {
                (() => {
                    f();
                })();
            })();
        })();
    })();
}


// If we never see a matching frame when requesting a truncated
// stack we should return the empty string
function test_no_match() {
    let obj = {};
    // test_elision chosen arbitrarily as a function object which
    // doesn't exist in the call stack here.
    let capture = () => Error.captureStackTrace(obj, test_elision);
    nestedLambda(capture);
    assertEq(obj.stack, "");
}
test_no_match()

function count_frames(str) {
    return str.split("\n").length
}

function test_nofilter() {
    let obj = {};
    let capture = () => Error.captureStackTrace(obj);
    nestedLambda(capture);
    assertEq(count_frames(obj.stack), 9);
}
test_nofilter();

function test_in_eval() {
    let obj = eval(`
    let obj = {};
    let capture = () => Error.captureStackTrace(obj);
    nestedLambda(capture);
    obj
    `)

    // Same as above, with an eval frame added!
    assertEq(count_frames(obj.stack), 10);
}
test_in_eval();

//
// [[ErrorData]]
//
const stackGetter = Object.getOwnPropertyDescriptor(Error.prototype, 'stack').get;
const getStack = function (obj) {
    return stackGetter.call(obj);
};

function test_uncensored() {
    let err = undefined;
    function create_err() {
        err = new Error;
        Error.captureStackTrace(err, test_uncensored);
    }

    nestedLambda(create_err);

    // Calling Error.captureStackTrace doesn't mess with the internal
    // [[ErrorData]] slot
    assertEq(count_frames(err.stack), 2);
    assertEq(count_frames(getStack(err)), 9)
}
test_uncensored()

// In general, the stacks a non-caller version of Error.captureStackStrace
// should match what Error gives you
function compare_stacks() {
    function censor_column(str) {
        return str.replace(/:(\d+):\d+\n/g, ":$1:censored\n")
    }

    let obj = {};
    let err = (Error.captureStackTrace(obj), new Error)
    assertEq(censor_column(err.stack), censor_column(obj.stack));
}
compare_stacks();
nestedLambda(compare_stacks)

// New global

function test_in_global(global) {
    global.evaluate(caller.toString());
    global.evaluate(fill.toString());
    global.evaluate(test_elision.toString());
    global.evaluate("test_elision()");

    global.evaluate(nestedLambda.toString())
    global.evaluate(test_no_match.toString());
    global.evaluate("test_no_match()");


    global.evaluate(compare_stacks.toString());
    global.evaluate(`
        compare_stacks();
        nestedLambda(compare_stacks)
    `)
}

let global = newGlobal();
test_in_global(global);

let global2 = newGlobal({ principal: 0 });
test_in_global(global2)

let global3 = newGlobal({ principal: 0xfffff });
test_in_global(global3)

// What if the caller is a proxy?
const caller_proxy = new Proxy(caller, {
    apply: function (target, thisArg, arguments) {
        return target(...arguments);
    }
});

function fill_proxy() {
    let x = {}
    Error.captureStackTrace(x, caller_proxy);
    return x;
}

// Proxies don't count for elision.
function test_proxy_elision() {
    let x = caller_proxy(fill_proxy);
    let { stack } = x;
    assertEq(stack.includes("caller"), true);
    assertEq(stack.includes("fill_proxy"), true);
}
test_proxy_elision();

const trivial_proxy = new Proxy(caller, {});
function fill_trivial() {
    let x = {}
    Error.captureStackTrace(x, trivial_proxy);
    return x;
}

// Elision doesn't work even on forwarding proxy
function test_trivial_elision() {
    let x = caller(fill_trivial);
    let { stack } = x;
    assertEq(stack.includes("caller"), true);
    assertEq(stack.includes("fill"), true);
}
test_trivial_elision();

// Elision happens through bind
function test_bind_elision() {
    let b = caller.bind(undefined, fill);
    let { stack } = b();
    assertEq(stack.includes("caller"), false);
    assertEq(stack.includes("fill"), false);
}
test_bind_elision();

// Cross Realm testing

let nr = newGlobal({ newCompartment: true })
nr.eval(`globalThis.x = {}`);
Error.captureStackTrace(nr.x);

// Test strict definition
function test_strict_definition() {
    "use strict";
    assertThrowsInstanceOf(() => Error.captureStackTrace(Object.freeze({ stack: null })), TypeError);
}
test_strict_definition();

function test_property_descriptor() {
    let o = {};
    Error.captureStackTrace(o);
    let desc = Object.getOwnPropertyDescriptor(o, "stack");
    assertEq(desc.configurable, true)
    assertEq(desc.writable, true)
    assertEq(desc.enumerable, false)
}
test_property_descriptor();

function test_delete() {
    let o = {};
    Error.captureStackTrace(o);
    delete o.stack
    assertEq("stack" in o, false)
}
test_delete();

// Principal testing: This is basic/shell-principals.js extended to support
// and compare Error.captureStackTrace.
//
// Reminder:
// >  In the shell, a principal is simply a 32-bit mask: P subsumes Q if the
// >  set bits in P are a superset of those in Q. Thus, the principal 0 is
// >  subsumed by everything, and the principal ~0 subsumes everything.

// Given a string of letters |expected|, say "abc", assert that the stack
// contains calls to a series of functions named by the next letter from
// the string, say a, b, and then c. Younger frames appear earlier in
// |expected| than older frames.
let count = 0;
function check(expected, stack) {
    print("check(" + JSON.stringify(expected) + ") against:\n" + stack);
    count++;

    // Extract only the function names from the stack trace. Omit the frames
    // for the top-level evaluation, if it is present.
    var split = stack.split(/(.)?@.*\n/).slice(0, -1);
    if (split[split.length - 1] === undefined)
        split = split.slice(0, -2);

    print(JSON.stringify(split));
    // Check the function names against the expected sequence.
    assertEq(split.length, expected.length * 2);
    for (var i = 0; i < expected.length; i++)
        assertEq(split[i * 2 + 1], expected[i]);
}

var low = newGlobal({ principal: 0 });
var mid = newGlobal({ principal: 0xffff });
var high = newGlobal({ principal: 0xfffff });

eval('function a() { let o = {}; Error.captureStackTrace(o); check("a",    o.stack); b(); }');
low.eval('function b() { let o = {}; Error.captureStackTrace(o); check("b",    o.stack); c(); }');
mid.eval('function c() { let o = {}; Error.captureStackTrace(o); check("cba",  o.stack); d(); }');
high.eval('function d() { let o = {}; Error.captureStackTrace(o); check("dcba", o.stack); e(); }');

// Globals created with no explicit principals get 0xffff.
eval('function e() { let o = {}; Error.captureStackTrace(o); check("ecba",     o.stack); f(); }');

low.eval('function f() { let o = {}; Error.captureStackTrace(o); check("fb",       o.stack); g(); }');
mid.eval('function g() { let o = {}; Error.captureStackTrace(o); check("gfecba",   o.stack); h(); }');
high.eval('function h() { let o = {}; Error.captureStackTrace(o); check("hgfedcba", o.stack);      }');

// Make everyone's functions visible to each other, as needed.
b = low.b;
low.c = mid.c;
mid.d = high.d;
high.e = e;
f = low.f;
low.g = mid.g;
mid.h = high.h;

low.check = mid.check = high.check = check;

// Kick the whole process off.
a();

assertEq(count, 8);

// Ensure filtering is based on caller realm not on target object.
low.eval("low_target = {}");
mid.eval("mid_target = {}");
high.eval("high_target = {}");

high.low_target = mid.low_target = low.low_target;
high.mid_target = low.mid_target = mid.mid_target;
mid.high_target = low.high_target = high.high_target;

high.low_cst = mid.low_cst = low.low_cst = low.Error.captureStackTrace;
high.mid_cst = low.mid_cst = mid.mid_cst = mid.Error.captureStackTrace;
mid.high_cst = low.high_cst = high.high_cst = high.Error.captureStackTrace;

for (let g of [low, mid, high]) {
    assertEq("low_target" in g, true);
    assertEq("mid_target" in g, true);
    assertEq("high_target" in g, true);

    assertEq("low_cst" in g, true);
    assertEq("mid_cst" in g, true);
    assertEq("high_cst" in g, true);

    // install caller function z -- single letter name for
    // check compat.
    g.eval("function z(f) { f() }")
}

low.eval("function q() { Error.captureStackTrace(low_target); }")


high.q = low.q;

// Caller function z is from high, but using low Error.captureStackTrace, so
// z should be elided.
high.eval("z(q)");
check("q", low.low_target.stack);

low.eval("function r() { high_cst(low_target) }")
high.r = low.r;

// Can see everything here using high cst and low target.
high.eval("function t() { z(r) }");
high.t();
check("rzt", low.low_target.stack);


