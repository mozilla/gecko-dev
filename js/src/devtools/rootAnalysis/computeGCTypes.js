/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*- */

"use strict";

loadRelativeToScript('utility.js');
loadRelativeToScript('annotations.js');

var gcTypes_filename = scriptArgs[0] || "gcTypes.txt";
var typeInfo_filename = scriptArgs[1] || "typeInfo.txt";

var annotations = {
    'GCPointers': [],
    'GCThings': [],
    'NonGCTypes': {}, // unused
    'NonGCPointers': {},
    'RootedPointers': {},
    'GCSuppressors': {},
};

var gDescriptors = new Map; // Map from descriptor string => Set of typeName

var structureParents = {}; // Map from field => list of <parent, fieldName>
var pointerParents = {}; // Map from field => list of <parent, fieldName>
var baseClasses = {}; // Map from struct name => list of base class name strings

var gcTypes = {}; // map from parent struct => Set of GC typed children
var gcPointers = {}; // map from parent struct => Set of GC typed children
var gcFields = new Map;

var rootedPointers = {};

function processCSU(csu, body)
{
    for (let { 'Base': base } of (body.CSUBaseClass || []))
        addBaseClass(csu, base);

    for (let field of (body.DataField || [])) {
        var type = field.Field.Type;
        var fieldName = field.Field.Name[0];
        if (type.Kind == "Pointer") {
            var target = type.Type;
            if (target.Kind == "CSU")
                addNestedPointer(csu, target.Name, fieldName);
        }
        if (type.Kind == "Array") {
            var target = type.Type;
            if (target.Kind == "CSU")
                addNestedStructure(csu, target.Name, fieldName);
        }
        if (type.Kind == "CSU") {
            // Ignore nesting in classes which are AutoGCRooters. We only consider
            // types with fields that may not be properly rooted.
            if (type.Name == "JS::AutoGCRooter" || type.Name == "JS::CustomAutoRooter")
                return;
            addNestedStructure(csu, type.Name, fieldName);
        }
    }

    for (let { 'Name': [ annType, tag ] } of (body.Annotation || [])) {
        if (annType != 'Tag')
            continue;

        if (tag == 'GC Pointer')
            annotations.GCPointers.push(csu);
        else if (tag == 'Invalidated by GC')
            annotations.GCPointers.push(csu);
        else if (tag == 'GC Thing')
            annotations.GCThings.push(csu);
        else if (tag == 'Suppressed GC Pointer')
            annotations.NonGCPointers[csu] = true;
        else if (tag == 'Rooted Pointer')
            annotations.RootedPointers[csu] = true;
        else if (tag == 'Suppress GC')
            annotations.GCSuppressors[csu] = true;
    }
}

// csu.field is of type inner
function addNestedStructure(csu, inner, field)
{
    if (!(inner in structureParents))
        structureParents[inner] = [];

    if (field.match(/^field:\d+$/) && (csu in baseClasses) && (baseClasses[csu].indexOf(inner) != -1))
        return;

    structureParents[inner].push([ csu, field ]);
}

function addBaseClass(csu, base) {
    if (!(csu in baseClasses))
        baseClasses[csu] = [];
    baseClasses[csu].push(base);
    var k = baseClasses[csu].length;
    addNestedStructure(csu, base, `<base-${k}>`);
}

function addNestedPointer(csu, inner, field)
{
    if (!(inner in pointerParents))
        pointerParents[inner] = [];
    pointerParents[inner].push([ csu, field ]);
}

var xdb = xdbLibrary();
xdb.open("src_comp.xdb");

var minStream = xdb.min_data_stream();
var maxStream = xdb.max_data_stream();

for (var csuIndex = minStream; csuIndex <= maxStream; csuIndex++) {
    var csu = xdb.read_key(csuIndex);
    var data = xdb.read_entry(csu);
    var json = JSON.parse(data.readString());
    assert(json.length == 1);
    processCSU(csu.readString(), json[0]);

    xdb.free_string(csu);
    xdb.free_string(data);
}

// Now that we have the whole hierarchy set up, add all the types and propagate
// info.
for (let csu of annotations.GCThings)
    addGCType(csu);
for (let csu of annotations.GCPointers)
    addGCPointer(csu);

function stars(n) { return n ? '*' + stars(n-1) : '' };

