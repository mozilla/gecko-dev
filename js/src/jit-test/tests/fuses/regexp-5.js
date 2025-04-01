// Tests for String.prototype builtins with custom regexp symbols on
// the regular expression object itself.

// Custom re[@@match].
function testMatch() {
  var s = "foobar";
  var re = /abc.+/;
  var count = 0;
  for (var i = 0; i < 200; i++) {
    s.match(re);
    if (i === 150) {
      re[Symbol.match] = function() {
        count++;
        return null;
      };
    }
  }
  assertEq(count, 49);
}
testMatch();

// Custom re[@@matchAll].
function testMatchAll() {
  var s = "foobar";
  var re = /abc.+/g;
  var count = 0;
  for (var i = 0; i < 200; i++) {
    s.matchAll(re);
    if (i === 150) {
      re[Symbol.matchAll] = function() {
        count++;
        return null;
      };
    }
  }
  assertEq(count, 49);
}
testMatchAll();

// Custom re[@@replace] for replace.
function testReplace() {
  var s = "foobar";
  var re = /abc.+/;
  var count = 0;
  for (var i = 0; i < 200; i++) {
    s.replace(re, "");
    if (i === 150) {
      re[Symbol.replace] = function() {
        count++;
        return "";
      };
    }
  }
  assertEq(count, 49);
}
testReplace();

// Custom re[@@replace] for replaceAll.
function testReplaceAll() {
  var s = "foobar";
  var re = /abc.+/g;
  var count = 0;
  for (var i = 0; i < 200; i++) {
    s.replaceAll(re, "");
    if (i === 150) {
      re[Symbol.replace] = function() {
        count++;
        return "";
      };
    }
  }
  assertEq(count, 49);
}
testReplaceAll();

// Custom re[@@search].
function testSearch() {
  var s = "foobar";
  var re = /abc.+/g;
  var count = 0;
  for (var i = 0; i < 200; i++) {
    s.search(re);
    if (i === 150) {
      re[Symbol.search] = function() {
        count++;
        return -1;
      };
    }
  }
  assertEq(count, 49);
}
testSearch();

// Custom re[@@split].
function testSplit() {
  var s = "foobar";
  var re = /abc.+/;
  var count = 0;
  for (var i = 0; i < 200; i++) {
    s.split(re);
    if (i === 150) {
      re[Symbol.split] = function() {
        count++;
        return [];
      };
    }
  }
  assertEq(count, 49);
}
testSplit();

// RegExp.prototype fuse must still be intact.
assertEq(getFuseState().OptimizeRegExpPrototypeFuse.intact, true);
