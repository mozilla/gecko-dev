function testClassesWithNoPrivateMembers() {
  class C {
    x;
    y;
    z;
  }
  const c = new C();
  assertEq(Object.keys(c).length == 3, true);
}

function testClassesWithPrivateMembers() {
  class C {
    x;
    y;
    z;
    #a;
  }
  const c = new C();
  assertEq(Object.keys(c).length == 3, true);
}

function testClassesWithConstructorMembers() {
  class C {
    x;
    y;
    z;
    constructor() {
      this.a = 1;
    }
  }
  const c = new C();
  assertEq(Object.keys(c).length == 4, true);
}

testClassesWithConstructorMembers();
testClassesWithNoPrivateMembers();
testClassesWithPrivateMembers();
