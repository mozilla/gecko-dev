function checkErr(f) {
    assertThrowsInstanceOfWithMessage(f, ReferenceError,
        "must call super constructor before using 'this' in derived class constructor");
}
class TestNormal extends class {} {
    constructor() { this; }
}
checkErr(() => new TestNormal());

class TestEval extends class {} {
    constructor() { eval("this") }
}
checkErr(() => new TestEval());

class TestNestedEval extends class {} {
    constructor() { eval("eval('this')") }
}
checkErr(() => new TestNestedEval());

checkErr(() => {
    new class extends class {} {
        constructor() { eval("this") }
    }
});

class TestArrow extends class {} {
    constructor() { (() => this)(); }
}
checkErr(() => new TestArrow());

class TestArrowEval extends class {} {
    constructor() { (() => eval("this"))(); }
}
checkErr(() => new TestArrowEval());

class TestEvalArrow extends class {} {
    constructor() { eval("(() => this)()"); }
}
checkErr(() => new TestEvalArrow());

class TestTypeOf extends class {} {
    constructor() { eval("typeof this"); }
}
checkErr(() => new TestTypeOf());

if (typeof reportCompare === 'function')
    reportCompare(0, 0);