// "typeName is a (pointer to a)^'typePtrLevel' GC type because it contains a field
// named 'child' of type 'why' (or pointer to 'why' if fieldPtrLevel == 1), which is
// itself a GCThing or GCPointer."
function markGCType(typeName, child, why, typePtrLevel, fieldPtrLevel, indent)
{
    //printErr(`${indent}${typeName}${stars(typePtrLevel)} may be a gctype/ptr because of its child '${child}' of type ${why}${stars(fieldPtrLevel)}`);

    // Some types, like UniquePtr, do not mark/trace/relocate their contained
    // pointers and so should not hold them live across a GC. UniquePtr in
    // particular should be the only thing pointing to a structure containing a
    // GCPointer, so nothing else can possibly trace it and it'll die when the
    // UniquePtr goes out of scope. So we say that memory pointed to by a
    // UniquePtr is just as unsafe as the stack for storing GC pointers.
    if (!fieldPtrLevel && isUnsafeStorage(typeName)) {
        // The UniquePtr itself is on the stack but when you dereference the
        // contained pointer, you get to the unsafe memory that we are treating
        // as if it were the stack (aka ptrLevel 0). Note that
        // UniquePtr<UniquePtr<JSObject*>> is fine, so we don't want to just
        // hardcode the ptrLevel.
        fieldPtrLevel = -1;
    }

    // Example: with:
    //    struct Pair { JSObject* foo; int bar; };
    //    struct { Pair** info }***
    // make a call to:
    //    child='info' typePtrLevel=3 fieldPtrLevel=2
    // for a final ptrLevel of 5, used to later call:
    //    child='foo' typePtrLevel=5 fieldPtrLevel=1
    //
    var ptrLevel = typePtrLevel + fieldPtrLevel;

    // ...except when > 2 levels of pointers away from an actual GC thing, stop
    // searching the graph. (This would just be > 1, except that a UniquePtr
    // field might still have a GC pointer.)
    if (ptrLevel > 2)
        return;

    if (ptrLevel == 0 && isRootedGCTypeName(typeName))
        return;
    if (ptrLevel == 1 && isRootedGCPointerTypeName(typeName))
        return;

    if (ptrLevel == 0) {
        if (typeName in annotations.NonGCTypes)
            return;
        if (!(typeName in gcTypes))
            gcTypes[typeName] = new Set();
        gcTypes[typeName].add(why);
    } else if (ptrLevel == 1) {
        if (typeName in annotations.NonGCPointers)
            return;
        if (!(typeName in gcPointers))
            gcPointers[typeName] = new Set();
        gcPointers[typeName].add(why);
    }

    if (ptrLevel < 2) {
        if (!gcFields.has(typeName))
            gcFields.set(typeName, new Map());
        gcFields.get(typeName).set(child, [ why, fieldPtrLevel ]);
    }

    if (typeName in structureParents) {
        for (var field of structureParents[typeName]) {
            var [ holderType, fieldName ] = field;
            markGCType(holderType, fieldName, typeName, ptrLevel, 0, indent + "  ");
        }
    }
    if (typeName in pointerParents) {
        for (var field of pointerParents[typeName]) {
            var [ holderType, fieldName ] = field;
            markGCType(holderType, fieldName, typeName, ptrLevel, 1, indent + "  ");
        }
    }
}

function addGCType(typeName, child, why, depth, fieldPtrLevel)
{
    markGCType(typeName, '<annotation>', '(annotation)', 0, 0, "");
}

function addGCPointer(typeName)
{
    markGCType(typeName, '<pointer-annotation>', '(annotation)', 1, 0, "");
}

// Add an arbitrary descriptor to a type, and apply it recursively to all base
// structs and structs that contain the given typeName as a field.
function addDescriptor(typeName, descriptor)
{
    if (!gDescriptors.has(descriptor))
        gDescriptors.set(descriptor, new Set);
    let descriptorTypes = gDescriptors.get(descriptor);
    if (!descriptorTypes.has(typeName)) {
        descriptorTypes.add(typeName);
        if (typeName in structureParents) {
            for (let [holder, field] of structureParents[typeName])
                addDescriptor(holder, descriptor);
        }
        if (typeName in baseClasses) {
            for (let base of baseClasses[typeName])
                addDescriptor(base, descriptor);
        }
    }
}

for (var type of listNonGCPointers())
    annotations.NonGCPointers[type] = true;

function explain(csu, indent, seen) {
    if (!seen)
        seen = new Set();
    seen.add(csu);
    if (!gcFields.has(csu))
        return;
    var fields = gcFields.get(csu);

    if (fields.has('<annotation>')) {
        print(indent + "which is annotated as a GCThing");
        return;
    }
    if (fields.has('<pointer-annotation>')) {
        print(indent + "which is annotated as a GCPointer");
        return;
    }
    for (var [ field, [ child, ptrdness ] ] of fields) {
        var msg = indent;
        if (field[0] == '<')
            msg += "inherits from ";
        else {
            msg += "contains field '" + field + "' ";
            if (ptrdness == -1)
                msg += "(with a pointer to unsafe storage) holding a ";
            else if (ptrdness == 0)
                msg += "of type ";
            else
                msg += "pointing to type ";
        }
        msg += child;
        print(msg);
        if (!seen.has(child))
            explain(child, indent + "  ", seen);
    }
}

var origOut = os.file.redirect(gcTypes_filename);

for (var csu in gcTypes) {
    print("GCThing: " + csu);
    explain(csu, "  ");
}
for (var csu in gcPointers) {
    print("GCPointer: " + csu);
    explain(csu, "  ");
}

// Redirect output to the typeInfo file and close the gcTypes file.
os.file.close(os.file.redirect(typeInfo_filename));

for (let csu in annotations.GCSuppressors)
    addDescriptor(csu, 'Suppress GC');

for (let [descriptor, types] of gDescriptors) {
    for (let csu of types)
        print(descriptor + "$$" + csu);
}

os.file.close(os.file.redirect(origOut));
