// |reftest| shell-option(--no-ggc) skip-if(typeof(this.gcparam)==="undefined")

try {
    gcparam("nurseryEnabled", true);
} catch (e) {
    exc = e;
}
gczeal(4);
new Object();
assertEq(exc.message.includes("Parameter value out of range"), true);
if (typeof reportCompare === "function")
    reportCompare(0, 0, "ok");
