enableShellAllocationMetadataBuilder();
var x = transplantableObject();
x.object[0] = (function(){});
x.transplant(newGlobal({newCompartment: true}));
gc();

