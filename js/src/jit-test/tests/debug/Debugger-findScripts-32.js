// Debugger.prototype.findScripts can filter scripts by start and end line and column.
load(libdir + "asserts.js");

function assertThrowsTypeError(query) {
    assertThrowsInstanceOf(() => dbg.findScripts(query), TypeError);
}
function assertFound(query, scriptWrapper) {
    assertEq(dbg.findScripts(query).includes(scriptWrapper), true, `Script not found, but should be (query: ${JSON.stringify(query)})`);
}
function assertNotFound(query, scriptWrapper) {
    assertEq(dbg.findScripts(query).includes(scriptWrapper), false, `Script found but should not be (query: ${JSON.stringify(query)})`);
}

var g = newGlobal({newCompartment: true});
var dbg = new Debugger();
var gw = dbg.addDebuggee(g);

var scriptname = scriptdir + 'Debugger-findScripts-32-script';
g.load(scriptname);

var gfw = gw.makeDebuggeeValue(g.f);
var ggw = gw.makeDebuggeeValue(g.f());
var ghw = gw.makeDebuggeeValue(g.h);
var gjw = gw.makeDebuggeeValue(g.j);

// actors/source.js uses {start: ..., end:...} for the query
// {url:scriptName, start:{ line: 3, column: 0}, end:{line: 3, column: Infinity}}
// NOTE: 'start.line' and 'end.line' are 1-origin, like 'line'

// 'start' correct types
assertThrowsTypeError({url:scriptname, start:3, end: {line: 8}});
assertThrowsTypeError({url:scriptname, start:"hi", end: {line: 8}});
assertThrowsTypeError({url:scriptname, start: {line: .34}, end: {line: 8}});
assertThrowsTypeError({url:scriptname, start: {line: -1}, end: {line: 8}});
assertThrowsTypeError({url:scriptname, start: {line: {}}, end: {line: 8}});
// 'end' correct types
assertThrowsTypeError({url:scriptname, start: {line: 8}, end:3});
assertThrowsTypeError({url:scriptname, start: {line: 8}, end:"hi"});
assertThrowsTypeError({url:scriptname, start: {line: 8}, end: {line: .34}});
assertThrowsTypeError({url:scriptname, start: {line: 8}, end: {line: -1}});
assertThrowsTypeError({url:scriptname, start: {line: 8}, end: {line: {}}});
// 'start' requires 'end'
assertThrowsInstanceOf(() => dbg.findScripts({url:scriptname, start: {line: 8}}), Error);
assertThrowsInstanceOf(() => dbg.findScripts({url:scriptname, line: 3, start: {line: 8}}), Error);
// 'end' requires 'start'
assertThrowsInstanceOf(() => dbg.findScripts({url:scriptname, end: {line: 8}}), Error);
assertThrowsInstanceOf(() => dbg.findScripts({url:scriptname, line: 4, end: {line: 8}}), Error);
// 'start'/'end' takes priority over 'line'
assertNotFound({url:scriptname, line: 8, start: {line: 3}, end: {line: 3}}, gfw.script);
assertFound({url:scriptname, line: 3, start: {line: 8}, end: {line: 8}}, gfw.script);

// 'start' and 'end'
// 'start' after,  'end' after the function
assertNotFound({url:scriptname, start: {line: 14}, end: {line: 16}}, gfw.script);
// 'start' in,     'end' after
assertFound({url:scriptname, start: {line: 8}, end: {line: 16}}, gfw.script);
// 'start' in,     'end' in
assertFound({url:scriptname, start: {line: 8}, end: {line: 11}}, gfw.script);
// 'start' before, 'end' after
assertFound({url:scriptname, start: {line: 6}, end: {line: 13}}, gfw.script);
// 'start' before, 'end' in
assertFound({url:scriptname, start: {line: 6}, end: {line: 10}}, gfw.script);
// 'start' before, 'end' before
assertNotFound({url:scriptname, start: {line: 4}, end: {line: 6}}, gfw.script);
// 'start.line' and 'end.line' are inclusive
assertFound({url:scriptname, start: {line: 6}, end: {line: 7}}, gfw.script);
assertFound({url:scriptname, start: {line: 12}, end: {line: 12}}, gfw.script);
assertFound({url:scriptname, start: {line: 20}, end: {line: 20}}, gjw.script);
// TODO: start after end

// innermost filter
assertFound({url:scriptname, innermost: true, start: {line: 20}, end: {line: 20}}, gjw.script);
assertFound({url:scriptname, innermost: true, start: {line: 6}, end: {line: 9}}, ggw.script);
assertFound({url:scriptname, innermost: true, start: {line: 6}, end: {line: 13}}, ggw.script);
assertFound({url:scriptname, innermost: true, start: {line: 11}, end: {line: 13}}, ggw.script);

// Specifying a line range outside of all functions screens out all function scripts.
assertNotFound({url:scriptname, start: {line: 4}, end: {line: 6}}, gfw.script);
assertNotFound({url:scriptname, start: {line: 4}, end: {line: 6}}, ggw.script);
assertNotFound({url:scriptname, start: {line: 4}, end: {line: 6}}, ghw.script);

// A line range within a nested function selects all enclosing functions' scripts.
assertFound({url:scriptname, start: {line: 10}, end: {line: 11}}, gfw.script);
assertFound({url:scriptname, start: {line: 10}, end: {line: 11}}, ggw.script);
assertNotFound({url:scriptname, start: {line: 10}, end: {line: 11}}, ghw.script);

// A line range outside a non-nested function does not select that function.
assertNotFound({url:scriptname, start: {line: 7}, end: {line: 8}}, ggw.script);
