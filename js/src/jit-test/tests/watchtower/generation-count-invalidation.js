// |jit-test| --ion-offthread-compile=off; --fast-warmup

let oldRegExp = RegExp;


function checkGlobalValue() {
    assertEq(RegExp, oldRegExp);
}


for (let i = 0; i < 1000; i++) {
    checkGlobalValue();
}

delete RegExp;

globalThis['extra'] = 12;
globalThis['RegExp'] = oldRegExp;


checkGlobalValue()

