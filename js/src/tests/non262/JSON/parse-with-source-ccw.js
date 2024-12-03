// |reftest| shell-option(--enable-json-parse-with-source) skip-if(!JSON.hasOwnProperty('isRawJSON')||!xulRuntime.shell)

(function checkCrossCompartmentWrappers() {
    var gbl = newGlobal({newCompartment: true});

    // the created context object should be wrapped in this compartment
    gbl.eval("context = JSON.parse('4.0', (k,v,context) => context);");
    assertEq(isCCW(gbl.context), true);
    // an object created in the reviver function should be wrapped in this compartment
    gbl.eval("sourceV = JSON.parse('4.0', (k,v,context) => { return {}; });");
    assertEq(isCCW(gbl.sourceV), true);

    // objects created by a reviver in this compartment should be wrapped in
    // the other compartment
    gbl.rev = (k,v,c) => { return {v}};
    gbl.eval("v2 = JSON.parse('4.0', rev);");
    assertEq(gbl.eval("isCCW(v2)"), true);

    gbl.eval("objCCW = {};");
    assertEq(JSON.isRawJSON(gbl.objCCW), false, "isRawJSON() should accept CCW arguments");
    rawJSONCCW = gbl.eval("JSON.rawJSON(455);");
    assertEq(JSON.isRawJSON(rawJSONCCW), true, "isRawJSON() should return true for wrapped rawJSON objects");

    assertEq(rawJSONCCW.rawJSON, "455", "rawJSON object enumerable property should be visible through CCW");

    objWithCCW = { ccw: rawJSONCCW, raw: JSON.rawJSON(true) };
    assertEq(JSON.stringify(objWithCCW), '{"ccw":455,"raw":true}');
    assertEq(isCCW(rawJSONCCW), true);
})();

if (typeof reportCompare == 'function')
    reportCompare(0, 0);
