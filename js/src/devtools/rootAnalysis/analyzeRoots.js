/* -*- Mode: Javascript; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

"use strict";

loadRelativeToScript('utility.js');
loadRelativeToScript('annotations.js');
loadRelativeToScript('CFG.js');

var sourceRoot = (environment['SOURCE'] || '') + '/'

var functionBodies;

if (typeof scriptArgs[0] != 'string' || typeof scriptArgs[1] != 'string')
    throw "Usage: analyzeRoots.js <gcFunctions.lst> <gcEdges.txt> <suppressedFunctions.lst> <gcTypes.txt> [start end [tmpfile]]";

var gcFunctionsFile = scriptArgs[0];
var gcEdgesFile = scriptArgs[1];
var suppressedFunctionsFile = scriptArgs[2];
var gcTypesFile = scriptArgs[3];
var batch = (scriptArgs[4]|0) || 1;
var numBatches = (scriptArgs[5]|0) || 1;
var tmpfile = scriptArgs[6] || "tmp.txt";

var gcFunctions = {};
var text = snarf("gcFunctions.lst").split("\n");
assert(text.pop().length == 0);
for (var line of text)
    gcFunctions[mangled(line)] = true;

var suppressedFunctions = {};
var text = snarf(suppressedFunctionsFile).split("\n");
assert(text.pop().length == 0);
for (var line of text) {
    suppressedFunctions[line] = true;
}
text = null;

var gcEdges = {};
text = snarf(gcEdgesFile).split('\n');
assert(text.pop().length == 0);
for (var line of text) {
    var [ block, edge, func ] = line.split(" || ");
    if (!(block in gcEdges))
        gcEdges[block] = {}
    gcEdges[block][edge] = func;
}
text = null;

var match;
var gcThings = {};
var gcPointers = {};

text = snarf(gcTypesFile).split("\n");
for (var line of text) {
    if (match = /^GCThing: (.*)/.exec(line))
        gcThings[match[1]] = true;
    if (match = /^GCPointer: (.*)/.exec(line))
        gcPointers[match[1]] = true;
}
text = null;

function isUnrootedType(type)
{
    if (type.Kind == "Pointer") {
        var target = type.Type;
        if (target.Kind == "CSU")
            return target.Name in gcThings;
        return false;
    }
    if (type.Kind == "CSU")
        return type.Name in gcPointers;
    return false;
}

function expressionUsesVariable(exp, variable)
{
    if (exp.Kind == "Var" && sameVariable(exp.Variable, variable))
        return true;
    if (!("Exp" in exp))
        return false;
    for (var childExp of exp.Exp) {
        if (expressionUsesVariable(childExp, variable))
            return true;
    }
    return false;
}

function edgeUsesVariable(edge, variable)
{
    if (ignoreEdgeUse(edge, variable))
        return false;
    switch (edge.Kind) {

    case "Assign":
        if (expressionUsesVariable(edge.Exp[0], variable))
            return true;
        return expressionUsesVariable(edge.Exp[1], variable);

    case "Assume":
        return expressionUsesVariable(edge.Exp[0], variable);

    case "Call":
        if (expressionUsesVariable(edge.Exp[0], variable))
            return true;
        if (1 in edge.Exp && expressionUsesVariable(edge.Exp[1], variable))
            return true;
        if ("PEdgeCallInstance" in edge) {
            if (expressionUsesVariable(edge.PEdgeCallInstance.Exp, variable))
                return true;
        }
        if ("PEdgeCallArguments" in edge) {
            for (var exp of edge.PEdgeCallArguments.Exp) {
                if (expressionUsesVariable(exp, variable))
                    return true;
            }
        }
        return false;

    case "Loop":
        return false;

    default:
        assert(false);
    }
}

function expressionIsVariableAddress(exp, variable)
{
    while (exp.Kind == "Fld")
        exp = exp.Exp[0];
    return exp.Kind == "Var" && sameVariable(exp.Variable, variable);
}

function edgeTakesVariableAddress(edge, variable)
{
    if (ignoreEdgeUse(edge, variable))
        return false;
    if (ignoreEdgeAddressTaken(edge))
        return false;
    switch (edge.Kind) {
    case "Assign":
        return expressionIsVariableAddress(edge.Exp[1], variable);
    case "Call":
        if ("PEdgeCallArguments" in edge) {
            for (var exp of edge.PEdgeCallArguments.Exp) {
                if (expressionIsVariableAddress(exp, variable))
                    return true;
            }
        }
        return false;
    default:
        return false;
    }
}

function edgeKillsVariable(edge, variable)
{
    // Direct assignments kill their lhs.
    if (edge.Kind == "Assign") {
        var lhs = edge.Exp[0];
        if (lhs.Kind == "Var" && sameVariable(lhs.Variable, variable))
            return true;
    }

    if (edge.Kind != "Call")
        return false;

    // Assignments of call results kill their lhs.
    if (1 in edge.Exp) {
        var lhs = edge.Exp[1];
        if (lhs.Kind == "Var" && sameVariable(lhs.Variable, variable))
            return true;
    }

    // Constructor calls kill their 'this' value.
    if ("PEdgeCallInstance" in edge) {
        do {
            var instance = edge.PEdgeCallInstance.Exp;

            // Kludge around incorrect dereference on some constructor calls.
            if (instance.Kind == "Drf")
                instance = instance.Exp[0];

            if (instance.Kind != "Var" || !sameVariable(instance.Variable, variable))
                break;

            var callee = edge.Exp[0];
            if (callee.Kind != "Var")
                break;

            assert(callee.Variable.Kind == "Func");
            var calleeName = readable(callee.Variable.Name[0]);

            // Constructor calls include the text 'Name::Name(' or 'Name<...>::Name('.
            var openParen = calleeName.indexOf('(');
            if (openParen < 0)
                break;
            calleeName = calleeName.substring(0, openParen);

            var lastColon = calleeName.lastIndexOf('::');
            if (lastColon < 0)
                break;
            var constructorName = calleeName.substr(lastColon + 2);
            calleeName = calleeName.substr(0, lastColon);

            var lastTemplateOpen = calleeName.lastIndexOf('<');
            if (lastTemplateOpen >= 0)
                calleeName = calleeName.substr(0, lastTemplateOpen);

            if (calleeName.endsWith(constructorName))
                return true;
        } while (false);
    }

    return false;
}

function edgeCanGC(edge)
{
    if (edge.Kind != "Call")
        return false;
    var callee = edge.Exp[0];
    if (callee.Kind == "Var") {
        var variable = callee.Variable;
        assert(variable.Kind == "Func");
        var callee = mangled(variable.Name[0]);
        if (callee in gcFunctions)
            return "'" + variable.Name[0] + "'";
        return null;
    }
    assert(callee.Kind == "Drf");
    if (callee.Exp[0].Kind == "Fld") {
        var field = callee.Exp[0].Field;
        var csuName = field.FieldCSU.Type.Name;
        var fullFieldName = csuName + "." + field.Name[0];
        if (fieldCallCannotGC(csuName, fullFieldName))
            return null;
        return (fullFieldName in suppressedFunctions) ? null : fullFieldName;
    }
    assert(callee.Exp[0].Kind == "Var");
    var varName = callee.Exp[0].Variable.Name[0];
    return indirectCallCannotGC(functionName, varName) ? null : "*" + varName;
}

function variableUseFollowsGC(suppressed, variable, worklist)
{
    // Scan through all edges following an unrooted variable use, using an
    // explicit worklist. A worklist contains a following edge together with a
    // description of where one of its predecessors GC'd (if any).

    while (worklist.length) {
        var entry = worklist.pop();
        var body = entry.body, ppoint = entry.ppoint;

        if (body.seen) {
            if (ppoint in body.seen) {
                var seenEntry = body.seen[ppoint];
                if (!entry.gcInfo || seenEntry.gcInfo)
                    continue;
            }
        } else {
            body.seen = [];
        }
        body.seen[ppoint] = {body:body, gcInfo:entry.gcInfo};

        if (ppoint == body.Index[0]) {
            if (body.BlockId.Kind == "Loop") {
                // propagate to parents that enter the loop body.
                if ("BlockPPoint" in body) {
                    for (var parent of body.BlockPPoint) {
                        var found = false;
                        for (var xbody of functionBodies) {
                            if (sameBlockId(xbody.BlockId, parent.BlockId)) {
                                assert(!found);
                                found = true;
                                worklist.push({body:xbody, ppoint:parent.Index,
                                               gcInfo:entry.gcInfo, why:entry});
                            }
                        }
                        assert(found);
                    }
                }
            } else if (variable.Kind == "Arg" && entry.gcInfo) {
                return {gcInfo:entry.gcInfo, why:entry};
            }
        }

        var predecessors = getPredecessors(body);
        if (!(ppoint in predecessors))
            continue;

        for (var edge of predecessors[ppoint]) {
            var source = edge.Index[0];

            if (edgeKillsVariable(edge, variable)) {
                if (entry.gcInfo)
                    return {gcInfo:entry.gcInfo, why:entry};
                if (!body.minimumUse || source < body.minimumUse)
                    body.minimumUse = source;
                continue;
            }

            var gcInfo = entry.gcInfo;
            if (!gcInfo && !(source in body.suppressed) && !suppressed) {
                var gcName = edgeCanGC(edge, body);
                if (gcName)
                    gcInfo = {name:gcName, body:body, ppoint:source};
            }

            if (edgeUsesVariable(edge, variable)) {
                if (gcInfo)
                    return {gcInfo:gcInfo, why:entry};
                if (!body.minimumUse || source < body.minimumUse)
                    body.minimumUse = source;
            }

            if (edge.Kind == "Loop") {
                // propagate to exit points of the loop body, in addition to the
                // predecessor of the loop edge itself.
                var found = false;
                for (var xbody of functionBodies) {
                    if (sameBlockId(xbody.BlockId, edge.BlockId)) {
                        assert(!found);
                        found = true;
                        worklist.push({body:xbody, ppoint:xbody.Index[1],
                                       gcInfo:gcInfo, why:entry});
                    }
                }
                assert(found);
                break;
            }
            worklist.push({body:body, ppoint:source, gcInfo:gcInfo, why:entry});
        }
    }

    return null;
}

function variableLiveAcrossGC(suppressed, variable)
{
    // A variable is live across a GC if (1) it is used by an edge, and (2) it
    // is used after a GC in a successor edge.

    for (var body of functionBodies) {
        body.seen = null;
        body.minimumUse = 0;
    }

    for (var body of functionBodies) {
        if (!("PEdge" in body))
            continue;
        for (var edge of body.PEdge) {
            if (edgeUsesVariable(edge, variable) && !edgeKillsVariable(edge, variable)) {
                var worklist = [{body:body, ppoint:edge.Index[0], gcInfo:null, why:null}];
                var call = variableUseFollowsGC(suppressed, variable, worklist);
                if (call)
                    return call;
            }
        }
    }
    return null;
}

// An unrooted variable has its address stored in another variable via
// assignment, or passed into a function that can GC. If the address is
// assigned into some other variable, we can't track it to see if it is held
// live across a GC. If it is passed into a function that can GC, then it's
// sort of like a Handle to an unrooted location, and the callee could GC
// before overwriting it or rooting it.
function unsafeVariableAddressTaken(suppressed, variable)
{
    for (var body of functionBodies) {
        if (!("PEdge" in body))
            continue;
        for (var edge of body.PEdge) {
            if (edgeTakesVariableAddress(edge, variable)) {
                if (edge.Kind == "Assign" || (!suppressed && edgeCanGC(edge)))
                    return {body:body, ppoint:edge.Index[0]};
            }
        }
    }
    return null;
}

function computePrintedLines(functionName)
{
    assert(!system("xdbfind src_body.xdb '" + functionName + "' > " + tmpfile));
    var lines = snarf(tmpfile).split('\n');

    for (var body of functionBodies)
        body.lines = [];

    // Distribute lines of output to the block they originate from.
    var currentBody = null;
    for (var i = 0; i < lines.length; i++) {
        var line = lines[i];
        if (/^block:/.test(line)) {
            if (match = /:(loop#[\d#]+)/.exec(line)) {
                var loop = match[1];
                var found = false;
                for (var body of functionBodies) {
                    if (body.BlockId.Kind == "Loop" && body.BlockId.Loop == loop) {
                        assert(!found);
                        found = true;
                        currentBody = body;
                    }
                }
                assert(found);
            } else {
                for (var body of functionBodies) {
                    if (body.BlockId.Kind == "Function")
                        currentBody = body;
                }
            }
        }
        if (currentBody)
            currentBody.lines.push(line);
    }
}

function findLocation(body, ppoint)
{
    var location = body.PPoint[ppoint - 1].Location;
    var text = location.CacheString + ":" + location.Line;
    if (text.indexOf(sourceRoot) == 0)
        return text.substring(sourceRoot.length);
    return text;
}

function locationLine(text)
{
    if (match = /:(\d+)$/.exec(text))
        return match[1];
    return 0;
}

function printEntryTrace(functionName, entry)
{
    if (!functionBodies[0].lines)
        computePrintedLines(functionName);

    while (entry) {
        var ppoint = entry.ppoint;
        var lineText = findLocation(entry.body, ppoint);

        var edgeText = null;
        if (entry.why && entry.why.body == entry.body) {
            // If the next point in the trace is in the same block, look for an edge between them.
            var next = entry.why.ppoint;
            for (var line of entry.body.lines) {
                if (match = /\((\d+),(\d+),/.exec(line)) {
                    if (match[1] == ppoint && match[2] == next)
                        edgeText = line; // May be multiple
                }
            }
            assert(edgeText);
        } else {
            // Look for any outgoing edge from the chosen point.
            for (var line of entry.body.lines) {
                if (match = /\((\d+),/.exec(line)) {
                    if (match[1] == ppoint) {
                        edgeText = line;
                        break;
                    }
                }
            }
        }

        print("    " + lineText + (edgeText ? ": " + edgeText : ""));
        entry = entry.why;
    }
}

function isRootedType(type)
{
    return type.Kind == "CSU" && isRootedTypeName(type.Name);
}

function typeDesc(type)
{
    if (type.Kind == "CSU") {
        return type.Name;
    } else if ('Type' in type) {
        var inner = typeDesc(type.Type);
        if (type.Kind == 'Pointer')
            return inner + '*';
        else if (type.Kind == 'Array')
            return inner + '[]';
        else
            return inner + '?';
    } else {
        return '???';
    }
}

function processBodies(functionName)
{
    if (!("DefineVariable" in functionBodies[0]))
        return;
    var suppressed = (mangled(functionName) in suppressedFunctions);
    for (var variable of functionBodies[0].DefineVariable) {
        if (variable.Variable.Kind == "Return")
            continue;
        var name;
        if (variable.Variable.Kind == "This")
            name = "this";
        else
            name = variable.Variable.Name[0];
        if (isRootedType(variable.Type)) {
            if (!variableLiveAcrossGC(suppressed, variable.Variable)) {
                // The earliest use of the variable should be its constructor.
                var lineText;
                for (var body of functionBodies) {
                    if (body.minimumUse) {
                        var text = findLocation(body, body.minimumUse);
                        if (!lineText || locationLine(lineText) > locationLine(text))
                            lineText = text;
                    }
                }
                print("\nFunction '" + functionName + "'" +
                      " has unnecessary root '" + name + "' at " + lineText);
            }
        } else if (isUnrootedType(variable.Type)) {
            var result = variableLiveAcrossGC(suppressed, variable.Variable);
            if (result) {
                var lineText = findLocation(result.gcInfo.body, result.gcInfo.ppoint);
                print("\nFunction '" + functionName + "'" +
                      " has unrooted '" + name + "'" +
                      " of type '" + typeDesc(variable.Type) + "'" +
                      " live across GC call " + result.gcInfo.name +
                      " at " + lineText);
                printEntryTrace(functionName, result.why);
            }
            result = unsafeVariableAddressTaken(suppressed, variable.Variable);
            if (result) {
                var lineText = findLocation(result.body, result.ppoint);
                print("\nFunction '" + functionName + "'" +
                      " takes unsafe address of unrooted '" + name + "'" +
                      " at " + lineText);
                printEntryTrace(functionName, {body:result.body, ppoint:result.ppoint});
            }
        }
    }
}

if (batch == 1)
    print("Time: " + new Date);

var xdb = xdbLibrary();
xdb.open("src_body.xdb");

var minStream = xdb.min_data_stream()|0;
var maxStream = xdb.max_data_stream()|0;

var N = (maxStream - minStream) + 1;
var each = Math.floor(N/numBatches);
var start = minStream + each * (batch - 1);
var end = Math.min(minStream + each * batch - 1, maxStream);

for (var nameIndex = start; nameIndex <= end; nameIndex++) {
    var name = xdb.read_key(nameIndex);
    var functionName = name.readString();
    var data = xdb.read_entry(name);
    xdb.free_string(name);
    var json = data.readString();
    xdb.free_string(data);
    functionBodies = JSON.parse(json);

    for (var body of functionBodies)
        body.suppressed = [];
    for (var body of functionBodies) {
        for (var [pbody, id] of allRAIIGuardedCallPoints(body, isSuppressConstructor))
            pbody.suppressed[id] = true;
    }
    processBodies(functionName);
}
