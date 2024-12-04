gczeal(17, 250);
var c = {};
function a() {
    const d = newGlobal({ newCompartment: true });
    d.grayRoot();
    with (d) {
        const f = new FinalizationRegistry(v => { });
        f.register(c);
    }
}
a();
a();
nukeAllCCWs();
